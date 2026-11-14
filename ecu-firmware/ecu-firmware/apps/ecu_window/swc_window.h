/* ==============================================================================
 * swc_window.h — Software Component Power Window
 * Pure AUTOSAR SWC
 * ============================================================================== */
#ifndef SWC_WINDOW_H
#define SWC_WINDOW_H

#include "Rte_Window.h"

#ifdef __cplusplus
extern "C" {
#endif

void Window_Init_Runnable(void);
void Window_10ms_PID_Runnable(void);
void Window_Cmd_Runnable(void);

#ifdef __cplusplus
}
#endif

#endif /* SWC_WINDOW_H */
