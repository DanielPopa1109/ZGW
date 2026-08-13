"""Export schema-v4 weights as float32 constants for AURIX TC3xx."""

from __future__ import annotations

import os
import numpy as np
import torch

from io_spec import *
from model import BCMNet

LAYER_SIZES = [INPUT_SIZE, 64, 32, 16, OUTPUT_SIZE]


def load_state(path="bcm.pth"):
    if not os.path.exists(path):
        raise FileNotFoundError("bcm.pth does not exist. Train schema-v4 first.")
    payload = torch.load(path, map_location="cpu", weights_only=False)
    if not isinstance(payload, dict) or payload.get("schema_version") != MODEL_SCHEMA_VERSION:
        raise RuntimeError("Incompatible bcm.pth. Retrain with schema-v4.")
    return payload["state_dict"]


def write_float_array(file, name, values):
    flat = np.asarray(values, dtype=np.float32).reshape(-1)
    file.write(f"static const float {name}[{len(flat)}] = {{\n")
    for index, value in enumerate(flat):
        file.write(f"{float(value):.9e}f")
        if index + 1 != len(flat):
            file.write(",")
        if (index + 1) % 8 == 0:
            file.write("\n")
    file.write("\n};\n\n")


def main():
    model = BCMNet()
    model.load_state_dict(load_state())
    model.eval()
    state = model.state_dict()

    with open("bcm_model.h", "w", encoding="utf-8", newline="\n") as file:
        file.write("#pragma once\n#include <stdint.h>\n\n")
        file.write(f"#define BCM_MODEL_SCHEMA_VERSION {MODEL_SCHEMA_VERSION}u\n")
        file.write(f"#define BCM_CYCLE_TIME_MS {CYCLE_TIME_MS}u\n")
        file.write(f"#define BCM_SEQ_LEN {SEQ_LEN}u\n")
        file.write(f"#define BCM_NUM_CHANNELS {NUM_CHANNELS}u\n")
        file.write(f"#define BCM_INFERENCE_PERIOD_MS {INFERENCE_PERIOD_MS}u\n")
        file.write(f"#define BCM_PREDICTION_HORIZON_MS {PREDICTION_HORIZON_MS}u\n")
        file.write(f"#define BCM_INPUT_SIZE {INPUT_SIZE}u\n")
        file.write(f"#define BCM_H1_SIZE {LAYER_SIZES[1]}u\n")
        file.write(f"#define BCM_H2_SIZE {LAYER_SIZES[2]}u\n")
        file.write(f"#define BCM_H3_SIZE {LAYER_SIZES[3]}u\n")
        file.write(f"#define BCM_OUTPUT_SIZE {OUTPUT_SIZE}u\n")
        file.write(f"#define BCM_NUM_FAULT_CLASSES {NUM_FAULT_CLASSES}u\n")
        file.write(f"#define BCM_OUTPUT_PREDICTED_CURRENT {OUTPUT_PREDICTED_CURRENT}u\n")
        file.write(f"#define BCM_OUTPUT_FAULT_SOON {OUTPUT_FAULT_SOON}u\n")
        file.write(f"#define BCM_OUTPUT_FAULT_OFFSET {OUTPUT_FAULT_OFFSET}u\n")
        file.write(f"#define BCM_VOLTAGE_MIN_V {VOLTAGE_MIN_V:.9e}f\n")
        file.write(f"#define BCM_VOLTAGE_MAX_V {VOLTAGE_MAX_V:.9e}f\n")
        file.write(f"#define BCM_UNDERVOLTAGE_THRESHOLD_V {UNDERVOLTAGE_THRESHOLD_V:.9e}f\n")
        file.write(f"#define BCM_OVERVOLTAGE_THRESHOLD_V {OVERVOLTAGE_THRESHOLD_V:.9e}f\n")
        file.write(f"#define BCM_CURRENT_MIN_A {CURRENT_MIN_A:.9e}f\n")
        file.write(f"#define BCM_CURRENT_MAX_A {CURRENT_MAX_A:.9e}f\n")
        file.write(f"#define BCM_CURRENT_RATING_MIN_A {CURRENT_RATING_MIN_A:.9e}f\n")
        file.write(f"#define BCM_CURRENT_RATING_MAX_A {CURRENT_RATING_MAX_A:.9e}f\n")
        file.write(f"#define BCM_CURRENT_RATIO_MAX {CURRENT_RATIO_MAX:.9e}f\n\n")

        for layer_index in (0, 2, 4, 6):
            write_float_array(file, f"bcm_net_{layer_index}_weight", state[f"net.{layer_index}.weight"].numpy())
            write_float_array(file, f"bcm_net_{layer_index}_bias", state[f"net.{layer_index}.bias"].numpy())

    print("Generated bcm_model.h with schema-v4 float32 TC3xx weights")


if __name__ == "__main__":
    main()
