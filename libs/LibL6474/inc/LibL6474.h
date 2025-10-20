/*
 * LibL6474.h
 *
 *  Created on: Dec 2, 2024
 *      Author: Thorsten
 */

 /*! \file */

#ifndef INC_LIBL6474_H_
#define INC_LIBL6474_H_ INC_LIBL6474_H_

/*!
 * The LibL6474Config.h must be provided by the user project. The header in conf folder is only a template!
 */
// --------------------------------------------------------------------------------------------------------------------
#include "LibL6474Config.h"


/*!
 * The L6474x_State_t enum is used to describe the current state of the driver library which helps
 * the user to decide the next steps to be taken or to get notified about an unexpected state which is caused
 * by an error
 */
// --------------------------------------------------------------------------------------------------------------------
typedef enum L6474x_State
// --------------------------------------------------------------------------------------------------------------------
{
	/*!
	 * stRESET is used to signal the initial power up state or a full device reset state in which the chip is kept
	 * until the user releases a reset state
	 */
	stRESET     = 0x00,

	/*!
	 * stDISABLED is used to signal the ready but disabled state of the chip. It is kept in this step until the
	 * user enables the power output stages
	 */
	stDISABLED  = 0x01,

	/*!
	 * stENABLED is used to signal the ready and enabled state of the chip. It is kept in this step until the
	 * user disables the power output stages or an error happens
	 */
	stENABLED   = 0x02,

	/*!
	 * stINVALID is used to signal a state which requires user interaction because something is completely wrong
	 * in regular cases, this state can not be present. This is an indicator of memory corruption or misleading
	 * usage of the library
	 */
	stINVALID   = 0x04

} L6474x_State_t;


/*!
 * The L6474x_StepMode_t enum is used to describe the current step mode (resolution) of the driver library and the
 * stepper motor. This changes the internal calculations for speed and position and requires a new reference run in
 * case it will be changed at runtime.
 */
// --------------------------------------------------------------------------------------------------------------------
typedef enum L6474x_StepMode
// --------------------------------------------------------------------------------------------------------------------
{
	/*!
	 * smFULL requires minimum of 4 steps for a full turn of the motor axis. This mode results in the highest torque but in
	 * the most noisy operation and the lowest resolution
	 */
	smFULL     = 0x00,

	/*!
	 * smHALF requires minimum of 8 steps for a full turn of the motor axis. better resolution but less torque
	 */
	smHALF     = 0x01,

	/*!
	 * smMICRO4 requires minimum of 16 steps for a full turn of the motor axis. better resolution but less torque
	 */
	smMICRO4   = 0x02,

	/*!
	 * smMICRO8 requires minimum of 32 steps for a full turn of the motor axis. better resolution but less torque
	 */
	smMICRO8   = 0x03,

	/*!
	 * smMICRO16 requires minimum of 64 steps for a full turn of the motor axis. better resolution but less torque
	 */
	smMICRO16  = 0x04
} L6474x_StepMode_t;

/*!
 * The L6474x_Direction_t enum describes the direction of the driver and therefore the rotation of the stepper axis.
 */
// --------------------------------------------------------------------------------------------------------------------
typedef enum L6474x_Direction
// --------------------------------------------------------------------------------------------------------------------
{
	/*!
	 * dirFOREWARD is interpreted as clock wise rotation
	 */
	dirFOREWARD     = 0x00,

	/*!
	 * dirFOREWARD is interpreted as counter clock wise rotation
	 */
	dirBACKWARD     = 0x01
} L6474x_Direction_t;

/*!
 * The L6474x_ErrorCode_t enum describes the possible error codes when using the API commands. Those error codes are
 * mostly returned as return value by the API function which has been called previously
 */
