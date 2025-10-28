/*
 * Console.h
 *
 *  Created on: Dec 8, 2024
 *      Author: Thorsten
 */

 /*! \file */

#ifndef INC_CONSOLE_CONSOLE_H_
#define INC_CONSOLE_CONSOLE_H_

/*!
 * The ConsoleHandle_t handle is an instance pointer of the console library which is generated whenever
 * the CONSOLE_CreateInstance function returns with success.
 */
typedef struct ConsoleHandle* ConsoleHandle_t;


/*!
 * The CONSOLE_CommandFunc function pointer type is used to describe the arguments which are passed by the
 * console processor toi the registered console function. The user gets the classical argc and argv arguments
 * which are known well from a standard main in nearly any programming language. There is an additional
 * context pointer, which optionally holds additional information. The pointer to this information is passed
 * with the registration of the command by the user and can not be changed anymore at runtime
 * 
 * The return value of a console function is always zero on success or any non zero number when an error occured.
 * The standard is more or less a non zero number which is smaller than 0.
 */
typedef int (*CONSOLE_CommandFunc)(int argc, char** argv, void* context);

/*!
 * ConsoleReadStream_t should return -1 on failure, or else the number of bytes
 * read (0 on EOF).  It is similar to syscall <<read>>, except that <int> rather
 * than <size_t> bounds a transaction size, and <[context]> will be passed
 * as the first argument.
 */
typedef int (*ConsoleReadStream_t)(void* pContext, char* pBuffer, int num);

/*!
 * ConsoleWriteStream_t should return -1 on failure, or else the number of bytes
 * written.  It is similar to <<write>>, except that <int> rather than
 * <size_t> bounds a transaction size, and <[cookie]> will be passed as
 * the first argument.
 */
typedef int (*ConsoleWriteStream_t)(void* pContext, const char* pBuffer, int num);


/*!
 * The CONSOLE_CreateInstance function is used to create the console processor. There is no singleton pattern implemented
 * for the console but as there is no stream abstraction for the console processor, it only makes sense to have one instance
 * at runtime!
 *
 * The return value of a console function is a null pointer in case an error occured or a pointer of type ConsoleHandle_t.
 * 
 * @param uxStackDepth is the stack depth of the console processor thread in words
 * @param xPrio is the console processor priority. This should always be on a low level because the stdlib function
 * may not block and so the processor always runs whenever it can. In case the reception of the newlib is blocking and
 * interrupt based, the priority could be higher!
 */
ConsoleHandle_t CONSOLE_CreateInstance(  unsigned int uxStackDepth, int xPrio );

/*!
 * The CONSOLE_DestroyInstance function is used to cleanup all used ressources which then stops the console processor.
 * This leads to the case that no console functionallity is provided in the design anymore
 */
void CONSOLE_DestroyInstance( ConsoleHandle_t h );

/*!
 * The CONSOLE_RegisterCommand function is used to register custom commands which can be called by the console processor
 * when the user enters the given command string. The arguments and an additional context pointer are passed by the console
 * processor to the function as well.
 * 
 * @param h is of type ConsoleHandle_t which is created by a call of CONSOLE_CreateInstance
 * @param cmd is of type char* which is the case sensitive name of the command
 * @param help is of type char* which is the help text or description of the command when the user types help
 * @param func is of type CONSOLE_CommandFunc which is the function pointer to the command
 * @param context is of type void* which is an optional data pointer which is passed to the function when called
 */
int CONSOLE_RegisterCommand( ConsoleHandle_t h, char* cmd, char* help, CONSOLE_CommandFunc func, void* context );

/*!
 * The CONSOLE_RegisterAlias function is used to register custom commands which can be mapped to other commands
 * when the user enters the given alias command string. Arguments will not be passed from an alias command to
 * the mapped command, so the alias should be specified with the required commands accordingly
 *
 * @param h is of type ConsoleHandle_t which is created by a call of CONSOLE_CreateInstance
 * @param cmd is of type char* which is the case sensitive name of the alias command
 * @param aliasCmd is of type char* which is the mapped command and its args and is visible when the user types help
 */
int CONSOLE_RegisterAlias( ConsoleHandle_t h, char* cmd, char* aliasCmd );

/*!
 * The CONSOLE_RemoveAliasOrCommand function is used to remove custom commands or alias entries which are mapped
 * in the console processor.
 *
 * @param h is of type ConsoleHandle_t which is created by a call of CONSOLE_CreateInstance
 * @param cmd is of type char* which is the case sensitive name of the alias or command
 */
int CONSOLE_RemoveAliasOrCommand( ConsoleHandle_t h, char* cmd);

/*!
 * The CONSOLE_RedirectStreams function is used to change stdin or stdout as default
 * streams for the console functions. In case one or both stream function pointers are
 * NULL, it will be set back to stdin respectively stdout. In case of an error, the
 * last stream configuration is kept and -1 is returned, otherwise 0
 *
 * @param h is of type ConsoleHandle_t which is created by a call of CONSOLE_CreateInstance
 * @param rdFunc is of type ConsoleReadStream_t which is the new stream read function for the command processor
 * @param wrFunc is of type ConsoleWriteStream_t which is the new stream write function for the command processor
 *
 * NOTE it might be that not on all platforms it is possible to use this function, it then returns -2 if it is
 * not implemented properly. So far only newlib stdlib is supported!
 */
