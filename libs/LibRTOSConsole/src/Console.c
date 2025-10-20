/*
 * Console.c
 *
 *  Created on: Dec 8, 2024
 *      Author: Thorsten
 */

 /*! \file */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <math.h>
#ifdef __NEWLIB__
#include <reent.h>
#endif
#include <sys/queue.h>

#include "main.h"
#include "Console.h"
#include "ConsoleConfig.h"


#ifdef __arm__
#  include "core_cm7.h"
#endif


#ifndef CONSOLE_USERNAME
#  define USERNAME  "STM32"
#endif

#ifndef CONSOLE_USE_DYNAMIC_USERNAME
#  define CONSOLE_USE_DYNAMIC_USERNAME 0
#endif

#ifndef CONSOLE_LINE_HISTORY
#  define CONSOLE_LINE_HISTORY 8
#endif

#ifndef CONSOLE_LINE_SIZE
#  define CONSOLE_LINE_SIZE 120
#endif

#ifndef CONSOLE_COMMAND_MAX_LENGTH
#  define CONSOLE_COMMAND_MAX_LENGTH 64
#endif

#ifndef CONSOLE_HELP_MAX_LENGTH
#  define CONSOLE_HELP_MAX_LENGTH 256
#endif

#if CONSOLE_HELP_MAX_LENGTH < CONSOLE_LINE_SIZE
#pragma error "the line size must not be larger than the help size, otherwise alias wont work anymore!"
#endif

#define CONSOLE_SAFETY_SPACE 4
// always min of 4 commands plus line size/3 because argument '-x ' and space at least!
#define CONSOLE_MAX_NUM_ARGS ((CONSOLE_LINE_SIZE / 3) + 4)

// --------------------------------------------------------------------------------------------------------------------
typedef struct cmdEntry
// --------------------------------------------------------------------------------------------------------------------
{
    struct
	{
    	CONSOLE_CommandFunc func;
    	void*               ctx;
		char                cmd[CONSOLE_COMMAND_MAX_LENGTH + 2];
		int                 cmdLen;
		char                help[CONSOLE_HELP_MAX_LENGTH + 2];
		int                 helpLen;
		int                 isAlias;
	} content;

    LIST_ENTRY(cmdEntry) navigate;
} cmdEntry_t;

// --------------------------------------------------------------------------------------------------------------------
typedef struct cmdState
// --------------------------------------------------------------------------------------------------------------------
{
	SemaphoreHandle_t             lockGuard;
	LIST_HEAD(cmd_list, cmdEntry) commands;
} cmdState_t;

// --------------------------------------------------------------------------------------------------------------------
typedef enum
// --------------------------------------------------------------------------------------------------------------------
{
	csptNONE,
	csptCHARACTER,
	csptCONTROL
} cspTYPE;

// --------------------------------------------------------------------------------------------------------------------
typedef enum
// --------------------------------------------------------------------------------------------------------------------
{
	ctrlC0_NUL   =   0x00,
	ctrlC0_SOH   =   0x01,
	ctrlC0_STX   =   0x02,
	ctrlC0_ETX   =   0x03,
	ctrlC0_EOT   =   0x04,
	ctrlC0_ENQ   =   0x05,
	ctrlC0_ACK   =   0x06,
	ctrlC0_BEL   =   0x07,
	ctrlC0_BS    =   0x08,
	ctrlC0_TAB   =   0x09,
	ctrlC0_LF    =   0x0A,
	ctrlC0_VT    =   0x0B,
	ctrlC0_FF    =   0x0C,
	ctrlC0_CR    =   0x0D,
	ctrlC0_SO    =   0x0E,
	ctrlC0_SI    =   0x0F,
	ctrlC0_DLE   =   0x10,
	ctrlC0_DC1   =   0x11,
	ctrlC0_DC2   =   0x12,
	ctrlC0_DC3   =   0x13,
	ctrlC0_DC4   =   0x14,
	ctrlC0_NAK   =   0x15,
	ctrlC0_SYN   =   0x16,
	ctrlC0_ETB   =   0x17,
	ctrlC0_CAN   =   0x18,
	ctrlC0_EM    =   0x19,
	ctrlC0_SUB   =   0x1A,
	ctrlC0_ESC   =   0x1B,
	ctrlC0_FS    =   0x1C,
	ctrlC0_GS    =   0x1D,
	ctrlC0_RS    =   0x1E,
	ctrlC0_US    =   0x1F,
	ctrlC0_DEL   =   0x7F,
	ctrlC1_MASK  =  0x100,
	ctrlC1_DCS   =  0x101,
	ctrlC1_CSI   =  0x102,
	ctrlC1_SOS   =  0x103,
	ctrlC1_PM    =  0x104,
	ctrlC1_APC   =  0x105,
	ctrlC1_ST    =  0x106,
	ctrlUNKNOWN  = 0x1000,
	ctrlOVERFLOW = 0x1001
} ctrlCodes_t;

// --------------------------------------------------------------------------------------------------------------------
typedef enum
// --------------------------------------------------------------------------------------------------------------------
{
	ctrlpsIDLE_DETECT = 0,
	ctrlpsSTART_C1    = 1,
	ctrlpsHANDLE_CSI  = 2,
	ctrlpsHANDLE_ST_1 = 3,
	ctrlpsHANDLE_ST_2 = 4
} ctrlpStates_t;

// --------------------------------------------------------------------------------------------------------------------
typedef struct
// --------------------------------------------------------------------------------------------------------------------
{
	ctrlpStates_t state;
	ctrlCodes_t   type;
	unsigned int  ptr;
	unsigned int  length;
	unsigned int  maxLength;
	char*         buff;
} cspState_t;

// --------------------------------------------------------------------------------------------------------------------
struct ConsoleHandle
// --------------------------------------------------------------------------------------------------------------------
{
	cmdState_t        cState;
	cspState_t        pState;
	TaskHandle_t      tHandle;
	int               cancel;
	struct
	{
		char          lines[CONSOLE_LINE_HISTORY][CONSOLE_LINE_SIZE+CONSOLE_SAFETY_SPACE];
		int           lineHead;
		int           linePtr;
	} history;

	int                  pendingRedirect;
	ConsoleReadStream_t  pendingRdStream;
	ConsoleWriteStream_t pendingWrStream;
	void*                pendingRdCtx;
	void*                pendingWrCtx;
};

#ifdef WIN32
// --------------------------------------------------------------------------------------------------------------------
int setenv(const char* name, const char* value, int overwrite)
// --------------------------------------------------------------------------------------------------------------------
{
	int errcode = 0;
	if (!overwrite) {
		size_t envsize = 0;
		errcode = getenv_s(&envsize, NULL, 0, name);
		if (errcode || envsize) return errcode;
	}
	return _putenv_s(name, value);
}
#endif