// --------------------------------------------------------------------------------------------------------------------
typedef enum L6474x_ErrorCode
// --------------------------------------------------------------------------------------------------------------------
{
	/*!
	 * errcNONE means no error and the operation was successful
	 */
	errcNONE         =  0,

	/*!
	 * errcINV_ARG means that at least one argument was invalid or out of range
	 */
	errcINV_ARG      = -1,

	/*!
	 * errcNULL_ARG means that at least one argument was a null pointer where it was not expected
	 */
	errcNULL_ARG     = -2,

	/*!
	 * errcINV_STATE means that the stepper driver or the library is not in the state of type L6474x_State_t and
	 * therefore the operation can not be executed
	 */
	errcINV_STATE    = -3,

	/*!
	 * errcINTERNAL means an error which happens when calling abstraction functions which then return an error code
	 */
	errcINTERNAL     = -4,

	/*!
	 * errcLOCKING in case the library is used multi-threaded, it might happen, that a function can not be locked
	 * because another caller is still using the library (lock guard principle). In mutex case this should not
	 * be returned by the API
	 */
	errcLOCKING      = -5,

	/*!
	 * errcDEVICE_STATE means that the state of the driver chip permits the operation
	 */
	errcDEVICE_STATE = -6,

	/*!
	 * errcPENDING means that another operation is still pending and therefore the operation can not be executed
	 * while there is still a pending one
	 */
	errcPENDING      = -7,

	/*!
	 * errcFORBIDDEN means that operation is not possible with the given arguments or this library
	 */
	errcFORBIDDEN    = -8

} L6474x_ErrorCode_t;


/*!
 * The L6474_Status_t enum describes the status register content of the stepper driver chip. It is reinterpreted
 * so it can be naturally used in the code to get some of the error codes or state bits of the driver.
 */
// --------------------------------------------------------------------------------------------------------------------
typedef struct L6474_Status
// --------------------------------------------------------------------------------------------------------------------
{
	/*!
	 * high impedance state active
	 */
	unsigned char HIGHZ      ;

	/*!
	 * direction is clock wise or counter clock wise
	 */
	unsigned char DIR        ;

	/*!
	 * a command has not been performed
	 */
	unsigned char NOTPERF_CMD;

	/*!
	 * a command was invalid
	 */
	unsigned char WRONG_CMD  ;

	/*!
	 * under-voltage lock out
	 */
	unsigned char UVLO       ;

	/*!
	 * threshold warning
	 */
	unsigned char TH_WARN    ;

	/*!
	 * thermal shutdown
	 */
	unsigned char TH_SD      ;

	/*!
	 * over current detection (short circuit)
	 */
	unsigned char OCD        ;

	/*!
	 * movement is ongoing (pending)
	 */
	unsigned char ONGOING    ;
} L6474_Status_t;


/*!
 * The L6474x_OCD_TH_t enum describes the possible overcurrent threshold values which can be set in the library
 * and which are then taken into account by the stepper driver
 */
// --------------------------------------------------------------------------------------------------------------------
typedef enum L6474x_OCD_TH
// --------------------------------------------------------------------------------------------------------------------
{
	ocdth375mA  = 0x00,
	ocdth750mA  = 0x01,
	ocdth1125mA = 0x02,
	ocdth1500mA = 0x03,
	ocdth1875mA = 0x04,
	ocdth2250mA = 0x05,
	ocdth2625mA = 0x06,
	ocdth3000mA = 0x07,
	ocdth3375mA = 0x08,
	ocdth3750mA = 0x09,
	ocdth4125mA = 0x0A,
	ocdth4500mA = 0x0B,
	ocdth4875mA = 0x0C,
	ocdth5250mA = 0x0D,
	ocdth5625mA = 0x0E,
	ocdth6000mA = 0x0F
} L6474x_OCD_TH_t;

/*!
 * The L6474_Handle_t handle is an instance pointer of the driver library which is generated whenever
 * the L6474_CreateInstance function returns with success. Therefore at least a L6474x_Platform_t 
 * platform description pointer is required
 */
// --------------------------------------------------------------------------------------------------------------------
typedef struct L6474_Handle* L6474_Handle_t;


/*!
 * The L6474x_Platform_t structure is used to encapsulate platform specific parameters and to provide environment
 * specific functions in a generalized format. Theses functions are malloc and free or some PWM and GPIO abstractions
 * The lock guards and thread safety mechanisms are also abstracted by this structure
 */
