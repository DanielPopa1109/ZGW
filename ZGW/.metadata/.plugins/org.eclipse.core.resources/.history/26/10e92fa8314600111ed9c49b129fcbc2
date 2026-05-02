#include "Os.h"
#include "FreeRTOS_core0.h"
#include "FreeRTOS_core1.h"
#include "FreeRTOS_core2.h"
#include "FreeRTOSConfig_core0.h"
#include "FreeRTOSConfig_core1.h"
#include "FreeRTOSConfig_core2.h"
#include "McuSm.h"
#include "Can.h"
#include "SysMgr.h"
#include "Dcm.h"
#include "Ain.h"
#include "timers_core0.h"
#include "timers_core1.h"
#include "timers_core2.h"
#include "task_core0.h"
#include "task_core1.h"
#include "task_core2.h"
#include "CanIf.h"
#include "CanTp.h"
#include "Dcm_Cfg.h"
#include "LinIf.h"
#include "LinSM.h"
#include "PduR.h"
#include "Com.h"
#include "CanSM.h"
#include "LinTp.h"
#include "DoIP.h"
#include "SoAd.h"
#include "SomeIp.h"
#include "SomeIpSd.h"
#include "TcpIpH.h"
#include "Fee.h"
#include "Nvm.h"
#include "Fls.h"
#include "Dem.h"

void Os_Init_C0(void);
void Os_Init_C1(void);
void Os_Init_C2(void);
void ASILD_APPL_MainCycle_Task_C0(void *pvParameters);
void QM_APPL_MainCycle_Task_C0(void *pvParameters);
void ASILD_BSW_Task_C0(void *pvParameters);
void ASILB_BSW_Task_C0(void *pvParameters);
void QM_BSW_Task_C0(void *pvParameters);
void ASILD_BSW_Task_C1(void *pvParameters);
void ASILD_APPL_MainCycle_Task_C1(void *pvParameters);
void ASILD_APPL_MainCycle_Task_C2(void *pvParameters);
void ASILD_BSW_Task_C2(void *pvParameters);
void Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C0( TimerHandle_t_core0 xTimer_core0 );
void Alarm5ms_Callback_QM_APPL_MainCycle_Task_C0( TimerHandle_t_core0 xTimer_core0 );
void Alarm5ms_Callback_ASILD_BSW_Task_C0( TimerHandle_t_core0 xTimer_core0 );
void Alarm5ms_Callback_ASILB_BSW_Task_C0( TimerHandle_t_core0 xTimer_core0 );
void Alarm5ms_Callback_QM_BSW_Task_C0( TimerHandle_t_core0 xTimer_core0 );
void Alarm5ms_Callback_ASILD_BSW_Task_C1( TimerHandle_t_core1 xTimer_core1);
void Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C1( TimerHandle_t_core1 xTimer_core1);
void Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C2( TimerHandle_t_core2 xTimer_core2);
void Alarm5ms_Callback_ASILD_BSW_Task_C2( TimerHandle_t_core2 xTimer_core2);

