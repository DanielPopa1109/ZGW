from __future__ import annotations

import os
import torch

from io_spec import INPUT_SIZE, MODEL_SCHEMA_VERSION, OUTPUT_SIZE
from model import BCMNet


def load_best_model(path="bcm.pth"):
    if not os.path.exists(path):
        raise FileNotFoundError("bcm.pth does not exist. Train schema-v3 first.")
    payload = torch.load(path, map_location="cpu", weights_only=False)
    if not isinstance(payload, dict) or payload.get("schema_version") != MODEL_SCHEMA_VERSION:
        raise RuntimeError("bcm.pth belongs to an incompatible model schema. Retrain first.")
    model = BCMNet()
    model.load_state_dict(payload["state_dict"])
    model.eval()
    return model


def main():
    model = load_best_model()
    dummy = torch.zeros(1, INPUT_SIZE, dtype=torch.float32)
    torch.onnx.export(
        model,
        dummy,
        "bcm.onnx",
        input_names=["consumer_history"],
        output_names=["raw_output"],
        opset_version=18,
        do_constant_folding=True,
    )
    with torch.no_grad():
        assert model(dummy).shape == (1, OUTPUT_SIZE)
    print(f"ONNX exported: input={INPUT_SIZE}, output={OUTPUT_SIZE}")


if __name__ == "__main__":
    main()
