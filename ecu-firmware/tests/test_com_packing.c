#include "unity.h"
#include "can_com.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* Test 1: 0x100 WSW_WARNING */
static void test_com_wsw_warning_pack_unpack(void)
{
    com_msg_wsw_warning_t tx_msg = {
        .slip_warning_active = 1,
        .brake_pressed       = 1,
        .warn_lamp           = 1,
        .decel_exceeded      = 1,
        .slip_percent        = 75,
        .alive_counter       = 0,
        .crc8                = 0
    };
    uint8_t payload[8] = {0};
    uint8_t dlc = 0;
    e2e_tx_state_t tx_state = { .alive_counter = 3 };

    TEST_ASSERT_TRUE(com_pack_wsw_warning(&tx_msg, payload, &dlc, &tx_state));
    TEST_ASSERT_EQUAL_UINT8(4, dlc);
    /* Byte 0: bits 0..3 all 1 -> 0x0F */
    TEST_ASSERT_EQUAL_HEX8(0x0F, payload[0]);
    /* Byte 1: 75 % */
    TEST_ASSERT_EQUAL_UINT8(75, payload[1]);
    /* Byte 2: counter = 3 */
    TEST_ASSERT_EQUAL_HEX8(0x03, payload[2]);
    /* Byte 3: CRC calculated */
    TEST_ASSERT_EQUAL_HEX8(e2e_frame_crc(E2E_DATA_ID_WSW_WARNING, payload, 3), payload[3]);
    TEST_ASSERT_EQUAL_UINT8(4, tx_state.alive_counter);

    /* Unpack */
    com_msg_wsw_warning_t rx_msg;
    e2e_rx_state_t rx_state;
    e2e_rx_init(&rx_state);

    e2e_status_t st = com_unpack_wsw_warning(&rx_msg, payload, dlc, &rx_state);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_INITIAL, st);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.slip_warning_active);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.brake_pressed);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.warn_lamp);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.decel_exceeded);
    TEST_ASSERT_EQUAL_UINT8(75, rx_msg.slip_percent);
    TEST_ASSERT_EQUAL_UINT8(3, rx_msg.alive_counter);
    TEST_ASSERT_EQUAL_HEX8(payload[3], rx_msg.crc8);
}

/* Test 2: 0x101 WHEEL_SPEED */
static void test_com_wheel_speed_pack_unpack(void)
{
    float spd = 123.4f;
    float ref = 120.0f;
    com_msg_wheel_speed_t tx_msg = {
        .wheel_speed_raw = com_wheel_speed_rpm_to_raw(spd), /* 1234 */
        .ref_speed_raw   = com_wheel_speed_rpm_to_raw(ref), /* 1200 */
        .brake_pressed   = 1,
        .alive_counter   = 0,
        .crc8            = 0
    };
    TEST_ASSERT_EQUAL_UINT16(1234, tx_msg.wheel_speed_raw);
    TEST_ASSERT_EQUAL_UINT16(1200, tx_msg.ref_speed_raw);

    uint8_t payload[8] = {0};
    uint8_t dlc = 0;
    e2e_tx_state_t tx_state = { .alive_counter = 9 };

    TEST_ASSERT_TRUE(com_pack_wheel_speed(&tx_msg, payload, &dlc, &tx_state));
    TEST_ASSERT_EQUAL_UINT8(7, dlc);

    /* Verify little-endian encoding */
    TEST_ASSERT_EQUAL_HEX8((uint8_t)(1234 & 0xFF), payload[0]);
    TEST_ASSERT_EQUAL_HEX8((uint8_t)((1234 >> 8) & 0xFF), payload[1]);
    TEST_ASSERT_EQUAL_HEX8((uint8_t)(1200 & 0xFF), payload[2]);
    TEST_ASSERT_EQUAL_HEX8((uint8_t)((1200 >> 8) & 0xFF), payload[3]);
    TEST_ASSERT_EQUAL_HEX8(0x01, payload[4]);
    TEST_ASSERT_EQUAL_HEX8(0x09, payload[5]);
    TEST_ASSERT_EQUAL_HEX8(e2e_frame_crc(E2E_DATA_ID_WHEEL_SPEED, payload, 6), payload[6]);

    /* Unpack */
    com_msg_wheel_speed_t rx_msg;
    e2e_rx_state_t rx_state;
    e2e_rx_init(&rx_state);

    e2e_status_t st = com_unpack_wheel_speed(&rx_msg, payload, dlc, &rx_state);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_INITIAL, st);
    TEST_ASSERT_EQUAL_UINT16(1234, rx_msg.wheel_speed_raw);
    TEST_ASSERT_EQUAL_UINT16(1200, rx_msg.ref_speed_raw);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 123.4f, com_wheel_speed_raw_to_rpm(rx_msg.wheel_speed_raw));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 120.0f, com_wheel_speed_raw_to_rpm(rx_msg.ref_speed_raw));
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.brake_pressed);
    TEST_ASSERT_EQUAL_UINT8(9, rx_msg.alive_counter);
}

