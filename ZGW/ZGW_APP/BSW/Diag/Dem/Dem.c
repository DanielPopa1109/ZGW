#include "Dem.h"
#include "Dem_Int.h"
#include "Dem_NvM.h"
#include "IfxCpu.h"
#include <string.h>
#include <stddef.h>

#define DEM_CRITICAL_SPIN_TIMEOUT 100000u
#define DEM_CRITICAL_CORE_COUNT   6u

typedef enum
{
    DEM_NVM_STATE_IDLE = 0,
    DEM_NVM_STATE_READ_PENDING,
    DEM_NVM_STATE_WRITE_PENDING
} Dem_InternalNvMStateType;

static const Dem_ConfigType *Dem_ConfigPtr = NULL_PTR;

static Dem_InitStateType Dem_InitState = DEM_UNINIT;
static Dem_InternalNvMStateType Dem_NvMState = DEM_NVM_STATE_IDLE;

static Dem_RuntimeEventType Dem_RuntimeEvents[DEM_MAX_EVENTS];
static Dem_PrimaryEntryType Dem_PrimaryMemory[DEM_PRIMARY_MEMORY_SIZE];

Dem_NvImageType Dem_NvImage;
static Dem_FilterType Dem_Filter;

static boolean Dem_Dirty = FALSE;
static boolean Dem_OperationCycleActive = FALSE;
static uint32 Dem_OperationCycleCounter = 0u;

static Dem_ClearDTCStatusType Dem_ClearStatus = DEM_CLEAR_IDLE;
static uint8 Dem_NvMWriteRetry = 0u;
static IfxCpu_spinLock Dem_CriticalSpinLock;
static boolean Dem_CriticalIrqState[DEM_CRITICAL_CORE_COUNT];
static volatile uint8 Dem_CriticalOwnedByCore[DEM_CRITICAL_CORE_COUNT];

volatile uint32 Dem_ChangeCounter = 0u;
volatile uint32 Dem_CriticalTimeoutCounter = 0u;
volatile uint16 Dem_DtcStatusListCount = 0u;
volatile Dem_DebugEventStatusType Dem_DtcStatusList[DEM_MAX_EVENTS];
volatile uint32 Dem_SetEventStatusCounter[DEM_MAX_EVENTS][DEM_EVENT_STATUS_DEBUG_COUNT];
volatile uint32 Dem_EventStatusChangeCounter[DEM_MAX_EVENTS];

long long Dem_MainFunction_Counter = 0;

void Dem_EnterCritical(void)
{
    uint32 coreIndex;

    coreIndex = (uint32)IfxCpu_getCoreIndex();

    if (coreIndex >= DEM_CRITICAL_CORE_COUNT)
    {
        coreIndex = 0u;
    }

    Dem_CriticalIrqState[coreIndex] = IfxCpu_disableInterrupts();

    if (IfxCpu_setSpinLock(&Dem_CriticalSpinLock, DEM_CRITICAL_SPIN_TIMEOUT) != FALSE)
    {
        Dem_CriticalOwnedByCore[coreIndex] = TRUE;
        __dsync();
    }
    else
    {
        Dem_CriticalTimeoutCounter++;
    }
}

void Dem_ExitCritical(void)
{
    uint32 coreIndex;

    coreIndex = (uint32)IfxCpu_getCoreIndex();

    if (coreIndex >= DEM_CRITICAL_CORE_COUNT)
    {
        coreIndex = 0u;
    }

    __dsync();

    if (Dem_CriticalOwnedByCore[coreIndex] != FALSE)
    {
        Dem_CriticalOwnedByCore[coreIndex] = FALSE;
        IfxCpu_resetSpinLock(&Dem_CriticalSpinLock);
    }

    IfxCpu_restoreInterrupts(Dem_CriticalIrqState[coreIndex]);
}

static uint32 Dem_Crc32(const uint8 *data, uint32 length)
{
    uint32 crc = DEM_CRC32_INITIAL_VALUE;
    uint32 i;
    uint8 bit;

    for (i = 0u; i < length; i++)
    {
        crc ^= (uint32)data[i];

        for (bit = 0u; bit < 8u; bit++)
        {
            if ((crc & 1u) != 0u)
            {
                crc = (crc >> 1u) ^ 0xEDB88320u;
            }
            else
            {
                crc >>= 1u;
            }
        }
    }

    return ~crc;
}

static uint32 Dem_GetImageCrc(const Dem_NvImageType *image)
{
    return Dem_Crc32(
        (const uint8 *)image,
        (uint32)offsetof(Dem_NvImageType, crc32)
    );
}

static boolean Dem_TestFailedChanged(
    Dem_UdsStatusByteType oldStatus,
    Dem_UdsStatusByteType newStatus
)
{
    return (((oldStatus ^ newStatus) & DEM_UDS_STATUS_TF) != 0u) ? TRUE : FALSE;
}

static boolean Dem_IsValidDtcFormatAndOrigin(
    Dem_DTCFormatType format,
    Dem_DTCOriginType origin
)
{
    if (format != DEM_DTC_FORMAT_UDS)
    {
        return FALSE;
    }

    if (origin != DEM_DTC_ORIGIN_PRIMARY_MEMORY)
    {
        return FALSE;
    }

    return TRUE;
}

static Std_ReturnType Dem_GetEventConfig(uint16 eventIndex, Dem_EventConfigType *eventConfig)
{
    if ((Dem_ConfigPtr == NULL_PTR) || (eventConfig == NULL_PTR))
    {
        return E_NOT_OK;
    }

    if (eventIndex >= Dem_ConfigPtr->EventCount)
    {
        return E_NOT_OK;
    }

    if (Dem_ConfigPtr == &Dem_Config)
    {
        return Dem_Cfg_GetEventConfig(eventIndex, eventConfig);
    }

    if (Dem_ConfigPtr->Events == NULL_PTR)
    {
        return E_NOT_OK;
    }

    *eventConfig = Dem_ConfigPtr->Events[eventIndex];
    return E_OK;
}

static boolean Dem_IsStoredDataAllowed(uint16 eventIndex)
{
    return (eventIndex < DEM_STORED_DATA_EVENT_LIMIT) ? TRUE : FALSE;
}

static void Dem_DebugUpdateEvent(uint16 eventIndex)
{
    Dem_EventConfigType eventConfig;

    if ((Dem_ConfigPtr == NULL_PTR) || (eventIndex >= Dem_ConfigPtr->EventCount))
    {
        return;
    }

    if (Dem_GetEventConfig(eventIndex, &eventConfig) != E_OK)
    {
        return;
    }

    Dem_DtcStatusList[eventIndex].eventId = eventConfig.EventId;
    Dem_DtcStatusList[eventIndex].dtc = eventConfig.DTC & 0x00FFFFFFu;
    Dem_DtcStatusList[eventIndex].udsStatus = Dem_RuntimeEvents[eventIndex].udsStatus;
    Dem_DtcStatusList[eventIndex].debounceCounter = Dem_RuntimeEvents[eventIndex].debounceCounter;
    Dem_DtcStatusList[eventIndex].confirmationCounter = Dem_RuntimeEvents[eventIndex].confirmationCounter;
    Dem_DtcStatusList[eventIndex].occurrenceCounter = Dem_RuntimeEvents[eventIndex].occurrenceCounter;
    Dem_DtcStatusList[eventIndex].agingCounter = Dem_RuntimeEvents[eventIndex].agingCounter;
    Dem_DtcStatusList[eventIndex].setPassedCount =
        Dem_SetEventStatusCounter[eventIndex][DEM_EVENT_STATUS_PASSED];
    Dem_DtcStatusList[eventIndex].setFailedCount =
        Dem_SetEventStatusCounter[eventIndex][DEM_EVENT_STATUS_FAILED];
    Dem_DtcStatusList[eventIndex].setPrePassedCount =
        Dem_SetEventStatusCounter[eventIndex][DEM_EVENT_STATUS_PREPASSED];
    Dem_DtcStatusList[eventIndex].setPreFailedCount =
        Dem_SetEventStatusCounter[eventIndex][DEM_EVENT_STATUS_PREFAILED];
    Dem_DtcStatusList[eventIndex].statusChangeCount = Dem_EventStatusChangeCounter[eventIndex];

    Dem_DtcStatusListCount = Dem_ConfigPtr->EventCount;
}

