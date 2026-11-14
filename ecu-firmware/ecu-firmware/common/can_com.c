/* ==============================================================================
 * can_com.c — AUTOSAR COM & PduR Signal Packing/Unpacking and Deadline Supervision
 * ============================================================================== */
#include "can_com.h"
#include <string.h>

/* ------------------------------------------------------------------------------
 * Chuyển đổi giá trị vật lý (Physical Helpers)
 * ------------------------------------------------------------------------------ */
float com_wheel_speed_raw_to_rpm(uint16_t raw)
{
    return (float)raw * 0.1f;
}

uint16_t com_wheel_speed_rpm_to_raw(float rpm)
{
    if (rpm <= 0.0f) return 0;
    if (rpm >= 6553.5f) return 65535;
    return (uint16_t)((rpm * 10.0f) + 0.5f);
}

float com_cabin_temp_raw_to_celsius(int16_t raw)
{
    return (float)raw * 0.1f;
}

int16_t com_cabin_temp_celsius_to_raw(float celsius)
{
    if (celsius >= 0.0f) {
        return (int16_t)((celsius * 10.0f) + 0.5f);
    } else {
        return (int16_t)((celsius * 10.0f) - 0.5f);
    }
}

float com_cabin_humidity_raw_to_percent(uint16_t raw)
{
    return (float)raw * 0.1f;
}

uint16_t com_cabin_humidity_percent_to_raw(float percent)
{
    if (percent <= 0.0f) return 0;
    if (percent >= 100.0f) return 1000;
    return (uint16_t)((percent * 10.0f) + 0.5f);
}

float com_temp_setpoint_raw_to_celsius(uint8_t raw)
{
    return (float)raw * 0.5f;
}

uint8_t com_temp_setpoint_celsius_to_raw(float celsius)
{
    if (celsius <= 0.0f) return 0;
    if (celsius >= 50.0f) return 100;
    return (uint8_t)((celsius * 2.0f) + 0.5f);
}

float com_ear_raw_to_physical(uint8_t raw)
{
    return (float)raw * 0.01f;
}

uint8_t com_ear_physical_to_raw(float ear)
{
    if (ear <= 0.0f) return 0;
    if (ear >= 2.55f) return 255;
    return (uint8_t)((ear * 100.0f) + 0.5f);
}

/* ------------------------------------------------------------------------------
 * RX Deadline Monitoring
 * ------------------------------------------------------------------------------ */
void com_rx_monitor_init(com_rx_monitor_t *mon, uint32_t timeout_ms)
{
    if (mon != NULL) {
        mon->timeout_ms = timeout_ms;
        mon->last_rx_time_ms = 0;
        mon->is_started = false;
        mon->is_timed_out = false;
    }
}

void com_rx_monitor_on_rx(com_rx_monitor_t *mon, uint32_t current_time_ms)
{
    if (mon != NULL) {
        mon->last_rx_time_ms = current_time_ms;
        mon->is_started = true;
        mon->is_timed_out = false;
    }
}

bool com_rx_monitor_check_timeout(com_rx_monitor_t *mon, uint32_t current_time_ms, e2e_rx_state_t *e2e_state)
{
    if (mon == NULL || !mon->is_started) {
        return false;
    }
    uint32_t elapsed = current_time_ms - mon->last_rx_time_ms;
    if (elapsed >= mon->timeout_ms) {
        mon->is_timed_out = true;
        if (e2e_state != NULL) {
            e2e_notify_timeout(e2e_state);
        }
        return true;
    }
    return false;
}

/* ------------------------------------------------------------------------------
 * Helper nội bộ cho Pack & Unpack E2E
 * ------------------------------------------------------------------------------ */
static uint8_t get_and_inc_tx_counter(const uint8_t msg_counter, e2e_tx_state_t *tx_state)
{
    if (tx_state != NULL) {
        uint8_t cnt = tx_state->alive_counter & 0x0Fu;
        tx_state->alive_counter = (uint8_t)((cnt + 1u) & 0x0Fu);
        return cnt;
    }
    return (uint8_t)(msg_counter & 0x0Fu);
}

