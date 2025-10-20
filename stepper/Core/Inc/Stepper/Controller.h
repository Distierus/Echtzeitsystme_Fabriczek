/*
 * Controller.h
 *
 *  Created on: Dec 9, 2024
 *      Author: Thorsten
 */

#ifndef INC_STEPPER_CONTROLLER_H_
#define INC_STEPPER_CONTROLLER_H_

#include "Console.h"

typedef struct StepCtrlPhysicalParams
{
	unsigned int stepsPerTurn;
	unsigned int pulsesPerSecondMax;
	float        mmPerTurn;
	float        positionMin;
	float        positionMax;
	float        positionRef;
	float        timerFrequency;
} StepCtrlPhysicalParams_t;

typedef struct StepCtrlHandle* StepCtrlHandle_t;

StepCtrlHandle_t STEPCTRL_CreateInstance( unsigned int uxStackDepth, int xPrio, ConsoleHandle_t cH, StepCtrlPhysicalParams_t* p );

#endif /* INC_STEPPER_CONTROLLER_H_ */
