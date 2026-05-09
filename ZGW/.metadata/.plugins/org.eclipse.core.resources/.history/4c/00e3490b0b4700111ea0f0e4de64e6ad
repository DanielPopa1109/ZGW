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

static void Dem_DefaultStatusChangedCallback(
    Dem_EventIdType eventId,
    Dem_UdsStatusByteType oldStatus,
    Dem_UdsStatusByteType newStatus
)
{
    (void)eventId;
    (void)oldStatus;
    (void)newStatus;
}

static const Dem_EventConfigType Dem_EventConfigList[] =
{
    {
        DEM_EVENT_ID_MCUSM_DTC_ID_SW_ERROR,
        DEM_DTC_BATTERY_UNDERVOLTAGE,
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
        Dem_DefaultStatusChangedCallback
    },
    {
        DEM_EVENT_ID_CAN_BUS_OFF,
        DEM_DTC_CAN_BUS_OFF,
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
        Dem_DefaultFreezeFrameCapture,
        Dem_DefaultExtendedDataCapture,
        Dem_DefaultStatusChangedCallback
    },
    {
        DEM_EVENT_ID_ETH_LINK_DOWN,
        DEM_DTC_ETH_LINK_DOWN,
        0u,
        2u,
        2,
        -2,
        1,
        1,
        2u,
        TRUE,
        40u,
        TRUE,
        Dem_DefaultFreezeFrameCapture,
        Dem_DefaultExtendedDataCapture,
        Dem_DefaultStatusChangedCallback
    },
    {
        DEM_EVENT_ID_NVM_FAILURE,
        DEM_DTC_NVM_FAILURE,
        0u,
        1u,
        1,
        -1,
        1,
        1,
        1u,
        FALSE,
        0u,
        TRUE,
        Dem_DefaultFreezeFrameCapture,
        Dem_DefaultExtendedDataCapture,
        Dem_DefaultStatusChangedCallback
    }
};

const Dem_ConfigType Dem_Config =
{
    Dem_EventConfigList,
    (uint16)(sizeof(Dem_EventConfigList) / sizeof(Dem_EventConfigList[0])),
    DEM_DEFAULT_OPERATION_CYCLE,
    DEM_NVM_BLOCK_ID_PRIMARY,
    Dem_DefaultStatusChangedCallback
};
