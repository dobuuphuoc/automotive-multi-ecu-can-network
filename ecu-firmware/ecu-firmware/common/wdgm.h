/* ==============================================================================
 * wdgm.h — AUTOSAR Watchdog Manager (WdgM) & Alive Supervision
 * Giám sát sống các task định kỳ (20ms, 100ms, background).
 * Khi toàn bộ task sống khỏe mạnh -> nạp IWDG phần cứng.
 * Khi bất kỳ task nào bị deadlock/hang -> ngắt nạp để IWDG reset MCU (Thực nghiệm E3).
 * ============================================================================== */
#ifndef WDGM_H
#define WDGM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Số lượng Supervised Entities (SE) tối đa */
#define WDGM_MAX_ENTITIES 8u

/* Các Supervised Entity ID chuẩn cho kiến trúc firmware */
typedef enum {
    WDGM_SE_TASK_COM_RX      = 0,  /* Task thu CAN & giải mã */
    WDGM_SE_TASK_CONTROL     = 1,  /* Task điều khiển / xử lý logic */
    WDGM_SE_TASK_COM_TX_DIAG = 2,  /* Task phát CAN định kỳ & UDS DCM */
    WDGM_SE_TASK_APP         = 3   /* Task nền / ứng dụng */
} wdgm_entity_id_t;

/* Trạng thái giám sát tổng thể WdgM */
typedef enum {
    WDGM_STATUS_OK      = 0,  /* Mọi task đạt checkpoint đúng định mức */
    WDGM_STATUS_FAILED  = 1,  /* Có task vi phạm checkpoint trong chu kỳ */
    WDGM_STATUS_EXPIRED = 2   /* Quá hạn giám sát -> Dừng nạp IWDG để MCU reset */
} wdgm_status_t;

/* Cấu hình Alive Supervision cho 1 SE */
typedef struct {
    uint8_t min_alive;  /* Số lần tối thiểu phải chạm checkpoint trong 100ms */
    uint8_t max_alive;  /* Số lần tối đa được chạm checkpoint trong 100ms */
    bool    configured; /* Cờ đã kích hoạt giám sát */
} wdgm_entity_cfg_t;

/* Con trỏ hàm hook nạp watchdog phần cứng */
typedef void (*wdgm_feed_hook_fn_t)(void);

/* API WdgM */
void WdgM_Init(void);
bool WdgM_ConfigureEntity(uint8_t se_id, uint8_t min_alive, uint8_t max_alive);
void WdgM_CheckpointReached(uint8_t se_id);
void WdgM_MainFunction_100ms(void);
wdgm_status_t WdgM_GetGlobalStatus(void);
uint32_t WdgM_GetSuccessReloadCount(void);

void WdgM_SetFeedHook(wdgm_feed_hook_fn_t hook);

/* Hàm weak nạp IWDG vi điều khiển */
void wdgm_hal_feed_iwdg(void);

#ifdef __cplusplus
}
#endif

#endif /* WDGM_H */
