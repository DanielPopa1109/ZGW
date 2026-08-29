#include "CanDiag.h"
#include "CanIf.h"
#include "CanSM.h"
#include "ComM.h"
#include "Dem.h"
#include "Dem_Cfg.h"
#include "GatewaySwc.h"

#define CANDIAG_ERROR_PASSIVE_FAIL_TICKS       100u
#define CANDIAG_ERROR_PASSIVE_PASS_TICKS       40u
#define CANDIAG_PROTOCOL_ERROR_FAIL_TICKS      200u
#define CANDIAG_PROTOCOL_ERROR_PASS_TICKS      80u
#define CANDIAG_ERROR_COUNTER_HIGH_LIMIT       160u

#define CANDIAG_SNAPSHOT_SIZE                  48u
#define CANDIAG_SNAPSHOT_KIND_OFFSET           DEM_DTC_TIMESTAMP_DATA_SIZE
#define CANDIAG_SNAPSHOT_CONTROLLER_OFFSET     (DEM_DTC_TIMESTAMP_DATA_SIZE + 1u)
#define CANDIAG_SNAPSHOT_FAULT_OFFSET          (DEM_DTC_TIMESTAMP_DATA_SIZE + 2u)
#define CANDIAG_SNAPSHOT_CAN_STATE_OFFSET      (DEM_DTC_TIMESTAMP_DATA_SIZE + 3u)
#define CANDIAG_SNAPSHOT_ERROR_STATE_OFFSET    (DEM_DTC_TIMESTAMP_DATA_SIZE + 4u)
#define CANDIAG_SNAPSHOT_TEC_OFFSET            (DEM_DTC_TIMESTAMP_DATA_SIZE + 5u)
#define CANDIAG_SNAPSHOT_REC_OFFSET            (DEM_DTC_TIMESTAMP_DATA_SIZE + 6u)
#define CANDIAG_SNAPSHOT_CANSM_STATE_OFFSET    (DEM_DTC_TIMESTAMP_DATA_SIZE + 7u)
#define CANDIAG_SNAPSHOT_COMM_MODE_OFFSET      (DEM_DTC_TIMESTAMP_DATA_SIZE + 8u)
#define CANDIAG_SNAPSHOT_PDU_MODE_OFFSET       (DEM_DTC_TIMESTAMP_DATA_SIZE + 9u)
#define CANDIAG_SNAPSHOT_BUSOFF_COUNT_OFFSET   (DEM_DTC_TIMESTAMP_DATA_SIZE + 10u)
#define CANDIAG_SNAPSHOT_OPERATIONAL_OFFSET    (DEM_DTC_TIMESTAMP_DATA_SIZE + 14u)
#define CANDIAG_SNAPSHOT_RX_ENABLED_OFFSET     (DEM_DTC_TIMESTAMP_DATA_SIZE + 15u)
#define CANDIAG_SNAPSHOT_TX_ENABLED_OFFSET     (DEM_DTC_TIMESTAMP_DATA_SIZE + 16u)
#define CANDIAG_SNAPSHOT_CONTROLLER_PENDING    (DEM_DTC_TIMESTAMP_DATA_SIZE + 17u)
#define CANDIAG_SNAPSHOT_RECOVERED_PENDING     (DEM_DTC_TIMESTAMP_DATA_SIZE + 18u)
#define CANDIAG_SNAPSHOT_ERROR_PASSIVE_FAIL    (DEM_DTC_TIMESTAMP_DATA_SIZE + 19u)
#define CANDIAG_SNAPSHOT_ERROR_PASSIVE_PASS    (DEM_DTC_TIMESTAMP_DATA_SIZE + 21u)
#define CANDIAG_SNAPSHOT_PROTOCOL_FAIL         (DEM_DTC_TIMESTAMP_DATA_SIZE + 23u)
#define CANDIAG_SNAPSHOT_PROTOCOL_PASS         (DEM_DTC_TIMESTAMP_DATA_SIZE + 25u)
#define CANDIAG_SNAPSHOT_ERROR_PASSIVE_SEEN    (DEM_DTC_TIMESTAMP_DATA_SIZE + 27u)

typedef struct
{
    uint8 busOffPending;
    uint8 recoveredPending;
    uint8 errorPassiveSeen;
    uint8 controllerFaultPending;
    uint16 errorPassiveFailTicks;
    uint16 errorPassivePassTicks;
    uint16 protocolFailTicks;
    uint16 protocolPassTicks;
    uint32 busOffCounter;
} CanDiag_ChannelRuntimeType;