// --------------------------------------------------------------------------------------------------------------------
cspTYPE ControlSequenceParserConsume( char input, cspState_t* s )
// --------------------------------------------------------------------------------------------------------------------
{
#define CHECK_FOR_OVERFLOW(x) do { if ( ((x)+1) > s->maxLength ) \
	                          { s->type = ctrlOVERFLOW; s->length = 0; s->state = ctrlpsIDLE_DETECT; \
	                          return csptCONTROL; } } while(0)

	switch (s->state)
	{
	case ctrlpsIDLE_DETECT:
		// default start condition
		s->buff[0] = input;
		s->buff[1] = ctrlC0_NUL;
		s->ptr = 0;
		s->length = 1;

		// escape code takes more than one byte in total length
		if (input == ctrlC0_ESC)
		{
			s->state = ctrlpsSTART_C1;
			s->ptr = 1;
			s->type = ctrlC0_ESC;
			return csptNONE;
		}

		// single length control code
		else if ( ( /* input >= ctrlC0_NUL &&*/ (unsigned char)input <= (unsigned char)ctrlC0_US ) ||
				  ( input == ctrlC0_DEL ) )
	    {
			s->type = input;
			return csptCONTROL;
	    }

		// only a character, nothing else
		return csptCHARACTER;

	case ctrlpsSTART_C1:

		// now we need to check the type of escape sequence
		s->buff[s->ptr++] = input;
		CHECK_FOR_OVERFLOW(s->ptr);
		if ( input == '[' )
		{
			s->type = ctrlC1_CSI;
			s->state = ctrlpsHANDLE_CSI;
		}
		else if ( input == 'X' )
		{
			s->type = ctrlC1_SOS;
			s->state = ctrlpsHANDLE_ST_1;
		}
		else if ( input == '^' )
		{
			s->type = ctrlC1_PM;
			s->state = ctrlpsHANDLE_ST_1;
		}
		else if ( input == '_' )
		{
			s->type = ctrlC1_APC;
			s->state = ctrlpsHANDLE_ST_1;
		}
		else if ( input == '\\' )
		{
			s->type = ctrlC1_ST;
			s->state = ctrlpsHANDLE_ST_1;
		}
		else if ( input == 'P' )
		{
			s->type = ctrlC1_DCS;
			s->state = ctrlpsHANDLE_ST_1;
		}
		else
		{
			s->type = ctrlUNKNOWN;
			s->state = ctrlpsIDLE_DETECT;
			return csptCONTROL;
		}
		return csptNONE;

	// data handling and termination of Control Sequence Introducer
	case ctrlpsHANDLE_CSI:
	{
		s->buff[s->ptr++] = input;
		CHECK_FOR_OVERFLOW(s->ptr);
		if ( input >= 0x40 && input <= 0x7E )
		{
			s->buff[s->ptr] = ctrlC0_NUL;
			s->length = s->ptr;
			s->state = ctrlpsIDLE_DETECT;
			return csptCONTROL;
		}
		return csptNONE;
	}

	// first part of ST terminator
	case ctrlpsHANDLE_ST_1:
	{
		s->buff[s->ptr++] = input;
		CHECK_FOR_OVERFLOW(s->ptr);
		if ( input == '\033' )
		{
			s->state = ctrlpsHANDLE_ST_2;
		}
		return csptNONE;
	}
	// second part of ST terminator
	case ctrlpsHANDLE_ST_2:
	{
		s->buff[s->ptr++] = input;
		CHECK_FOR_OVERFLOW(s->ptr);
		if ( input != '\\' )
		{
			s->state = ctrlpsHANDLE_ST_1;
		}
		else
		{
			s->buff[s->ptr] = ctrlC0_NUL;
			s->length = s->ptr;
			s->state = ctrlpsIDLE_DETECT;
			return csptCONTROL;
		}
		return csptNONE;
	}
	default:
		return csptCHARACTER;
	}
}

// --------------------------------------------------------------------------------------------------------------------
static int ProcessCommand(char* command, int cmdLen, char** args, int numArgs, cmdState_t* c, int* isAlias, char* inputBuffer, int inbuffsz)
// --------------------------------------------------------------------------------------------------------------------
{
	// here we have to look for a matching entry and therefore we have to search linearly through
	// our linked list of command entries
	xSemaphoreTakeRecursive( c->lockGuard, -1 );
	cmdEntry_t* pElement = c->commands.lh_first;
	int found = 0;
	int result = 0;
	while ( pElement != NULL )
	{
		// if string compare result and determined length match, then this must be the function
		if ( strncmp(command, pElement->content.cmd, cmdLen) == 0 && cmdLen == pElement->content.cmdLen )
		{
			found = 1;
			if ( pElement->content.isAlias )
			{
				*isAlias = 1;
				// first we have to copy the arguments behind the command (as long as we have enough space)
				int currentArg = 0;
				int stillCopiedLength = 0;
				char tempInBuff[CONSOLE_LINE_SIZE + 1];
				char* tempArgs[CONSOLE_MAX_NUM_ARGS];
				memset(tempArgs, 0, sizeof(tempArgs));
				for (int i = 0; i < numArgs; i++)
				{
					tempArgs[i] = args[i] - inputBuffer + tempInBuff;
				}
				memcpy(tempInBuff, inputBuffer, inbuffsz);
				while (numArgs > 0)
				{
					// all args are NULL-terminated so we can safely use strlen
					int argCopyLen = strlen(tempArgs[currentArg]);
					int additionalTermination = 0;
					if (*(tempArgs[currentArg] - 1) == '"' || tempArgs[currentArg] == NULL)
					{
						additionalTermination = 1;
					}
					if ((argCopyLen + pElement->content.helpLen + stillCopiedLength + 1) > inbuffsz)
					{
						printf("\033[31mAlias Argument Substitution Overflow\033[0m");
						result = -1;
						*isAlias = 0;
						return result;
					}
					if (additionalTermination)
					{
						inputBuffer[pElement->content.helpLen + stillCopiedLength + 1] = '"';
						stillCopiedLength += 1;
					}
					memcpy(&inputBuffer[pElement->content.helpLen + stillCopiedLength + 1], tempArgs[currentArg], argCopyLen);
					stillCopiedLength += argCopyLen;
					if (additionalTermination)
					{
						inputBuffer[pElement->content.helpLen + stillCopiedLength + 1] = '"';
						stillCopiedLength += 1;
					}
					inputBuffer[pElement->content.helpLen + stillCopiedLength + 1] = ' ';
					stillCopiedLength += 1;
					numArgs -= 1;
					currentArg += 1;
				}

				memcpy(inputBuffer, pElement->content.help, pElement->content.helpLen);
				memset(&inputBuffer[pElement->content.helpLen+ stillCopiedLength], 0, inbuffsz-(pElement->content.helpLen+stillCopiedLength));
				if (currentArg != 0) inputBuffer[pElement->content.helpLen] = ' ';
				result = 0;
			}
			else
			{
				result = pElement->content.func(numArgs, args, pElement->content.ctx);
			}
			break;
		}

		pElement = pElement->navigate.le_next;
	}

	xSemaphoreGiveRecursive( c->lockGuard );
	if ( found == 0 )
	{
		printf("\033[31mInvalid command\033[0m");
		fflush(stdout);
		result = -1;
	}
	return result;
}