static void Dem_DebugRefreshStatusList(void)
{
    uint16 i;

    if (Dem_ConfigPtr == NULL_PTR)
    {
        Dem_DtcStatusListCount = 0u;
        return;
    }

    Dem_DtcStatusListCount = Dem_ConfigPtr->EventCount;

    for (i = 0u; i < Dem_ConfigPtr->EventCount; i++)
    {
        Dem_DebugUpdateEvent(i);
    }
}

static void Dem_DebugReset(void)
{
    uint16 i;
    uint8 statusIndex;

    Dem_DtcStatusListCount = 0u;

    for (i = 0u; i < (uint16)DEM_MAX_EVENTS; i++)
    {
        Dem_DtcStatusList[i].eventId = 0u;
        Dem_DtcStatusList[i].dtc = 0u;
        Dem_DtcStatusList[i].udsStatus = 0u;
        Dem_DtcStatusList[i].debounceCounter = 0;
        Dem_DtcStatusList[i].confirmationCounter = 0u;
        Dem_DtcStatusList[i].occurrenceCounter = 0u;
        Dem_DtcStatusList[i].agingCounter = 0u;
        Dem_DtcStatusList[i].setPassedCount = 0u;
        Dem_DtcStatusList[i].setFailedCount = 0u;
        Dem_DtcStatusList[i].setPrePassedCount = 0u;
        Dem_DtcStatusList[i].setPreFailedCount = 0u;
        Dem_DtcStatusList[i].statusChangeCount = 0u;
        Dem_EventStatusChangeCounter[i] = 0u;

        for (statusIndex = 0u; statusIndex < DEM_EVENT_STATUS_DEBUG_COUNT; statusIndex++)
        {
            Dem_SetEventStatusCounter[i][statusIndex] = 0u;
        }
    }
}

static uint16 Dem_FindEventIndex(Dem_EventIdType eventId)
{
    uint16 i;
    Dem_EventConfigType eventConfig;

    if (Dem_ConfigPtr == NULL_PTR)
    {
        return DEM_MAX_EVENTS;
    }

    if (Dem_ConfigPtr == &Dem_Config)
    {
        if (eventId == DEM_EVENT_ID_MCUSM_SW_ERROR)
        {
            return 0u;
        }

        if ((eventId >= DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_FIRST) &&
                (eventId <= DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_LAST))
        {
            return (uint16)(DEM_STATIC_EVENT_COUNT +
                    (eventId - DEM_EVENT_ID_GATEWAY_RX_MESSAGE_TIMEOUT_FIRST));
        }

        if ((eventId >= DEM_EVENT_ID_GATEWAY_RX_SIGNAL_INVALID_FIRST) &&
                (eventId <= DEM_EVENT_ID_GATEWAY_RX_SIGNAL_INVALID_LAST))
        {
            return (uint16)(DEM_STATIC_EVENT_COUNT +
                    DEM_GATEWAY_RX_MESSAGE_EVENT_COUNT +
                    (eventId - DEM_EVENT_ID_GATEWAY_RX_SIGNAL_INVALID_FIRST));
        }
    }

    for (i = 0u; i < Dem_ConfigPtr->EventCount; i++)
    {
        if ((Dem_GetEventConfig(i, &eventConfig) == E_OK) &&
            (eventConfig.EventId == eventId))
        {
            return i;
        }
    }

    return DEM_MAX_EVENTS;
}

static uint16 Dem_FindEventIndexByDTC(Dem_DTCType dtc)
{
    uint16 i;
    Dem_EventConfigType eventConfig;
    uint32 udsDtc;
    uint32 baseDtc;

    if (Dem_ConfigPtr == NULL_PTR)
    {
        return DEM_MAX_EVENTS;
    }

    udsDtc = dtc & 0x00FFFFFFu;

    if (Dem_ConfigPtr == &Dem_Config)
    {
        if (udsDtc == (DEM_DTC_MCUSM_SW_ERROR & 0x00FFFFFFu))
        {
            return 0u;
        }

        baseDtc = DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT & 0x00FFFFFFu;
        if ((udsDtc >= baseDtc) &&
                (udsDtc < (baseDtc + (uint32)DEM_GATEWAY_RX_MESSAGE_EVENT_COUNT)))
        {
            return (uint16)(DEM_STATIC_EVENT_COUNT + (udsDtc - baseDtc));
        }

        baseDtc = DEM_DTC_GATEWAY_RX_SIGNAL_INVALID & 0x00FFFFFFu;
        if ((udsDtc >= baseDtc) &&
                (udsDtc < (baseDtc + (uint32)DEM_GATEWAY_RX_SIGNAL_EVENT_COUNT)))
        {
            return (uint16)(DEM_STATIC_EVENT_COUNT +
                    DEM_GATEWAY_RX_MESSAGE_EVENT_COUNT +
                    (udsDtc - baseDtc));
        }
    }

    for (i = 0u; i < Dem_ConfigPtr->EventCount; i++)
    {
        if ((Dem_GetEventConfig(i, &eventConfig) == E_OK) &&
            ((eventConfig.DTC & 0x00FFFFFFu) == udsDtc))
        {
            return i;
        }
    }

    return DEM_MAX_EVENTS;
}

static uint16 Dem_FindPrimaryEntryByEvent(Dem_EventIdType eventId)
{
    uint16 i;

    for (i = 0u; i < DEM_PRIMARY_MEMORY_SIZE; i++)
    {
        if ((Dem_PrimaryMemory[i].valid != 0u) &&
            (Dem_PrimaryMemory[i].eventId == eventId))
        {
            return i;
        }
    }

    return DEM_PRIMARY_MEMORY_SIZE;
}

static uint16 Dem_FindPrimaryEntryByDTC(Dem_DTCType dtc)
{
    uint16 i;

    dtc &= 0x00FFFFFFu;

    for (i = 0u; i < DEM_PRIMARY_MEMORY_SIZE; i++)
    {
        if ((Dem_PrimaryMemory[i].valid != 0u) &&
            ((Dem_PrimaryMemory[i].dtc & 0x00FFFFFFu) == dtc))
        {
            return i;
        }
    }

    return DEM_PRIMARY_MEMORY_SIZE;
}

