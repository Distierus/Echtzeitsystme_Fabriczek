#ifndef MY_STEPPER_H
#define MY_STEPPER_H

// functions which are included in the library documentation:
static void* StepLibraryMalloc( unsigned int size );
static void StepLibraryFree( const void* const ptr );
static int StepDriverSpiTransfer( void* pIO, char* pRX, const char* pTX, unsigned int length );

// own functions:
static int reset_stepper(void);

#endif
