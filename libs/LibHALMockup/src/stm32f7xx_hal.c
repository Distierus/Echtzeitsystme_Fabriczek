#include "stm32f7xx_hal.h"
#include "main.h"
#include "Windows.h"

// USE THIS DEFINE TO DISABLE THE GATING SIMULATION AND INSTEAD USE
// THE PWM IRQS OF THE PWM GENERATOR DIRECTLY!!!
//#define USE_PWM_GENERATOR_DIRECTLY_INSTEAD_OF_GATING_SLAVE

#define STATUS_HIGHZ_MASK       ( 1 <<  0 )
#define STATUS_DIRECTION_MASK   ( 1 <<  4 )
#define STATUS_NOTPERF_CMD_MASK ( 1 <<  7 )
#define STATUS_WRONG_CMD_MASK   ( 1 <<  8 )
#define STATUS_UNDERVOLT_MASK   ( 1 <<  9 )
#define STATUS_THR_WARN_MASK    ( 1 << 10 )
#define STATUS_THR_SHORTD_MASK  ( 1 << 11 )
#define STATUS_OCD_MASK         ( 1 << 12 )

#define STEP_REG_RANGE_MASK   0x1F
#define STEP_REG_CMD_MASK     0xE0

#define STEP_CMD_NOP_PREFIX      0x00 //Nothing
#define STEP_CMD_NOP_LENGTH      0x01
#define STEP_CMD_SET_PREFIX      0x00 //Writes VALUE in PARAM register
#define STEP_CMD_SET_LENGTH      0x01
#define STEP_CMD_SET_MAX_PAYLOAD 0x04
#define STEP_CMD_GET_PREFIX      0x20 //Reads VALUE from PARAM register
#define STEP_CMD_GET_LENGTH      0x01
#define STEP_CMD_GET_MAX_PAYLOAD 0x04
#define STEP_CMD_ENA_PREFIX      0xB8 //Enable the power stage
#define STEP_CMD_ENA_LENGTH      0x01
#define STEP_CMD_DIS_PREFIX      0xA8 //Puts the bridges in High Impedance status immediately
#define STEP_CMD_DIS_LENGTH      0x01
#define STEP_CMD_STA_PREFIX      0xD0 //Returns the status register value
#define STEP_CMD_STA_LENGTH      0x03
#define STEP_REG_ABS_POS      0x01
#define STEP_LEN_ABS_POS      0x03
#define STEP_MASK_ABS_POS     0x3FFFFF
#define STEP_OFFSET_ABS_POS   0x0

#define STEP_REG_EL_POS       0x02
#define STEP_LEN_EL_POS       0x02
#define STEP_MASK_EL_POS      0x1FF
#define STEP_OFFSET_EL_POS    0x0

#define STEP_REG_MARK         0x03
#define STEP_LEN_MARK         0x03
#define STEP_MASK_MARK        0x3FFFFF
#define STEP_OFFSET_MARK      0x0

#define STEP_REG_TVAL         0x09
#define STEP_LEN_TVAL         0x01
#define STEP_MASK_TVAL        0x7F
#define STEP_OFFSET_TVAL      0x0

#define STEP_REG_T_FAST       0x0E
#define STEP_LEN_T_FAST       0x01
#define STEP_MASK_T_FAST      0xFF
#define STEP_OFFSET_T_FAST    0x0

#define STEP_REG_TON_MIN      0x0F
#define STEP_LEN_TON_MIN      0x01
#define STEP_MASK_TON_MIN     0x7F
#define STEP_OFFSET_TON_MIN   0x0

#define STEP_REG_TOFF_MIN     0x10
#define STEP_LEN_TOFF_MIN     0x01
#define STEP_MASK_TOFF_MIN    0x7F
#define STEP_OFFSET_TOFF_MIN  0x0

#define STEP_REG_ADC_OUT      0x12
#define STEP_LEN_ADC_OUT      0x01
#define STEP_MASK_ADC_OUT     0x1F
#define STEP_OFFSET_ADC_OUT   0x0

#define STEP_REG_OCD_TH       0x13
#define STEP_LEN_OCD_TH       0x01
#define STEP_MASK_OCD_TH      0x0F
#define STEP_OFFSET_OCD_TH    0x0

