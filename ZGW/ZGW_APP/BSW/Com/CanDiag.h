#ifndef CANDIAG_H
#define CANDIAG_H

#include "Std_Types.h"
#include "Can.h"
#include "Dem_Types.h"

#define CANDIAG_CHANNEL_CLASSIC   CAN_CONTROLLER_CLASSIC
#define CANDIAG_CHANNEL_FD        CAN_CONTROLLER_FD
#define CANDIAG_CHANNEL_COUNT     CAN_NUM_CONTROLLERS

typedef enum
{
    CANDIAG_FAULT_BUS_OFF = 0u,
    CANDIAG_FAULT_ERROR_PASSIVE,
    CANDIAG_FAULT_CONTROLLER,
    CANDIAG_FAULT_PROTOCOL_ERROR,
    CANDIAG_FAULT_COUNT
} CanDiag_FaultType;

void CanDiag_Init(void);
void CanDiag_MainFunction(void);

void CanDiag_ReportBusOff(uint8 controllerId);
void CanDiag_ReportControllerRecovered(uint8 controllerId);
void CanDiag_ReportErrorPassive(uint8 controllerId);
void CanDiag_ReportErrorWarning(uint8 controllerId);
void CanDiag_ReportControllerFault(uint8 controllerId);
boolean CanDiag_IsChannelUnavailable(uint8 controllerId);

Std_ReturnType CanDiag_CaptureSnapshotData(Dem_EventIdType eventId, uint8 *buffer, uint16 *length);

#endif
