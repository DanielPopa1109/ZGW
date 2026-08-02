#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "bcm_infer.h"
#include "bcm_model.h"

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define BCM_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#define BCM_STATIC_ASSERT_CONCAT_INNER(a, b) a##b
#define BCM_STATIC_ASSERT_CONCAT(a, b) BCM_STATIC_ASSERT_CONCAT_INNER(a, b)
#define BCM_STATIC_ASSERT(cond, msg) typedef char BCM_STATIC_ASSERT_CONCAT(bcm_static_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

BCM_STATIC_ASSERT(BCM_MODEL_SCHEMA_VERSION == 2u, "Unsupported BCM model schema");
BCM_STATIC_ASSERT(BCM_INPUT_SIZE == 48u, "Unexpected BCM input size");
BCM_STATIC_ASSERT(BCM_OUTPUT_SIZE == 14u, "Unexpected BCM output size");
BCM_STATIC_ASSERT(BCM_SEQ_LEN == 16u, "Unexpected BCM sequence length");

static uint8_t BCM_IsFinite(float value)
{
    return ((value == value) && (value <= 3.402823466e+38f) && (value >= -3.402823466e+38f)) ? 1u : 0u;
}

static float BCM_Nan(void)
{
    volatile float zero = 0.0f;
    return zero / zero;
}

static void BCM_SetInvalidOutput(float raw_output[BCM_OUTPUT_SIZE])
{
    uint16_t index;

    if (raw_output == (float *)0)
    {
        return;
    }

    for (index = 0u; index < BCM_OUTPUT_SIZE; index++)
    {
        raw_output[index] = BCM_Nan();
    }
}

static float BCM_Relu(float value)
{
    return (value > 0.0f) ? value : 0.0f;
}

static float BCM_Sigmoid(float value)
{
    if (value >= 0.0f)
    {
        const float z = expf(-value);
        return 1.0f / (1.0f + z);
    }

    const float z = expf(value);
    return z / (1.0f + z);
}

static void BCM_Dense(
    const float *input,
    const float *weight,
    const float *bias,
    uint16_t input_size,
    uint16_t output_size,
    float *output)
{
    for (uint16_t row = 0u; row < output_size; ++row)
    {
        float accumulator = bias[row];
        const float *row_weight = &weight[(size_t)row * input_size];
        for (uint16_t column = 0u; column < input_size; ++column)
        {
            accumulator += input[column] * row_weight[column];
        }
        output[row] = accumulator;
    }
}

static float BCM_Normalize(float value, float minimum, float maximum)
{
    float normalized = (value - minimum) / (maximum - minimum);
    if (normalized < 0.0f)
    {
        normalized = 0.0f;
    }
    else if (normalized > 1.0f)
    {
        normalized = 1.0f;
    }
    return normalized;
}

static uint8_t BCM_ArrayIsFinite(const float *values, uint16_t count)
{
    uint16_t index;

    for (index = 0u; index < count; index++)
    {
        if (BCM_IsFinite(values[index]) == 0u)
        {
            return 0u;
        }
    }

    return 1u;
}

void BCM_BuildInput(
    const float voltage_v[BCM_SEQ_LEN],
    const float current_a[BCM_SEQ_LEN],
    const float temperature_c[BCM_SEQ_LEN],
    float input[BCM_INPUT_SIZE])
{
    for (uint16_t sample = 0u; sample < BCM_SEQ_LEN; ++sample)
    {
        const uint16_t base = (uint16_t)(sample * 3u);
        input[base] = BCM_Normalize(voltage_v[sample], BCM_VOLTAGE_MIN_V, BCM_VOLTAGE_MAX_V);
        input[base + 1u] = BCM_Normalize(current_a[sample], BCM_CURRENT_MIN_A, BCM_CURRENT_MAX_A);
        input[base + 2u] = BCM_Normalize(temperature_c[sample], BCM_TEMPERATURE_MIN_C, BCM_TEMPERATURE_MAX_C);
    }
}