#define STEP_REG_STEP_MODE    0x16
#define STEP_LEN_STEP_MODE    0x01
#define STEP_MASK_STEP_MODE   0xFF
#define STEP_OFFSET_STEP_MODE 0x0

#define STEP_REG_ALARM_EN     0x17
#define STEP_LEN_ALARM_EN     0x01
#define STEP_MASK_ALARM_EN    0xFF
#define STEP_OFFSET_ALARM_EN  0x0

#define STEP_REG_CONFIG       0x18
#define STEP_LEN_CONFIG       0x02
#define STEP_MASK_CONFIG      0xFFFF
#define STEP_OFFSET_CONFIG    0x0

#define STEP_REG_STATUS       0x19
#define STEP_LEN_STATUS       0x02
#define STEP_MASK_STATUS      0xFFFF
#define STEP_OFFSET_STATUS    0x0

// --------------------------------------------------------------------------------------------------------------------
typedef enum L6474x_AccessFlags
// --------------------------------------------------------------------------------------------------------------------
{
	afNONE = 0x00,
	afREAD = 0x01,
	afWRITE = 0x02,
	afWRITE_HighZ = 0x04
} L6474x_AccessFlags_t;

// --------------------------------------------------------------------------------------------------------------------
typedef struct L6474x_ParameterDescriptor
// --------------------------------------------------------------------------------------------------------------------
{
	unsigned char        command;
	unsigned char        defined;
	unsigned char        length;
	unsigned int         mask;
	char*                address;
	L6474x_AccessFlags_t flags;
} L6474x_ParameterDescriptor_t;

// --------------------------------------------------------------------------------------------------------------------
static struct
{
	int initialized;
	int state;
	int regPtr;
	int pending;
	int pwmGeneratorStarted;
	HANDLE pwmGeneratorHandle;
	struct
	{
		uint16_t status;
		uint32_t abs_pos;
		uint32_t el_pos;
		uint32_t mark;
		uint8_t  ton;
		uint8_t  toff;
		uint8_t  tfast;
		uint8_t  tval;
		uint8_t  adc_out;
		uint8_t  ocd_th;
		uint8_t  step_mode;
		uint8_t  alarm;
		uint16_t config;
	} regs;
	struct
	{
		int pwm_step;
		int spi_cs;
		int led_green;
		int led_red;
		int led_blue;
		int step_rst;
		int step_dir;
		int spindle_ena_b;
		int spindle_ena_f;
		int spindle_pwm_b;
		int spindle_pwm_f;
	} pins;
} myConfig;

