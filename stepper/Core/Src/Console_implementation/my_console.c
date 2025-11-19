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
#include "Stepper_implementation/my_stepper.h"
#include <stdio.h>
#include <stdbool.h>

extern bool error_variable;
extern L6474_Handle_t stepperHandle;

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

int StepperCommand(int argc, char **argv, void *context)
{
    // Mechanikparameter – zentral definiert, Informationen aus dem Pflichtenheft
    const int steps_per_turn = 200; // 200 Schritte pro Umdrehung, jeder Schritt hat 16 Mikrosteps
    const int microsteps = 16;	// 200 * 16 = 3200 Mikro-Schritte pro Umdrehung
    const float mm_per_turn = 4.0f;	// 4 mm vorwaerts/ruckwaerts pro Umdrehung beim Linearantrieb

    // wenn keine zusätzliche commands dazu gegeben werden
    if(argc < 1)
    {
        printf("Fail: No subcommand provided\r\n");
        return -1;
    }

    // Abfrage ob subcommand "move": synchron oder asynchron
    else if (strcmp(argv[0], "move") == 0)
    {
        float target_mm = 0.0f;	// wie weit der Schrittmotor fahren soll
        float speed_mm_per_min = 500.0f; // Geschwindigkeit pro min.
        int async = 0; // Variable für asynchron oder synchron
        int relative = 0; // Variable für relative oder absolute Bewegung


        if (argc == 2)
        {
        	// atof converts string to float
        	// stepper bewegt sich um target_mm in eine Richtung
            target_mm = atof(argv[1]);
        }
        else if (argc == 4 && strcmp(argv[1], "-s") == 0)
        {
        	// speed subcommand -> move -s Geschwindigkeit Ziel
            speed_mm_per_min = atof(argv[2]);
            target_mm = atof(argv[3]);
        }
        else if (argc == 3 && strcmp(argv[1], "-a") == 0)
        {
        	// absolute subcommand -> move -a absolutes Ziel
            target_mm = atof(argv[2]);
            async = 1;
        }
        else if (argc == 3 && strcmp(argv[1], "-r") == 0)
        {
        	// relative subcommand -> move -r relatives Ziel
            target_mm = atof(argv[2]);
            relative = 1;
        }
        else
        {
        	// falls nur move eingegeben wird -> etwas fehlt, Ziel oder Ziel + speed
            printf("Fail: Invalid move syntax\r\n");
            return -1;
        }


        // Treiber und LEDs werden angeschaltet -> siehe my_stepper.c
        if (EnableStepperDrivers() != 0) {
            printf("Fail: Could not enable drivers\r\n");
            return -1;
        }

        // absolute Position abfragen
        int current_steps;
        // gibt die absolute Position des Schrittmotors zurück -> absolute Anzahl an steps ausgehend
        // von reference point werden zurueckgegeben
        if (L6474_GetAbsolutePosition(stepperHandle, &current_steps) != errcNONE) {
            printf("Fail: Could not read current position\r\n");
            return -1;
        }

        // Berechnung wie weit man in mm von reference point entfernt ist
        float current_mm = ((float)current_steps * mm_per_turn) / (steps_per_turn * microsteps);

        // Berechnung von Schritte pro Umdrehung und mm pro Umdrehung -> siehe Fotos Roman
        if (relative)
        {
            steps = target_mm / 4 * steps_per_turn * microsteps;

            // da nur eine absolute Position mit der move-Funktion angefahren werden kann
            steps += current_steps;
        }
        else
        {
            steps = (target_mm + current_mm) / 4 * steps_per_turn * microsteps;
        }

        if (steps == 0) {
            printf("Ok, Already at target position\r\n");
            // man muss nichts machen und wartet dann wieder auf den nächsten Befehl
            return 0;
        }

        // Formel: uebergeben wird die gewuenschte Geschwindigkeit in mm pro min
        // Berechnung: Schritte pro Sek. = gewuenscht. v in mm/min * Schritte (pro Umdrehung) / (mm (pro Umdrehung) * 60 sek.)
        // -> siehe Notizen OneNote

        float sec_per_min = 60.0f;
        float steps_per_sec = speed_mm_per_min * (steps_per_turn * microsteps) / (mm_per_turn * sec_per_min);
        SetStepperSpeed(steps_per_sec);
        // ternary operator for absolute oder relative Unterscheidung
        float delta_mm = relative ? target_mm : (target_mm - current_mm);
        //TODO: delta_mm evt. noch falsch, wenn absolute Fahrt umgesetzt wird -> fuer debugging Zwecke
        printf("Moving %.2f mm at %.2f steps/sec (%d steps)\r\n", delta_mm, steps_per_sec, steps);

        // Funktion die ausgefuehrt wird, damit der Schrittmotor faehrt
        L6474_StepIncremental(stepperHandle, steps);

        // fuer synchrone Fahrt -> andere Tasks werden durch vTaskDelay verhindert
        if (!async)
        {
            int moving = 1;
            while(moving == 1){
                L6474_IsMoving(stepperHandle, &moving); // Funktion schreibt in moving rein, ob sich Stepper noch bewegt oder nicht
                vTaskDelay(100);
            }
        }

        return 0;
    }

    // Abfrage ob sucommand "reference": synchron mit Stop bei Schalter
    else if(strcmp(argv[0], "reference") == 0) {
    	float speed_mm_per_min = 500.0f;
    	const int steps_per_turn = 200;
    	    const int microsteps = 16;
    	    const float mm_per_turn = 4.0f;
    	    float steps_per_sec = CalculateStepsPerSecond(speed_mm_per_min, steps_per_turn, microsteps, mm_per_turn);
    	            SetStepperSpeed(steps_per_sec);

        if (EnableStepperDrivers() != 0) {
            printf("Fail: Could not enable drivers\r\n");
            return -1;
        }

        L6474_StepIncremental(stepperHandle, -10000000);

        int moving = 1;
        while (moving == 1) {
            if (HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_RESET) {
                L6474_StopMovement(stepperHandle);
                L6474_SetPositionMark(stepperHandle, 0);
                L6474_SetAbsolutePosition(stepperHandle, 0);
                doneReference = 1;
                printf("Ok, Reference found and position set to 0\r\n");
                return 0;
            }

            L6474_IsMoving(stepperHandle, &moving);
            vTaskDelay(100);
        }

        printf("Fail: Reference movement stopped unexpectedly\r\n");
        return -1;
    }

    // POSITION: Ausgabe in mm
    else if(strcmp(argv[0], "position") == 0) {
        int steps;
        if (L6474_GetAbsolutePosition(stepperHandle, &steps) != errcNONE) {
            printf("Fail: Could not read absolute position\r\n");
            return -1;
        }

        float pos_mm = ((float)steps * mm_per_turn) / (steps_per_turn * microsteps);
        printf("Ok, Current absolute position: %ld steps = %.2f mm\r\n", steps, pos_mm);
        return 0;
    }

    // STATUS
    else if(strcmp(argv[0], "status") == 0) {
        L6474_Status_t status;
        if (L6474_GetStatus(stepperHandle, &status) != errcNONE) {
            printf("Fail: Could not read status\r\n");
            return -1;
        }

        printf("Ok, Stepper status:\r\n");
        printf("  HIGHZ      : %d\r\n", status.HIGHZ);
        printf("  DIR        : %d\r\n", status.DIR);
        printf("  ONGOING    : %d\r\n", status.ONGOING);
        printf("  UVLO       : %d\r\n", status.UVLO);
        printf("  TH_SD      : %d\r\n", status.TH_SD);
        printf("  OCD        : %d\r\n", status.OCD);
        return 0;
    }

    // RESET
    else if(strcmp(argv[0], "reset") == 0) {
        printf("Ok, Resetting stepper...\r\n");
        if(L6474_ResetStandBy(stepperHandle) != errcNONE ||
           L6474_Initialize(stepperHandle, &baseParams) != errcNONE) {
            printf("Fail: Reset or re-init failed\r\n");
            HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
            blueLedBlinking = 0;
            return -1;
        }

        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
        blueLedBlinking = 0;
        return 0;
    }

    // CANCEL
    else if(strcmp(argv[0], "cancel") == 0) {
        if (L6474_StopMovement(stepperHandle) != errcNONE) {
            printf("Fail: Could not cancel movement\r\n");
            return -1;
        }
        printf("Ok, Movement cancelled\r\n");
        return 0;
    }

    printf("Fail: Unknown Stepper subcommand\r\n");
    return -1;
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

    // Stepper Befehl registrieren
    CONSOLE_RegisterCommand(console_handle, "stepper", "Stepper control: move <AbsPos> | reference", StepperCommand, NULL);

    // Spindle initialisieren
    Initialize_Spindle(console_handle);

    // Stepper initialisieren
    Initialize_Stepper();
}