/* Test 3: 0x102 CLIMATE_STATUS (bit-packed, 12-bit signed temp) */
static void test_com_climate_status_pack_unpack(void)
{
    /* Test negative temperature: -12.5 degC -> raw = -125 */
    float temp_c = -12.5f;
    float hum_pct = 65.5f;
    com_msg_climate_status_t tx_msg = {
        .cabin_temp_raw     = com_cabin_temp_celsius_to_raw(temp_c),     /* -125 */
        .cabin_humidity_raw = com_cabin_humidity_percent_to_raw(hum_pct), /* 655 */
        .sunlight_level     = 185,
        .fan_speed          = 70,
        .flap_angle         = 135,
        .climate_mode       = COM_CLIMATE_MODE_AUTO,
        .dht_fault          = 0,
        .fog_risk           = 1,
        .fan_override       = 1,
        .alive_counter      = 0,
        .crc8               = 0
    };

    uint8_t payload[8] = {0};
    uint8_t dlc = 0;
    e2e_tx_state_t tx_state = { .alive_counter = 12 };

    TEST_ASSERT_TRUE(com_pack_climate_status(&tx_msg, payload, &dlc, &tx_state));
    TEST_ASSERT_EQUAL_UINT8(8, dlc);

    /* Unpack */
    com_msg_climate_status_t rx_msg;
    e2e_rx_state_t rx_state;
    e2e_rx_init(&rx_state);

    e2e_status_t st = com_unpack_climate_status(&rx_msg, payload, dlc, &rx_state);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_INITIAL, st);
    TEST_ASSERT_EQUAL_INT16(-125, rx_msg.cabin_temp_raw);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -12.5f, com_cabin_temp_raw_to_celsius(rx_msg.cabin_temp_raw));
    TEST_ASSERT_EQUAL_UINT16(655, rx_msg.cabin_humidity_raw);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 65.5f, com_cabin_humidity_raw_to_percent(rx_msg.cabin_humidity_raw));
    TEST_ASSERT_EQUAL_UINT8(185, rx_msg.sunlight_level);
    TEST_ASSERT_EQUAL_UINT8(70, rx_msg.fan_speed);
    TEST_ASSERT_EQUAL_UINT8(135, rx_msg.flap_angle);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.climate_mode);
    TEST_ASSERT_EQUAL_UINT8(0, rx_msg.dht_fault);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.fog_risk);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.fan_override);
    TEST_ASSERT_EQUAL_UINT8(12, rx_msg.alive_counter);
    TEST_ASSERT_EQUAL_HEX8(payload[7], rx_msg.crc8);
}

/* Test 4: 0x103 BCM_STATUS */
static void test_com_bcm_status_pack_unpack(void)
{
    com_msg_bcm_status_t tx_msg = {
        .door_open      = 1,
        .lock_state     = COM_LOCK_STATE_LOCKED,
        .headlight_on   = 1,
        .cabinlight_on  = 0,
        .buzzer_on      = 1,
        .pir_motion     = 1,
        .alarm_state    = COM_ALARM_STATE_ALARM, /* 2 */
        .ambient_light  = 210,
        .alive_counter  = 0,
        .crc8           = 0
    };
    uint8_t payload[8] = {0};
    uint8_t dlc = 0;
    e2e_tx_state_t tx_state = { .alive_counter = 7 };

    TEST_ASSERT_TRUE(com_pack_bcm_status(&tx_msg, payload, &dlc, &tx_state));
    TEST_ASSERT_EQUAL_UINT8(4, dlc);

    com_msg_bcm_status_t rx_msg;
    e2e_rx_state_t rx_state;
    e2e_rx_init(&rx_state);

    e2e_status_t st = com_unpack_bcm_status(&rx_msg, payload, dlc, &rx_state);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_INITIAL, st);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.door_open);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.lock_state);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.headlight_on);
    TEST_ASSERT_EQUAL_UINT8(0, rx_msg.cabinlight_on);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.buzzer_on);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.pir_motion);
    TEST_ASSERT_EQUAL_UINT8(COM_ALARM_STATE_ALARM, rx_msg.alarm_state);
    TEST_ASSERT_EQUAL_UINT8(210, rx_msg.ambient_light);
    TEST_ASSERT_EQUAL_UINT8(7, rx_msg.alive_counter);
}

