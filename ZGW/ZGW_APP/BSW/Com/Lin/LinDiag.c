#include "LinDiag.h"
#include "ComM.h"
#include "Dem.h"
#include "Dem_Cfg.h"
#include "GatewaySwc.h"
#include "LinIf.h"
#include "LinSM.h"
#include <string.h>

#define LINDIAG_SNAPSHOT_SIZE                    64u
#define LINDIAG_SNAPSHOT_KIND_OFFSET             DEM_DTC_TIMESTAMP_DATA_SIZE
#define LINDIAG_SNAPSHOT_CHANNEL_OFFSET          (DEM_DTC_TIMESTAMP_DATA_SIZE + 1u)
#define LINDIAG_SNAPSHOT_FAULT_OFFSET            (DEM_DTC_TIMESTAMP_DATA_SIZE + 2u)
#define LINDIAG_SNAPSHOT_FRAME_ID_OFFSET         (DEM_DTC_TIMESTAMP_DATA_SIZE + 3u)
#define LINDIAG_SNAPSHOT_PID_OFFSET              (DEM_DTC_TIMESTAMP_DATA_SIZE + 4u)
#define LINDIAG_SNAPSHOT_SLAVE_NAD_OFFSET        (DEM_DTC_TIMESTAMP_DATA_SIZE + 5u)
#define LINDIAG_SNAPSHOT_SCHEDULE_OFFSET         (DEM_DTC_TIMESTAMP_DATA_SIZE + 6u)
#define LINDIAG_SNAPSHOT_LINSM_STATE_OFFSET      (DEM_DTC_TIMESTAMP_DATA_SIZE + 7u)
#define LINDIAG_SNAPSHOT_COMM_MODE_OFFSET        (DEM_DTC_TIMESTAMP_DATA_SIZE + 8u)
#define LINDIAG_SNAPSHOT_LIN_STATE_OFFSET        (DEM_DTC_TIMESTAMP_DATA_SIZE + 9u)
#define LINDIAG_SNAPSHOT_LAST_RESULT_OFFSET      (DEM_DTC_TIMESTAMP_DATA_SIZE + 10u)
#define LINDIAG_SNAPSHOT_NO_RESPONSE_COUNT_OFFSET (DEM_DTC_TIMESTAMP_DATA_SIZE + 11u)
#define LINDIAG_SNAPSHOT_PROTOCOL_COUNT_OFFSET   (DEM_DTC_TIMESTAMP_DATA_SIZE + 12u)
#define LINDIAG_SNAPSHOT_CONTROLLER_COUNT_OFFSET (DEM_DTC_TIMESTAMP_DATA_SIZE + 13u)
#define LINDIAG_SNAPSHOT_WAKEUP_COUNT_OFFSET     (DEM_DTC_TIMESTAMP_DATA_SIZE + 14u)
#define LINDIAG_SNAPSHOT_SLEEP_COUNT_OFFSET      (DEM_DTC_TIMESTAMP_DATA_SIZE + 15u)
#define LINDIAG_SNAPSHOT_CHANNEL_UNAVAILABLE_OFFSET (DEM_DTC_TIMESTAMP_DATA_SIZE + 16u)
#define LINDIAG_SNAPSHOT_ERROR_CLASS_OFFSET      (DEM_DTC_TIMESTAMP_DATA_SIZE + 17u)
#define LINDIAG_SNAPSHOT_CHECKSUM_COUNT_OFFSET   (DEM_DTC_TIMESTAMP_DATA_SIZE + 18u)
#define LINDIAG_SNAPSHOT_PID_COUNT_OFFSET        (DEM_DTC_TIMESTAMP_DATA_SIZE + 19u)
#define LINDIAG_SNAPSHOT_FRAMING_COUNT_OFFSET    (DEM_DTC_TIMESTAMP_DATA_SIZE + 20u)
#define LINDIAG_SNAPSHOT_SYNC_COUNT_OFFSET       (DEM_DTC_TIMESTAMP_DATA_SIZE + 21u)
#define LINDIAG_SNAPSHOT_HEADER_COUNT_OFFSET     (DEM_DTC_TIMESTAMP_DATA_SIZE + 22u)
#define LINDIAG_SNAPSHOT_SCHEDULE_ERROR_OFFSET   (DEM_DTC_TIMESTAMP_DATA_SIZE + 23u)
#define LINDIAG_SNAPSHOT_DIAG_TIMEOUT_OFFSET     (DEM_DTC_TIMESTAMP_DATA_SIZE + 27u)
#define LINDIAG_SNAPSHOT_LINIF_STATE_OFFSET      (DEM_DTC_TIMESTAMP_DATA_SIZE + 31u)
#define LINDIAG_SNAPSHOT_DIAG_STATE_OFFSET       (DEM_DTC_TIMESTAMP_DATA_SIZE + 32u)
#define LINDIAG_SNAPSHOT_COMM_RX_ENABLED_OFFSET  (DEM_DTC_TIMESTAMP_DATA_SIZE + 33u)
#define LINDIAG_SNAPSHOT_COMM_TX_ENABLED_OFFSET  (DEM_DTC_TIMESTAMP_DATA_SIZE + 34u)
#define LINDIAG_SNAPSHOT_SLAVE_EXPECTED_OFFSET   (DEM_DTC_TIMESTAMP_DATA_SIZE + 35u)
#define LINDIAG_SNAPSHOT_DIAG_SCHEDULE_ACTIVE_OFFSET (DEM_DTC_TIMESTAMP_DATA_SIZE + 36u)