static e2e_status_t verify_rx_e2e(uint8_t data_id, uint8_t expected_dlc,
                                 const uint8_t *payload, uint8_t dlc,
                                 uint8_t counter, e2e_rx_state_t *rx_state)
{
    const e2e_config_t cfg = {
        .data_id = data_id,
        .dlc = expected_dlc,
        .max_delta_counter = 3u
    };

    if (rx_state != NULL) {
        return e2e_check_counter(&cfg, rx_state, payload, dlc, counter);
    }

    if (payload == NULL || dlc != expected_dlc) {
        return E2E_STATUS_WRONG_LENGTH;
    }
    uint8_t crc = e2e_frame_crc(data_id, payload, (size_t)(expected_dlc - 1));
    if (crc != payload[expected_dlc - 1]) {
        return E2E_STATUS_WRONG_CRC;
    }
    return E2E_STATUS_OK;
}

/* ------------------------------------------------------------------------------
 * 0x100 — WSW_WARNING (DLC = 4, DataID = 0x3C)
 * ------------------------------------------------------------------------------ */
bool com_pack_wsw_warning(const com_msg_wsw_warning_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state)
{
    if (msg == NULL || payload == NULL) return false;

    uint8_t cnt = get_and_inc_tx_counter(msg->alive_counter, tx_state);

    payload[0] = (uint8_t)((msg->slip_warning_active & 0x01u) |
                          ((msg->brake_pressed & 0x01u) << 1) |
                          ((msg->warn_lamp & 0x01u) << 2) |
                          ((msg->decel_exceeded & 0x01u) << 3));
    payload[1] = msg->slip_percent;
    payload[2] = (uint8_t)(cnt & 0x0Fu);
    payload[3] = e2e_frame_crc(E2E_DATA_ID_WSW_WARNING, payload, 3);

    if (dlc != NULL) *dlc = CAN_DLC_WSW_WARNING;
    return true;
}

e2e_status_t com_unpack_wsw_warning(com_msg_wsw_warning_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state)
{
    if (msg == NULL || payload == NULL) return E2E_STATUS_WRONG_LENGTH;

    uint8_t counter = (dlc >= 3) ? (payload[2] & 0x0Fu) : 0u;
    e2e_status_t st = verify_rx_e2e(E2E_DATA_ID_WSW_WARNING, CAN_DLC_WSW_WARNING, payload, dlc, counter, rx_state);
    if (st != E2E_STATUS_INITIAL && st != E2E_STATUS_OK && st != E2E_STATUS_OK_SOME_LOST) {
        return st;
    }

    msg->slip_warning_active = payload[0] & 0x01u;
    msg->brake_pressed       = (payload[0] >> 1) & 0x01u;
    msg->warn_lamp           = (payload[0] >> 2) & 0x01u;
    msg->decel_exceeded      = (payload[0] >> 3) & 0x01u;
    msg->slip_percent        = payload[1];
    msg->alive_counter       = counter;
    msg->crc8                = payload[3];
    return st;
}

/* ------------------------------------------------------------------------------
 * 0x101 — WHEEL_SPEED (DLC = 7, DataID = 0x5A)
 * ------------------------------------------------------------------------------ */
bool com_pack_wheel_speed(const com_msg_wheel_speed_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state)
{
    if (msg == NULL || payload == NULL) return false;

    uint8_t cnt = get_and_inc_tx_counter(msg->alive_counter, tx_state);

    payload[0] = (uint8_t)(msg->wheel_speed_raw & 0xFFu);
    payload[1] = (uint8_t)((msg->wheel_speed_raw >> 8) & 0xFFu);
    payload[2] = (uint8_t)(msg->ref_speed_raw & 0xFFu);
    payload[3] = (uint8_t)((msg->ref_speed_raw >> 8) & 0xFFu);
    payload[4] = (uint8_t)(msg->brake_pressed & 0x01u);
    payload[5] = (uint8_t)(cnt & 0x0Fu);
    payload[6] = e2e_frame_crc(E2E_DATA_ID_WHEEL_SPEED, payload, 6);

    if (dlc != NULL) *dlc = CAN_DLC_WHEEL_SPEED;
    return true;
}

