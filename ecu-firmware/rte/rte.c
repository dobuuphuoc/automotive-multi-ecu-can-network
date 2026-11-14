/* ==============================================================================
 * rte.c — AUTOSAR RTE Virtual Function Bus (VFB) Implementation
 * ============================================================================== */
#include "Rte_Type.h"
#include "Rte_BCM.h"
#include "Rte_Climate.h"
#include "Rte_Window.h"
#include "Rte_WSW.h"
#include "diag_dem.h"
#include <string.h>

/* ------------------------------------------------------------------------------
 * Virtual Function Bus (VFB) Static Buffers
 * ------------------------------------------------------------------------------ */

/* BCM VFB */
static uint8_t  s_bcm_door_sensor = 0;
static uint16_t s_bcm_light_sensor = 2000;
static uint16_t s_bcm_wheel_speed_rx = 0;
static uint8_t  s_bcm_wheel_speed_status = RTE_SIG_VALID;
static uint8_t  s_bcm_dms_alert_type = 0;
static uint8_t  s_bcm_dms_alert_severity = 0;
static uint8_t  s_bcm_dms_alert_status = RTE_SIG_VALID;
static com_msg_bcm_cmd_t s_bcm_cmd_rx;
static uint8_t  s_bcm_cmd_status = RTE_SIG_VALID;

static Rte_BcmInternalState_t s_bcm_state;

/* Climate VFB */
static int16_t  s_cli_dht_temp_raw = 240; /* 24.0 C */
static uint16_t s_cli_dht_hum_raw  = 500; /* 50.0 % */
static bool     s_cli_dht_valid    = true;
static uint8_t  s_cli_sunlight     = 100;
static com_msg_climate_cmd_t s_cli_cmd_rx;
static uint8_t  s_cli_cmd_status   = RTE_SIG_VALID;
static uint8_t  s_cli_dms_alert_type = 0;
static uint8_t  s_cli_dms_alert_severity = 0;
static uint8_t  s_cli_dms_alert_status = RTE_SIG_VALID;

static Rte_ClimateInternalState_t s_cli_state;

/* Window VFB */
static com_msg_window_cmd_t  s_win_cmd_rx;
static uint8_t               s_win_cmd_status = RTE_SIG_VALID;
static com_msg_bcm_status_t  s_win_bcm_rx;
static uint8_t               s_win_bcm_status = RTE_SIG_VALID;
static uint8_t               s_win_obstacle_sensor = 0;

static Rte_WindowInternalState_t s_win_state;

/* WSW VFB */
static uint16_t s_wsw_encoder_speed_raw = 0;
static bool     s_wsw_encoder_dropout = false;
static uint8_t  s_wsw_brake_switch = 0;

static Rte_WswInternalState_t s_wsw_state;

/* ------------------------------------------------------------------------------
 * Khởi tạo RTE
 * ------------------------------------------------------------------------------ */
void Rte_Init(void)
{
    memset(&s_bcm_state, 0, sizeof(s_bcm_state));
    memset(&s_bcm_cmd_rx, 0, sizeof(s_bcm_cmd_rx));
    s_bcm_door_sensor = 0;
    s_bcm_light_sensor = 2000;
    s_bcm_wheel_speed_rx = 0;
    s_bcm_wheel_speed_status = RTE_SIG_VALID;
    s_bcm_dms_alert_type = 0;
    s_bcm_dms_alert_severity = 0;
    s_bcm_dms_alert_status = RTE_SIG_VALID;
    s_bcm_cmd_status = RTE_SIG_INVALID;

    memset(&s_cli_state, 0, sizeof(s_cli_state));
    memset(&s_cli_cmd_rx, 0, sizeof(s_cli_cmd_rx));
    s_cli_dht_temp_raw = 240;
    s_cli_dht_hum_raw  = 500;
    s_cli_dht_valid    = true;
    s_cli_sunlight     = 100;
    s_cli_cmd_status   = RTE_SIG_INVALID;
    s_cli_dms_alert_type = 0;
    s_cli_dms_alert_severity = 0;
    s_cli_dms_alert_status = RTE_SIG_VALID;

    memset(&s_win_state, 0, sizeof(s_win_state));
    memset(&s_win_cmd_rx, 0, sizeof(s_win_cmd_rx));
    memset(&s_win_bcm_rx, 0, sizeof(s_win_bcm_rx));
    s_win_cmd_status = RTE_SIG_INVALID;
    s_win_bcm_status = RTE_SIG_INVALID;
    s_win_obstacle_sensor = 0;

    memset(&s_wsw_state, 0, sizeof(s_wsw_state));
    s_wsw_encoder_speed_raw = 0;
    s_wsw_encoder_dropout = false;
    s_wsw_brake_switch = 0;
}