// --------------------------------------------------------------------------------------------------------------------
static const L6474x_ParameterDescriptor_t L6474_Parameters[STEP_REG_RANGE_MASK]
// --------------------------------------------------------------------------------------------------------------------
= {
	[STEP_REG_ABS_POS] = {.command = STEP_REG_ABS_POS,   .defined = 1, .length = STEP_LEN_ABS_POS,   .mask = STEP_MASK_ABS_POS,   .address = &myConfig.regs.abs_pos,   .flags = afREAD | afWRITE       },
	[STEP_REG_EL_POS] = {.command = STEP_REG_EL_POS,    .defined = 1, .length = STEP_LEN_EL_POS,    .mask = STEP_MASK_EL_POS,    .address = &myConfig.regs.el_pos,    .flags = afREAD | afWRITE       },
	[STEP_REG_MARK] = {.command = STEP_REG_MARK,      .defined = 1, .length = STEP_LEN_MARK,      .mask = STEP_MASK_MARK,      .address = &myConfig.regs.mark,      .flags = afREAD | afWRITE       },
	[STEP_REG_TVAL] = {.command = STEP_REG_TVAL,      .defined = 1, .length = STEP_LEN_TVAL,      .mask = STEP_MASK_TVAL,      .address = &myConfig.regs.tval,      .flags = afREAD | afWRITE       },
	[STEP_REG_T_FAST] = {.command = STEP_REG_T_FAST,    .defined = 1, .length = STEP_LEN_T_FAST,    .mask = STEP_MASK_T_FAST,    .address = &myConfig.regs.tfast,    .flags = afREAD | afWRITE_HighZ },
	[STEP_REG_TON_MIN] = {.command = STEP_REG_TON_MIN,   .defined = 1, .length = STEP_LEN_TON_MIN,   .mask = STEP_MASK_TON_MIN,   .address = &myConfig.regs.ton,   .flags = afREAD | afWRITE_HighZ },
	[STEP_REG_TOFF_MIN] = {.command = STEP_REG_TOFF_MIN,  .defined = 1, .length = STEP_LEN_TOFF_MIN,  .mask = STEP_MASK_TOFF_MIN,  .address = &myConfig.regs.toff,  .flags = afREAD | afWRITE_HighZ },
	[STEP_REG_ADC_OUT] = {.command = STEP_REG_ADC_OUT,   .defined = 1, .length = STEP_LEN_ADC_OUT,   .mask = STEP_MASK_ADC_OUT,   .address = &myConfig.regs.adc_out,   .flags = afREAD                 },
	[STEP_REG_OCD_TH] = {.command = STEP_REG_OCD_TH,    .defined = 1, .length = STEP_LEN_OCD_TH,    .mask = STEP_MASK_OCD_TH,    .address = &myConfig.regs.ocd_th,    .flags = afREAD | afWRITE       },
	[STEP_REG_STEP_MODE] = {.command = STEP_REG_STEP_MODE, .defined = 1, .length = STEP_LEN_STEP_MODE, .mask = STEP_MASK_STEP_MODE, .address = &myConfig.regs.step_mode, .flags = afREAD | afWRITE_HighZ },
	[STEP_REG_ALARM_EN] = {.command = STEP_REG_ALARM_EN,  .defined = 1, .length = STEP_LEN_ALARM_EN,  .mask = STEP_MASK_ALARM_EN,  .address = &myConfig.regs.alarm,  .flags = afREAD | afWRITE       },
	[STEP_REG_CONFIG] = {.command = STEP_REG_CONFIG,    .defined = 1, .length = STEP_LEN_CONFIG,    .mask = STEP_MASK_CONFIG,    .address = &myConfig.regs.config,    .flags = afREAD | afWRITE_HighZ },
	[STEP_REG_STATUS] = {.command = STEP_REG_STATUS,    .defined = 1, .length = STEP_LEN_STATUS,    .mask = STEP_MASK_STATUS,    .address = &myConfig.regs.status,    .flags = afREAD                 }
};

SPI_TypeDef __int_SPI1;
SPI_TypeDef __int_SPI2;
SPI_TypeDef __int_SPI3;
SPI_TypeDef __int_SPI4;
SPI_TypeDef __int_SPI5;
SPI_TypeDef __int_SPI6;

TIM_TypeDef __int_TIM1;
TIM_TypeDef __int_TIM2;
TIM_TypeDef __int_TIM3;
TIM_TypeDef __int_TIM4;
TIM_TypeDef __int_TIM5;
TIM_TypeDef __int_TIM6;
TIM_TypeDef __int_TIM7;
TIM_TypeDef __int_TIM8;
TIM_TypeDef __int_TIM9;
TIM_TypeDef __int_TIM10;
TIM_TypeDef __int_TIM11;
TIM_TypeDef __int_TIM12;
TIM_TypeDef __int_TIM13;
TIM_TypeDef __int_TIM14;

// --------------------------------------------------------------------------------------------------------------------
static uint8_t ExecDriverProcessorSPI(uint8_t input);
static uint8_t directionForward = 1;
static int32_t internalPosition = 0;