// --------------------------------------------------------------------------------------------------------------------
typedef struct L6474x_Platform
// --------------------------------------------------------------------------------------------------------------------
{
	/*!
	 * classic malloc function pointer which returns null in case of an error or a memory void* pointer with the 
	 * given size by the first argument. The user must provide this function as part of the platform abstraction.
	 * The minimal functionality can be reached by providing a function which simply calls malloc.
	 *
     * @param[in] size number of bytes requested by the memory allocation request
	 *
	 * The behavior is schematically as follows:
	 * @startuml
     * Library -> platform_malloc_function : Requests dynamic memory
	 * platform_malloc_function -> stdlib  : call malloc
	 * platform_malloc_function <- stdlib
     * Library <- platform_malloc_function : returns memory pointer
     * @enduml
	 */
	void* (*malloc)    ( unsigned int size                                                                            );

	/*!
	 * classic free function pointer which releases memory previously allocated by malloc. 
	 * The user must provide this function as part of the platform abstraction.
	 * The minimal functionality can be reached by providing a function which simply calls free in case
	 * The malloc abstraction function is a call to malloc.
	 * 
     * @param[in] pMem pointer of the memory space which shall be freed
	 *
	 * The behavior is schematically as follows:
	 * @startuml
     * Library -> platform_free_function : Requests release of allocated memory
	 * platform_free_function -> stdlib : call free
	 * platform_free_function <- stdlib
     * Library <- platform_free_function
     * @enduml
	 */
	void  (*free)      ( const void* const pMem                                                                       );

	/*!
	 * the transfer function is used to provide bus access to the stepper driver chip
	 * 
     * @param[in,out] pIO    optional user context pointer which has been passed by the L6474_CreateInstance call
     * @param[out]    pRX    pointer to the receive data buffer
     * @param[in]     pTX    pointer to the transmit data buffer
     * @param[in]     length number of bytes for RX and TX
	 *
	 * The behavior is schematically as follows:
	 * @startuml
     * Library -> platform_transfer_function : Requests read and write simultaneously
     * loop num of bytes
     *   platform_transfer_function -> gpio_driver : CS low
	 *   platform_transfer_function <- gpio_driver
	 *   platform_transfer_function -> spi_driver : 1 byte rx + tx
	 *   platform_transfer_function <- spi_driver
	 *   platform_transfer_function -> gpio_driver : CS high
	 *   platform_transfer_function <- gpio_driver
	 * end
     * Library <- platform_transfer_function
     * @enduml
	 */
	int   (*transfer)  ( void* pIO, char* pRX, const char* pTX, unsigned int length                                   );

	/*!
	 * the reset function is used to provide gpio access to the reset of the stepper driver chip. keep in mind
	 * that the chip has a reset not pin and so the ena signal must be inverted to set the correct reset level
	 * physically
	 * 
     * @param[in,out] pGPO   optional user context pointer which has been passed by the L6474_CreateInstance call
     * @param[in]     ena    output state to set without level inversion
	 *
	 * The behavior is schematically as follows:
	 * @startuml
     * Library -> platform_reset_function : Requests reset of the chip
	 * platform_reset_function -> gpio_driver : set pin to !ena
	 * platform_reset_function <- gpio_driver
     * Library <- platform_reset_function
     * @enduml
	 */
	void  (*reset)     ( void* pGPO, const int ena                                                                    );

	/*!
	 * the sleep function implements a wait time with the argument in milliseconds
	 * 
     * @param[in]     ms    time to sleep in milliseconds
	 *
	 * The behavior is schematically as follows:
	 * @startuml
     * Library -> platform_sleep_function : Requests sleep by the library
	 * platform_sleep_function -> scheduler : sleep(ms)
	 * platform_sleep_function <- scheduler
     * Library <- platform_sleep_function
     * @enduml
	 */
	void  (*sleep)     ( unsigned int ms                                                                              );

#if defined(LIBL6474_STEP_ASYNC) && ( LIBL6474_STEP_ASYNC == 0 )
	/*!
	 * the step function is a synchronous blocking function until the movement has been fully executed.
	 * This can be e.g. a PWM generation via GPIO pulsing given by the numPulses argument
	 * 
     * @param[in,out] pPWM      optional user context pointer which has been passed by the L6474_CreateInstance call
     * @param[in]     dir       output signal to drive the stepper clock wise or counter clockwise
     * @param[in]     numPulses number of high and low transitions to generate a pulse for the stepper chip
	 *
	 * The behavior is schematically as follows:
	 * @startuml
     * Library -> platform_step_function : Requests generating pulses async
	 *   loop 'numPulses' times
     *     platform_step_function -> gpio_driver : Set high
     *     platform_step_function <- gpio_driver
     *     platform_step_function -> scheduler : sleep(ms)
     *     platform_step_function <- scheduler
     *     platform_step_function -> gpio_driver : Set low
     *     platform_step_function <- gpio_driver
     *     platform_step_function -> scheduler : sleep(ms)
     *     platform_step_function <- scheduler
     *   end
     * Library <- platform_step_function
     * @enduml
	 */
	int   (*step)      ( void* pPWM, int dir, unsigned int numPulses                                                  );
#else
	/*!
	 * the stepAsync function is an asynchronous non-blocking function which e.g. enables a timer which then generates
	 * the required amount of pulses given by the numPulses argument. The doneCLB is a callback provided by the library
	 * which is called when the asynchronous operation has been done
	 * 
     * @param[in,out] pPWM      optional user context pointer which has been passed by the L6474_CreateInstance call
     * @param[in]     dir       output signal to drive the stepper clock wise or counter clockwise
     * @param[in]     numPulses number of high and low transitions to generate a pulse for the stepper chip
     * @param[in]     doneClb   callback function pointer, which is required to be called after the async process has been finished
     * @param[in]     h         handle pointer which is at least required by the callback of doneClb argument
	 *
	 * The behavior is schematically as follows:
	 * @startuml
     * Library ->> platform_step_function : Requests generating pulses async
	 *   loop 'numPulses' times
     *     platform_step_function -> gpio_driver : Set high
     *     platform_step_function <- gpio_driver
     *     platform_step_function -> scheduler : sleep(ms)
     *     platform_step_function <- scheduler
     *     platform_step_function -> gpio_driver : Set low
     *     platform_step_function <- gpio_driver
     *     platform_step_function -> scheduler : sleep(ms)
     *     platform_step_function <- scheduler
     *   end
     * @enduml
	 */
	int   (*stepAsync) ( void* pPWM, int dir, unsigned int numPulses, void (*doneClb)(L6474_Handle_t), L6474_Handle_t h );

	/*!
	 * the cancelStep function is a synchronous blocking function which cancels the asynchronous previously started step
	 * operation in case it has not been finished
	 *
     * @param[in,out] pPWM      optional user context pointer which has been passed by the L6474_CreateInstance call
	 */
	int   (*cancelStep)( void* pPWM                                                                                   );
#endif

#if defined(LIBL6474_HAS_LOCKING) && LIBL6474_HAS_LOCKING == 1
	/*!
	 * in case the library provides locking functions, this function locks the API for thread safety
	 */
	int   (*lock)      ( void                                                                                         );

	/*!
	 * in case the library provides locking functions, this function unlocks the API to allow other callers to execute
	 * an API command
	 */
	void  (*unlock)    ( void                                                                                         );
#endif

#if defined(LIBL6474_HAS_FLAG) && LIBL6474_HAS_FLAG == 1
	/*!
	 * in case the FLAG pin of the stepper driver chip is used, this function returns the current value of the FLAG pin.
	 * This is an optional abstraction function and is not required for regular operation because the library can
	 * read the device state by the status register as well.
	 *
     * @param[in,out] pIO      optional user context pointer which has been passed by the L6474_CreateInstance call
	 */
	int   (*getFlag)   ( void* pIO                                                                                    );
#endif

} L6474x_Platform_t;

