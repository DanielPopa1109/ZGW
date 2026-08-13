#ifndef AIMODEL_H
#define AIMODEL_H

#include "Ifx_Types.h"
#include "APP/AiModel/bcm_infer.h"
#include "Dem_Types.h"

#define AIMODEL_ERROR_INPUT_INVALID            0x00000001u
#define AIMODEL_ERROR_INPUT_STALE              0x00000002u
#define AIMODEL_ERROR_WINDOW_NOT_READY         0x00000004u
#define AIMODEL_ERROR_OUTPUT_NOT_FINITE        0x00000008u
#define AIMODEL_ERROR_OUTPUT_OUT_OF_RANGE      0x00000010u
#define AIMODEL_ERROR_DEADLINE_EXCEEDED        0x00000020u
#define AIMODEL_ERROR_MAILBOX_OVERFLOW         0x00000040u
#define AIMODEL_ERROR_MODEL_SCHEMA_MISMATCH    0x00000080u
#define AIMODEL_ERROR_INTERNAL_FAILURE         0x00000100u

#define AIMODEL_PERIOD_MS                      BCM_CYCLE_TIME_MS
#define AIMODEL_INFERENCE_PERIOD_MS            BCM_INFERENCE_PERIOD_MS
#define AIMODEL_WCET_BUDGET_US                 40000u

typedef struct
{
    uint8 channelId;
    float32 measuredCurrent_A;
    float32 currentRating_A;
    float32 predictedCurrent_A;
    float32 faultSoonProbability;
    float32 faultProbability[BCM_NUM_FAULT_CLASSES];
    float32 currentUtilization;
    uint8 predictedFaultClass;
    uint8 impendingOvercurrent;
    uint8 actualOvercurrent;
    uint8 inferenceValid;
} AiModel_ChannelResultType;

typedef struct
{
    float32 inputVoltage_V;
    uint8 undervoltage;
    uint8 overvoltage;
    uint8 inputValid;
    uint8 inferenceValid;
    uint8 windowReady;
    uint8 dominantFaultChannel;
    uint8 dominantFault;
    uint32 inputTimestamp;
    uint32 inferenceSequence;
    uint32 channelExecutionTimeUs;
    uint32 totalInferenceTimeUs;
    uint32 processingTimeUs;
    uint32 maxChannelExecutionTimeUs;
    uint32 maxTotalInferenceTimeUs;
    uint32 maxProcessingTimeUs;
    uint32 errorFlags;
} AiModel_GlobalResultType;

typedef struct
{
    float32 inputVoltage_V;
    AiModel_ChannelResultType channel[BCM_NUM_CHANNELS];
    uint8 undervoltage;
    uint8 overvoltage;
    uint8 inputValid;
    uint8 inferenceValid;
    uint8 windowReady;
    uint8 dominantFaultChannel;
    uint8 dominantFault;
    uint32 inputTimestamp;
    uint32 inferenceSequence;
    uint32 channelExecutionTimeUs;
    uint32 totalInferenceTimeUs;
    uint32 processingTimeUs;
    uint32 maxChannelExecutionTimeUs;
    uint32 maxTotalInferenceTimeUs;
    uint32 maxProcessingTimeUs;
    uint32 errorFlags;
} AiModel_ResultType;

typedef struct
{
    float32 voltage_V;
    float32 current_A[BCM_NUM_CHANNELS];
    uint8 signalValid;
    uint32 timestamp;
    uint32 cycleCounter;
} AiModel_Pdm1SnapshotType;

void AiModel_Init(void);
void AiModel_MainFunction(void);
Std_ReturnType AiModel_GetLatestResult(AiModel_ResultType *result);
Std_ReturnType AiModel_GetLatestGlobalResult(AiModel_GlobalResultType *result);
Std_ReturnType AiModel_GetLatestChannelResult(uint8 channel, AiModel_ChannelResultType *result);
Std_ReturnType AiModel_GetPdm1Snapshot(AiModel_Pdm1SnapshotType *snapshot);
Std_ReturnType AiModel_CaptureDiagSnapshotData(Dem_EventIdType eventId, uint8 *buffer, uint16 *length);

#endif
