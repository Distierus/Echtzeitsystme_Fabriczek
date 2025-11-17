#include "Stepper_implementation/my_stepper.h"
#include "Controller.h"
#include "LibL6474.h"
#include "LibL6474Config.h"
#include "FreeRTOSConfig.h"
#include "main.h"
#include "stdint.h"


// from LibL6474 library documentation
static void* StepLibraryMalloc( unsigned int size )
{
     return malloc(size);
}

// from LibL6474 library documentation
static void StepLibraryFree( const void* const ptr )
{
     free((void*)ptr);
}

// from LibL6474 library documentation
static int StepDriverSpiTransfer( void* pIO, char* pRX, const char* pTX, unsigned int length )
{
	// byte based access, so keep in mind that only single byte transfers are performed!
	for ( unsigned int i = 0; i < length; i++ )
	{
	//TODO
	}
	return 0;
}

//TODO

static int reset_stepper(void)
{
	L6474_BaseParameter_t base_parameter;
	L6474_SetBaseParameter(&base_parameter);
}
// TODO

// from LibL6474 library documentation:
// pass all function pointers required by the stepper library
// to a separate platform abstraction structure
L6474x_Platform_t p;
p.malloc     = StepLibraryMalloc;
p.free       = StepLibraryFree;
p.transfer   = StepDriverSpiTransfer;
p.reset      = StepDriverReset;
p.sleep      = StepLibraryDelay;
p.stepAsync  = StepTimerAsync;
p.cancelStep = StepTimerCancelAsync;

// now create the handle
L6474_Handle_t h = L6474_CreateInstance(&p, null, null, null);
