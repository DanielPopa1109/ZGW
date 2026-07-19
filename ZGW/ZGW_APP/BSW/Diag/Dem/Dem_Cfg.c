#include "Dem_Cfg.h"
#include "SysMgr.h"
#include "EthernetDiag.h"
#include "BSW/Time/TimeBase.h"
#include "SafetyKit_Main.h"
#include <string.h>

static void Dem_StoreU64(uint8 *buffer, uint16 offset, uint64 value)
{
    buffer[offset] = (uint8)((value >> 56u) & 0xFFu);
    buffer[(uint16)(offset + 1u)] = (uint8)((value >> 48u) & 0xFFu);
    buffer[(uint16)(offset + 2u)] = (uint8)((value >> 40u) & 0xFFu);
    buffer[(uint16)(offset + 3u)] = (uint8)((value >> 32u) & 0xFFu);
    buffer[(uint16)(offset + 4u)] = (uint8)((value >> 24u) & 0xFFu);
    buffer[(uint16)(offset + 5u)] = (uint8)((value >> 16u) & 0xFFu);
    buffer[(uint16)(offset + 6u)] = (uint8)((value >> 8u) & 0xFFu);
    buffer[(uint16)(offset + 7u)] = (uint8)(value & 0xFFu);
}

static void Dem_StoreU16(uint8 *buffer, uint16 offset, uint16 value)
{
    buffer[offset] = (uint8)((value >> 8u) & 0xFFu);
    buffer[(uint16)(offset + 1u)] = (uint8)(value & 0xFFu);
}

static sint16 Dem_ScaleMcuTemperatureCdeg(float32 temperature)
{
    float32 scaled;

    scaled = temperature * 100.0f;
    if (scaled > 32767.0f)
    {
        return 32767;
    }

    if (scaled < -32768.0f)
    {
        return -32768;
    }

    return (sint16)scaled;
}

Std_ReturnType Dem_Cfg_CaptureTimestampTemperatureData(uint8 *buffer, uint16 *length, uint8 snapshotKind)
{
    TimeBase_DtcTimestampType timestamp;
    sint16 mcuTempCdeg;

    if ((buffer == NULL_PTR) ||
        (length == NULL_PTR) ||
        (*length < DEM_DTC_TIMESTAMP_DATA_SIZE))
    {
        return E_NOT_OK;
    }

    memset(buffer, 0, DEM_DTC_TIMESTAMP_DATA_SIZE);
    TimeBase_GetDtcTimestamp(&timestamp);
    mcuTempCdeg = Dem_ScaleMcuTemperatureCdeg(g_SafetyKitStatus.dieTempStatus.dieTemperatureCore);

    Dem_StoreU64(buffer, DEM_SNAPSHOT_VEHICLE_TIME_NS_OFFSET, timestamp.vehicle_time_ns);
    Dem_StoreU64(buffer, DEM_SNAPSHOT_UTC_TIME_NS_OFFSET, timestamp.utc_time_ns);
    buffer[DEM_SNAPSHOT_UTC_VALID_OFFSET] = (timestamp.utc_valid != FALSE) ? 1u : 0u;
    buffer[DEM_SNAPSHOT_TIME_SOURCE_OFFSET] = timestamp.time_source;
    Dem_StoreU16(buffer, DEM_SNAPSHOT_MCU_TEMP_CDEG_OFFSET, (uint16)mcuTempCdeg);
    buffer[DEM_SNAPSHOT_VERSION_OFFSET] = DEM_SNAPSHOT_VERSION;
    buffer[DEM_SNAPSHOT_KIND_OFFSET] = snapshotKind;

    *length = DEM_DTC_TIMESTAMP_DATA_SIZE;
    return E_OK;
}

