
#ifndef STM32F7XX_HAL_H_
#define STM32F7XX_HAL_H_ STM32F7XX_HAL_H_

#include "stdint.h"

#define __IO volatile

  /** @defgroup GPIO_pins_define GPIO pins define
	* @{
	*/
#define GPIO_PIN_0                 ((uint16_t)0x0001U)  /* Pin 0 selected    */
#define GPIO_PIN_1                 ((uint16_t)0x0002U)  /* Pin 1 selected    */
#define GPIO_PIN_2                 ((uint16_t)0x0004U)  /* Pin 2 selected    */
#define GPIO_PIN_3                 ((uint16_t)0x0008U)  /* Pin 3 selected    */
#define GPIO_PIN_4                 ((uint16_t)0x0010U)  /* Pin 4 selected    */
#define GPIO_PIN_5                 ((uint16_t)0x0020U)  /* Pin 5 selected    */
#define GPIO_PIN_6                 ((uint16_t)0x0040U)  /* Pin 6 selected    */
#define GPIO_PIN_7                 ((uint16_t)0x0080U)  /* Pin 7 selected    */
#define GPIO_PIN_8                 ((uint16_t)0x0100U)  /* Pin 8 selected    */
#define GPIO_PIN_9                 ((uint16_t)0x0200U)  /* Pin 9 selected    */
#define GPIO_PIN_10                ((uint16_t)0x0400U)  /* Pin 10 selected   */
#define GPIO_PIN_11                ((uint16_t)0x0800U)  /* Pin 11 selected   */
#define GPIO_PIN_12                ((uint16_t)0x1000U)  /* Pin 12 selected   */
#define GPIO_PIN_13                ((uint16_t)0x2000U)  /* Pin 13 selected   */
#define GPIO_PIN_14                ((uint16_t)0x4000U)  /* Pin 14 selected   */
#define GPIO_PIN_15                ((uint16_t)0x8000U)  /* Pin 15 selected   */
#define GPIO_PIN_All               ((uint16_t)0xFFFFU)  /* All pins selected */

#define GPIO_PIN_MASK              ((uint32_t)0x0000FFFFU) /* PIN mask for assert test */
	/**
	  * @}
	  */

/** @defgroup TIM_Channel TIM Channel
  * @{
  */
#define TIM_CHANNEL_1                      0x00000000U                          /*!< Capture/compare channel 1 identifier      */
#define TIM_CHANNEL_2                      0x00000004U                          /*!< Capture/compare channel 2 identifier      */
#define TIM_CHANNEL_3                      0x00000008U                          /*!< Capture/compare channel 3 identifier      */
#define TIM_CHANNEL_4                      0x0000000CU                          /*!< Capture/compare channel 4 identifier      */
#define TIM_CHANNEL_5                      0x00000010U                          /*!< Compare channel 5 identifier              */
#define TIM_CHANNEL_6                      0x00000014U                          /*!< Compare channel 6 identifier              */
#define TIM_CHANNEL_ALL                    0x0000003CU                          /*!< Global Capture/compare channel identifier  */
  /**
	* @}
	*/


	/**
	  * @brief  Set the TIM Autoreload Register value on runtime without calling another time any Init function.
	  * @param  __HANDLE__ TIM handle.
	  * @param  __AUTORELOAD__ specifies the Counter register new value.
	  * @retval None
	  */
#define __HAL_TIM_SET_AUTORELOAD(__HANDLE__, __AUTORELOAD__) \
  do{                                                    \
    (__HANDLE__)->Instance->ARR = (__AUTORELOAD__);  \
    (__HANDLE__)->Init.Period = (__AUTORELOAD__);    \
  } while(0)

	  /**
		* @brief  Set the TIM Prescaler on runtime.
		* @param  __HANDLE__ TIM handle.
		* @param  __PRESC__ specifies the Prescaler new value.
		* @retval None
		*/
#define __HAL_TIM_SET_PRESCALER(__HANDLE__, __PRESC__)       ((__HANDLE__)->Instance->PSC = (__PRESC__))

