#ifndef MY_STEPPER_H
#define MY_STEPPER_H

// functions which are included in the library documentation:
static void* StepLibraryMalloc( unsigned int size );
static void StepLibraryFree( const void* const ptr );
static int StepDriverSpiTransfer( void* pIO, char* pRX, const char* pTX, unsigned int length );

static void StepDriverReset(void *pGPO, const int ena);
static void StepLibraryDelay(unsigned int ms);
// static int StepTimerAsync(void *pPWM, int dir, unsigned int numPulses, void(*doneClb)(L6474_Handle_t), L6474_Handle_t h);
// static int StepTimerCancelAsync(void *pPWM);

// own functions - brauche ich dir ueberhaupt noch?:
void reset_stepper(void);

#endif
