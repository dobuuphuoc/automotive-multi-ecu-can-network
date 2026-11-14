/* ==============================================================================
 * Rte_Climate.h — RTE Application Header for Climate Control SWC
 * ============================================================================== */
#ifndef RTE_CLIMATE_H
#define RTE_CLIMATE_H

#include "Rte_Type.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------------
 * Sender/Receiver Ports API
 * ------------------------------------------------------------------------------ */
Std_ReturnType Rte_Read_DHT_Sensors(int16_t *temp_raw, uint16_t *hum_raw, bool *is_valid);
Std_ReturnType Rte_Read_SunlightSensor(uint8_t *sunlight);
Std_ReturnType Rte_Read_ClimateCmd(com_msg_climate_cmd_t *cmd, uint8_t *status);
Std_ReturnType Rte_Read_DMSAlert(uint8_t *alert_type, uint8_t *severity, uint8_t *status);

Std_ReturnType Rte_Write_FanSpeed(uint8_t fan_speed);
Std_ReturnType Rte_Write_FlapAngle(uint8_t flap_angle);
Std_ReturnType Rte_Write_ClimateMode(uint8_t mode);
Std_ReturnType Rte_Write_DHT_Fault(uint8_t fault);
Std_ReturnType Rte_Write_FogRisk(uint8_t fog_risk);
Std_ReturnType Rte_Write_FanOverride(uint8_t fan_override);
Std_ReturnType Rte_Write_CabinTemp(int16_t temp_raw);
Std_ReturnType Rte_Write_CabinHumidity(uint16_t hum_raw);

/* ------------------------------------------------------------------------------
 * Client/Server Ports API
 * ------------------------------------------------------------------------------ */
Std_ReturnType Rte_Call_Dem_SetEventStatus(uint32_t dtc_id, dem_event_status_t event_status);

/* ------------------------------------------------------------------------------
 * Runnable Entities
 * ------------------------------------------------------------------------------ */
void Climate_Init_Runnable(void);
void Climate_100ms_Runnable(void);
void Climate_SensorRead_2s_Runnable(void);

#ifdef __cplusplus
}
#endif

#endif /* RTE_CLIMATE_H */