// --------------------------------------------------------------------------------------------------------------------
static int TransformAndProcessTheCommand(char* lineBuff, int line_size, cmdState_t* cState)
// --------------------------------------------------------------------------------------------------------------------
{
	int startIdx;
	int endIdx;
	int numArgs;
	int isAlias;

	char* args[CONSOLE_MAX_NUM_ARGS];
	char* command;
	char* strtokNewIndex = NULL;

restart:
	startIdx = 0;
	endIdx = CONSOLE_LINE_SIZE - 1;
	numArgs = 0;
	isAlias = 0;
	command = NULL;

	memset(args, 0, sizeof(args));

	if ( lineBuff[startIdx] == '\0' ) return 0;

	while(startIdx < line_size)
	{
		if ( lineBuff[startIdx] != ' ' ) break;
		startIdx += 1;
	}

	while(endIdx > 0)
	{
		if ( lineBuff[endIdx] != '\0' && lineBuff[endIdx] != ' ' ) break;
		endIdx -= 1;
	}

	if ( startIdx <= endIdx )
	{
		// strtok is safe because we have a nulled safety margin behind the string
		command = strtok(&lineBuff[startIdx], " ");

		// some sanity checks before tokenizing
		if ( command == NULL ) return 0;
		if ((int)strnlen(command, line_size) == 0 ) return 0;

		int cmdLength = (int)strnlen(command, line_size);

		// now get the arguments
		while((args[numArgs] = strtok(strtokNewIndex, " ")) != NULL && numArgs < CONSOLE_MAX_NUM_ARGS)
		{
			strtokNewIndex = NULL;
			if (args[numArgs][0] == '"')
			{
				// move the argument to remove the quotes
				args[numArgs] += 1;
				if (args[numArgs][0] == '"') {
					args[numArgs][0] = '\0';
					// we have to add a plus 2 because there is the '"' char and '\0' from strtok as second
					// char. So we need to add 2 chars to get to the next valid char or the end of the string
					strtokNewIndex = &args[numArgs][2];
				}
				else
				{
					// now look for the end of the argument and set new strtok index to this
					// string part
					int firstLen = strlen(args[numArgs]);
					args[numArgs][firstLen] = ' ';
					char* endChar = &args[numArgs][firstLen-1];
					// while loop is safe because we have a nulled safety margin behind the string
					while (*endChar != '\0' && *endChar != '"') endChar += 1;
					if (*endChar == '"') {
						*endChar = '\0'; endChar += 1;
					}
					strtokNewIndex = endChar;
				}
			}
			numArgs+=1;
		}

		// now call the command
		int retVal = ProcessCommand(command, cmdLength, args, numArgs, cState, &isAlias, lineBuff, line_size);
		if ( isAlias )
		{
			// in case it is an alias, the line buffer has been overwritten with the alias and so we have to do
			// this round again
			goto restart;
		}
		return retVal;
	}

	return 0;
}

// --------------------------------------------------------------------------------------------------------------------
static void PrintConsoleControl( cspState_t* s )
// --------------------------------------------------------------------------------------------------------------------
{
	if ( s->length >= 3 && s->type == ctrlC1_CSI )
	{
		for ( unsigned int i = 0; i < s->length; i ++ )
		{
			putchar(s->buff[i]);
		}
		fflush(stdout);
	}
}

// --------------------------------------------------------------------------------------------------------------------
static int ConsoleIsArrowLeft( cspState_t* s )
// --------------------------------------------------------------------------------------------------------------------
{
	return ( s->length >= 3 && s->type == ctrlC1_CSI && s->buff[2] == 68);
}

// --------------------------------------------------------------------------------------------------------------------
static int ConsoleIsArrowRight( cspState_t* s )
// --------------------------------------------------------------------------------------------------------------------
{
	return ( s->length >= 3 && s->type == ctrlC1_CSI && s->buff[2] == 67);
}

// --------------------------------------------------------------------------------------------------------------------
static int ConsoleIsArrowUp( cspState_t* s )
// --------------------------------------------------------------------------------------------------------------------
{
	return ( s->length >= 3 && s->type == ctrlC1_CSI && s->buff[2] == 65);
}

// --------------------------------------------------------------------------------------------------------------------
static int ConsoleIsArrowDown( cspState_t* s )
// --------------------------------------------------------------------------------------------------------------------
{
	return ( s->length >= 3 && s->type == ctrlC1_CSI && s->buff[2] == 66);
}

// --------------------------------------------------------------------------------------------------------------------
static int ConsoleIsEntf( cspState_t* s )
// --------------------------------------------------------------------------------------------------------------------
{
	return ( s->length >= 4 && s->type == ctrlC1_CSI && s->buff[2] == 51 && s->buff[3] == 126);
}

// --------------------------------------------------------------------------------------------------------------------
static void PrintConsoleArrowLeft( void )
// --------------------------------------------------------------------------------------------------------------------
{
	putchar('\033');
	putchar('[');
	putchar(68);
	fflush(stdout);
}

// --------------------------------------------------------------------------------------------------------------------
int CONSOLE_RedirectStreams( ConsoleHandle_t h, ConsoleReadStream_t rdFunc, ConsoleWriteStream_t wrFunc,
		void* rdContext, void* wrContext )
// --------------------------------------------------------------------------------------------------------------------
{
	(void)h;
	(void)rdFunc;
	(void)wrFunc;
	(void)rdContext;
	(void)wrContext;
#ifndef __NEWLIB__ // so far only newlib is supported
	return -2;
#else
	// we can only exec the real stream redirection when we are the console thread itself and the scheduler is running,
	// otherwise we have to set the request to pending state
	if ( ( taskSCHEDULER_RUNNING == xTaskGetSchedulerState() ) && ( xTaskGetCurrentTaskHandle() == h->tHandle ) )
	{
		FILE* rdToClean = NULL;
		if ( _impure_ptr->_stdin != &__sf[0])
		{
			rdToClean = _impure_ptr->_stdin;
		}

		FILE* wrToClean = NULL;
		if ( _impure_ptr->_stdout != &__sf[1])
		{
			wrToClean = _impure_ptr->_stdout;
		}

		FILE* myStdOut = &__sf[1];
		if ( wrFunc != NULL )
		{
			myStdOut = fwopen(wrContext, wrFunc);
			if ( myStdOut == NULL ) return -1;
		}

		FILE* myStdIn = &__sf[0];
		if ( rdFunc != NULL )
		{
			myStdIn = fropen(rdContext, rdFunc);
			if ( myStdIn == NULL )
			{
				if ( myStdOut != NULL && myStdOut != &__sf[1]) fclose(myStdOut);
				return -1;
			}
		}

		_impure_ptr->_stdin  = myStdIn;
		_impure_ptr->_stdout = myStdOut;

		if (wrToClean != NULL) fclose(wrToClean);
		if (rdToClean != NULL) fclose(rdToClean);
	}
	else
	{
		h->pendingRedirect = 1;
		h->pendingRdStream = rdFunc;
		h->pendingWrStream = wrFunc;
		h->pendingRdCtx    = rdContext;
		h->pendingWrCtx    = wrContext;
	}
#endif
	return 0;
}