long long OS_Counter_core0 = 0u;
long long IDLE_Counter_core0 = 0u;
long long OS_Counter_core1 = 0u;
long long IDLE_Counter_core1 = 0u;
long long OS_Counter_core2 = 0u;
long long IDLE_Counter_core2 = 0u;
uint8 Alarm5ms_Flag_ASILD_APPL_MainCycle_Task_C0 = 0u;
uint8 Alarm5ms_Flag_QM_APPL_MainCycle_Task_C0 = 0u;
uint8 Alarm5ms_Flag_ASILD_BSW_Task_C0 = 0u;
uint8 Alarm5ms_Flag_ASILB_BSW_Task_C0 = 0u;
uint8 Alarm5ms_Flag_QM_BSW_Task_C0 = 0u;
uint8 Alarm5ms_Flag_ASILD_BSW_Task_C1 = 0u;
uint8 Alarm5ms_Flag_ASILD_APPL_MainCycle_Task_C1 = 0u;
uint8 Alarm5ms_Flag_ASILD_APPL_MainCycle_Task_C2 = 0u;
uint8 Alarm5ms_Flag_ASILD_BSW_Task_C2 = 0u;
TimerHandle_t_core0 Handler_Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C0;
TimerHandle_t_core0 Handler_Alarm5ms_Callback_ASILD_BSW_Task_C0;
TimerHandle_t_core0 Handler_Alarm5ms_Callback_QM_APPL_MainCycle_Task_C0;
TimerHandle_t_core0 Handler_Alarm5ms_Callback_ASILB_BSW_Task_C0;
TimerHandle_t_core0 Handler_Alarm5ms_Callback_QM_BSW_Task_C0;
TimerHandle_t_core1 Handler_Alarm5ms_Callback_ASILD_BSW_Task_C1;
TimerHandle_t_core1 Handler_Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C1;
TimerHandle_t_core2 Handler_Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C2;
TimerHandle_t_core2 Handler_Alarm5ms_Callback_ASILD_BSW_Task_C2;
TaskHandle_t_core0 ASILD_APPL_MainCycle_Task_C0_THandle   ;
TaskHandle_t_core0 QM_APPL_MainCycle_Task_C0_THandle      ;
TaskHandle_t_core0 ASILD_BSW_Task_C0_THandle              ;
TaskHandle_t_core0 ASILB_BSW_Task_C0_THandle              ;
TaskHandle_t_core0 QM_BSW_Task_C0_THandle                 ;
TaskHandle_t_core1 ASILD_BSW_Task_C1_THandle              ;
TaskHandle_t_core1 ASILD_APPL_MainCycle_Task_C1_THandle   ;
TaskHandle_t_core2 ASILD_APPL_MainCycle_Task_C2_THandle   ;
TaskHandle_t_core2 ASILD_BSW_Task_C2_THandle              ;

void Os_Init_C0(void)
{
    xTaskCreate_core0(ASILD_APPL_MainCycle_Task_C0, "ASILD_APPL_MainCycle_Task_C0", 4*configMINIMAL_STACK_SIZE_core0, NULL, 25u, &ASILD_APPL_MainCycle_Task_C0_THandle);
    xTaskCreate_core0(QM_APPL_MainCycle_Task_C0, "QM_APPL_MainCycle_Task_C0", 4*configMINIMAL_STACK_SIZE_core0, NULL, 29u, &QM_APPL_MainCycle_Task_C0_THandle);
    xTaskCreate_core0(ASILD_BSW_Task_C0, "ASILD_BSW_Task_C0", configMINIMAL_STACK_SIZE_core0, NULL, 28u, &ASILD_BSW_Task_C0_THandle);
    xTaskCreate_core0(ASILB_BSW_Task_C0, "ASILB_BSW_Task_C0", configMINIMAL_STACK_SIZE_core0, NULL, 27u, &ASILB_BSW_Task_C0_THandle);
    xTaskCreate_core0(QM_BSW_Task_C0, "QM_BSW_Task_C0", configMINIMAL_STACK_SIZE_core0, NULL, 26u, &QM_BSW_Task_C0_THandle);
    Handler_Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C0 = xTimerCreate_core0("Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C0",
            pdMS_TO_TICKS_core0(5u),
            1u,
            NULL,
            Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C0);
    Handler_Alarm5ms_Callback_ASILD_BSW_Task_C0 = xTimerCreate_core0("Alarm5ms_Callback_ASILD_BSW_Task_C0",
            pdMS_TO_TICKS_core0(5u),
            1u,
            NULL,
            Alarm5ms_Callback_ASILD_BSW_Task_C0);
    Handler_Alarm5ms_Callback_ASILB_BSW_Task_C0 = xTimerCreate_core0("Alarm5ms_Callback_ASILB_BSW_Task_C0",
            pdMS_TO_TICKS_core0(5u),
            1u,
            NULL,
            Alarm5ms_Callback_ASILB_BSW_Task_C0);
    Handler_Alarm5ms_Callback_QM_BSW_Task_C0 = xTimerCreate_core0("Alarm5ms_Callback_QM_BSW_Task_C0",
            pdMS_TO_TICKS_core0(5u),
            1u,
            NULL,
            Alarm5ms_Callback_QM_BSW_Task_C0);
    Handler_Alarm5ms_Callback_QM_APPL_MainCycle_Task_C0 = xTimerCreate_core0("Alarm5ms_Callback_QM_APPL_MainCycle_Task_C0",
            pdMS_TO_TICKS_core0(5u),
            1u,
            NULL,
            Alarm5ms_Callback_QM_APPL_MainCycle_Task_C0);
    xTimerStart_core0(Handler_Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C0, pdMS_TO_TICKS_core0(5u));
    xTimerStart_core0(Handler_Alarm5ms_Callback_ASILD_BSW_Task_C0, pdMS_TO_TICKS_core0(5u));
    xTimerStart_core0(Handler_Alarm5ms_Callback_ASILB_BSW_Task_C0, pdMS_TO_TICKS_core0(5u));
    xTimerStart_core0(Handler_Alarm5ms_Callback_QM_BSW_Task_C0, pdMS_TO_TICKS_core0(5u));
    xTimerStart_core0(Handler_Alarm5ms_Callback_QM_APPL_MainCycle_Task_C0, 5u);
}