static CanDiag_ChannelRuntimeType CanDiag_Runtime[CANDIAG_CHANNEL_COUNT];

static void CanDiag_StoreU16(uint8 *buffer, uint16 offset, uint16 value)
{
    buffer[offset] = (uint8)((value >> 8u) & 0xFFu);
    buffer[(uint16)(offset + 1u)] = (uint8)(value & 0xFFu);
}

static void CanDiag_StoreU32(uint8 *buffer, uint16 offset, uint32 value)
{
    buffer[offset] = (uint8)((value >> 24u) & 0xFFu);
    buffer[(uint16)(offset + 1u)] = (uint8)((value >> 16u) & 0xFFu);
    buffer[(uint16)(offset + 2u)] = (uint8)((value >> 8u) & 0xFFu);
    buffer[(uint16)(offset + 3u)] = (uint8)(value & 0xFFu);
}

static Dem_EventIdType CanDiag_GetEventId(uint8 controllerId, CanDiag_FaultType fault)
{
    if ((controllerId >= CANDIAG_CHANNEL_COUNT) || ((uint8)fault >= (uint8)CANDIAG_FAULT_COUNT))
    {
        return 0u;
    }

    return (Dem_EventIdType)(DEM_EVENT_ID_CAN_BUS_DIAG_FIRST +
            ((Dem_EventIdType)controllerId * (Dem_EventIdType)CANDIAG_FAULT_COUNT) +
            (Dem_EventIdType)fault);
}

static uint8 CanDiag_DecodeEventId(Dem_EventIdType eventId, uint8 *controllerId, CanDiag_FaultType *fault)
{
    Dem_EventIdType offset;

    if ((controllerId == NULL_PTR) || (fault == NULL_PTR) ||
        (eventId < DEM_EVENT_ID_CAN_BUS_DIAG_FIRST) ||
        (eventId > DEM_EVENT_ID_CAN_BUS_DIAG_LAST))
    {
        return FALSE;
    }

    offset = (Dem_EventIdType)(eventId - DEM_EVENT_ID_CAN_BUS_DIAG_FIRST);
    *controllerId = (uint8)(offset / (Dem_EventIdType)CANDIAG_FAULT_COUNT);
    *fault = (CanDiag_FaultType)(offset % (Dem_EventIdType)CANDIAG_FAULT_COUNT);

    return (*controllerId < CANDIAG_CHANNEL_COUNT) ? TRUE : FALSE;
}

static ComM_ChannelType CanDiag_GetComMChannel(uint8 controllerId)
{
    return (controllerId == CAN_CONTROLLER_FD) ? COMM_CH_CANFD : COMM_CH_CAN;
}

static uint8 CanDiag_IsChannelOperational(uint8 controllerId)
{
    ComM_ModeType commMode;
    CanSM_ComModeType canSmMode;
    CanSM_StateType canSmState;

    if (controllerId >= CANDIAG_CHANNEL_COUNT)
    {
        return FALSE;
    }

    canSmState = CanSM_GetState(controllerId);
    if ((canSmState != CANSM_BSM_FULL_COMMUNICATION) &&
        (canSmState != CANSM_BSM_SILENT_COMMUNICATION))
    {
        return FALSE;
    }

    if (CanSM_GetCurrentComMode(controllerId, &canSmMode) != E_OK)
    {
        return FALSE;
    }

    if ((canSmMode != CANSM_COMM_FULL_COMMUNICATION) &&
        (canSmMode != CANSM_COMM_SILENT_COMMUNICATION))
    {
        return FALSE;
    }

    if ((GatewaySwc_IsNormalCommunicationRxEnabled() == FALSE) ||
        (GatewaySwc_IsNormalCommunicationTxEnabled() == FALSE))
    {
        return FALSE;
    }

    if (ComM_GetCurrentComMode(CanDiag_GetComMChannel(controllerId), &commMode) != E_OK)
    {
        return FALSE;
    }

    return (commMode != COMM_NO_COMMUNICATION) ? TRUE : FALSE;
}

boolean CanDiag_IsChannelUnavailable(uint8 controllerId)
{
    return (CanDiag_IsChannelOperational(controllerId) == FALSE) ? TRUE : FALSE;
}