/*!
 * The L6474_Property_t enum is a address representation for externally changeable properties by the library via the
 * API commands. In case the torque shall be changed, the user can call L6474_SetProperty with the L6474_PROP_TORQUE
 * entry. Some of the properties can not be changed while the driver outputs are enabled and the library is in stENABLED
 * state
 */
// --------------------------------------------------------------------------------------------------------------------
typedef enum
// --------------------------------------------------------------------------------------------------------------------
{
	/*!
	 * L6474_PROP_TORQUE is the current setpoint value of each phase of the stepper which follows the torque directly
	 */
	L6474_PROP_TORQUE  = 0x09,

	/*!
	 * L6474_PROP_TON is the minimum time for the ON-part of the current control loop
	 */
	L6474_PROP_TON     = 0x0F,

	/*!
	 * L6474_PROP_TOFF is the minimum time for the OFF-part of the current control loop
	 */
	L6474_PROP_TOFF    = 0x10,

	/*!
	 * L6474_PROP_ADC_OUT controls the ADC output behavior
	 */
	L6474_PROP_ADC_OUT = 0x12,

	/*!
	 * L6474_PROP_OCDTH sets the overcurrent threshold detection value
	 */
	L6474_PROP_OCDTH   = 0x13,

	/*!
	 * L6474_PROP_TFAST is used to change switching times
	 */
	L6474_PROP_TFAST   = 0x0E,
} L6474_Property_t;

