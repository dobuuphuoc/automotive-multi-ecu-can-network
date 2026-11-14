/* ==============================================================================
 * can_com.h — AUTOSAR COM & PduR Signal Packing/Unpacking and Deadline Supervision
 * Khớp chuẩn file đặc tả can_matrix.dbc (v0.5) và doc/CAN-byte-layout.md
 * ============================================================================== */
#ifndef CAN_COM_H
#define CAN_COM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "e2e_crc8.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------------
 * CAN IDs (Chuẩn 11-bit)
 * ------------------------------------------------------------------------------ */
#define CAN_ID_WSW_WARNING      0x100u
#define CAN_ID_WHEEL_SPEED      0x101u
#define CAN_ID_CLIMATE_STATUS   0x102u
#define CAN_ID_BCM_STATUS       0x103u
#define CAN_ID_WINDOW_CMD       0x104u
#define CAN_ID_CLIMATE_CMD      0x105u
#define CAN_ID_BCM_CMD          0x106u
#define CAN_ID_DMS_ALERT        0x110u
#define CAN_ID_DMS_STATUS       0x111u
#define CAN_ID_WINDOW_STATUS    0x204u

/* ------------------------------------------------------------------------------
 * E2E DataID theo DBC (Thuộc tính E2E_DataID)
 * ------------------------------------------------------------------------------ */
#define E2E_DATA_ID_WSW_WARNING     0x3Cu  /* 60 */
#define E2E_DATA_ID_WHEEL_SPEED     0x5Au  /* 90 */
#define E2E_DATA_ID_CLIMATE_STATUS  0x96u  /* 150 */
#define E2E_DATA_ID_BCM_STATUS      0xC3u  /* 195 */
#define E2E_DATA_ID_WINDOW_CMD      0x69u  /* 105 */
#define E2E_DATA_ID_CLIMATE_CMD     0xA5u  /* 165 */
#define E2E_DATA_ID_BCM_CMD         0x0Fu  /* 15 */
#define E2E_DATA_ID_DMS_ALERT       0xF0u  /* 240 */
#define E2E_DATA_ID_DMS_STATUS      0x99u  /* 153 */
#define E2E_DATA_ID_WINDOW_STATUS   0x33u  /* 51 */

/* ------------------------------------------------------------------------------
 * DLC cố định từng frame
 * ------------------------------------------------------------------------------ */
#define CAN_DLC_WSW_WARNING     4u
#define CAN_DLC_WHEEL_SPEED     7u
#define CAN_DLC_CLIMATE_STATUS  8u
#define CAN_DLC_BCM_STATUS      4u
#define CAN_DLC_WINDOW_CMD      3u
#define CAN_DLC_CLIMATE_CMD     5u
#define CAN_DLC_BCM_CMD         3u
#define CAN_DLC_DMS_ALERT       3u
#define CAN_DLC_DMS_STATUS      8u
#define CAN_DLC_WINDOW_STATUS   4u

/* ------------------------------------------------------------------------------
 * Chu kỳ danh định và Deadline Timeout (3x chu kỳ)
 * ------------------------------------------------------------------------------ */
#define CAN_CYCLE_WHEEL_SPEED_MS        20u
#define CAN_CYCLE_CLIMATE_STATUS_MS    100u
#define CAN_CYCLE_BCM_STATUS_MS        100u
#define CAN_CYCLE_DMS_STATUS_MS        100u
#define CAN_CYCLE_WINDOW_STATUS_MS     100u
#define CAN_CYCLE_WSW_WARNING_MS       100u

#define COM_TIMEOUT_WHEEL_SPEED_MS      60u   /* 3 x 20 ms */
#define COM_TIMEOUT_CLIMATE_STATUS_MS  300u   /* 3 x 100 ms */
#define COM_TIMEOUT_BCM_STATUS_MS      300u   /* 3 x 100 ms */
#define COM_TIMEOUT_DMS_STATUS_MS      300u   /* 3 x 100 ms */
#define COM_TIMEOUT_WINDOW_STATUS_MS   300u   /* 3 x 100 ms */
#define COM_TIMEOUT_WSW_WARNING_MS     300u   /* 3 x 100 ms */
#define COM_TIMEOUT_DMS_ALERT_BCM_MS   500u   /* Timeout an toàn của BCM cho DMS_ALERT */