/* Test 5: 0x104 WINDOW_CMD */
static void test_com_window_cmd_pack_unpack(void)
{
    com_msg_window_cmd_t tx_msg = {
        .target_angle = 145,
        .child_lock   = 1,
        .alive_counter = 0,
        .crc8          = 0
    };
    uint8_t payload[8] = {0};
    uint8_t dlc = 0;
    e2e_tx_state_t tx_state = { .alive_counter = 2 };

    TEST_ASSERT_TRUE(com_pack_window_cmd(&tx_msg, payload, &dlc, &tx_state));
    TEST_ASSERT_EQUAL_UINT8(3, dlc);
    TEST_ASSERT_EQUAL_UINT8(145, payload[0]);
    /* Byte 1: counter 2 | (1 << 4) = 0x12 */
    TEST_ASSERT_EQUAL_HEX8(0x12, payload[1]);

    com_msg_window_cmd_t rx_msg;
    e2e_rx_state_t rx_state;
    e2e_rx_init(&rx_state);

    e2e_status_t st = com_unpack_window_cmd(&rx_msg, payload, dlc, &rx_state);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_INITIAL, st);
    TEST_ASSERT_EQUAL_UINT8(145, rx_msg.target_angle);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.child_lock);
    TEST_ASSERT_EQUAL_UINT8(2, rx_msg.alive_counter);
}

/* Test 6: 0x105 CLIMATE_CMD */
static void test_com_climate_cmd_pack_unpack(void)
{
    float temp_sp = 23.5f; /* raw = 23.5 * 2 = 47 */
    com_msg_climate_cmd_t tx_msg = {
        .climate_mode_cmd  = COM_CLIMATE_MODE_AUTO,
        .fan_speed_cmd     = 85,
        .flap_angle_cmd    = 120,
        .temp_setpoint_raw = com_temp_setpoint_celsius_to_raw(temp_sp),
        .alive_counter     = 0,
        .crc8              = 0
    };
    TEST_ASSERT_EQUAL_UINT8(47, tx_msg.temp_setpoint_raw);

    uint8_t payload[8] = {0};
    uint8_t dlc = 0;
    e2e_tx_state_t tx_state = { .alive_counter = 11 };

    TEST_ASSERT_TRUE(com_pack_climate_cmd(&tx_msg, payload, &dlc, &tx_state));
    TEST_ASSERT_EQUAL_UINT8(5, dlc);
    /* Byte 0: mode 1 | (11 << 4) = 0xB1 */
    TEST_ASSERT_EQUAL_HEX8(0xB1, payload[0]);

    com_msg_climate_cmd_t rx_msg;
    e2e_rx_state_t rx_state;
    e2e_rx_init(&rx_state);

    e2e_status_t st = com_unpack_climate_cmd(&rx_msg, payload, dlc, &rx_state);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_INITIAL, st);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.climate_mode_cmd);
    TEST_ASSERT_EQUAL_UINT8(85, rx_msg.fan_speed_cmd);
    TEST_ASSERT_EQUAL_UINT8(120, rx_msg.flap_angle_cmd);
    TEST_ASSERT_EQUAL_UINT8(47, rx_msg.temp_setpoint_raw);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 23.5f, com_temp_setpoint_raw_to_celsius(rx_msg.temp_setpoint_raw));
    TEST_ASSERT_EQUAL_UINT8(11, rx_msg.alive_counter);
}

/* Test 7: 0x106 BCM_CMD */
static void test_com_bcm_cmd_pack_unpack(void)
{
    com_msg_bcm_cmd_t tx_msg = {
        .lock_cmd       = COM_LOCK_CMD_UNLOCK,
        .headlight_cmd  = COM_LIGHT_CMD_ON,
        .cabinlight_cmd = COM_LIGHT_CMD_OFF,
        .buzzer_test    = 1,
        .alive_counter  = 0,
        .crc8           = 0
    };
    uint8_t payload[8] = {0};
    uint8_t dlc = 0;
    e2e_tx_state_t tx_state = { .alive_counter = 4 };

    TEST_ASSERT_TRUE(com_pack_bcm_cmd(&tx_msg, payload, &dlc, &tx_state));
    TEST_ASSERT_EQUAL_UINT8(3, dlc);

    com_msg_bcm_cmd_t rx_msg;
    e2e_rx_state_t rx_state;
    e2e_rx_init(&rx_state);

    e2e_status_t st = com_unpack_bcm_cmd(&rx_msg, payload, dlc, &rx_state);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_INITIAL, st);
    TEST_ASSERT_EQUAL_UINT8(COM_LOCK_CMD_UNLOCK, rx_msg.lock_cmd);
    TEST_ASSERT_EQUAL_UINT8(COM_LIGHT_CMD_ON, rx_msg.headlight_cmd);
    TEST_ASSERT_EQUAL_UINT8(COM_LIGHT_CMD_OFF, rx_msg.cabinlight_cmd);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.buzzer_test);
    TEST_ASSERT_EQUAL_UINT8(4, rx_msg.alive_counter);
}

