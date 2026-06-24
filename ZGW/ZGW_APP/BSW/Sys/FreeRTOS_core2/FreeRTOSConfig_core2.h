#ifndef FREERTOS_CONFIG_CORE2_H
#define FREERTOS_CONFIG_CORE2_H

#define configUSE_PREEMPTION_core2                       1
#define configUSE_IDLE_HOOK_core2                        1
#define configCPU_CLOCK_HZ_core2                         ( ( unsigned long ) 300000000UL )
#define configTICK_RATE_HZ_core2                         ( ( TickType_t_core2 ) 1000UL )
#define configMAX_PRIORITIES_core2                       ( 31 )
/* Use the generic (portable C) task selector, NOT the __clz bitmap one.
 * This MUST be defined here (FreeRTOSConfig is included before portable_core2.h)
 * to resolve a latent conflict: FreeRTOS_core2.h defaults this to 0 while
 * portmacro_core2.h defaults it to 1, and include order silently selected the
 * bitmap selector. The bitmap (uxTopReadyPriority_core2) trusts a per-priority
 * "ready" bit; a single stale bit (observed: bit 28 = ASIL_APPL_Task_C2 left
 * set after its ready list emptied) makes portGET_HIGHEST_PRIORITY pick a
 * priority whose list is empty, yielding a NULL/!=runnable task and a context-
 * switch fault (reset reason 386). The generic selector reads real list
 * emptiness and walks down, so it is immune to a stale priority hint. */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION_core2    0
#define configMINIMAL_STACK_SIZE_core2                   ( ( unsigned short ) 256 )
#define configTOTAL_HEAP_SIZE_core2                      ( ( size_t ) 32920 )
#define configMAX_TASK_NAME_LEN_core2                    ( 32 )
#define configENABLE_BACKWARD_COMPATIBILITY_core2        0
#define configUSE_TRACE_FACILITY_core2                   0
#define configGENERATE_RUN_TIME_STATS_core2              1
#define configRUN_TIME_COUNTER_TYPE_core2                uint32_t
#define configUSE_16_BIT_TICKS_core2                     0
#define configIDLE_SHOULD_YIELD_core2                    0
#define configUSE_MALLOC_FAILED_HOOK_core2               1
#define configCHECK_FOR_STACK_OVERFLOW_core2             2
#define configUSE_TICK_HOOK_core2                        1
#define configUSE_COUNTING_SEMAPHORES_core2              1
#define configUSE_RECURSIVE_MUTEXES_core2                1
#define configUSE_MUTEXES_core2                          1
#define configRECORD_STACK_HIGH_ADDRESS_core2            1
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS_core2    5
/* Software timer configuration. */
#define configUSE_TIMERS_core2                           ( 1 )
#define configTIMER_TASK_PRIORITY_core2                  ( configMAX_PRIORITIES_core2 - 1 )
#define configTIMER_QUEUE_LENGTH_core2                   ( 15 )
#define configTIMER_TASK_STACK_DEPTH_core2               ( configMINIMAL_STACK_SIZE_core2 * 2u )
/* Set the following definitions to 1 to include the API function, or zero
 * to exclude the API function. */
#define INCLUDE_vTaskPrioritySet_core2                   1
#define INCLUDE_uxTaskPriorityGet_core2                  1
#define INCLUDE_vTaskDelete_core2                        1
#define INCLUDE_vTaskCleanUpResources_core2              1
#define INCLUDE_vTaskSuspend_core2                       1
#define INCLUDE_vTaskDelayUntil_core2                    1
#define INCLUDE_vTaskDelay_core2                         1
/* Interrupts above priority 31 are not affected by critical sections and must
 * not call FreeRTOS APIs. Keep the FreeRTOS tick/context interrupts at or below
 * this mask level so scheduler list updates are atomic. */
#define configMAX_API_CALL_INTERRUPT_PRIORITY_core2      31
#ifdef __TASKING__
/* Trap on a failed assertion instead of silently continuing. The kernel's
 * internal integrity checks (empty-list underflow in the task selector, list
 * validity, etc.) run through configASSERT; leaving this empty let core2
 * scheduler corruption sail through undetected until it surfaced much later as
 * a NULL pxCurrentTCB_core2 in the context switch (reset reason 386). Halting
 * here freezes the CPU at the origin of the corruption for inspection. */
#define configASSERT_core2( x_core2 )    if( ( x_core2 ) == 0 ) { __disable(); __debug(); for( ;; ) {} }
#elif defined(__clang__)
#define configASSERT_core2( x_core2 )    if( ( x_core2 ) == 0 ) { __builtin_tricore_disable(); __builtin_tricore_debug(); }
#endif
#ifndef configASSERT_core2
#define configASSERT_core2( x_core2 ) ((void)(x_core2)) /* Empty macro to remove compiler warning(s) about unused variables */
#endif
/* AURIX TCxxx definitions */
#define configCONTEXT_INTERRUPT_PRIORITY_core2    30
#define configTIMER_INTERRUPT_PRIORITY_core2      31
#define configCPU_NR_core2                        2
#define configPROVIDE_SYSCALL_TRAP_core2          0
#define configSYSCALL_CALL_DEPTH_core2            2
#define configSTM_core2                           ( ( uint32_t * ) (0xF0001000 + configCPU_NR_core2*0x100 ) )
#define configSTM_SRC_core2                       ( ( uint32_t * ) (0xF0038300 + configCPU_NR_core2*0x8) )
#define configSTM_CLOCK_HZ_core2                  ( 100000000 )
#define configSTM_DEBUG_core2                     ( 0 )
#define configCONTEXT_SRC_core2                   ( ( uint32_t * ) 0xF0038998 )
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS_core2()
#define portGET_RUN_TIME_COUNTER_VALUE_core2() \
    ( ( configRUN_TIME_COUNTER_TYPE_core2 ) ( ( ( volatile uint32_t * ) configSTM_core2 )[ 0x10u >> 2 ] ) )

extern void Os_CpuLoad_TaskSwitchedIn(unsigned char CoreId, unsigned char IsIdleTask);
extern void Os_CpuLoad_TaskSwitchedOut(unsigned char CoreId);
#define traceTASK_SWITCHED_IN_core2() \
    Os_CpuLoad_TaskSwitchedIn(2u, (unsigned char)(pxCurrentTCB_core2 == xIdleTaskHandle_core2))
#define traceTASK_SWITCHED_OUT_core2() \
    Os_CpuLoad_TaskSwitchedOut(2u)
#endif /* FREERTOS_CONFIG_H */