int CONSOLE_RedirectStreams( ConsoleHandle_t h, ConsoleReadStream_t rdFunc, ConsoleWriteStream_t wrFunc,
		void* rdContext, void* wrContext );
/*!
 * \mainpage FreeRTOS Console Library
 * \section intro_sec Introduction
 *
 * The following code documentation is a set of tipps and diagramms as well as function and structure descriptions
 * which help when using the library. It is used to increase the implementation speed. Some examples are attached
 * in this documentation as well
 *
 * \section static_sec static compile flags of the library
 *
 * CONSOLE_USERNAME: The username in the console which is printed at least on power up before changing the username<br>
 * CONSOLE_USE_DYNAMIC_USERNAME: The username can be changed on runtime which is enabled with this DEFINE<br>
 * CONSOLE_LINE_HISTORY: specifies the maximum number of entries of the "last commands" buffer   <br>
 * CONSOLE_LINE_SIZE: Specifies the number of chars per line. When exceeding this number without a newline,<br>
 * the console prints a buffer overflow and clears the input. All inputs are then invalidated within<br>
 * this line<br>
 * CONSOLE_COMMAND_MAX_LENGTH: Specifies the maximum number of chars per command<br>
 * CONSOLE_HELP_MAX_LENGTH: Specifies the maximum number of chars per command help text<br>
 * 
 * \section state_example Examples
 * The following example shows how to create a instance of the console library
 * and how to attach a function for console usage. This is called registering a command
 * 
 * \code
 * 
 * static int CapabilityFunc( int argc, char** argv, void* ctx )
 * {
 * 	printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\nOK",
 * 	    1, // has spindle
 * 		1, // has spindle status
 * 		1, // has stepper
 * 		1, // has stepper move relative
 * 		1, // has stepper move speed
 * 		1, // has stepper move async
 * 		1, // has stepper status
 * 		1, // has stepper refrun
 * 		1, // has stepper refrun timeout
 * 		1, // has stepper refrun skip
 * 		1, // has stepper refrun stay enabled
 * 		1, // has stepper reset
 * 		1, // has stepper position
 * 		1, // has stepper config
 * 		1, // has stepper config torque
 * 		1, // has stepper config throvercurr
 * 		1, // has stepper config powerena
 * 		1, // has stepper config stepmode
 * 		1, // has stepper config timeoff
 * 		1, // has stepper config timeon
 * 		1, // has stepper config timefast
 * 		1, // has stepper config mmperturn
 * 		1, // has stepper config posmax
 * 		1, // has stepper config posmin
 * 		1, // has stepper config posref
 * 		1, // has stepper config stepsperturn
 * 		1  // has stepper cancel
 * 	);
 * 	return 0;
 * }
 * 
 * ...
 * 
 * // create the console processor. There are no additional arguments required because it uses stdin, stderr and
 * // stdout of the stdlib of the platform
 * ConsoleHandle_t c = CONSOLE_CreateInstance( 4*configMINIMAL_STACK_SIZE, configMAX_PRIORITIES - 5  );
 * 
 * // register the function, there is always a help text required, an empty string or null is not allowed!
 * CONSOLE_RegisterCommand(c, "capability", "prints a specified string of capability bits",
 *   CapabilityFunc, NULL);
 * 
 * ...
 * 
 * \endcode
 *
 * The following example shows a really simple console function and how
 * it is used to work with the console and arguments passed by the user
 *
 * \code
 *
 * static int ConsoleFunction( int argc, char** argv, void* ctx )
 * {
 *   //possible commands are
 *   //(command) start
 *   //(command) stop
 *   
 *   // first decode the subcommand and all arguments
 *   if ( argc == 0 )
 *   {
 *   	printf("invalid number of arguments\r\nFAIL");
 *   	return -1;
 *   }
 *   if ( strcmp(argv[0], "stop") == 0 )
 *   {
 *   	// do something
 *   }
 *   else if ( strcmp(argv[0], "start") == 0 )
 *   {
 *   	// do something else
 *   }
 *   else
 *   {
 *   	printf("invalid subcommand was given as argument\r\nFAIL");
 *   }
 * }
 *
 * \endcode
 */


 /*!
 * \page lib_port_page Library porting
 * \section intro_sec Introduction
 *
 * The library is written with a minimum set of compiler dependencies to make it portable on most toolchains
 * and most compilers which are able to compile C. There are no GCC attributes or MSVC pragmas included in the code
 * furthermore there are no weak functions or other non ISO-C language keywords which are not parsable from
 * all compilers. To include the library in a project, the following page helps to do the job and can be used as
 * a step by step quick porting guide.
 *
 * \section step_by_step_sec required steps to include the library
 * -# include all include paths of the library folder (inc-folder) to your project or copy them into your project as needed
 * -# link all C files (src-folder) to your project or copy them to your project as needed
 * -# adapt your makefile in case you have one. When using eclipse project this is not required
 * -# copy the template header ConsoleConfig.h file to your project or create your own and adapt the compile time @parameters as needed.
 * -# compile the source code once to make sure everything builds without issues and your include paths are correct.
 * -# make sure FreeRTOS is implemented properly and compiles with the defualt include paths.
 * -# no error should be returned after the call of CONSOLE_CreateInstance. Porting is done.
 */


#endif /* INC_CONSOLE_CONSOLE_H_ */
