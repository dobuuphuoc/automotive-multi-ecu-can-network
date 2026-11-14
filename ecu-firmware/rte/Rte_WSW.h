/* ==============================================================================
 * Rte_WSW.h — RTE Application Header for Wheel Slip Warning SWC
 * ============================================================================== */
#ifndef RTE_WSW_H
#define RTE_WSW_H

#include "Rte_Type.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------------
 * Sender/Receiver Ports API
 * ------------------------------------------------------------------------------ */
Std_ReturnType Rte_Read_EncoderSpeed(uint16_t *speed_raw, bool *dropout);
Std_ReturnType Rte_Read_BrakeSwitch(uint8_t *brake_pressed);

Std_ReturnType Rte_Write_WheelSpeed(uint16_t speed_raw);
Std_ReturnType Rte_Write_RefSpeed(uint16_t ref_speed_raw);
Std_ReturnType Rte_Write_SlipPercent(uint8_t slip_pct);
Std_ReturnType Rte_Write_SlipWarningActive(uint8_t active);
Std_ReturnType Rte_Write_WarnLamp(uint8_t lamp);
Std_ReturnType Rte_Write_DecelExceeded(uint8_t decel_exceeded);

/* ------------------------------------------------------------------------------
 * Client/Server Ports API
 * ------------------------------------------------------------------------------ */
Std_ReturnType Rte_Call_Dem_SetEventStatus(uint32_t dtc_id, dem_event_status_t event_status);
Std_ReturnType Rte_Call_Dem_CaptureFreezeFrame(uint16_t speed_raw, uint8_t slip_pct, uint8_t decel);

/* ------------------------------------------------------------------------------
 * Runnable Entities
 * ------------------------------------------------------------------------------ */
void WSW_Init_Runnable(void);
void WSW_20ms_Capture_Runnable(void);
void WSW_SlipCalc_Runnable(void);

#ifdef __cplusplus
}
#endif

#endif /* RTE_WSW_H */
