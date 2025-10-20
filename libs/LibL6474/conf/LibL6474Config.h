/*
 * LibL6474Config.h
 *
 *  Created on: Dec 2, 2024
 *      Author: Thorsten
 */

 /*! \file */

/*!
 * ATTENTION, this header is only a template, the user must provide its own implementation in the user project 
 */

#ifndef INC_LIBL6474_CONFIG_H_
#define INC_LIBL6474_CONFIG_H_ INC_LIBL6474_CONFIG_H_

/*!
 * This DEFINE is used to switch from blocking synchronous mode to asynchronous non-blocking step mode. 
 * This changes the API behavior
 */
#define LIBL6474_STEP_ASYNC  1

/*!
 * This DEFINE is used to enable the lock guard and thread synchronization guard abstraction,
 * which requires additional abstraction functions
 */
#define LIBL6474_HAS_LOCKING 0

/*!
 * This DEFINE is used to disable the overcurrent detection feature in the library and the stepper driver
 */
#define LIBL6474_DISABLE_OCD 0

/*!
 * This DEFINE is used to enable the FLAG pin support, which requires additional abstraction functions
 */
#define LIBL6474_HAS_FLAG    0

#endif  /* INC_LIBL6474_CONFIG_H_ */
