/* ==============================================================================
 * diag_dem.h — AUTOSAR DEM (Diagnostic Event Manager)
 * 3-byte DTCs, 3-strike debounce, status byte & Freeze Frame RAM snapshot.
 * Khớp chuẩn can_matrix.dbc và doc/CAN-byte-layout.md Mục 7.
 * ============================================================================== */
#ifndef DIAG_DEM_H
#define DIAG_DEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------------
 * Định nghĩa 3-byte DTC chuẩn hệ thống (Mục 7 CAN-byte-layout.md)
 * ------------------------------------------------------------------------------ */
#define DEM_DTC_LDR_SHORT_OPEN      0xB10001u  /* BCM: LDR ngoài dải (kẹt 0 hoặc max) */
#define DEM_DTC_DHT_TIMEOUT_NAN     0xB20001u  /* Climate: Đọc cảm biến DHT thất bại */
#define DEM_DTC_DHT_LDR_RANGE       0xB20002u  /* Climate: LDR ngoài dải */
#define DEM_DTC_WINDOW_POS_RANGE    0xB30001u  /* Window: Lệnh góc ngoài dải (>180 deg) */
#define DEM_DTC_WINDOW_PINCH        0xB30002u  /* Window: Chống kẹt mô phỏng kích hoạt */
#define DEM_DTC_WSW_PULSE_DROPOUT   0xC10001u  /* WSW: Mất xung encoder */
#define DEM_DTC_WSW_SPEED_MAX       0xC10002u  /* WSW: Cảnh báo trượt bánh được kích hoạt */
#define DEM_DTC_CAN_BUS_OFF         0xD00001u  /* Tất cả: Phục hồi sau CAN Bus-Off */
#define DEM_DTC_E2E_ERROR           0xD00002u  /* Tất cả: Lỗi E2E (3 lần lỗi liên tiếp) */
#define DEM_DTC_CAN_TIMEOUT         0xD00003u  /* Tất cả: Mất frame định kỳ quá 3x chu kỳ */
#define DEM_DTC_DMS_CAMERA_UNAVAIL  0xE00001u  /* DMS: Camera không khả dụng */
#define DEM_DTC_DMS_FPS_LOW         0xE00002u  /* DMS: FPS dưới ngưỡng kéo dài */

/* ------------------------------------------------------------------------------
 * Trạng thái DTC (Status Byte) & Event Reporting
 * ------------------------------------------------------------------------------ */
#define DEM_DTC_STATUS_PASSED   0x00u  /* Đã hết lỗi (healed / inactive) */
#define DEM_DTC_STATUS_FAILED   0x01u  /* Đang có lỗi (active / confirmed) */

typedef enum {
    DEM_EVENT_STATUS_PASSED = 0,
    DEM_EVENT_STATUS_FAILED = 1
} dem_event_status_t;

/* ------------------------------------------------------------------------------
 * Cấu trúc Freeze Frame cho WSW (DID 0x0120, 4 byte RAM snapshot)
 * Byte 0-1: Wheel_Speed raw (0.1 rpm, big-endian)
 * Byte 2:   Slip percent (0..100)
 * Byte 3:   Deceleration rpm trên 100ms
 * ------------------------------------------------------------------------------ */
typedef struct {
    uint16_t wheel_speed_raw;
    uint8_t  slip_percent;
    uint8_t  decel_rpm_100ms;
    bool     is_valid;
} dem_freeze_frame_t;

/* Số lượng khe lưu trữ DTC tối đa trên RAM tĩnh */
#define DEM_MAX_DTC_SLOTS 8u

typedef struct {
    uint32_t dtc_id;
    uint8_t  status;     /* 0x01: active, 0x00: healed */
    uint8_t  fail_count; /* Đếm lỗi liên tiếp (>=3 -> active) */
    uint8_t  pass_count; /* Đếm hợp lệ liên tiếp (>=3 -> healed) */
    bool     recorded;   /* Đã từng được kích hoạt */
} dem_dtc_slot_t;

/* ------------------------------------------------------------------------------
 * API DEM
 * ------------------------------------------------------------------------------ */
void Dem_Init(void);

/* Báo cáo trạng thái sự kiện lỗi (áp dụng bộ lọc debounce 3 lần) */
void Dem_ReportErrorStatus(uint32_t dtc_id, dem_event_status_t status);

/* Xóa toàn bộ bộ nhớ lỗi DTC và Freeze Frame (UDS Service 0x14) */
void Dem_ClearAllDtc(void);

/* Lấy trạng thái của một mã DTC cụ thể */
uint8_t Dem_GetDtcStatus(uint32_t dtc_id);
bool    Dem_IsDtcActive(uint32_t dtc_id);

/* Đếm số DTC thỏa mãn status_mask */
uint8_t Dem_GetDtcCount(uint8_t status_mask);

/* Lấy DTC đầu tiên thỏa mãn status_mask (dùng cho UDS 0x19 single frame) */
bool Dem_GetFirstMatchingDtc(uint8_t status_mask, uint32_t *dtc_id, uint8_t *status);

/* Quản lý Freeze Frame (DID 0x0120) */
void Dem_CaptureFreezeFrame(uint16_t wheel_speed_raw, uint8_t slip_pct, uint8_t decel_rpm_100ms);
bool Dem_GetFreezeFrameData(uint8_t buffer[4]);
bool Dem_IsFreezeFrameValid(void);

#ifdef __cplusplus
}
#endif

#endif /* DIAG_DEM_H */
