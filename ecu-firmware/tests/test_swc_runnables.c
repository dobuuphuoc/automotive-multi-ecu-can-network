#include "unity.h"
#include "Rte_Type.h"
#include "Rte_BCM.h"
#include "Rte_Climate.h"
#include "Rte_Window.h"
#include "Rte_WSW.h"
#include "swc_bcm.h"
#include "swc_climate.h"
#include "swc_window.h"
#include "swc_wsw.h"
#include "diag_dem.h"
#include <string.h>

/* Khai báo các helper setter từ rte.c */
void Rte_Init(void);
void Rte_Set_DoorSensor(uint8_t open);
void Rte_Set_LightSensor(uint16_t raw);
void Rte_Set_DHT_Sensors(int16_t temp_raw, uint16_t hum_raw, bool valid);
void Rte_Set_SunlightSensor(uint8_t sunlight);
void Rte_Set_ObstacleSensor(uint8_t obstacle);
void Rte_Set_EncoderSpeed(uint16_t raw, bool dropout);
void Rte_Set_BrakeSwitch(uint8_t pressed);

void Rte_Sync_WheelSpeed_Rx(uint16_t speed_raw, uint8_t sig_status);
void Rte_Sync_DmsAlert_Rx(uint8_t alert_type, uint8_t severity, uint8_t sig_status);
void Rte_Sync_BcmCmd_Rx(const com_msg_bcm_cmd_t *cmd, uint8_t sig_status);
void Rte_Sync_ClimateCmd_Rx(const com_msg_climate_cmd_t *cmd, uint8_t sig_status);
void Rte_Sync_WindowCmd_Rx(const com_msg_window_cmd_t *cmd, uint8_t sig_status);
void Rte_Sync_BcmStatus_Rx(const com_msg_bcm_status_t *msg, uint8_t sig_status);

void Rte_Get_BcmStatus_Tx(com_msg_bcm_status_t *msg);
void Rte_Get_ClimateStatus_Tx(com_msg_climate_status_t *msg);
void Rte_Get_WindowStatus_Tx(com_msg_window_status_t *msg);
void Rte_Get_WswWarning_Tx(com_msg_wsw_warning_t *msg);

void setUp(void)
{
    Dem_Init();
    Rte_Init();
    BCM_Init_Runnable();
    Climate_Init_Runnable();
    Window_Init_Runnable();
    WSW_Init_Runnable();
}

void tearDown(void) {}

/* ------------------------------------------------------------------------------
 * Test BCM SWC Runnables
 * ------------------------------------------------------------------------------ */
static void test_bcm_auto_lock_and_failsafe(void)
{
    com_msg_bcm_status_t bcm_st;

    /* 1. Xe đứng yên: tốc độ 0 -> Cửa vẫn mở (unlocked) */
    Rte_Sync_WheelSpeed_Rx(0, RTE_SIG_VALID);
    BCM_100ms_Runnable();
    Rte_Get_BcmStatus_Tx(&bcm_st);
    TEST_ASSERT_EQUAL_UINT8(0, bcm_st.lock_state);

    /* 2. Xe tăng tốc lên 65 rpm (> 60.0 rpm) và tín hiệu VALID -> Tự động khóa cửa! */
    Rte_Sync_WheelSpeed_Rx(650, RTE_SIG_VALID);
    BCM_100ms_Runnable();
    Rte_Get_BcmStatus_Tx(&bcm_st);
    TEST_ASSERT_EQUAL_UINT8(1, bcm_st.lock_state);

    /* 3. Mở khóa bằng lệnh khi xe dừng */
    Rte_Sync_WheelSpeed_Rx(0, RTE_SIG_VALID);
    com_msg_bcm_cmd_t cmd = { .lock_cmd = COM_LOCK_CMD_UNLOCK };
    Rte_Sync_BcmCmd_Rx(&cmd, RTE_SIG_VALID);
    BCM_CmdHandler_Runnable();
    BCM_100ms_Runnable();
    Rte_Get_BcmStatus_Tx(&bcm_st);
    TEST_ASSERT_EQUAL_UINT8(0, bcm_st.lock_state);

    /* 4. Failsafe: Tốc độ cao nhưng tín hiệu INVALID (lỗi E2E/mất CAN) -> KHÔNG tự khóa! */
    Rte_Sync_WheelSpeed_Rx(700, RTE_SIG_INVALID);
    for (int i = 0; i < 3; i++) {
        BCM_100ms_Runnable();
    }
    Rte_Get_BcmStatus_Tx(&bcm_st);
    TEST_ASSERT_EQUAL_UINT8(0, bcm_st.lock_state); /* Vẫn giữ unlock failsafe */
    TEST_ASSERT_TRUE(Dem_IsDtcActive(DEM_DTC_CAN_TIMEOUT));
}