// --------------------------------------------------------------------------------------------------------------------
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
// --------------------------------------------------------------------------------------------------------------------
{
	if (GPIO_Pin == GPIO_PIN_15 && GPIOx == GPIOD) // stepper PWM pin
	{
		return myConfig.pins.pwm_step;
	}
	else if (GPIOx == GPIOC && GPIO_Pin == GPIO_PIN_13) // reference mark
	{
		return internalPosition >= 100;
	}
	else if (GPIO_Pin == GPIO_PIN_0 && GPIOx == GPIOB) // led green
	{
		return myConfig.pins.led_green;
	}
	else if (GPIO_Pin == GPIO_PIN_12 && GPIOx == GPIOF) // step reset
	{
		return myConfig.pins.step_rst;
	}
	else if (GPIO_Pin == GPIO_PIN_13 && GPIOx == GPIOF) // step dir
	{
		return myConfig.pins.step_dir;
	}
	else if (GPIO_Pin == GPIO_PIN_15 && GPIOx == GPIOF) // step flag
	{
		return GPIO_PIN_RESET;
	}
	else if (GPIO_Pin == GPIO_PIN_14 && GPIOx == GPIOE) // spindle ena back
	{
		return myConfig.pins.spindle_ena_b;
	}
	else if (GPIO_Pin == GPIO_PIN_15 && GPIOx == GPIOE) // spindle ena for
	{
		return myConfig.pins.spindle_ena_f;
	}
	else if (GPIO_Pin == GPIO_PIN_11 && GPIOx == GPIOB) // spindle pwm back
	{
		return myConfig.pins.spindle_pwm_b;
	}
	else if (GPIO_Pin == GPIO_PIN_10 && GPIOx == GPIOB) // spindle pwm for
	{
		return myConfig.pins.spindle_pwm_f;
	}
	else if (GPIO_Pin == GPIO_PIN_14 && GPIOx == GPIOB) // led red
	{
		return myConfig.pins.led_red;
	}
	else if (GPIO_Pin == GPIO_PIN_14 && GPIOx == GPIOD) // spi cs pin
	{
		return myConfig.pins.spi_cs;
	}
	else if (GPIO_Pin == GPIO_PIN_7 && GPIOx == GPIOB) // led blue
	{
		return myConfig.pins.led_blue;
	}
	

	return GPIO_PIN_RESET;
}

// --------------------------------------------------------------------------------------------------------------------
void HAL_GPIO_WritePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState)
// --------------------------------------------------------------------------------------------------------------------
{
	if (GPIO_Pin == GPIO_PIN_15 && GPIOx == GPIOD) // stepper PWM pin
	{
		if (myConfig.pins.pwm_step == 0 && PinState != 0)
		{
			if ((myConfig.regs.status & STATUS_HIGHZ_MASK) == 0)
			{
				if (directionForward) { internalPosition += 1; myConfig.regs.abs_pos += 1; }
				else { internalPosition -= 1; myConfig.regs.abs_pos -= 1; }
				if (internalPosition > 100) internalPosition = 100;
				if (internalPosition < -100) internalPosition = -100;
			}
		}
		myConfig.pins.pwm_step = !!PinState;
	}
	else if (GPIOx == GPIOC && GPIO_Pin == GPIO_PIN_13) // reference mark
	{
		return;
	}
	else if (GPIO_Pin == GPIO_PIN_0 && GPIOx == GPIOB) // led green
	{
		myConfig.pins.led_green = !!PinState;
	}	
	else if (GPIO_Pin == GPIO_PIN_12 && GPIOx == GPIOF) // step reset
	{
		// do some preinitialization
		myConfig.pins.step_rst = !!PinState;
		if (PinState == GPIO_PIN_RESET)
		{
			myConfig.initialized = 1;
			myConfig.state = 0;
			myConfig.regs.abs_pos = 0;
			myConfig.regs.el_pos = 0;
			myConfig.regs.mark = 0;
			myConfig.regs.status = STATUS_HIGHZ_MASK | STATUS_OCD_MASK | STATUS_THR_SHORTD_MASK | STATUS_THR_WARN_MASK | STATUS_UNDERVOLT_MASK;
		}
	}
	else if (GPIO_Pin == GPIO_PIN_13 && GPIOx == GPIOF) // step dir
	{
		myConfig.pins.step_dir = !!PinState;
		if (PinState == GPIO_PIN_SET) myConfig.regs.status |= STATUS_DIRECTION_MASK;
		else myConfig.regs.status &= ~STATUS_DIRECTION_MASK;

		if (PinState == GPIO_PIN_SET) directionForward = 1;
		else directionForward = 0;
	}
	else if (GPIO_Pin == GPIO_PIN_15 && GPIOx == GPIOF) // step flag
	{
		return;
	}
	else if (GPIO_Pin == GPIO_PIN_14 && GPIOx == GPIOE) // spindle ena back
	{
		myConfig.pins.spindle_ena_b = !!PinState;
	}
	else if (GPIO_Pin == GPIO_PIN_15 && GPIOx == GPIOE) // spindle ena for
	{
		myConfig.pins.spindle_ena_f = !!PinState;
	}
	else if (GPIO_Pin == GPIO_PIN_11 && GPIOx == GPIOB) // spindle pwm back
	{
		myConfig.pins.spindle_pwm_b = !!PinState;
	}
	else if (GPIO_Pin == GPIO_PIN_10 && GPIOx == GPIOB) // spindle pwm for
	{
		myConfig.pins.spindle_pwm_f = !!PinState;
	}
	else if (GPIO_Pin == GPIO_PIN_14 && GPIOx == GPIOB) // led red
	{
		myConfig.pins.led_red = !!PinState;
	}
	else if (GPIO_Pin == GPIO_PIN_14 && GPIOx == GPIOD) // spi cs pin
	{
		myConfig.pins.spi_cs = !!PinState;
	}
	else if (GPIO_Pin == GPIO_PIN_7 && GPIOx == GPIOB) // led blue
	{
		myConfig.pins.led_blue = !!PinState;
	}
}