/* ------------------------------------------------------------------------------
 * BCM RTE API
 * ------------------------------------------------------------------------------ */
Std_ReturnType Rte_Read_DoorSensor(uint8_t *door_open)
{
    if (door_open == NULL) return E_NOT_OK;
    *door_open = s_bcm_door_sensor;
    return E_OK;
}

Std_ReturnType Rte_Read_LightSensor(uint16_t *raw_light)
{
    if (raw_light == NULL) return E_NOT_OK;
    *raw_light = s_bcm_light_sensor;
    return E_OK;
}

Std_ReturnType Rte_Read_WheelSpeed(uint16_t *speed_raw, uint8_t *status)
{
    if (speed_raw != NULL) *speed_raw = s_bcm_wheel_speed_rx;
    if (status != NULL) *status = s_bcm_wheel_speed_status;
    return E_OK;
}

Std_ReturnType Rte_Read_DMSAlert(uint8_t *alert_type, uint8_t *severity, uint8_t *status)
{
    if (alert_type != NULL) *alert_type = s_bcm_dms_alert_type;
    if (severity != NULL)   *severity   = s_bcm_dms_alert_severity;
    if (status != NULL)     *status     = s_bcm_dms_alert_status;
    return E_OK;
}

Std_ReturnType Rte_Read_BcmCmd(com_msg_bcm_cmd_t *cmd, uint8_t *status)
{
    if (cmd != NULL) *cmd = s_bcm_cmd_rx;
    if (status != NULL) *status = s_bcm_cmd_status;
    return E_OK;
}

Std_ReturnType Rte_Write_DoorStatus(uint8_t door_open)
{
    s_bcm_state.door_open = door_open;
    return E_OK;
}

Std_ReturnType Rte_Write_LockState(uint8_t lock_state)
{
    s_bcm_state.lock_state = lock_state;
    return E_OK;
}

Std_ReturnType Rte_Write_Headlight(uint8_t headlight_on)
{
    s_bcm_state.headlight_on = headlight_on;
    return E_OK;
}

Std_ReturnType Rte_Write_CabinLight(uint8_t cabin_on)
{
    s_bcm_state.cabinlight_on = cabin_on;
    return E_OK;
}

Std_ReturnType Rte_Write_Buzzer(uint8_t buzzer_on)
{
    s_bcm_state.buzzer_on = buzzer_on;
    return E_OK;
}

Std_ReturnType Rte_Write_AlarmState(uint8_t alarm_state)
{
    s_bcm_state.alarm_state = alarm_state;
    return E_OK;
}

Std_ReturnType Rte_Write_AmbientLight(uint8_t light_8bit)
{
    s_bcm_state.ambient_light = light_8bit;
    return E_OK;
}

/* ------------------------------------------------------------------------------
 * Climate RTE API
 * ------------------------------------------------------------------------------ */
Std_ReturnType Rte_Read_DHT_Sensors(int16_t *temp_raw, uint16_t *hum_raw, bool *is_valid)
{
    if (temp_raw != NULL) *temp_raw = s_cli_dht_temp_raw;
    if (hum_raw != NULL)  *hum_raw  = s_cli_dht_hum_raw;
    if (is_valid != NULL) *is_valid = s_cli_dht_valid;
    return E_OK;
}

