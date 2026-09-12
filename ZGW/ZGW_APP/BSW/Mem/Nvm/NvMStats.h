#ifndef NVMSTATS_H
#define NVMSTATS_H

#include "Std_Types.h"
#include "BSW/Diag/Dcm/Dcm.h"

#define NVMSTATS_ROUTINE_ID                       (0xF195u)
#define NVMSTATS_MAGIC                            (0x4E565354u)
#define NVMSTATS_VERSION                          (1u)
#define NVMSTATS_COUNTER_WIDTH_BITS               (64u)
#define NVMSTATS_PER_BLOCK_COUNTERS               (8u)

#define NVMSTATS_SELECTOR_SUMMARY                 (0x00u)
#define NVMSTATS_SELECTOR_NVM                     (0x01u)
#define NVMSTATS_SELECTOR_READ                    (0x02u)
#define NVMSTATS_SELECTOR_WRITE                   (0x03u)
#define NVMSTATS_SELECTOR_FEE                     (0x04u)
#define NVMSTATS_SELECTOR_GC                      (0x05u)
#define NVMSTATS_SELECTOR_FLS                     (0x06u)
#define NVMSTATS_SELECTOR_ERROR                   (0x07u)
#define NVMSTATS_SELECTOR_UTILIZATION             (0x08u)
#define NVMSTATS_SELECTOR_PER_BLOCK               (0x09u)
#define NVMSTATS_SELECTOR_CLEAR                   (0x7Fu)

/*
 * Persistent lifetime image.
 *
 * Metadata:
 * - magic/version/length identify a compatible persisted image.
 *
 * Lifetime counters:
 * - bootCount: completed NvM_ReadAll startups that loaded or restored this image.
 * - nvmReadAllCount/nvmWriteAllCount: accepted NvM_ReadAll/NvM_WriteAll requests.
 *   The current-boot ReadAll acceptance is held outside the NvM mirror and
 *   merged after the statistics block has been restored.
 * - nvmWriteBlockRequests/Accepted/Rejected: external NvM_WriteBlock API requests by admission result.
 * - nvmSuccessfulLogicalWrites/nvmFailedLogicalWrites: completed non-statistics NvM logical writes by result.
 * - nvmReadBlockRequests/Accepted/Rejected: external NvM_ReadBlock API requests by admission result.
 * - nvmSuccessfulReads/nvmReadFailures: all completed NvM block reads by
 *   result, including blocks processed internally by NvM_ReadAll. These are
 *   intentionally a broader scope than the external ReadBlock API counters.
 * - nvmCrcFailures: NvM read failures reported as MEMIF_BLOCK_INCONSISTENT.
 * - nvmInvalidBlockReads: NvM read failures reported as MEMIF_BLOCK_INVALID.
 * - nvmQueueRejections: requests rejected because the single-operation NvM state machine was busy.
 * - nvmBusyRejections/nvmUninitRejections: requests rejected while busy or uninitialized.
 * - nvmInvalidBlockRejections: requests rejected because the NvM block ID was not configured.
 * - nvmWriteProtectionRejections: NvM_WriteBlock requests rejected by block write protection.
 * - nvmInvalidationRequests/Successes/Failures: NvM invalidation API attempts by final result.
 * - nvmEraseRequests: NvM_EraseNvBlock calls; this stack aliases erase to invalidation.
 * - nvmRestoredDefaults: NvM read completions that restored ROM defaults.
 * - feeWriteRequests/SuccessfulLogicalWrites/FailedLogicalWrites: Fee write jobs by request/final result.
 * - feeInvalidationRequests/Successes: Fee invalidation jobs by request/final success.
 * - feeGcCount: garbage-collection passes started.
 * - feeSectorSwitchCount: successful active-sector switches after GC copy.
 * - feeGcCopiedBlocks: live records copied by GC.
 * - feeGcCopiedPayloadBytes: payload bytes copied by GC.
 * - feeGcCopiedPhysicalBytes: complete physical record bytes copied by GC.
 * - feeIntegrityFailures: invalid sector/record/header/trailer/payload integrity observations.
 * - feeInvalidMissingBlockReads: reads for missing or invalidated Fee blocks.
 * - feeScanRecoveryEvents: boot-time scan recoveries from torn sector tails.
 * - feeDeferredFormatEvents: boot-time deferred-format recovery events.
 * - flsPageWriteCount/flsWriteBytes: DFLASH page program operations and programmed bytes.
 * - flsEraseSectorCount/flsEraseBytes: DFLASH sector erase operations and erased bytes.
 * - flsFailedJobs: completed Fls jobs with MEMIF_JOB_FAILED.
 * - flsWriteFailures/flsEraseFailures/flsReadFailures: failed Fls jobs by job type.
 * - flsDmuBusyRejects/flsDmuTimeouts: DMU readiness rejection/timeout observations.
 * - perBlockSuccessfulWrites: successful non-statistics NvM logical writes by configured block index.
 *
 * Last-state gauges:
 * - lastNvMError/lastFeeError/lastFlsError: packed most recent module error context.
 * - lastMemIfResult: most recent MemIf read/write result contributing to an NvM read/write outcome.
 * - activeFeeFreeBytes/activeFeeAppendOffset: current active-sector utilization sample.
 * - maxFeeAppendOffset/minFeeFreeBytes: lifetime high-water/low-water active-sector utilization.
 * - maxGcCopiedBlocks/maxGcCopiedPayloadBytes/maxGcCopiedPhysicalBytes: largest single GC copy pass.
 */
