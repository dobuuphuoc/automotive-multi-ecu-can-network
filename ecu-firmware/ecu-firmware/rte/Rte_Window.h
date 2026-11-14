/* ==============================================================================
 * Rte_Window.h — RTE Application Header for Power Window SWC
 * ============================================================================== */
#ifndef RTE_WINDOW_H
#define RTE_WINDOW_H

#include "Rte_Type.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------------
 * Sender/Receiver Ports API
 * ------------------------------------------------------------------------------ */
Std_ReturnType Rte_Read_WindowCmd(com_msg_window_cmd_t *cmd, uint8_t *status);
Std_ReturnType Rte_Read_BcmStatus(com_msg_bcm_status_t *status_msg, uint8_t *status);
Std_ReturnType Rte_Read_ObstacleSensor(uint8_t *obstacle_detected);

Std_ReturnType Rte_Write_CurrentAngle(uint8_t angle);
Std_ReturnType Rte_Write_TargetAngleEcho(uint8_t angle);
Std_ReturnType Rte_Write_Moving(uint8_t moving);
Std_ReturnType Rte_Write_PinchDetected(uint8_t pinch);
Std_ReturnType Rte_Write_ChildLockState(uint8_t child_lock);

/* ------------------------------------------------------------------------------
 * Client/Server Ports API
 * ------------------------------------------------------------------------------ */
Std_ReturnType Rte_Call_Dem_SetEventStatus(uint32_t dtc_id, dem_event_status_t event_status);

/* ------------------------------------------------------------------------------
 * Runnable Entities
 * ------------------------------------------------------------------------------ */
void Window_Init_Runnable(void);
void Window_10ms_PID_Runnable(void);
void Window_Cmd_Runnable(void);

#ifdef __cplusplus
}
#endif

#endif /* RTE_WINDOW_H */
