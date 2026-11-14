/* ==============================================================================
 * Rte_Type.h — AUTOSAR RTE Type Definitions
 * ============================================================================== */
#ifndef RTE_TYPE_H
#define RTE_TYPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "diag_dem.h"
#include "can_com.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Mã trả về tiêu chuẩn của AUTOSAR API */
typedef uint8_t Std_ReturnType;
#define E_OK        0x00u
#define E_NOT_OK    0x01u

/* Trạng thái hợp lệ của tín hiệu từ CAN E2E / Timeout */
#define RTE_SIG_VALID    0x00u
#define RTE_SIG_INVALID  0x01u

/* ------------------------------------------------------------------------------
 * Kiểu dữ liệu đặc thù cho BCM SWC
 * ------------------------------------------------------------------------------ */
typedef struct {
    uint8_t door_open;     /* 0=Closed, 1=Open */
    uint8_t lock_state;    /* 0=Unlocked, 1=Locked */
    uint8_t headlight_on;  /* 0=Off, 1=On */
    uint8_t cabinlight_on; /* 0=Off, 1=On */
    uint8_t buzzer_on;     /* 0=Off, 1=On */
    uint8_t alarm_state;   /* 0=Disarmed, 1=Armed, 2=Alarm */
    uint8_t ambient_light; /* 0..255 */
} Rte_BcmInternalState_t;

/* ------------------------------------------------------------------------------
 * Kiểu dữ liệu đặc thù cho Climate SWC
 * ------------------------------------------------------------------------------ */
typedef struct {
    int16_t cabin_temp_raw;     /* 0.1 degC */
    uint16_t cabin_humidity_raw;/* 0.1 % */
    uint8_t  sunlight_level;    /* 0..255 */
    uint8_t  fan_speed;         /* 0..100 % */
    uint8_t  flap_angle;        /* 0..180 deg */
    uint8_t  climate_mode;      /* 0=Manual, 1=Auto */
    uint8_t  temp_setpoint_raw; /* 0.5 degC, e.g. 48 = 24.0 degC */
    uint8_t  fan_override;      /* 0 or 1 */
    uint8_t  dht_fault;         /* 0 or 1 */
    uint8_t  fog_risk;          /* 0 or 1 */
} Rte_ClimateInternalState_t;

/* ------------------------------------------------------------------------------
 * Kiểu dữ liệu đặc thù cho Window SWC
 * ------------------------------------------------------------------------------ */
typedef struct {
    uint8_t current_angle;     /* 0..180 deg */
    uint8_t target_angle;      /* 0..180 deg */
    uint8_t moving;            /* 0 or 1 */
    uint8_t pinch_detected;    /* 0 or 1 */
    uint8_t child_lock_state;  /* 0 or 1 */
} Rte_WindowInternalState_t;

/* ------------------------------------------------------------------------------
 * Kiểu dữ liệu đặc thù cho WSW SWC
 * ------------------------------------------------------------------------------ */
typedef struct {
    uint16_t wheel_speed_raw;      /* 0.1 rpm */
    uint16_t ref_speed_raw;        /* 0.1 rpm */
    uint8_t  slip_percent;         /* 0..100 % */
    uint8_t  slip_warning_active;  /* 0 or 1 */
    uint8_t  brake_pressed;        /* 0 or 1 */
    uint8_t  warn_lamp;            /* 0 or 1 */
    uint8_t  decel_exceeded;       /* 0 or 1 */
} Rte_WswInternalState_t;

#ifdef __cplusplus
}
#endif

#endif /* RTE_TYPE_H */