typedef struct
{
    uint32 magic;
    uint16 version;
    uint16 length;

    uint64 bootCount;
    uint64 nvmReadAllCount;
    uint64 nvmWriteAllCount;
    uint64 nvmWriteBlockRequests;
    uint64 nvmWriteBlockAccepted;
    uint64 nvmWriteBlockRejected;
    uint64 nvmSuccessfulLogicalWrites;
    uint64 nvmFailedLogicalWrites;
    uint64 nvmReadBlockRequests;
    uint64 nvmReadBlockAccepted;
    uint64 nvmReadBlockRejected;
    uint64 nvmSuccessfulReads;
    uint64 nvmReadFailures;
    uint64 nvmCrcFailures;
    uint64 nvmInvalidBlockReads;
    uint64 nvmQueueRejections;
    uint64 nvmBusyRejections;
    uint64 nvmUninitRejections;
    uint64 nvmInvalidBlockRejections;
    uint64 nvmWriteProtectionRejections;
    uint64 nvmInvalidationRequests;
    uint64 nvmInvalidationSuccesses;
    uint64 nvmInvalidationFailures;
    uint64 nvmEraseRequests;
    uint64 nvmRestoredDefaults;

    uint64 feeWriteRequests;
    uint64 feeSuccessfulLogicalWrites;
    uint64 feeFailedLogicalWrites;
    uint64 feeInvalidationRequests;
    uint64 feeInvalidationSuccesses;
    uint64 feeGcCount;
    uint64 feeSectorSwitchCount;
    uint64 feeGcCopiedBlocks;
    uint64 feeGcCopiedPayloadBytes;
    uint64 feeGcCopiedPhysicalBytes;
    uint64 feeIntegrityFailures;
    uint64 feeInvalidMissingBlockReads;
    uint64 feeScanRecoveryEvents;
    uint64 feeDeferredFormatEvents;

    uint64 flsPageWriteCount;
    uint64 flsWriteBytes;
    uint64 flsEraseSectorCount;
    uint64 flsEraseBytes;
    uint64 flsFailedJobs;
    uint64 flsWriteFailures;
    uint64 flsEraseFailures;
    uint64 flsReadFailures;
    uint64 flsDmuBusyRejects;
    uint64 flsDmuTimeouts;

    uint32 lastNvMError;
    uint32 lastFeeError;
    uint32 lastFlsError;
    uint32 lastMemIfResult;
    uint32 activeFeeFreeBytes;
    uint32 activeFeeAppendOffset;
    uint32 maxFeeAppendOffset;
    uint32 minFeeFreeBytes;
    uint32 maxGcCopiedBlocks;
    uint32 maxGcCopiedPayloadBytes;
    uint32 maxGcCopiedPhysicalBytes;

    uint64 perBlockSuccessfulWrites[NVMSTATS_PER_BLOCK_COUNTERS];
} NvMStats_NvImageType;