e2e_status_t com_unpack_wheel_speed(com_msg_wheel_speed_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state)
{
    if (msg == NULL || payload == NULL) return E2E_STATUS_WRONG_LENGTH;

    uint8_t counter = (dlc >= 6) ? (payload[5] & 0x0Fu) : 0u;
    e2e_status_t st = verify_rx_e2e(E2E_DATA_ID_WHEEL_SPEED, CAN_DLC_WHEEL_SPEED, payload, dlc, counter, rx_state);
    if (st != E2E_STATUS_INITIAL && st != E2E_STATUS_OK && st != E2E_STATUS_OK_SOME_LOST) {
        return st;
    }

    msg->wheel_speed_raw = (uint16_t)(((uint16_t)payload[0]) | (((uint16_t)payload[1]) << 8));
    msg->ref_speed_raw   = (uint16_t)(((uint16_t)payload[2]) | (((uint16_t)payload[3]) << 8));
    msg->brake_pressed   = payload[4] & 0x01u;
    msg->alive_counter   = counter;
    msg->crc8            = payload[6];
    return st;
}

/* ------------------------------------------------------------------------------
 * 0x102 — CLIMATE_STATUS (DLC = 8, DataID = 0x96)
 * ------------------------------------------------------------------------------ */
bool com_pack_climate_status(const com_msg_climate_status_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state)
{
    if (msg == NULL || payload == NULL) return false;

    uint8_t cnt = get_and_inc_tx_counter(msg->alive_counter, tx_state);

    uint16_t temp12 = (uint16_t)(msg->cabin_temp_raw & 0x0FFFu);
    uint16_t hum10  = (uint16_t)(msg->cabin_humidity_raw & 0x03FFu);

    payload[0] = (uint8_t)(temp12 & 0xFFu);
    payload[1] = (uint8_t)(((temp12 >> 8) & 0x0Fu) | ((hum10 & 0x0Fu) << 4));
    payload[2] = (uint8_t)(((hum10 >> 4) & 0x3Fu) | ((msg->sunlight_level & 0x03u) << 6));
    payload[3] = (uint8_t)(((msg->sunlight_level >> 2) & 0x3Fu) | ((msg->fan_speed & 0x03u) << 6));
    payload[4] = (uint8_t)(((msg->fan_speed >> 2) & 0x1Fu) | ((msg->flap_angle & 0x07u) << 5));
    payload[5] = (uint8_t)(((msg->flap_angle >> 3) & 0x1Fu) |
                          ((msg->climate_mode & 0x01u) << 5) |
                          ((msg->dht_fault & 0x01u) << 6) |
                          ((msg->fog_risk & 0x01u) << 7));
    payload[6] = (uint8_t)((cnt & 0x0Fu) | ((msg->fan_override & 0x01u) << 4));
    payload[7] = e2e_frame_crc(E2E_DATA_ID_CLIMATE_STATUS, payload, 7);

    if (dlc != NULL) *dlc = CAN_DLC_CLIMATE_STATUS;
    return true;
}

