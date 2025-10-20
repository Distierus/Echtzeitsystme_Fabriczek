/*
 * Controller.h
 *
 *  Created on: Dec 9, 2024
 *      Author: Thorsten
 */

 /*! \file */

#ifndef INC_SPINDLE_CONTROLLER_H_
#define INC_SPINDLE_CONTROLLER_H_

#include "Console.h"

/*!
 * The SpindleHandle_t handle is an instance pointer of the spindle library which is generated whenever
 * the SPINDLE_CreateInstance function returns with success.
 */
typedef struct SpindleHandle* SpindleHandle_t;

/*!
 * The SpindlePhysicalParams_t structure is represents the abstraction functions and members as a container.
 * The objetcs are passed as structure pointer when calling SPINDLE_CreateInstance. It contains function pointers
 * to platform specific functions which enable the spindle or regulate the speed and direction of it.
 * There are also limit values for the maximum, minimum and absolute minimum RPM and an additional
 * context pointer which is passed the the abstraction functions. The user must provide a configured
 * structure in its design when calling SPINDLE_CreateInstance.
 */
typedef struct SpindlePhysicalParams
{
	/*!
	 * This function pointer is used to change the direction of the spindle. It is always called
	 * before setDutyCycle and is never called without any other combination of setDutyCycle or 
	 * enaPWM. This means that it can simply be used to set a variable in the user design instead
	 * of handling all required PWM steps to change the direction of the spindle.
	 * 
	 * The pointer must not be null
	 *
     * @param[in,out] h        optional handle of the spindle library.
     * @param[in,out] context  optional context pointer the user has passed by the SPINDLE_CreateInstance call.
     * @param[in]     backward sets the direction of the spindle (clockwise or counter clockwise = backward).
	 *
	 * The behavior is schematically as follows:
	 * @startuml
     * Library -> platform_set_direction_function : Requests a direction change
     * Library <- platform_set_direction_function
     * @enduml
	 */
	void (*setDirection)(SpindleHandle_t h, void* context, int backward);

	/*!
	 * This function pointer is used to change the duty cycle of the spindle. This function is
	 * necessary for changing the PWM generation itself
	 * 
	 * The pointer must not be null
	 * 
     * @param[in,out] h         optional handle of the spindle library.
     * @param[in,out] context   optional context pointer the user has passed by the SPINDLE_CreateInstance call.
     * @param[in]     dutyCycle value between 0 = 0% and 1 = 100%
	 *
	 * The behavior is schematically as follows:
	 * @startuml
     * Library -> platform_set_duty_cycle_function : Requests a duty cycle change
	 * platform_set_duty_cycle_function -> timer_driver : change duty cycle
	 * platform_set_duty_cycle_function <- timer_driver
     * Library <- platform_set_duty_cycle_function
     * @enduml
	 */
	void (*setDutyCycle)(SpindleHandle_t h, void* context, float dutyCycle);

	/*!
	 * This function pointer is used to enable or disable the PWM output and to set
	 * the enable outputs od the half bridges of the spindle
	 * 
	 * The pointer must not be null
	 * 
     * @param[in,out] h         optional handle of the spindle library.
     * @param[in,out] context   optional context pointer the user has passed by the SPINDLE_CreateInstance call.
     * @param[in]     ena       value that sets enable or disable state (1 = enabled, 0 = disabled)
	 *
	 * The behavior is schematically as follows:
	 * @startuml
	 * alt ena = 1
     *   Library -> platform_enable_function : Requests a duty cycle change
	 *   platform_enable_function -> timer_driver : enable PWM
	 *   platform_enable_function <- timer_driver
	 *   platform_enable_function -> gpio_driver : enable H-bridge
	 *   platform_enable_function <- gpio_driver
     *   Library <- platform_enable_function
	 * else ena = 0
     *   Library -> platform_enable_function : Requests a duty cycle change
	 *   platform_enable_function -> timer_driver : disable PWM or set duty cycle to 0
	 *   platform_enable_function <- timer_driver
	 *   platform_enable_function -> gpio_driver : disable H-bridge
	 *   platform_enable_function <- gpio_driver
     *   Library <- platform_enable_function
	 * end
     * @enduml
	 */
	void (*enaPWM)      (SpindleHandle_t h, void* context, int ena);


	/*!
	 * specifies the maximum positive RPM value which represents a duty cycle of 1.0
	 */
	float        maxRPM;

	/*!
	 * specifies the minimum possible absolute RPM value which is usable with the spindle
	 * a requested RPM value lower than this value is limited and therefore its not possible
	 * to let the spindle run slower. This value is used for clock wise and counter clock wise
	 * spindle rotation in the same way.
	 */
	float        absMinRPM;

	/*!
	 * specifies the minimum negative RPM value. Mostly this value is symmetrical to the max value
	 * which means it can be the same value
	 */
	float        minRPM;

	/*!
	 * optional context pointer for platform abstraction. can be null.
	 */
	void*        context;

} SpindlePhysicalParams_t;