#define __HAL_TIM_ENABLE(__HANDLE__)                 ((__HANDLE__)->Instance->CR1|=(1))

/**
  * @brief  HAL Status structures definition
  */
typedef enum
{
	HAL_OK = 0x00U,
	HAL_ERROR = 0x01U,
	HAL_BUSY = 0x02U,
	HAL_TIMEOUT = 0x03U
} HAL_StatusTypeDef;

/**
  * @brief  HAL Lock structures definition
  */
typedef enum
{
	HAL_UNLOCKED = 0x00U,
	HAL_LOCKED = 0x01U
} HAL_LockTypeDef;

/**
  * @brief  GPIO Bit SET and Bit RESET enumeration
  */
typedef enum
{
	GPIO_PIN_RESET = 0,
	GPIO_PIN_SET
} GPIO_PinState;

/**
  * @brief  HAL Active channel structures definition
  */
typedef enum
{
	HAL_TIM_ACTIVE_CHANNEL_1 = 0x01U,    /*!< The active channel is 1     */
	HAL_TIM_ACTIVE_CHANNEL_2 = 0x02U,    /*!< The active channel is 2     */
	HAL_TIM_ACTIVE_CHANNEL_3 = 0x04U,    /*!< The active channel is 3     */
	HAL_TIM_ACTIVE_CHANNEL_4 = 0x08U,    /*!< The active channel is 4     */
	HAL_TIM_ACTIVE_CHANNEL_5 = 0x10U,    /*!< The active channel is 5     */
	HAL_TIM_ACTIVE_CHANNEL_6 = 0x20U,    /*!< The active channel is 6     */
	HAL_TIM_ACTIVE_CHANNEL_CLEARED = 0x00U     /*!< All active channels cleared */
} HAL_TIM_ActiveChannel;

/**
  * @brief  HAL State structures definition
  */
typedef enum
{
	HAL_TIM_STATE_RESET = 0x00U,    /*!< Peripheral not yet initialized or disabled  */
	HAL_TIM_STATE_READY = 0x01U,    /*!< Peripheral Initialized and ready for use    */
	HAL_TIM_STATE_BUSY = 0x02U,    /*!< An internal process is ongoing              */
	HAL_TIM_STATE_TIMEOUT = 0x03U,    /*!< Timeout state                               */
	HAL_TIM_STATE_ERROR = 0x04U     /*!< Reception process is ongoing                */
} HAL_TIM_StateTypeDef;

/**
  * @brief  TIM Channel States definition
  */
typedef enum
{
	HAL_TIM_CHANNEL_STATE_RESET = 0x00U,    /*!< TIM Channel initial state                         */
	HAL_TIM_CHANNEL_STATE_READY = 0x01U,    /*!< TIM Channel ready for use                         */
	HAL_TIM_CHANNEL_STATE_BUSY = 0x02U,    /*!< An internal process is ongoing on the TIM channel */
} HAL_TIM_ChannelStateTypeDef;


/** @defgroup TIM_Counter_Mode TIM Counter Mode
  * @{
  */
#define TIM_COUNTERMODE_UP                 0x00000000U                          /*!< Counter used as up-counter   */
#define TIM_COUNTERMODE_DOWN               0x00000001U                          /*!< Counter used as down-counter */
#define TIM_COUNTERMODE_CENTERALIGNED1     0x00000002U                        /*!< Center-aligned mode 1        */
#define TIM_COUNTERMODE_CENTERALIGNED2     0x00000004U                        /*!< Center-aligned mode 2        */
#define TIM_COUNTERMODE_CENTERALIGNED3     0x00000008U                          /*!< Center-aligned mode 3        */
  /**
	* @}
	*/

	/** @defgroup TIM_ClockDivision TIM Clock Division
	  * @{
	  */
