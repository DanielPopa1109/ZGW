#include "Dem_Cfg.h"
#include <string.h>

static Std_ReturnType Dem_DefaultFreezeFrameCapture(
    Dem_EventIdType eventId,
    uint8 *buffer,
    uint16 *length
)
{
    uint16 i;
    uint16 maxLen;

    if ((buffer == NULL_PTR) || (length == NULL_PTR))
    {
        return E_NOT_OK;
    }

    maxLen = *length;
    if (maxLen > DEM_FREEZE_FRAME_SIZE)
    {
        maxLen = DEM_FREEZE_FRAME_SIZE;
    }

    for (i = 0u; i < maxLen; i++)
    {
        buffer[i] = 0u;
    }

    if (maxLen >= 4u)
    {
        buffer[0] = (uint8)((eventId >> 8u) & 0xFFu);
        buffer[1] = (uint8)(eventId & 0xFFu);
        buffer[2] = 0u;
        buffer[3] = 0u;
    }

    *length = maxLen;
    return E_OK;
}

static Std_ReturnType Dem_DefaultExtendedDataCapture(
    Dem_EventIdType eventId,
    uint8 *buffer,
    uint16 *length
)
{
    uint16 i;
    uint16 maxLen;

    if ((buffer == NULL_PTR) || (length == NULL_PTR))
    {
        return E_NOT_OK;
    }

    maxLen = *length;
    if (maxLen > DEM_EXTENDED_DATA_SIZE)
    {
        maxLen = DEM_EXTENDED_DATA_SIZE;
    }

    for (i = 0u; i < maxLen; i++)
    {
        buffer[i] = 0u;
    }

    if (maxLen >= 4u)
    {
        buffer[0] = (uint8)((eventId >> 8u) & 0xFFu);
        buffer[1] = (uint8)(eventId & 0xFFu);
        buffer[2] = 0u;
        buffer[3] = 1u;
    }

    *length = maxLen;
    return E_OK;
}

static Std_ReturnType Dem_GetDtcForEventId(Dem_EventIdType eventId, Dem_DTCType *dtc)
{
    if (dtc == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (eventId == DEM_EVENT_ID_MCUSM_SW_ERROR)
    {
        *dtc = DEM_DTC_MCUSM_SW_ERROR;
        return E_OK;
    }

    if ((eventId >= DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_FIRST) &&
        (eventId <= DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_LAST))
    {
        *dtc = (Dem_DTCType)(DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT +
                (Dem_DTCType)(eventId - DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_FIRST));
        return E_OK;
    }

    if ((eventId >= DEM_EVENT_ID_GATEWAY_RX_SIGNAL_INVALID_FIRST) &&
        (eventId <= DEM_EVENT_ID_GATEWAY_RX_SIGNAL_INVALID_LAST))
    {
        *dtc = (Dem_DTCType)(DEM_DTC_GATEWAY_RX_SIGNAL_INVALID +
                (Dem_DTCType)(eventId - DEM_EVENT_ID_GATEWAY_RX_SIGNAL_INVALID_FIRST));
        return E_OK;
    }

    return E_NOT_OK;
}

static void Dem_DefaultStatusChangedCallback(
    Dem_EventIdType eventId,
    Dem_UdsStatusByteType oldStatus,
    Dem_UdsStatusByteType newStatus
)
{
    Dem_DTCType dtc;

    if (((oldStatus ^ newStatus) & DEM_UDS_STATUS_TF) == 0u)
    {
        return;
    }

    if (Dem_GetDtcForEventId(eventId, &dtc) == E_OK)
    {
        GatewaySwc_ReportDtcTransition(dtc & 0x00FFFFFFu, newStatus);
    }
}

static const Dem_EventConfigType Dem_StaticEventConfigList[] =
{
    {
        DEM_EVENT_ID_MCUSM_SW_ERROR,
        DEM_DTC_MCUSM_SW_ERROR,
        0u,
        1u,
        3,
        -3,
        1,
        1,
        1u,
        TRUE,
        40u,
        TRUE,
        Dem_DefaultFreezeFrameCapture,
        Dem_DefaultExtendedDataCapture,
        NULL_PTR
    }
};

static void Dem_FillGatewayEventConfig(
    Dem_EventConfigType *eventConfig,
    Dem_EventIdType eventId,
    Dem_DTCType dtc
)
{
    eventConfig->EventId = eventId;
    eventConfig->DTC = dtc;
    eventConfig->Severity = 0u;
    eventConfig->Priority = 1u;
    eventConfig->FailedThreshold = 3;
    eventConfig->PassedThreshold = -3;
    eventConfig->IncrementStep = 1;
    eventConfig->DecrementStep = 1;
    eventConfig->ConfirmationThreshold = 1u;
    eventConfig->AgingAllowed = TRUE;
    eventConfig->AgingThreshold = 40u;
    eventConfig->StorageEnabled = TRUE;
    eventConfig->FreezeFrameCapture = GatewaySwc_CaptureRxDiagFreezeFrame;
    eventConfig->ExtendedDataCapture = GatewaySwc_CaptureRxDiagExtendedData;
    eventConfig->StatusChangedCallback = NULL_PTR;
}

Std_ReturnType Dem_Cfg_GetEventConfig(uint16 eventIndex, Dem_EventConfigType *eventConfig)
{
    uint16 offset;

    if (eventConfig == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (eventIndex < DEM_STATIC_EVENT_COUNT)
    {
        *eventConfig = Dem_StaticEventConfigList[eventIndex];
        return E_OK;
    }

    offset = (uint16)(eventIndex - DEM_STATIC_EVENT_COUNT);

    if (offset < (uint16)DEM_GATEWAY_RX_MESSAGE_EVENT_COUNT)
    {
        Dem_FillGatewayEventConfig(
                eventConfig,
                (Dem_EventIdType)(DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_FIRST + offset),
                (Dem_DTCType)(DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT + offset));
        return E_OK;
    }

    offset = (uint16)(offset - DEM_GATEWAY_RX_MESSAGE_EVENT_COUNT);

    if (offset < (uint16)DEM_GATEWAY_RX_SIGNAL_EVENT_COUNT)
    {
        Dem_FillGatewayEventConfig(
                eventConfig,
                (Dem_EventIdType)(DEM_EVENT_ID_GATEWAY_RX_SIGNAL_INVALID_FIRST + offset),
                (Dem_DTCType)(DEM_DTC_GATEWAY_RX_SIGNAL_INVALID + offset));
        return E_OK;
    }

    return E_NOT_OK;
}

const Dem_ConfigType Dem_Config =
{
    Dem_StaticEventConfigList,
    (uint16)DEM_MAX_EVENTS,
    DEM_DEFAULT_OPERATION_CYCLE,
    DEM_NVM_BLOCK_ID_PRIMARY,
    Dem_DefaultStatusChangedCallback
};