/*!
 * The SPINDLE_CreateInstance function is used to create the spindle controller. There is a singleton pattern implemented
 * for the controller so there is only one instance possible for the design. In case it is called multiple times, it returns
 * the instance pointer of the first created spindle controller
 *
 * The return value of SPINDLE_CreateInstance is a null pointer in case an error occured or a pointer of type SpindleHandle_t.
 *
 * param uxStackDepth is the stack depth of the console processor thread in words
 * param xPrio is the spindle controller priority.
 * param cH of type ConsoleHandle_t is the handle pointer of a console processor instance which is required to register the
 * console command for the spindle controller
 * param p of type SpindlePhysicalParams_t* is a pointer to the platform abstraction functions
 * 
 * ATTENTION: This function is not locked or guarded and therefore its never thread safe when calling from two different threads
 * at the same or nearly the same time while no instance has been created before! One pointer and its ressources could be lost
 * which leads to a memory leak and/or to console command registration issues at runtime when using the spindle command!
 */
SpindleHandle_t SPINDLE_CreateInstance( unsigned int uxStackDepth, int xPrio, ConsoleHandle_t cH, SpindlePhysicalParams_t* p );


/*!
 * \mainpage FreeRTOS Spindle Library
 * \section intro_sec Introduction
 *
 * The following code documentation is a set of tipps and diagramms as well as function and structure descriptions
 * which help when using the library. It is used to increase the implementation speed. Some examples are attached
 * in this documentation as well
 *
 * \section static_sec static compile flags of the library
 * There are no additional static compile flags or DEFINES which the user can configure
 * 
 * \section instantiation library instantiation
 * The library is implemented as a singleton pattern. This means that only one instance
 * can be created at runtime. Calling the SPINDLE_CreateInstance multiple times will
 * succeed but always the pointer of the first instance will be returned.
 * 
 * \section state_example Examples
 * The following example shows how to create a instance of the spindle controller library.
 *
 * \code
 * 
 * // set parameters for the physical system
 * SpindlePhysicalParams_t s;
 * s.maxRPM             =  9000.0f;
 * s.minRPM             = -9000.0f;
 * s.absMinRPM          =  1600.0f;
 * s.setDirection       = SPINDLE_SetDirection;
 * s.setDutyCycle       = SPINDLE_SetDutyCycle;
 * s.enaPWM             = SPINDLE_EnaPWM;
 * s.context            = NULL;
 * SPINDLE_CreateInstance( 4*configMINIMAL_STACK_SIZE, configMAX_PRIORITIES - 3, c, &s);
 * 
 * ...
 *
 * \endcode
 *
 * The following example shows the usage of the spindle library via console. With the spindle command
 * the user can start or stop the spindle and it can also change the spindle speed with the start command
 * The command returns OK or FAIL in the given cases of failure or success. There is also a status command.
 * It returns the state of the spindle (turning) and the RPM which has been set. It also terminates with "OK"
 * or "FAIL"
 *
 * \code
 * // starts the spindle
 * $> spindle start <RPM>
 * 
 * // sets another spped of the spindle
 * $> spindle start <RPM>
 * 
 * // stops the spindle
 * $> spindle stop
 * \endcode
 * 
 * \code
 * // get the status the spindle
 * $> spindle status
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
 * -# compile the source code once to make sure everything builds without issues and your include paths are correct.
 * -# now implement the platform functions which are required be the SpindlePhysicalParams_t structure when calling SPINDLE_CreateInstance.
 * -# when all functions are defined, no error should be returned after the call of SPINDLE_CreateInstance. Porting is done.
 * 
 * \section additional note
 * when porting the platform functions, it is not required to execute anything in the SpindlePhysicalParams_t[setDirection] function, because
 * the setDutyCycle function and the enaPWM function is called directly after the setDirection function. So it is sufficient to only store a
 * variable value anywhere and interpret it later in the setDutyCyle function!
 */


#endif /* INC_SPINDLE_CONTROLLER_H_ */