#define TIM_CLOCKDIVISION_DIV1             0x00000000U                          /*!< Clock division: tDTS=tCK_INT   */
#define TIM_CLOCKDIVISION_DIV2             0x00000001U                        /*!< Clock division: tDTS=2*tCK_INT */
#define TIM_CLOCKDIVISION_DIV4             0x00000002U                        /*!< Clock division: tDTS=4*tCK_INT */
	  /**
		* @}
		*/


		/** @defgroup TIM_AutoReloadPreload TIM Auto-Reload Preload
		  * @{
		  */
#define TIM_AUTORELOAD_PRELOAD_DISABLE                0x00000000U               /*!< TIMx_ARR register is not buffered */
#define TIM_AUTORELOAD_PRELOAD_ENABLE                 0x00000001U              /*!< TIMx_ARR register is buffered */


/**
  * @brief  TIM Output Compare Configuration Structure definition
  */
typedef struct
{
	uint32_t OCMode;        /*!< Specifies the TIM mode.
								 This parameter can be a value of @ref TIM_Output_Compare_and_PWM_modes */

	uint32_t Pulse;         /*!< Specifies the pulse value to be loaded into the Capture Compare Register.
								 This parameter can be a number between Min_Data = 0x0000 and Max_Data = 0xFFFF */

	uint32_t OCPolarity;    /*!< Specifies the output polarity.
								 This parameter can be a value of @ref TIM_Output_Compare_Polarity */

	uint32_t OCNPolarity;   /*!< Specifies the complementary output polarity.
								 This parameter can be a value of @ref TIM_Output_Compare_N_Polarity
								 @note This parameter is valid only for timer instances supporting break feature. */

	uint32_t OCFastMode;    /*!< Specifies the Fast mode state.
								 This parameter can be a value of @ref TIM_Output_Fast_State
								 @note This parameter is valid only in PWM1 and PWM2 mode. */


	uint32_t OCIdleState;   /*!< Specifies the TIM Output Compare pin state during Idle state.
								 This parameter can be a value of @ref TIM_Output_Compare_Idle_State
								 @note This parameter is valid only for timer instances supporting break feature. */

	uint32_t OCNIdleState;  /*!< Specifies the TIM Output Compare pin state during Idle state.
								 This parameter can be a value of @ref TIM_Output_Compare_N_Idle_State
								 @note This parameter is valid only for timer instances supporting break feature. */
} TIM_OC_InitTypeDef;

/**
  * @brief Serial Peripheral Interface
  */
typedef struct
{
	__IO uint32_t CR1;        /*!< SPI control register 1 (not used in I2S mode),      Address offset: 0x00 */
	__IO uint32_t CR2;        /*!< SPI control register 2,                             Address offset: 0x04 */
	__IO uint32_t SR;         /*!< SPI status register,                                Address offset: 0x08 */
	__IO uint32_t DR;         /*!< SPI data register,                                  Address offset: 0x0C */
	__IO uint32_t CRCPR;      /*!< SPI CRC polynomial register (not used in I2S mode), Address offset: 0x10 */
	__IO uint32_t RXCRCR;     /*!< SPI RX CRC register (not used in I2S mode),         Address offset: 0x14 */
	__IO uint32_t TXCRCR;     /*!< SPI TX CRC register (not used in I2S mode),         Address offset: 0x18 */
	__IO uint32_t I2SCFGR;    /*!< SPI_I2S configuration register,                     Address offset: 0x1C */
	__IO uint32_t I2SPR;      /*!< SPI_I2S prescaler register,                         Address offset: 0x20 */
} SPI_TypeDef;


/**
  * @brief General Purpose I/O
  */
typedef struct
{
	__IO uint32_t MODER;    /*!< GPIO port mode register,               Address offset: 0x00      */
	__IO uint32_t OTYPER;   /*!< GPIO port output type register,        Address offset: 0x04      */
	__IO uint32_t OSPEEDR;  /*!< GPIO port output speed register,       Address offset: 0x08      */
	__IO uint32_t PUPDR;    /*!< GPIO port pull-up/pull-down register,  Address offset: 0x0C      */
	__IO uint32_t IDR;      /*!< GPIO port input data register,         Address offset: 0x10      */
	__IO uint32_t ODR;      /*!< GPIO port output data register,        Address offset: 0x14      */
	__IO uint32_t BSRR;     /*!< GPIO port bit set/reset register,      Address offset: 0x18      */
	__IO uint32_t LCKR;     /*!< GPIO port configuration lock register, Address offset: 0x1C      */
	__IO uint32_t AFR[2];   /*!< GPIO alternate function registers,     Address offset: 0x20-0x24 */
} GPIO_TypeDef;