typedef struct
{
    uint8 lastSchedule;
    uint8 lastFrameId;
    uint8 lastPid;
    uint8 lastSlaveNad;
    Lin_ResultType lastResult;
    uint8 noResponseCount;
    uint8 protocolErrorCount;
    uint8 controllerFaultCount;
    uint8 wakeupFailureCount;
    uint8 sleepFailureCount;
    uint8 channelUnavailable;
    uint8 lastSlaveResponseExpected;
    uint8 lastCommRxEnabled;
    uint8 lastCommTxEnabled;
    uint8 lastDiagActive;
    LinDiag_ErrorClassType lastErrorClass;
    uint8 checksumErrorCount;
    uint8 pidErrorCount;
    uint8 framingErrorCount;
    uint8 syncErrorCount;
    uint8 headerErrorCount;
    uint32 lastScheduleErrorCount;
    uint32 lastDiagTimeoutCount;
} LinDiag_ChannelRuntimeType;

static LinDiag_ChannelRuntimeType LinDiag_Runtime[LINDIAG_CHANNEL_COUNT];

static LinDiag_ErrorClassType LinDiag_ClassifyResult(Lin_ResultType result)
{
    switch (result)
    {
        case LIN_RES_NO_RESPONSE:
            return LINDIAG_ERROR_NO_RESPONSE;
        case LIN_RES_TIMEOUT:
            return LINDIAG_ERROR_TIMEOUT;
        case LIN_RES_CHECKSUM_ERROR:
            return LINDIAG_ERROR_CHECKSUM;
        case LIN_RES_PID_ERROR:
            return LINDIAG_ERROR_PID;
        case LIN_RES_FRAMING_ERROR:
            return LINDIAG_ERROR_FRAMING;
        case LIN_RES_SYNC_ERROR:
            return LINDIAG_ERROR_SYNC;
        case LIN_RES_HEADER_ERROR:
            return LINDIAG_ERROR_HEADER;
        case LIN_RES_NOT_OK:
            return LINDIAG_ERROR_CONTROLLER;
        default:
            return LINDIAG_ERROR_NONE;
    }
}

static Dem_EventIdType LinDiag_GetEventId(uint8 channel, LinDiag_FaultType fault)
{
    if ((channel >= LINDIAG_CHANNEL_COUNT) || ((uint8)fault >= (uint8)LINDIAG_FAULT_COUNT))
    {
        return 0u;
    }

    return (Dem_EventIdType)(DEM_EVENT_ID_LIN_BUS_DIAG_FIRST +
            ((Dem_EventIdType)channel * (Dem_EventIdType)LINDIAG_FAULT_COUNT) +
            (Dem_EventIdType)fault);
}

static uint8 LinDiag_DecodeEventId(Dem_EventIdType eventId, uint8 *channel, LinDiag_FaultType *fault)
{
    Dem_EventIdType offset;

    if ((channel == NULL_PTR) || (fault == NULL_PTR) ||
        (eventId < DEM_EVENT_ID_LIN_BUS_DIAG_FIRST) ||
        (eventId > DEM_EVENT_ID_LIN_BUS_DIAG_LAST))
    {
        return FALSE;
    }

    offset = (Dem_EventIdType)(eventId - DEM_EVENT_ID_LIN_BUS_DIAG_FIRST);
    *channel = (uint8)(offset / (Dem_EventIdType)LINDIAG_FAULT_COUNT);
    *fault = (LinDiag_FaultType)(offset % (Dem_EventIdType)LINDIAG_FAULT_COUNT);

    return (*channel < LINDIAG_CHANNEL_COUNT) ? TRUE : FALSE;
}