static void CanDiag_Report(uint8 controllerId, CanDiag_FaultType fault, Dem_EventStatusType status)
{
    Dem_EventIdType eventId = CanDiag_GetEventId(controllerId, fault);

    if (eventId != 0u)
    {
        (void)Dem_ReportErrorStatus(eventId, status);
    }
}

static void CanDiag_ReportAllPassed(uint8 controllerId)
{
    CanDiag_Report(controllerId, CANDIAG_FAULT_ERROR_PASSIVE, DEM_EVENT_STATUS_PREPASSED);
    CanDiag_Report(controllerId, CANDIAG_FAULT_PROTOCOL_ERROR, DEM_EVENT_STATUS_PREPASSED);
}

void CanDiag_Init(void)
{
    uint8 controller;

    for (controller = 0u; controller < CANDIAG_CHANNEL_COUNT; controller++)
    {
        CanDiag_Runtime[controller].busOffPending = FALSE;
        CanDiag_Runtime[controller].recoveredPending = FALSE;
        CanDiag_Runtime[controller].errorPassiveSeen = FALSE;
        CanDiag_Runtime[controller].controllerFaultPending = FALSE;
        CanDiag_Runtime[controller].errorPassiveFailTicks = 0u;
        CanDiag_Runtime[controller].errorPassivePassTicks = 0u;
        CanDiag_Runtime[controller].protocolFailTicks = 0u;
        CanDiag_Runtime[controller].protocolPassTicks = 0u;
        CanDiag_Runtime[controller].busOffCounter = 0u;
    }
}

void CanDiag_ReportBusOff(uint8 controllerId)
{
    if (controllerId < CANDIAG_CHANNEL_COUNT)
    {
        CanDiag_Runtime[controllerId].busOffPending = TRUE;
        CanDiag_Runtime[controllerId].busOffCounter++;
    }
}

void CanDiag_ReportControllerRecovered(uint8 controllerId)
{
    if (controllerId < CANDIAG_CHANNEL_COUNT)
    {
        CanDiag_Runtime[controllerId].recoveredPending = TRUE;
        CanDiag_Runtime[controllerId].errorPassiveSeen = FALSE;
    }
}

void CanDiag_ReportErrorPassive(uint8 controllerId)
{
    if (controllerId < CANDIAG_CHANNEL_COUNT)
    {
        CanDiag_Runtime[controllerId].errorPassiveSeen = TRUE;
    }
}

void CanDiag_ReportErrorWarning(uint8 controllerId)
{
    (void)controllerId;
}

void CanDiag_ReportControllerFault(uint8 controllerId)
{
    if (controllerId < CANDIAG_CHANNEL_COUNT)
    {
        CanDiag_Runtime[controllerId].controllerFaultPending = TRUE;
    }
}

static void CanDiag_ProcessBusOff(uint8 controllerId)
{
    if (CanDiag_Runtime[controllerId].busOffPending != FALSE)
    {
        CanDiag_Runtime[controllerId].busOffPending = FALSE;
        CanDiag_Report(controllerId, CANDIAG_FAULT_BUS_OFF, DEM_EVENT_STATUS_FAILED);
    }

    if (CanDiag_Runtime[controllerId].recoveredPending != FALSE)
    {
        CanDiag_Runtime[controllerId].recoveredPending = FALSE;
        CanDiag_Report(controllerId, CANDIAG_FAULT_BUS_OFF, DEM_EVENT_STATUS_PASSED);
    }
}

static void CanDiag_ProcessControllerFault(uint8 controllerId)
{
    if (CanDiag_Runtime[controllerId].controllerFaultPending != FALSE)
    {
        CanDiag_Runtime[controllerId].controllerFaultPending = FALSE;
        CanDiag_Report(controllerId, CANDIAG_FAULT_CONTROLLER, DEM_EVENT_STATUS_FAILED);
    }
    else if (CanDiag_IsChannelOperational(controllerId) != FALSE)
    {
        CanDiag_Report(controllerId, CANDIAG_FAULT_CONTROLLER, DEM_EVENT_STATUS_PREPASSED);
    }
}