void Os_Init_C1(void)
{
    xTaskCreate_core1(ASILD_BSW_Task_C1, "ASILD_BSW_Task_C1", 4*configMINIMAL_STACK_SIZE_core1, NULL, 29u, &ASILD_BSW_Task_C1_THandle);
    xTaskCreate_core1(ASILD_APPL_MainCycle_Task_C1, "ASILD_APPL_MainCycle_Task_C1", 4*configMINIMAL_STACK_SIZE_core1, NULL, 28u, &ASILD_APPL_MainCycle_Task_C1_THandle);

    Handler_Alarm5ms_Callback_ASILD_BSW_Task_C1 = xTimerCreate_core1("Alarm5ms_Callback_ASILD_BSW_Task_C1",
            pdMS_TO_TICKS_core1(5u),
            1u,
            NULL,
            Alarm5ms_Callback_ASILD_BSW_Task_C1);


    Handler_Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C1 = xTimerCreate_core1("Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C1",
            pdMS_TO_TICKS_core1(5u),
            1u,
            NULL,
            Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C1);

    xTimerStart_core1(Handler_Alarm5ms_Callback_ASILD_BSW_Task_C1, 5u);
    xTimerStart_core1(Handler_Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C1, 5u);
}

void Os_Init_C2(void)
{
    xTaskCreate_core2(ASILD_APPL_MainCycle_Task_C2, "ASILD_APPL_MainCycle_Task_C2", 4*configMINIMAL_STACK_SIZE_core2, NULL, 29u, &ASILD_APPL_MainCycle_Task_C2_THandle);
    xTaskCreate_core2(ASILD_BSW_Task_C2, "ASILD_BSW_Task_C2", 4*configMINIMAL_STACK_SIZE_core2, NULL, 28u, &ASILD_BSW_Task_C2_THandle);
    Handler_Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C2 = xTimerCreate_core2("Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C2",
            pdMS_TO_TICKS_core2(5u),
            1u,
            NULL,
            Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C2);
    Handler_Alarm5ms_Callback_ASILD_BSW_Task_C2 = xTimerCreate_core2("Alarm5ms_Callback_ASILD_BSW_Task_C2",
            pdMS_TO_TICKS_core2(5u),
            1u,
            NULL,
            Alarm5ms_Callback_ASILD_BSW_Task_C2);
    xTimerStart_core2(Handler_Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C2, 5u);
    xTimerStart_core2(Handler_Alarm5ms_Callback_ASILD_BSW_Task_C2, 5u);
}

void vApplicationMallocFailedHook_core0(void)
{
    McuSm_PerformResetHook(380u, 1u);
}

void vApplicationStackOverflowHook_core0(TaskHandle_t_core0 xTask, char * pcTaskName )
{
    McuSm_PerformResetHook(379u, 1u);
}

void vApplicationTickHook_core0(void)
{
    OS_Counter_core0++;
}

void vApplicationIdleHook_core0(void)
{
    IDLE_Counter_core0++;
}