static uint16 Dem_AllocatePrimaryEntry(uint16 eventIndex)
{
    uint16 i;
    uint16 replaceIndex = DEM_PRIMARY_MEMORY_SIZE;
    uint8 newPriority;
    uint8 worstPriority = 0u;
    uint32 oldestChange = 0xFFFFFFFFu;
    Dem_EventConfigType eventConfig;

    for (i = 0u; i < DEM_PRIMARY_MEMORY_SIZE; i++)
    {
        if (Dem_PrimaryMemory[i].valid == 0u)
        {
            return i;
        }
    }

    if (Dem_GetEventConfig(eventIndex, &eventConfig) != E_OK)
    {
        return DEM_PRIMARY_MEMORY_SIZE;
    }

    newPriority = eventConfig.Priority;

    for (i = 0u; i < DEM_PRIMARY_MEMORY_SIZE; i++)
    {
        uint16 oldEventIndex = Dem_FindEventIndex(Dem_PrimaryMemory[i].eventId);
        uint8 oldPriority = 255u;
        Dem_EventConfigType oldEventConfig;

        if ((oldEventIndex < DEM_MAX_EVENTS) &&
            (Dem_GetEventConfig(oldEventIndex, &oldEventConfig) == E_OK))
        {
            oldPriority = oldEventConfig.Priority;
        }

        if (oldPriority > newPriority)
        {
            if ((replaceIndex == DEM_PRIMARY_MEMORY_SIZE) ||
                (oldPriority > worstPriority) ||
                ((oldPriority == worstPriority) &&
                 (Dem_PrimaryMemory[i].lastChangedCounter < oldestChange)))
            {
                replaceIndex = i;
                worstPriority = oldPriority;
                oldestChange = Dem_PrimaryMemory[i].lastChangedCounter;
            }
        }
    }

    return replaceIndex;
}

static void Dem_InvokeStatusChanged(
    uint16 eventIndex,
    Dem_UdsStatusByteType oldStatus,
    Dem_UdsStatusByteType newStatus
)
{
    Dem_EventIdType eventId;
    Dem_EventConfigType eventConfig;

    if (oldStatus == newStatus)
    {
        return;
    }

    if (Dem_GetEventConfig(eventIndex, &eventConfig) != E_OK)
    {
        return;
    }

    eventId = eventConfig.EventId;

    if (eventConfig.StatusChangedCallback != NULL_PTR)
    {
        eventConfig.StatusChangedCallback(
            eventId,
            oldStatus,
            newStatus
        );
    }

    if (Dem_ConfigPtr->GlobalStatusChangedCallback != NULL_PTR)
    {
        Dem_ConfigPtr->GlobalStatusChangedCallback(
            eventId,
            oldStatus,
            newStatus
        );
    }
}

static void Dem_UpdatePrimaryEntryStatus(Dem_EventIdType eventId, Dem_UdsStatusByteType status)
{
    uint16 entryIndex = Dem_FindPrimaryEntryByEvent(eventId);

    if (entryIndex < DEM_PRIMARY_MEMORY_SIZE)
    {
        Dem_PrimaryMemory[entryIndex].udsStatus = status;
        Dem_PrimaryMemory[entryIndex].lastChangedCounter = ++Dem_ChangeCounter;
    }
}

static void Dem_SetStatusByte(uint16 eventIndex, Dem_UdsStatusByteType newStatus)
{
    Dem_UdsStatusByteType oldStatus;
    Dem_EventConfigType eventConfig;

    oldStatus = Dem_RuntimeEvents[eventIndex].udsStatus;

    if (oldStatus != newStatus)
    {
        if (Dem_GetEventConfig(eventIndex, &eventConfig) != E_OK)
        {
            return;
        }

        DEM_ENTER_CRITICAL();
        Dem_RuntimeEvents[eventIndex].udsStatus = newStatus;
        Dem_EventStatusChangeCounter[eventIndex]++;
        Dem_UpdatePrimaryEntryStatus(
            eventConfig.EventId,
            newStatus
        );
        if (Dem_TestFailedChanged(oldStatus, newStatus) != FALSE)
        {
            Dem_Dirty = TRUE;
        }
        DEM_EXIT_CRITICAL();

        Dem_InvokeStatusChanged(eventIndex, oldStatus, newStatus);
    }

    Dem_DebugUpdateEvent(eventIndex);
}

static void Dem_InitDefaultRuntime(void)
{
    uint16 i;
    Dem_EventConfigType eventConfig;

    memset(Dem_RuntimeEvents, 0, sizeof(Dem_RuntimeEvents));
    memset(Dem_PrimaryMemory, 0, sizeof(Dem_PrimaryMemory));
    memset(&Dem_Filter, 0, sizeof(Dem_Filter));
    Dem_DebugReset();

    if (Dem_ConfigPtr == NULL_PTR)
    {
        return;
    }

    for (i = 0u; i < Dem_ConfigPtr->EventCount; i++)
    {
        if (Dem_GetEventConfig(i, &eventConfig) != E_OK)
        {
            continue;
        }

        Dem_RuntimeEvents[i].eventId = eventConfig.EventId;
        Dem_RuntimeEvents[i].udsStatus =
            (uint8)(DEM_UDS_STATUS_TNCSLC | DEM_UDS_STATUS_TNCTOC);
        Dem_RuntimeEvents[i].debounceCounter = 0;
        Dem_RuntimeEvents[i].confirmationCounter = 0u;
        Dem_RuntimeEvents[i].occurrenceCounter = 0u;
        Dem_RuntimeEvents[i].agingCounter = 0u;
        Dem_RuntimeEvents[i].available = TRUE;
    }

    Dem_DebugRefreshStatusList();
}

static void Dem_BuildNvImage(void)
{
    uint16 i;

    memset(&Dem_NvImage, 0, sizeof(Dem_NvImage));

    Dem_NvImage.magic = DEM_NV_MAGIC;
    Dem_NvImage.version = DEM_NV_VERSION;
    Dem_NvImage.eventRecordCount = Dem_ConfigPtr->EventCount;
    Dem_NvImage.primaryEntryCount = DEM_PRIMARY_MEMORY_SIZE;

    for (i = 0u; i < Dem_ConfigPtr->EventCount; i++)
    {
        Dem_NvImage.eventRecords[i].eventId = Dem_RuntimeEvents[i].eventId;
        Dem_NvImage.eventRecords[i].udsStatus = Dem_RuntimeEvents[i].udsStatus;
        Dem_NvImage.eventRecords[i].debounceCounter = Dem_RuntimeEvents[i].debounceCounter;
        Dem_NvImage.eventRecords[i].confirmationCounter = Dem_RuntimeEvents[i].confirmationCounter;
        Dem_NvImage.eventRecords[i].occurrenceCounter = Dem_RuntimeEvents[i].occurrenceCounter;
        Dem_NvImage.eventRecords[i].agingCounter = Dem_RuntimeEvents[i].agingCounter;
    }

    for (i = 0u; i < DEM_PRIMARY_MEMORY_SIZE; i++)
    {
        Dem_NvImage.primaryEntries[i] = Dem_PrimaryMemory[i];
    }

    Dem_NvImage.crc32 = Dem_GetImageCrc(&Dem_NvImage);
}

static boolean Dem_IsNvImageValid(const Dem_NvImageType *image)
{
    uint32 crc;

    if (image == NULL_PTR)
    {
        return FALSE;
    }

    if (image->magic != DEM_NV_MAGIC)
    {
        return FALSE;
    }

    if (image->version != DEM_NV_VERSION)
    {
        return FALSE;
    }

    if (image->eventRecordCount > DEM_MAX_EVENTS)
    {
        return FALSE;
    }

    if (image->primaryEntryCount > DEM_PRIMARY_MEMORY_SIZE)
    {
        return FALSE;
    }

    crc = Dem_GetImageCrc(image);

    if (crc != image->crc32)
    {
        return FALSE;
    }

    return TRUE;
}