/** @defgroup TIM_Output_Compare_and_PWM_modes TIM Output Compare and PWM Modes
  * @{
  */
#define TIM_OCMODE_TIMING                   0x00000000U                                              /*!< Frozen                                 */
#define TIM_OCMODE_ACTIVE                   0x00000001U /*!< Set channel to active level on match   */
#define TIM_OCMODE_INACTIVE                 0x00000002U /*!< Set channel to inactive level on match */
#define TIM_OCMODE_TOGGLE                   0x00000003U /*!< Toggle                                 */
#define TIM_OCMODE_PWM1                     0x00000004U /*!< PWM mode 1                             */
#define TIM_OCMODE_PWM2                     0x00000005U /*!< PWM mode 2                             */
#define TIM_OCMODE_FORCED_ACTIVE            0x00000006U /*!< Force active level                     */
#define TIM_OCMODE_FORCED_INACTIVE          0x00000007U /*!< Force inactive level                   */
#define TIM_OCMODE_RETRIGERRABLE_OPM1       0x00000008U /*!< Retrigerrable OPM mode 1               */
#define TIM_OCMODE_RETRIGERRABLE_OPM2       0x00000009U /*!< Retrigerrable OPM mode 2               */
#define TIM_OCMODE_COMBINED_PWM1            0x0000000AU /*!< Combined PWM mode 1                    */
#define TIM_OCMODE_COMBINED_PWM2            0x0000000BU /*!< Combined PWM mode 2                    */
#define TIM_OCMODE_ASYMMETRIC_PWM1          0x0000000CU /*!< Asymmetric PWM mode 1                  */
#define TIM_OCMODE_ASYMMETRIC_PWM2          0x0000000DU /*!< Asymmetric PWM mode 2                  */
  /**
	* @}
	*/

	/** @defgroup TIM_Event_Source TIM Event Source
	  * @{
	  */
#define TIM_EVENTSOURCE_UPDATE              0x00000000U     /*!< Reinitialize the counter and generates an update of the registers */
#define TIM_EVENTSOURCE_CC1                 0x00000001U   /*!< A capture/compare event is generated on channel 1 */
#define TIM_EVENTSOURCE_CC2                 0x00000002U   /*!< A capture/compare event is generated on channel 2 */
#define TIM_EVENTSOURCE_CC3                 0x00000003U   /*!< A capture/compare event is generated on channel 3 */
#define TIM_EVENTSOURCE_CC4                 0x00000004U   /*!< A capture/compare event is generated on channel 4 */
#define TIM_EVENTSOURCE_COM                 0x00000005U   /*!< A commutation event is generated */
#define TIM_EVENTSOURCE_TRIGGER             0x00000006U     /*!< A trigger event is generated */
#define TIM_EVENTSOURCE_BREAK               0x00000007U     /*!< A break event is generated */
#define TIM_EVENTSOURCE_BREAK2              0x00000008U    /*!< A break 2 event is generated */
	  /**
		* @}
		*/


	/** @defgroup TIM_Output_Fast_State TIM Output Fast State
	  * @{
	  */
#define TIM_OCFAST_DISABLE                 0x00000000U                          /*!< Output Compare fast disable */
#define TIM_OCFAST_ENABLE                  0x00000001U                          /*!< Output Compare fast enable  */
	  /**
		* @}
		*/

		/** @defgroup TIM_Output_Compare_N_State TIM Complementary Output Compare State
		  * @{
		  */