// --------------------------------------------------------------------------------------------------------------------
static void ConsoleFunction( void * arg )
// --------------------------------------------------------------------------------------------------------------------
{
	ConsoleHandle_t h = (ConsoleHandle_t)arg;
	if (h == NULL) goto destroy;

	if( h->pendingRedirect != 0 )
	{
		h->pendingRedirect = 0;
		if ( CONSOLE_RedirectStreams(h, h->pendingRdStream, h->pendingWrStream, h->pendingRdCtx, h->pendingWrCtx) )
		{
			printf("was not able to redirect console streams, requested by user!");
		}
	}

#define xstr(a) str(a)
#define str(a) #a
    static const char headerASCIIArt[] =
        "\033c\033[3J\r\n\r\n"
        "\033[31m               -+                                         \r\n"
        "\033[31m              [[+                                         \r\n"
        "\033[31m            [[[[=                                         \r\n"
        "\033[31m  [[[[[[[[} }}}})\033[39m<<<>   \033[31m  ____    _   _  \033[39m ____   __        __    \r\n"
        "\033[31m  [[[[[[}}} }}}})\033[39m<<<>   \033[31m |  _ \\  | | | | \033[39m| __ )  \\ \\      / / \r\n"
        "\033[31m  [[[[[}}}} }}}>\033[39m<<<<>   \033[31m | | | | | |_| | \033[39m|  _ \\   \\ \\ /\\ / / \r\n"
        "\033[31m  [[[[[}}}} }>\033[39m<<<><<>   \033[31m | |_| | |  _  | \033[39m| |_) |   \\ V  V /    \r\n"
        "\033[31m  ::::=<<<< \033[39m:::::::::   \033[31m |____/  |_| |_| \033[39m|____/     \\_/\\_/     \r\n"
        "\033[39m      -<<:              \r\n"
#ifdef WIN32
		"\033[39m      -=   MSVC RTOS SIMULATOR ";
#else
		"\033[39m      -=   ARM RTOS ";
#endif
    printf((char*)headerASCIIArt);
#ifdef EXERCISE
    printf("EXERCISE: ");
    printf(xstr(EXERCISE)));
    printf("\r\n\r\n");
#else
    printf("PLAYGROUND\r\n\r\n");
#endif

#if CONSOLE_USE_DYNAMIC_USERNAME != 0
	char* usernamePtr = getenv("USERNAME");
	if ( usernamePtr == 0 ) usernamePtr = CONSOLE_USERNAME;
#else
	char* usernamePtr = CONSOLE_USERNAME;
