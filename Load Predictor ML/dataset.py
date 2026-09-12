"""Synthetic per-consumer predictive-fault dataset.

This generator creates current precursors before the actual fault point. It is a
prototype training model, not evidence that real consumers expose these exact
precursors. Real vehicle data is required for production calibration.
"""

from __future__ import annotations

import numpy as np
import torch
from torch.utils.data import IterableDataset

from io_spec import (
    CHANNEL_RATINGS_A,
    CURRENT_MAX_A,
    FAULT_IMPENDING_INTERMITTENT,
    FAULT_IMPENDING_OPEN_LOAD,
    FAULT_IMPENDING_OVERCURRENT,
    FAULT_NORMAL,
    INPUT_SIZE,
    NUM_FAULT_CLASSES,
    OUTPUT_FAULT_OFFSET,
    OUTPUT_FAULT_SOON,
    OUTPUT_PREDICTED_CURRENT,
    OUTPUT_SIZE,
    PREDICTION_HORIZON_STEPS,
    SEQ_LEN,
    VOLTAGE_MAX_V,
    VOLTAGE_MIN_V,
    build_input,
    normalize_current,
)

FAULT_PROBABILITIES = np.array([0.50, 0.18, 0.16, 0.16], dtype=np.float64)
FAULT_PROBABILITIES /= FAULT_PROBABILITIES.sum()

# Increment this whenever the generated data distribution changes.  Training
# checkpoints record the version so an optimizer that has already converged on
# an older distribution (and may have a near-zero learning rate) is not reused.
DATASET_SCHEMA_VERSION = 2

# Half of the normal class deliberately reproduces LoadSim's healthy Driving
# scenario.  This makes 25% of the complete balanced stream hard negatives and
# teaches the classifier that high utilization alone is not an impending fault.
NORMAL_DRIVING_PROBABILITY = 0.50
LOADSIM_DRIVING_ACCESSORY_CHANNELS = frozenset((2, 7, 14, 25, 41, 58, 66))


def _voltage_trace(rng: np.random.Generator, total: int) -> np.ndarray:
    # Train over the complete 6..32 V input range so voltage is learned as context,
    # not as a consumer-output voltage.
    nominal = rng.uniform(10.5, 15.0)
    voltage = np.empty(total, dtype=np.float32)
    voltage[0] = np.clip(nominal + rng.normal(0.0, 0.05), VOLTAGE_MIN_V, VOLTAGE_MAX_V)
    target = nominal

    for k in range(1, total):
        if rng.random() < 0.01:
            # Include supply events (cranking/load-dump-like context) without
            # automatically labelling the consumer as faulty.
            target = rng.uniform(VOLTAGE_MIN_V, VOLTAGE_MAX_V)
        elif rng.random() < 0.03:
            target = nominal + rng.normal(0.0, 0.5)

        voltage[k] = np.clip(
            0.94 * voltage[k - 1] + 0.06 * target + rng.normal(0.0, 0.03),
            VOLTAGE_MIN_V,
            VOLTAGE_MAX_V,
        )

    return voltage


def _healthy_current_trace(rng: np.random.Generator, voltage: np.ndarray, rating_a: float) -> np.ndarray:
    total = voltage.size
    load_fraction = rng.uniform(0.10, 0.72)
    reference_current = load_fraction * rating_a
    load_type = int(rng.integers(0, 3))
    current = np.empty(total, dtype=np.float32)

    for k in range(total):
        v = max(float(voltage[k]), VOLTAGE_MIN_V)
        if load_type == 0:  # approximately resistive
            target = reference_current * (v / 13.5)
        elif load_type == 1:  # approximately constant power
            target = reference_current * (13.5 / v)
        else:  # approximately current regulated
            target = reference_current

        ripple = 0.015 * rating_a * np.sin(2.0 * np.pi * k / rng.uniform(14.0, 45.0))
        noise = rng.normal(0.0, max(0.02, 0.006 * rating_a))
        current[k] = np.clip(target + ripple + noise, 0.0, min(CURRENT_MAX_A, rating_a * 0.88))

    return current


def _smoothstep(x: np.ndarray) -> np.ndarray:
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3.0 - 2.0 * x)


def _loadsim_driving_trace(
    rng: np.random.Generator,
    total: int,
    channel: int,
    rating_a: float,
) -> tuple[np.ndarray, np.ndarray]:
    """Reproduce a healthy LoadSim Driving channel as a hard negative.

    The random time offset exposes training to every phase of a long Driving
    run.  Rounding mirrors the integer-valued PDM1 CAN payload received by the
    ECU rather than training only on unrealistically smooth floating-point data.
    """
    start_s = rng.uniform(0.0, 60.0)
    t = start_s + np.arange(total, dtype=np.float32) * 0.005

    voltage = 13.4 + 0.8 * _smoothstep(np.mod(t, 3.0) / 1.5)
    voltage += 0.25 * np.sin(2.0 * np.pi * 1.1 * t)

    base = rating_a * rng.uniform(0.58, 0.82)
    phase = rng.uniform(0.0, 2.0 * np.pi)
    frequency = rng.uniform(0.35, 2.5)
    load_scale = 1.0 + 0.20 * np.sin(2.0 * np.pi * 0.18 * t)
    ripple = 0.025 * rating_a * np.sin(2.0 * np.pi * frequency * t + phase)
    slow = 0.018 * rating_a * np.sin(2.0 * np.pi * 0.11 * t + phase * 0.37)
    current = np.clip(base * load_scale + ripple + slow, 0.0, min(CURRENT_MAX_A, rating_a * 0.88))

    if channel in LOADSIM_DRIVING_ACCESSORY_CHANNELS:
        accessory_on = np.isin((t * 1.2 + channel).astype(np.int32) % 4, (0, 1))
        current = np.where(
            accessory_on,
            np.minimum(current + 0.18 * rating_a, min(CURRENT_MAX_A, rating_a * 0.88)),
            current,
        )

    # LoadSim's packers round both signals before putting them on CAN.
    return np.rint(voltage).astype(np.float32), np.rint(current).astype(np.float32)