// --------------------------------------------------------------------------------------------------------------------
void HAL_GPIO_TogglePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
// --------------------------------------------------------------------------------------------------------------------
{
	if (GPIO_Pin == GPIO_PIN_15 && GPIOx == GPIOD) // stepper PWM pin
	{
		int tmp = !myConfig.pins.pwm_step;
		if (myConfig.pins.pwm_step == 0 && tmp != 0)
		{
			if ((myConfig.regs.status & STATUS_HIGHZ_MASK) == 0)
			{
				if (directionForward) { internalPosition += 1; myConfig.regs.abs_pos += 1; }
				else { internalPosition -= 1; myConfig.regs.abs_pos -= 1; }
				if (internalPosition > 100) internalPosition = 100;
				if (internalPosition < -100) internalPosition = -100;
			}
		}
		myConfig.pins.pwm_step = tmp;
	}
	else if (GPIO_Pin == GPIO_PIN_14 && GPIOx == GPIOD) // spi cs pin
	{
		myConfig.pins.spi_cs = !myConfig.pins.spi_cs;
	}
	else if (GPIOx == GPIOC && GPIO_Pin == GPIO_PIN_13) // reference mark
	{
		return;
	}
	else if (GPIO_Pin == GPIO_PIN_0 && GPIOx == GPIOB) // led green
	{
		myConfig.pins.led_green = !myConfig.pins.led_green;
	}
	else if (GPIO_Pin == GPIO_PIN_12 && GPIOx == GPIOF) // step reset
	{
		// do some preinitialization
		myConfig.pins.step_rst = !myConfig.pins.step_rst;
		if (myConfig.pins.step_rst == GPIO_PIN_RESET)
		{
			myConfig.initialized = 1;
			myConfig.state = 0;
			myConfig.regs.abs_pos = 0;
			myConfig.regs.el_pos = 0;
			myConfig.regs.mark = 0;
			myConfig.regs.status = STATUS_HIGHZ_MASK | STATUS_OCD_MASK | STATUS_THR_SHORTD_MASK | STATUS_THR_WARN_MASK | STATUS_UNDERVOLT_MASK;
		}
	}
	else if (GPIO_Pin == GPIO_PIN_13 && GPIOx == GPIOF) // step dir
	{
		myConfig.pins.step_dir = !myConfig.pins.step_dir;
		if (myConfig.pins.step_dir == GPIO_PIN_SET) myConfig.regs.status |= STATUS_DIRECTION_MASK;
		else myConfig.regs.status &= ~STATUS_DIRECTION_MASK;

		if (myConfig.pins.step_dir == GPIO_PIN_SET) directionForward = 1;
		else directionForward = 0;
	}
	else if (GPIO_Pin == GPIO_PIN_15 && GPIOx == GPIOF) // step flag
	{
		return;
	}
	else if (GPIO_Pin == GPIO_PIN_14 && GPIOx == GPIOE) // spindle ena back
	{
		myConfig.pins.spindle_ena_b = !myConfig.pins.spindle_ena_b;
	}
	else if (GPIO_Pin == GPIO_PIN_15 && GPIOx == GPIOE) // spindle ena for
	{
		myConfig.pins.spindle_ena_f = !myConfig.pins.spindle_ena_f;
	}
	else if (GPIO_Pin == GPIO_PIN_11 && GPIOx == GPIOB) // spindle pwm back
	{
		myConfig.pins.spindle_pwm_b = !myConfig.pins.spindle_pwm_b;
	}
	else if (GPIO_Pin == GPIO_PIN_10 && GPIOx == GPIOB) // spindle pwm for
	{
		myConfig.pins.spindle_pwm_f = !myConfig.pins.spindle_pwm_f;
	}
	else if (GPIO_Pin == GPIO_PIN_14 && GPIOx == GPIOB) // led red
	{
		myConfig.pins.led_red = !myConfig.pins.led_red;
	}
	else if (GPIO_Pin == GPIO_PIN_14 && GPIOx == GPIOD) // spi cs pin
	{
		myConfig.pins.spi_cs = !myConfig.pins.spi_cs;
	}
	else if (GPIO_Pin == GPIO_PIN_7 && GPIOx == GPIOB) // led blue
	{
		myConfig.pins.led_blue = !myConfig.pins.led_blue;
	}

}