static void test_bcm_door_warning_and_headlight_hysteresis(void)
{
    com_msg_bcm_status_t bcm_st;

    /* Cảnh báo cửa mở khi đang chạy (>10 rpm) */
    Rte_Set_DoorSensor(1);
    Rte_Sync_WheelSpeed_Rx(150, RTE_SIG_VALID); /* 15 rpm */
    BCM_100ms_Runnable();
    Rte_Get_BcmStatus_Tx(&bcm_st);
    TEST_ASSERT_EQUAL_UINT8(1, bcm_st.door_open);
    TEST_ASSERT_EQUAL_UINT8(1, bcm_st.buzzer_on);

    /* Đóng cửa -> hết còi */
    Rte_Set_DoorSensor(0);
    BCM_100ms_Runnable();
    Rte_Get_BcmStatus_Tx(&bcm_st);
    TEST_ASSERT_EQUAL_UINT8(0, bcm_st.buzzer_on);

    /* Headlight Hysteresis:
     * Trời tối (<1500) liên tục 2.0s (20 ticks x 100ms) -> Bật đèn */
    Rte_Set_LightSensor(1200);
    for (int i = 0; i < 19; i++) {
        BCM_100ms_Runnable();
    }
    Rte_Get_BcmStatus_Tx(&bcm_st);
    TEST_ASSERT_EQUAL_UINT8(0, bcm_st.headlight_on); /* 1.9s chưa bật */

    BCM_100ms_Runnable(); /* Tick 20 = 2.0s */
    Rte_Get_BcmStatus_Tx(&bcm_st);
    TEST_ASSERT_EQUAL_UINT8(1, bcm_st.headlight_on); /* Đã bật! */

    /* Trời sáng (>2500) liên tục 5.0s (50 ticks x 100ms) -> Tắt đèn */
    Rte_Set_LightSensor(3000);
    for (int i = 0; i < 49; i++) {
        BCM_100ms_Runnable();
    }
    Rte_Get_BcmStatus_Tx(&bcm_st);
    TEST_ASSERT_EQUAL_UINT8(1, bcm_st.headlight_on); /* 4.9s vẫn bật */

    BCM_100ms_Runnable(); /* Tick 50 = 5.0s */
    Rte_Get_BcmStatus_Tx(&bcm_st);
    TEST_ASSERT_EQUAL_UINT8(0, bcm_st.headlight_on); /* Đã tắt! */

    /* LDR out of range (ngắn mạch = 0) -> Failsafe: Bật đèn pha + Báo lỗi DTC 0xB10001 */
    Rte_Set_LightSensor(0);
    for (int i = 0; i < 3; i++) {
        BCM_100ms_Runnable();
    }
    Rte_Get_BcmStatus_Tx(&bcm_st);
    TEST_ASSERT_EQUAL_UINT8(1, bcm_st.headlight_on);
    TEST_ASSERT_TRUE(Dem_IsDtcActive(DEM_DTC_LDR_SHORT_OPEN));
}