#define TIM_OUTPUTNSTATE_DISABLE           0x00000000U                          /*!< OCxN is disabled  */
#define TIM_OUTPUTNSTATE_ENABLE            0x00000001U                          /*!< OCxN is enabled   */
		  /**
			* @}
			*/

			/** @defgroup TIM_Output_Compare_Polarity TIM Output Compare Polarity
			  * @{
			  */
#define TIM_OCPOLARITY_HIGH                0x00000000U                          /*!< Capture/Compare output polarity  */
#define TIM_OCPOLARITY_LOW                 0x00000001U                          /*!< Capture/Compare output polarity  */
			  /**
				* @}
				*/

				/** @defgroup TIM_Output_Compare_N_Polarity TIM Complementary Output Compare Polarity
				  * @{
				  */
#define TIM_OCNPOLARITY_HIGH               0x00000000U                          /*!< Capture/Compare complementary output polarity */
#define TIM_OCNPOLARITY_LOW                0x00000001U                          /*!< Capture/Compare complementary output polarity */
				  /**
					* @}
					*/

/**
  * @brief TIM
  */
typedef struct
{
	__IO uint32_t CR1;         /*!< TIM control register 1,              Address offset: 0x00 */
	__IO uint32_t CR2;         /*!< TIM control register 2,              Address offset: 0x04 */
	__IO uint32_t SMCR;        /*!< TIM slave mode control register,     Address offset: 0x08 */
	__IO uint32_t DIER;        /*!< TIM DMA/interrupt enable register,   Address offset: 0x0C */
	__IO uint32_t SR;          /*!< TIM status register,                 Address offset: 0x10 */
	__IO uint32_t EGR;         /*!< TIM event generation register,       Address offset: 0x14 */
	__IO uint32_t CCMR1;       /*!< TIM capture/compare mode register 1, Address offset: 0x18 */
	__IO uint32_t CCMR2;       /*!< TIM capture/compare mode register 2, Address offset: 0x1C */
	__IO uint32_t CCER;        /*!< TIM capture/compare enable register, Address offset: 0x20 */
	__IO uint32_t CNT;         /*!< TIM counter register,                Address offset: 0x24 */
	__IO uint32_t PSC;         /*!< TIM prescaler,                       Address offset: 0x28 */
	__IO uint32_t ARR;         /*!< TIM auto-reload register,            Address offset: 0x2C */
	__IO uint32_t RCR;         /*!< TIM repetition counter register,     Address offset: 0x30 */
	__IO uint32_t CCR1;        /*!< TIM capture/compare register 1,      Address offset: 0x34 */
	__IO uint32_t CCR2;        /*!< TIM capture/compare register 2,      Address offset: 0x38 */
	__IO uint32_t CCR3;        /*!< TIM capture/compare register 3,      Address offset: 0x3C */
	__IO uint32_t CCR4;        /*!< TIM capture/compare register 4,      Address offset: 0x40 */
	__IO uint32_t BDTR;        /*!< TIM break and dead-time register,    Address offset: 0x44 */
	__IO uint32_t DCR;         /*!< TIM DMA control register,            Address offset: 0x48 */
	__IO uint32_t DMAR;        /*!< TIM DMA address for full transfer,   Address offset: 0x4C */
	__IO uint32_t OR;          /*!< TIM option register,                 Address offset: 0x50 */
	__IO uint32_t CCMR3;       /*!< TIM capture/compare mode register 3,      Address offset: 0x54 */
	__IO uint32_t CCR5;        /*!< TIM capture/compare mode register5,       Address offset: 0x58 */
	__IO uint32_t CCR6;        /*!< TIM capture/compare mode register6,       Address offset: 0x5C */
} TIM_TypeDef;

/**
  * @brief  HAL SPI State structure definition
  */