static void Dem_LoadNvImage(const Dem_NvImageType *image)
{
    uint16 i;

    Dem_InitDefaultRuntime();

    if (Dem_IsNvImageValid(image) == FALSE)
    {
        Dem_Dirty = TRUE;
        return;
    }

    for (i = 0u; i < image->eventRecordCount; i++)
    {
        uint16 eventIndex = Dem_FindEventIndex(image->eventRecords[i].eventId);

        if (eventIndex < DEM_MAX_EVENTS)
        {
            Dem_RuntimeEvents[eventIndex].udsStatus =
                image->eventRecords[i].udsStatus & DEM_UDS_STATUS_AVAILABILITY_MASK;
            Dem_RuntimeEvents[eventIndex].debounceCounter =
                image->eventRecords[i].debounceCounter;
            Dem_RuntimeEvents[eventIndex].confirmationCounter =
                image->eventRecords[i].confirmationCounter;
            Dem_RuntimeEvents[eventIndex].occurrenceCounter =
                image->eventRecords[i].occurrenceCounter;
            Dem_RuntimeEvents[eventIndex].agingCounter =
                image->eventRecords[i].agingCounter;
            Dem_RuntimeEvents[eventIndex].available = TRUE;
        }
    }

    for (i = 0u; i < image->primaryEntryCount; i++)
    {
        uint16 eventIndex = Dem_FindEventIndex(image->primaryEntries[i].eventId);

        if ((image->primaryEntries[i].valid != 0u) &&
            (eventIndex < DEM_MAX_EVENTS) &&
            (Dem_IsStoredDataAllowed(eventIndex) != FALSE))
        {
            Dem_PrimaryMemory[i] = image->primaryEntries[i];
        }
    }

    Dem_DebugRefreshStatusList();
}

static void Dem_CaptureSnapshot(uint16 eventIndex, uint16 entryIndex)
{
    uint16 len;
    Dem_EventConfigType eventConfig;

    if (entryIndex >= DEM_PRIMARY_MEMORY_SIZE)
    {
        return;
    }

    if (Dem_IsStoredDataAllowed(eventIndex) == FALSE)
    {
        return;
    }

    if (Dem_GetEventConfig(eventIndex, &eventConfig) != E_OK)
    {
        return;
    }

    len = DEM_FREEZE_FRAME_SIZE;
    Dem_PrimaryMemory[entryIndex].freezeFrameLength = 0u;

    if (eventConfig.FreezeFrameCapture != NULL_PTR)
    {
        if (eventConfig.FreezeFrameCapture(
                eventConfig.EventId,
                Dem_PrimaryMemory[entryIndex].freezeFrame,
                &len) == E_OK)
        {
            if (len > DEM_FREEZE_FRAME_SIZE)
            {
                len = DEM_FREEZE_FRAME_SIZE;
            }

            Dem_PrimaryMemory[entryIndex].freezeFrameLength = len;
        }
    }

    len = DEM_EXTENDED_DATA_SIZE;
    Dem_PrimaryMemory[entryIndex].extendedDataLength = 0u;

    if (eventConfig.ExtendedDataCapture != NULL_PTR)
    {
        if (eventConfig.ExtendedDataCapture(
                eventConfig.EventId,
                Dem_PrimaryMemory[entryIndex].extendedData,
                &len) == E_OK)
        {
            if (len > DEM_EXTENDED_DATA_SIZE)
            {
                len = DEM_EXTENDED_DATA_SIZE;
            }

            Dem_PrimaryMemory[entryIndex].extendedDataLength = len;
        }
    }
}

static void Dem_StoreOrUpdatePrimaryEntry(uint16 eventIndex)
{
    uint16 entryIndex;
    Dem_EventIdType eventId;
    Dem_EventConfigType eventConfig;

    if (Dem_GetEventConfig(eventIndex, &eventConfig) != E_OK)
    {
        return;
    }

    if (Dem_IsStoredDataAllowed(eventIndex) == FALSE)
    {
        return;
    }

    if (eventConfig.StorageEnabled == FALSE)
    {
        return;
    }

    eventId = eventConfig.EventId;
    entryIndex = Dem_FindPrimaryEntryByEvent(eventId);

    if (entryIndex >= DEM_PRIMARY_MEMORY_SIZE)
    {
        entryIndex = Dem_AllocatePrimaryEntry(eventIndex);
    }

    if (entryIndex >= DEM_PRIMARY_MEMORY_SIZE)
    {
        return;
    }

    if (Dem_PrimaryMemory[entryIndex].valid == 0u)
    {
        memset(&Dem_PrimaryMemory[entryIndex], 0, sizeof(Dem_PrimaryMemory[entryIndex]));

        Dem_PrimaryMemory[entryIndex].valid = TRUE;
        Dem_PrimaryMemory[entryIndex].eventId = eventId;
        Dem_PrimaryMemory[entryIndex].dtc = eventConfig.DTC & 0x00FFFFFFu;
        Dem_PrimaryMemory[entryIndex].firstFailedCycle = Dem_OperationCycleCounter;

        Dem_CaptureSnapshot(eventIndex, entryIndex);
    }

    Dem_PrimaryMemory[entryIndex].udsStatus = Dem_RuntimeEvents[eventIndex].udsStatus;
    Dem_PrimaryMemory[entryIndex].occurrenceCounter = Dem_RuntimeEvents[eventIndex].occurrenceCounter;
    Dem_PrimaryMemory[entryIndex].agingCounter = Dem_RuntimeEvents[eventIndex].agingCounter;
    Dem_PrimaryMemory[entryIndex].lastFailedCycle = Dem_OperationCycleCounter;
    Dem_PrimaryMemory[entryIndex].lastChangedCounter = ++Dem_ChangeCounter;

    Dem_CaptureSnapshot(eventIndex, entryIndex);
}

static void Dem_RemovePrimaryEntryByEvent(Dem_EventIdType eventId)
{
    uint16 i;

    for (i = 0u; i < DEM_PRIMARY_MEMORY_SIZE; i++)
    {
        if ((Dem_PrimaryMemory[i].valid != 0u) &&
            (Dem_PrimaryMemory[i].eventId == eventId))
        {
            memset(&Dem_PrimaryMemory[i], 0, sizeof(Dem_PrimaryMemory[i]));
        }
    }
}

static void Dem_ProcessFailed(uint16 eventIndex)
{
    Dem_UdsStatusByteType oldStatus;
    Dem_UdsStatusByteType status;
    uint8 confirmationThreshold;
    Dem_EventConfigType eventConfig;
    boolean testFailedTransition;

    if (Dem_GetEventConfig(eventIndex, &eventConfig) != E_OK)
    {
        return;
    }

    oldStatus = Dem_RuntimeEvents[eventIndex].udsStatus;
    status = oldStatus;
    testFailedTransition = (((oldStatus & DEM_UDS_STATUS_TF) == 0u) ? TRUE : FALSE);

    status &= (uint8)~DEM_UDS_STATUS_TNCSLC;
    status &= (uint8)~DEM_UDS_STATUS_TNCTOC;

    status |= DEM_UDS_STATUS_TF;
    status |= DEM_UDS_STATUS_TFTOC;
    status |= DEM_UDS_STATUS_PDTC;
    status |= DEM_UDS_STATUS_TFSLC;

    confirmationThreshold = eventConfig.ConfirmationThreshold;
    if (confirmationThreshold == 0u)
    {
        confirmationThreshold = 1u;
    }

    if (testFailedTransition != FALSE)
    {
        if (Dem_RuntimeEvents[eventIndex].confirmationCounter < 255u)
        {
            Dem_RuntimeEvents[eventIndex].confirmationCounter++;
        }

        if (Dem_RuntimeEvents[eventIndex].confirmationCounter >= confirmationThreshold)
        {
            status |= DEM_UDS_STATUS_CDTC;
        }

        if (Dem_RuntimeEvents[eventIndex].occurrenceCounter < 255u)
        {
            Dem_RuntimeEvents[eventIndex].occurrenceCounter++;
        }

        Dem_RuntimeEvents[eventIndex].agingCounter = 0u;
    }

    Dem_SetStatusByte(eventIndex, status);

    if (testFailedTransition != FALSE)
    {
        Dem_StoreOrUpdatePrimaryEntry(eventIndex);
        Dem_Dirty = TRUE;
    }
}

