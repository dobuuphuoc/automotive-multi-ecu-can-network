/* ==============================================================================
 * swc_bcm.c — Software Component Body Control Module Implementation
 * ============================================================================== */
#include "swc_bcm.h"

/* Cấu hình ngưỡng */
#define BCM_AUTO_LOCK_SPEED_RAW   600u   /* 60.0 rpm (0.1 rpm/LSB) */
#define BCM_DOOR_WARN_SPEED_RAW   100u   /* 10.0 rpm */
#define BCM_DARK_THRESHOLD_ADC    1500u  /* Ngưỡng tối (bật đèn) */
#define BCM_LIGHT_THRESHOLD_ADC   2500u  /* Ngưỡng sáng (tắt đèn) */
#define BCM_DARK_HOLD_TICKS       20u    /* 2.0 s (20 x 100ms) */
#define BCM_LIGHT_HOLD_TICKS      50u    /* 5.0 s (50 x 100ms) */
#define BCM_DMS_TIMEOUT_TICKS     5u     /* 500 ms (5 x 100ms) */

/* Trạng thái nội bộ */
static uint8_t  s_lock_state = 0;       /* 0=Unlocked, 1=Locked */
static uint8_t  s_headlight_mode = 0;   /* 0=Auto, 1=Off, 2=On */
static uint8_t  s_headlight_state = 0;
static uint16_t s_dark_counter = 0;
static uint16_t s_light_counter = 0;
static uint8_t  s_cabin_mode = 0;       /* 0=Auto, 1=Off, 2=On */
static uint8_t  s_cabin_state = 0;
static uint8_t  s_alarm_state = 0;      /* 0=Disarmed, 1=Armed, 2=Alarm */
static uint8_t  s_dms_buzzer_active = 0;
static uint8_t  s_dms_timeout_counter = 0;
static uint8_t  s_door_warn_buzzer = 0;

void BCM_Init_Runnable(void)
{
    s_lock_state = 0;
    s_headlight_mode = 0;
    s_headlight_state = 0;
    s_dark_counter = 0;
    s_light_counter = 0;
    s_cabin_mode = 0;
    s_cabin_state = 0;
    s_alarm_state = 0;
    s_dms_buzzer_active = 0;
    s_dms_timeout_counter = 0;
    s_door_warn_buzzer = 0;

    Rte_Write_LockState(s_lock_state);
    Rte_Write_Headlight(s_headlight_state);
    Rte_Write_CabinLight(s_cabin_state);
    Rte_Write_Buzzer(0);
    Rte_Write_AlarmState(s_alarm_state);
}

void BCM_CmdHandler_Runnable(void)
{
    com_msg_bcm_cmd_t cmd;
    uint8_t status = RTE_SIG_VALID;

    if (Rte_Read_BcmCmd(&cmd, &status) == E_OK) {
        if (status == RTE_SIG_VALID) {
            /* Lệnh khóa/mở cửa */
            if (cmd.lock_cmd == COM_LOCK_CMD_LOCK) {
                s_lock_state = 1;
            } else if (cmd.lock_cmd == COM_LOCK_CMD_UNLOCK) {
                s_lock_state = 0;
            }

            /* Lệnh đèn pha */
            s_headlight_mode = cmd.headlight_cmd;
            if (s_headlight_mode == COM_LIGHT_CMD_ON) {
                s_headlight_state = 1;
            } else if (s_headlight_mode == COM_LIGHT_CMD_OFF) {
                s_headlight_state = 0;
            }

            /* Lệnh đèn cabin */
            s_cabin_mode = cmd.cabinlight_cmd;
            if (s_cabin_mode == COM_LIGHT_CMD_ON) {
                s_cabin_state = 1;
            } else if (s_cabin_mode == COM_LIGHT_CMD_OFF) {
                s_cabin_state = 0;
            }
        }
    }
}