/* ------------------------------------------------------------------------------
 * Định nghĩa giá trị liệt kê của các tín hiệu (VAL_ trong DBC)
 * ------------------------------------------------------------------------------ */
typedef enum {
    COM_CLIMATE_MODE_MANUAL = 0,
    COM_CLIMATE_MODE_AUTO   = 1
} com_climate_mode_t;

typedef enum {
    COM_LOCK_STATE_UNLOCKED = 0,
    COM_LOCK_STATE_LOCKED   = 1
} com_lock_state_t;

typedef enum {
    COM_ALARM_STATE_DISARMED = 0,
    COM_ALARM_STATE_ARMED    = 1,
    COM_ALARM_STATE_ALARM    = 2
} com_alarm_state_t;

typedef enum {
    COM_LOCK_CMD_NO_ACTION = 0,
    COM_LOCK_CMD_LOCK      = 1,
    COM_LOCK_CMD_UNLOCK    = 2
} com_lock_cmd_t;

typedef enum {
    COM_LIGHT_CMD_AUTO = 0,
    COM_LIGHT_CMD_OFF  = 1,
    COM_LIGHT_CMD_ON   = 2
} com_light_cmd_t;

typedef enum {
    COM_ALERT_TYPE_NONE       = 0,
    COM_ALERT_TYPE_DROWSY     = 1,
    COM_ALERT_TYPE_DISTRACTED = 2,
    COM_ALERT_TYPE_NO_FACE    = 3
} com_alert_type_t;

typedef enum {
    COM_SEVERITY_NONE     = 0,
    COM_SEVERITY_LOW      = 1,
    COM_SEVERITY_HIGH     = 2,
    COM_SEVERITY_CRITICAL = 3
} com_severity_t;

/* ------------------------------------------------------------------------------
 * Cấu trúc thông điệp (PDU Signals) cho 10 frame
 * ------------------------------------------------------------------------------ */

/* 0x100 WSW_WARNING */
typedef struct {
    uint8_t slip_warning_active; /* 0..1 */
    uint8_t brake_pressed;       /* 0..1 */
    uint8_t warn_lamp;           /* 0..1 */
    uint8_t decel_exceeded;      /* 0..1 */
    uint8_t slip_percent;        /* 0..100 % */
    uint8_t alive_counter;       /* 0..15 */
    uint8_t crc8;
} com_msg_wsw_warning_t;

/* 0x101 WHEEL_SPEED */
typedef struct {
    uint16_t wheel_speed_raw;    /* factor 0.1 rpm: 0..6553.5 rpm */
    uint16_t ref_speed_raw;      /* factor 0.1 rpm: 0..6553.5 rpm */
    uint8_t  brake_pressed;      /* 0..1 */
    uint8_t  alive_counter;      /* 0..15 */
    uint8_t  crc8;
} com_msg_wheel_speed_t;

/* 0x102 CLIMATE_STATUS */
typedef struct {
    int16_t  cabin_temp_raw;     /* 12-bit signed, factor 0.1 degC: -40..125 degC */
    uint16_t cabin_humidity_raw; /* 10-bit unsigned, factor 0.1 %: 0..100 % */
    uint8_t  sunlight_level;     /* 0..255 */
    uint8_t  fan_speed;          /* 0..100 % */
    uint8_t  flap_angle;         /* 0..180 deg */
    uint8_t  climate_mode;       /* 0=Manual, 1=Auto */
    uint8_t  dht_fault;          /* 0..1 */
    uint8_t  fog_risk;           /* 0..1 */
    uint8_t  fan_override;       /* 0..1 */
    uint8_t  alive_counter;      /* 0..15 */
    uint8_t  crc8;
} com_msg_climate_status_t;

