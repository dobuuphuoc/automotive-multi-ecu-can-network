/* ==============================================================================
 * main.c — Climate Control (ECU Climate) Firmware
 * FreeRTOS + HAL + bxCAN 500kbps + AUTOSAR RTE + SWC Climate + BSW
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
#include "Rte_Climate.h"
#include "swc_climate.h"

#define STACK_BLINK      128
#define STACK_CAN_RX     320
#define STACK_CAN_TX     320
#define STACK_SWC        320
#define STACK_SENSOR     256
#define STACK_WDGM       256

static StaticTask_t s_tcb_blink, s_tcb_rx, s_tcb_tx, s_tcb_swc, s_tcb_sensor, s_tcb_wdgm;
static StackType_t  s_stk_blink[STACK_BLINK], s_stk_rx[STACK_CAN_RX],
                    s_stk_tx[STACK_CAN_TX],   s_stk_swc[STACK_SWC],
                    s_stk_sensor[STACK_SENSOR], s_stk_wdgm[STACK_WDGM];

#define RXQ_LEN 16
static StaticQueue_t s_rxq_cb;
static uint8_t       s_rxq_storage[RXQ_LEN * sizeof(can_frame_t)];
static QueueHandle_t s_rxq;

static e2e_rx_state_t   s_e2e_climate_cmd;
static e2e_rx_state_t   s_e2e_dms_alert;
static e2e_tx_state_t   s_e2e_tx_climate_status;

static void SystemClock_Config(void);
static void Error_Handler(void) { for (;;) { } }

/* Callbacks cho dịch vụ UDS 0x22 (ReadDataByIdentifier) */
static uint32_t get_uptime_ms(void)
{
    return HAL_GetTick();
}

static uint32_t get_tx_count(void)
{
    return can_driver_get_stats_ptr()->tx_ok;
}

static void get_can_errors(uint8_t *tec, uint8_t *rec)
{
    if (tec != NULL) {
        *tec = can_driver_tec();
    }
    if (rec != NULL) {
        *rec = can_driver_rec();
    }
    (void)(can_driver_bus_off() ? 1U : 0U);
}

void Rte_Get_ClimateStatus_Tx(com_msg_climate_status_t *msg);

static uint16_t get_primary_signal(void)
{
    com_msg_climate_status_t msg;
    Rte_Get_ClimateStatus_Tx(&msg);
    return (uint16_t)msg.cabin_temp_raw;
}

/* Phần cứng Watchdog (IWDG) kết nối WdgM */
#if !defined(CAN_LOOPBACK_MODE) || (CAN_LOOPBACK_MODE == 0)
static IWDG_HandleTypeDef s_hiwdg;

static void iwdg_init(void)
{
    s_hiwdg.Instance       = IWDG;
    s_hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
    s_hiwdg.Init.Reload    = 625;
    if (HAL_IWDG_Init(&s_hiwdg) != HAL_OK) {
        Error_Handler();
    }
}

void wdgm_hal_feed_iwdg(void)
{
    HAL_IWDG_Refresh(&s_hiwdg);
}
#else
static void iwdg_init(void) {}
void wdgm_hal_feed_iwdg(void) {}
#endif

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

void Rte_Sync_ClimateCmd_Rx(const com_msg_climate_cmd_t *cmd, uint8_t sig_status);
void Rte_Sync_DmsAlert_Rx(uint8_t alert_type, uint8_t severity, uint8_t sig_status);
void Rte_Get_ClimateStatus_Tx(com_msg_climate_status_t *msg);

