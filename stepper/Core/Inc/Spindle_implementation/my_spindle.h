/*
 * spindle.h
 *
 *  Created on: Oct 27, 2025
 *      Author: Basti
 */

#ifndef MY_SPINDLE_H
#define MY_SPINDLE_H


#include "Spindle.h"
//Initial Configurations
void init_Spindle(void);
//notwendige Harware Funktionen, die an die Lib Spindle uebergeben werden muessen
void SPINDLE_SetDirection(SpindleHandle_t h, void* context, int backward);
void SPINDLE_SetDutyCycle(SpindleHandle_t h, void* context, float dutyCycle);
void SPINDLE_EnaPWM(SpindleHandle_t h, void* context, int ena);

void Initialize_Spindle(ConsoleHandle_t c);

#endif