/* 0x103 BCM_STATUS */
typedef struct {
    uint8_t door_open;           /* 0..1 */
    uint8_t lock_state;          /* 0=Unlocked, 1=Locked */
    uint8_t headlight_on;        /* 0..1 */
    uint8_t cabinlight_on;       /* 0..1 */
    uint8_t buzzer_on;           /* 0..1 */
    uint8_t pir_motion;          /* 0..1 */
    uint8_t alarm_state;         /* 0=Disarmed, 1=Armed, 2=Alarm */
    uint8_t ambient_light;       /* 0..255 */
    uint8_t alive_counter;       /* 0..15 */
    uint8_t crc8;
} com_msg_bcm_status_t;

/* 0x104 WINDOW_CMD */
typedef struct {
    uint8_t target_angle;        /* 0..180 deg */
    uint8_t child_lock;          /* 0..1 */
    uint8_t alive_counter;       /* 0..15 */
    uint8_t crc8;
} com_msg_window_cmd_t;

/* 0x105 CLIMATE_CMD */
typedef struct {
    uint8_t climate_mode_cmd;    /* 0=Manual, 1=Auto */
    uint8_t fan_speed_cmd;       /* 0..100 % */
    uint8_t flap_angle_cmd;      /* 0..180 deg */
    uint8_t temp_setpoint_raw;   /* factor 0.5: 0..50 degC */
    uint8_t alive_counter;       /* 0..15 */
    uint8_t crc8;
} com_msg_climate_cmd_t;

/* 0x106 BCM_CMD */
typedef struct {
    uint8_t lock_cmd;            /* 0=No_Action, 1=Lock, 2=Unlock */
    uint8_t headlight_cmd;       /* 0=Auto, 1=Off, 2=On */
    uint8_t cabinlight_cmd;      /* 0=Auto, 1=Off, 2=On */
    uint8_t buzzer_test;         /* 0..1 */
    uint8_t alive_counter;       /* 0..15 */
    uint8_t crc8;
} com_msg_bcm_cmd_t;

/* 0x110 DMS_ALERT */
typedef struct {
    uint8_t alert_type;          /* 0=None, 1=Drowsy, 2=Distracted, 3=No_Face */
    uint8_t severity;            /* 0=None, 1=Low, 2=High, 3=Critical */
    uint8_t alive_counter;       /* 0..15 */
    uint8_t crc8;
} com_msg_dms_alert_t;

/* 0x111 DMS_STATUS */
typedef struct {
    uint8_t face_detected;       /* 0..1 */
    uint8_t camera_ok;           /* 0..1 */
    uint8_t ear_value_raw;       /* factor 0.01: 0..2.55 */
    int8_t  head_yaw;            /* factor 1: -90..90 deg */
    int8_t  head_pitch;          /* factor 1: -90..90 deg */
    uint8_t dms_fps;             /* 0..255 fps */
    uint8_t perclos_pct;         /* 0..100 % */
    uint8_t alive_counter;       /* 0..15 */
    uint8_t crc8;
} com_msg_dms_status_t;

/* 0x204 WINDOW_STATUS */
typedef struct {
    uint8_t current_angle;       /* 0..180 deg */
    uint8_t target_angle_echo;   /* 0..180 deg */
    uint8_t moving;              /* 0..1 */
    uint8_t pinch_detected;      /* 0..1 */
    uint8_t child_lock_state;    /* 0..1 */
    uint8_t alive_counter;       /* 0..15 */
    uint8_t crc8;
} com_msg_window_status_t;

/* ------------------------------------------------------------------------------
 * Chuyển đổi giá trị vật lý (Physical Helpers)
 * ------------------------------------------------------------------------------ */
float    com_wheel_speed_raw_to_rpm(uint16_t raw);
uint16_t com_wheel_speed_rpm_to_raw(float rpm);