#endif

	char* lineBuff = NULL;
	char* ctrlBuff = malloc(CONSOLE_LINE_SIZE + CONSOLE_SAFETY_SPACE); // make sure we have a little space behind
	if (ctrlBuff == NULL) goto exit;

	lineBuff = malloc(CONSOLE_LINE_SIZE + CONSOLE_SAFETY_SPACE); // make sure we have a little space behind
	if (lineBuff == NULL) goto exit;

	memset(ctrlBuff, ctrlC0_NUL, CONSOLE_LINE_SIZE + CONSOLE_SAFETY_SPACE);
	memset(lineBuff, ctrlC0_NUL, CONSOLE_LINE_SIZE + CONSOLE_SAFETY_SPACE);
	unsigned int lbPtr = 0;

	printf("\r\nFreeRTOS Console Up and Running\r\n");
	printf("\r\n\r\n-------------------------------------------------------------------\r\n");

	h->pState.buff = ctrlBuff;

	printf("\r\n%s(\033[32m\xE2\x9C\x93\033[0m) $>", usernamePtr);
	int consoleStartIndex = (int)strlen(usernamePtr)+6;
	fflush(stdout);

	while(h->cancel == 0)
	{
		if( h->pendingRedirect != 0 )
		{
			h->pendingRedirect = 0;
			if ( CONSOLE_RedirectStreams(h, h->pendingRdStream, h->pendingWrStream, h->pendingRdCtx, h->pendingWrCtx) )
			{
				printf("was not able to redirect console streams, requested by user!");
			}
		}

		int res = EOF;
		while((res = getchar()) == EOF)
		{
			if ( h->cancel == 1 ) goto exit;
		}
		char myChar = res;
		cspTYPE result = ControlSequenceParserConsume(myChar, &h->pState);
		if ( result == csptCHARACTER )
		{
			putchar(myChar);
			fflush(stdout);

			if ( lineBuff[lbPtr + 1] != '\0' )
			{
				putchar(lineBuff[lbPtr]);
				fflush(stdout);

				int tmpPtr = lbPtr + 1;
				char parking = myChar;
				char parking2 = lineBuff[lbPtr];
				while(tmpPtr < CONSOLE_LINE_SIZE)
				{
					putchar(lineBuff[tmpPtr]);
					fflush(stdout);


					lineBuff[tmpPtr - 1] = parking;
					parking = parking2;
					parking2 = lineBuff[tmpPtr];

					if ( lineBuff[tmpPtr] == '\0' && parking == '\0')
						break;

					tmpPtr += 1;
				}

				printf("\033[%dD", CONSOLE_LINE_SIZE + consoleStartIndex);
				printf("\033[%dC", consoleStartIndex + lbPtr + 1);
				lbPtr += 1;
				fflush(stdout);
			}
			else
			{
				lineBuff[lbPtr] = myChar;
				lbPtr++;
			}

			if ( lbPtr > CONSOLE_LINE_SIZE )
			{
				printf("\r\n Buffer Overrun! Clearing input...\r\n");
				// print new console line and decode the result
				printf("\r\n%s(\033[31m\xE2\x98\x93\033[0m) $>", usernamePtr);
				fflush(stdout);

				// clear the buffer and restore the pointer
				do
				{
					lineBuff[lbPtr] = ctrlC0_NUL;
				    if ( lbPtr == 0 ) break;
				    else lbPtr -= 1;
				} while (1);
			}
		}
		else if ( result == csptCONTROL )
		{
			switch (h->pState.type)
			{
				// implicit fall through
			case ctrlC0_LF:
			case ctrlC0_CR:
			{
				putchar(h->pState.type);
				fflush(stdout);

				// implicit CR on every LF?
				if (0 && h->pState.type == ctrlC0_LF)
				{
					putchar(ctrlC0_CR);
					fflush(stdout);
				}

				// implicit LF on every CR?
				if (1 && h->pState.type == ctrlC0_CR)
				{
					putchar(ctrlC0_LF);
					fflush(stdout);
				}

				// now adapt the line history accordingly
				memcpy(h->history.lines[h->history.lineHead], lineBuff, CONSOLE_LINE_SIZE);
				h->history.lineHead = (h->history.lineHead + 1) % CONSOLE_LINE_HISTORY;
				h->history.linePtr = h->history.lineHead;

				// parse and execute the command and make sure the output streams
				// are flushed before doing anything else with the result
				int result = TransformAndProcessTheCommand(lineBuff, CONSOLE_LINE_SIZE, &h->cState);
				fflush(stdout);
				fflush(stderr);

#if CONSOLE_USE_DYNAMIC_USERNAME != 0
				// now check if there is a new user name (which is only possible by setenv command
				// which is executed after process command call above...
				usernamePtr = getenv("USERNAME");
				if ( usernamePtr == 0 ) usernamePtr = CONSOLE_USERNAME;
				consoleStartIndex = (int)strlen(usernamePtr)+6;
#endif
				// print new console line and decode the result
				printf("\r\n%s(", usernamePtr);
				if (result == 0)
				{
					printf("\033[32m\xE2\x9C\x93\033[0m");
				}
				else
				{
					printf("\033[31m\xE2\x98\x93\033[0m");
				}
				printf(") $>");
				fflush(stdout);

				// clear the buffer completely because an alias could change
				// the buffer content way more than the user has entered and so
				// we can not only clear lbPtr--!! as we have a safety space we
				// can set CONSOLE_LINE_SIZE as matching pointer value;
				lbPtr = CONSOLE_LINE_SIZE;
				do
				{
					lineBuff[lbPtr] = ctrlC0_NUL;
					if (lbPtr == 0) break;
					else lbPtr -= 1;
				} while (1);

				break;
			}
			case ctrlC0_DEL:
			{
				if (lbPtr > 0)
				{
					int tmpPtr = lbPtr;
					lbPtr -= 1;
					lineBuff[lbPtr] = ctrlC0_NUL;
					putchar(ctrlC0_DEL);
					fflush(stdout);
					while (lineBuff[tmpPtr] != ctrlC0_NUL)
					{
						lineBuff[tmpPtr - 1] = lineBuff[tmpPtr];
						putchar(lineBuff[tmpPtr - 1]);
						fflush(stdout);
						tmpPtr += 1;
						if (tmpPtr >= CONSOLE_LINE_SIZE) break;
					}
					lineBuff[tmpPtr - 1] = ctrlC0_NUL;
					putchar(' ');
					fflush(stdout);

					int moveBack = tmpPtr - lbPtr;
					for (; moveBack > 0; moveBack--)
						PrintConsoleArrowLeft();
				}
				break;
			}
			case ctrlC0_TAB:
			{
				int nums = 4 - (lbPtr % 4);
				for (; nums > 0; nums--)
				{
					lineBuff[lbPtr] = ' ';
					lbPtr += 1;
					putchar(' ');
					fflush(stdout);
				}
				break;
			}

			case ctrlC1_CSI:
			{
				if (ConsoleIsArrowLeft(&h->pState))
				{
					if (lbPtr > 0)
					{
						lbPtr -= 1;
						PrintConsoleControl(&h->pState);
					}
				}
				else if (ConsoleIsArrowRight(&h->pState))
				{
					if (lbPtr < (CONSOLE_LINE_SIZE - 1))
					{
						if (lineBuff[lbPtr] == ctrlC0_NUL)
						{
							lineBuff[lbPtr] = ' ';
							putchar(' ');
							fflush(stdout);
						}
						else
						{
							PrintConsoleControl(&h->pState);
						}
						lbPtr += 1;
					}
				}
				else if (ConsoleIsEntf(&h->pState))
				{
					if (lbPtr < (CONSOLE_LINE_SIZE - 1))
					{
						int tmpPtr = lbPtr + 1;
						lineBuff[lbPtr] = ' ';
						PrintConsoleControl(&h->pState);
						while (lineBuff[tmpPtr] != ctrlC0_NUL)
						{
							lineBuff[tmpPtr - 1] = lineBuff[tmpPtr];
							putchar(lineBuff[tmpPtr - 1]);
							fflush(stdout);
							tmpPtr += 1;
							if (tmpPtr >= CONSOLE_LINE_SIZE) break;
						}
						lineBuff[tmpPtr - 1] = ctrlC0_NUL;
						putchar(' ');
						fflush(stdout);

						int moveBack = tmpPtr - lbPtr;
						for (; moveBack > 0; moveBack--)
							PrintConsoleArrowLeft();
					}
				}
				else if (ConsoleIsArrowUp(&h->pState) || ConsoleIsArrowDown(&h->pState))
				{
					// first of all we have to set the history properly
					if (ConsoleIsArrowUp(&h->pState))
					{
						h->history.linePtr -= 1;
						if (h->history.linePtr < 0) h->history.linePtr = CONSOLE_LINE_HISTORY - 1;
					}
					else
					{
						h->history.linePtr += 1;
						if (h->history.linePtr >= CONSOLE_LINE_HISTORY) h->history.linePtr = 0;
					}

					// in case there is no "full" history, stop it
					if (h->history.linePtr == h->history.lineHead)
					{
						int inputLength = (int)strnlen(lineBuff, CONSOLE_LINE_SIZE);
						printf("\033[%dD", CONSOLE_LINE_SIZE + consoleStartIndex);
						printf("\033[%dC", consoleStartIndex);
						for (int i = 0; i < inputLength; i++)
							putchar(' ');
						memset(lineBuff, 0, CONSOLE_LINE_SIZE);
						lbPtr = 0;

						printf("\033[%dD", CONSOLE_LINE_SIZE + consoleStartIndex);
						printf("\033[%dC", consoleStartIndex);
						fflush(stdout);
					}
					else
					{
						int inputLength = (int)strnlen(lineBuff, CONSOLE_LINE_SIZE);
						printf("\033[%dD", CONSOLE_LINE_SIZE + consoleStartIndex);
						printf("\033[%dC", consoleStartIndex);
						int i = 0;
						for (; i < inputLength; i++)
							putchar(' ');

						printf("\033[%dD", CONSOLE_LINE_SIZE + consoleStartIndex);
						printf("\033[%dC", consoleStartIndex);
						fflush(stdout);

						i = 0;
						memset(lineBuff, 0, CONSOLE_LINE_SIZE);
						lbPtr = 0;
						while (h->history.lines[h->history.linePtr][i] != '\0')
						{
							putchar(h->history.lines[h->history.linePtr][i]);
							lineBuff[lbPtr] = h->history.lines[h->history.linePtr][i];
							lbPtr++;
							i++;
						}
						fflush(stdout);
					}
				}
				else goto unimp;
				break;
			}

			// all other non implemented controls
			default:
			{
			unimp:
				printf("UNIMP-CTRL-SEQ: ");
				for (int i = 0; ctrlBuff[i] != '\0'; i++)
					printf("%2.2x(%d) ", ctrlBuff[i], ctrlBuff[i]);
				printf("\r\n");
				fflush(stdout);
				break;
			}
			}


		}
	}

exit:
	while (h->cancel == 0) vTaskDelay(pdTICKS_TO_MS(100));
	
	printf("Console terminated, cleaning up...");
	fflush(stdout);

	xSemaphoreTakeRecursive(h->cState.lockGuard, -1);
	while (!LIST_EMPTY(&h->cState.commands))
	{
		cmdEntry_t* pElement = h->cState.commands.lh_first;
		if (pElement != NULL)
		{
			LIST_REMOVE(pElement, navigate);
			free(pElement);
		}
		else break;
	}

	xSemaphoreGiveRecursive(h->cState.lockGuard);
	vSemaphoreDelete(h->cState.lockGuard);
	free(h);
	
	if (lineBuff != NULL) free(lineBuff);
	if (ctrlBuff != NULL) free(ctrlBuff);
	printf("done\r\n");
destroy:
	vTaskDelete(NULL);
}

