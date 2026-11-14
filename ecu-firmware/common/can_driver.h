/* Lớp CAN mỏng trên bxCAN (CAN1, PA11=RX, PA12=TX), 500 kbit/s, ID chuẩn 11 bit (CAN 2.0A).
 * Nhận bằng ngắt vào hàng đợi FreeRTOS. Gửi từ MỘT task (chưa có khóa). */
#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H
#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

typedef struct {
    uint32_t id;        /* ID chuẩn 11 bit */
    uint8_t  dlc;
    uint8_t  data[8];
} can_frame_t;

typedef struct {
    uint32_t tx_ok;
    uint32_t tx_fail;       /* hết mailbox hoặc lỗi HAL */
    uint32_t rx_frames;
    uint32_t rx_queue_full;
} can_stats_t;

/* rx_queue: hàng đợi chứa can_frame_t. loopback=true: tự thu frame của chính mình (không cần bus). */
bool can_driver_init(QueueHandle_t rx_queue, bool loopback);
bool can_driver_send(uint32_t id, const uint8_t *data, uint8_t dlc);
void can_driver_get_stats(can_stats_t *out);
const can_stats_t *can_driver_get_stats_ptr(void);
uint8_t can_driver_tec(void);
uint8_t can_driver_rec(void);
bool can_driver_bus_off(void);
void can_driver_irq(void);   /* gọi từ USB_LP_CAN1_RX0_IRQHandler */

#endif
