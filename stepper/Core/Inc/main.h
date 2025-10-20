/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define USR_BUTTON_Pin GPIO_PIN_13
#define USR_BUTTON_GPIO_Port GPIOC
#define SPINDLE_SI_R_Pin GPIO_PIN_0
#define SPINDLE_SI_R_GPIO_Port GPIOA
#define STEP_SPI_SCK_Pin GPIO_PIN_5
#define STEP_SPI_SCK_GPIO_Port GPIOA
#define STEP_SPI_MISO_Pin GPIO_PIN_6
#define STEP_SPI_MISO_GPIO_Port GPIOA
#define STEP_SPI_MOSI_Pin GPIO_PIN_7
#define STEP_SPI_MOSI_GPIO_Port GPIOA
#define LED_GREEN_Pin GPIO_PIN_0
#define LED_GREEN_GPIO_Port GPIOB
#define STEP_RSTN_Pin GPIO_PIN_12
#define STEP_RSTN_GPIO_Port GPIOF
#define STEP_DIR_Pin GPIO_PIN_13
#define STEP_DIR_GPIO_Port GPIOF
#define STEP_FLAG_Pin GPIO_PIN_15
#define STEP_FLAG_GPIO_Port GPIOF
#define SPINDLE_ENA_L_Pin GPIO_PIN_14
#define SPINDLE_ENA_L_GPIO_Port GPIOE
#define SPINDLE_ENA_R_Pin GPIO_PIN_15
#define SPINDLE_ENA_R_GPIO_Port GPIOE
#define SPINDLE_PWM_L_Pin GPIO_PIN_10
#define SPINDLE_PWM_L_GPIO_Port GPIOB
#define SPINDLE_PWM_R_Pin GPIO_PIN_11
#define SPINDLE_PWM_R_GPIO_Port GPIOB
#define LED_RED_Pin GPIO_PIN_14
#define LED_RED_GPIO_Port GPIOB
#define DEBUG_UART_TX_Pin GPIO_PIN_8
#define DEBUG_UART_TX_GPIO_Port GPIOD
#define DEBUG_UART_RX_Pin GPIO_PIN_9
#define DEBUG_UART_RX_GPIO_Port GPIOD
#define STEP_SPI_CS_Pin GPIO_PIN_14
#define STEP_SPI_CS_GPIO_Port GPIOD
#define STEP_PULSE_Pin GPIO_PIN_15
#define STEP_PULSE_GPIO_Port GPIOD
#define LED_BLUE_Pin GPIO_PIN_7
#define LED_BLUE_GPIO_Port GPIOB
#define REFERENCE_MARK_Pin GPIO_PIN_8
#define REFERENCE_MARK_GPIO_Port GPIOB
#define LIMIT_SWITCH_Pin GPIO_PIN_9
#define LIMIT_SWITCH_GPIO_Port GPIOB
#define SPINDLE_SI_L_Pin GPIO_PIN_0
#define SPINDLE_SI_L_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