void vApplicationMallocFailedHook_core1(void)
{
    McuSm_PerformResetHook(380u, 2u);
}

void vApplicationStackOverflowHook_core1(TaskHandle_t_core1 xTask, char * pcTaskName )
{
    McuSm_PerformResetHook(379u, 2u);
}

void vApplicationTickHook_core1(void)
{
    OS_Counter_core1++;
}

void vApplicationIdleHook_core1(void)
{
    IDLE_Counter_core1++;
}

void vApplicationMallocFailedHook_core2(void)
{
    McuSm_PerformResetHook(380u, 3u);
}

void vApplicationStackOverflowHook_core2(TaskHandle_t_core2 xTask, char * pcTaskName )
{
    McuSm_PerformResetHook(379u, 3u);
}

void vApplicationTickHook_core2(void)
{
    OS_Counter_core2++;
}

void vApplicationIdleHook_core2(void)
{
    IDLE_Counter_core2++;
}

void ASILD_APPL_MainCycle_Task_C0(void *pvParameters)
{
    for(;;)
    {
        if(1u == Alarm5ms_Flag_ASILD_APPL_MainCycle_Task_C0 && (2u == SysMgr_EcuState || 3u == SysMgr_EcuState))
        {
            Alarm5ms_Flag_ASILD_APPL_MainCycle_Task_C0 = 0u;
            Can_MainFunction();
            Dcm_MainFunction();
            CanTp_MainFunction();
            LinSM_MainFunction();
            LinIf_MainFunction();
            LinTp_MainFunction();
            NvM_MainFunction();
            Fee_MainFunction();
            Fls_MainFunction();
            Dem_MainFunction();
        }
        else
        {
            /* Do nothing. */
        }

        vTaskSuspend_core0(NULL);
    }
}

void QM_APPL_MainCycle_Task_C0(void *pvParameters)
{
    while(1)
    {
        if(1u == Alarm5ms_Flag_QM_APPL_MainCycle_Task_C0 && (2u == SysMgr_EcuState || 3u == SysMgr_EcuState))
        {
            Alarm5ms_Flag_QM_APPL_MainCycle_Task_C0 = 0u;
        }
        else
        {
            /* Do nothing. */
        }

        vTaskSuspend_core0(NULL);
    }
}

void ASILD_BSW_Task_C0(void *pvParameters)
{
    while(1)
    {
        if(1u == Alarm5ms_Flag_ASILD_BSW_Task_C0)
        {
            Alarm5ms_Flag_ASILD_BSW_Task_C0 = 0u;
            SysMgr_MainFunction();
        }
        else
        {
            /* Do nothing. */
        }

        vTaskSuspend_core0(NULL);
    }
}

void ASILB_BSW_Task_C0(void *pvParameters)
{
    while(1)
    {
        if(1u == Alarm5ms_Flag_ASILB_BSW_Task_C0 && 2u == SysMgr_EcuState)
        {
            Alarm5ms_Flag_ASILB_BSW_Task_C0 = 0u;
            Ain_MainFunction();
        }
        else
        {
            /* Do nothing. */
        }

        vTaskSuspend_core0(NULL);
    }
}

void QM_BSW_Task_C0(void *pvParameters)
{
    while(1)
    {
        if(1u == Alarm5ms_Flag_QM_BSW_Task_C0 && (2u == SysMgr_EcuState || 3u == SysMgr_EcuState))
        {
            Alarm5ms_Flag_QM_BSW_Task_C0 = 0u;
        }
        else
        {
            /* Do nothing. */
        }

        vTaskSuspend_core0(NULL);
    }
}

void ASILD_BSW_Task_C1(void *pvParameters)
{
    while(1)
    {
        if(1u == Alarm5ms_Flag_ASILD_BSW_Task_C1)
        {
            Alarm5ms_Flag_ASILD_BSW_Task_C1 = 0u;
        }
        else
        {
            /* Do nothing. */
        }

        vTaskSuspend_core1(NULL);
    }
}