// --------------------------------------------------------------------------------------------------------------------
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef* hspi, uint8_t* pTxData, uint8_t* pRxData, uint16_t Size,
	uint32_t Timeout)
// --------------------------------------------------------------------------------------------------------------------
{
	if (hspi->Instance == SPI1)
	{
		// here we have to push the byte into the processor and then we have to write the response into tx byte
		*pRxData = ExecDriverProcessorSPI(*pTxData);
	}
	return HAL_OK;
}

#if !defined(USE_PWM_GENERATOR_DIRECTLY_INSTEAD_OF_GATING_SLAVE)
// --------------------------------------------------------------------------------------------------------------------
DWORD WINAPI ApplnMessageDispatcherThreadTIM1(LPVOID lpParameter)
// --------------------------------------------------------------------------------------------------------------------
{
	extern void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef * htim);
	extern TIM_HandleTypeDef htim1;

	Sleep(1000);
	TIM1->SR |= (1 << 2);
	HAL_TIM_PWM_PulseFinishedCallback(&htim1);

	if ((myConfig.regs.status & STATUS_HIGHZ_MASK) == 0)
	{
		if (directionForward)
		{
			internalPosition += ((int32_t)TIM1->ARR + 1);
			myConfig.regs.abs_pos += (TIM1->ARR + 1);
		}
		else
		{
			internalPosition -= (TIM1->ARR + 1);
			myConfig.regs.abs_pos -= (TIM1->ARR + 1);
		}

		if (internalPosition >  100) internalPosition =  100;
		if (internalPosition < -100) internalPosition = -100;
		TIM1->ARR = 0;
	}

	Sleep(100);
	TIM1->SR &= ~(1 << 2);
	HAL_TIM_PWM_PulseFinishedCallback(&htim1);

	return 0;
}
#endif

#if defined(USE_PWM_GENERATOR_DIRECTLY_INSTEAD_OF_GATING_SLAVE)
// --------------------------------------------------------------------------------------------------------------------
DWORD WINAPI ApplnMessageDispatcherThreadTIM4(LPVOID lpParameter)
// --------------------------------------------------------------------------------------------------------------------
{
	extern void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef * htim);
	extern TIM_HandleTypeDef htim4;

	while (myConfig.pwmGeneratorStarted)
	{
		Sleep(1);
		if (!myConfig.pwmGeneratorStarted) break;

		if ((myConfig.regs.status & STATUS_HIGHZ_MASK) == 0)
		{
			if (directionForward)
			{
				internalPosition += 1;
				myConfig.regs.abs_pos += 1;
			}
			else
			{
				internalPosition -= 1;
				myConfig.regs.abs_pos -= 1;
			}

			if (internalPosition > 100) internalPosition = 100;
			if (internalPosition < -100) internalPosition = -100;
		}
		HAL_TIM_PeriodElapsedCallback(&htim4);
	}
	return 0;
}
#endif