/* Test 8: 0x110 DMS_ALERT */
static void test_com_dms_alert_pack_unpack(void)
{
    com_msg_dms_alert_t tx_msg = {
        .alert_type   = COM_ALERT_TYPE_DROWSY,
        .severity     = COM_SEVERITY_HIGH,
        .alive_counter = 0,
        .crc8          = 0
    };
    uint8_t payload[8] = {0};
    uint8_t dlc = 0;
    e2e_tx_state_t tx_state = { .alive_counter = 8 };

    TEST_ASSERT_TRUE(com_pack_dms_alert(&tx_msg, payload, &dlc, &tx_state));
    TEST_ASSERT_EQUAL_UINT8(3, dlc);
    /* Byte 0: alert 1 | (2 << 3) = 0x11 */
    TEST_ASSERT_EQUAL_HEX8(0x11, payload[0]);

    com_msg_dms_alert_t rx_msg;
    e2e_rx_state_t rx_state;
    e2e_rx_init(&rx_state);

    e2e_status_t st = com_unpack_dms_alert(&rx_msg, payload, dlc, &rx_state);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_INITIAL, st);
    TEST_ASSERT_EQUAL_UINT8(COM_ALERT_TYPE_DROWSY, rx_msg.alert_type);
    TEST_ASSERT_EQUAL_UINT8(COM_SEVERITY_HIGH, rx_msg.severity);
    TEST_ASSERT_EQUAL_UINT8(8, rx_msg.alive_counter);
}

/* Test 9: 0x111 DMS_STATUS (includes negative signed yaw/pitch) */
static void test_com_dms_status_pack_unpack(void)
{
    float ear = 0.28f; /* raw = 28 */
    com_msg_dms_status_t tx_msg = {
        .face_detected = 1,
        .camera_ok     = 1,
        .ear_value_raw = com_ear_physical_to_raw(ear),
        .head_yaw      = -25, /* signed 8-bit */
        .head_pitch    = 15,
        .dms_fps       = 30,
        .perclos_pct   = 45,
        .alive_counter = 0,
        .crc8          = 0
    };
    uint8_t payload[8] = {0};
    uint8_t dlc = 0;
    e2e_tx_state_t tx_state = { .alive_counter = 1 };

    TEST_ASSERT_TRUE(com_pack_dms_status(&tx_msg, payload, &dlc, &tx_state));
    TEST_ASSERT_EQUAL_UINT8(8, dlc);

    com_msg_dms_status_t rx_msg;
    e2e_rx_state_t rx_state;
    e2e_rx_init(&rx_state);

    e2e_status_t st = com_unpack_dms_status(&rx_msg, payload, dlc, &rx_state);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_INITIAL, st);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.face_detected);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.camera_ok);
    TEST_ASSERT_EQUAL_UINT8(28, rx_msg.ear_value_raw);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.28f, com_ear_raw_to_physical(rx_msg.ear_value_raw));
    TEST_ASSERT_EQUAL_INT8(-25, rx_msg.head_yaw);
    TEST_ASSERT_EQUAL_INT8(15, rx_msg.head_pitch);
    TEST_ASSERT_EQUAL_UINT8(30, rx_msg.dms_fps);
    TEST_ASSERT_EQUAL_UINT8(45, rx_msg.perclos_pct);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.alive_counter);
}