typedef enum
{
	HAL_SPI_STATE_RESET = 0x00U,    /*!< Peripheral not Initialized                         */
	HAL_SPI_STATE_READY = 0x01U,    /*!< Peripheral Initialized and ready for use           */
	HAL_SPI_STATE_BUSY = 0x02U,    /*!< an internal process is ongoing                     */
	HAL_SPI_STATE_BUSY_TX = 0x03U,    /*!< Data Transmission process is ongoing               */
	HAL_SPI_STATE_BUSY_RX = 0x04U,    /*!< Data Reception process is ongoing                  */
	HAL_SPI_STATE_BUSY_TX_RX = 0x05U,    /*!< Data Transmission and Reception process is ongoing */
	HAL_SPI_STATE_ERROR = 0x06U,    /*!< SPI error state                                    */
	HAL_SPI_STATE_ABORT = 0x07U     /*!< SPI abort is ongoing                               */
} HAL_SPI_StateTypeDef;

/**
  * @brief  DMA handle Structure definition
  */
typedef struct __DMA_HandleTypeDef
{
	void* Instance;
}DMA_HandleTypeDef;

/**
  * @brief  SPI Configuration Structure definition
  */
typedef struct
{
	uint32_t Mode;                /*!< Specifies the SPI operating mode.
									   This parameter can be a value of @ref SPI_Mode */

	uint32_t Direction;           /*!< Specifies the SPI bidirectional mode state.
									   This parameter can be a value of @ref SPI_Direction */

	uint32_t DataSize;            /*!< Specifies the SPI data size.
									   This parameter can be a value of @ref SPI_Data_Size */

	uint32_t CLKPolarity;         /*!< Specifies the serial clock steady state.
									   This parameter can be a value of @ref SPI_Clock_Polarity */

	uint32_t CLKPhase;            /*!< Specifies the clock active edge for the bit capture.
									   This parameter can be a value of @ref SPI_Clock_Phase */

	uint32_t NSS;                 /*!< Specifies whether the NSS signal is managed by
									   hardware (NSS pin) or by software using the SSI bit.
									   This parameter can be a value of @ref SPI_Slave_Select_management */

	uint32_t BaudRatePrescaler;   /*!< Specifies the Baud Rate prescaler value which will be
									   used to configure the transmit and receive SCK clock.
									   This parameter can be a value of @ref SPI_BaudRate_Prescaler
									   @note The communication clock is derived from the master
									   clock. The slave clock does not need to be set. */

	uint32_t FirstBit;            /*!< Specifies whether data transfers start from MSB or LSB bit.
									   This parameter can be a value of @ref SPI_MSB_LSB_transmission */

	uint32_t TIMode;              /*!< Specifies if the TI mode is enabled or not.
									   This parameter can be a value of @ref SPI_TI_mode */

	uint32_t CRCCalculation;      /*!< Specifies if the CRC calculation is enabled or not.
									   This parameter can be a value of @ref SPI_CRC_Calculation */

	uint32_t CRCPolynomial;       /*!< Specifies the polynomial used for the CRC calculation.
									   This parameter must be an odd number between Min_Data = 1 and Max_Data = 65535 */

	uint32_t CRCLength;           /*!< Specifies the CRC Length used for the CRC calculation.
									   CRC Length is only used with Data8 and Data16, not other data size
									   This parameter can be a value of @ref SPI_CRC_length */

	uint32_t NSSPMode;            /*!< Specifies whether the NSSP signal is enabled or not .
									   This parameter can be a value of @ref SPI_NSSP_Mode
									   This mode is activated by the NSSP bit in the SPIx_CR2 register and
									   it takes effect only if the SPI interface is configured as Motorola SPI
									   master (FRF=0) with capture on the first edge (SPIx_CR1 CPHA = 0,
									   CPOL setting is ignored).. */
} SPI_InitTypeDef;

/**
  * @brief  TIM Time base Configuration Structure definition
  */
