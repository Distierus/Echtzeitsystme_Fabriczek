/*
 * my_console.c
 *
 *  Created on: Oct 28, 2025
 *      Author: Theo Schreiber
 */
#include "Console_implementation/my_console.h"
#include "Console.h"
#include "ConsoleConfig.h"
#include "FreeRTOSConfig.h"
#include "Spindle_implementation/my_spindle.h"
#include <stdio.h>
#include <stdbool.h>

extern bool error_variable;


// register the function, there is always a help text required, an empty string or null is not allowed!
static int CapabilityFunc( int argc, char** argv, void* ctx )
{
	(void)argc;
	(void)argv;
	(void)ctx;
    printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\nOK",
        1, // has spindle
        1, // has spindle status
        1, // has stepper
        1, // has stepper move relative
        1, // has stepper move speed
        0, // has stepper move async
        0, // has stepper status
        0, // has stepper refrun
        0, // has stepper refrun timeout
        0, // has stepper refrun skip
        0, // has stepper refrun stay enabled
        0, // has stepper reset
        0, // has stepper position
        0, // has stepper config
        0, // has stepper config torque
        0, // has stepper config throvercurr
        0, // has stepper config powerena
        0, // has stepper config stepmode
        0, // has stepper config timeoff
        0, // has stepper config timeon
        0, // has stepper config timefast
        0, // has stepper config mmperturn
        0, // has stepper config posmax
        0, // has stepper config posmin
        0, // has stepper config posref
        0, // has stepper config stepsperturn
        0  // has stepper cancel
    );
    return 0;
}



// create the console processor. There are no additional arguments required because it uses stdin, stderr and
// stdout of the stdlib of the platform
ConsoleHandle_t console_handle =  NULL;
void MyConsole_Init(void)
{
    // Jetzt die Instanz zur Laufzeit erstellen und der globalen Variable zuweisen
    console_handle = CONSOLE_CreateInstance( 4 * configMINIMAL_STACK_SIZE, configMAX_PRIORITIES - 5 );

    // Befehl registrieren, nachdem die Instanz erstellt wurde
    CONSOLE_RegisterCommand(console_handle, "capability", "prints a specified string of capability bits", CapabilityFunc, NULL);

    // Spindle initialisieren
    Initialize_Spindle(console_handle);

    // Stepper initialisieren
    // Initialize_Stepper();
}
