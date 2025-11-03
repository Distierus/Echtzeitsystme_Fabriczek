/*
 * spindle.c
 *
 *  Created on: Oct 27, 2025
 *      Author: Administrator
 */
#include "my_spindle.h" //
#include "Spindle.h"
#include "Console.h" // fuer ConsoleHandle_t

extern int configMINIMAL_STACK_SIZE;
extern int configMAX_PRIORITIES;
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
	//TODO
}
void Initialize_Spindle(ConsoleHandle_t c){
		//Struct für Spindel erstellen
	SpindlePhysicalParams_t s;
	//RPM-Werte zuweisen
	s.maxRPM             =  9000.0f;
	s.minRPM             = -9000.0f;
	s.absMinRPM          =  1600.0f;
	//Funktions Handels an LIB übergeben
	s.setDirection       = SPINDLE_SetDirection;
	s.setDutyCycle       = SPINDLE_SetDutyCycle;
	s.enaPWM             = SPINDLE_EnaPWM;
	SpindleHandle_t spindle_Handle = SPINDLE_CreateInstance( 4*configMINIMAL_STACK_SIZE, configMAX_PRIORITIES - 3, c, &s);
}