Std_ReturnType Rte_Read_SunlightSensor(uint8_t *sunlight)
{
    if (sunlight == NULL) return E_NOT_OK;
    *sunlight = s_cli_sunlight;
    return E_OK;
}

Std_ReturnType Rte_Read_ClimateCmd(com_msg_climate_cmd_t *cmd, uint8_t *status)
{
    if (cmd != NULL) *cmd = s_cli_cmd_rx;
    if (status != NULL) *status = s_cli_cmd_status;
    return E_OK;
}

Std_ReturnType Rte_Write_FanSpeed(uint8_t fan_speed)
{
    s_cli_state.fan_speed = fan_speed;
    return E_OK;
}

Std_ReturnType Rte_Write_FlapAngle(uint8_t flap_angle)
{
    s_cli_state.flap_angle = flap_angle;
    return E_OK;
}

Std_ReturnType Rte_Write_ClimateMode(uint8_t mode)
{
    s_cli_state.climate_mode = mode;
    return E_OK;
}

Std_ReturnType Rte_Write_DHT_Fault(uint8_t fault)
{
    s_cli_state.dht_fault = fault;
    return E_OK;
}

Std_ReturnType Rte_Write_FogRisk(uint8_t fog_risk)
{
    s_cli_state.fog_risk = fog_risk;
    return E_OK;
}

Std_ReturnType Rte_Write_FanOverride(uint8_t fan_override)
{
    s_cli_state.fan_override = fan_override;
    return E_OK;
}

Std_ReturnType Rte_Write_CabinTemp(int16_t temp_raw)
{
    s_cli_state.cabin_temp_raw = temp_raw;
    return E_OK;
}

Std_ReturnType Rte_Write_CabinHumidity(uint16_t hum_raw)
{
    s_cli_state.cabin_humidity_raw = hum_raw;
    return E_OK;
}

/* ------------------------------------------------------------------------------
 * Window RTE API
 * ------------------------------------------------------------------------------ */
Std_ReturnType Rte_Read_WindowCmd(com_msg_window_cmd_t *cmd, uint8_t *status)
{
    if (cmd != NULL) *cmd = s_win_cmd_rx;
    if (status != NULL) *status = s_win_cmd_status;
    return E_OK;
}

Std_ReturnType Rte_Read_BcmStatus(com_msg_bcm_status_t *status_msg, uint8_t *status)
{
    if (status_msg != NULL) *status_msg = s_win_bcm_rx;
    if (status != NULL)     *status     = s_win_bcm_status;
    return E_OK;
}

Std_ReturnType Rte_Read_ObstacleSensor(uint8_t *obstacle_detected)
{
    if (obstacle_detected == NULL) return E_NOT_OK;
    *obstacle_detected = s_win_obstacle_sensor;
    return E_OK;
}

Std_ReturnType Rte_Write_CurrentAngle(uint8_t angle)
{
    s_win_state.current_angle = angle;
    return E_OK;
}

Std_ReturnType Rte_Write_TargetAngleEcho(uint8_t angle)
{
    s_win_state.target_angle = angle;
    return E_OK;
}

Std_ReturnType Rte_Write_Moving(uint8_t moving)
{
    s_win_state.moving = moving;
    return E_OK;
}

Std_ReturnType Rte_Write_PinchDetected(uint8_t pinch)
{
    s_win_state.pinch_detected = pinch;
    return E_OK;
}

Std_ReturnType Rte_Write_ChildLockState(uint8_t child_lock)
{
    s_win_state.child_lock_state = child_lock;
    return E_OK;
}

/* ------------------------------------------------------------------------------
 * WSW RTE API
 * ------------------------------------------------------------------------------ */