static uint8 LinDiag_IsOperational(uint8 channel)
{
    ComM_ModeType commMode = COMM_NO_COMMUNICATION;
    LinSM_StateType linSmState;
    LinIf_DiagStateType diagState;
    uint8 activeSchedule;

    if (channel >= LINDIAG_CHANNEL_COUNT)
    {
        return FALSE;
    }

    linSmState = LinSM_GetState(channel);
    if (linSmState != LINSM_FULL_COMMUNICATION)
    {
        return FALSE;
    }

    if ((GatewaySwc_IsNormalCommunicationRxEnabled() == FALSE) ||
        (GatewaySwc_IsNormalCommunicationTxEnabled() == FALSE))
    {
        return FALSE;
    }

    if (ComM_GetCurrentComMode(COMM_CH_LIN, &commMode) != E_OK)
    {
        return FALSE;
    }

    if (commMode != COMM_FULL_COMMUNICATION)
    {
        return FALSE;
    }

    diagState = LinIf_GetDiagState();
    if ((diagState != LINIF_DIAG_IDLE) &&
        (diagState != LINIF_DIAG_DONE) &&
        (diagState != LINIF_DIAG_ERROR))
    {
        return FALSE;
    }

    activeSchedule = LinIf_GetActiveSchedule();
    if (activeSchedule != LINIF_SCHED_NORMAL)
    {
        return FALSE;
    }

    return TRUE;
}

static void LinDiag_Report(uint8 channel, LinDiag_FaultType fault, Dem_EventStatusType status)
{
    Dem_EventIdType eventId = LinDiag_GetEventId(channel, fault);

    if (eventId != 0u)
    {
        (void)Dem_ReportErrorStatus(eventId, status);
    }
}

void LinDiag_Init(void)
{
    memset(LinDiag_Runtime, 0, sizeof(LinDiag_Runtime));
}

void LinDiag_ReportFrameResult(uint8 channel,
                               uint8 scheduleId,
                               uint8 frameId,
                               uint8 pid,
                               uint8 publisherNad,
                               uint8 slaveResponseExpected,
                               Lin_ResultType result)
{
    LinDiag_ChannelRuntimeType *runtime;

    if (channel >= LINDIAG_CHANNEL_COUNT)
    {
        return;
    }

    runtime = &LinDiag_Runtime[channel];
    runtime->lastSchedule = scheduleId;
    runtime->lastFrameId = frameId;
    runtime->lastPid = pid;
    runtime->lastSlaveNad = publisherNad;
    runtime->lastResult = result;
    runtime->lastErrorClass = LinDiag_ClassifyResult(result);
    runtime->lastSlaveResponseExpected = slaveResponseExpected;
    runtime->lastCommRxEnabled = (GatewaySwc_IsNormalCommunicationRxEnabled() != FALSE) ? TRUE : FALSE;
    runtime->lastCommTxEnabled = (GatewaySwc_IsNormalCommunicationTxEnabled() != FALSE) ? TRUE : FALSE;
    runtime->lastDiagActive = (LinIf_GetActiveSchedule() != LINIF_SCHED_NORMAL) ? TRUE : FALSE;

    if ((scheduleId != LINIF_SCHED_NORMAL) || (slaveResponseExpected == FALSE) ||
        (LinDiag_IsOperational(channel) == FALSE))
    {
        return;
    }

    if (result == LIN_RES_OK)
    {
        runtime->lastErrorClass = LINDIAG_ERROR_NONE;
        LinDiag_Report(channel, LINDIAG_FAULT_SLAVE_NO_RESPONSE, DEM_EVENT_STATUS_PREPASSED);
        LinDiag_Report(channel, LINDIAG_FAULT_PROTOCOL_ERROR, DEM_EVENT_STATUS_PREPASSED);
        LinDiag_Report(channel, LINDIAG_FAULT_CONTROLLER_FAULT, DEM_EVENT_STATUS_PREPASSED);
        return;
    }

    if ((result == LIN_RES_NO_RESPONSE) || (result == LIN_RES_TIMEOUT))
    {
        runtime->noResponseCount++;
        runtime->channelUnavailable = TRUE;
        LinDiag_Report(channel, LINDIAG_FAULT_SLAVE_NO_RESPONSE, DEM_EVENT_STATUS_PREFAILED);
        return;
    }

    if ((result == LIN_RES_CHECKSUM_ERROR) ||
        (result == LIN_RES_PID_ERROR) ||
        (result == LIN_RES_FRAMING_ERROR) ||
        (result == LIN_RES_SYNC_ERROR) ||
        (result == LIN_RES_HEADER_ERROR))
    {
        runtime->protocolErrorCount++;
        if (result == LIN_RES_CHECKSUM_ERROR)
        {
            runtime->checksumErrorCount++;
        }
        else if (result == LIN_RES_PID_ERROR)
        {
            runtime->pidErrorCount++;
        }
        else if (result == LIN_RES_FRAMING_ERROR)
        {
            runtime->framingErrorCount++;
        }
        else if (result == LIN_RES_SYNC_ERROR)
        {
            runtime->syncErrorCount++;
        }
        else
        {
            runtime->headerErrorCount++;
        }
        LinDiag_Report(channel, LINDIAG_FAULT_PROTOCOL_ERROR, DEM_EVENT_STATUS_PREFAILED);
        return;
    }

    runtime->controllerFaultCount++;
    runtime->channelUnavailable = TRUE;
    LinDiag_Report(channel, LINDIAG_FAULT_CONTROLLER_FAULT, DEM_EVENT_STATUS_PREFAILED);
}

