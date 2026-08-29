#ifndef LINDIAG_H
#define LINDIAG_H

#include "Std_Types.h"
#include "Dem_Types.h"
#include "Lin.h"

#define LINDIAG_CHANNEL_COUNT        1u
#define LINDIAG_SLAVE_HVDCDC         4u

typedef enum
{
    LINDIAG_FAULT_SLAVE_NO_RESPONSE = 0u,
    LINDIAG_FAULT_PROTOCOL_ERROR,
    LINDIAG_FAULT_CONTROLLER_FAULT,
    LINDIAG_FAULT_WAKEUP_FAILURE,
    LINDIAG_FAULT_SLEEP_FAILURE,
    LINDIAG_FAULT_COUNT
} LinDiag_FaultType;

typedef enum
{
    LINDIAG_ERROR_NONE = 0u,
    LINDIAG_ERROR_NO_RESPONSE,
    LINDIAG_ERROR_TIMEOUT,
    LINDIAG_ERROR_CHECKSUM,
    LINDIAG_ERROR_PID,
    LINDIAG_ERROR_FRAMING,
    LINDIAG_ERROR_SYNC,
    LINDIAG_ERROR_HEADER,
    LINDIAG_ERROR_CONTROLLER,
    LINDIAG_ERROR_DIAG_TIMEOUT,
    LINDIAG_ERROR_SCHEDULE
} LinDiag_ErrorClassType;

void LinDiag_Init(void);
void LinDiag_MainFunction(void);

void LinDiag_ReportFrameResult(uint8 channel,
                               uint8 scheduleId,
                               uint8 frameId,
                               uint8 pid,
                               uint8 publisherNad,
                               uint8 slaveResponseExpected,
                               Lin_ResultType result);
void LinDiag_ReportControllerFault(uint8 channel);
void LinDiag_ReportWakeupFailure(uint8 channel);
void LinDiag_ReportSleepFailure(uint8 channel);

uint8 LinDiag_IsChannelUnavailable(uint8 channel);
Std_ReturnType LinDiag_CaptureSnapshotData(Dem_EventIdType eventId, uint8 *buffer, uint16 *length);

#endif
