/* Bring-up Blue Pill: FreeRTOS + HAL + bxCAN 500 kbit/s.
 *   - task Blink : nháy LED PC13
 *   - task CanTx : gửi 0x6F0 mỗi 100 ms, có Alive_Counter + CRC8
 *   - task CanRx : nhận từ hàng đợi, kiểm CRC8 và Alive_Counter của 0x6F1 (DK1) / 0x6F0 (loopback)
 *   - task Status: in thống kê mỗi giây qua USART1 (PA9) */
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "board_pins.h"
#include "can_driver.h"
#include "e2e_crc8.h"
#include "log.h"
#include "bringup_config.h"

/* ---------- Bộ nhớ tĩnh của task và hàng đợi ---------- */
#define STACK_BLINK   128
#define STACK_CAN_TX  320
#define STACK_CAN_RX  320
#define STACK_STATUS  320

static StaticTask_t s_tcb_blink, s_tcb_tx, s_tcb_rx, s_tcb_status;
static StackType_t  s_stk_blink[STACK_BLINK], s_stk_tx[STACK_CAN_TX],
                    s_stk_rx[STACK_CAN_RX],   s_stk_status[STACK_STATUS];
static TaskHandle_t s_h_tx, s_h_rx, s_h_status;

#define RXQ_LEN 16
static StaticQueue_t s_rxq_cb;
static uint8_t       s_rxq_storage[RXQ_LEN * sizeof(can_frame_t)];
static QueueHandle_t s_rxq;

/* ---------- Thống kê E2E ---------- */
typedef struct { uint32_t ok, crc_err, alive_err, len_err; int16_t last_alive; } e2e_rx_stats_t;
static e2e_rx_stats_t s_e2e_dk1  = { .last_alive = -1 };
static e2e_rx_stats_t s_e2e_self = { .last_alive = -1 };
static uint32_t       s_rx_other;

static void SystemClock_Config(void);
static void Error_Handler(void) { for (;;) { } }

static void led_set(bool on)
{
    /* Blue Pill: LED nối lên 3V3, sáng khi chân ở mức thấp */
    HAL_GPIO_WritePin(BRINGUP_LED_PORT, BRINGUP_LED_PIN, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void task_blink(void *arg)
{
    (void)arg;
    for (;;) {
        led_set(true);  vTaskDelay(pdMS_TO_TICKS(100));
        led_set(false); vTaskDelay(pdMS_TO_TICKS(400));
    }
}

static void task_can_tx(void *arg)
{
    (void)arg;
    uint8_t  alive = 0;
    uint16_t seq   = 0;
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        uint8_t d[8];
        d[0] = alive & 0x0Fu;
        d[1] = 0x11; d[2] = 0x22;
        d[3] = (uint8_t)(seq & 0xFFu);
        d[4] = (uint8_t)(seq >> 8);
        d[5] = 0x55; d[6] = 0x66;
        d[7] = e2e_frame_crc(DATAID_BP, d, 7);
        (void)can_driver_send(ID_BP_TO_DK1, d, 8);
        alive = (uint8_t)((alive + 1u) & 0x0Fu);
        seq++;
        vTaskDelayUntil(&last, pdMS_TO_TICKS(100));
    }
}

static void check_e2e(e2e_rx_stats_t *st, uint8_t data_id, const can_frame_t *f)
{
    if (f->dlc != 8) { st->len_err++; return; }
    if (e2e_frame_crc(data_id, f->data, 7) != f->data[7]) { st->crc_err++; return; }
    int16_t alive = f->data[0] & 0x0F;
    if (st->last_alive >= 0 && alive != ((st->last_alive + 1) & 0x0F)) { st->alive_err++; }
    st->last_alive = alive;
    st->ok++;
}

static void task_can_rx(void *arg)
{
    (void)arg;
    can_frame_t f;
    for (;;) {
        if (xQueueReceive(s_rxq, &f, portMAX_DELAY) != pdTRUE) { continue; }
        if (f.id == ID_DK1_TO_BP)      { check_e2e(&s_e2e_dk1,  DATAID_DK1, &f); }
        else if (f.id == ID_BP_TO_DK1) { check_e2e(&s_e2e_self, DATAID_BP,  &f); }   /* chỉ thấy khi loopback */
        else                            { s_rx_other++; }
    }
}

static void task_status(void *arg)
{
    (void)arg;
    log_printf("\r\n=== Blue Pill CAN bring-up | SYSCLK %lu Hz | loopback=%d ===\r\n",
               (unsigned long)SystemCoreClock, BRINGUP_CAN_LOOPBACK_DEFAULT);
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(1000));
        can_stats_t s;
        can_driver_get_stats(&s);
        log_printf("TX ok=%lu fail=%lu | RX=%lu qfull=%lu | DK1 ok=%lu crc=%lu alive=%lu | self ok=%lu | TEC=%u REC=%u%s\r\n",
                   (unsigned long)s.tx_ok, (unsigned long)s.tx_fail,
                   (unsigned long)s.rx_frames, (unsigned long)s.rx_queue_full,
                   (unsigned long)s_e2e_dk1.ok, (unsigned long)s_e2e_dk1.crc_err,
                   (unsigned long)s_e2e_dk1.alive_err, (unsigned long)s_e2e_self.ok,
                   can_driver_tec(), can_driver_rec(), can_driver_bus_off() ? " BUS-OFF" : "");
        log_printf("   stack free (words): tx=%u rx=%u status=%u\r\n",
                   (unsigned)uxTaskGetStackHighWaterMark(s_h_tx),
                   (unsigned)uxTaskGetStackHighWaterMark(s_h_rx),
                   (unsigned)uxTaskGetStackHighWaterMark(s_h_status));
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* Giải phóng PA15/PB3/PB4 (giữ SWD) - bắt buộc cho board carrier */
    __HAL_RCC_AFIO_CLK_ENABLE();
    BOARD_DISABLE_JTAG();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = BRINGUP_LED_PIN; g.Mode = GPIO_MODE_OUTPUT_PP; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BRINGUP_LED_PORT, &g);
    led_set(false);

    log_init();

    s_rxq = xQueueCreateStatic(RXQ_LEN, sizeof(can_frame_t), s_rxq_storage, &s_rxq_cb);
    if (!can_driver_init(s_rxq, BRINGUP_CAN_LOOPBACK_DEFAULT != 0)) { Error_Handler(); }

    xTaskCreateStatic(task_blink,  "blink",  STACK_BLINK,  NULL, 1, s_stk_blink,  &s_tcb_blink);
    s_h_status = xTaskCreateStatic(task_status, "status", STACK_STATUS, NULL, 2, s_stk_status, &s_tcb_status);
    s_h_tx     = xTaskCreateStatic(task_can_tx, "can_tx", STACK_CAN_TX, NULL, 3, s_stk_tx,     &s_tcb_tx);
    s_h_rx     = xTaskCreateStatic(task_can_rx, "can_rx", STACK_CAN_RX, NULL, 4, s_stk_rx,     &s_tcb_rx);

    vTaskStartScheduler();
    Error_Handler();            /* không bao giờ tới đây */
    return 0;
}

/* HSE 8 MHz -> PLL x9 = 72 MHz; APB1 = 36 MHz (bxCAN), APB2 = 72 MHz */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState            = RCC_HSE_ON;
    osc.HSEPredivValue      = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL          = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) { Error_Handler(); }

    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) { Error_Handler(); }
}