void LinDiag_ReportWakeupFailure(uint8 channel)
{
    if (channel < LINDIAG_CHANNEL_COUNT)
    {
        LinDiag_Runtime[channel].wakeupFailureCount++;
        LinDiag_Runtime[channel].channelUnavailable = TRUE;
        LinDiag_Report(channel, LINDIAG_FAULT_WAKEUP_FAILURE, DEM_EVENT_STATUS_FAILED);
    }
}

void LinDiag_ReportSleepFailure(uint8 channel)
{
    if (channel < LINDIAG_CHANNEL_COUNT)
    {
        LinDiag_Runtime[channel].sleepFailureCount++;
        LinDiag_Report(channel, LINDIAG_FAULT_SLEEP_FAILURE, DEM_EVENT_STATUS_FAILED);
    }
}

void LinDiag_MainFunction(void)
{
    uint8 channel;
    uint32 scheduleErrorCounter;
    uint32 diagTimeoutCounter;

    for (channel = 0u; channel < LINDIAG_CHANNEL_COUNT; channel++)
    {
        LinDiag_Runtime[channel].lastCommRxEnabled =
                (GatewaySwc_IsNormalCommunicationRxEnabled() != FALSE) ? TRUE : FALSE;
        LinDiag_Runtime[channel].lastCommTxEnabled =
                (GatewaySwc_IsNormalCommunicationTxEnabled() != FALSE) ? TRUE : FALSE;
        LinDiag_Runtime[channel].lastDiagActive =
                (LinIf_GetActiveSchedule() != LINIF_SCHED_NORMAL) ? TRUE : FALSE;

        scheduleErrorCounter = LinIf_GetScheduleErrorCounter();
        diagTimeoutCounter = LinIf_GetDiagTimeoutCounter();
        if (scheduleErrorCounter > LinDiag_Runtime[channel].lastScheduleErrorCount)
        {
            LinDiag_Runtime[channel].lastErrorClass = LINDIAG_ERROR_SCHEDULE;
        }
        if (diagTimeoutCounter > LinDiag_Runtime[channel].lastDiagTimeoutCount)
        {
            LinDiag_Runtime[channel].lastErrorClass = LINDIAG_ERROR_DIAG_TIMEOUT;
        }
        LinDiag_Runtime[channel].lastScheduleErrorCount = scheduleErrorCounter;
        LinDiag_Runtime[channel].lastDiagTimeoutCount = diagTimeoutCounter;

        if (LinDiag_IsOperational(channel) != FALSE)
        {
            LinDiag_Report(channel, LINDIAG_FAULT_SLAVE_NO_RESPONSE, DEM_EVENT_STATUS_PREPASSED);
            LinDiag_Report(channel, LINDIAG_FAULT_PROTOCOL_ERROR, DEM_EVENT_STATUS_PREPASSED);
            LinDiag_Report(channel, LINDIAG_FAULT_CONTROLLER_FAULT, DEM_EVENT_STATUS_PREPASSED);
            LinDiag_Report(channel, LINDIAG_FAULT_WAKEUP_FAILURE, DEM_EVENT_STATUS_PREPASSED);
            LinDiag_Report(channel, LINDIAG_FAULT_SLEEP_FAILURE, DEM_EVENT_STATUS_PREPASSED);
        }

        if ((LinDiag_IsOperational(channel) != FALSE) &&
            (LinDiag_Runtime[channel].channelUnavailable != FALSE) &&
            (LinDiag_Runtime[channel].lastResult == LIN_RES_OK))
        {
            LinDiag_Runtime[channel].channelUnavailable = FALSE;
        }
    }
}

