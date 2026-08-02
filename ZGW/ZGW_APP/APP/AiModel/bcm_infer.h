#ifndef BCM_INFER_H
#define BCM_INFER_H

#include <stdint.h>

#define BCM_MODEL_SCHEMA_VERSION 2u
#define BCM_SEQ_LEN 16u
#define BCM_INPUT_SIZE 48u
#define BCM_OUTPUT_SIZE 14u
#define BCM_NUM_FAULT_CLASSES 9u

#define BCM_FAULT_CLASS_UNKNOWN 0xFFu

typedef struct
{
    float expected_voltage_v;
    float expected_current_a;
    float expected_temperature_c;
    float anomaly;
    float health;
    uint8_t fault_class;
    float fault_probability[BCM_NUM_FAULT_CLASSES];
} BCM_InferenceResult;

void BCM_BuildInput(
    const float voltage_v[BCM_SEQ_LEN],
    const float current_a[BCM_SEQ_LEN],
    const float temperature_c[BCM_SEQ_LEN],
    float input[BCM_INPUT_SIZE]);

void BCM_Infer(
    const float input[BCM_INPUT_SIZE],
    float raw_output[BCM_OUTPUT_SIZE]);

void BCM_DecodeOutput(
    const float raw_output[BCM_OUTPUT_SIZE],
    BCM_InferenceResult *result);

#endif /* BCM_INFER_H */
