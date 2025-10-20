/*
 * Spindle.c
 *
 *  Created on: Feb 10, 2025
 *      Author: Thorsten
 */

 /*! \file */

#include "Spindle.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "timers.h"

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <math.h>

// singleton instance pointer
// --------------------------------------------------------------------------------------------------------------------
static SpindleHandle_t SpindleInstancePointer = NULL;

// --------------------------------------------------------------------------------------------------------------------
typedef enum
// --------------------------------------------------------------------------------------------------------------------
{
	cctNONE      = 0x00,
	cctSTART     = 0x01,
	cctSTOP      = 0x02,
	cctSTATUS    = 0x04,
} CtrlCommandType_t;

// --------------------------------------------------------------------------------------------------------------------
typedef struct StepCommandResponse
// --------------------------------------------------------------------------------------------------------------------
{
	int code;
	int requestID;
	union
	{
		struct
		{
			float speed;
			int running;
		} asStatus;
	} args;
} StepCommandResponse_t;

// --------------------------------------------------------------------------------------------------------------------
typedef struct CtrlCommand
// --------------------------------------------------------------------------------------------------------------------
{
	struct
	{
		int requestID;
		CtrlCommandType_t type;
	} head;
	struct
	{
		union
		{
			struct
			{
				float speed;
			} asStart;
		} args;
		SemaphoreHandle_t syncEvent;
	} request;
	StepCommandResponse_t* response;
} CtrlCommand_t;

// --------------------------------------------------------------------------------------------------------------------
typedef struct stepSyncEventElement
// --------------------------------------------------------------------------------------------------------------------
{
    struct
	{
    	int               allocated;
    	SemaphoreHandle_t event;
	} content;

    LIST_ENTRY(stepSyncEventElement) navigate;
} stepSyncEventElement_t;

// --------------------------------------------------------------------------------------------------------------------
struct SpindleHandle
// --------------------------------------------------------------------------------------------------------------------
{
	int               nextRequestID;
	ConsoleHandle_t   consoleH;
	TaskHandle_t      tHandle;
	QueueHandle_t     cmdQueue;
	int               cancel;
	SpindlePhysicalParams_t physical;
	float             currentSpeed;
	struct
	{
		SemaphoreHandle_t lockGuard;
		LIST_HEAD(pool_list, stepSyncEventElement) pool;
	} syncEventPool;
};

