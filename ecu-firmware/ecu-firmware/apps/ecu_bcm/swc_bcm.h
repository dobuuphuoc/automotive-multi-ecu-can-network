/* ==============================================================================
 * swc_bcm.h — Software Component Body Control Module
 * Pure AUTOSAR SWC (chỉ gọi API RTE, không phụ thuộc HAL/FreeRTOS)
 * ============================================================================== */
#ifndef SWC_BCM_H
#define SWC_BCM_H

#include "Rte_BCM.h"

#ifdef __cplusplus
extern "C" {
#endif

void BCM_Init_Runnable(void);
void BCM_100ms_Runnable(void);
void BCM_CmdHandler_Runnable(void);

#ifdef __cplusplus
}
#endif

#endif /* SWC_BCM_H */