Std_ReturnType Rte_Read_EncoderSpeed(uint16_t *speed_raw, bool *dropout)
{
    if (speed_raw != NULL) *speed_raw = s_wsw_encoder_speed_raw;
    if (dropout != NULL)   *dropout   = s_wsw_encoder_dropout;
    return E_OK;
}

Std_ReturnType Rte_Read_BrakeSwitch(uint8_t *brake_pressed)
{
    if (brake_pressed == NULL) return E_NOT_OK;
    *brake_pressed = s_wsw_brake_switch;
    return E_OK;
}

Std_ReturnType Rte_Write_WheelSpeed(uint16_t speed_raw)
{
    s_wsw_state.wheel_speed_raw = speed_raw;
    return E_OK;
}

Std_ReturnType Rte_Write_RefSpeed(uint16_t ref_speed_raw)
{
    s_wsw_state.ref_speed_raw = ref_speed_raw;
    return E_OK;
}

Std_ReturnType Rte_Write_SlipPercent(uint8_t slip_pct)
{
    s_wsw_state.slip_percent = slip_pct;
    return E_OK;
}

Std_ReturnType Rte_Write_SlipWarningActive(uint8_t active)
{
    s_wsw_state.slip_warning_active = active;
    return E_OK;
}

Std_ReturnType Rte_Write_WarnLamp(uint8_t lamp)
{
    s_wsw_state.warn_lamp = lamp;
    return E_OK;
}

Std_ReturnType Rte_Write_DecelExceeded(uint8_t decel_exceeded)
{
    s_wsw_state.decel_exceeded = decel_exceeded;
    return E_OK;
}

/* ------------------------------------------------------------------------------
 * Client/Server Port: BSW DEM Integration
 * ------------------------------------------------------------------------------ */
Std_ReturnType Rte_Call_Dem_SetEventStatus(uint32_t dtc_id, dem_event_status_t event_status)
{
    Dem_ReportErrorStatus(dtc_id, event_status);
    return E_OK;
}

Std_ReturnType Rte_Call_Dem_CaptureFreezeFrame(uint16_t speed_raw, uint8_t slip_pct, uint8_t decel)
{
    Dem_CaptureFreezeFrame(speed_raw, slip_pct, decel);
    return E_OK;
}

/* ------------------------------------------------------------------------------
 * Helper đồng bộ giữa CAN/BSW/Hardware và RTE Buffers
 * ------------------------------------------------------------------------------ */
void Rte_Sync_WheelSpeed_Rx(uint16_t speed_raw, uint8_t sig_status)
{
    s_bcm_wheel_speed_rx = speed_raw;
    s_bcm_wheel_speed_status = sig_status;
}

void Rte_Sync_DmsAlert_Rx(uint8_t alert_type, uint8_t severity, uint8_t sig_status)
{
    s_bcm_dms_alert_type = alert_type;
    s_bcm_dms_alert_severity = severity;
    s_bcm_dms_alert_status = sig_status;

    s_cli_dms_alert_type = alert_type;
    s_cli_dms_alert_severity = severity;
    s_cli_dms_alert_status = sig_status;
}

void Rte_Sync_BcmCmd_Rx(const com_msg_bcm_cmd_t *cmd, uint8_t sig_status)
{
    if (cmd != NULL) s_bcm_cmd_rx = *cmd;
    s_bcm_cmd_status = sig_status;
}

void Rte_Sync_ClimateCmd_Rx(const com_msg_climate_cmd_t *cmd, uint8_t sig_status)
{
    if (cmd != NULL) s_cli_cmd_rx = *cmd;
    s_cli_cmd_status = sig_status;
}

void Rte_Sync_WindowCmd_Rx(const com_msg_window_cmd_t *cmd, uint8_t sig_status)
{
    if (cmd != NULL) s_win_cmd_rx = *cmd;
    s_win_cmd_status = sig_status;
}