static void task_can_rx(void *arg)
{
    (void)arg;
    can_frame_t f;

    for (;;) {
        if (xQueueReceive(s_rxq, &f, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /* 1. UDS 0x7DF / 0x7E2 */
        uint32_t tx_id = 0;
        uint8_t tx_data[8];
        uint8_t tx_dlc = 0;
        if (Dcm_ProcessCanFrame(f.id, f.data, f.dlc, &tx_id, tx_data, &tx_dlc)) {
            can_driver_send(tx_id, tx_data, tx_dlc);
            WdgM_CheckpointReached(WDGM_SE_TASK_COM_RX);
            continue;
        }

        /* 2. CLIMATE_CMD (0x105) */
        if (f.id == CAN_ID_CLIMATE_CMD) {
            com_msg_climate_cmd_t msg;
            e2e_status_t st = com_unpack_climate_cmd(&msg, f.data, f.dlc, &s_e2e_climate_cmd);
            if (st == E2E_STATUS_INITIAL || st == E2E_STATUS_OK || st == E2E_STATUS_OK_SOME_LOST) {
                Rte_Sync_ClimateCmd_Rx(&msg, RTE_SIG_VALID);
            } else {
                Dem_ReportErrorStatus(DEM_DTC_E2E_ERROR, DEM_EVENT_STATUS_FAILED);
            }
        }
        /* 3. DMS_ALERT (0x110) */
        else if (f.id == CAN_ID_DMS_ALERT) {
            com_msg_dms_alert_t msg;
            e2e_status_t st = com_unpack_dms_alert(&msg, f.data, f.dlc, &s_e2e_dms_alert);
            if (st == E2E_STATUS_INITIAL || st == E2E_STATUS_OK || st == E2E_STATUS_OK_SOME_LOST) {
                Rte_Sync_DmsAlert_Rx(msg.alert_type, msg.severity, RTE_SIG_VALID);
            }
        }

        WdgM_CheckpointReached(WDGM_SE_TASK_COM_RX);
    }
}

static void task_can_tx(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        /* Gửi CLIMATE_STATUS (0x102) định kỳ 100 ms */
        com_msg_climate_status_t msg;
        Rte_Get_ClimateStatus_Tx(&msg);

        uint8_t payload[8];
        uint8_t dlc = 0;
        if (com_pack_climate_status(&msg, payload, &dlc, &s_e2e_tx_climate_status)) {
            can_driver_send(CAN_ID_CLIMATE_STATUS, payload, dlc);
        }

        WdgM_CheckpointReached(WDGM_SE_TASK_COM_TX_DIAG);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(100));
    }
}

static void task_sensor_2s(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        Climate_SensorRead_2s_Runnable();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(2000));
    }
}

static void task_swc_climate(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        Climate_100ms_Runnable();
        WdgM_CheckpointReached(WDGM_SE_TASK_CONTROL);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(100));
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
    Dcm_Init(ECU_NODE_CLIMATE);
    Dcm_SetFirmwareVersion(1, 0);
    Dcm_RegisterUptimeGetter(get_uptime_ms);
    Dcm_RegisterTxCountGetter(get_tx_count);
    Dcm_RegisterCanErrorsGetter(get_can_errors);
    Dcm_RegisterPrimarySignalGetter(get_primary_signal);

    e2e_rx_init(&s_e2e_climate_cmd);
    e2e_rx_init(&s_e2e_dms_alert);
    e2e_tx_init(&s_e2e_tx_climate_status);

    WdgM_Init();
    WdgM_ConfigureEntity(WDGM_SE_TASK_COM_RX, 0, 50);
    WdgM_ConfigureEntity(WDGM_SE_TASK_CONTROL, 1, 2);
    WdgM_ConfigureEntity(WDGM_SE_TASK_COM_TX_DIAG, 1, 2);

    Climate_Init_Runnable();

    s_rxq = xQueueCreateStatic(RXQ_LEN, sizeof(can_frame_t), s_rxq_storage, &s_rxq_cb);
    if (!can_driver_init(s_rxq, false)) {
        Error_Handler();
    }

    xTaskCreateStatic(task_blink,       "blink",   STACK_BLINK,  NULL, 1, s_stk_blink,  &s_tcb_blink);
    xTaskCreateStatic(task_bsw_wdgm,    "wdgm",    STACK_WDGM,   NULL, 2, s_stk_wdgm,   &s_tcb_wdgm);
    xTaskCreateStatic(task_can_tx,      "can_tx",  STACK_CAN_TX, NULL, 3, s_stk_tx,     &s_tcb_tx);
    xTaskCreateStatic(task_swc_climate, "swc_cli", STACK_SWC,    NULL, 4, s_stk_swc,    &s_tcb_swc);
    xTaskCreateStatic(task_sensor_2s,   "dht_2s",  STACK_SENSOR, NULL, 4, s_stk_sensor, &s_tcb_sensor);
    xTaskCreateStatic(task_can_rx,      "can_rx",  STACK_CAN_RX, NULL, 5, s_stk_rx,     &s_tcb_rx);

    iwdg_init();

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