/*!
 * The L6474_BaseParameter_t structure is used to set default initialization values for the library whenever a reset
 * has been called or the library is used the first time. It can be filled with the global defaults by calling
 * L6474_SetBaseParameter and it then can be adapted or directly passed into L6474_Initialize which always needs
 * to be called after resetting the stepper driver or the library
 */
// --------------------------------------------------------------------------------------------------------------------
typedef struct
// --------------------------------------------------------------------------------------------------------------------
{
	/*!
	 * stepMode describes the stepping operation mode, see L6474x_StepMode_t
	 */
	L6474x_StepMode_t stepMode;

	/*!
	 * OcdTh describes the overcurrent detection threshold, see L6474x_OCD_TH_t
	 */
	L6474x_OCD_TH_t   OcdTh;

	/*!
	 * TimeOnMin is used to change the minimum time for the ON-part of the current control loop
	 */
	char              TimeOnMin;

	/*!
	 * TimeOffMin is used to change the minimum time for the OFF-part of the current control loop
	 */
	char              TimeOffMin;

	/*!
	 * TorqueVal is used to change the current setpoint of the controller which follows directly the torque
	 */
	char              TorqueVal;

	/*!
	 * L6474_PROP_TFAST is used to change switching times
	 */
	char              TFast;

} L6474_BaseParameter_t;


/*!
 * L6474_CreateInstance is used once to create a library instance which encapsulates the handling for one stepper driver chip
 * It needs a L6474x_Platform_t abstraction pointer and optionally some context pointers which are platform specific. The
 * context pointers are passed to the abstraction functions which are provided by the platform structure.
 * 
 * In case it fails, a null pointer is returned. In case it was successful, a handle pointer is returned which is always
 * required for all other API calls
 */
L6474_Handle_t L6474_CreateInstance(L6474x_Platform_t* p, void* pIO, void* pGPO, void* pPWM);

/*!
 * L6474_DestroyInstance is used to destroy a library instance which has been previously created by a call to L6474_CreateInstance
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error
 */
int L6474_DestroyInstance(L6474_Handle_t h);


/*!
 * Calling L6474_ResetStandBy sets the driver in deep power down or reset state and the library will be set to default
 * startup state again. All initialization steps must be done again by at least calling L6474_Initialize. The library
 * is set to stRESET from any other state before.
 * 
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum 
 * in case of an error
 * 
 * note that in case the stepper is enabled an moving, the operation will be safely disabled but depending on the
 * speed, the position can not be tracked properly anymore. So a new reference run after re-initialization is required!
 * 
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 */
int L6474_ResetStandBy(L6474_Handle_t h);

/*!
 * Calling L6474_SetBaseParameter sets the basic global default parameters for all properties in the L6474_BaseParameter_t
 * structure. These parameters are required when calling L6474_Initialize
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error
 */
int L6474_SetBaseParameter(L6474_BaseParameter_t* p);

/*!
 * Calling L6474_EncodePhaseCurrent converts the requested phase current into the register value which is
 * required in the base parameter structure. The member for this value is named TorqueVal
 *
 * The function returns a register value which represents the given current value
 */
