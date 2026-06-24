#ifndef DEM_INT_H_
#define DEM_INT_H_

#include "Dem_Cfg.h"

typedef struct
{
    Dem_EventIdType eventId;
    Dem_UdsStatusByteType udsStatus;
    sint16 debounceCounter;
    uint8 confirmationCounter;
    uint8 occurrenceCounter;
    uint8 agingCounter;
    uint8 available;
} Dem_RuntimeEventType;

typedef struct
{
    uint8 valid;
    Dem_EventIdType eventId;
    Dem_DTCType dtc;
    Dem_UdsStatusByteType udsStatus;

    uint8 occurrenceCounter;
    uint8 agingCounter;

    uint32 firstFailedCycle;
    uint32 lastFailedCycle;
    uint32 lastChangedCounter;

    uint16 snapshotDataLength;
    uint8 snapshotData[DEM_SNAPSHOT_DATA_SIZE];
} Dem_PrimaryEntryType;

typedef struct
{
    Dem_EventIdType eventId;
    Dem_UdsStatusByteType udsStatus;
    sint16 debounceCounter;
    uint8 confirmationCounter;
    uint8 occurrenceCounter;
    uint8 agingCounter;
} Dem_NvEventRecordType;

typedef struct
{
    uint32 magic;
    uint16 version;
    uint16 eventRecordCount;
    uint16 primaryEntryCount;
    uint16 reserved;

    Dem_NvEventRecordType eventRecords[DEM_MAX_EVENTS];
    Dem_PrimaryEntryType primaryEntries[DEM_PRIMARY_MEMORY_SIZE];

    uint32 trailerReserved;
} Dem_NvImageType;

extern Dem_NvImageType Dem_NvImage;

typedef struct
{
    boolean active;
    uint8 statusMask;
    Dem_DTCFormatType format;
    Dem_DTCOriginType origin;
    boolean filterWithSeverity;
    Dem_DTCSeverityType severityMask;
    boolean filterForFdc;
    uint16 nextIndex;
} Dem_FilterType;

#endif