static void test_bcm_dms_alert_and_timeout(void)
{
    com_msg_bcm_status_t bcm_st;

    /* Nhận DMS Alert (Drowsy, High severity) */
    Rte_Sync_DmsAlert_Rx(COM_ALERT_TYPE_DROWSY, COM_SEVERITY_HIGH, RTE_SIG_VALID);
    BCM_100ms_Runnable();
    Rte_Get_BcmStatus_Tx(&bcm_st);
    TEST_ASSERT_EQUAL_UINT8(1, bcm_st.buzzer_on);
    TEST_ASSERT_EQUAL_UINT8(COM_ALARM_STATE_ALARM, bcm_st.alarm_state);

    /* Timeout DMS alert (>500ms không có frame hợp lệ) -> Tự tắt buzzer */
    Rte_Sync_DmsAlert_Rx(COM_ALERT_TYPE_NONE, COM_SEVERITY_NONE, RTE_SIG_INVALID);
    for (int i = 0; i < 6; i++) { /* 600ms */
        BCM_100ms_Runnable();
    }
    Rte_Get_BcmStatus_Tx(&bcm_st);
    TEST_ASSERT_EQUAL_UINT8(0, bcm_st.buzzer_on);
}

/* ------------------------------------------------------------------------------
 * Test Climate SWC Runnables
 * ------------------------------------------------------------------------------ */
static void test_climate_bang_bang_and_dht_failsafe(void)
{
    com_msg_climate_status_t cli_st;

    /* 1. Auto mode: Setpoint = 24.0 C (raw 48).
     * Khi nhiệt độ cabin = 25.0 C (> 24.0 + 0.5) -> Quạt bật làm mát 75% */
    Rte_Set_DHT_Sensors(250, 500, true);
    Climate_SensorRead_2s_Runnable();
    Climate_100ms_Runnable();
    Rte_Get_ClimateStatus_Tx(&cli_st);
    TEST_ASSERT_EQUAL_UINT8(75, cli_st.fan_speed);
    TEST_ASSERT_EQUAL_UINT8(0, cli_st.dht_fault);

    /* Khi nhiệt độ cabin = 23.0 C (< 24.0 - 0.5) -> Quạt giảm về 20% */
    Rte_Set_DHT_Sensors(230, 500, true);
    Climate_SensorRead_2s_Runnable();
    Climate_100ms_Runnable();
    Rte_Get_ClimateStatus_Tx(&cli_st);
    TEST_ASSERT_EQUAL_UINT8(20, cli_st.fan_speed);

    /* 2. DHT Failsafe: DHT hỏng (invalid) */
    Rte_Set_DHT_Sensors(0, 0, false);
    for (int i = 0; i < 3; i++) {
        Climate_SensorRead_2s_Runnable();
    }
    Climate_100ms_Runnable();
    Rte_Get_ClimateStatus_Tx(&cli_st);
    TEST_ASSERT_EQUAL_UINT8(1, cli_st.dht_fault);
    TEST_ASSERT_EQUAL_INT16(250, cli_st.cabin_temp_raw); /* Giá trị dự phòng 25.0 C */
    TEST_ASSERT_EQUAL_UINT8(50, cli_st.fan_speed);        /* Quạt failsafe 50% */
    TEST_ASSERT_TRUE(Dem_IsDtcActive(DEM_DTC_DHT_TIMEOUT_NAN));
}

static void test_climate_fog_risk_and_dms_override(void)
{
    com_msg_climate_status_t cli_st;

    /* Độ ẩm cao 85% -> Cảnh báo sương mù Fog_Risk = 1 */
    Rte_Set_DHT_Sensors(220, 850, true);
    Climate_SensorRead_2s_Runnable();
    Climate_100ms_Runnable();
    Rte_Get_ClimateStatus_Tx(&cli_st);
    TEST_ASSERT_EQUAL_UINT8(1, cli_st.fog_risk);

    /* DMS Drowsiness Override: Khi có cảnh báo buồn ngủ mức High -> Quạt chạy 100% trong 30s */
    Rte_Sync_DmsAlert_Rx(COM_ALERT_TYPE_DROWSY, COM_SEVERITY_HIGH, RTE_SIG_VALID);
    Climate_100ms_Runnable();
    Rte_Get_ClimateStatus_Tx(&cli_st);
    TEST_ASSERT_EQUAL_UINT8(1, cli_st.fan_override);
    TEST_ASSERT_EQUAL_UINT8(100, cli_st.fan_speed);

    /* Giữ override trong 30s (300 ticks) */
    Rte_Sync_DmsAlert_Rx(0, 0, RTE_SIG_VALID);
    for (int i = 0; i < 298; i++) {
        Climate_100ms_Runnable();
    }
    Rte_Get_ClimateStatus_Tx(&cli_st);
    TEST_ASSERT_EQUAL_UINT8(1, cli_st.fan_override);

    /* Hết 30s -> Trả về chế độ bình thường */
    Climate_100ms_Runnable();
    Climate_100ms_Runnable();
    Rte_Get_ClimateStatus_Tx(&cli_st);
    TEST_ASSERT_EQUAL_UINT8(0, cli_st.fan_override);
}