char L6474_EncodePhaseCurrent(float mA);

/*!
 * Calling L6474_EncodePhaseCurrentParameter converts the requested phase current into the register value which is
 * required in the base parameter structure. The member for this value is named TorqueVal. This function directly inserts
 * the converted value into the given base parameter structure.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error
 */
int L6474_EncodePhaseCurrentParameter(L6474_BaseParameter_t* p, float mA);

/*!
 * func L6474_Initialize is used to set all basic parameters and take the required steps to bring up the driver into an
 * idle state. It sets the library to stDISABLED in case no error happens.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum 
 * in case of an error
 * 
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 * 
 * param p is required and can not be null. the struct can be filled by calling L6474_SetBaseParameter before.
 */
int L6474_Initialize(L6474_Handle_t h, L6474_BaseParameter_t* p);

/*!
 * func L6474_SetStepMode is used to change the step mode and therefore the resolution. It is required to execute
 * a new reference run because the position value of the lib does not match the stepping mode anymore. The library must not be
 * in stRESET to perform this operation.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 *
 * param mode is required and is one out of L6474x_StepMode_t
 */
int L6474_SetStepMode(L6474_Handle_t h, L6474x_StepMode_t mode);

/*!
 * func L6474_SetStepMode is used to read back the step mode and therefore the resolution. The library must not be
 * in stRESET to perform this operation.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 *
 * param mode is required and is one out of L6474x_StepMode_t
 */
int L6474_GetStepMode(L6474_Handle_t h, L6474x_StepMode_t* mode);

/*!
 * func L6474_SetPowerOutputs is used to enable or disable the driver output stages. The library must not be
 * in stRESET to perform this operation. The libraries state will be changed to stENABLED or stDISABLED depending
 * on the ena parameter.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 *
 * param ena is required and is either 0 when disabling or 1 when enabling is requested
 */
int L6474_SetPowerOutputs(L6474_Handle_t h, int ena);

/*!
 * func L6474_GetStatus is used to read back the current libraries and devices status. The library must not be
 * in stRESET to perform this operation.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 *
 * param status is required. And must not be null
 */
int L6474_GetStatus(L6474_Handle_t h, L6474_Status_t* status);


/*!
 * func L6474_GetState is used to read back the current libraries state. The library can be
 * in state to perform this operation.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 *
 * param state is required. And must not be null
 */
int L6474_GetState(L6474_Handle_t h, L6474x_State_t* state);

/*!
 * func L6474_StepIncremental is used to issue a movement with the given amount of steps. The library has to be
 * in stENABLED state to perform this operation.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 *
 * param steps is required. negative values lead to a counter clock wise movement, 0 does not lead to any movement
 * but returns errcNONE in case no other issue is present.
 */
int L6474_StepIncremental(L6474_Handle_t h, int steps );

/*!
 * func L6474_StopMovement is used to stop a pending movement in case it has been configured as async. If not,
 * it always return errcNONE in case no other issue is present. The library has to be in stENABLED state 
 * to perform this operation.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 */
int L6474_StopMovement(L6474_Handle_t h);

/*!
 * func L6474_IsMoving is used to read the state of movement. In case the stepper moves, it returns true. The library 
 * must not be in stRESET state to perform this operation.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 *
 * param moving is required. It returns 0 when no movement is pending or 1 when there is a movement.
 */
int L6474_IsMoving(L6474_Handle_t h, int* moving);

/*!
 * func L6474_SetProperty is used to write a property of the type of L6474_Property_t. The library
 * must not be in stRESET state to perform this operation.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 * 
 * param prop is required.
 *
 * param value is required. The value formats can be found in the datasheet
 */
int L6474_SetProperty(L6474_Handle_t h, L6474_Property_t prop, int value);

/*!
 * func L6474_SetProperty is used to read a property of the type of L6474_Property_t. The library
 * must not be in stRESET state to perform this operation.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 *
 * param prop is required.
 *
 * param value is required. The value formats can be found in the datasheet
 */
