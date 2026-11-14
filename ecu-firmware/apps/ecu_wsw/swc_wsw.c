/* ==============================================================================
 * swc_wsw.c — Software Component Wheel Slip Warning Implementation
 * ============================================================================== */
#include "swc_wsw.h"

/* Hằng số cấu hình */
#define WSW_MAX_SPEED_RAW        8000u  /* 800.0 rpm (0.1 rpm/LSB) */
#define WSW_SLIP_ENTER_THRESHOLD   20u  /* 20 % */
#define WSW_SLIP_EXIT_THRESHOLD    10u  /* 10 % */
#define WSW_MIN_HOLD_TICKS         25u  /* 500 ms (25 x 20ms) */
#define WSW_MIN_REF_SPEED_RAW      50u  /* 5.0 rpm */
#define WSW_DECEL_THRESHOLD_RAW   100u  /* Giảm tốc > 10.0 rpm trên 100ms */

/* Trạng thái nội bộ */
static uint16_t s_wheel_speed_raw = 0;
static uint16_t s_ref_speed_raw = 0;
static uint8_t  s_slip_percent = 0;
static uint8_t  s_slip_warning_active = 0;
static uint8_t  s_warn_lamp = 0;
static uint8_t  s_decel_exceeded = 0;
static uint8_t  s_slip_consecutive_count = 0;
static uint16_t s_hold_timer_ticks = 0;
static uint16_t s_speed_history[5] = {0}; /* Lịch sử 5 mẫu 20ms = 100ms */
static uint8_t  s_history_idx = 0;

void WSW_Init_Runnable(void)
{
    s_wheel_speed_raw = 0;
    s_ref_speed_raw = 0;
    s_slip_percent = 0;
    s_slip_warning_active = 0;
    s_warn_lamp = 0;
    s_decel_exceeded = 0;
    s_slip_consecutive_count = 0;
    s_hold_timer_ticks = 0;
    s_history_idx = 0;
    for (int i = 0; i < 5; i++) {
        s_speed_history[i] = 0;
    }

    Rte_Write_WheelSpeed(0);
    Rte_Write_RefSpeed(0);
    Rte_Write_SlipPercent(0);
    Rte_Write_SlipWarningActive(0);
    Rte_Write_WarnLamp(0);
    Rte_Write_DecelExceeded(0);
}

void WSW_20ms_Capture_Runnable(void)
{
    uint16_t enc_speed = 0;
    bool dropout = false;

    Rte_Read_EncoderSpeed(&enc_speed, &dropout);

    if (dropout) {
        /* Báo lỗi mất xung encoder bất thường */
        Rte_Call_Dem_SetEventStatus(DEM_DTC_WSW_PULSE_DROPOUT, DEM_EVENT_STATUS_FAILED);
    } else {
        Rte_Call_Dem_SetEventStatus(DEM_DTC_WSW_PULSE_DROPOUT, DEM_EVENT_STATUS_PASSED);
    }

    if (enc_speed > WSW_MAX_SPEED_RAW) {
        /* Vượt tốc độ an toàn tối đa 800 rpm */
        Rte_Call_Dem_SetEventStatus(DEM_DTC_WSW_SPEED_MAX, DEM_EVENT_STATUS_FAILED);
    } else {
        Rte_Call_Dem_SetEventStatus(DEM_DTC_WSW_SPEED_MAX, DEM_EVENT_STATUS_PASSED);
    }

    s_wheel_speed_raw = enc_speed;

    /* Ước lượng RefSpeed: Trung bình trượt Exponential Moving Average:
     * ref = (ref * 7 + speed) / 8 */
    if (s_ref_speed_raw == 0) {
        s_ref_speed_raw = enc_speed;
    } else {
        s_ref_speed_raw = (uint16_t)(((uint32_t)s_ref_speed_raw * 7u + (uint32_t)enc_speed) / 8u);
    }

    /* Lưu vào bộ đệm lịch sử 100ms để tính giảm tốc */
    s_speed_history[s_history_idx] = enc_speed;
    s_history_idx = (uint8_t)((s_history_idx + 1u) % 5u);

    Rte_Write_WheelSpeed(s_wheel_speed_raw);
    Rte_Write_RefSpeed(s_ref_speed_raw);
}

