/* Móc của FreeRTOS cho cấu hình cấp phát tĩnh. */
#include "FreeRTOS.h"
#include "task.h"

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t  **ppxIdleTaskStackBuffer,
                                   configSTACK_DEPTH_TYPE *puxIdleTaskStackSize)
{
    static StaticTask_t idle_tcb;
    static StackType_t  idle_stack[configMINIMAL_STACK_SIZE];
    *ppxIdleTaskTCBBuffer   = &idle_tcb;
    *ppxIdleTaskStackBuffer = idle_stack;
    *puxIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask; (void)pcTaskName;
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}
