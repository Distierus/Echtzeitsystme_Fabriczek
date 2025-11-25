#ifndef MY_STEPPER_H
#define MY_STEPPER_H

#include "LibL6474.h"
#include "LibL6474Config.h"
#include "FreeRTOSConfig.h"

// functions which are included in the library documentation:
void* StepLibraryMalloc( unsigned int size );
void StepLibraryFree( const void* const ptr );
int StepDriverSpiTransfer( void* pIO, char* pRX, const char* pTX, unsigned int length );

void StepDriverReset(void *pGPO, const int ena);
void StepLibraryDelay(unsigned int ms);
int StepTimerAsync(void *pPWM, int dir, unsigned int numPulses, void(*doneClb)(L6474_Handle_t), L6474_Handle_t h);
int StepTimerCancelAsync(void *pPWM);

// own functions
void Initialize_Stepper(void);
void SetStepperSpeed(float steps_per_sec);
void FindOptimalTimerSettings(float steps_per_sec, uint32_t timer_clk, uint16_t *out_prescaler, uint16_t *out_arr);
int check_abs(L6474_Handle_t t, int mm_to_move);
// void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim);
int EnableStepperDrivers(void);
void vLedBlinkTask(void* pvParameters);

#endif
