#include "Stepper_implementation/my_stepper.h"
#include "Controller.h"
#include "LibL6474.h"
#include "LibL6474Config.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "main.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h> // wichtig für fabsf Funktion
#include <task.h> // wichtig für vTaskDelay() !!!

extern int blueLedBlinking;
L6474_Handle_t stepperHandle;

// in main.c definiert
extern SPI_HandleTypeDef hspi1;
extern int asyncStepsRemaining;
extern L6474_Handle_t asyncStepperHandle;
extern void (*asyncDoneCallback)(L6474_Handle_t);
extern TIM_HandleTypeDef htim4;
extern L6474_BaseParameter_t base_parameter;

void Initialize_Stepper(void)
{


	// einstellen, dass Stepper max. 0.6A ziehen darf
	L6474_EncodePhaseCurrentParameter(&base_parameter, 600.0f);

	// den overcurrent detecten threshold neu setzen
	base_parameter.OcdTh = ocdth3000mA;

	// from LibL6474 library documentation:
	// pass all function pointers required by the stepper library
	// to a separate platform abstraction structure
	L6474x_Platform_t p;
	p.malloc     = StepLibraryMalloc;
	p.free       = StepLibraryFree;
	p.transfer   = StepDriverSpiTransfer;
	p.reset      = StepDriverReset;
	p.sleep      = StepLibraryDelay;

	// nicht mehr auskommentiert, da Flag in LibL6474Config.h Header gesetzt wurde
	p.stepAsync  = StepTimerAsync;
	p.cancelStep = StepTimerCancelAsync;


	// now create the handle
	stepperHandle = L6474_CreateInstance(&p, NULL, NULL, NULL);
	if (stepperHandle == NULL)
	{
		printf("error at creating instance in my_stepper.c\n");
		// gruene und blaue LED aus und rote LED an
		HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, 0);
		HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, 0);
		HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, 1);
	}
}

// from LibL6474 library documentation
void* StepLibraryMalloc( unsigned int size )
{
	// size	number of bytes requested by the memory allocation request
	return malloc(size);
}

// from LibL6474 library documentation
void StepLibraryFree( const void* const ptr )
{
	free((void*)ptr);
}

// from LibL6474 library documentation
int StepDriverSpiTransfer( void* pIO, char* pRX, const char* pTX, unsigned int length )
{
	// byte based access, so keep in mind that only single byte transfers are performed!
	// siehe stm32f7xx_hal_def.h Z. 38
	HAL_StatusTypeDef errorcode = HAL_OK;

	for ( unsigned int i = 0; i < length; i++ )
	{
		// Warum CS low nach jedem Byte? -> wegen dem UML Diagramm in der Doku
		// bringe Chips Select low to transfer data
		HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 0);

		// TransmitReceive, damit weder Transmit Byte noch Receive Byte verworfen wird
		// 1 Byte versenden und 1 Byte empfangen, HAL_MAX_DELAY aus stm32f7xx_hal_def.h Z. 61
		errorcode = HAL_SPI_TransmitReceive(&hspi1, (uint8_t*) &(pTX[i]), (uint8_t*) &(pRX[i]), 1, HAL_MAX_DELAY);

		// bringe Chips Select hight to end data transfer
		HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 1);

		if (errorcode != HAL_OK)
		{
			// return -1, wenn Fehler gesetzt wurde
			return -1;
		}
	}
	// TODO: if something is not working put CS High command here

	return 0;
}

void StepDriverReset(void *pGPO, const int ena)
{
	// the reset function is used to provide gpio access to the reset of the stepper driver chip.
	// the chip has a reset not pin and so the ena signal must be inverted to set the correct reset level physically
	HAL_GPIO_WritePin(STEP_RSTN_GPIO_Port, STEP_RSTN_Pin, !ena);
}

void StepLibraryDelay(unsigned int ms)
{
	// damit keine anderen Tasks ausgefuehrt werden koennen -> Dispatcher kann keinen anderen Task anbieten
	vTaskDelay(ms);
}

int StepTimerAsync(void *pPWM, int dir, unsigned int numPulses, void(*doneClb)(L6474_Handle_t), L6474_Handle_t h)
{
	(void)pPWM;

	// Richtung setzen
	HAL_GPIO_WritePin(GPIOF, GPIO_PIN_13, dir ? GPIO_PIN_SET : GPIO_PIN_RESET);

	asyncStepsRemaining = numPulses;
	asyncStepperHandle = h;
	asyncDoneCallback = doneClb;
	//Timer PWM Interrupt starten
	HAL_TIM_PWM_Start_IT(&htim4, TIM_CHANNEL_4);

	return 0;
}

int StepTimerCancelAsync(void *pPWM)
{
	HAL_TIM_PWM_Stop_IT(&htim4, TIM_CHANNEL_4);
	return 0;
}

