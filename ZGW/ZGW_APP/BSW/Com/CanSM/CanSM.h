#ifndef CANSM_H
#define CANSM_H

#include "Std_Types.h"
#include "Can.h"

typedef enum
{
    CANSM_BSM_UNINIT = 0u,
    CANSM_BSM_NO_COMMUNICATION,
    CANSM_BSM_SILENT_COMMUNICATION,
    CANSM_BSM_FULL_COMMUNICATION,
    CANSM_BSM_BUS_OFF_CHECK,
    CANSM_BSM_BUS_OFF_RECOVERY_L1,
    CANSM_BSM_BUS_OFF_RECOVERY_L2,
    CANSM_BSM_FAILED
} CanSM_StateType;

typedef enum
{
    CANSM_COMM_NO_COMMUNICATION = 0u,
    CANSM_COMM_SILENT_COMMUNICATION,
    CANSM_COMM_FULL_COMMUNICATION
} CanSM_ComModeType;

void CanSM_Init(void);
void CanSM_MainFunction(void);

Std_ReturnType CanSM_RequestComMode(uint8 ControllerId, CanSM_ComModeType ComMode);
Std_ReturnType CanSM_GetCurrentComMode(uint8 ControllerId, CanSM_ComModeType* ComMode);
CanSM_StateType CanSM_GetState(uint8 ControllerId);

void CanSM_ControllerBusOff(uint8 ControllerId);

void CanSM_BusOffBeginNotification(uint8 ControllerId);
void CanSM_BusOffEndNotification(uint8 ControllerId);
void CanSM_ModeChangeNotification(uint8 ControllerId, CanSM_ComModeType Mode);
void CanSM_FailedNotification(uint8 ControllerId);

#endif
