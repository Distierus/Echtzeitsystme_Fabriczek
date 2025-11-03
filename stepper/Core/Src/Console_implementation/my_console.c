/*
 * my_console.c
 *
 *  Created on: Oct 28, 2025
 *      Author: Theo Schreiber
 */
#include "my_console.h"
#include "Console.h"
#include "ConsoleConfig.h"
#include "FreeRTOSConfig.h"
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
        1, // has stepper move async
        1, // has stepper status
        1, // has stepper refrun
        1, // has stepper refrun timeout
        1, // has stepper refrun skip
        1, // has stepper refrun stay enabled
        1, // has stepper reset
        1, // has stepper position
        1, // has stepper config
        1, // has stepper config torque
        1, // has stepper config throvercurr
        1, // has stepper config powerena
        1, // has stepper config stepmode
        1, // has stepper config timeoff
        1, // has stepper config timeon
        1, // has stepper config timefast
        1, // has stepper config mmperturn
        1, // has stepper config posmax
        1, // has stepper config posmin
        1, // has stepper config posref
        1, // has stepper config stepsperturn
        1  // has stepper cancel
    );
    return 0;
}



// create the console processor. There are no additional arguments required because it uses stdin, stderr and
// stdout of the stdlib of the platform
ConsoleHandle_t c =  NULL;
void MyConsole_Init(void)
{
    // Jetzt die Instanz zur Laufzeit erstellen und der globalen Variable zuweisen
    c = CONSOLE_CreateInstance( 4 * configMINIMAL_STACK_SIZE, configMAX_PRIORITIES - 5 );

    // Befehl registrieren, nachdem die Instanz erstellt wurde
    CONSOLE_RegisterCommand(c, "capability", "prints a specified string of capability bits",
      CapabilityFunc, NULL);
}
