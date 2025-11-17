#include "Stepper_implementation/my_stepper.h"
#include "Controller.h"
#include "LibL6474.h"
#include "LibL6474Config.h"
#include "FreeRTOSConfig.h"
#include "main.h"
#include "stdint.h"
#include "stdlib.h"

// in main.c definiert
extern SPI_HandleTypeDef hspi1;

void reset_stepper(void)
{
	L6474_BaseParameter_t base_parameter;
	L6474_SetBaseParameter(&base_parameter);

	// from LibL6474 library documentation:
	// pass all function pointers required by the stepper library
	// to a separate platform abstraction structure
	L6474x_Platform_t p;
	p.malloc     = StepLibraryMalloc;
	p.free       = StepLibraryFree;
	p.transfer   = StepDriverSpiTransfer;
	p.reset      = StepDriverReset;
	p.sleep      = StepLibraryDelay;

	// auskommentiert, da Flag in LibL6474Config.h Header gesetzt wurde
	// p.stepAsync  = StepTimerAsync;
	// p.cancelStep = StepTimerCancelAsync;


	// now create the handle
	L6474_Handle_t h = L6474_CreateInstance(&p, NULL, NULL, NULL);
}

// from LibL6474 library documentation
static void* StepLibraryMalloc( unsigned int size )
{
	// size	number of bytes requested by the memory allocation request
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
	// siehe stm32f7xx_hal_def.h Z. 38
	HAL_StatusTypeDef errorcode = HAL_OK;

	// bringe Chips Select low to transfer data
	HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 0);
	for ( unsigned int i = 0; i < length; i++ )
	{
		// TransmitReceive, damit weder Transmit Byte noch Receive Byte verworfen wird
		// 1 Byte versenden und 1 Byte empfangen, HAL_MAX_DELAY aus stm32f7xx_hal_def.h Z. 61
		errorcode = HAL_SPI_TransmitReceive(&hspi1, (uint8_t*) pTX, (uint8_t*) pRX, 1, HAL_MAX_DELAY);
		if (errorcode != HAL_OK)
		{
			return -1;
		}

	}
	// bringe Chips Select hight to end data transfer
	HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 1);

	return 0;
}

static void StepDriverReset(void *pGPO, const int ena)
{
	// the reset function is used to provide gpio access to the reset of the stepper driver chip.
	// the chip has a reset not pin and so the ena signal must be inverted to set the correct reset level physically
	HAL_GPIO_WritePin(STEP_RSTN_GPIO_Port, STEP_RSTN_Pin, !ena);
}

static void StepLibraryDelay(unsigned int ms)
{

}

static int StepTimerAsync(void *pPWM, int dir, unsigned int numPulses, void(*doneClb)(L6474_Handle_t), L6474_Handle_t h)
{
	// TODO for Async Movement

	// return 0 just for debugging
	return 0;
}

static int StepTimerCancelAsync(void *pPWM)
{
	// TODO for Async Movement

	// return 0 just for debugging
	return 0;
}