int L6474_GetProperty(L6474_Handle_t h, L6474_Property_t prop, int* value);

/*!
 * func L6474_GetAbsolutePosition is used to read the current stepper position. The library
 * must not be in stRESET state to perform this operation.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 *
 * param position is required. It returns the position 
 */
int L6474_GetAbsolutePosition(L6474_Handle_t h, int* position);

/*!
 * func L6474_SetAbsolutePosition is used to write the current stepper position. The library
 * must not be in stRESET state to perform this operation.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 *
 * param position is required. It sets the position
 */
int L6474_SetAbsolutePosition(L6474_Handle_t h, int position);

/*!
 * func L6474_GetElectricalPosition is used to read the current stepper electrical position. The library
 * must not be in stRESET state to perform this operation. The electrical Position returns
 * the step state and micro step component of the driver.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 *
 * param position is required.
 */
int L6474_GetElectricalPosition(L6474_Handle_t h, int* position);

/*!
 * func L6474_SetElectricalPosition is used to write the current stepper electrical position. The library
 * must not be in stRESET state to perform this operation. The electrical Position consists of
 * the step state and micro step component of the driver.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 *
 * param position is required.
 */
int L6474_SetElectricalPosition(L6474_Handle_t h, int position);

/*!
 * func L6474_GetPositionMark is used to read the current stepper position mark. The library
 * must not be in stRESET state to perform this operation.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 *
 * param position is required.
 */
int L6474_GetPositionMark(L6474_Handle_t h, int* position);

/*!
 * func L6474_SetPositionMark is used to write the current stepper position mark. The library
 * must not be in stRESET state to perform this operation.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 *
 * param position is required.
 */
int L6474_SetPositionMark(L6474_Handle_t h, int position);

/*!
 * func L6474_GetAlarmEnables is used to read the current enable bits for the ERROR ALARM and FLAG Pin. The library
 * must not be in stRESET state to perform this operation.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 *
 * param bits is required. The bitmask can be found in the datasheet
 */
int L6474_GetAlarmEnables(L6474_Handle_t h, int* bits);

/*!
 * func L6474_SetAlarmEnables is used to write the current enable bits for the ERROR ALARM and FLAG Pin. The library
 * must not be in stRESET state to perform this operation.
 *
 * The function returns errcNONE in case no error happens or any other error code from L6474x_ErrorCode_t enum
 * in case of an error.
 *
 * param h is required and can not be null. the handle can be created by calling L6474_CreateInstance before.
 *
 * param bits is required. The bitmask can be found in the datasheet
 */
int L6474_SetAlarmEnables(L6474_Handle_t h, int bits);


