# BCM predictive current model — schema v4

Purpose: predict an impending consumer fault from electrical-current behavior before the deterministic protection threshold is crossed.

## Runtime contract
- 75 output channels.
- Current and shared BCM input voltage sampled every 5 ms.
- 200 samples retained per channel: 1.0 s history.
- Intended inference scheduling target: every 20 ms (tune after TC375 CPU1 WCET measurement).
- Prediction horizon: 500 ms.
- Input voltage: 6..32 V. It is context only; there is no per-channel output-voltage input.
- Deterministic undervoltage: Vin < 9 V.
- Deterministic overvoltage: Vin > 15 V.
- Current: 0..250 A.
- Each channel has an example rating in CHANNEL_RATINGS_A; exactly one channel is 250 A.
- Temperature input removed.

## Feature-based inference
The neural network does not consume all 200 raw samples directly. `build_input()` / `BCM_BuildInput()` derive 34 compact features:
- current now;
- mean, max, min, RMS, variance, and least-squares slope over 25, 100, 500, and 1000 ms windows;
- maximum and minimum sample-to-sample dI;
- current/rating now, 100 ms mean/rating, and 1 s peak/rating;
- Vin now, Vin 1 s mean, Vin 1 s slope;
- channel rating.

The raw 5 ms histories are still retained so short electrical precursors are not discarded.

## Network
Shared MLP, executed once per channel:
`34 -> 64 -> 32 -> 16 -> 6`

Approximately 4,896 MACs/channel/inference, excluding feature extraction and nonlinear output decoding.
At 75 channels and 20 ms inference cadence this is about 18.36 MMAC/s for the dense layers.

Outputs:
1. predicted current at +500 ms;
2. probability that a fault is impending;
3. logits for normal / impending overcurrent / impending open-load / impending intermittent.

Actual UV/OV/OC protection remains deterministic in `BCM_EvaluateProtectionStatus()`; the neural network is not the safety/protection mechanism.

## Training
`dataset.py` creates synthetic electrical precursor trajectories. These are prototype assumptions, not evidence of real consumer failure behavior. Production usefulness requires real pre-failure vehicle measurements.

Run:
`python smoke_test.py`
`python train.py --forever`

Export after a valid schema-v4 `bcm.pth` exists:
`python export_c.py`
`python export.py`

## Start training from zero
Delete all generated/training-state artifacts, but keep the source `.py`, `.c`, `.h`, README and requirements files.

Delete if present:
- `bcm.pth`
- `bcm.onnx`
- `bcm.onnx.data`
- `bcm_model.h`
- entire `training_artifacts/` directory (especially `last_checkpoint.pth` and `training_state.json`)
- optional `__pycache__/` directories

After deletion, `train.py` starts with randomly initialized weights and no previous optimizer/checkpoint state.