// --------------------------------------------------------------------------------------------------------------------
static void SpindleFunction( void * arg )
// --------------------------------------------------------------------------------------------------------------------
{
	CtrlCommand_t cmd;
	StepCommandResponse_t asyncResponse;
	SpindleHandle_t h = (SpindleHandle_t)arg;
	unsigned int running = 0;
	unsigned int startupBoost = 0;

	h->physical.enaPWM(h, h->physical.context, 0);
	h->physical.setDutyCycle(h, h->physical.context, 0.0f );
	h->currentSpeed = 0;

	// now here comes the command processor part
	while( !h->cancel )
	{
		// wait for next command
		if ( xQueueReceive( h->cmdQueue, &cmd, 100) == pdPASS )
		{
			if ( cmd.response == NULL || cmd.request.syncEvent == NULL )
			{
				cmd.response = &asyncResponse;
			}
			memset(cmd.response, 0, sizeof(StepCommandResponse_t));
			cmd.response->code = -1;
			cmd.response->requestID = cmd.head.requestID;


			switch ( cmd.head.type )
			{
			case cctNONE:
				cmd.response->code = 0;
				break;
			case cctSTART:
				cmd.response->code = 0;
				if ( cmd.request.args.asStart.speed < h->physical.minRPM ) cmd.request.args.asStart.speed = h->physical.minRPM;
				if ( cmd.request.args.asStart.speed > h->physical.maxRPM ) cmd.request.args.asStart.speed = h->physical.maxRPM;

				if (  cmd.request.args.asStart.speed > 0.0f && cmd.request.args.asStart.speed <  h->physical.absMinRPM ) cmd.request.args.asStart.speed =  h->physical.absMinRPM;
				if (  cmd.request.args.asStart.speed < 0.0f && cmd.request.args.asStart.speed > -h->physical.absMinRPM ) cmd.request.args.asStart.speed = -h->physical.absMinRPM;

				int directionChange = 0;
				if ((h->currentSpeed < 0.0f && cmd.request.args.asStart.speed > 0.0f) ||
					(h->currentSpeed > 0.0f && cmd.request.args.asStart.speed < 0.0f))
					directionChange = 1;
				h->currentSpeed = cmd.request.args.asStart.speed;


				h->physical.setDirection(h, h->physical.context, h->currentSpeed < 0.0f );
				if ( running == 1 && directionChange == 0 )
				{
					h->physical.setDutyCycle(h, h->physical.context, ( fabsf(h->currentSpeed) / h->physical.maxRPM) );
					startupBoost = 0;
				}
				else if ( ( running == 0 || directionChange == 1 ) && fabsf(cmd.request.args.asStart.speed) <= (0.25f * h->physical.maxRPM) )
				{
					h->physical.setDutyCycle(h, h->physical.context, 0.5f );
					startupBoost = 1;
				}
				if ( startupBoost == 0 )
				{
					h->physical.setDutyCycle(h, h->physical.context, ( fabsf(h->currentSpeed) / h->physical.maxRPM ) );
				}

				h->physical.enaPWM(h, h->physical.context, 1);
				running = 1;
				break;
			case cctSTOP:
				cmd.response->code = 0;
				h->currentSpeed = 0;
				startupBoost = 0;
				running = 0;
				h->physical.setDutyCycle(h, h->physical.context, 0.0f );
				h->physical.enaPWM(h, h->physical.context, 0);
				break;
			case cctSTATUS:
				cmd.response->code = 0;
				cmd.response->args.asStatus.running = running;
				cmd.response->args.asStatus.speed = h->currentSpeed;
				break;
			default:
				break;
			}

			// after processing the command we have to release the caller to keep
			// synchronous calling mechanism. In case there is no sync object, it was##
			// called asynchronously
			if ( cmd.request.syncEvent != NULL )
			{
				xSemaphoreGive(cmd.request.syncEvent);
			}
		}
		else
		{
			// here we have to do some additional steps to regulate correct rpm in case
			// the low speed boost has been performed
			if ( startupBoost == 1 && running == 1 )
			{
				startupBoost = 0;
				h->physical.setDutyCycle(h, h->physical.context, ( fabsf(h->currentSpeed) / h->physical.maxRPM) );
			}
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------
static SemaphoreHandle_t GetCommandEvent( SpindleHandle_t h )
// --------------------------------------------------------------------------------------------------------------------
{
	xSemaphoreTakeRecursive( h->syncEventPool.lockGuard, -1 );

	stepSyncEventElement_t* el = LIST_FIRST(&h->syncEventPool.pool);
	while ( el != NULL )
	{
		if ( el->content.allocated == 0 )
		{
			el->content.allocated = 1;
			// make sure we the event is in held state
			xSemaphoreTake( el->content.event, 0 );
			xSemaphoreGiveRecursive( h->syncEventPool.lockGuard );
			return el->content.event;
		}
		el = LIST_NEXT(el, navigate);
	}

	xSemaphoreGiveRecursive( h->syncEventPool.lockGuard );
	return 0;
}

// --------------------------------------------------------------------------------------------------------------------
static void ReleaseCommandEvent( SpindleHandle_t h, SemaphoreHandle_t s )
// --------------------------------------------------------------------------------------------------------------------
{
	xSemaphoreTakeRecursive( h->syncEventPool.lockGuard, -1 );

	stepSyncEventElement_t* el = LIST_FIRST(&h->syncEventPool.pool);
	while ( el != NULL )
	{
		if ( el->content.allocated == 1 && el->content.event == s)
		{
			el->content.allocated = 0;
			xSemaphoreGiveRecursive( h->syncEventPool.lockGuard );
			return;
		}
		el = LIST_NEXT(el, navigate);
	}

	xSemaphoreGiveRecursive( h->syncEventPool.lockGuard );
}

// --------------------------------------------------------------------------------------------------------------------
static int SpindleConsoleFunction( int argc, char** argv, void* ctx )
// --------------------------------------------------------------------------------------------------------------------
{
	//possible commands are
	//(spindle) start 100
	//(spindle) stop
	//(spindle) status

	SpindleHandle_t h = (SpindleHandle_t)ctx;
	StepCommandResponse_t response = { 0 };
	CtrlCommand_t cmd;

	cmd.response       = &response;
	cmd.head.requestID = h->nextRequestID;
	h->nextRequestID += 1;

	// first decode the subcommand and all arguments
	if ( argc == 0 )
	{
		printf("invalid number of arguments\r\nFAIL");
		return -1;
	}
	if ( strcmp(argv[0], "stop") == 0 )
	{
		// no further arguments
		cmd.head.type = cctSTOP;
	}
	else if ( strcmp(argv[0], "start") == 0 )
	{
		// rpm value directly after start
		cmd.head.type = cctSTART;
		cmd.request.args.asStart.speed    = 600.0f;
		if ( argc < 2 )
		{
			printf("missing RPM value for start command\r\nFAIL");
			return -1;
		}

		cmd.request.args.asStart.speed = (float)atof(argv[1]);
	}
	else if ( strcmp(argv[0], "status") == 0 )
	{
		// no further arguments, everything in result
		cmd.head.type = cctSTATUS;
	}
	else
	{
		printf("passed invalid sub command\r\nFAIL");
		return -1;
	}

	// now pass the request to the controller
	cmd.request.syncEvent = GetCommandEvent(h);

	if ( pdPASS != xQueueSend( h->cmdQueue, &cmd, -1 ) )
	{
		ReleaseCommandEvent(h, cmd.request.syncEvent );
		return -1;
	}

	xSemaphoreTake( cmd.request.syncEvent, -1 );
	ReleaseCommandEvent(h, cmd.request.syncEvent );

	// now decode the result in case there is one
	if ( response.code == 0 )
	{
		if ( cmd.head.type == cctSTATUS )
		{
			printf("%d\r\n", !!cmd.response->args.asStatus.running);
			printf("%d\r\n", (int)cmd.response->args.asStatus.speed);
		}
		printf("OK");
	}
	else
	{
		printf("error returned\r\nFAIL");
	}

	// now back to console
	return response.code;
}

// --------------------------------------------------------------------------------------------------------------------
static void SpindleRegisterBasicCommands( SpindleHandle_t h, ConsoleHandle_t cH )
// --------------------------------------------------------------------------------------------------------------------
{
	CONSOLE_RegisterCommand(cH, "spindle", "<<spindle>> is used to control a spindle motor.\r\nValid subcommands are start, stop, status.\r\nStart needs an additional RPM argument!",
			SpindleConsoleFunction, h);
}

// --------------------------------------------------------------------------------------------------------------------
SpindleHandle_t SPINDLE_CreateInstance( unsigned int uxStackDepth, int xPrio, ConsoleHandle_t cH, SpindlePhysicalParams_t* p )
// --------------------------------------------------------------------------------------------------------------------
{
#define ON_NULL_GOTO_ERROR(x) do { if ((x) == NULL) goto error; } while(0);
	// singleton pattern
	if ( SpindleInstancePointer != NULL ) return SpindleInstancePointer;

	if ( p == NULL || p->enaPWM == NULL || p->setDirection == NULL ||
	     p->minRPM >= p->maxRPM || p->setDutyCycle == NULL || cH == NULL )
		return NULL;

	struct SpindleHandle* h = calloc(sizeof(struct SpindleHandle), 1);
	ON_NULL_GOTO_ERROR(h);

	if ( h == NULL ) return NULL;
	h->consoleH = cH;
	h->cancel = 0;
	h->nextRequestID = 0;
	h->cmdQueue = xQueueCreate(16, sizeof(CtrlCommand_t));
	ON_NULL_GOTO_ERROR(h->cmdQueue);

	// copy arguments
	memcpy(&h->physical, p, sizeof(SpindlePhysicalParams_t));

	// now we create the sync event pool
	LIST_INIT(&h->syncEventPool.pool);
	h->syncEventPool.lockGuard = xSemaphoreCreateRecursiveMutex();
	ON_NULL_GOTO_ERROR(h->syncEventPool.lockGuard);
	for ( int i = 0; i < 8; i++)
	{
		stepSyncEventElement_t* el = (stepSyncEventElement_t*)calloc(sizeof(stepSyncEventElement_t), 1);
		ON_NULL_GOTO_ERROR(el);
		el->content.allocated = 0;
		el->content.event = xSemaphoreCreateBinary();
		if (el->content.event == NULL)
		{
			free(el);
			el = NULL;
			goto error;
		}
		else
		{
			LIST_INSERT_HEAD(&h->syncEventPool.pool, el, navigate);
		}
	}

	// setup the console commands
	SpindleRegisterBasicCommands(h, cH);
	SpindleInstancePointer = h;

	// setup the task which handles all communications and the RPM generation
	xTaskCreate(SpindleFunction, "spindlectrl", uxStackDepth, h, xPrio, &h->tHandle);
	ON_NULL_GOTO_ERROR(h->tHandle);
	return h;

error:
	if (h != NULL)
	{
		if (h->cmdQueue != NULL)
		{
			vQueueDelete(h->cmdQueue);
			h->cmdQueue = NULL;
		}

		if (h->syncEventPool.lockGuard != NULL)
		{
			vSemaphoreDelete(h->syncEventPool.lockGuard);
			h->syncEventPool.lockGuard = NULL;
		}

		// first clean all event elements
		stepSyncEventElement_t* el = NULL;
		stepSyncEventElement_t* tel = NULL;
		for (el = LIST_FIRST(&h->syncEventPool.pool); el && (tel = LIST_NEXT(el, navigate), 1); el = tel)
		{
			if (el->content.event != NULL)
			{
				vSemaphoreDelete(el->content.event);
				el->content.event = NULL;
			}
		}

		// now remove all elements one by one from the list and free them
		while (!LIST_EMPTY(&h->syncEventPool.pool))
		{
			stepSyncEventElement_t* el = LIST_FIRST(&h->syncEventPool.pool);
			if (el != NULL)
			{
				LIST_REMOVE(el, navigate);
				free(el);
			}
		}

		free(h);
	}

	return NULL;
}