// --------------------------------------------------------------------------------------------------------------------
static int ConsolePrintHelp(int argc, char** argv, void* context)
// --------------------------------------------------------------------------------------------------------------------
{
	ConsoleHandle_t h = (ConsoleHandle_t)context;
	cmdState_t* c = &h->cState;
	int found = 0;
	int cmdLen = 0;
	if ( argc > 0 )
	{
		cmdLen = (int)strlen(argv[0]);
	}
	xSemaphoreTakeRecursive( c->lockGuard, -1 );
	cmdEntry_t* pElement = c->commands.lh_first;

	printf("HELP FOR:\r\n");
	printf("-------------------------------------------------------------------\r\n");
	while ( pElement != NULL )
	{
		// if string compare result and determined length match, then this must be the function
		if ( ( argc == 0 ) || ( strncmp(argv[0], pElement->content.cmd, cmdLen) == 0 && cmdLen == pElement->content.cmdLen ) )
		{
			found = 1;
			if ( pElement->content.isAlias ) printf("ALIAS\r\n");
			else printf("COMMAND\r\n");
			printf("%s\r\n\r\n", pElement->content.cmd);
			if ( pElement->content.isAlias )
			{
				printf("MAPPING\r\n");
				printf("%s -> '%s'\r\n", pElement->content.cmd, pElement->content.help);
			}
			else
			{
				printf("DESCRIPTION\r\n");
				printf("%s\r\n", pElement->content.help);
			}
			printf("-------------------------------------------------------------------\r\n");
		}

		pElement = pElement->navigate.le_next;
	}

	xSemaphoreGiveRecursive( c->lockGuard );
	return -(found == 0);
}

// --------------------------------------------------------------------------------------------------------------------
static int ConsoleExecReset(int argc, char** argv, void* context)
// --------------------------------------------------------------------------------------------------------------------
{
	(void)argc;
	(void)argv;
	(void)context;
#if defined(__arm__)
	NVIC_SystemReset();
	return 0;
#elif defined(WIN32)
	exit(0);
	return 0;
#else
	printf("not implemented!");
	return -1;
#endif
}

// --------------------------------------------------------------------------------------------------------------------
static int ConsolePrintKernelTicks(int argc, char** argv, void* context)
// --------------------------------------------------------------------------------------------------------------------
{
	(void)argc;
	(void)argv;
	(void)context;
	printf("%u", (unsigned int)xTaskGetTickCount());
	return 0;
}

#if defined(configGENERATE_RUN_TIME_STATS) && (configGENERATE_RUN_TIME_STATS != 0)
// --------------------------------------------------------------------------------------------------------------------
static int ConsolePrintTaskStats(int argc, char** argv, void* context)
// --------------------------------------------------------------------------------------------------------------------
{
	(void)argc;
	(void)argv;
	(void)context;
	unsigned int numTasks = (unsigned int)uxTaskGetNumberOfTasks();
	TaskStatus_t tasks[32]; // a maximium of 32 so far
	configRUN_TIME_COUNTER_TYPE totalTime = 0;

	unsigned int numFeedback = uxTaskGetSystemState( tasks, numTasks, &totalTime);
	if (numFeedback > 0)
	{
		printf("|----|----------|----------|----------|---------|------------|-------|\r\n");
		printf("| ID | NAME     | Prio     | BasePrio | State   | Ticks      | Rel.  |\r\n");
		printf("|----|----------|----------|----------|---------|------------|-------|\r\n");
	}
	for (unsigned int i = 0; i < numFeedback; i++ )
	{
		float relativeRuntime = ( (float)tasks[i].ulRunTimeCounter * 100.0f / (float)totalTime );
		char* state = (tasks[i].eCurrentState == eRunning) ? "RUN    " :
			(tasks[i].eCurrentState == eReady) ? "READY  " :
			(tasks[i].eCurrentState == eBlocked) ? "BLOCKED" :
			(tasks[i].eCurrentState == eSuspended) ? "SUSPEND" :
			(tasks[i].eCurrentState == eDeleted) ? "DELETED" : "INVALID";
		printf("| %2.2d | %-8.8s | %4.4d     | %4.4d     | %s | %10.10u | %5.1f |\r\n",
			(int)tasks[i].xTaskNumber, (char*)tasks[i].pcTaskName, (int)tasks[i].uxCurrentPriority, 
			(int)tasks[i].uxBasePriority, (char*)state, (unsigned int)tasks[i].ulRunTimeCounter,
			(float)relativeRuntime);
		printf("|----|----------|----------|----------|---------|------------|-------|\r\n");
	}

	return 0;
}
#endif

// --------------------------------------------------------------------------------------------------------------------
static int ConsolePrintKernelVersion(int argc, char** argv, void* context)
// --------------------------------------------------------------------------------------------------------------------
{
	ConsoleHandle_t h = (ConsoleHandle_t)context;
	(void)h;
	(void)argc;
	(void)argv;

	printf("FreeRTOS Kernel %s", tskKERNEL_VERSION_NUMBER);
	return 0;
}

// --------------------------------------------------------------------------------------------------------------------
static int ConsoleWhoAmI(int argc, char** argv, void* context)
// --------------------------------------------------------------------------------------------------------------------
{
	ConsoleHandle_t h = (ConsoleHandle_t)context;
	(void)h;
	(void)argc;
	(void)argv;

#if CONSOLE_USE_DYNAMIC_USERNAME != 0
	char* usernamePtr = getenv("USERNAME");
	if ( usernamePtr == 0 ) usernamePtr = CONSOLE_USERNAME;
#else
	char* usernamePtr = CONSOLE_USERNAME;
#endif

	printf("%s", usernamePtr);
	return 0;
}

// --------------------------------------------------------------------------------------------------------------------
static int ConsoleExit(int argc, char** argv, void* context)
// --------------------------------------------------------------------------------------------------------------------
{
	ConsoleHandle_t h = (ConsoleHandle_t)context;
	(void)h;
	(void)argc;
	(void)argv;

	h->cancel = 1;
	return 0;
}

//---------------------------------------------------------------------------------------------------------------------
static int ConsoleMallInfo(int argc, char** argv, void* context)
// --------------------------------------------------------------------------------------------------------------------
{
	ConsoleHandle_t h = (ConsoleHandle_t)context;
	(void)h;
	(void)argc;
	(void)argv;

#ifndef WIN32
	struct mallinfo info = mallinfo();
	printf("arena    : %d\r\n", info.arena);
	printf("ordblks  : %d\r\n", info.ordblks);
	printf("uordblks : %d\r\n", info.uordblks);
	printf("fordblks : %d\r\n", info.fordblks);
	printf("keepcost : %d\r\n", info.keepcost);
	return 0;
#else
	printf("WIN32 has quite a lot!");
	return -1;
#endif
}

//---------------------------------------------------------------------------------------------------------------------
static int ConsoleGetEnv(int argc, char** argv, void* context)
// --------------------------------------------------------------------------------------------------------------------
{
	ConsoleHandle_t h = (ConsoleHandle_t)context;
	(void)h;

	if ( argc > 0 )
	{
		char* envValue = getenv(argv[0]);
		if ( envValue != NULL )
		{
			printf("%s=%s", argv[0], envValue);
			return 0;
		}
		else
		{
			printf("%s is no environment variable", argv[0]);
			return -1;
		}
	}
	else
	{
		printf("invalid number of arguments");
		return -1;
	}
}

