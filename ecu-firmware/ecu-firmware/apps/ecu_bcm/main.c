/* ==============================================================================
 * main.c — Body Control Module (ECU BCM) Firmware
 * FreeRTOS + HAL + bxCAN 500kbps + AUTOSAR RTE + SWC BCM + BSW (COM/E2E/DCM/DEM/WdgM)
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
#include "Rte_BCM.h"
#include "swc_bcm.h"

/* Kích thước bộ nhớ tĩnh FreeRTOS */
#define STACK_BLINK      128
#define STACK_CAN_RX     320
#define STACK_CAN_TX     320
#define STACK_SWC        320
#define STACK_WDGM       256

static StaticTask_t s_tcb_blink, s_tcb_rx, s_tcb_tx, s_tcb_swc, s_tcb_wdgm;
static StackType_t  s_stk_blink[STACK_BLINK], s_stk_rx[STACK_CAN_RX],
                    s_stk_tx[STACK_CAN_TX],   s_stk_swc[STACK_SWC],
                    s_stk_wdgm[STACK_WDGM];

#define RXQ_LEN 16
static StaticQueue_t s_rxq_cb;
static uint8_t       s_rxq_storage[RXQ_LEN * sizeof(can_frame_t)];
static QueueHandle_t s_rxq;

/* Trạng thái E2E RX & Timeout Monitors */
static e2e_rx_state_t   s_e2e_wheel_speed;
static e2e_rx_state_t   s_e2e_dms_alert;
static e2e_rx_state_t   s_e2e_bcm_cmd;
static com_rx_monitor_t s_mon_wheel_speed;
static com_rx_monitor_t s_mon_dms_alert;

/* Trạng thái E2E TX */
static e2e_tx_state_t   s_e2e_tx_bcm_status;

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

/* Khai báo hàm đồng bộ RTE từ rte.c */
void Rte_Sync_WheelSpeed_Rx(uint16_t speed_raw, uint8_t sig_status);
void Rte_Sync_DmsAlert_Rx(uint8_t alert_type, uint8_t severity, uint8_t sig_status);
void Rte_Sync_BcmCmd_Rx(const com_msg_bcm_cmd_t *cmd, uint8_t sig_status);
void Rte_Get_BcmStatus_Tx(com_msg_bcm_status_t *msg);

