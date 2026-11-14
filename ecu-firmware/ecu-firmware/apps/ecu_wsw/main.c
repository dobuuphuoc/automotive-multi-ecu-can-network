/* ==============================================================================
 * main.c — Wheel Slip Warning (ECU WSW) Firmware
 * FreeRTOS + HAL + bxCAN 500kbps + AUTOSAR RTE + SWC WSW + BSW
 * ============================================================================== */
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "board_pins.h"
#include "can_driver.h"
#include "can_com.h"
#include "e2e_crc8.h"
#include "diag_dem.h"
#include "diag_dcm.h"
#include "wdgm.h"
#include "log.h"
#include "Rte_WSW.h"
#include "swc_wsw.h"

#define STACK_BLINK      128
#define STACK_CAN_RX     320
#define STACK_CAN_TX_20  320
#define STACK_WSW_20     320
#define STACK_WDGM       256

static StaticTask_t s_tcb_blink, s_tcb_rx, s_tcb_tx, s_tcb_wsw, s_tcb_wdgm;
static StackType_t  s_stk_blink[STACK_BLINK], s_stk_rx[STACK_CAN_RX],
                    s_stk_tx[STACK_CAN_TX_20], s_stk_wsw[STACK_WSW_20],
                    s_stk_wdgm[STACK_WDGM];

#define RXQ_LEN 16
static StaticQueue_t s_rxq_cb;
static uint8_t       s_rxq_storage[RXQ_LEN * sizeof(can_frame_t)];
static QueueHandle_t s_rxq;

static e2e_tx_state_t s_e2e_tx_wheel_speed;
static e2e_tx_state_t s_e2e_tx_wsw_warning;

static void SystemClock_Config(void);
static void Error_Handler(void) { for (;;) { } }

static void led_set(bool on)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void task_blink(void *arg)
{
    (void)arg;
    for (;;) {
        led_set(true);  vTaskDelay(pdMS_TO_TICKS(100));
        led_set(false); vTaskDelay(pdMS_TO_TICKS(400));
    }
}

void Rte_Get_WheelSpeed_Tx(com_msg_wheel_speed_t *msg);
void Rte_Get_WswWarning_Tx(com_msg_wsw_warning_t *msg);

static void task_can_rx(void *arg)
{
    (void)arg;
    can_frame_t f;

    for (;;) {
        if (xQueueReceive(s_rxq, &f, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /* 1. UDS 0x7DF / 0x7E3 */
        uint32_t tx_id = 0;
        uint8_t tx_data[8];
        uint8_t tx_dlc = 0;
        if (Dcm_ProcessCanFrame(f.id, f.data, f.dlc, &tx_id, tx_data, &tx_dlc)) {
            can_driver_send(tx_id, tx_data, tx_dlc);
        }

        WdgM_CheckpointReached(WDGM_SE_TASK_COM_RX);
    }
}

static void task_can_tx_20ms(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    uint8_t counter_100ms = 0;

    for (;;) {
        /* Gửi WHEEL_SPEED (0x101) chu kỳ 20 ms */
        com_msg_wheel_speed_t spd_msg;
        Rte_Get_WheelSpeed_Tx(&spd_msg);

        uint8_t payload[8];
        uint8_t dlc = 0;
        if (com_pack_wheel_speed(&spd_msg, payload, &dlc, &s_e2e_tx_wheel_speed)) {
            can_driver_send(CAN_ID_WHEEL_SPEED, payload, dlc);
        }

        /* Gửi WSW_WARNING (0x100) khi active hoặc lặp mỗi 100 ms */
        if (++counter_100ms >= 5) {
            counter_100ms = 0;
            com_msg_wsw_warning_t warn_msg;
            Rte_Get_WswWarning_Tx(&warn_msg);

            if (warn_msg.slip_warning_active) {
                if (com_pack_wsw_warning(&warn_msg, payload, &dlc, &s_e2e_tx_wsw_warning)) {
                    can_driver_send(CAN_ID_WSW_WARNING, payload, dlc);
                }
            }
        }

        /* 20ms task chạy 5 lần trong 100ms */
        WdgM_CheckpointReached(WDGM_SE_TASK_COM_TX_DIAG);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(20));
    }
}

static void task_wsw_20ms(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        WSW_20ms_Capture_Runnable();
        WSW_SlipCalc_Runnable();

        /* 20ms task chạy 5 lần trong 100ms */
        WdgM_CheckpointReached(WDGM_SE_TASK_CONTROL);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(20));
    }
}

static void task_bsw_wdgm(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        WdgM_MainFunction_100ms();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(100));
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    __HAL_RCC_AFIO_CLK_ENABLE();
    BOARD_DISABLE_JTAG();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_13;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &g);
    led_set(false);

    log_init();

    Dem_Init();
    Dcm_Init(ECU_NODE_WSW);
    Dcm_SetFirmwareVersion(1, 0);

    e2e_tx_init(&s_e2e_tx_wheel_speed);
    e2e_tx_init(&s_e2e_tx_wsw_warning);

    WdgM_Init();
    WdgM_ConfigureEntity(WDGM_SE_TASK_COM_RX, 0, 50);
    WdgM_ConfigureEntity(WDGM_SE_TASK_CONTROL, 4, 6);     /* 20ms -> 5 lần/100ms */
    WdgM_ConfigureEntity(WDGM_SE_TASK_COM_TX_DIAG, 4, 6); /* 20ms -> 5 lần/100ms */

    WSW_Init_Runnable();

    s_rxq = xQueueCreateStatic(RXQ_LEN, sizeof(can_frame_t), s_rxq_storage, &s_rxq_cb);
    if (!can_driver_init(s_rxq, false)) {
        Error_Handler();
    }

    xTaskCreateStatic(task_blink,        "blink",     STACK_BLINK,     NULL, 1, s_stk_blink,  &s_tcb_blink);
    xTaskCreateStatic(task_bsw_wdgm,     "wdgm",      STACK_WDGM,      NULL, 2, s_stk_wdgm,   &s_tcb_wdgm);
    xTaskCreateStatic(task_can_tx_20ms,  "can_tx_20", STACK_CAN_TX_20, NULL, 3, s_stk_tx,     &s_tcb_tx);
    xTaskCreateStatic(task_wsw_20ms,     "wsw_20",    STACK_WSW_20,    NULL, 4, s_stk_wsw,    &s_tcb_wsw);
    xTaskCreateStatic(task_can_rx,       "can_rx",    STACK_CAN_RX,    NULL, 5, s_stk_rx,     &s_tcb_rx);

    vTaskStartScheduler();
    Error_Handler();
    return 0;
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef o = {0};
    RCC_ClkInitTypeDef c = {0};

    o.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    o.HSEState       = RCC_HSE_ON;
    o.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    o.PLL.PLLState   = RCC_PLL_ON;
    o.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    o.PLL.PLLMUL     = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&o) != HAL_OK) { Error_Handler(); }

    c.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    c.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    c.APB1CLKDivider = RCC_HCLK_DIV2;
    c.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&c, FLASH_LATENCY_2) != HAL_OK) { Error_Handler(); }
}