e2e_status_t com_unpack_climate_status(com_msg_climate_status_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state)
{
    if (msg == NULL || payload == NULL) return E2E_STATUS_WRONG_LENGTH;

    uint8_t counter = (dlc >= 7) ? (payload[6] & 0x0Fu) : 0u;
    e2e_status_t st = verify_rx_e2e(E2E_DATA_ID_CLIMATE_STATUS, CAN_DLC_CLIMATE_STATUS, payload, dlc, counter, rx_state);
    if (st != E2E_STATUS_INITIAL && st != E2E_STATUS_OK && st != E2E_STATUS_OK_SOME_LOST) {
        return st;
    }

    /* Giữ dấu 12-bit signed: bit 11 là bit dấu */
    int16_t t = (int16_t)(((uint16_t)payload[0]) | (((uint16_t)(payload[1] & 0x0Fu)) << 8));
    if (t & 0x0800) {
        t = (int16_t)(t | 0xF000);
    }
    msg->cabin_temp_raw     = t;
    msg->cabin_humidity_raw = (uint16_t)(((payload[1] >> 4) & 0x0Fu) | (((uint16_t)(payload[2] & 0x3Fu)) << 4));
    msg->sunlight_level     = (uint8_t)(((payload[2] >> 6) & 0x03u) | ((payload[3] & 0x3Fu) << 2));
    msg->fan_speed          = (uint8_t)(((payload[3] >> 6) & 0x03u) | ((payload[4] & 0x1Fu) << 2));
    msg->flap_angle         = (uint8_t)(((payload[4] >> 5) & 0x07u) | ((payload[5] & 0x1Fu) << 3));
    msg->climate_mode       = (payload[5] >> 5) & 0x01u;
    msg->dht_fault          = (payload[5] >> 6) & 0x01u;
    msg->fog_risk           = (payload[5] >> 7) & 0x01u;
    msg->alive_counter      = counter;
    msg->fan_override       = (payload[6] >> 4) & 0x01u;
    msg->crc8               = payload[7];
    return st;
}

/* ------------------------------------------------------------------------------
 * 0x103 — BCM_STATUS (DLC = 4, DataID = 0xC3)
 * ------------------------------------------------------------------------------ */
bool com_pack_bcm_status(const com_msg_bcm_status_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state)
{
    if (msg == NULL || payload == NULL) return false;

    uint8_t cnt = get_and_inc_tx_counter(msg->alive_counter, tx_state);

    payload[0] = (uint8_t)((msg->door_open & 0x01u) |
                          ((msg->lock_state & 0x01u) << 1) |
                          ((msg->headlight_on & 0x01u) << 2) |
                          ((msg->cabinlight_on & 0x01u) << 3) |
                          ((msg->buzzer_on & 0x01u) << 4) |
                          ((msg->pir_motion & 0x01u) << 5) |
                          ((msg->alarm_state & 0x03u) << 6));
    payload[1] = msg->ambient_light;
    payload[2] = (uint8_t)(cnt & 0x0Fu);
    payload[3] = e2e_frame_crc(E2E_DATA_ID_BCM_STATUS, payload, 3);

    if (dlc != NULL) *dlc = CAN_DLC_BCM_STATUS;
    return true;
}

e2e_status_t com_unpack_bcm_status(com_msg_bcm_status_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state)
{
    if (msg == NULL || payload == NULL) return E2E_STATUS_WRONG_LENGTH;

    uint8_t counter = (dlc >= 3) ? (payload[2] & 0x0Fu) : 0u;
    e2e_status_t st = verify_rx_e2e(E2E_DATA_ID_BCM_STATUS, CAN_DLC_BCM_STATUS, payload, dlc, counter, rx_state);
    if (st != E2E_STATUS_INITIAL && st != E2E_STATUS_OK && st != E2E_STATUS_OK_SOME_LOST) {
        return st;
    }

    msg->door_open     = payload[0] & 0x01u;
    msg->lock_state    = (payload[0] >> 1) & 0x01u;
    msg->headlight_on  = (payload[0] >> 2) & 0x01u;
    msg->cabinlight_on = (payload[0] >> 3) & 0x01u;
    msg->buzzer_on     = (payload[0] >> 4) & 0x01u;
    msg->pir_motion    = (payload[0] >> 5) & 0x01u;
    msg->alarm_state   = (payload[0] >> 6) & 0x03u;
    msg->ambient_light = payload[1];
    msg->alive_counter = counter;
    msg->crc8          = payload[3];
    return st;
}

