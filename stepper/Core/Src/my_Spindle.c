/*
 * spindle.c
 *
 *  Created on: Oct 27, 2025
 *      Author: Administrator
 */
#include "my_spindle.h" //
#include "Spindle.h"
#include "Console.h" // Für ConsoleHandle_t
//Hardwarespezifische Funktionen
void SPINDLE_SetDirection(SpindleHandle_t h, void* context, int backward){}
void SPINDLE_SetDutyCycle(SpindleHandle_t h, void* context, float dutyCycle){}
void SPINDLE_EnaPWM(SpindleHandle_t h, void* context, int ena){}
void Initialize_Spindle(ConsoleHandle_t c){
	int configMINIMAL_STACK_SIZE = 128; //gibt minimale Speicherallokation an, die währen der Task auftreten kann, um Owerfolow zu vermeiden
	int configMAX_PRIORITIES = 10; //definiert das Prioritätslevel der Task, später sinnvoll global anzugeben
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