void ASILD_APPL_MainCycle_Task_C1(void *pvParameters)
{
    while(1)
    {
        if(1u == Alarm5ms_Flag_ASILD_APPL_MainCycle_Task_C1  && 2u == SysMgr_EcuState)
        {
            Alarm5ms_Flag_ASILD_APPL_MainCycle_Task_C1 = 0u;
        }
        else
        {
            /* Do nothing. */
        }

        vTaskSuspend_core1(NULL);
    }
}

void ASILD_APPL_MainCycle_Task_C2(void *pvParameters)
{
    while(1)
    {
        if(1u == Alarm5ms_Flag_ASILD_APPL_MainCycle_Task_C2 && 2u == SysMgr_EcuState)
        {
            Alarm5ms_Flag_ASILD_APPL_MainCycle_Task_C2 = 0u;
        }
        else
        {
            /* Do nothing. */
        }

        vTaskSuspend_core2(NULL);
    }
}

void ASILD_BSW_Task_C2(void *pvParameters)
{
    while(1)
    {
        if(1u == Alarm5ms_Flag_ASILD_BSW_Task_C2)
        {
            Alarm5ms_Flag_ASILD_BSW_Task_C2 = 0u;
            TcpIp_MainFunction();
            SoAd_MainFunction();
            DoIP_MainFunction(5);
            SomeIp_MainFunction(5);
            SomeIpSd_MainFunction(5);
        }
        else
        {
            /* Do nothing. */
        }

        vTaskSuspend_core2(NULL);
    }
}

void Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C0( TimerHandle_t_core0 xTimer_core0 )
{
    Alarm5ms_Flag_ASILD_APPL_MainCycle_Task_C0 = 1u;
    vTaskResume_core0(ASILD_APPL_MainCycle_Task_C0_THandle);
}

void Alarm5ms_Callback_QM_APPL_MainCycle_Task_C0( TimerHandle_t_core0 xTimer_core0 )
{
    Alarm5ms_Flag_QM_APPL_MainCycle_Task_C0 = 1u;
    vTaskResume_core0(QM_APPL_MainCycle_Task_C0_THandle);
}

void Alarm5ms_Callback_ASILD_BSW_Task_C0( TimerHandle_t_core0 xTimer_core0 )
{
    Alarm5ms_Flag_ASILD_BSW_Task_C0 = 1u;
    vTaskResume_core0(ASILD_BSW_Task_C0_THandle);
}

void Alarm5ms_Callback_ASILB_BSW_Task_C0( TimerHandle_t_core0 xTimer_core0 )
{
    Alarm5ms_Flag_ASILB_BSW_Task_C0 = 1u;
    vTaskResume_core0(ASILB_BSW_Task_C0_THandle);
}

void Alarm5ms_Callback_QM_BSW_Task_C0( TimerHandle_t_core0 xTimer_core0 )
{
    Alarm5ms_Flag_QM_BSW_Task_C0 = 1u;
    vTaskResume_core0(QM_BSW_Task_C0_THandle);
}

void Alarm5ms_Callback_ASILD_BSW_Task_C1( TimerHandle_t_core1 xTimer_core1 )
{
    Alarm5ms_Flag_ASILD_BSW_Task_C1 = 1u;
    vTaskResume_core1(ASILD_BSW_Task_C1_THandle);
}

void Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C1( TimerHandle_t_core1 xTimer_core1 )
{
    Alarm5ms_Flag_ASILD_APPL_MainCycle_Task_C1 = 1u;
    vTaskResume_core1(ASILD_APPL_MainCycle_Task_C1_THandle);
}

void Alarm5ms_Callback_ASILD_APPL_MainCycle_Task_C2( TimerHandle_t_core2 xTimer_core2 )
{
    Alarm5ms_Flag_ASILD_APPL_MainCycle_Task_C2 = 1u;
    vTaskResume_core2(ASILD_APPL_MainCycle_Task_C2_THandle);
}

void Alarm5ms_Callback_ASILD_BSW_Task_C2( TimerHandle_t_core2 xTimer_core2 )
{
    Alarm5ms_Flag_ASILD_BSW_Task_C2 = 1u;
    vTaskResume_core2(ASILD_BSW_Task_C2_THandle);
}