void BCM_100ms_Runnable(void)
{
    /* 1. Đọc trạng thái cảm biến cửa */
    uint8_t door_open = 0;
    Rte_Read_DoorSensor(&door_open);
    Rte_Write_DoorStatus(door_open);

    /* 2. Đọc tốc độ bánh xe từ WHEEL_SPEED qua CAN */
    uint16_t wheel_speed = 0;
    uint8_t speed_status = RTE_SIG_VALID;
    Rte_Read_WheelSpeed(&wheel_speed, &speed_status);

    if (speed_status == RTE_SIG_VALID) {
        /* Báo cáo DEM đã có frame hợp lệ */
        Rte_Call_Dem_SetEventStatus(DEM_DTC_E2E_ERROR, DEM_EVENT_STATUS_PASSED);
        Rte_Call_Dem_SetEventStatus(DEM_DTC_CAN_TIMEOUT, DEM_EVENT_STATUS_PASSED);

        /* FR-BCM-06: Tự khóa cửa khi tốc độ > 60 rpm */
        if (wheel_speed > BCM_AUTO_LOCK_SPEED_RAW) {
            s_lock_state = 1;
        }

        /* FR-BCM-07: Cảnh báo cửa mở khi đang chạy (>10 rpm) */
        if (door_open == 1 && wheel_speed > BCM_DOOR_WARN_SPEED_RAW) {
            s_door_warn_buzzer = 1;
        } else {
            s_door_warn_buzzer = 0;
        }
    } else {
        /* Failsafe: Tín hiệu mất hoặc sai E2E -> Bỏ tự khóa cửa, ghi DTC */
        Rte_Call_Dem_SetEventStatus(DEM_DTC_CAN_TIMEOUT, DEM_EVENT_STATUS_FAILED);
        s_door_warn_buzzer = 0;
    }
    Rte_Write_LockState(s_lock_state);

    /* 3. Đọc cảm biến ánh sáng LDR & Điều khiển đèn pha tự động */
    uint16_t ldr_val = 2000;
    Rte_Read_LightSensor(&ldr_val);

    /* Chuyển đổi 12-bit ADC sang 8-bit CAN Ambient_Light */
    uint8_t ambient_8bit = (uint8_t)(ldr_val >> 4);
    Rte_Write_AmbientLight(ambient_8bit);

    if (ldr_val == 0 || ldr_val >= 4095u) {
        /* Lỗi hở mạch hoặc ngắn mạch LDR -> Failsafe: Bật đèn pha, ghi DTC 0xB10001 */
        Rte_Call_Dem_SetEventStatus(DEM_DTC_LDR_SHORT_OPEN, DEM_EVENT_STATUS_FAILED);
        s_headlight_state = 1;
    } else {
        Rte_Call_Dem_SetEventStatus(DEM_DTC_LDR_SHORT_OPEN, DEM_EVENT_STATUS_PASSED);

        /* Tự động bật/tắt có Hysteresis và trễ thời gian (2.0s tối / 5.0s sáng) */
        if (s_headlight_mode == COM_LIGHT_CMD_AUTO) {
            if (ldr_val < BCM_DARK_THRESHOLD_ADC) {
                s_dark_counter++;
                s_light_counter = 0;
                if (s_dark_counter >= BCM_DARK_HOLD_TICKS) {
                    s_headlight_state = 1;
                }
            } else if (ldr_val > BCM_LIGHT_THRESHOLD_ADC) {
                s_light_counter++;
                s_dark_counter = 0;
                if (s_light_counter >= BCM_LIGHT_HOLD_TICKS) {
                    s_headlight_state = 0;
                }
            } else {
                s_dark_counter = 0;
                s_light_counter = 0;
            }
        }
    }
    Rte_Write_Headlight(s_headlight_state);

    /* 4. Điều khiển đèn cabin: tự động bật khi cửa mở */
    if (s_cabin_mode == COM_LIGHT_CMD_AUTO) {
        s_cabin_state = door_open ? 1 : 0;
    }
    Rte_Write_CabinLight(s_cabin_state);

    /* 5. Xử lý cảnh báo DMS Alert (buồn ngủ / mất tập trung) */
    uint8_t dms_alert = 0, dms_severity = 0, dms_status = RTE_SIG_VALID;
    Rte_Read_DMSAlert(&dms_alert, &dms_severity, &dms_status);

    if (dms_status == RTE_SIG_VALID && dms_alert != COM_ALERT_TYPE_NONE && dms_severity > 0) {
        s_dms_buzzer_active = 1;
        s_dms_timeout_counter = 0;
        s_alarm_state = COM_ALARM_STATE_ALARM;
    } else {
        /* Timeout 500ms không nhận DMS Alert hợp lệ -> Tự động tắt buzzer do DMS kích hoạt */
        if (s_dms_timeout_counter < BCM_DMS_TIMEOUT_TICKS) {
            s_dms_timeout_counter++;
        } else {
            s_dms_buzzer_active = 0;
            if (s_alarm_state == COM_ALARM_STATE_ALARM) {
                s_alarm_state = s_lock_state ? COM_ALARM_STATE_ARMED : COM_ALARM_STATE_DISARMED;
            }
        }
    }
    Rte_Write_AlarmState(s_alarm_state);

    /* Tổng hợp còi buzzer: bật khi cảnh báo cửa mở hoặc có DMS alert */
    uint8_t final_buzzer = (s_door_warn_buzzer || s_dms_buzzer_active) ? 1 : 0;
    Rte_Write_Buzzer(final_buzzer);
}