/*! 
 * \mainpage Stepper Library Lib6474
 * \section intro_sec Introduction
 *
 * The following code documentation is a set of tips and diagrams as well as function and structure descriptions
 * which help when using the library. It is used to increase the implementation speed. Some examples are attached
 * in this documentation as well
 *
 * \section install_sec Static compile flags
 *
 * LIBL6474_STEP_ASYNC:
 * This DEFINE is used to switch from blocking synchronous mode to asynchronous non-blocking step mode. 
 * This changes the API behavior
 * 
 * LIBL6474_HAS_LOCKING:
 * This DEFINE is used to enable the lock guard and thread synchronization guard abstraction,
 * which requires additional abstraction functions
 *
 * LIBL6474_DISABLE_OCD:
 * Should only be used for debugging purposes and not for productive code!
 * This DEFINE is used to disable the overcurrent detection feature in the library and the stepper driver
 * 
 * LIBL6474_HAS_FLAG:
 * enables the support of the flag pin
 * This DEFINE is used to enable the FLAG pin support, which requires additional abstraction functions
 *
 * \section state_sec State diagram 
 * The following state diagram shows the internal state machine handling which follows or represents the
 * state of the stepper driver chip. The fault conditions are not depicted in the diagram but in all regular
 * cases the error handling is done by resetting and reinitialization of the stepper driver by the library.
 * 
 * \dot
 * digraph G {
 * 
 *   subgraph cluster_0 {
 * 	node [style=filled];
 * 	stRESET;
 * 	stDISABLED;
 * 	stENABLED;
 * 	label = "State machine";
 * 	color=blue
 *   }
 * 
 *   POR -> stRESET [label="L6474_CreateInstance"];
 *   stRESET -> stDISABLED [label="L6474_Initialize"];
 * 
 *   stDISABLED -> stENABLED [label="L6474_SetPowerOutputs"];
 *   stDISABLED -> stRESET [label="L6474_ResetStandBy"];
 * 
 *   stENABLED -> stDISABLED [label="L6474_SetPowerOutputs"];
 *   stENABLED -> stRESET [label="L6474_ResetStandBy"];
 * 
 *   POR [shape=Mdiamond];
 * }
 * \enddot
 * 
 * \section state_example Examples
 * The following example shows a part of the abstraction functions which are required
 * for the usage of the Stepper library. It also shows how to create a instance of
 * the stepper library.
 * \code
 * static void* StepLibraryMalloc( unsigned int size )
 * {
 * 	 return malloc(size);
 * }
 * 
 * static void StepLibraryFree( const void* const ptr )
 * {
 * 	 free((void*)ptr);
 * }
 * 
 * static int StepDriverSpiTransfer( void* pIO, char* pRX, const char* pTX, unsigned int length )
 * {
 *   // byte based access, so keep in mind that only single byte transfers are performed!
 *   for ( unsigned int i = 0; i < length; i++ )
 *   {
 *     ...
 *   }
 *   return 0;
 * }
 * 
 * ...
 * 
 * // pass all function pointers required by the stepper library
 * // to a separate platform abstraction structure
 * L6474x_Platform_t p;
 * p.malloc     = StepLibraryMalloc;
 * p.free       = StepLibraryFree;
 * p.transfer   = StepDriverSpiTransfer;
 * p.reset      = StepDriverReset;
 * p.sleep      = StepLibraryDelay;
 * p.stepAsync  = StepTimerAsync;
 * p.cancelStep = StepTimerCancelAsync;
 * 
 * // now create the handle
 * L6474_Handle_t h = L6474_CreateInstance(&p, null, null, null); 
 *
 * \endcode
 * 
 * The following example shows a really simple instantiation and usage
 * of the library with a simple default and straight forward configuration
 * and no calculation of any step widths or resolutions
 * 
 * \code
 * 
 * ...
 * 
 * int result = 0;
 * ...
 * 
 * // reset all and take all initialization steps
 * result |= L6474_ResetStandBy(h);
 * result |= L6474_Initialize(h, &param);
 * result |= L6474_SetPowerOutputs(h, 1);
 * 
 * ...
 * 
 * // in case we have no error, we can enable the drivers
 * // and then we step a bit
 * if ( result == 0 )
 * {
 *     result |= L6474_StepIncremental(h, 1000 );
 * }
 * else
 * {
 *	   // error handling
 *     ...
 * }
 * 
 * ...
 * 
 * \endcode
 *
 * The following example shows how to change the phase current (max torque) of the stepper.
 * This can be achieved by reinitializing the library or by changing the parameter by calling
 * L6474_SetProperty;
 *
 * \code
 *
 * ...
 *
 * int result = 0;
 * ...
 *
 * // first way: set the torque on initialization
 * result |= L6474_ResetStandBy(h);
 * result |= L6474_EncodePhaseCurrentParameter(&param, 1200.0f);
 * result |= L6474_Initialize(h, &param);
 *
 * ...
 *
 * // second way: set the torque after initialization
 * char torqueval = L6474_EncodePhaseCurrent(1500.0f);
 * result |= L6474_SetProperty(h, L6474_PROP_TORQUE, torqueval);
 *
 * ...
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
 * -# copy the template header LibL6474Config.h file to your project or create your own and adapt the compile time parameters as needed.
 * -# compile the source code once to make sure everything builds without issues and your include paths are correct.
 * -# now implement the platform functions which are required be the L6474x_Platform_t structure when calling L6474_CreateInstance.
 * -# when all functions are defined, no error should be returned after the call of L6474_CreateInstance. Porting is done.
 */
 
 #endif /* INC_LIBL6474_H_ */
