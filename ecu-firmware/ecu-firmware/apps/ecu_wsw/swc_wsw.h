/* ==============================================================================
 * swc_wsw.h — Software Component Wheel Slip Warning
 * Pure AUTOSAR SWC
 * ============================================================================== */
#ifndef SWC_WSW_H
#define SWC_WSW_H

#include "Rte_WSW.h"

#ifdef __cplusplus
extern "C" {
#endif

void WSW_Init_Runnable(void);
void WSW_20ms_Capture_Runnable(void);
void WSW_SlipCalc_Runnable(void);

#ifdef __cplusplus
}
#endif

#endif /* SWC_WSW_H */