#define NVMSTATS_NVM_IMAGE_SIZE                   ((uint32)sizeof(NvMStats_NvImageType))

extern uint8 NvMStats_Ram[NVMSTATS_NVM_IMAGE_SIZE];
extern const uint8 NvMStats_Rom[NVMSTATS_NVM_IMAGE_SIZE];

void NvMStats_Init(void);
void NvMStats_OnReadAllComplete(void);
void NvMStats_PrepareForWriteAllBlock(uint16 blockIndex, uint16 blockId);
void NvMStats_MarkCleanAfterOwnWrite(void);
void NvMStats_AbortOwnWrite(void);
boolean NvMStats_IsDirty(void);

void NvMStats_RecordNvMError(uint8 api, uint8 error, uint32 detail);
void NvMStats_RecordNvMWriteBlockRequest(void);
void NvMStats_RecordNvMWriteBlockAccepted(void);
void NvMStats_RecordNvMWriteBlockRejected(void);
void NvMStats_RecordNvMReadBlockRequest(void);
void NvMStats_RecordNvMReadBlockAccepted(void);
void NvMStats_RecordNvMReadBlockRejected(void);
void NvMStats_RecordNvMReadAllAccepted(void);
void NvMStats_RecordNvMWriteAllAccepted(void);
void NvMStats_RecordNvMBusyRejection(void);
void NvMStats_RecordNvMUninitRejection(void);
void NvMStats_RecordNvMInvalidBlockRejection(void);
void NvMStats_RecordNvMWriteProtectionRejection(void);
void NvMStats_RecordNvMSuccessfulWrite(uint16 blockIndex, uint16 blockId);
void NvMStats_RecordNvMFailedWrite(void);
void NvMStats_RecordNvMSuccessfulRead(void);
void NvMStats_RecordNvMReadFailure(uint8 memIfResult);
void NvMStats_RecordNvMRestoredDefaults(void);
void NvMStats_RecordNvMInvalidationRequest(void);
void NvMStats_RecordNvMInvalidationSuccess(void);
void NvMStats_RecordNvMInvalidationFailure(void);
void NvMStats_RecordNvMEraseRequest(void);

void NvMStats_RecordFeeError(uint8 error, uint32 detail);
void NvMStats_RecordFeeWriteRequest(uint16 blockNumber);
void NvMStats_RecordFeeWriteSuccess(uint16 blockNumber);
void NvMStats_RecordFeeWriteFailure(void);
void NvMStats_RecordFeeInvalidationRequest(void);
void NvMStats_RecordFeeInvalidationSuccess(void);
void NvMStats_RecordFeeGcStart(void);
void NvMStats_RecordFeeSectorSwitch(void);
void NvMStats_RecordFeeGcCopiedRecord(uint32 payloadBytes, uint32 physicalBytes);
void NvMStats_RecordFeeIntegrityFailure(void);
void NvMStats_RecordFeeInvalidMissingBlockRead(void);
void NvMStats_RecordFeeScanRecovery(void);
void NvMStats_RecordFeeDeferredFormat(void);
void NvMStats_RecordFeeUtilization(uint32 appendOffset, uint32 freeBytes);

void NvMStats_RecordFlsError(uint8 api, uint8 error, uint32 detail);
void NvMStats_RecordFlsPageWrite(uint32 bytes);
void NvMStats_RecordFlsEraseSector(uint32 bytes);
void NvMStats_RecordFlsJobFailure(uint32 job);
void NvMStats_RecordFlsDmuBusyReject(void);
void NvMStats_RecordFlsDmuTimeout(void);

boolean NvMStats_IsRoutineId(uint16 routineId);
Dcm_ReturnType NvMStats_HandleRoutineControl(
        Dcm_OpStatusType opStatus,
        uint8 routineControlType,
        uint16 routineId,
        const uint8* reqData,
        Dcm_PduLengthType reqLen,
        uint8* respData,
        Dcm_PduLengthType* respLen);

#endif /* NVMSTATS_H */
