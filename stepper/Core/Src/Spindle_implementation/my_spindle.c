/*
 * spindle.c
 *
 *  Created on: Oct 27, 2025
 *      Author: Administrator
 */
#include "main.h"
#include "my_spindle.h" // own Spindle-Header file
#include "Spindle.h" // from LibSpindle
#include "Console.h" // fuer ConsoleHandle_t
#include <stdint.h> // fuer einheitliche Datentypen
#include <stdbool.h> // fuer boolean type
#include <newlib.h> // fuer printf Output

extern int configMINIMAL_STACK_SIZE;
extern int configMAX_PRIORITIES;
extern bool error_variable;
extern TIM_HandleTypeDef htim2; //

//Hardwarespezifische Funktionen
void init_Spindle(void){
	//initialise all neccessary Harware Components to use Spindle
	//TODO
}

void SPINDLE_SetDirection(SpindleHandle_t h, void* context, int backward){
	//Hardware Funktion um Rotationsrichtung zu bestimmen
	if(backward <= 0){
		//Roatation links herum
		//TODO
	}else{
		//Rotation rechts herum
		//TODO
	}
}

void SPINDLE_SetDutyCycle(SpindleHandle_t h, void* context, float dutyCycle){
	//DUTY Cycle bestimmt Drehgeschwindigkeit vermutlich mit maxRPM* %Duty Cycle
	//TODO
}

void SPINDLE_EnaPWM(SpindleHandle_t h, void* context, int ena){
	//Switch PWM on or off -> Switch Spindle on or off

	// check if enabling PWM-Signal is successful
	uint8_t error_occurred1 = 0;
	uint8_t error_occurred2 = 0;

	// die GPIO-Pins benutzen den Timer 2, Channel 3 und 4 -> siehe main.c: htim2 für TIM_HandleTypeDef*
	// die HAL-Makros für die Channel in Drivers->...HAL_Driver->Inc->...hal_tim.h gefunden

	//ena = value that sets enable or disable state (=1 enabled, =0 disabled)
	if (ena == 1)
	{
		__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 0);
		__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, 0);
		error_occurred1 = (int) HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
		error_occurred2 = (int) HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
		// HAL_TIM_PWM_Start returns enum ("HAL_StatusTypeDef") with value 0 (=no error), 1, 2 or 3 (=error)
		// -> stm32f7xx_hal_def.h lign 38
		if (error_occurred1 != 0 || error_occurred2 != 0)
		{
			error_variable = true;
			// printf("error occured in SPINDLE_EnaPWM function"\n);
			return;
		}

		// C-Makros fuer die Pins -> siehe main.h
		HAL_GPIO_WritePin(SPINDLE_ENA_L_GPIO_Port, SPINDLE_ENA_L_Pin, ena);
		HAL_GPIO_WritePin(SPINDLE_ENA_R_GPIO_Port, SPINDLE_ENA_R_Pin, ena);
	}

	if (ena == 0)
	{
		HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);
		HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_4);

		// muss noch der Pin wieder deaktiviert werden? Bzw. wie wird die H-Brücke disabled?
	}
}

void Initialize_Spindle(ConsoleHandle_t c){
	//Struct fuer spindel erstellen
	SpindlePhysicalParams_t s;
	//RPM-Werte zuweisen
	s.maxRPM			=  9000.0f;
	s.minRPM			= -9000.0f;
	s.absMinRPM			=  1600.0f;
	//function pointer an struct member uebergeben
	s.setDirection		= SPINDLE_SetDirection;
	s.setDutyCycle		= SPINDLE_SetDutyCycle;
	s.enaPWM			= SPINDLE_EnaPWM;
	s.context			= NULL;
	SpindleHandle_t spindle_Handle = SPINDLE_CreateInstance( 4*configMINIMAL_STACK_SIZE, configMAX_PRIORITIES - 3, c, &s);
}