float   com_cabin_temp_raw_to_celsius(int16_t raw);
int16_t com_cabin_temp_celsius_to_raw(float celsius);

float    com_cabin_humidity_raw_to_percent(uint16_t raw);
uint16_t com_cabin_humidity_percent_to_raw(float percent);

float   com_temp_setpoint_raw_to_celsius(uint8_t raw);
uint8_t com_temp_setpoint_celsius_to_raw(float celsius);

float   com_ear_raw_to_physical(uint8_t raw);
uint8_t com_ear_physical_to_raw(float ear);

/* ------------------------------------------------------------------------------
 * RX Deadline Monitoring (Timeout Supervision)
 * ------------------------------------------------------------------------------ */
typedef struct {
    uint32_t timeout_ms;
    uint32_t last_rx_time_ms;
    bool is_started;
    bool is_timed_out;
} com_rx_monitor_t;

void com_rx_monitor_init(com_rx_monitor_t *mon, uint32_t timeout_ms);
void com_rx_monitor_on_rx(com_rx_monitor_t *mon, uint32_t current_time_ms);
bool com_rx_monitor_check_timeout(com_rx_monitor_t *mon, uint32_t current_time_ms, e2e_rx_state_t *e2e_state);

/* ------------------------------------------------------------------------------
 * Hàm Pack / Unpack cho từng loại thông điệp
 * Khi tx_state != NULL, counter được tự động gán và tăng sau khi pack.
 * Khi rx_state != NULL, e2e_check_counter được tự động gọi khi unpack.
 * ------------------------------------------------------------------------------ */

/* 0x100 WSW_WARNING */
bool com_pack_wsw_warning(const com_msg_wsw_warning_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state);
e2e_status_t com_unpack_wsw_warning(com_msg_wsw_warning_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state);

/* 0x101 WHEEL_SPEED */
bool com_pack_wheel_speed(const com_msg_wheel_speed_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state);
e2e_status_t com_unpack_wheel_speed(com_msg_wheel_speed_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state);

/* 0x102 CLIMATE_STATUS */
bool com_pack_climate_status(const com_msg_climate_status_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state);
e2e_status_t com_unpack_climate_status(com_msg_climate_status_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state);

/* 0x103 BCM_STATUS */
bool com_pack_bcm_status(const com_msg_bcm_status_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state);
e2e_status_t com_unpack_bcm_status(com_msg_bcm_status_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state);

/* 0x104 WINDOW_CMD */
bool com_pack_window_cmd(const com_msg_window_cmd_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state);
e2e_status_t com_unpack_window_cmd(com_msg_window_cmd_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state);

/* 0x105 CLIMATE_CMD */
bool com_pack_climate_cmd(const com_msg_climate_cmd_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state);
e2e_status_t com_unpack_climate_cmd(com_msg_climate_cmd_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state);

/* 0x106 BCM_CMD */
bool com_pack_bcm_cmd(const com_msg_bcm_cmd_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state);
e2e_status_t com_unpack_bcm_cmd(com_msg_bcm_cmd_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state);

/* 0x110 DMS_ALERT */
bool com_pack_dms_alert(const com_msg_dms_alert_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state);
e2e_status_t com_unpack_dms_alert(com_msg_dms_alert_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state);

/* 0x111 DMS_STATUS */
bool com_pack_dms_status(const com_msg_dms_status_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state);
e2e_status_t com_unpack_dms_status(com_msg_dms_status_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state);

/* 0x204 WINDOW_STATUS */
bool com_pack_window_status(const com_msg_window_status_t *msg, uint8_t *payload, uint8_t *dlc, e2e_tx_state_t *tx_state);
e2e_status_t com_unpack_window_status(com_msg_window_status_t *msg, const uint8_t *payload, uint8_t dlc, e2e_rx_state_t *rx_state);

#ifdef __cplusplus
}
#endif

#endif /* CAN_COM_H */
