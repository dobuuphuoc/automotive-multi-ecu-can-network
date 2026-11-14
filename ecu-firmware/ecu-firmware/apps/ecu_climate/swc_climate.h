/* ==============================================================================
 * swc_climate.h — Software Component Climate Control
 * Pure AUTOSAR SWC
 * ============================================================================== */
#ifndef SWC_CLIMATE_H
#define SWC_CLIMATE_H

#include "Rte_Climate.h"

#ifdef __cplusplus
extern "C" {
#endif

void Climate_Init_Runnable(void);
void Climate_100ms_Runnable(void);
void Climate_SensorRead_2s_Runnable(void);

#ifdef __cplusplus
}
#endif

#endif /* SWC_CLIMATE_H */