// --------------------------------------------------------------------------------------------------------------------
HAL_StatusTypeDef HAL_TIM_OnePulse_Start_IT(TIM_HandleTypeDef* htim, uint32_t OutputChannel)
// --------------------------------------------------------------------------------------------------------------------
{
#if !defined(USE_PWM_GENERATOR_DIRECTLY_INSTEAD_OF_GATING_SLAVE)
	if (htim->Instance == TIM1) // is the gating timer which enables the tim4 pwm generator
	{
		CreateThread(0, 0, ApplnMessageDispatcherThreadTIM1, NULL, 0, NULL);
	}
	return HAL_OK;
#endif
}

// --------------------------------------------------------------------------------------------------------------------
HAL_StatusTypeDef HAL_TIM_OnePulse_Stop_IT(TIM_HandleTypeDef* htim, uint32_t OutputChannel)
// --------------------------------------------------------------------------------------------------------------------
{
	return HAL_OK;
}

// --------------------------------------------------------------------------------------------------------------------
HAL_StatusTypeDef HAL_TIM_GenerateEvent(TIM_HandleTypeDef* htim, uint32_t EventSource)
// --------------------------------------------------------------------------------------------------------------------
{
	return HAL_OK;
}

// --------------------------------------------------------------------------------------------------------------------
HAL_StatusTypeDef HAL_TIM_OnePulse_Stop(TIM_HandleTypeDef* htim, uint32_t OutputChannel)
// --------------------------------------------------------------------------------------------------------------------
{
	return HAL_OK;
}

// --------------------------------------------------------------------------------------------------------------------
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef* htim, uint32_t Channel)
// --------------------------------------------------------------------------------------------------------------------
{
#if defined(USE_PWM_GENERATOR_DIRECTLY_INSTEAD_OF_GATING_SLAVE)
	if (htim->Instance == TIM4) // is the pwm generator
	{
		if (myConfig.pwmGeneratorStarted == 0)
		{
			// this blocks until the thread has exited
			if (myConfig.pwmGeneratorHandle) WaitForSingleObject(myConfig.pwmGeneratorHandle, INFINITE); 

			myConfig.pwmGeneratorStarted = 1;
			myConfig.pwmGeneratorHandle = CreateThread(0, 0, ApplnMessageDispatcherThreadTIM4, NULL, 0, NULL);
		}
	}
#endif
	return HAL_OK;
}

// --------------------------------------------------------------------------------------------------------------------
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef* htim, uint32_t Channel)
// --------------------------------------------------------------------------------------------------------------------
{
	myConfig.pwmGeneratorStarted = 0;
#if defined(USE_PWM_GENERATOR_DIRECTLY_INSTEAD_OF_GATING_SLAVE)
	Sleep(10);
	if (myConfig.pwmGeneratorHandle != NULL)
	{
		WaitForSingleObject(myConfig.pwmGeneratorHandle, INFINITE); // this blocks until the thread has exited
	}
#endif		
	myConfig.pwmGeneratorHandle = NULL;
	return HAL_OK;
}

// --------------------------------------------------------------------------------------------------------------------
HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef* htim, const TIM_OC_InitTypeDef* sConfig,uint32_t Channel)
// --------------------------------------------------------------------------------------------------------------------
{
	if (Channel == TIM_CHANNEL_1) htim->Instance->CCR1 = sConfig->Pulse;
	else if (Channel == TIM_CHANNEL_2) htim->Instance->CCR2 = sConfig->Pulse;
	else if (Channel == TIM_CHANNEL_3) htim->Instance->CCR3 = sConfig->Pulse;
	else if (Channel == TIM_CHANNEL_4) htim->Instance->CCR4 = sConfig->Pulse;
	else if (Channel == TIM_CHANNEL_5) htim->Instance->CCR5 = sConfig->Pulse;
	else if (Channel == TIM_CHANNEL_6) htim->Instance->CCR6 = sConfig->Pulse;
	return HAL_OK;
}

// --------------------------------------------------------------------------------------------------------------------
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef* htim)
// --------------------------------------------------------------------------------------------------------------------
{
	htim->Instance->ARR = htim->Init.Period;
	htim->Instance->PSC = htim->Init.Prescaler;
	return HAL_OK;
}