/* Test 10: 0x204 WINDOW_STATUS */
static void test_com_window_status_pack_unpack(void)
{
    com_msg_window_status_t tx_msg = {
        .current_angle      = 90,
        .target_angle_echo  = 90,
        .moving             = 0,
        .pinch_detected     = 1,
        .child_lock_state   = 1,
        .alive_counter      = 0,
        .crc8               = 0
    };
    uint8_t payload[8] = {0};
    uint8_t dlc = 0;
    e2e_tx_state_t tx_state = { .alive_counter = 14 };

    TEST_ASSERT_TRUE(com_pack_window_status(&tx_msg, payload, &dlc, &tx_state));
    TEST_ASSERT_EQUAL_UINT8(4, dlc);
    /* Byte 2: moving(0) | pinch(1<<1) | child_lock(1<<2) | (14<<4) = 2 | 4 | 0xE0 = 0xE6 */
    TEST_ASSERT_EQUAL_HEX8(0xE6, payload[2]);

    com_msg_window_status_t rx_msg;
    e2e_rx_state_t rx_state;
    e2e_rx_init(&rx_state);

    e2e_status_t st = com_unpack_window_status(&rx_msg, payload, dlc, &rx_state);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_INITIAL, st);
    TEST_ASSERT_EQUAL_UINT8(90, rx_msg.current_angle);
    TEST_ASSERT_EQUAL_UINT8(90, rx_msg.target_angle_echo);
    TEST_ASSERT_EQUAL_UINT8(0, rx_msg.moving);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.pinch_detected);
    TEST_ASSERT_EQUAL_UINT8(1, rx_msg.child_lock_state);
    TEST_ASSERT_EQUAL_UINT8(14, rx_msg.alive_counter);
}

/* Test 11: RX Deadline Monitoring / Timeout Supervision */
static void test_com_rx_monitor_wheel_speed_and_status(void)
{
    com_rx_monitor_t mon;
    e2e_rx_state_t e2e;
    e2e_rx_init(&e2e);

    /* WHEEL_SPEED 20 ms nominal -> 60 ms timeout */
    com_rx_monitor_init(&mon, COM_TIMEOUT_WHEEL_SPEED_MS);
    TEST_ASSERT_FALSE(mon.is_started);
    TEST_ASSERT_FALSE(mon.is_timed_out);

    /* First frame received at t = 100 ms */
    com_rx_monitor_on_rx(&mon, 100);
    TEST_ASSERT_TRUE(mon.is_started);

    /* Check at t = 150 ms (elapsed 50 ms < 60 ms) */
    TEST_ASSERT_FALSE(com_rx_monitor_check_timeout(&mon, 150, &e2e));
    TEST_ASSERT_EQUAL_INT(E2E_STATE_VALID, e2e.state);

    /* Check at t = 160 ms (elapsed 60 ms >= 60 ms) -> Timeout! */
    TEST_ASSERT_TRUE(com_rx_monitor_check_timeout(&mon, 160, &e2e));
    TEST_ASSERT_TRUE(mon.is_timed_out);
    TEST_ASSERT_EQUAL_INT(E2E_STATE_INVALID, e2e.state);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_NONEWDATA, e2e.last_status);

    /* Receive fresh frame at t = 200 ms -> clears timeout flag */
    com_rx_monitor_on_rx(&mon, 200);
    TEST_ASSERT_FALSE(mon.is_timed_out);
}

/* Test 12: Corrupted payload detection on unpack */
static void test_com_unpack_corrupted_payload(void)
{
    com_msg_wheel_speed_t tx_msg = { .wheel_speed_raw = 500, .ref_speed_raw = 500 };
    uint8_t payload[8] = {0};
    uint8_t dlc = 0;
    com_pack_wheel_speed(&tx_msg, payload, &dlc, NULL);

    com_msg_wheel_speed_t rx_msg;
    e2e_rx_state_t rx_state;
    e2e_rx_init(&rx_state);

    /* Normal unpack works */
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_INITIAL, com_unpack_wheel_speed(&rx_msg, payload, dlc, &rx_state));

    /* Corrupt byte 0 */
    payload[0] ^= 0xAA;
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_WRONG_CRC, com_unpack_wheel_speed(&rx_msg, payload, dlc, &rx_state));

    /* Corrupt DLC */
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_WRONG_LENGTH, com_unpack_wheel_speed(&rx_msg, payload, 5, &rx_state));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_com_wsw_warning_pack_unpack);
    RUN_TEST(test_com_wheel_speed_pack_unpack);
    RUN_TEST(test_com_climate_status_pack_unpack);
    RUN_TEST(test_com_bcm_status_pack_unpack);
    RUN_TEST(test_com_window_cmd_pack_unpack);
    RUN_TEST(test_com_climate_cmd_pack_unpack);
    RUN_TEST(test_com_bcm_cmd_pack_unpack);
    RUN_TEST(test_com_dms_alert_pack_unpack);
    RUN_TEST(test_com_dms_status_pack_unpack);
    RUN_TEST(test_com_window_status_pack_unpack);
    RUN_TEST(test_com_rx_monitor_wheel_speed_and_status);
    RUN_TEST(test_com_unpack_corrupted_payload);
    return UNITY_END();
}