uint8 LinDiag_IsChannelUnavailable(uint8 channel)
{
    if (channel >= LINDIAG_CHANNEL_COUNT)
    {
        return FALSE;
    }

    return LinDiag_Runtime[channel].channelUnavailable;
}

Std_ReturnType LinDiag_CaptureSnapshotData(Dem_EventIdType eventId, uint8 *buffer, uint16 *length)
{
    uint8 channel;
    LinDiag_FaultType fault;
    ComM_ModeType commMode = COMM_NO_COMMUNICATION;
    uint16 captureLength;

    if ((buffer == NULL_PTR) || (length == NULL_PTR) || (*length < LINDIAG_SNAPSHOT_SIZE))
    {
        return E_NOT_OK;
    }

    if (LinDiag_DecodeEventId(eventId, &channel, &fault) == FALSE)
    {
        return E_NOT_OK;
    }

    captureLength = *length;
    if (Dem_Cfg_CaptureTimestampTemperatureData(buffer, &captureLength, DEM_SNAPSHOT_KIND_LIN_BUS) != E_OK)
    {
        return E_NOT_OK;
    }

    (void)ComM_GetCurrentComMode(COMM_CH_LIN, &commMode);

    buffer[LINDIAG_SNAPSHOT_KIND_OFFSET] = DEM_SNAPSHOT_KIND_LIN_BUS;
    buffer[LINDIAG_SNAPSHOT_CHANNEL_OFFSET] = channel;
    buffer[LINDIAG_SNAPSHOT_FAULT_OFFSET] = (uint8)fault;
    buffer[LINDIAG_SNAPSHOT_FRAME_ID_OFFSET] = LinDiag_Runtime[channel].lastFrameId;
    buffer[LINDIAG_SNAPSHOT_PID_OFFSET] = LinDiag_Runtime[channel].lastPid;
    buffer[LINDIAG_SNAPSHOT_SLAVE_NAD_OFFSET] = LinDiag_Runtime[channel].lastSlaveNad;
    buffer[LINDIAG_SNAPSHOT_SCHEDULE_OFFSET] = LinDiag_Runtime[channel].lastSchedule;
    buffer[LINDIAG_SNAPSHOT_LINSM_STATE_OFFSET] = (uint8)LinSM_GetState(channel);
    buffer[LINDIAG_SNAPSHOT_COMM_MODE_OFFSET] = (uint8)commMode;
    buffer[LINDIAG_SNAPSHOT_LIN_STATE_OFFSET] = (uint8)Lin_GetState(channel);
    buffer[LINDIAG_SNAPSHOT_LAST_RESULT_OFFSET] = (uint8)LinDiag_Runtime[channel].lastResult;
    buffer[LINDIAG_SNAPSHOT_NO_RESPONSE_COUNT_OFFSET] = LinDiag_Runtime[channel].noResponseCount;
    buffer[LINDIAG_SNAPSHOT_PROTOCOL_COUNT_OFFSET] = LinDiag_Runtime[channel].protocolErrorCount;
    buffer[LINDIAG_SNAPSHOT_CONTROLLER_COUNT_OFFSET] = LinDiag_Runtime[channel].controllerFaultCount;
    buffer[LINDIAG_SNAPSHOT_WAKEUP_COUNT_OFFSET] = LinDiag_Runtime[channel].wakeupFailureCount;
    buffer[LINDIAG_SNAPSHOT_SLEEP_COUNT_OFFSET] = LinDiag_Runtime[channel].sleepFailureCount;
    buffer[LINDIAG_SNAPSHOT_CHANNEL_UNAVAILABLE_OFFSET] = LinDiag_Runtime[channel].channelUnavailable;
    buffer[LINDIAG_SNAPSHOT_ERROR_CLASS_OFFSET] = (uint8)LinDiag_Runtime[channel].lastErrorClass;
    buffer[LINDIAG_SNAPSHOT_CHECKSUM_COUNT_OFFSET] = LinDiag_Runtime[channel].checksumErrorCount;
    buffer[LINDIAG_SNAPSHOT_PID_COUNT_OFFSET] = LinDiag_Runtime[channel].pidErrorCount;
    buffer[LINDIAG_SNAPSHOT_FRAMING_COUNT_OFFSET] = LinDiag_Runtime[channel].framingErrorCount;
    buffer[LINDIAG_SNAPSHOT_SYNC_COUNT_OFFSET] = LinDiag_Runtime[channel].syncErrorCount;
    buffer[LINDIAG_SNAPSHOT_HEADER_COUNT_OFFSET] = LinDiag_Runtime[channel].headerErrorCount;
    buffer[LINDIAG_SNAPSHOT_LINIF_STATE_OFFSET] = (uint8)LinIf_GetChannelState();
    buffer[LINDIAG_SNAPSHOT_DIAG_STATE_OFFSET] = (uint8)LinIf_GetDiagState();
    buffer[LINDIAG_SNAPSHOT_COMM_RX_ENABLED_OFFSET] = LinDiag_Runtime[channel].lastCommRxEnabled;
    buffer[LINDIAG_SNAPSHOT_COMM_TX_ENABLED_OFFSET] = LinDiag_Runtime[channel].lastCommTxEnabled;
    buffer[LINDIAG_SNAPSHOT_SLAVE_EXPECTED_OFFSET] = LinDiag_Runtime[channel].lastSlaveResponseExpected;
    buffer[LINDIAG_SNAPSHOT_DIAG_SCHEDULE_ACTIVE_OFFSET] = LinDiag_Runtime[channel].lastDiagActive;
    buffer[LINDIAG_SNAPSHOT_SCHEDULE_ERROR_OFFSET] = 0u;
    buffer[LINDIAG_SNAPSHOT_SCHEDULE_ERROR_OFFSET + 1u] = 0u;
    buffer[LINDIAG_SNAPSHOT_SCHEDULE_ERROR_OFFSET + 2u] = 0u;
    buffer[LINDIAG_SNAPSHOT_SCHEDULE_ERROR_OFFSET + 3u] = 0u;
    buffer[LINDIAG_SNAPSHOT_DIAG_TIMEOUT_OFFSET] = 0u;
    buffer[LINDIAG_SNAPSHOT_DIAG_TIMEOUT_OFFSET + 1u] = 0u;
    buffer[LINDIAG_SNAPSHOT_DIAG_TIMEOUT_OFFSET + 2u] = 0u;
    buffer[LINDIAG_SNAPSHOT_DIAG_TIMEOUT_OFFSET + 3u] = 0u;
    {
        uint32 scheduleErrors = LinIf_GetScheduleErrorCounter();
        uint32 diagTimeouts = LinIf_GetDiagTimeoutCounter();

        buffer[LINDIAG_SNAPSHOT_SCHEDULE_ERROR_OFFSET] = (uint8)((scheduleErrors >> 24u) & 0xFFu);
        buffer[LINDIAG_SNAPSHOT_SCHEDULE_ERROR_OFFSET + 1u] = (uint8)((scheduleErrors >> 16u) & 0xFFu);
        buffer[LINDIAG_SNAPSHOT_SCHEDULE_ERROR_OFFSET + 2u] = (uint8)((scheduleErrors >> 8u) & 0xFFu);
        buffer[LINDIAG_SNAPSHOT_SCHEDULE_ERROR_OFFSET + 3u] = (uint8)(scheduleErrors & 0xFFu);
        buffer[LINDIAG_SNAPSHOT_DIAG_TIMEOUT_OFFSET] = (uint8)((diagTimeouts >> 24u) & 0xFFu);
        buffer[LINDIAG_SNAPSHOT_DIAG_TIMEOUT_OFFSET + 1u] = (uint8)((diagTimeouts >> 16u) & 0xFFu);
        buffer[LINDIAG_SNAPSHOT_DIAG_TIMEOUT_OFFSET + 2u] = (uint8)((diagTimeouts >> 8u) & 0xFFu);
        buffer[LINDIAG_SNAPSHOT_DIAG_TIMEOUT_OFFSET + 3u] = (uint8)(diagTimeouts & 0xFFu);
    }

    *length = LINDIAG_SNAPSHOT_SIZE;
    return E_OK;
}