static void CanDiag_ProcessErrorState(uint8 controllerId)
{
    uint8 rxErr = 0u;
    uint8 txErr = 0u;
    uint8 operational;
    Can_ErrorStateType errorState;

    operational = CanDiag_IsChannelOperational(controllerId);

    if (operational == FALSE)
    {
        CanDiag_Runtime[controllerId].errorPassiveFailTicks = 0u;
        CanDiag_Runtime[controllerId].protocolFailTicks = 0u;
        CanDiag_ReportAllPassed(controllerId);
        return;
    }

    (void)Can_GetControllerRxErrorCounter(controllerId, &rxErr);
    (void)Can_GetControllerTxErrorCounter(controllerId, &txErr);
    errorState = Can_GetControllerErrorState(controllerId);

    if ((errorState == CAN_ERROR_PASSIVE) || (CanDiag_Runtime[controllerId].errorPassiveSeen != FALSE))
    {
        CanDiag_Runtime[controllerId].errorPassivePassTicks = 0u;
        if (CanDiag_Runtime[controllerId].errorPassiveFailTicks < CANDIAG_ERROR_PASSIVE_FAIL_TICKS)
        {
            CanDiag_Runtime[controllerId].errorPassiveFailTicks++;
        }
        CanDiag_Report(controllerId, CANDIAG_FAULT_ERROR_PASSIVE, DEM_EVENT_STATUS_PREFAILED);
    }
    else
    {
        CanDiag_Runtime[controllerId].errorPassiveFailTicks = 0u;
        if (CanDiag_Runtime[controllerId].errorPassivePassTicks < CANDIAG_ERROR_PASSIVE_PASS_TICKS)
        {
            CanDiag_Runtime[controllerId].errorPassivePassTicks++;
        }
        if (CanDiag_Runtime[controllerId].errorPassivePassTicks >= CANDIAG_ERROR_PASSIVE_PASS_TICKS)
        {
            CanDiag_Report(controllerId, CANDIAG_FAULT_ERROR_PASSIVE, DEM_EVENT_STATUS_PREPASSED);
        }
    }

    CanDiag_Runtime[controllerId].errorPassiveSeen = FALSE;

    if ((txErr >= CANDIAG_ERROR_COUNTER_HIGH_LIMIT) || (rxErr >= CANDIAG_ERROR_COUNTER_HIGH_LIMIT))
    {
        CanDiag_Runtime[controllerId].protocolPassTicks = 0u;
        if (CanDiag_Runtime[controllerId].protocolFailTicks < CANDIAG_PROTOCOL_ERROR_FAIL_TICKS)
        {
            CanDiag_Runtime[controllerId].protocolFailTicks++;
        }
        CanDiag_Report(controllerId, CANDIAG_FAULT_PROTOCOL_ERROR, DEM_EVENT_STATUS_PREFAILED);
    }
    else
    {
        CanDiag_Runtime[controllerId].protocolFailTicks = 0u;
        if (CanDiag_Runtime[controllerId].protocolPassTicks < CANDIAG_PROTOCOL_ERROR_PASS_TICKS)
        {
            CanDiag_Runtime[controllerId].protocolPassTicks++;
        }
        if (CanDiag_Runtime[controllerId].protocolPassTicks >= CANDIAG_PROTOCOL_ERROR_PASS_TICKS)
        {
            CanDiag_Report(controllerId, CANDIAG_FAULT_PROTOCOL_ERROR, DEM_EVENT_STATUS_PREPASSED);
        }
    }
}

void CanDiag_MainFunction(void)
{
    uint8 controller;

    for (controller = 0u; controller < CANDIAG_CHANNEL_COUNT; controller++)
    {
        CanDiag_ProcessBusOff(controller);
        CanDiag_ProcessControllerFault(controller);
        CanDiag_ProcessErrorState(controller);
    }
}