typedef struct
{
	uint32_t Prescaler;         /*!< Specifies the prescaler value used to divide the TIM clock.
									 This parameter can be a number between Min_Data = 0x0000 and Max_Data = 0xFFFF */

	uint32_t CounterMode;       /*!< Specifies the counter mode.
									 This parameter can be a value of @ref TIM_Counter_Mode */

	uint32_t Period;            /*!< Specifies the period value to be loaded into the active
									 Auto-Reload Register at the next update event.
									 This parameter can be a number between Min_Data = 0x0000 and Max_Data = 0xFFFF.  */

	uint32_t ClockDivision;     /*!< Specifies the clock division.
									 This parameter can be a value of @ref TIM_ClockDivision */

	uint32_t RepetitionCounter;  /*!< Specifies the repetition counter value. Each time the RCR downcounter
									  reaches zero, an update event is generated and counting restarts
									  from the RCR value (N).
									  This means in PWM mode that (N+1) corresponds to:
										  - the number of PWM periods in edge-aligned mode
										  - the number of half PWM period in center-aligned mode
									   GP timers: this parameter must be a number between Min_Data = 0x00 and
									   Max_Data = 0xFF.
									   Advanced timers: this parameter must be a number between Min_Data = 0x0000 and
									   Max_Data = 0xFFFF. */

	uint32_t AutoReloadPreload;  /*!< Specifies the auto-reload preload.
									 This parameter can be a value of @ref TIM_AutoReloadPreload */
} TIM_Base_InitTypeDef;

/**
  * @brief  SPI handle Structure definition
  */
typedef struct __SPI_HandleTypeDef
{
	SPI_TypeDef* Instance;      /*!< SPI registers base address               */

	SPI_InitTypeDef            Init;           /*!< SPI communication parameters             */

	uint8_t* pTxBuffPtr;    /*!< Pointer to SPI Tx transfer Buffer        */

	uint16_t                   TxXferSize;     /*!< SPI Tx Transfer size                     */

	__IO uint16_t              TxXferCount;    /*!< SPI Tx Transfer Counter                  */

	uint8_t* pRxBuffPtr;    /*!< Pointer to SPI Rx transfer Buffer        */

	uint16_t                   RxXferSize;     /*!< SPI Rx Transfer size                     */

	__IO uint16_t              RxXferCount;    /*!< SPI Rx Transfer Counter                  */

	uint32_t                   CRCSize;        /*!< SPI CRC size used for the transfer       */

	void (*RxISR)(struct __SPI_HandleTypeDef* hspi);   /*!< function pointer on Rx ISR       */

	void (*TxISR)(struct __SPI_HandleTypeDef* hspi);   /*!< function pointer on Tx ISR       */

	DMA_HandleTypeDef* hdmatx;        /*!< SPI Tx DMA Handle parameters             */

	DMA_HandleTypeDef* hdmarx;        /*!< SPI Rx DMA Handle parameters             */

	HAL_LockTypeDef            Lock;           /*!< Locking object                           */

	__IO HAL_SPI_StateTypeDef  State;          /*!< SPI communication state                  */

	__IO uint32_t              ErrorCode;      /*!< SPI Error code                           */
} SPI_HandleTypeDef;

/**
  * @brief  TIM Time Base Handle Structure definition
  */
typedef struct
{
	TIM_TypeDef* Instance;         /*!< Register base address                             */
	TIM_Base_InitTypeDef               Init;              /*!< TIM Time Base required parameters                 */
	HAL_TIM_ActiveChannel              Channel;           /*!< Active channel                                    */
	DMA_HandleTypeDef* hdma[7];          /*!< DMA Handlers array
															   This array is accessed by a @ref DMA_Handle_index */
	HAL_LockTypeDef                    Lock;              /*!< Locking object                                    */
	__IO HAL_TIM_StateTypeDef          State;             /*!< TIM operation state                               */
	__IO HAL_TIM_ChannelStateTypeDef   ChannelState[6];   /*!< TIM channel operation state                       */
	__IO HAL_TIM_ChannelStateTypeDef   ChannelNState[4];  /*!< TIM complementary channel operation state         */
} TIM_HandleTypeDef;





extern SPI_TypeDef __int_SPI1;
extern SPI_TypeDef __int_SPI2;
extern SPI_TypeDef __int_SPI3;
extern SPI_TypeDef __int_SPI4;
extern SPI_TypeDef __int_SPI5;
extern SPI_TypeDef __int_SPI6;