void WSW_SlipCalc_Runnable(void)
{
    uint8_t brake_pressed = 0;
    Rte_Read_BrakeSwitch(&brake_pressed);

    /* 1. Tính toán Slip Ratio: Slip = (RefSpeed - WheelSpeed) / RefSpeed * 100% */
    if (s_ref_speed_raw > WSW_MIN_REF_SPEED_RAW && s_wheel_speed_raw < s_ref_speed_raw) {
        uint32_t diff = (uint32_t)(s_ref_speed_raw - s_wheel_speed_raw);
        uint32_t slip = (diff * 100u) / (uint32_t)s_ref_speed_raw;
        if (slip > 100u) slip = 100u;
        s_slip_percent = (uint8_t)slip;
    } else {
        s_slip_percent = 0;
    }

    /* 2. Tiêu chí 2: Tính giảm tốc trên cửa sổ 100ms (5 mẫu 20ms) */
    uint16_t oldest_speed = s_speed_history[s_history_idx];
    uint8_t decel_rpm_100ms = 0;
    if (oldest_speed > s_wheel_speed_raw) {
        uint16_t diff_spd = oldest_speed - s_wheel_speed_raw;
        decel_rpm_100ms = (uint8_t)(diff_spd / 10u); /* quy ra rpm */
        if (diff_spd > WSW_DECEL_THRESHOLD_RAW) {
            s_decel_exceeded = 1;
        } else {
            s_decel_exceeded = 0;
        }
    } else {
        s_decel_exceeded = 0;
    }

    /* 3. Logic Hysteresis cảnh báo trượt:
     * - Vào cảnh báo: Phanh đang nhấn VÀ Slip > 20% trong 2 mẫu liên tiếp (40 ms)
     * - Giữ tối thiểu 500 ms
     * - Ra khỏi cảnh báo: khi hết 500 ms VÀ Slip < 10% */
    if (brake_pressed && s_slip_percent >= WSW_SLIP_ENTER_THRESHOLD) {
        if (s_slip_consecutive_count < 2u) {
            s_slip_consecutive_count++;
        }
    } else {
        s_slip_consecutive_count = 0;
    }

    if (s_slip_consecutive_count >= 2u) {
        if (!s_slip_warning_active) {
            /* Kích hoạt cảnh báo trượt lần đầu -> Chụp Freeze Frame ngay lập tức! */
            Rte_Call_Dem_CaptureFreezeFrame(s_wheel_speed_raw, s_slip_percent, decel_rpm_100ms);
            Rte_Call_Dem_SetEventStatus(DEM_DTC_WSW_SPEED_MAX, DEM_EVENT_STATUS_FAILED);
        }
        s_slip_warning_active = 1;
        s_warn_lamp = 1;
        s_hold_timer_ticks = WSW_MIN_HOLD_TICKS; /* Đặt thời gian giữ 500 ms */
    }

    if (s_slip_warning_active) {
        if (s_hold_timer_ticks > 0) {
            s_hold_timer_ticks--;
        } else {
            /* Đã hết 500 ms -> Kiểm tra điều kiện thoát (Slip < 10%) */
            if (s_slip_percent < WSW_SLIP_EXIT_THRESHOLD) {
                s_slip_warning_active = 0;
                s_warn_lamp = 0;
            }
        }
    }

    Rte_Write_SlipPercent(s_slip_percent);
    Rte_Write_SlipWarningActive(s_slip_warning_active);
    Rte_Write_WarnLamp(s_warn_lamp);
    Rte_Write_DecelExceeded(s_decel_exceeded);
}