static Std_ReturnType Dem_DefaultSnapshotDataCapture(
    Dem_EventIdType eventId,
    uint8 *buffer,
    uint16 *length
)
{
    (void)eventId;
    return Dem_Cfg_CaptureTimestampTemperatureData(buffer, length, DEM_SNAPSHOT_KIND_COMMON);
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

    if (eventId == DEM_EVENT_ID_CODING_ECU_NOT_CODED)
    {
        *dtc = DEM_DTC_CODING_ECU_NOT_CODED;
        return E_OK;
    }

    if (eventId == DEM_EVENT_ID_CODING_INVALID)
    {
        *dtc = DEM_DTC_CODING_INVALID;
        return E_OK;
    }

    if ((eventId >= DEM_EVENT_ID_ETH_LINK_LOST) &&
        (eventId <= DEM_EVENT_ID_ETH_PARTNER_COMM_TERMINATED))
    {
        *dtc = (Dem_DTCType)(DEM_DTC_ETH_LINK_LOST +
                (Dem_DTCType)(eventId - DEM_EVENT_ID_ETH_LINK_LOST));
        return E_OK;
    }

    if ((eventId >= DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_FIRST) &&
        (eventId <= DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_LAST))
    {
        *dtc = (Dem_DTCType)(DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT +
                (Dem_DTCType)(eventId - DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_FIRST));
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
        SysMgr_CaptureMcuSmSnapshotData,
        NULL_PTR
    },
    {
        DEM_EVENT_ID_CODING_ECU_NOT_CODED,
        DEM_DTC_CODING_ECU_NOT_CODED,
        0u,
        1u,
        1,
        -1,
        1,
        1,
        1u,
        TRUE,
        40u,
        TRUE,
        Dem_DefaultSnapshotDataCapture,
        NULL_PTR
    },
    {
        DEM_EVENT_ID_CODING_INVALID,
        DEM_DTC_CODING_INVALID,
        0u,
        1u,
        1,
        -1,
        1,
        1,
        1u,
        TRUE,
        40u,
        TRUE,
        Dem_DefaultSnapshotDataCapture,
        NULL_PTR
    },
    {
        DEM_EVENT_ID_ETH_LINK_LOST,
        DEM_DTC_ETH_LINK_LOST,
        0u,
        1u,
        1,
        -1,
        1,
        1,
        1u,
        TRUE,
        40u,
        TRUE,
        EthernetDiag_CaptureSnapshotData,
        NULL_PTR
    },
    {
        DEM_EVENT_ID_ETH_CTRL_DMA_FAILURE,
        DEM_DTC_ETH_CTRL_DMA_FAILURE,
        0u,
        1u,
        1,
        -1,
        1,
        1,
        1u,
        TRUE,
        40u,
        TRUE,
        EthernetDiag_CaptureSnapshotData,
        NULL_PTR
    },
    {
        DEM_EVENT_ID_ETH_RX_COMM_FAILURE,
        DEM_DTC_ETH_RX_COMM_FAILURE,
        0u,
        1u,
        1,
        -1,
        1,
        1,
        1u,
        TRUE,
        40u,
        TRUE,
        EthernetDiag_CaptureSnapshotData,
        NULL_PTR
    },
    {
        DEM_EVENT_ID_ETH_TX_COMM_FAILURE,
        DEM_DTC_ETH_TX_COMM_FAILURE,
        0u,
        1u,
        1,
        -1,
        1,
        1,
        1u,
        TRUE,
        40u,
        TRUE,
        EthernetDiag_CaptureSnapshotData,
        NULL_PTR
    },
    {
        DEM_EVENT_ID_ETH_TCP_UNEXPECTED_TERMINATION,
        DEM_DTC_ETH_TCP_UNEXPECTED_TERMINATION,
        0u,
        1u,
        1,
        -1,
        1,
        1,
        1u,
        TRUE,
        40u,
        TRUE,
        EthernetDiag_CaptureSnapshotData,
        NULL_PTR
    },
    {
        DEM_EVENT_ID_ETH_TCP_ESTABLISHMENT_FAILURE,
        DEM_DTC_ETH_TCP_ESTABLISHMENT_FAILURE,
        0u,
        1u,
        1,
        -1,
        1,
        1,
        1u,
        TRUE,
        40u,
        TRUE,
        EthernetDiag_CaptureSnapshotData,
        NULL_PTR
    },
    {
        DEM_EVENT_ID_ETH_UDP_SUPERVISION_TIMEOUT,
        DEM_DTC_ETH_UDP_SUPERVISION_TIMEOUT,
        0u,
        1u,
        1,
        -1,
        1,
        1,
        1u,
        TRUE,
        40u,
        TRUE,
        EthernetDiag_CaptureSnapshotData,
        NULL_PTR
    },
    {
        DEM_EVENT_ID_ETH_SERVICE_AVAILABILITY_FAILURE,
        DEM_DTC_ETH_SERVICE_AVAILABILITY_FAILURE,
        0u,
        1u,
        1,
        -1,
        1,
        1,
        1u,
        TRUE,
        40u,
        TRUE,
        EthernetDiag_CaptureSnapshotData,
        NULL_PTR
    },
    {
        DEM_EVENT_ID_ETH_DOIP_COMM_FAILURE,
        DEM_DTC_ETH_DOIP_COMM_FAILURE,
        0u,
        1u,
        1,
        -1,
        1,
        1,
        1u,
        TRUE,
        40u,
        TRUE,
        EthernetDiag_CaptureSnapshotData,
        NULL_PTR
    },
    {
        DEM_EVENT_ID_ETH_PARTNER_COMM_TERMINATED,
        DEM_DTC_ETH_PARTNER_COMM_TERMINATED,
        0u,
        1u,
        1,
        -1,
        1,
        1,
        1u,
        TRUE,
        40u,
        TRUE,
        EthernetDiag_CaptureSnapshotData,
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
    eventConfig->SnapshotDataCapture = GatewaySwc_CaptureRxDiagSnapshotData;
    eventConfig->StatusChangedCallback = NULL_PTR;
}

static void Dem_ApplyStoredDataLimit(
    uint16 eventIndex,
    Dem_EventConfigType *eventConfig
)
{
    if (eventIndex >= DEM_STORED_DATA_EVENT_LIMIT)
    {
        eventConfig->StorageEnabled = FALSE;
    }
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
        Dem_ApplyStoredDataLimit(eventIndex, eventConfig);
        return E_OK;
    }

    offset = (uint16)(eventIndex - DEM_STATIC_EVENT_COUNT);

    if (offset < (uint16)DEM_GATEWAY_RX_MESSAGE_EVENT_COUNT)
    {
        Dem_FillGatewayEventConfig(
                eventConfig,
                (Dem_EventIdType)(DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_FIRST + offset),
                (Dem_DTCType)(DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT + offset));
        Dem_ApplyStoredDataLimit(eventIndex, eventConfig);
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
