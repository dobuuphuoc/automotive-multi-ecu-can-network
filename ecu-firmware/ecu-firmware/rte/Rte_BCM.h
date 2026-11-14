/* ==============================================================================
 * Rte_BCM.h — RTE Application Header for Body Control Module SWC
 * ============================================================================== */
#ifndef RTE_BCM_H
#define RTE_BCM_H

#include "Rte_Type.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------------
 * Sender/Receiver Ports API
 * ------------------------------------------------------------------------------ */
Std_ReturnType Rte_Read_DoorSensor(uint8_t *door_open);
Std_ReturnType Rte_Read_LightSensor(uint16_t *raw_light);
Std_ReturnType Rte_Read_WheelSpeed(uint16_t *speed_raw, uint8_t *status);
Std_ReturnType Rte_Read_DMSAlert(uint8_t *alert_type, uint8_t *severity, uint8_t *status);
Std_ReturnType Rte_Read_BcmCmd(com_msg_bcm_cmd_t *cmd, uint8_t *status);

Std_ReturnType Rte_Write_DoorStatus(uint8_t door_open);
Std_ReturnType Rte_Write_LockState(uint8_t lock_state);
Std_ReturnType Rte_Write_Headlight(uint8_t headlight_on);
Std_ReturnType Rte_Write_CabinLight(uint8_t cabin_on);
Std_ReturnType Rte_Write_Buzzer(uint8_t buzzer_on);
Std_ReturnType Rte_Write_AlarmState(uint8_t alarm_state);
Std_ReturnType Rte_Write_AmbientLight(uint8_t light_8bit);

/* ------------------------------------------------------------------------------
 * Client/Server Ports API
 * ------------------------------------------------------------------------------ */
Std_ReturnType Rte_Call_Dem_SetEventStatus(uint32_t dtc_id, dem_event_status_t event_status);

/* ------------------------------------------------------------------------------
 * Runnable Entities
 * ------------------------------------------------------------------------------ */
void BCM_Init_Runnable(void);
void BCM_100ms_Runnable(void);
void BCM_CmdHandler_Runnable(void);

#ifdef __cplusplus
}
#endif

#endif /* RTE_BCM_H */
