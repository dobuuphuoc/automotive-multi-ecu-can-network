/* board_pins.h - gán chân board carrier 4 ECU (STM32F103C8T6, Blue Pill)
 * Nguồn: Pin-mapping-4ECU.xlsx / PCB-Requirements-4ECU.md. Đổi gán chân thì sửa cả hai. */
#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include "stm32f1xx_hal.h"

#define PIN_ADC_A_PORT GPIOA
#define PIN_ADC_A_PIN  GPIO_PIN_0   /* ADC1_IN0; AI */
#define PIN_ADC_B_PORT GPIOA
#define PIN_ADC_B_PIN  GPIO_PIN_1   /* ADC1_IN1; AI */
#define PIN_LED_FAULT_PORT GPIOA
#define PIN_LED_FAULT_PIN  GPIO_PIN_2   /* GPIO; DO */
#define PIN_USER_BTN_PORT GPIOA
#define PIN_USER_BTN_PIN  GPIO_PIN_3   /* GPIO; DI */
#define PIN_ECU_ID0_PORT GPIOA
#define PIN_ECU_ID0_PIN  GPIO_PIN_4   /* GPIO; DI */
#define PIN_ECU_ID1_PORT GPIOA
#define PIN_ECU_ID1_PIN  GPIO_PIN_5   /* GPIO; DI */
#define PIN_ENC_CAP_PORT GPIOA
#define PIN_ENC_CAP_PIN  GPIO_PIN_8   /* TIM1_CH1 (IC); DI */
#define PIN_UART_TX_PORT GPIOA
#define PIN_UART_TX_PIN  GPIO_PIN_9   /* USART1_TX; DO */
#define PIN_UART_RX_PORT GPIOA
#define PIN_UART_RX_PIN  GPIO_PIN_10   /* USART1_RX; DI */
#define PIN_CAN_RX_PORT GPIOA
#define PIN_CAN_RX_PIN  GPIO_PIN_11   /* CAN_RX (bxCAN); DI */
#define PIN_CAN_TX_PORT GPIOA
#define PIN_CAN_TX_PIN  GPIO_PIN_12   /* CAN_TX (bxCAN); DO */
#define PIN_DO_4_PORT GPIOA
#define PIN_DO_4_PIN  GPIO_PIN_15   /* GPIO; DO */
#define PIN_PWM_H1_PORT GPIOB
#define PIN_PWM_H1_PIN  GPIO_PIN_0   /* TIM3_CH3; PWM */
#define PIN_PWM_H2_PORT GPIOB
#define PIN_PWM_H2_PIN  GPIO_PIN_1   /* TIM3_CH4; PWM */
#define PIN_CAN_STBY_PORT GPIOB
#define PIN_CAN_STBY_PIN  GPIO_PIN_4   /* GPIO; DO */
#define PIN_PWM_L1_PORT GPIOB
#define PIN_PWM_L1_PIN  GPIO_PIN_6   /* TIM4_CH1; PWM */
#define PIN_PWM_L2_PORT GPIOB
#define PIN_PWM_L2_PIN  GPIO_PIN_7   /* TIM4_CH2; PWM */
#define PIN_DO_3_PORT GPIOB
#define PIN_DO_3_PIN  GPIO_PIN_8   /* GPIO; DO */
#define PIN_DHT_DATA_PORT GPIOB
#define PIN_DHT_DATA_PIN  GPIO_PIN_9   /* GPIO (1-wire); IO */
#define PIN_DO_1_PORT GPIOB
#define PIN_DO_1_PIN  GPIO_PIN_10   /* GPIO; DO */
#define PIN_DO_2_PORT GPIOB
#define PIN_DO_2_PIN  GPIO_PIN_11   /* GPIO; DO */
#define PIN_DI_1_PORT GPIOB
#define PIN_DI_1_PIN  GPIO_PIN_12   /* GPIO; DI */
#define PIN_DI_2_PORT GPIOB
#define PIN_DI_2_PIN  GPIO_PIN_13   /* GPIO; DI */
#define PIN_DI_3_PORT GPIOB
#define PIN_DI_3_PIN  GPIO_PIN_14   /* GPIO; DI */
#define PIN_DI_4_PORT GPIOB
#define PIN_DI_4_PIN  GPIO_PIN_15   /* GPIO; DI */

/* Bắt buộc trong main(): tắt JTAG để dùng PA15, PB3, PB4 (giữ SWD) */
#define BOARD_DISABLE_JTAG()  __HAL_AFIO_REMAP_SWJ_NOJTAG()

/* Timer */
#define TIM_ENCODER   TIM1   /* PA8 CH1 input capture */
#define TIM_PWM_FAST  TIM3   /* PB0 CH3, PB1 CH4 */
#define TIM_PWM_SLOW  TIM4   /* PB6 CH1, PB7 CH2 */
#define TIM_HAL_BASE  TIM2   /* HAL timebase, SysTick dành cho FreeRTOS */

#endif