static void Dem_ProcessPassed(uint16 eventIndex)
{
    Dem_UdsStatusByteType oldStatus;
    Dem_UdsStatusByteType status;
    boolean testPassedTransition;

    oldStatus = Dem_RuntimeEvents[eventIndex].udsStatus;
    status = oldStatus;
    testPassedTransition = (((oldStatus & DEM_UDS_STATUS_TF) != 0u) ? TRUE : FALSE);

    status &= (uint8)~DEM_UDS_STATUS_TF;
    status &= (uint8)~DEM_UDS_STATUS_TNCSLC;
    status &= (uint8)~DEM_UDS_STATUS_TNCTOC;

    Dem_SetStatusByte(eventIndex, status);

    if (testPassedTransition != FALSE)
    {
        Dem_Dirty = TRUE;
    }
}

static void Dem_ProcessPreFailed(uint16 eventIndex)
{
    sint16 counter;
    Dem_EventConfigType eventConfig;

    if (Dem_GetEventConfig(eventIndex, &eventConfig) != E_OK)
    {
        return;
    }

    counter = Dem_RuntimeEvents[eventIndex].debounceCounter;
    counter = (sint16)(counter + eventConfig.IncrementStep);

    if (counter > eventConfig.FailedThreshold)
    {
        counter = eventConfig.FailedThreshold;
    }

    Dem_RuntimeEvents[eventIndex].debounceCounter = counter;

    if (counter >= eventConfig.FailedThreshold)
    {
        Dem_ProcessFailed(eventIndex);
    }
}

static void Dem_ProcessPrePassed(uint16 eventIndex)
{
    sint16 counter;
    Dem_EventConfigType eventConfig;

    if (Dem_GetEventConfig(eventIndex, &eventConfig) != E_OK)
    {
        return;
    }

    counter = Dem_RuntimeEvents[eventIndex].debounceCounter;
    counter = (sint16)(counter - eventConfig.DecrementStep);

    if (counter < eventConfig.PassedThreshold)
    {
        counter = eventConfig.PassedThreshold;
    }

    Dem_RuntimeEvents[eventIndex].debounceCounter = counter;

    if (counter <= eventConfig.PassedThreshold)
    {
        Dem_ProcessPassed(eventIndex);
    }
}