Std_ReturnType CanDiag_CaptureSnapshotData(Dem_EventIdType eventId, uint8 *buffer, uint16 *length)
{
    uint8 controllerId;
    uint8 rxErr = 0u;
    uint8 txErr = 0u;
    CanDiag_FaultType fault;
    ComM_ModeType commMode = COMM_NO_COMMUNICATION;
    uint16 captureLength;

    if ((buffer == NULL_PTR) || (length == NULL_PTR) || (*length < CANDIAG_SNAPSHOT_SIZE))
    {
        return E_NOT_OK;
    }

    if (CanDiag_DecodeEventId(eventId, &controllerId, &fault) == FALSE)
    {
        return E_NOT_OK;
    }

    captureLength = *length;
    if (Dem_Cfg_CaptureTimestampTemperatureData(buffer, &captureLength, DEM_SNAPSHOT_KIND_CAN_BUS) != E_OK)
    {
        return E_NOT_OK;
    }

    (void)Can_GetControllerTxErrorCounter(controllerId, &txErr);
    (void)Can_GetControllerRxErrorCounter(controllerId, &rxErr);
    (void)ComM_GetCurrentComMode(CanDiag_GetComMChannel(controllerId), &commMode);

    buffer[CANDIAG_SNAPSHOT_KIND_OFFSET] = DEM_SNAPSHOT_KIND_CAN_BUS;
    buffer[CANDIAG_SNAPSHOT_CONTROLLER_OFFSET] = controllerId;
    buffer[CANDIAG_SNAPSHOT_FAULT_OFFSET] = (uint8)fault;
    buffer[CANDIAG_SNAPSHOT_CAN_STATE_OFFSET] = (uint8)Can_GetControllerState(controllerId);
    buffer[CANDIAG_SNAPSHOT_ERROR_STATE_OFFSET] = (uint8)Can_GetControllerErrorState(controllerId);
    buffer[CANDIAG_SNAPSHOT_TEC_OFFSET] = txErr;
    buffer[CANDIAG_SNAPSHOT_REC_OFFSET] = rxErr;
    buffer[CANDIAG_SNAPSHOT_CANSM_STATE_OFFSET] = (uint8)CanSM_GetState(controllerId);
    buffer[CANDIAG_SNAPSHOT_COMM_MODE_OFFSET] = (uint8)commMode;
    buffer[CANDIAG_SNAPSHOT_PDU_MODE_OFFSET] = CanIf_GetPduMode(controllerId);
    CanDiag_StoreU32(buffer,
            CANDIAG_SNAPSHOT_BUSOFF_COUNT_OFFSET,
            CanDiag_Runtime[controllerId].busOffCounter);
    buffer[CANDIAG_SNAPSHOT_OPERATIONAL_OFFSET] =
            (CanDiag_IsChannelOperational(controllerId) != FALSE) ? 1u : 0u;
    buffer[CANDIAG_SNAPSHOT_RX_ENABLED_OFFSET] =
            (GatewaySwc_IsNormalCommunicationRxEnabled() != FALSE) ? 1u : 0u;
    buffer[CANDIAG_SNAPSHOT_TX_ENABLED_OFFSET] =
            (GatewaySwc_IsNormalCommunicationTxEnabled() != FALSE) ? 1u : 0u;
    buffer[CANDIAG_SNAPSHOT_CONTROLLER_PENDING] =
            (CanDiag_Runtime[controllerId].controllerFaultPending != FALSE) ? 1u : 0u;
    buffer[CANDIAG_SNAPSHOT_RECOVERED_PENDING] =
            (CanDiag_Runtime[controllerId].recoveredPending != FALSE) ? 1u : 0u;
    CanDiag_StoreU16(buffer,
            CANDIAG_SNAPSHOT_ERROR_PASSIVE_FAIL,
            CanDiag_Runtime[controllerId].errorPassiveFailTicks);
    CanDiag_StoreU16(buffer,
            CANDIAG_SNAPSHOT_ERROR_PASSIVE_PASS,
            CanDiag_Runtime[controllerId].errorPassivePassTicks);
    CanDiag_StoreU16(buffer,
            CANDIAG_SNAPSHOT_PROTOCOL_FAIL,
            CanDiag_Runtime[controllerId].protocolFailTicks);
    CanDiag_StoreU16(buffer,
            CANDIAG_SNAPSHOT_PROTOCOL_PASS,
            CanDiag_Runtime[controllerId].protocolPassTicks);
    buffer[CANDIAG_SNAPSHOT_ERROR_PASSIVE_SEEN] =
            (CanDiag_Runtime[controllerId].errorPassiveSeen != FALSE) ? 1u : 0u;

    *length = CANDIAG_SNAPSHOT_SIZE;
    return E_OK;
}

void CanIf_AppBusOff(uint8 ControllerId)
{
    CanDiag_ReportBusOff(ControllerId);
}

void CanIf_AppControllerRecovered(uint8 ControllerId)
{
    CanDiag_ReportControllerRecovered(ControllerId);
}

void CanIf_AppErrorPassive(uint8 ControllerId)
{
    CanDiag_ReportErrorPassive(ControllerId);
}

void CanIf_AppErrorWarning(uint8 ControllerId)
{
    CanDiag_ReportErrorWarning(ControllerId);
}

void CanSM_FailedNotification(uint8 ControllerId)
{
    CanDiag_ReportControllerFault(ControllerId);
}
