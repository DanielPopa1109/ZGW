#ifndef AIMODEL_H
#define AIMODEL_H

#include "Ifx_Types.h"
#include "APP/AiModel/bcm_infer.h"

#define AIMODEL_ERROR_PDM1_INPUT_INVALID       0x00000001u
#define AIMODEL_ERROR_PDM1_INPUT_STALE         0x00000002u
#define AIMODEL_ERROR_WINDOW_NOT_READY         0x00000004u
#define AIMODEL_ERROR_OUTPUT_NOT_FINITE        0x00000008u
#define AIMODEL_ERROR_OUTPUT_OUT_OF_RANGE      0x00000010u
#define AIMODEL_ERROR_DEADLINE_EXCEEDED        0x00000020u
#define AIMODEL_ERROR_MAILBOX_OVERFLOW         0x00000040u
#define AIMODEL_ERROR_MODEL_SCHEMA_MISMATCH    0x00000080u
#define AIMODEL_ERROR_INTERNAL_FAILURE         0x00000100u

#define AIMODEL_PERIOD_MS                      5u
#define AIMODEL_WCET_BUDGET_US                 2000u

typedef struct
{
    float32 inputVoltage_V;
    float32 inputCurrent_A;
    float32 inputTemperature_C;
    float32 expectedVoltage_V;
    float32 expectedCurrent_A;
    float32 expectedTemperature_C;
    float32 anomaly;
    float32 health;
    float32 faultProbability[BCM_NUM_FAULT_CLASSES];
    uint8 dominantFault;
    uint8 inputValid;
    uint8 inferenceValid;
    uint8 windowReady;
    uint8 reserved;
    uint32 inputTimestamp;
    uint32 inferenceSequence;
    uint32 executionTimeUs;
    uint32 errorFlags;
} AiModel_ResultType;

typedef struct
{
    float32 voltage_V;
    float32 current_A;
    float32 temperature_C;
    uint8 signalValid;
    uint32 timestamp;
    uint32 cycleCounter;
} AiModel_Pdm1SnapshotType;

void AiModel_Init(void);
void AiModel_MainFunction(void);
Std_ReturnType AiModel_GetLatestResult(AiModel_ResultType *result);
Std_ReturnType AiModel_GetPdm1Snapshot(AiModel_Pdm1SnapshotType *snapshot);

#endif