def _inject_precursor(current: np.ndarray, rating_a: float, fault: int, rng: np.random.Generator) -> np.ndarray:
    observed = current.copy()
    total = observed.size

    # Start the precursor inside the observed 1 s window and let the actual
    # failure occur inside the future prediction horizon.
    onset = int(rng.integers(SEQ_LEN // 2, SEQ_LEN - 20))
    future_end = min(total, SEQ_LEN + PREDICTION_HORIZON_STEPS)
    n = future_end - onset
    progress = np.linspace(0.0, 1.0, n, dtype=np.float32)

    if fault == FAULT_NORMAL:
        return observed

    if fault == FAULT_IMPENDING_OVERCURRENT:
        start = float(observed[onset])
        final = min(CURRENT_MAX_A, rating_a * rng.uniform(1.05, 1.35))
        curve = start + (final - start) * np.power(progress, rng.uniform(1.4, 2.4))
        observed[onset:future_end] = curve + rng.normal(0.0, max(0.02, 0.008 * rating_a), n)
        # Ensure prediction is genuinely early: the last observed point remains below rating.
        observed[:SEQ_LEN] = np.minimum(observed[:SEQ_LEN], rating_a * 0.97)

    elif fault == FAULT_IMPENDING_OPEN_LOAD:
        baseline = max(float(observed[onset]), 0.04 * rating_a)
        final = rng.uniform(0.0, 0.015) * rating_a
        curve = baseline + (final - baseline) * np.power(progress, rng.uniform(1.3, 2.0))
        observed[onset:future_end] = np.maximum(0.0, curve + rng.normal(0.0, max(0.01, 0.004 * rating_a), n))
        # Keep the channel visibly alive at the observation boundary.
        observed[SEQ_LEN - 1] = max(observed[SEQ_LEN - 1], 0.08 * rating_a)

    elif fault == FAULT_IMPENDING_INTERMITTENT:
        base = observed[onset:future_end].copy()
        probability = 0.02 + 0.68 * progress
        for j in range(n):
            if rng.random() < probability[j]:
                base[j] *= rng.uniform(0.0, max(0.03, 0.75 * (1.0 - progress[j])))
        observed[onset:future_end] = base
        observed[SEQ_LEN - 1] = max(observed[SEQ_LEN - 1], 0.05 * rating_a)

    np.clip(observed, 0.0, CURRENT_MAX_A, out=observed)
    return observed


def generate_sample(seed=None):
    rng = np.random.default_rng(seed)
    channel = int(rng.integers(0, CHANNEL_RATINGS_A.size))
    rating_a = float(CHANNEL_RATINGS_A[channel])
    total = SEQ_LEN + PREDICTION_HORIZON_STEPS + 1

    fault = int(rng.choice(NUM_FAULT_CLASSES, p=FAULT_PROBABILITIES))
    use_driving_negative = fault == FAULT_NORMAL and rng.random() < NORMAL_DRIVING_PROBABILITY

    if use_driving_negative:
        voltage, current = _loadsim_driving_trace(rng, total, channel, rating_a)
    else:
        voltage = _voltage_trace(rng, total)
        healthy_current = _healthy_current_trace(rng, voltage, rating_a)
        current = _inject_precursor(healthy_current, rating_a, fault, rng)

    x = build_input(voltage[:SEQ_LEN], current[:SEQ_LEN], rating_a)

    future_index = SEQ_LEN + PREDICTION_HORIZON_STEPS - 1
    future_current = float(current[future_index])
    fault_soon = 0.0 if fault == FAULT_NORMAL else 1.0

    y = np.zeros(OUTPUT_SIZE, dtype=np.float32)
    y[OUTPUT_PREDICTED_CURRENT] = float(normalize_current(future_current))
    y[OUTPUT_FAULT_SOON] = fault_soon
    y[OUTPUT_FAULT_OFFSET + fault] = 1.0

    assert x.shape == (INPUT_SIZE,)
    return x, y


class SyntheticDataset(IterableDataset):
    def __init__(self, seed=1234, samples_per_epoch=100000):
        super().__init__()
        self.seed = int(seed)
        self.samples_per_epoch = int(samples_per_epoch)

    def __iter__(self):
        worker_info = torch.utils.data.get_worker_info()
        worker_id = 0 if worker_info is None else worker_info.id
        num_workers = 1 if worker_info is None else worker_info.num_workers
        rng = np.random.default_rng(self.seed + worker_id * 1_000_003)

        for _ in range(worker_id, self.samples_per_epoch, num_workers):
            sample_seed = int(rng.integers(0, 2**31 - 1))
            x, y = generate_sample(sample_seed)
            yield torch.from_numpy(x), torch.from_numpy(y)


def build_dataset(n=50000, seed=None):
    seed = int(np.random.randint(0, 2**31 - 1)) if seed is None else int(seed)
    x = np.zeros((n, INPUT_SIZE), dtype=np.float32)
    y = np.zeros((n, OUTPUT_SIZE), dtype=np.float32)
    for index in range(n):
        x[index], y[index] = generate_sample(seed + index)
    return x, y
