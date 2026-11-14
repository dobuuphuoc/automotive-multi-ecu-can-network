/* ==============================================================================
 * swc_climate.c — Software Component Climate Control Implementation
 * ============================================================================== */
#include "swc_climate.h"

/* Hằng số cấu hình */
#define CLI_DEFAULT_TEMP_SETPOINT_RAW  48u    /* 24.0 degC (0.5 degC/LSB) */
#define CLI_DEFAULT_SETPOINT_01C      240     /* 24.0 degC (0.1 degC/LSB) */
#define CLI_HYSTERESIS_01C             5      /* 0.5 degC */
#define CLI_DMS_OVERRIDE_TICKS        300u   /* 30.0 s (300 x 100ms) */
#define CLI_FOG_HUMIDITY_THRESHOLD    800u   /* 80.0 % (0.1 %/LSB) */

/* Trạng thái nội bộ */
static uint8_t  s_climate_mode = COM_CLIMATE_MODE_AUTO;
static uint8_t  s_temp_setpoint_raw = CLI_DEFAULT_TEMP_SETPOINT_RAW;
static int16_t  s_cabin_temp_raw = 240;
static uint16_t s_cabin_hum_raw  = 500;
static uint8_t  s_fan_speed = 30;
static uint8_t  s_flap_angle = 90;
static uint8_t  s_dht_fault = 0;
static uint8_t  s_fog_risk = 0;
static uint8_t  s_fan_override = 0;
static uint16_t s_override_counter = 0;

static uint8_t  s_manual_fan_cmd = 0;
static uint8_t  s_manual_flap_cmd = 90;

void Climate_Init_Runnable(void)
{
    s_climate_mode = COM_CLIMATE_MODE_AUTO;
    s_temp_setpoint_raw = CLI_DEFAULT_TEMP_SETPOINT_RAW;
    s_cabin_temp_raw = 240;
    s_cabin_hum_raw = 500;
    s_fan_speed = 30;
    s_flap_angle = 90;
    s_dht_fault = 0;
    s_fog_risk = 0;
    s_fan_override = 0;
    s_override_counter = 0;
    s_manual_fan_cmd = 0;
    s_manual_flap_cmd = 90;

    Rte_Write_ClimateMode(s_climate_mode);
    Rte_Write_FanSpeed(s_fan_speed);
    Rte_Write_FlapAngle(s_flap_angle);
    Rte_Write_DHT_Fault(s_dht_fault);
    Rte_Write_FogRisk(s_fog_risk);
    Rte_Write_FanOverride(s_fan_override);
    Rte_Write_CabinTemp(s_cabin_temp_raw);
    Rte_Write_CabinHumidity(s_cabin_hum_raw);
}

void Climate_SensorRead_2s_Runnable(void)
{
    int16_t t_raw = 0;
    uint16_t h_raw = 0;
    bool valid = false;

    Rte_Read_DHT_Sensors(&t_raw, &h_raw, &valid);

    if (valid) {
        s_cabin_temp_raw = t_raw;
        s_cabin_hum_raw  = h_raw;
        s_dht_fault      = 0;
        Rte_Call_Dem_SetEventStatus(DEM_DTC_DHT_TIMEOUT_NAN, DEM_EVENT_STATUS_PASSED);
    } else {
        /* Failsafe: DHT22 hỏng/mất dữ liệu -> Báo DTC 0xB20001, dùng giá trị dự phòng 25.0 C, quạt 50% */
        s_dht_fault      = 1;
        s_cabin_temp_raw = 250; /* 25.0 degC */
        s_cabin_hum_raw  = 500; /* 50.0 % */
        Rte_Call_Dem_SetEventStatus(DEM_DTC_DHT_TIMEOUT_NAN, DEM_EVENT_STATUS_FAILED);
    }

    Rte_Write_CabinTemp(s_cabin_temp_raw);
    Rte_Write_CabinHumidity(s_cabin_hum_raw);
    Rte_Write_DHT_Fault(s_dht_fault);
}

void Climate_100ms_Runnable(void)
{
    /* 1. Đọc lệnh điều khiển từ Master (MP157) qua CAN */
    com_msg_climate_cmd_t cmd;
    uint8_t cmd_status = RTE_SIG_VALID;
    if (Rte_Read_ClimateCmd(&cmd, &cmd_status) == E_OK && cmd_status == RTE_SIG_VALID) {
        s_climate_mode      = cmd.climate_mode_cmd;
        s_manual_fan_cmd    = cmd.fan_speed_cmd;
        s_manual_flap_cmd   = cmd.flap_angle_cmd;
        s_temp_setpoint_raw = cmd.temp_setpoint_raw;
    }

    /* 2. Cảnh báo sương mù (Fog Risk) khi độ ẩm >= 80% */
    if (s_cabin_hum_raw >= CLI_FOG_HUMIDITY_THRESHOLD) {
        s_fog_risk = 1;
    } else {
        s_fog_risk = 0;
    }
    Rte_Write_FogRisk(s_fog_risk);

    /* 3. Phản ứng cảnh báo buồn ngủ từ DMS Alert (Jetson) */
    uint8_t alert_type = 0, severity = 0, dms_status = RTE_SIG_VALID;
    Rte_Read_DMSAlert(&alert_type, &severity, &dms_status);

    if (dms_status == RTE_SIG_VALID && alert_type == COM_ALERT_TYPE_DROWSY && severity >= COM_SEVERITY_HIGH) {
        /* Kích hoạt Fan Override 100% trong 30 giây */
        s_fan_override = 1;
        s_override_counter = CLI_DMS_OVERRIDE_TICKS;
    }

    if (s_override_counter > 0) {
        s_override_counter--;
        s_fan_override = 1;
        s_fan_speed = 100; /* Quạt chạy tối đa để làm tỉnh táo lái xe */
        if (s_override_counter == 0) {
            s_fan_override = 0;
        }
    } else {
        s_fan_override = 0;

        /* Nếu không bị Override bởi DMS -> Điều khiển theo chế độ Auto / Manual */
        if (s_climate_mode == COM_CLIMATE_MODE_AUTO) {
            /* Nếu DHT hỏng -> quạt 50% failsafe */
            if (s_dht_fault) {
                s_fan_speed = 50;
            } else {
                /* Bang-bang control có hysteresis +-0.5 C */
                int16_t setpoint_01c = (int16_t)(s_temp_setpoint_raw * 5); /* factor 0.5 * 10 = 5 */

                if (s_cabin_temp_raw > (setpoint_01c + CLI_HYSTERESIS_01C)) {
                    /* Nhiệt độ cabin cao hơn setpoint -> bật làm mát quạt 75% */
                    s_fan_speed = 75;
                } else if (s_cabin_temp_raw < (setpoint_01c - CLI_HYSTERESIS_01C)) {
                    /* Nhiệt độ cabin thấp hơn setpoint -> giảm quạt về mức tối thiểu 20% */
                    s_fan_speed = 20;
                }
            }
            s_flap_angle = 90; /* Auto giữ góc cánh gió trung hòa */
        } else {
            /* Chế độ Manual: áp dụng trực tiếp lệnh từ Master */
            s_fan_speed  = s_manual_fan_cmd;
            s_flap_angle = s_manual_flap_cmd;
        }
    }

    Rte_Write_ClimateMode(s_climate_mode);
    Rte_Write_FanSpeed(s_fan_speed);
    Rte_Write_FlapAngle(s_flap_angle);
    Rte_Write_FanOverride(s_fan_override);
}
