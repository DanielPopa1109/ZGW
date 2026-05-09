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
        Dem_DefaultStatusChangedCallback
    },

};

const Dem_ConfigType Dem_Config =
{
    Dem_EventConfigList,
    (uint16)(sizeof(Dem_EventConfigList) / sizeof(Dem_EventConfigList[0])),
    DEM_DEFAULT_OPERATION_CYCLE,
    DEM_NVM_BLOCK_ID_PRIMARY,
    Dem_DefaultStatusChangedCallback
};
