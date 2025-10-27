/*
 * spindle.h
 *
 *  Created on: Oct 27, 2025
 *      Author: Administrator
 */

#ifndef SRC_OUR_SPINDLE_H_
#define SRC_OUR_SPINDLE_H_#

#include "Spindle.h"
//Notwenidge Harware Dunktionen, die an die Lib Spindle übergeben werden müssen
void SPINDLE_SetDirection(SpindleHandle_t h, void* context, int backward);
void SPINDLE_SetDutyCycle(SpindleHandle_t h, void* context, float dutyCycle);
void SPINDLE_EnaPWM(SpindleHandle_t h, void* context, int ena);

#endif /* SRC_OUR_SPINDLE_H_ */