//---------------------------------------------------------------------------------------------------------------------
static int ConsoleSetEnv(int argc, char** argv, void* context)
// --------------------------------------------------------------------------------------------------------------------
{
	ConsoleHandle_t h = (ConsoleHandle_t)context;
	(void)h;
	if ( argc > 1 )
	{
		int result = setenv(argv[0], argv[1], 1);
		if ( result == 0 )
		{
			return 0;
		}
		else
		{
			printf("could not set %s with value %s", argv[0], argv[1]);
			return -1;
		}
	}
	else
	{
		printf("invalid number of arguments");
		return -1;
	}
}

// --------------------------------------------------------------------------------------------------------------------
static int ConsoleAliasConfig(int argc, char** argv, void* context)
// --------------------------------------------------------------------------------------------------------------------
{
	ConsoleHandle_t h = (ConsoleHandle_t)context;

	if ( argc == 0 )
	{
		printf("invalid number of arguments");
		return -1;
	}

	if( argc == 1 )
	{
		if ( CONSOLE_RemoveAliasOrCommand(h, argv[0]) == 0 )
		{
			printf("alias removed successfully");
			return 0;
		}
		else
		{
			printf("alias was not removed");
			return -1;
		}
	}
	else
	{
		char aliasBuffer[CONSOLE_LINE_SIZE];
		unsigned int cmdPtr = 1;
		unsigned int buffPtr = 0;
		memset(aliasBuffer, 0, sizeof(aliasBuffer));
		for( unsigned int i = 0; i < sizeof(aliasBuffer)/sizeof(*aliasBuffer); i++)
		{
			if ( cmdPtr < (unsigned int)argc )
			{
				int argLen = strnlen(argv[cmdPtr], CONSOLE_LINE_SIZE);
				if ( argLen > 0 )
				{
					if ( (buffPtr+1) + argLen >= CONSOLE_LINE_SIZE )
					{
						printf("the sum of the alias parameters is longer than the max line buffer size!");
						return -1;
					}
					else
					{
						memcpy(&aliasBuffer[buffPtr], argv[cmdPtr], argLen);
						buffPtr += argLen;
						if ( ( cmdPtr + 1) != (unsigned int)argc )
						{
							aliasBuffer[buffPtr] = ' ';
							buffPtr += 1;
						}
					}
				}
				else
				{
					printf("at least one of the alias parameters is too long for mapping");
					return -1;
				}
			}
			cmdPtr++;
		}
		if ( CONSOLE_RegisterAlias(h, argv[0], aliasBuffer) == 0 )
		{
			printf("alias created successfully");
			return 0;
		}
		else
		{
			printf("alias was not created");
			return -1;
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------
static void ConsoleRegisterBasicCommands( ConsoleHandle_t h )
// --------------------------------------------------------------------------------------------------------------------
{
	CONSOLE_RegisterCommand(h, "help",     "<<help>> prints the help of all commands.\r\nIf a <<command>> is passed as argument to help,\r\nonly the help text of this command will be printed on the console.",
			ConsolePrintHelp, h);
	CONSOLE_RegisterCommand(h, "version",  "<<version>> prints the kernel version of the FreeRTOS kernel with which\r\n the current project has been built.",
			ConsolePrintKernelVersion, h);
	CONSOLE_RegisterCommand(h, "whoami",   "<<whoami>> prints the current username which is set in this console instance",
			ConsoleWhoAmI, h);
	CONSOLE_RegisterCommand(h, "mallinfo", "<<mallinfo>> returns a structure describing the current state of\r\nmemory allocation.  The structure is defined in malloc.h.  The\r\nfollowing fields are defined: <<arena>> is the total amount of space\r\nin the heap; <<ordblks>> is the number of chunks which are not in use;\r\n<<uordblks>> is the total amount of space allocated by <<malloc>>;\r\n<<fordblks>> is the total amount of space not in use; <<keepcost>> is\r\nthe size of the top most memory block.\r\n",
			ConsoleMallInfo, h);
	CONSOLE_RegisterCommand(h, "getenv",   "<<getenv>> searches the list of environment variable names and values\r\n(using the global pointer ``<<char **environ>>'') for a variable whose\r\nname matches the string at <[name]>.  If a variable name matches,\r\n<<getenv>> returns a pointer to the associated value.",
			ConsoleGetEnv, h);
	CONSOLE_RegisterCommand(h, "setenv",   "<<setenv>> is similar to <<getenv>> but it sets a global variable\r\nin the list of environment variable names and values",
			ConsoleSetEnv, h);
	CONSOLE_RegisterCommand(h, "exit",     "<<exit>> leaves the running console instance and stops the consoel thread.\r\n It clears all given resources.\r\nThere is no console support after calling <<exit>> anymore",
			ConsoleExit, h);
	CONSOLE_RegisterCommand(h, "reset",     "<<reset>> executes a soft reset of the MCU.",
			ConsoleExecReset, h);
	CONSOLE_RegisterCommand(h, "ticks",     "<<ticks>> prints the time elapsed since system\r\nis running in the unit of ticks.",
			ConsolePrintKernelTicks, h);
	CONSOLE_RegisterCommand(h, "alias",     "<<alias>>",
			ConsoleAliasConfig, h);
#if defined(configGENERATE_RUN_TIME_STATS) && (configGENERATE_RUN_TIME_STATS != 0)
	CONSOLE_RegisterCommand(h, "tasks",     "<<tasks>> prints information about the active tasks\r\nand prints also runtime information.",
		ConsolePrintTaskStats, h);
#endif
}

// --------------------------------------------------------------------------------------------------------------------
ConsoleHandle_t CONSOLE_CreateInstance( unsigned int uxStackDepth, int xPrio )
// --------------------------------------------------------------------------------------------------------------------
{
#define ON_NULL_GOTO_ERROR(x) do { if ((x) == NULL) goto error; } while(0);
	struct ConsoleHandle* h = calloc(sizeof(struct ConsoleHandle), 1);
	ON_NULL_GOTO_ERROR(h);

	h->cState.lockGuard = xSemaphoreCreateRecursiveMutex();
	ON_NULL_GOTO_ERROR(h->cState.lockGuard);
	h->pState.state = ctrlpsIDLE_DETECT;
	h->pState.length = 0;
	h->pState.maxLength = CONSOLE_LINE_SIZE;
	h->pState.type = ctrlUNKNOWN;
	h->pState.buff = NULL;
	h->cancel = 0;
	h->pendingRedirect = 0;
	h->pendingRdStream = NULL;
	h->pendingWrStream = NULL;

	LIST_INIT(&h->cState.commands);
	ConsoleRegisterBasicCommands(h);

	memset(h->history.lines, 0, sizeof(h->history.lines));
	h->history.linePtr = h->history.lineHead = 0;

	xTaskCreate(ConsoleFunction, "console", uxStackDepth, h, xPrio, &h->tHandle);
	ON_NULL_GOTO_ERROR(h->tHandle);
	return h;

error:
	if ( h != NULL )
	{
		if ( h->cState.lockGuard != NULL )
		{
			vSemaphoreDelete(h->cState.lockGuard);
			h->cState.lockGuard = NULL;
		}

		free(h);
	}

	return NULL;
}

// --------------------------------------------------------------------------------------------------------------------
int CONSOLE_RegisterCommand( ConsoleHandle_t h, char* cmd, char* help, CONSOLE_CommandFunc func, void* context )
// --------------------------------------------------------------------------------------------------------------------
{
	int result = -1;
	if ( cmd == NULL || help == NULL || func == NULL ) return result;
	if ( *cmd == '\0' || *help == '\0' ) return result;
	int cmdLen  = 0;
	int helpLen = 0;
	if ( (cmdLen  = (int)strnlen(cmd, CONSOLE_COMMAND_MAX_LENGTH+1) )   > CONSOLE_COMMAND_MAX_LENGTH  ) return result;
	if ( (helpLen = (int)strnlen(help, CONSOLE_HELP_MAX_LENGTH+1) ) > CONSOLE_HELP_MAX_LENGTH ) return result;

	// could be called while the scheduler is not running or suspended, so we must not use to use the lock guard
	if ( taskSCHEDULER_RUNNING == xTaskGetSchedulerState() ) xSemaphoreTakeRecursive( h->cState.lockGuard, -1 );

	cmdState_t* c = &h->cState;
	int found = 0;
	cmdEntry_t* pElement = c->commands.lh_first;
	while ( pElement != NULL )
	{
		// if string compare result and determined length match, then this must be the function
		if ( strncmp(cmd, pElement->content.cmd, cmdLen) == 0 && cmdLen == pElement->content.cmdLen )
		{
			found = 1;
			break;
		}
		pElement = pElement->navigate.le_next;
	}

	if ( found == 1 )
	{
		result = -1;
	}
	else
	{
		struct cmdEntry *item = malloc(sizeof(struct cmdEntry));
		if (item == NULL) return result;
		item->content.isAlias = 0;
		item->content.cmdLen  = cmdLen;
		item->content.helpLen = helpLen;
		item->content.func    = func;
		item->content.ctx     = context;
		memcpy(item->content.cmd, cmd, cmdLen);
		item->content.cmd[cmdLen] = '\0';
		memcpy(item->content.help, help, helpLen);
		item->content.help[helpLen] = '\0';
		LIST_INSERT_HEAD(&h->cState.commands, item, navigate);
		result = 0;
	}

	// could be called while the scheduler is not running or suspended, so we must not use to use the lock guard
	if ( taskSCHEDULER_RUNNING == xTaskGetSchedulerState() ) xSemaphoreGiveRecursive( h->cState.lockGuard );
	return result;
}


// --------------------------------------------------------------------------------------------------------------------
int CONSOLE_RegisterAlias( ConsoleHandle_t h, char* cmd, char* aliasCmd )
// --------------------------------------------------------------------------------------------------------------------
{
	int result = -1;
	if ( cmd == NULL || aliasCmd == NULL ) return result;
	if ( *cmd == '\0' || *aliasCmd == '\0' ) return result;
	int cmdLen  = 0;
	int aliasCmdLen = 0;
	if ( (cmdLen      = (int)strnlen(cmd, CONSOLE_COMMAND_MAX_LENGTH+1) )      > CONSOLE_COMMAND_MAX_LENGTH ) return result;
	if ( (aliasCmdLen = (int)strnlen(aliasCmd, CONSOLE_COMMAND_MAX_LENGTH+1) ) > CONSOLE_COMMAND_MAX_LENGTH ) return result;

	// could be called while the scheduler is not running or suspended, so we must not use to use the lock guard
	if ( taskSCHEDULER_RUNNING == xTaskGetSchedulerState() ) xSemaphoreTakeRecursive( h->cState.lockGuard, -1 );

	cmdState_t* c = &h->cState;
	int found = 0;
	cmdEntry_t* pElement = c->commands.lh_first;
	while ( pElement != NULL )
	{
		// if string compare result and determined length match, then this must be the function
		if ( strncmp(cmd, pElement->content.cmd, cmdLen) == 0 && cmdLen == pElement->content.cmdLen )
		{
			found = 1;
			break;
		}
		pElement = pElement->navigate.le_next;
	}

	if ( found == 1 )
	{
		result = -1;
	}
	else
	{
		struct cmdEntry *item = malloc(sizeof(struct cmdEntry));
		if (item == NULL) return result;
		item->content.isAlias = 1;
		item->content.cmdLen  = cmdLen;
		item->content.helpLen = aliasCmdLen;
		item->content.func    = NULL;
		item->content.ctx     = NULL;
		memcpy(item->content.cmd, cmd, cmdLen);
		item->content.cmd[cmdLen] = '\0';
		memcpy(item->content.help, aliasCmd, aliasCmdLen);
		item->content.help[aliasCmdLen] = '\0';
		LIST_INSERT_HEAD(&h->cState.commands, item, navigate);
		result = 0;
	}

	// could be called while the scheduler is not running or suspended, so we must not use to use the lock guard
	if ( taskSCHEDULER_RUNNING == xTaskGetSchedulerState() ) xSemaphoreGiveRecursive( h->cState.lockGuard );
	return result;
}

// --------------------------------------------------------------------------------------------------------------------
int CONSOLE_RemoveAliasOrCommand( ConsoleHandle_t h, char* cmd)
// --------------------------------------------------------------------------------------------------------------------
{
	int result = -1;
	if ( cmd == NULL ) return result;
	if ( *cmd == '\0' ) return result;
	int cmdLen  = 0;
	if ( (cmdLen      = (int)strnlen(cmd, CONSOLE_COMMAND_MAX_LENGTH+1) ) > CONSOLE_COMMAND_MAX_LENGTH ) return result;

	// could be called while the scheduler is not running or suspended, so we must not use to use the lock guard
	if ( taskSCHEDULER_RUNNING == xTaskGetSchedulerState() ) xSemaphoreTakeRecursive( h->cState.lockGuard, -1 );

	cmdState_t* c = &h->cState;
	int found = 0;
	cmdEntry_t* pElement = c->commands.lh_first;
	while ( pElement != NULL )
	{
		// if string compare result and determined length match, then this must be the function
		if ( strncmp(cmd, pElement->content.cmd, cmdLen) == 0 && cmdLen == pElement->content.cmdLen )
		{
			found = 1;
			break;
		}
		pElement = pElement->navigate.le_next;
	}

	if ( found == 1 )
	{
		LIST_REMOVE(pElement, navigate);
		free(pElement);
		result = 0;
	}

	// could be called while the scheduler is not running or suspended, so we must not use to use the lock guard
	if ( taskSCHEDULER_RUNNING == xTaskGetSchedulerState() ) xSemaphoreGiveRecursive( h->cState.lockGuard );
	return result;
}

// --------------------------------------------------------------------------------------------------------------------
void CONSOLE_DestroyInstance( ConsoleHandle_t h )
// --------------------------------------------------------------------------------------------------------------------
{
	h->cancel = 1;
}