static void task_can_rx(void *arg)
{
    (void)arg;
    can_frame_t f;

    for (;;) {
        if (xQueueReceive(s_rxq, &f, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        /* 1. Kiểm tra chẩn đoán UDS (0x7DF hoặc 0x7E0) */
        uint32_t tx_id = 0;
        uint8_t tx_data[8];
        uint8_t tx_dlc = 0;
        if (Dcm_ProcessCanFrame(f.id, f.data, f.dlc, &tx_id, tx_data, &tx_dlc)) {
            can_driver_send(tx_id, tx_data, tx_dlc);
            WdgM_CheckpointReached(WDGM_SE_TASK_COM_RX);
            continue;
        }

        /* 2. Giải mã WHEEL_SPEED (0x101) */
        if (f.id == CAN_ID_WHEEL_SPEED) {
            com_msg_wheel_speed_t msg;
            e2e_status_t st = com_unpack_wheel_speed(&msg, f.data, f.dlc, &s_e2e_wheel_speed);
            if (st == E2E_STATUS_INITIAL || st == E2E_STATUS_OK || st == E2E_STATUS_OK_SOME_LOST) {
                com_rx_monitor_on_rx(&s_mon_wheel_speed, now_ms);
                Rte_Sync_WheelSpeed_Rx(msg.wheel_speed_raw, RTE_SIG_VALID);
            } else {
                Rte_Sync_WheelSpeed_Rx(0, RTE_SIG_INVALID);
                Dem_ReportErrorStatus(DEM_DTC_E2E_ERROR, DEM_EVENT_STATUS_FAILED);
            }
        }
        /* 3. Giải mã DMS_ALERT (0x110) */
        else if (f.id == CAN_ID_DMS_ALERT) {
            com_msg_dms_alert_t msg;
            e2e_status_t st = com_unpack_dms_alert(&msg, f.data, f.dlc, &s_e2e_dms_alert);
            if (st == E2E_STATUS_INITIAL || st == E2E_STATUS_OK || st == E2E_STATUS_OK_SOME_LOST) {
                com_rx_monitor_on_rx(&s_mon_dms_alert, now_ms);
                Rte_Sync_DmsAlert_Rx(msg.alert_type, msg.severity, RTE_SIG_VALID);
            } else {
                Rte_Sync_DmsAlert_Rx(0, 0, RTE_SIG_INVALID);
                Dem_ReportErrorStatus(DEM_DTC_E2E_ERROR, DEM_EVENT_STATUS_FAILED);
            }
        }
        /* 4. Giải mã BCM_CMD (0x106) */
        else if (f.id == CAN_ID_BCM_CMD) {
            com_msg_bcm_cmd_t msg;
            e2e_status_t st = com_unpack_bcm_cmd(&msg, f.data, f.dlc, &s_e2e_bcm_cmd);
            if (st == E2E_STATUS_INITIAL || st == E2E_STATUS_OK || st == E2E_STATUS_OK_SOME_LOST) {
                Rte_Sync_BcmCmd_Rx(&msg, RTE_SIG_VALID);
                BCM_CmdHandler_Runnable();
            } else {
                Dem_ReportErrorStatus(DEM_DTC_E2E_ERROR, DEM_EVENT_STATUS_FAILED);
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
        /* Gửi BCM_STATUS (0x103) định kỳ 100 ms */
        com_msg_bcm_status_t msg;
        Rte_Get_BcmStatus_Tx(&msg);

        uint8_t payload[8];
        uint8_t dlc = 0;
        if (com_pack_bcm_status(&msg, payload, &dlc, &s_e2e_tx_bcm_status)) {
            can_driver_send(CAN_ID_BCM_STATUS, payload, dlc);
        }

        WdgM_CheckpointReached(WDGM_SE_TASK_COM_TX_DIAG);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(100));
    }
}

static void task_swc_bcm(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        /* Kiểm tra giám sát thời hạn nhận (Deadline / Timeout) */
        if (com_rx_monitor_check_timeout(&s_mon_wheel_speed, now_ms, &s_e2e_wheel_speed)) {
            Rte_Sync_WheelSpeed_Rx(0, RTE_SIG_INVALID);
            Dem_ReportErrorStatus(DEM_DTC_CAN_TIMEOUT, DEM_EVENT_STATUS_FAILED);
        }
        if (com_rx_monitor_check_timeout(&s_mon_dms_alert, now_ms, &s_e2e_dms_alert)) {
            Rte_Sync_DmsAlert_Rx(0, 0, RTE_SIG_INVALID);
        }

        /* Chạy Runnable chính của BCM */
        BCM_100ms_Runnable();

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

    /* Khởi tạo các module BSW */
    Dem_Init();
    Dcm_Init(ECU_NODE_BCM);
    Dcm_SetFirmwareVersion(1, 0);

    e2e_rx_init(&s_e2e_wheel_speed);
    e2e_rx_init(&s_e2e_dms_alert);
    e2e_rx_init(&s_e2e_bcm_cmd);
    e2e_tx_init(&s_e2e_tx_bcm_status);

    com_rx_monitor_init(&s_mon_wheel_speed, COM_TIMEOUT_WHEEL_SPEED_MS);
    com_rx_monitor_init(&s_mon_dms_alert, COM_TIMEOUT_DMS_ALERT_BCM_MS);

    /* Khởi tạo WdgM Alive Supervision:
     * RX: 1..20, Control: 1..2, TX: 1..2 mỗi 100ms */
    WdgM_Init();
    WdgM_ConfigureEntity(WDGM_SE_TASK_COM_RX, 0, 50);
    WdgM_ConfigureEntity(WDGM_SE_TASK_CONTROL, 1, 2);
    WdgM_ConfigureEntity(WDGM_SE_TASK_COM_TX_DIAG, 1, 2);

    /* Khởi tạo RTE và SWC */
    BCM_Init_Runnable();

    /* Khởi tạo bxCAN */
    s_rxq = xQueueCreateStatic(RXQ_LEN, sizeof(can_frame_t), s_rxq_storage, &s_rxq_cb);
    if (!can_driver_init(s_rxq, false)) {
        Error_Handler();
    }

    /* Tạo các task FreeRTOS */
    xTaskCreateStatic(task_blink,    "blink",    STACK_BLINK,  NULL, 1, s_stk_blink,  &s_tcb_blink);
    xTaskCreateStatic(task_bsw_wdgm, "wdgm",     STACK_WDGM,   NULL, 2, s_stk_wdgm,   &s_tcb_wdgm);
    xTaskCreateStatic(task_can_tx,   "can_tx",   STACK_CAN_TX, NULL, 3, s_stk_tx,     &s_tcb_tx);
    xTaskCreateStatic(task_swc_bcm,  "swc_bcm",  STACK_SWC,    NULL, 4, s_stk_swc,    &s_tcb_swc);
    xTaskCreateStatic(task_can_rx,   "can_rx",   STACK_CAN_RX, NULL, 5, s_stk_rx,     &s_tcb_rx);

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
    o.PLL.PLLMUL     = RCC_PLL_MUL9; /* 8 MHz x 9 = 72 MHz */
    if (HAL_RCC_OscConfig(&o) != HAL_OK) { Error_Handler(); }

    c.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    c.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    c.APB1CLKDivider = RCC_HCLK_DIV2;
    c.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&c, FLASH_LATENCY_2) != HAL_OK) { Error_Handler(); }
}