/* ------------------------------------------------------------------------------
 * 0x104 — WINDOW_CMD (DLC = 3, DataID = 0x69)
 * ------------------------------------------------------------------------------ */
bool com_pack_window_cmd(const com_msg_window_cmd_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state)
{
    if (msg == NULL || payload == NULL) return false;

    uint8_t cnt = get_and_inc_tx_counter(msg->alive_counter, tx_state);

    payload[0] = msg->target_angle;
    payload[1] = (uint8_t)((cnt & 0x0Fu) | ((msg->child_lock & 0x01u) << 4));
    payload[2] = e2e_frame_crc(E2E_DATA_ID_WINDOW_CMD, payload, 2);

    if (dlc != NULL) *dlc = CAN_DLC_WINDOW_CMD;
    return true;
}

e2e_status_t com_unpack_window_cmd(com_msg_window_cmd_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state)
{
    if (msg == NULL || payload == NULL) return E2E_STATUS_WRONG_LENGTH;

    uint8_t counter = (dlc >= 2) ? (payload[1] & 0x0Fu) : 0u;
    e2e_status_t st = verify_rx_e2e(E2E_DATA_ID_WINDOW_CMD, CAN_DLC_WINDOW_CMD, payload, dlc, counter, rx_state);
    if (st != E2E_STATUS_INITIAL && st != E2E_STATUS_OK && st != E2E_STATUS_OK_SOME_LOST) {
        return st;
    }

    msg->target_angle  = payload[0];
    msg->alive_counter = counter;
    msg->child_lock    = (payload[1] >> 4) & 0x01u;
    msg->crc8          = payload[2];
    return st;
}

/* ------------------------------------------------------------------------------
 * 0x105 — CLIMATE_CMD (DLC = 5, DataID = 0xA5)
 * ------------------------------------------------------------------------------ */
bool com_pack_climate_cmd(const com_msg_climate_cmd_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state)
{
    if (msg == NULL || payload == NULL) return false;

    uint8_t cnt = get_and_inc_tx_counter(msg->alive_counter, tx_state);

    payload[0] = (uint8_t)((msg->climate_mode_cmd & 0x01u) | ((cnt & 0x0Fu) << 4));
    payload[1] = msg->fan_speed_cmd;
    payload[2] = msg->flap_angle_cmd;
    payload[3] = msg->temp_setpoint_raw;
    payload[4] = e2e_frame_crc(E2E_DATA_ID_CLIMATE_CMD, payload, 4);

    if (dlc != NULL) *dlc = CAN_DLC_CLIMATE_CMD;
    return true;
}

e2e_status_t com_unpack_climate_cmd(com_msg_climate_cmd_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state)
{
    if (msg == NULL || payload == NULL) return E2E_STATUS_WRONG_LENGTH;

    uint8_t counter = (dlc >= 1) ? ((payload[0] >> 4) & 0x0Fu) : 0u;
    e2e_status_t st = verify_rx_e2e(E2E_DATA_ID_CLIMATE_CMD, CAN_DLC_CLIMATE_CMD, payload, dlc, counter, rx_state);
    if (st != E2E_STATUS_INITIAL && st != E2E_STATUS_OK && st != E2E_STATUS_OK_SOME_LOST) {
        return st;
    }

    msg->climate_mode_cmd  = payload[0] & 0x01u;
    msg->alive_counter     = counter;
    msg->fan_speed_cmd     = payload[1];
    msg->flap_angle_cmd    = payload[2];
    msg->temp_setpoint_raw = payload[3];
    msg->crc8              = payload[4];
    return st;
}

/* ------------------------------------------------------------------------------
 * 0x106 — BCM_CMD (DLC = 3, DataID = 0x0F)
 * ------------------------------------------------------------------------------ */