void BCM_Infer(const float input[BCM_INPUT_SIZE], float raw_output[BCM_OUTPUT_SIZE])
{
    float h1[BCM_H1_SIZE];
    float h2[BCM_H2_SIZE];
    float h3[BCM_H3_SIZE];
    uint16_t index;

    if ((input == (const float *)0) || (raw_output == (float *)0) ||
            (BCM_ArrayIsFinite(input, BCM_INPUT_SIZE) == 0u))
    {
        BCM_SetInvalidOutput(raw_output);
        return;
    }

    BCM_Dense(input, bcm_net_0_weight, bcm_net_0_bias, BCM_INPUT_SIZE, BCM_H1_SIZE, h1);
    for (index = 0u; index < BCM_H1_SIZE; ++index)
    {
        h1[index] = BCM_Relu(h1[index]);
    }
    if (BCM_ArrayIsFinite(h1, BCM_H1_SIZE) == 0u) { BCM_SetInvalidOutput(raw_output); return; }

    BCM_Dense(h1, bcm_net_2_weight, bcm_net_2_bias, BCM_H1_SIZE, BCM_H2_SIZE, h2);
    for (index = 0u; index < BCM_H2_SIZE; ++index)
    {
        h2[index] = BCM_Relu(h2[index]);
    }
    if (BCM_ArrayIsFinite(h2, BCM_H2_SIZE) == 0u) { BCM_SetInvalidOutput(raw_output); return; }

    BCM_Dense(h2, bcm_net_4_weight, bcm_net_4_bias, BCM_H2_SIZE, BCM_H3_SIZE, h3);
    for (index = 0u; index < BCM_H3_SIZE; ++index)
    {
        h3[index] = BCM_Relu(h3[index]);
    }
    if (BCM_ArrayIsFinite(h3, BCM_H3_SIZE) == 0u) { BCM_SetInvalidOutput(raw_output); return; }

    BCM_Dense(h3, bcm_net_6_weight, bcm_net_6_bias, BCM_H3_SIZE, BCM_OUTPUT_SIZE, raw_output);
    if (BCM_ArrayIsFinite(raw_output, BCM_OUTPUT_SIZE) == 0u)
    {
        BCM_SetInvalidOutput(raw_output);
    }
}

void BCM_DecodeOutput(const float raw_output[BCM_OUTPUT_SIZE], BCM_InferenceResult *result)
{
    float maximum_logit = raw_output[BCM_OUTPUT_FAULT_OFFSET];
    float denominator = 0.0f;
    uint8_t maximum_index = 0u;

    if ((raw_output == (const float *)0) || (result == (BCM_InferenceResult *)0) ||
            (BCM_ArrayIsFinite(raw_output, BCM_OUTPUT_SIZE) == 0u))
    {
        return;
    }

    result->expected_voltage_v = BCM_VOLTAGE_MIN_V + BCM_Sigmoid(raw_output[BCM_OUTPUT_EXPECTED_VOLTAGE]) * (BCM_VOLTAGE_MAX_V - BCM_VOLTAGE_MIN_V);
    result->expected_current_a = BCM_CURRENT_MIN_A + BCM_Sigmoid(raw_output[BCM_OUTPUT_EXPECTED_CURRENT]) * (BCM_CURRENT_MAX_A - BCM_CURRENT_MIN_A);
    result->expected_temperature_c = BCM_TEMPERATURE_MIN_C + BCM_Sigmoid(raw_output[BCM_OUTPUT_EXPECTED_TEMPERATURE]) * (BCM_TEMPERATURE_MAX_C - BCM_TEMPERATURE_MIN_C);
    result->anomaly = BCM_Sigmoid(raw_output[BCM_OUTPUT_ANOMALY]);
    result->health = BCM_Sigmoid(raw_output[BCM_OUTPUT_HEALTH]);

    for (uint8_t fault = 1u; fault < BCM_NUM_FAULT_CLASSES; ++fault)
    {
        const float logit = raw_output[BCM_OUTPUT_FAULT_OFFSET + fault];
        if (logit > maximum_logit)
        {
            maximum_logit = logit;
            maximum_index = fault;
        }
    }

    for (uint8_t fault = 0u; fault < BCM_NUM_FAULT_CLASSES; ++fault)
    {
        const float probability = expf(raw_output[BCM_OUTPUT_FAULT_OFFSET + fault] - maximum_logit);
        result->fault_probability[fault] = probability;
        denominator += probability;
    }

    for (uint8_t fault = 0u; fault < BCM_NUM_FAULT_CLASSES; ++fault)
    {
        result->fault_probability[fault] /= denominator;
    }
    result->fault_class = maximum_index;
}
