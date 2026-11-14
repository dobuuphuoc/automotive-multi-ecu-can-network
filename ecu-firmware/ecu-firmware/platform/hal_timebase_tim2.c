/* HAL dùng TIM2 làm nền thời gian 1 kHz để SysTick dành riêng cho FreeRTOS. */
#include "stm32f1xx_hal.h"

TIM_HandleTypeDef g_htim_hal;

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    __HAL_RCC_TIM2_CLK_ENABLE();

    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    /* Timer trên APB1 chạy gấp đôi PCLK1 khi bộ chia APB1 khác 1 */
    uint32_t tclk  = ((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1) ? pclk1 : (2u * pclk1);

    g_htim_hal.Instance               = TIM2;
    g_htim_hal.Init.Prescaler         = (tclk / 1000000u) - 1u;   /* 1 MHz */
    g_htim_hal.Init.Period            = 1000u - 1u;               /* 1 kHz */
    g_htim_hal.Init.CounterMode       = TIM_COUNTERMODE_UP;
    g_htim_hal.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    g_htim_hal.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&g_htim_hal) != HAL_OK) { return HAL_ERROR; }

    HAL_NVIC_SetPriority(TIM2_IRQn, TickPriority, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    return HAL_TIM_Base_Start_IT(&g_htim_hal);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) { HAL_IncTick(); }
}