bool com_pack_bcm_cmd(const com_msg_bcm_cmd_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state)
{
    if (msg == NULL || payload == NULL) return false;

    uint8_t cnt = get_and_inc_tx_counter(msg->alive_counter, tx_state);

    payload[0] = (uint8_t)((msg->lock_cmd & 0x03u) |
                          ((msg->headlight_cmd & 0x03u) << 2) |
                          ((msg->cabinlight_cmd & 0x03u) << 4) |
                          ((msg->buzzer_test & 0x01u) << 6));
    payload[1] = (uint8_t)(cnt & 0x0Fu);
    payload[2] = e2e_frame_crc(E2E_DATA_ID_BCM_CMD, payload, 2);

    if (dlc != NULL) *dlc = CAN_DLC_BCM_CMD;
    return true;
}

e2e_status_t com_unpack_bcm_cmd(com_msg_bcm_cmd_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state)
{
    if (msg == NULL || payload == NULL) return E2E_STATUS_WRONG_LENGTH;

    uint8_t counter = (dlc >= 2) ? (payload[1] & 0x0Fu) : 0u;
    e2e_status_t st = verify_rx_e2e(E2E_DATA_ID_BCM_CMD, CAN_DLC_BCM_CMD, payload, dlc, counter, rx_state);
    if (st != E2E_STATUS_INITIAL && st != E2E_STATUS_OK && st != E2E_STATUS_OK_SOME_LOST) {
        return st;
    }

    msg->lock_cmd       = payload[0] & 0x03u;
    msg->headlight_cmd  = (payload[0] >> 2) & 0x03u;
    msg->cabinlight_cmd = (payload[0] >> 4) & 0x03u;
    msg->buzzer_test    = (payload[0] >> 6) & 0x01u;
    msg->alive_counter  = counter;
    msg->crc8           = payload[2];
    return st;
}

/* ------------------------------------------------------------------------------
 * 0x110 — DMS_ALERT (DLC = 3, DataID = 0xF0)
 * ------------------------------------------------------------------------------ */
bool com_pack_dms_alert(const com_msg_dms_alert_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state)
{
    if (msg == NULL || payload == NULL) return false;

    uint8_t cnt = get_and_inc_tx_counter(msg->alive_counter, tx_state);

    payload[0] = (uint8_t)((msg->alert_type & 0x07u) | ((msg->severity & 0x03u) << 3));
    payload[1] = (uint8_t)(cnt & 0x0Fu);
    payload[2] = e2e_frame_crc(E2E_DATA_ID_DMS_ALERT, payload, 2);

    if (dlc != NULL) *dlc = CAN_DLC_DMS_ALERT;
    return true;
}

e2e_status_t com_unpack_dms_alert(com_msg_dms_alert_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state)
{
    if (msg == NULL || payload == NULL) return E2E_STATUS_WRONG_LENGTH;

    uint8_t counter = (dlc >= 2) ? (payload[1] & 0x0Fu) : 0u;
    e2e_status_t st = verify_rx_e2e(E2E_DATA_ID_DMS_ALERT, CAN_DLC_DMS_ALERT, payload, dlc, counter, rx_state);
    if (st != E2E_STATUS_INITIAL && st != E2E_STATUS_OK && st != E2E_STATUS_OK_SOME_LOST) {
        return st;
    }

    msg->alert_type    = payload[0] & 0x07u;
    msg->severity      = (payload[0] >> 3) & 0x03u;
    msg->alive_counter = counter;
    msg->crc8          = payload[2];
    return st;
}

/* ------------------------------------------------------------------------------
 * 0x111 — DMS_STATUS (DLC = 8, DataID = 0x99)
 * ------------------------------------------------------------------------------ */