// --------------------------------------------------------------------------------------------------------------------
static uint8_t ExecDriverProcessorSPI(uint8_t input)
// --------------------------------------------------------------------------------------------------------------------
{
	if (myConfig.initialized == 0)
	{
		// do some preinitialization
		myConfig.initialized = 1;
		myConfig.state = 0;
		myConfig.regs.abs_pos = 0;
		myConfig.regs.el_pos = 0;
		myConfig.regs.mark = 0;
		myConfig.regs.status = STATUS_HIGHZ_MASK | STATUS_OCD_MASK | STATUS_THR_SHORTD_MASK | STATUS_THR_WARN_MASK | STATUS_UNDERVOLT_MASK;
	}

	uint8_t output = STEP_CMD_NOP_PREFIX;

	switch (myConfig.state)
	{
	case 0x00:
		if (input == STEP_CMD_STA_PREFIX)
			myConfig.state = 0x50;
		else if (input == STEP_CMD_ENA_PREFIX)
			myConfig.regs.status &= ~STATUS_HIGHZ_MASK;
		else if (input == STEP_CMD_DIS_PREFIX)
			myConfig.regs.status |= STATUS_HIGHZ_MASK;
		else if ((input & STEP_REG_CMD_MASK) == STEP_CMD_GET_PREFIX) {
			myConfig.regPtr = input & STEP_REG_RANGE_MASK;
			myConfig.pending = L6474_Parameters[myConfig.regPtr].length;
			myConfig.state = 0x20 + myConfig.pending - 1;
		}
		else if ((input & STEP_REG_CMD_MASK) == STEP_CMD_SET_PREFIX) {
			myConfig.regPtr = input & STEP_REG_RANGE_MASK;
			myConfig.pending = L6474_Parameters[myConfig.regPtr].length;
			myConfig.state = 0x10 + myConfig.pending - 1;
		}
		break;

    // part of register set
	case 0x10:
		if (((myConfig.regs.status & STATUS_HIGHZ_MASK) == 0) && (L6474_Parameters[myConfig.regPtr].flags & afWRITE_HighZ))
		{
			myConfig.regs.status |= STATUS_NOTPERF_CMD_MASK;
		}
		else
		{
			*(L6474_Parameters[myConfig.regPtr].address + 0) = input;
		}
		output = *(L6474_Parameters[myConfig.regPtr].address + 0);
		myConfig.state = 0x00;
		break;

	case 0x11:
		if (((myConfig.regs.status & STATUS_HIGHZ_MASK) == 0) && (L6474_Parameters[myConfig.regPtr].flags & afWRITE_HighZ))
		{
			myConfig.regs.status |= STATUS_NOTPERF_CMD_MASK;
		}
		else
		{
			*(L6474_Parameters[myConfig.regPtr].address + 1) = input;
		}
		output = *(L6474_Parameters[myConfig.regPtr].address + 1);
		myConfig.state = 0x10;
		break;

	case 0x12:
		if (((myConfig.regs.status & STATUS_HIGHZ_MASK) == 0) && (L6474_Parameters[myConfig.regPtr].flags & afWRITE_HighZ))
		{
			myConfig.regs.status |= STATUS_NOTPERF_CMD_MASK;
		}
		else
		{
			*(L6474_Parameters[myConfig.regPtr].address + 2) = input;
		}
		output = *(L6474_Parameters[myConfig.regPtr].address + 2);
		myConfig.state = 0x11;
		break;

	// part of register get
	case 0x20:
		output = *(L6474_Parameters[myConfig.regPtr].address + 0);
		myConfig.state = 0x00;
		break;

	case 0x21:
		output = *(L6474_Parameters[myConfig.regPtr].address + 1);
		myConfig.state = 0x20;
		break;

	case 0x22:
		output = *(L6474_Parameters[myConfig.regPtr].address + 2);
		myConfig.state = 0x21;
		break;

	// part of status get
	case 0x50:
		output = myConfig.regs.status >> 8;
		myConfig.state = 0x51;
		break;

	case 0x51:
		output = myConfig.regs.status;
		myConfig.state = 0x00;
		break;

	default:
		myConfig.state = 0x00;
		myConfig.regs.status |= STATUS_WRONG_CMD_MASK;
		break;
	}

	return output;
}