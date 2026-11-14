#include "log.h"
#include "board_pins.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef s_huart1;
static StaticSemaphore_t  s_mutex_buf;
static SemaphoreHandle_t  s_mutex;
static char               s_line[96];

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) { return; }
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin   = PIN_UART_TX_PIN;
    g.Mode  = GPIO_MODE_AF_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(PIN_UART_TX_PORT, &g);
    g.Pin  = PIN_UART_RX_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(PIN_UART_RX_PORT, &g);
}

void log_init(void)
{
    s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buf);
    s_huart1.Instance          = USART1;
    s_huart1.Init.BaudRate     = 115200;
    s_huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    s_huart1.Init.StopBits     = UART_STOPBITS_1;
    s_huart1.Init.Parity       = UART_PARITY_NONE;
    s_huart1.Init.Mode         = UART_MODE_TX_RX;
    s_huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    s_huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    (void)HAL_UART_Init(&s_huart1);
}

void log_printf(const char *fmt, ...)
{
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) != pdTRUE) { return; }
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(s_line, sizeof s_line, fmt, ap);
    va_end(ap);
    if (n > 0) {
        if ((size_t)n >= sizeof s_line) { n = (int)sizeof s_line - 1; }
        (void)HAL_UART_Transmit(&s_huart1, (uint8_t *)s_line, (uint16_t)n, 100);
    }
    xSemaphoreGive(s_mutex);
}