bool com_pack_dms_status(const com_msg_dms_status_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state)
{
    if (msg == NULL || payload == NULL) return false;

    uint8_t cnt = get_and_inc_tx_counter(msg->alive_counter, tx_state);

    payload[0] = (uint8_t)((msg->face_detected & 0x01u) | ((msg->camera_ok & 0x01u) << 1));
    payload[1] = msg->ear_value_raw;
    payload[2] = (uint8_t)msg->head_yaw;
    payload[3] = (uint8_t)msg->head_pitch;
    payload[4] = msg->dms_fps;
    payload[5] = msg->perclos_pct;
    payload[6] = (uint8_t)(cnt & 0x0Fu);
    payload[7] = e2e_frame_crc(E2E_DATA_ID_DMS_STATUS, payload, 7);

    if (dlc != NULL) *dlc = CAN_DLC_DMS_STATUS;
    return true;
}

e2e_status_t com_unpack_dms_status(com_msg_dms_status_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state)
{
    if (msg == NULL || payload == NULL) return E2E_STATUS_WRONG_LENGTH;

    uint8_t counter = (dlc >= 7) ? (payload[6] & 0x0Fu) : 0u;
    e2e_status_t st = verify_rx_e2e(E2E_DATA_ID_DMS_STATUS, CAN_DLC_DMS_STATUS, payload, dlc, counter, rx_state);
    if (st != E2E_STATUS_INITIAL && st != E2E_STATUS_OK && st != E2E_STATUS_OK_SOME_LOST) {
        return st;
    }

    msg->face_detected = payload[0] & 0x01u;
    msg->camera_ok     = (payload[0] >> 1) & 0x01u;
    msg->ear_value_raw = payload[1];
    msg->head_yaw      = (int8_t)payload[2];
    msg->head_pitch    = (int8_t)payload[3];
    msg->dms_fps       = payload[4];
    msg->perclos_pct   = payload[5];
    msg->alive_counter = counter;
    msg->crc8          = payload[7];
    return st;
}

/* ------------------------------------------------------------------------------
 * 0x204 — WINDOW_STATUS (DLC = 4, DataID = 0x33)
 * ------------------------------------------------------------------------------ */
bool com_pack_window_status(const com_msg_window_status_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state)
{
    if (msg == NULL || payload == NULL) return false;

    uint8_t cnt = get_and_inc_tx_counter(msg->alive_counter, tx_state);

    payload[0] = msg->current_angle;
    payload[1] = msg->target_angle_echo;
    payload[2] = (uint8_t)((msg->moving & 0x01u) |
                          ((msg->pinch_detected & 0x01u) << 1) |
                          ((msg->child_lock_state & 0x01u) << 2) |
                          ((cnt & 0x0Fu) << 4));
    payload[3] = e2e_frame_crc(E2E_DATA_ID_WINDOW_STATUS, payload, 3);

    if (dlc != NULL) *dlc = CAN_DLC_WINDOW_STATUS;
    return true;
}

e2e_status_t com_unpack_window_status(com_msg_window_status_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state)
{
    if (msg == NULL || payload == NULL) return E2E_STATUS_WRONG_LENGTH;

    uint8_t counter = (dlc >= 3) ? ((payload[2] >> 4) & 0x0Fu) : 0u;
    e2e_status_t st = verify_rx_e2e(E2E_DATA_ID_WINDOW_STATUS, CAN_DLC_WINDOW_STATUS, payload, dlc, counter, rx_state);
    if (st != E2E_STATUS_INITIAL && st != E2E_STATUS_OK && st != E2E_STATUS_OK_SOME_LOST) {
        return st;
    }

    msg->current_angle      = payload[0];
    msg->target_angle_echo  = payload[1];
    msg->moving             = payload[2] & 0x01u;
    msg->pinch_detected     = (payload[2] >> 1) & 0x01u;
    msg->child_lock_state   = (payload[2] >> 2) & 0x01u;
    msg->alive_counter      = counter;
    msg->crc8               = payload[3];
    return st;
}