/* ------------------------------------------------------------------------------
 * Test Window SWC Runnables
 * ------------------------------------------------------------------------------ */
static void test_window_rate_limited_tracking_and_range_check(void)
{
    com_msg_window_status_t win_st;

    /* 1. Lệnh vượt ngưỡng 180 độ (>180) -> Bỏ lệnh + Báo DTC 0xB30001 */
    com_msg_window_cmd_t cmd_invalid = { .target_angle = 195 };
    Rte_Sync_WindowCmd_Rx(&cmd_invalid, RTE_SIG_VALID);
    for (int i = 0; i < 3; i++) {
        Window_Cmd_Runnable();
    }
    Rte_Get_WindowStatus_Tx(&win_st);
    TEST_ASSERT_EQUAL_UINT8(0, win_st.target_angle_echo);
    TEST_ASSERT_TRUE(Dem_IsDtcActive(DEM_DTC_WINDOW_POS_RANGE));

    /* 2. Lệnh hợp lệ mở 50 độ -> Di chuyển mượt từng độ */
    com_msg_window_cmd_t cmd_valid = { .target_angle = 50 };
    Rte_Sync_WindowCmd_Rx(&cmd_valid, RTE_SIG_VALID);
    Window_Cmd_Runnable();

    /* Sau 10 chu kỳ 10ms = 100ms -> góc hiện tại đạt 10 độ, đang moving */
    for (int i = 0; i < 10; i++) {
        Window_10ms_PID_Runnable();
    }
    Rte_Get_WindowStatus_Tx(&win_st);
    TEST_ASSERT_EQUAL_UINT8(10, win_st.current_angle);
    TEST_ASSERT_EQUAL_UINT8(1, win_st.moving);

    /* Sau thêm 41 chu kỳ -> đạt đúng 50 độ và chuyển sang trạng thái dừng (moving = 0) */
    for (int i = 0; i < 41; i++) {
        Window_10ms_PID_Runnable();
    }
    Rte_Get_WindowStatus_Tx(&win_st);
    TEST_ASSERT_EQUAL_UINT8(50, win_st.current_angle);
    TEST_ASSERT_EQUAL_UINT8(0, win_st.moving);
}

static void test_window_anti_pinch_and_child_lock(void)
{
    com_msg_window_status_t win_st;

    /* Mở kính ra 100 độ trước */
    com_msg_window_cmd_t cmd = { .target_angle = 100 };
    Rte_Sync_WindowCmd_Rx(&cmd, RTE_SIG_VALID);
    Window_Cmd_Runnable();
    for (int i = 0; i < 100; i++) {
        Window_10ms_PID_Runnable();
    }

    /* Ra lệnh đóng kính về 0 độ */
    cmd.target_angle = 0;
    Rte_Sync_WindowCmd_Rx(&cmd, RTE_SIG_VALID);
    Window_Cmd_Runnable();

    /* Đang đóng kính về 80 độ thì gặp vật cản (Obstacle = 1) */
    for (int i = 0; i < 20; i++) {
        Window_10ms_PID_Runnable();
    }
    Rte_Set_ObstacleSensor(1);
    Window_10ms_PID_Runnable();

    /* Chống kẹt kích hoạt: đảo chiều mở thêm 36 độ (80 + 36 = 116) */
    Rte_Get_WindowStatus_Tx(&win_st);
    TEST_ASSERT_EQUAL_UINT8(1, win_st.pinch_detected);
    TEST_ASSERT_EQUAL_UINT8(116, win_st.target_angle_echo);

    /* Child lock: chỉ nhận lệnh đóng, bỏ lệnh mở */
    cmd.child_lock = 1;
    cmd.target_angle = 150; /* Muốn mở thêm -> bị từ chối */
    Rte_Sync_WindowCmd_Rx(&cmd, RTE_SIG_VALID);
    Window_Cmd_Runnable();
    Rte_Get_WindowStatus_Tx(&win_st);
    TEST_ASSERT_NOT_EQUAL(150, win_st.target_angle_echo);
}

