/* Các handler ngắt của ứng dụng. SVC/PendSV/SysTick do FreeRTOS cung cấp (xem FreeRTOSConfig.h). */
#include "stm32f1xx_hal.h"
#include "can_driver.h"

extern TIM_HandleTypeDef g_htim_hal;        /* TIM2: nền thời gian của HAL */

void USB_LP_CAN1_RX0_IRQHandler(void) { can_driver_irq(); }
void TIM2_IRQHandler(void)            { HAL_TIM_IRQHandler(&g_htim_hal); }

void HardFault_Handler(void) { for (;;) { } }