void Rte_Sync_BcmStatus_Rx(const com_msg_bcm_status_t *msg, uint8_t sig_status)
{
    if (msg != NULL) s_win_bcm_rx = *msg;
    s_win_bcm_status = sig_status;
}

void Rte_Set_DoorSensor(uint8_t open)
{
    s_bcm_door_sensor = open;
}

void Rte_Set_LightSensor(uint16_t raw)
{
    s_bcm_light_sensor = raw;
}

void Rte_Set_DHT_Sensors(int16_t temp_raw, uint16_t hum_raw, bool valid)
{
    s_cli_dht_temp_raw = temp_raw;
    s_cli_dht_hum_raw  = hum_raw;
    s_cli_dht_valid    = valid;
}

void Rte_Set_SunlightSensor(uint8_t sunlight)
{
    s_cli_sunlight = sunlight;
}

void Rte_Set_ObstacleSensor(uint8_t obstacle)
{
    s_win_obstacle_sensor = obstacle;
}

void Rte_Set_EncoderSpeed(uint16_t raw, bool dropout)
{
    s_wsw_encoder_speed_raw = raw;
    s_wsw_encoder_dropout = dropout;
}

void Rte_Set_BrakeSwitch(uint8_t pressed)
{
    s_wsw_brake_switch = pressed;
}

/* Getters cho việc gửi frame CAN */
void Rte_Get_BcmStatus_Tx(com_msg_bcm_status_t *msg)
{
    if (msg == NULL) return;
    msg->door_open      = s_bcm_state.door_open;
    msg->lock_state     = s_bcm_state.lock_state;
    msg->headlight_on   = s_bcm_state.headlight_on;
    msg->cabinlight_on  = s_bcm_state.cabinlight_on;
    msg->buzzer_on      = s_bcm_state.buzzer_on;
    msg->pir_motion     = 0;
    msg->alarm_state    = s_bcm_state.alarm_state;
    msg->ambient_light  = s_bcm_state.ambient_light;
}

void Rte_Get_ClimateStatus_Tx(com_msg_climate_status_t *msg)
{
    if (msg == NULL) return;
    msg->cabin_temp_raw     = s_cli_state.cabin_temp_raw;
    msg->cabin_humidity_raw = s_cli_state.cabin_humidity_raw;
    msg->sunlight_level     = s_cli_sunlight;
    msg->fan_speed          = s_cli_state.fan_speed;
    msg->flap_angle         = s_cli_state.flap_angle;
    msg->climate_mode       = s_cli_state.climate_mode;
    msg->dht_fault          = s_cli_state.dht_fault;
    msg->fog_risk           = s_cli_state.fog_risk;
    msg->fan_override       = s_cli_state.fan_override;
}

void Rte_Get_WindowStatus_Tx(com_msg_window_status_t *msg)
{
    if (msg == NULL) return;
    msg->current_angle      = s_win_state.current_angle;
    msg->target_angle_echo  = s_win_state.target_angle;
    msg->moving             = s_win_state.moving;
    msg->pinch_detected     = s_win_state.pinch_detected;
    msg->child_lock_state   = s_win_state.child_lock_state;
}

void Rte_Get_WswWarning_Tx(com_msg_wsw_warning_t *msg)
{
    if (msg == NULL) return;
    msg->slip_warning_active = s_wsw_state.slip_warning_active;
    msg->brake_pressed       = s_wsw_state.brake_pressed;
    msg->warn_lamp           = s_wsw_state.warn_lamp;
    msg->decel_exceeded      = s_wsw_state.decel_exceeded;
    msg->slip_percent        = s_wsw_state.slip_percent;
}

void Rte_Get_WheelSpeed_Tx(com_msg_wheel_speed_t *msg)
{
    if (msg == NULL) return;
    msg->wheel_speed_raw = s_wsw_state.wheel_speed_raw;
    msg->ref_speed_raw   = s_wsw_state.ref_speed_raw;
    msg->brake_pressed   = s_wsw_state.brake_pressed;
}