/* ------------------------------------------------------------------------------
 * Test WSW SWC Runnables
 * ------------------------------------------------------------------------------ */
static void test_wsw_slip_hysteresis_and_freeze_frame(void)
{
    com_msg_wsw_warning_t warn_st;

    /* Xe chạy đều: encoder speed = 100.0 rpm (1000 raw) */
    Rte_Set_EncoderSpeed(1000, false);
    Rte_Set_BrakeSwitch(0);
    WSW_20ms_Capture_Runnable();
    WSW_SlipCalc_Runnable();

    Rte_Get_WswWarning_Tx(&warn_st);
    TEST_ASSERT_EQUAL_UINT8(0, warn_st.slip_warning_active);
    TEST_ASSERT_EQUAL_UINT8(0, warn_st.slip_percent);

    /* Bắt đầu đạp phanh và bánh xe bị trượt: RefSpeed ~ 1000, bánh giảm xuống 700 (trượt 30%) */
    Rte_Set_BrakeSwitch(1);
    Rte_Set_EncoderSpeed(700, false);

    /* Mẫu 1: Slip > 20% lần 1 -> chưa kích hoạt cảnh báo (cần 2 mẫu liên tiếp) */
    WSW_20ms_Capture_Runnable();
    WSW_SlipCalc_Runnable();
    Rte_Get_WswWarning_Tx(&warn_st);
    TEST_ASSERT_EQUAL_UINT8(0, warn_st.slip_warning_active);

    /* Mẫu 2: Slip > 20% lần 2 (sau 40ms) -> Kích hoạt Slip Warning & Freeze Frame! */
    WSW_20ms_Capture_Runnable();
    WSW_SlipCalc_Runnable();
    Rte_Get_WswWarning_Tx(&warn_st);
    TEST_ASSERT_EQUAL_UINT8(1, warn_st.slip_warning_active);
    TEST_ASSERT_EQUAL_UINT8(1, warn_st.warn_lamp);
    TEST_ASSERT_TRUE(Dem_IsFreezeFrameValid());

    /* Thời gian giữ tối thiểu 500ms (25 mẫu 20ms): kể cả Slip giảm xuống 5% ngay, vẫn giữ cảnh báo */
    Rte_Set_EncoderSpeed(950, false); /* Slip ~ 5% */
    for (int i = 0; i < 20; i++) {
        WSW_20ms_Capture_Runnable();
        WSW_SlipCalc_Runnable();
    }
    Rte_Get_WswWarning_Tx(&warn_st);
    TEST_ASSERT_EQUAL_UINT8(1, warn_st.slip_warning_active); /* Vẫn giữ cảnh báo */

    /* Hết 500ms và Slip < 10% -> Thoát cảnh báo */
    for (int i = 0; i < 10; i++) {
        WSW_20ms_Capture_Runnable();
        WSW_SlipCalc_Runnable();
    }
    Rte_Get_WswWarning_Tx(&warn_st);
    TEST_ASSERT_EQUAL_UINT8(0, warn_st.slip_warning_active);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_bcm_auto_lock_and_failsafe);
    RUN_TEST(test_bcm_door_warning_and_headlight_hysteresis);
    RUN_TEST(test_bcm_dms_alert_and_timeout);
    RUN_TEST(test_climate_bang_bang_and_dht_failsafe);
    RUN_TEST(test_climate_fog_risk_and_dms_override);
    RUN_TEST(test_window_rate_limited_tracking_and_range_check);
    RUN_TEST(test_window_anti_pinch_and_child_lock);
    RUN_TEST(test_wsw_slip_hysteresis_and_freeze_frame);
    return UNITY_END();
}