static boolean Dem_DtcAlreadySeen(uint16 currentIndex, Dem_DTCType dtc)
{
    uint16 i;
    Dem_EventConfigType eventConfig;

    for (i = 0u; i < currentIndex; i++)
    {
        if ((Dem_GetEventConfig(i, &eventConfig) == E_OK) &&
            ((eventConfig.DTC & 0x00FFFFFFu) == (dtc & 0x00FFFFFFu)))
        {
            if ((Dem_RuntimeEvents[i].udsStatus & Dem_Filter.statusMask) != 0u)
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

static boolean Dem_FilterMatch(uint16 eventIndex)
{
    Dem_UdsStatusByteType status;
    Dem_EventConfigType eventConfig;

    if (Dem_Filter.active == FALSE)
    {
        return FALSE;
    }

    if (Dem_IsValidDtcFormatAndOrigin(Dem_Filter.format, Dem_Filter.origin) == FALSE)
    {
        return FALSE;
    }

    if (Dem_GetEventConfig(eventIndex, &eventConfig) != E_OK)
    {
        return FALSE;
    }

    if ((eventConfig.DTC & 0x00FFFFFFu) == 0u)
    {
        return FALSE;
    }

    if (Dem_Filter.filterWithSeverity != FALSE)
    {
        if ((eventConfig.Severity & Dem_Filter.severityMask) == 0u)
        {
            return FALSE;
        }
    }

    status = Dem_RuntimeEvents[eventIndex].udsStatus;

    if (Dem_Filter.statusMask != 0u)
    {
        if ((status & Dem_Filter.statusMask) == 0u)
        {
            return FALSE;
        }
    }
    else
    {
        if (status == 0u)
        {
            return FALSE;
        }
    }

    if (Dem_DtcAlreadySeen(eventIndex, eventConfig.DTC) != FALSE)
    {
        return FALSE;
    }

    return TRUE;
}

static void Dem_ClearRuntimeEvent(uint16 eventIndex)
{
    Dem_UdsStatusByteType oldStatus;
    Dem_UdsStatusByteType newStatus;
    Dem_EventConfigType eventConfig;

    oldStatus = Dem_RuntimeEvents[eventIndex].udsStatus;
    newStatus = (uint8)(DEM_UDS_STATUS_TNCSLC | DEM_UDS_STATUS_TNCTOC);

    if (Dem_GetEventConfig(eventIndex, &eventConfig) != E_OK)
    {
        return;
    }

    DEM_ENTER_CRITICAL();
    Dem_RuntimeEvents[eventIndex].udsStatus = newStatus;
    Dem_RuntimeEvents[eventIndex].debounceCounter = 0;
    Dem_RuntimeEvents[eventIndex].confirmationCounter = 0u;
    Dem_RuntimeEvents[eventIndex].occurrenceCounter = 0u;
    Dem_RuntimeEvents[eventIndex].agingCounter = 0u;

    Dem_RemovePrimaryEntryByEvent(eventConfig.EventId);
    DEM_EXIT_CRITICAL();

    if (oldStatus != newStatus)
    {
        Dem_EventStatusChangeCounter[eventIndex]++;
        Dem_InvokeStatusChanged(eventIndex, oldStatus, newStatus);
    }

    Dem_DebugUpdateEvent(eventIndex);
}

void Dem_PreInit(void)
{
    Dem_ConfigPtr = NULL_PTR;
    Dem_InitState = DEM_UNINIT;
    Dem_NvMState = DEM_NVM_STATE_IDLE;
    Dem_Dirty = FALSE;
    Dem_OperationCycleActive = FALSE;
    Dem_OperationCycleCounter = 0u;
    Dem_ChangeCounter = 0u;
    Dem_ClearStatus = DEM_CLEAR_IDLE;
    Dem_NvMWriteRetry = 0u;

    memset(Dem_RuntimeEvents, 0, sizeof(Dem_RuntimeEvents));
    memset(Dem_PrimaryMemory, 0, sizeof(Dem_PrimaryMemory));
    memset(&Dem_NvImage, 0, sizeof(Dem_NvImage));
    memset(&Dem_Filter, 0, sizeof(Dem_Filter));
    Dem_DebugReset();
}

void Dem_Init(const Dem_ConfigType *ConfigPtr)
{
    Dem_PreInit();

    if (ConfigPtr == NULL_PTR)
    {
        Dem_ConfigPtr = &Dem_Config;
    }
    else
    {
        Dem_ConfigPtr = ConfigPtr;
    }

    if ((Dem_ConfigPtr == NULL_PTR) ||
        (Dem_ConfigPtr->Events == NULL_PTR) ||
        (Dem_ConfigPtr->EventCount == 0u) ||
        (Dem_ConfigPtr->EventCount > DEM_MAX_EVENTS))
    {
        Dem_InitState = DEM_UNINIT;
        return;
    }

    Dem_InitDefaultRuntime();

#if (DEM_NVM_ENABLED == 1u)
    if (Dem_NvM_StartRead(Dem_ConfigPtr->NvMBlockId, &Dem_NvImage) == E_OK)
    {
        Dem_InitState = DEM_INIT_RESTORE_PENDING;
        Dem_NvMState = DEM_NVM_STATE_READ_PENDING;
    }
    else
    {
        Dem_InitState = DEM_INITIALIZED;
        Dem_NvMState = DEM_NVM_STATE_IDLE;
        Dem_Dirty = TRUE;
    }
#else
    Dem_InitState = DEM_INITIALIZED;
    Dem_NvMState = DEM_NVM_STATE_IDLE;
#endif
}

Std_ReturnType Dem_Shutdown(void)
{
    if (Dem_InitState == DEM_UNINIT)
    {
        return E_NOT_OK;
    }

#if (DEM_NVM_ENABLED == 1u)
    if (Dem_Dirty != FALSE)
    {
        Dem_BuildNvImage();

        if (Dem_NvM_UpdateRamBlock(Dem_ConfigPtr->NvMBlockId, &Dem_NvImage) == E_OK)
        {
            Dem_Dirty = FALSE;
            if (Dem_ClearStatus == DEM_CLEAR_PENDING)
            {
                Dem_ClearStatus = DEM_CLEAR_OK;
            }
            return E_OK;
        }

        return E_NOT_OK;
    }
#endif

    return E_OK;
}

Dem_InitStateType Dem_GetInitState(void)
{
    return Dem_InitState;
}

boolean Dem_IsReady(void)
{
    return (Dem_InitState == DEM_INITIALIZED) ? TRUE : FALSE;
}

void Dem_MainFunction(void)
{
#if (DEM_NVM_ENABLED == 1u)
    Dem_NvMRequestResultType result;

    if (Dem_ConfigPtr == NULL_PTR)
    {
        return;
    }

    if (Dem_NvMState == DEM_NVM_STATE_READ_PENDING)
    {
        result = Dem_NvM_GetResult(Dem_ConfigPtr->NvMBlockId);

        if (result == DEM_NVM_REQ_PENDING)
        {
            return;
        }

        if (result == DEM_NVM_REQ_OK)
        {
            Dem_LoadNvImage(&Dem_NvImage);
        }
        else
        {
            Dem_InitDefaultRuntime();
            Dem_Dirty = TRUE;
        }

        Dem_InitState = DEM_INITIALIZED;
        Dem_NvMState = DEM_NVM_STATE_IDLE;
    }

    if (Dem_InitState != DEM_INITIALIZED)
    {
        return;
    }

    if (Dem_NvMState == DEM_NVM_STATE_WRITE_PENDING)
    {
        result = Dem_NvM_GetResult(Dem_ConfigPtr->NvMBlockId);

        if (result == DEM_NVM_REQ_PENDING)
        {
            return;
        }

        if (result == DEM_NVM_REQ_OK)
        {
            Dem_NvMState = DEM_NVM_STATE_IDLE;
            Dem_NvMWriteRetry = 0u;

            if (Dem_ClearStatus == DEM_CLEAR_PENDING)
            {
                Dem_ClearStatus = DEM_CLEAR_OK;
            }
        }
        else
        {
            Dem_NvMState = DEM_NVM_STATE_IDLE;

            if (Dem_NvMWriteRetry < DEM_NVM_WRITE_RETRY_LIMIT)
            {
                Dem_NvMWriteRetry++;
                Dem_Dirty = TRUE;
            }
            else
            {
                Dem_NvMWriteRetry = 0u;

                if (Dem_ClearStatus == DEM_CLEAR_PENDING)
                {
                    Dem_ClearStatus = DEM_CLEAR_FAILED;
                }
            }
        }
    }

    if ((Dem_NvMState == DEM_NVM_STATE_IDLE) && (Dem_Dirty != FALSE))
    {
        Dem_BuildNvImage();

        if (Dem_NvM_UpdateRamBlock(Dem_ConfigPtr->NvMBlockId, &Dem_NvImage) == E_OK)
        {
            Dem_Dirty = FALSE;
            if (Dem_ClearStatus == DEM_CLEAR_PENDING)
            {
                Dem_ClearStatus = DEM_CLEAR_OK;
            }
        }
    }
#else
    if (Dem_ClearStatus == DEM_CLEAR_PENDING)
    {
        Dem_ClearStatus = DEM_CLEAR_OK;
    }
#endif

    Dem_MainFunction_Counter++;
}

Std_ReturnType Dem_SetEventStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus)
{
    uint16 eventIndex;
    Dem_EventConfigType eventConfig;

    if (Dem_InitState != DEM_INITIALIZED)
    {
        return E_NOT_OK;
    }

    eventIndex = Dem_FindEventIndex(EventId);

    if (eventIndex >= DEM_MAX_EVENTS)
    {
        return E_NOT_OK;
    }

    if (Dem_RuntimeEvents[eventIndex].available == FALSE)
    {
        return E_NOT_OK;
    }

    if (Dem_GetEventConfig(eventIndex, &eventConfig) != E_OK)
    {
        return E_NOT_OK;
    }

    DEM_ENTER_CRITICAL();

    if ((uint8)EventStatus < DEM_EVENT_STATUS_DEBUG_COUNT)
    {
        Dem_SetEventStatusCounter[eventIndex][(uint8)EventStatus]++;
    }

    switch (EventStatus)
    {
        case DEM_EVENT_STATUS_FAILED:
            Dem_RuntimeEvents[eventIndex].debounceCounter =
                eventConfig.FailedThreshold;
            DEM_EXIT_CRITICAL();
            Dem_ProcessFailed(eventIndex);
            break;

        case DEM_EVENT_STATUS_PASSED:
            Dem_RuntimeEvents[eventIndex].debounceCounter =
                eventConfig.PassedThreshold;
            DEM_EXIT_CRITICAL();
            Dem_ProcessPassed(eventIndex);
            break;

        case DEM_EVENT_STATUS_PREFAILED:
            DEM_EXIT_CRITICAL();
            Dem_ProcessPreFailed(eventIndex);
            break;

        case DEM_EVENT_STATUS_PREPASSED:
            DEM_EXIT_CRITICAL();
            Dem_ProcessPrePassed(eventIndex);
            break;

        default:
            DEM_EXIT_CRITICAL();
            return E_NOT_OK;
    }

    Dem_DebugUpdateEvent(eventIndex);

    return E_OK;
}

Std_ReturnType Dem_ReportErrorStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus)
{
    return Dem_SetEventStatus(EventId, EventStatus);
}

Std_ReturnType Dem_ResetEventStatus(Dem_EventIdType EventId)
{
    uint16 eventIndex;
    Dem_UdsStatusByteType status;

    if (Dem_InitState != DEM_INITIALIZED)
    {
        return E_NOT_OK;
    }

    eventIndex = Dem_FindEventIndex(EventId);

    if (eventIndex >= DEM_MAX_EVENTS)
    {
        return E_NOT_OK;
    }

    status = Dem_RuntimeEvents[eventIndex].udsStatus;
    status &= (uint8)~DEM_UDS_STATUS_TF;

    DEM_ENTER_CRITICAL();
    Dem_RuntimeEvents[eventIndex].debounceCounter = 0;
    DEM_EXIT_CRITICAL();
    Dem_SetStatusByte(eventIndex, status);

    return E_OK;
}

Std_ReturnType Dem_GetEventUdsStatus(
    Dem_EventIdType EventId,
    Dem_UdsStatusByteType *EventStatusByte
)
{
    uint16 eventIndex;

    if ((Dem_InitState != DEM_INITIALIZED) || (EventStatusByte == NULL_PTR))
    {
        return E_NOT_OK;
    }

    eventIndex = Dem_FindEventIndex(EventId);

    if (eventIndex >= DEM_MAX_EVENTS)
    {
        return E_NOT_OK;
    }

    *EventStatusByte = Dem_RuntimeEvents[eventIndex].udsStatus;
    return E_OK;
}

Std_ReturnType Dem_GetEventFailed(
    Dem_EventIdType EventId,
    boolean *EventFailed
)
{
    Dem_UdsStatusByteType status;

    if (EventFailed == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (Dem_GetEventUdsStatus(EventId, &status) != E_OK)
    {
        return E_NOT_OK;
    }

    *EventFailed = ((status & DEM_UDS_STATUS_TF) != 0u) ? TRUE : FALSE;
    return E_OK;
}

Std_ReturnType Dem_GetEventTested(
    Dem_EventIdType EventId,
    boolean *EventTested
)
{
    Dem_UdsStatusByteType status;

    if (EventTested == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (Dem_GetEventUdsStatus(EventId, &status) != E_OK)
    {
        return E_NOT_OK;
    }

    *EventTested = ((status & DEM_UDS_STATUS_TNCTOC) == 0u) ? TRUE : FALSE;
    return E_OK;
}

Std_ReturnType Dem_GetFaultDetectionCounter(
    Dem_EventIdType EventId,
    sint8 *FaultDetectionCounter
)
{
    uint16 eventIndex;
    sint16 counter;
    sint16 failedThreshold;
    sint16 passedThreshold;
    sint32 scaled;
    Dem_EventConfigType eventConfig;

    if ((Dem_InitState != DEM_INITIALIZED) || (FaultDetectionCounter == NULL_PTR))
    {
        return E_NOT_OK;
    }

    eventIndex = Dem_FindEventIndex(EventId);

    if (eventIndex >= DEM_MAX_EVENTS)
    {
        return E_NOT_OK;
    }

    if (Dem_GetEventConfig(eventIndex, &eventConfig) != E_OK)
    {
        return E_NOT_OK;
    }

    counter = Dem_RuntimeEvents[eventIndex].debounceCounter;
    failedThreshold = eventConfig.FailedThreshold;
    passedThreshold = eventConfig.PassedThreshold;

    if (counter >= failedThreshold)
    {
        *FaultDetectionCounter = 127;
        return E_OK;
    }

    if (counter <= passedThreshold)
    {
        *FaultDetectionCounter = -128;
        return E_OK;
    }

    if (counter >= 0)
    {
        scaled = ((sint32)counter * 127) / failedThreshold;
    }
    else
    {
        scaled = ((sint32)(-counter) * -128) / (sint32)(-passedThreshold);
    }

    if (scaled > 127)
    {
        scaled = 127;
    }

    if (scaled < -128)
    {
        scaled = -128;
    }

    *FaultDetectionCounter = (sint8)scaled;
    return E_OK;
}

Std_ReturnType Dem_GetDTCOfEvent(
    Dem_EventIdType EventId,
    Dem_DTCFormatType DTCFormat,
    Dem_DTCType *DTCOfEvent
)
{
    uint16 eventIndex;
    Dem_EventConfigType eventConfig;

    if ((Dem_InitState != DEM_INITIALIZED) || (DTCOfEvent == NULL_PTR))
    {
        return E_NOT_OK;
    }

    if (DTCFormat != DEM_DTC_FORMAT_UDS)
    {
        return E_NOT_OK;
    }

    eventIndex = Dem_FindEventIndex(EventId);

    if (eventIndex >= DEM_MAX_EVENTS)
    {
        return E_NOT_OK;
    }

    if (Dem_GetEventConfig(eventIndex, &eventConfig) != E_OK)
    {
        return E_NOT_OK;
    }

    *DTCOfEvent = eventConfig.DTC & 0x00FFFFFFu;
    return E_OK;
}

Std_ReturnType Dem_SetOperationCycleState(
    Dem_OperationCycleIdType OperationCycleId,
    Dem_OperationCycleStateType CycleState
)
{
    uint16 i;

    if (Dem_InitState != DEM_INITIALIZED)
    {
        return E_NOT_OK;
    }

    if (OperationCycleId != Dem_ConfigPtr->DefaultOperationCycleId)
    {
        return E_NOT_OK;
    }

    if (CycleState == DEM_CYCLE_STATE_START)
    {
        Dem_OperationCycleActive = TRUE;
        Dem_OperationCycleCounter++;

        for (i = 0u; i < Dem_ConfigPtr->EventCount; i++)
        {
            Dem_UdsStatusByteType status = Dem_RuntimeEvents[i].udsStatus;

            status &= (uint8)~DEM_UDS_STATUS_TFTOC;
            status |= DEM_UDS_STATUS_TNCTOC;

            Dem_SetStatusByte(i, status);
        }

        return E_OK;
    }

    if (CycleState == DEM_CYCLE_STATE_END)
    {
        Dem_OperationCycleActive = FALSE;

        for (i = 0u; i < Dem_ConfigPtr->EventCount; i++)
        {
            Dem_UdsStatusByteType status = Dem_RuntimeEvents[i].udsStatus;
            boolean failedThisCycle = ((status & DEM_UDS_STATUS_TFTOC) != 0u) ? TRUE : FALSE;
            boolean testedThisCycle = ((status & DEM_UDS_STATUS_TNCTOC) == 0u) ? TRUE : FALSE;
            Dem_EventConfigType eventConfig;

            if (Dem_GetEventConfig(i, &eventConfig) != E_OK)
            {
                continue;
            }

            if ((failedThisCycle == FALSE) && (testedThisCycle != FALSE))
            {
                status &= (uint8)~DEM_UDS_STATUS_PDTC;
            }

            if (((status & DEM_UDS_STATUS_CDTC) != 0u) &&
                (failedThisCycle == FALSE) &&
                (testedThisCycle != FALSE) &&
                (eventConfig.AgingAllowed != FALSE))
            {
                if (Dem_RuntimeEvents[i].agingCounter < 255u)
                {
                    Dem_RuntimeEvents[i].agingCounter++;
                }

                if (Dem_RuntimeEvents[i].agingCounter >= eventConfig.AgingThreshold)
                {
                    status &= (uint8)~DEM_UDS_STATUS_CDTC;
                    Dem_RemovePrimaryEntryByEvent(eventConfig.EventId);
                }
            }

            Dem_SetStatusByte(i, status);
        }

        return E_OK;
    }

    return E_NOT_OK;
}

Std_ReturnType Dem_GetStatusOfDTC(
    Dem_DTCType DTC,
    Dem_DTCFormatType DTCFormat,
    Dem_DTCOriginType DTCOrigin,
    Dem_UdsStatusByteType *DTCStatus
)
{
    uint16 eventIndex;

    if ((Dem_InitState != DEM_INITIALIZED) || (DTCStatus == NULL_PTR))
    {
        return E_NOT_OK;
    }

    if (Dem_IsValidDtcFormatAndOrigin(DTCFormat, DTCOrigin) == FALSE)
    {
        return E_NOT_OK;
    }

    eventIndex = Dem_FindEventIndexByDTC(DTC);

    if (eventIndex >= DEM_MAX_EVENTS)
    {
        return E_NOT_OK;
    }

    *DTCStatus = Dem_RuntimeEvents[eventIndex].udsStatus;
    return E_OK;
}

Std_ReturnType Dem_ClearDTC(
    Dem_DTCType DTC,
    Dem_DTCFormatType DTCFormat,
    Dem_DTCOriginType DTCOrigin
)
{
    uint16 i;
    boolean found = FALSE;

    if (Dem_InitState != DEM_INITIALIZED)
    {
        return E_NOT_OK;
    }

    if (Dem_IsValidDtcFormatAndOrigin(DTCFormat, DTCOrigin) == FALSE)
    {
        return E_NOT_OK;
    }

    if (Dem_ClearStatus == DEM_CLEAR_PENDING)
    {
        return E_NOT_OK;
    }

    if ((DTC & 0x00FFFFFFu) == DEM_DTC_GROUP_ALL_DTCS)
    {
        for (i = 0u; i < Dem_ConfigPtr->EventCount; i++)
        {
            Dem_ClearRuntimeEvent(i);
        }

        found = TRUE;
    }
    else
    {
        for (i = 0u; i < Dem_ConfigPtr->EventCount; i++)
        {
            Dem_EventConfigType eventConfig;

            if ((Dem_GetEventConfig(i, &eventConfig) == E_OK) &&
                ((eventConfig.DTC & 0x00FFFFFFu) == (DTC & 0x00FFFFFFu)))
            {
                Dem_ClearRuntimeEvent(i);
                found = TRUE;
            }
        }
    }

    if (found == FALSE)
    {
        return E_NOT_OK;
    }

    Dem_Dirty = TRUE;

#if (DEM_NVM_ENABLED == 1u)
    Dem_ClearStatus = DEM_CLEAR_PENDING;
#else
    Dem_ClearStatus = DEM_CLEAR_OK;
#endif

    return E_OK;
}

Dem_ClearDTCStatusType Dem_GetClearDTCStatus(void)
{
    return Dem_ClearStatus;
}

Std_ReturnType Dem_SetDTCFilter(
    uint8 DTCStatusMask,
    Dem_DTCFormatType DTCFormat,
    Dem_DTCOriginType DTCOrigin,
    boolean FilterWithSeverity,
    Dem_DTCSeverityType DTCSeverityMask,
    boolean FilterForFaultDetectionCounter
)
{
    if (Dem_InitState != DEM_INITIALIZED)
    {
        return E_NOT_OK;
    }

    if (Dem_IsValidDtcFormatAndOrigin(DTCFormat, DTCOrigin) == FALSE)
    {
        return E_NOT_OK;
    }

    Dem_Filter.active = TRUE;
    Dem_Filter.statusMask = DTCStatusMask;
    Dem_Filter.format = DTCFormat;
    Dem_Filter.origin = DTCOrigin;
    Dem_Filter.filterWithSeverity = FilterWithSeverity;
    Dem_Filter.severityMask = DTCSeverityMask;
    Dem_Filter.filterForFdc = FilterForFaultDetectionCounter;
    Dem_Filter.nextIndex = 0u;

    return E_OK;
}

Std_ReturnType Dem_GetNumberOfFilteredDTC(uint16 *NumberOfFilteredDTC)
{
    uint16 i;
    uint16 count = 0u;
    uint16 savedNextIndex;

    if ((Dem_InitState != DEM_INITIALIZED) || (NumberOfFilteredDTC == NULL_PTR))
    {
        return E_NOT_OK;
    }

    if (Dem_Filter.active == FALSE)
    {
        return E_NOT_OK;
    }

    savedNextIndex = Dem_Filter.nextIndex;
    Dem_Filter.nextIndex = 0u;

    for (i = 0u; i < Dem_ConfigPtr->EventCount; i++)
    {
        if (Dem_FilterMatch(i) != FALSE)
        {
            count++;
        }
    }

    Dem_Filter.nextIndex = savedNextIndex;

    *NumberOfFilteredDTC = count;
    return E_OK;
}

Std_ReturnType Dem_GetNextFilteredDTC(
    Dem_DTCType *DTC,
    Dem_UdsStatusByteType *DTCStatus
)
{
    uint16 i;

    if ((Dem_InitState != DEM_INITIALIZED) ||
        (DTC == NULL_PTR) ||
        (DTCStatus == NULL_PTR))
    {
        return E_NOT_OK;
    }

    if (Dem_Filter.active == FALSE)
    {
        return E_NOT_OK;
    }

    for (i = Dem_Filter.nextIndex; i < Dem_ConfigPtr->EventCount; i++)
    {
        if (Dem_FilterMatch(i) != FALSE)
        {
            Dem_EventConfigType eventConfig;

            if (Dem_GetEventConfig(i, &eventConfig) != E_OK)
            {
                continue;
            }

            *DTC = eventConfig.DTC & 0x00FFFFFFu;
            *DTCStatus = Dem_RuntimeEvents[i].udsStatus;
            Dem_Filter.nextIndex = (uint16)(i + 1u);
            return E_OK;
        }
    }

    return E_NOT_OK;
}

Std_ReturnType Dem_GetFreezeFrameDataByDTC(
    Dem_DTCType DTC,
    Dem_DTCFormatType DTCFormat,
    Dem_DTCOriginType DTCOrigin,
    uint8 RecordNumber,
    uint8 *DestBuffer,
    uint16 *BufSize
)
{
    uint16 entryIndex;
    uint16 copyLen;

    if ((Dem_InitState != DEM_INITIALIZED) ||
        (DestBuffer == NULL_PTR) ||
        (BufSize == NULL_PTR))
    {
        return E_NOT_OK;
    }

    if (Dem_IsValidDtcFormatAndOrigin(DTCFormat, DTCOrigin) == FALSE)
    {
        return E_NOT_OK;
    }

    if ((RecordNumber != 0x01u) && (RecordNumber != 0xFFu))
    {
        return E_NOT_OK;
    }

    entryIndex = Dem_FindPrimaryEntryByDTC(DTC);

    if (entryIndex >= DEM_PRIMARY_MEMORY_SIZE)
    {
        return E_NOT_OK;
    }

    copyLen = Dem_PrimaryMemory[entryIndex].freezeFrameLength;

    if (*BufSize < copyLen)
    {
        return E_NOT_OK;
    }

    memcpy(DestBuffer, Dem_PrimaryMemory[entryIndex].freezeFrame, copyLen);
    *BufSize = copyLen;

    return E_OK;
}

Std_ReturnType Dem_GetExtendedDataRecordByDTC(
    Dem_DTCType DTC,
    Dem_DTCFormatType DTCFormat,
    Dem_DTCOriginType DTCOrigin,
    uint8 RecordNumber,
    uint8 *DestBuffer,
    uint16 *BufSize
)
{
    uint16 entryIndex;
    uint16 copyLen;

    if ((Dem_InitState != DEM_INITIALIZED) ||
        (DestBuffer == NULL_PTR) ||
        (BufSize == NULL_PTR))
    {
        return E_NOT_OK;
    }

    if (Dem_IsValidDtcFormatAndOrigin(DTCFormat, DTCOrigin) == FALSE)
    {
        return E_NOT_OK;
    }

    if ((RecordNumber != 0x01u) && (RecordNumber != 0xFFu))
    {
        return E_NOT_OK;
    }

    entryIndex = Dem_FindPrimaryEntryByDTC(DTC);

    if (entryIndex >= DEM_PRIMARY_MEMORY_SIZE)
    {
        return E_NOT_OK;
    }

    copyLen = Dem_PrimaryMemory[entryIndex].extendedDataLength;

    if (*BufSize < copyLen)
    {
        return E_NOT_OK;
    }

    memcpy(DestBuffer, Dem_PrimaryMemory[entryIndex].extendedData, copyLen);
    *BufSize = copyLen;

    return E_OK;
}

uint8 Dem_GetDTCStatusAvailabilityMask(void)
{
    return DEM_UDS_STATUS_AVAILABILITY_MASK;
}
