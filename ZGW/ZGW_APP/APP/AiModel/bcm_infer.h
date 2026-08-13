#ifndef BCM_INFER_H
#define BCM_INFER_H

#include <stdint.h>
#include "bcm_model.h"

typedef struct {
    float predicted_current_a;
    float fault_soon_probability;
    uint8_t fault_class;
    float fault_probability[BCM_NUM_FAULT_CLASSES];
} BCM_InferenceResult;

typedef struct {
    uint8_t undervoltage_now;
    uint8_t overvoltage_now;
    uint8_t overcurrent_now;
    float current_utilization;
} BCM_ProtectionStatus;

/* Histories are oldest -> newest, 200 samples at 5 ms = 1 s. */
void BCM_BuildInput(const float input_voltage_v[BCM_SEQ_LEN],
                    const float channel_current_a[BCM_SEQ_LEN],
                    float channel_current_rating_a,
                    float input[BCM_INPUT_SIZE]);
void BCM_Infer(const float input[BCM_INPUT_SIZE], float raw_output[BCM_OUTPUT_SIZE]);
void BCM_DecodeOutput(const float raw_output[BCM_OUTPUT_SIZE], BCM_InferenceResult *result);
void BCM_EvaluateProtectionStatus(float input_voltage_v, float channel_current_a,
                                  float channel_current_rating_a, BCM_ProtectionStatus *status);

#endif /* BCM_INFER_H */
