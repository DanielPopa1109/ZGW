#ifndef DEM_H_
#define DEM_H_

#include "Dem_Types.h"
#include "Dem_Cfg.h"

void Dem_PreInit(void);
void Dem_Init(const Dem_ConfigType *ConfigPtr);
Std_ReturnType Dem_Shutdown(void);
void Dem_MainFunction(void);

Dem_InitStateType Dem_GetInitState(void);
boolean Dem_IsReady(void);

Std_ReturnType Dem_SetEventStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus);
Std_ReturnType Dem_ReportErrorStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus);
Std_ReturnType Dem_ResetEventStatus(Dem_EventIdType EventId);

Std_ReturnType Dem_GetEventUdsStatus(
    Dem_EventIdType EventId,
    Dem_UdsStatusByteType *EventStatusByte
);

Std_ReturnType Dem_GetEventFailed(
    Dem_EventIdType EventId,
    boolean *EventFailed
);

Std_ReturnType Dem_GetEventTested(
    Dem_EventIdType EventId,
    boolean *EventTested
);

Std_ReturnType Dem_GetFaultDetectionCounter(
    Dem_EventIdType EventId,
    sint8 *FaultDetectionCounter
);

Std_ReturnType Dem_GetDTCOfEvent(
    Dem_EventIdType EventId,
    Dem_DTCFormatType DTCFormat,
    Dem_DTCType *DTCOfEvent
);

Std_ReturnType Dem_SetOperationCycleState(
    Dem_OperationCycleIdType OperationCycleId,
    Dem_OperationCycleStateType CycleState
);

Std_ReturnType Dem_GetStatusOfDTC(
    Dem_DTCType DTC,
    Dem_DTCFormatType DTCFormat,
    Dem_DTCOriginType DTCOrigin,
    Dem_UdsStatusByteType *DTCStatus
);

Std_ReturnType Dem_ClearDTC(
    Dem_DTCType DTC,
    Dem_DTCFormatType DTCFormat,
    Dem_DTCOriginType DTCOrigin
);

Dem_ClearDTCStatusType Dem_GetClearDTCStatus(void);

Std_ReturnType Dem_SetDTCFilter(
    uint8 DTCStatusMask,
    Dem_DTCFormatType DTCFormat,
    Dem_DTCOriginType DTCOrigin,
    boolean FilterWithSeverity,
    Dem_DTCSeverityType DTCSeverityMask,
    boolean FilterForFaultDetectionCounter
);

Std_ReturnType Dem_GetNumberOfFilteredDTC(uint16 *NumberOfFilteredDTC);

Std_ReturnType Dem_GetNextFilteredDTC(
    Dem_DTCType *DTC,
    Dem_UdsStatusByteType *DTCStatus
);

Std_ReturnType Dem_GetFreezeFrameDataByDTC(
    Dem_DTCType DTC,
    Dem_DTCFormatType DTCFormat,
    Dem_DTCOriginType DTCOrigin,
    uint8 RecordNumber,
    uint8 *DestBuffer,
    uint16 *BufSize
);

Std_ReturnType Dem_GetExtendedDataRecordByDTC(
    Dem_DTCType DTC,
    Dem_DTCFormatType DTCFormat,
    Dem_DTCOriginType DTCOrigin,
    uint8 RecordNumber,
    uint8 *DestBuffer,
    uint16 *BufSize
);

uint8 Dem_GetDTCStatusAvailabilityMask(void);

#endif
