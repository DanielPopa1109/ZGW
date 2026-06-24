#ifndef DEM_H_
#define DEM_H_

#include "Dem_Types.h"
#include "Dem_Cfg.h"

#define DEM_EVENT_STATUS_DEBUG_COUNT    4u

typedef struct
{
    Dem_EventIdType eventId;
    Dem_DTCType dtc;
    Dem_UdsStatusByteType udsStatus;
    sint16 debounceCounter;
    uint8 confirmationCounter;
    uint8 occurrenceCounter;
    uint8 agingCounter;
    uint32 setPassedCount;
    uint32 setFailedCount;
    uint32 setPrePassedCount;
    uint32 setPreFailedCount;
    uint32 statusChangeCount;
} Dem_DebugEventStatusType;

extern volatile uint32 Dem_ChangeCounter;
extern volatile uint16 Dem_DtcStatusListCount;
extern volatile Dem_DebugEventStatusType Dem_DtcStatusList[DEM_MAX_EVENTS];
extern volatile uint32 Dem_SetEventStatusCounter[DEM_MAX_EVENTS][DEM_EVENT_STATUS_DEBUG_COUNT];
extern volatile uint32 Dem_EventStatusChangeCounter[DEM_MAX_EVENTS];

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

Std_ReturnType Dem_GetSnapshotDataByDTC(
    Dem_DTCType DTC,
    Dem_DTCFormatType DTCFormat,
    Dem_DTCOriginType DTCOrigin,
    uint8 RecordNumber,
    uint8 *DestBuffer,
    uint16 *BufSize
);

uint8 Dem_GetDTCStatusAvailabilityMask(void);

#endif
