/*
 * my_console.c
 *
 *  Created on: Oct 28, 2025
 *      Author: Basti
 */
#include "Console_implementation/my_console.h"
#include "Console.h"
#include "ConsoleConfig.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "LibL6474.h"
#include "Spindle_implementation/my_spindle.h"
#include "Stepper_implementation/my_stepper.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <main.h>
#include <task.h> // wichtig für vTaskDelay() !!!

extern bool error_variable;
extern L6474_Handle_t stepperHandle;
bool doneReference = false; // bevor reference Fahrt nicht gemacht wurde, darf Stepper nicht auf absolute Position 0 fahren
extern int blueLedBlinking;
int steps = 0; // for calculation of steps
float sec_per_min = 60.0f;
extern L6474_BaseParameter_t base_parameter;


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
        0, // has stepper refrun timeout
        0, // has stepper refrun skip
        0, // has stepper refrun stay enabled
        1, // has stepper reset
        1, // has stepper position
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
        1  // has stepper cancel
    );
    return 0;
}

int StepperCommand(int argc, char **argv, void *context)
{
	// da context nicht verwendet wird
	(void)context;

	// Mechanikparameter – zentral definiert, Informationen aus dem Pflichtenheft
	const int steps_per_turn = 200; // 200 Schritte pro Umdrehung, jeder Schritt hat 16 Mikrosteps
	const int microsteps = 16;	// 200 * 16 = 3200 Mikro-Schritte pro Umdrehung
	const float mm_per_turn = 4.0f;	// 4 mm vorwaerts/ruckwaerts pro Umdrehung beim Linearantrieb

	// wenn keine zusätzliche commands dazu gegeben werden
	if(argc == 0)
	{
		printf("FAIL: No subcommand provided\r\n");
		return -1;
	}

    // Abfrage ob subcommand "move": synchron oder asynchron
    else if (strcmp(argv[0], "move") == 0)
    {
        float target_mm = 0.0f;	// wie weit der Schrittmotor fahren soll
        float speed_mm_per_min = 500.0f; // Geschwindigkeit pro min.
        int async = 0; // Variable für asynchron oder synchron
        int relative = 0; // Variable für relative oder absolute Bewegung

        if (doneReference == false)
        {
        	printf("FAIL: reference run not done\r\n");
        	return -1;
        }

        /*
         * old command if causes -> falls neue subcommand Abfragen nicht mehr funktionieren
        if (argc == 2)
        {
        	// atof converts string to float
        	// stepper bewegt sich um target_mm in eine Richtung
            target_mm = atof(argv[1]);
        }
        else if (argc == 4 && strcmp(argv[2], "-s") == 0)
        {
        	// speed subcommand -> move -s Geschwindigkeit Ziel
       		speed_mm_per_min = atof(argv[3]);
       		target_mm = atof(argv[1]);
        }
        else if (argc == 3 && strcmp(argv[2], "-a") == 0)
        {
        	// absolute subcommand -> move -a absolutes Ziel
            target_mm = atof(argv[1]);
            async = 1;
        }
        else if (argc == 3 && strcmp(argv[2], "-r") == 0)
        {
        	// relative subcommand -> move -r relatives Ziel
            target_mm = atof(argv[1]);
            relative = 1;
        }
        else
        {
        	// falls nur move eingegeben wird -> etwas fehlt, Ziel oder Ziel + speed
            printf("FAIL: Invalid move syntax\r\n");
            return -1;
        }
        */


        if (argc < 2) {
			printf("Invalid number of arguments\r\n");
			return -1;
		}

		target_mm = atof(argv[1]);



		for (int i = 2; i < argc; ) {
			// async flag
			if (strcmp(argv[i], "-a") == 0)
			{
				async = 1;
				i++;
			}

			// relative flag
			else if (strcmp(argv[i], "-r") == 0)
			{
				relative = 1;
				i++;
			}

			// speed flag
			else if (strcmp(argv[i], "-s") == 0)
			{
				if (i == argc - 1) {
					printf("Invalid number of arguments\r\n");
					return -1;
				}

				speed_mm_per_min = atof(argv[i + 1]);
				i += 2;
			}

			else
			{
				printf("Invalid Flag\r\n");
				return -1;
			}
		}


        // Treiber und LEDs werden angeschaltet -> siehe my_stepper.c
        if (EnableStepperDrivers() != 0)
        {
            printf("FAIL: Could not enable drivers\r\n");
            return -1;
        }

        if (HAL_GPIO_ReadPin(LIMIT_SWITCH_GPIO_Port, LIMIT_SWITCH_Pin) == GPIO_PIN_RESET)
		{
        	// wenn absoluter Wert angegeben wird
			if (relative == 1)
			{
				if (target_mm > 0)
				{
					printf("FAIL: stepper cannot move in this direction due to reached limit switch\r\n");
					return -1;
				}
			}
		}

        // absolute Position abfragen
        int current_steps = 0;
        // gibt die absolute Position des Schrittmotors zurück -> absolute Anzahl an steps ausgehend
        // von reference point werden zurueckgegeben
        if (L6474_GetAbsolutePosition(stepperHandle, &current_steps) != errcNONE)
        {
            printf("FAIL: Could not read current position\r\n");
            return -1;
        }

        // Berechnung wie weit man in mm von reference point entfernt ist
        float current_mm = ((float)current_steps * mm_per_turn) / (steps_per_turn * microsteps);

        // Berechnung von Schritte pro Umdrehung und mm pro Umdrehung -> siehe Fotos Roman
        if (relative)
        {
        	// current_mm + target_mm, current_mm, steps_per_turn, microsteps, mm_per_turn
			float delta_mm = (target_mm + current_mm) - current_mm;
			steps = (delta_mm * steps_per_turn * microsteps) / mm_per_turn;
            // steps = target_mm / 4 * steps_per_turn * microsteps;

            // da nur eine absolute Position mit der move-Funktion angefahren werden kann
            // steps += current_steps;
        }
        else
        {
        	float delta_mm = target_mm - current_mm;
        	steps = (delta_mm * steps_per_turn * microsteps) / mm_per_turn;
            // steps = (target_mm + current_mm) / 4 * steps_per_turn * microsteps;
        }

        if (steps == 0)
        {
            printf("OK, Already at target position\r\n");
            // man muss nichts machen und wartet dann wieder auf den nächsten Befehl
            return 0;
        }

        // Formel: uebergeben wird die gewuenschte Geschwindigkeit in mm pro min
        // Berechnung: Schritte pro Sek. = gewuenscht. v in mm/min * Schritte (pro Umdrehung) / (mm (pro Umdrehung) * 60 sek.)
        // -> siehe Notizen OneNote

        float steps_per_sec = speed_mm_per_min * (steps_per_turn * microsteps) / (mm_per_turn * sec_per_min);
        SetStepperSpeed(steps_per_sec);
        // ternary operator for absolute oder relative Unterscheidung
        float delta_mm = relative ? target_mm : (target_mm - current_mm);
        //TODO: delta_mm evt. noch falsch, wenn absolute Fahrt umgesetzt wird -> fuer debugging Zwecke printf
        printf("Moving %.2f mm at %.2f steps/sec (%d steps)\r\n", delta_mm, steps_per_sec, steps);

        // Funktion die ausgefuehrt wird, damit der Schrittmotor faehrt
        L6474_StepIncremental(stepperHandle, steps);

        // fuer synchrone Fahrt -> andere Tasks werden durch vTaskDelay verhindert
        if (!async)
        {
            int moving = 1;
            while(moving == 1)
            {
                L6474_IsMoving(stepperHandle, &moving); // Funktion schreibt in moving rein, ob sich Stepper noch bewegt oder nicht
                vTaskDelay(100);
            }
        }

        return 0;
    }

    // Abfrage ob sucommand "reference": synchron mit Stop bei Schalter
    else if(strcmp(argv[0], "reference") == 0)
    {
    	float speed_mm_per_min = 500.0f;
    	const int steps_per_turn = 200;
		const int microsteps = 16;
		const float mm_per_turn = 4.0f;
		float steps_per_sec = speed_mm_per_min * (steps_per_turn * microsteps) / (mm_per_turn * sec_per_min);
		SetStepperSpeed(steps_per_sec);

		bool power_output_enabled_after_refrun = false;
		bool skip_reference_enabled = false;
		float timeout_ms = 0;

		for (int i = 1; i < argc; )
		{
			// additional timeout in seconds
			if (strcmp(argv[i], "-t") == 0)
			{
				if (i == argc - 1)
				{
					printf("Invalid number of arguments\r\n");
					return -1;
				}

				timeout_ms = atof(argv[2]) * 1000;
				if (timeout_ms <= 0)
				{
					printf("Invalid timeout value\r\n");
					return -1;
				}
				i += 2;
			}

			// enable power output after reference run
			else if (strcmp(argv[i], "-e") == 0)
			{
				power_output_enabled_after_refrun = true;
				i++;
			}

			// skip reference run
			else if (strcmp(argv[i], "-s") == 0)
			{
				skip_reference_enabled = true;
				i++;
			}

			else
			{
				printf("Invalid Flag\r\n");
				return -1;
			}
		}


        if (EnableStepperDrivers() != 0)
        {
            printf("FAIL: Could not enable drivers\r\n");
            return -1;
        }


        // TODO: finish implementation of timeout
        if (power_output_enabled == true)
		{
			// TODO: implement this function
        	printf("FAIL: power output enabled after reference not implemented\r\n");
		}

        // TODO: new implementation after testing in DHBW -> test now
        if (skip_reference_enabled == true)
        {
        	L6474_SetPositionMark(stepperHandle, 0);
			L6474_SetAbsolutePosition(stepperHandle, 0);
			doneReference = true;
        	return 0;
        }

        // stepper already at reference mark
        if(HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_RESET) {
			// stepper faehrt von reference point zuerst mal nochmal 2mm weg
			L6474_StepIncremental(stepperHandle, 1600);
			// int result = L6474_StepIncremental(stepperHandle, 1600);
			// printf("%d\n", result);
			// result = L6474_StopMovement(stepperHandle); // just for safety
			// printf("%d\n", result);
			vTaskDelay(1000);
		}

        L6474_StepIncremental(stepperHandle, -10000000);

        // da synchrone Fahrt wird immer noch abgefragt, ob der Schrittmotor sich noch bewegt
        // solange Motor sich noch bewegt ist moving = 1
        int moving = 1;
        while (moving == 1)
        {
            if (HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_RESET)
            {
                L6474_StopMovement(stepperHandle);
                L6474_SetPositionMark(stepperHandle, 0);
                L6474_SetAbsolutePosition(stepperHandle, 0);
                doneReference = true; // es soll geschaut werden, ob reference Fahrt schon gemacht wurde
                // ob Referenzfahrt vor absoluter Fahrt zu 0 schon gemacht wurde
                printf("OK, Reference found and position set to 0\r\n");
                return 0;
            }

            L6474_IsMoving(stepperHandle, &moving);
            vTaskDelay(100);
        }

        // falls Schrittmotor nicht an Referenzpunkt angekommen ist wird while-loop verlassen
        printf("FAIL: Reference movement stopped unexpectedly\r\n");
        return -1;
    }

    // Abfrage fuer subcommand "position": Ausgabe in mm
    else if(strcmp(argv[0], "position") == 0)
    {
        int steps;
        if (L6474_GetAbsolutePosition(stepperHandle, &steps) != errcNONE)
        {
            printf("Fail: Could not read absolute position\r\n");
            return -1;
        }

        float pos_mm = ((float)steps * mm_per_turn) / (steps_per_turn * microsteps);
        printf("OK, Current absolute position: %d steps = %.2f mm\r\n", steps, pos_mm);
        return 0;
    }

    // Abfrage fuer subcommand "status"
    else if(strcmp(argv[0], "status") == 0)
    {
        L6474_Status_t status;
        if (L6474_GetStatus(stepperHandle, &status) != errcNONE)
        {
            printf("FAIL: Could not read status\r\n");
            return -1;
        }

        // unterschiedliche Werte des Status structs werden auf der Konsole ausgegeben
        printf("Ok, Stepper status:\r\n");
        printf("  HIGHZ      : %d\r\n", status.HIGHZ);
        printf("  DIR        : %d\r\n", status.DIR);
        printf("  ONGOING    : %d\r\n", status.ONGOING);
        printf("  UVLO       : %d\r\n", status.UVLO);
        printf("  TH_SD      : %d\r\n", status.TH_SD);
        printf("  OCD        : %d\r\n", status.OCD);
        return 0;
    }

    // Abfrage fuer subcommand "reset"
    // TODO: Loesung suchen
    else if(strcmp(argv[0], "reset") == 0)
    {
        printf("OK, Resetting stepper...\r\n");
        if(L6474_ResetStandBy(stepperHandle) != errcNONE ||
           L6474_Initialize(stepperHandle, &base_parameter) != errcNONE)
        {
            printf("FAIL: Reset or re-init failed\r\n");
            HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
            // printf("blueLedBlinking disabled3\n"); // TODO: remove debugging
            blueLedBlinking = 0;
            return -1;
        }

        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
        blueLedBlinking = 0;
        return 0;
    }

    // Abfrage fuer sub command "cancel" (nur fuer asynchrone Fahrt)
    else if(strcmp(argv[0], "cancel") == 0)
    {
        if (L6474_StopMovement(stepperHandle) != errcNONE) {
            printf("FAIL: Could not cancel movement\r\n");
            return -1;
        }
        printf("OK, Movement cancelled\r\n");
        return 0;
    }

	// Abfrage fuer config subcommand
    else if (strcmp(argv[0], "config") == 0)
    {
		// result = config(stepper_ctx, argc, argv);
	}

    printf("FAIL: Unknown Stepper sub command\r\n");
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
    CONSOLE_RegisterCommand(console_handle, "stepper", "commands to control the stepper command", StepperCommand, NULL);

    // Spindle initialisieren
    Initialize_Spindle(console_handle);

    // Stepper initialisieren
    Initialize_Stepper();
}