extern TIM_TypeDef __int_TIM1;
extern TIM_TypeDef __int_TIM2;
extern TIM_TypeDef __int_TIM3;
extern TIM_TypeDef __int_TIM4;
extern TIM_TypeDef __int_TIM5;
extern TIM_TypeDef __int_TIM6;
extern TIM_TypeDef __int_TIM7;
extern TIM_TypeDef __int_TIM8;
extern TIM_TypeDef __int_TIM9;
extern TIM_TypeDef __int_TIM10;
extern TIM_TypeDef __int_TIM11;
extern TIM_TypeDef __int_TIM12;
extern TIM_TypeDef __int_TIM13;
extern TIM_TypeDef __int_TIM14;



/** @addtogroup Peripheral_declaration
  * @{
  */
#define TIM2                ((TIM_TypeDef *) &__int_TIM2)
#define TIM3                ((TIM_TypeDef *) &__int_TIM3)
#define TIM4                ((TIM_TypeDef *) &__int_TIM4)
#define TIM5                ((TIM_TypeDef *) &__int_TIM5)
#define TIM6                ((TIM_TypeDef *) &__int_TIM6)
#define TIM7                ((TIM_TypeDef *) &__int_TIM7)
#define TIM12               ((TIM_TypeDef *) &__int_TIM12)
#define TIM13               ((TIM_TypeDef *) &__int_TIM13)
#define TIM14               ((TIM_TypeDef *) &__int_TIM14)
#define SPI2                ((SPI_TypeDef *) &__int_SPI2)
#define SPI3                ((SPI_TypeDef *) &__int_SPI3)
#define TIM1                ((TIM_TypeDef *) &__int_TIM1)
#define TIM8                ((TIM_TypeDef *) &__int_TIM8)
#define SPI1                ((SPI_TypeDef *) &__int_SPI1)
#define SPI4                ((SPI_TypeDef *) &__int_SPI4)
#define TIM9                ((TIM_TypeDef *) &__int_TIM9)
#define TIM10               ((TIM_TypeDef *) &__int_TIM10)
#define TIM11               ((TIM_TypeDef *) &__int_TIM11)
#define SPI5                ((SPI_TypeDef *) &__int_SPI5)
#define SPI6                ((SPI_TypeDef *) &__int_SPI6)
#define GPIOA               ((GPIO_TypeDef *) 0x01)
#define GPIOB               ((GPIO_TypeDef *) 0x02)
#define GPIOC               ((GPIO_TypeDef *) 0x03)
#define GPIOD               ((GPIO_TypeDef *) 0x04)
#define GPIOE               ((GPIO_TypeDef *) 0x05)
#define GPIOF               ((GPIO_TypeDef *) 0x06)
#define GPIOG               ((GPIO_TypeDef *) 0x07)
#define GPIOH               ((GPIO_TypeDef *) 0x08)
#define GPIOI               ((GPIO_TypeDef *) 0x09)
#define GPIOJ               ((GPIO_TypeDef *) 0x0A)
#define GPIOK               ((GPIO_TypeDef *) 0x0B)



GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
void HAL_GPIO_WritePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);
void HAL_GPIO_TogglePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);

HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef* hspi, uint8_t* pTxData, uint8_t* pRxData, uint16_t Size,
	uint32_t Timeout);

HAL_StatusTypeDef HAL_TIM_OnePulse_Start_IT(TIM_HandleTypeDef* htim, uint32_t OutputChannel);
HAL_StatusTypeDef HAL_TIM_OnePulse_Stop_IT(TIM_HandleTypeDef* htim, uint32_t OutputChannel);
HAL_StatusTypeDef HAL_TIM_GenerateEvent(TIM_HandleTypeDef* htim, uint32_t EventSource);
HAL_StatusTypeDef HAL_TIM_OnePulse_Stop(TIM_HandleTypeDef* htim, uint32_t OutputChannel);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef* htim, uint32_t Channel);
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef* htim, uint32_t Channel);
HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef* htim, const TIM_OC_InitTypeDef* sConfig, uint32_t Channel);
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef* htim);

#endif /* STM32F7XX_HAL_H_ */