// konfiguriert den Timer -> Timer entscheidet, wie schnell PWM Signal ist
void SetStepperSpeed(float steps_per_sec)
{
    if (steps_per_sec <= 0)
    {
    	return;
    }

    uint16_t prescaler, arr;
    FindOptimalTimerSettings(steps_per_sec, 90000000, &prescaler, &arr);

    TIM4->PSC = prescaler;
    TIM4->ARR = arr;
    TIM4->CCR4 = arr / 2;
    TIM4->EGR = TIM_EGR_UG;

    printf("Timer configured: PSC=%u, ARR=%u → %.2f steps/sec\r\n", prescaler, arr,
           90000000.0f / ((prescaler + 1) * (arr + 1)));
}

// findet die optimalen Timer Einstellungen, damit Schrittmotor vernünftig läuft
void FindOptimalTimerSettings(float steps_per_sec, uint32_t timer_clk, uint16_t *out_prescaler, uint16_t *out_arr)
{
    float best_error = 1e9;
    uint16_t best_prescaler = 0;
    uint16_t best_arr = 0;

    for (uint16_t prescaler = 1; prescaler <= 1000; prescaler++)
    {
        float temp = (float)timer_clk / (prescaler * steps_per_sec);
        uint16_t arr = (uint16_t)(temp + 0.5f) - 1;

        if (arr == 0)
        {
        	continue;
        }

        float actual_freq = (float)timer_clk / (prescaler * (arr + 1));
        float error = fabsf(actual_freq - steps_per_sec); // fabsf berechnet den absoluten Wert von dem Argument

        // Naehe zwischen Prescaler und ARR soll moeglichst klein sein, damit move so klein wie moeglich ist
        float balance = fabsf((float)prescaler - (float)arr);

        // Kombiniertes Bewertungskriterium
        float score = error + balance * 0.01f;

        if (score < best_error) {
            best_error = score;
            best_prescaler = prescaler - 1;
            best_arr = arr;
        }
    }

    *out_prescaler = best_prescaler;
    *out_arr = best_arr;
}

/*
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &htim4)
    {
        //Limit-Schalter prüfen
        if (HAL_GPIO_ReadPin(LIMIT_SWITCH_GPIO_Port, LIMIT_SWITCH_Pin) == GPIO_PIN_RESET)
        {
            HAL_TIM_PWM_Stop_IT(&htim4, TIM_CHANNEL_4);
            printf("Fail: Async movement stopped due to limit switch\r\n");
            if (asyncDoneCallback && asyncStepperHandle)
            {
                asyncDoneCallback(asyncStepperHandle);
            }
            return;
        }

        if (asyncStepsRemaining <= 1)
        {
            HAL_TIM_PWM_Stop_IT(&htim4, TIM_CHANNEL_4);
            if (asyncDoneCallback && asyncStepperHandle)
            {
                asyncDoneCallback(asyncStepperHandle);
            }
            return;
        }
        asyncStepsRemaining--;
    }
}
*/

// Helferfunktion zur Aktivierung der Treiber (und LEDs)
// TODO: kommentieren
int EnableStepperDrivers(void)
{
    L6474_Status_t status;
    // Status holen, um zu prüfen, ob Treiber schon aktiv sind
    if (L6474_GetStatus(stepperHandle, &status) != errcNONE)
    {
        // LED FEHLER [cite: 399]
        HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
        blueLedBlinking = 0;
        return -1; // Fehler
    }

    // Prüfen, ob Treiber im High-Z (AUS) sind
    if (status.HIGHZ)
    {
        printf("Enabling power outputs...\r\n");
        if (L6474_SetPowerOutputs(stepperHandle, 1) != errcNONE)
        {
            // LED FEHLER [cite: 399]
            HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
            blueLedBlinking = 0;
            return -1; // Fehler
        }

        // LED AKTIV (Grün AN, Blau blinkt) [cite: 309, 397-398]
        blueLedBlinking = 1;
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
    }
    // Falls Treiber schon aktiv waren, blinkt Blau bereits, nichts zu tun.
    return 0; // Erfolg
}


// Task fuer die blaue LED
void vLedBlinkTask(void* pvParameters)
{
  (void)pvParameters;

  while(1)
  {
	  if (blueLedBlinking)
	  {
		  // Blaue LED 1Hz blinken (500ms an, 500ms aus)
		  HAL_GPIO_TogglePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin);
		  vTaskDelay(pdMS_TO_TICKS(500));
	  }

	  else
	  {
		  // Wenn Blinken deaktiviert ist, LED ausschalten und kurz warten
		  HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
		  vTaskDelay(pdMS_TO_TICKS(100)); // Poll-Intervall
	  }
  }
}
