import numpy as np
import torch

from dataset import build_dataset, generate_sample
from io_spec import INPUT_SIZE, OUTPUT_FAULT_OFFSET, OUTPUT_SIZE
from model import BCMNet
from train import loss_function

x, y = generate_sample(123)
assert x.shape == (INPUT_SIZE,)
assert y.shape == (OUTPUT_SIZE,)
assert np.isfinite(x).all() and np.isfinite(y).all()
assert np.isclose(y[OUTPUT_FAULT_OFFSET:].sum(), 1.0)

xb, yb = build_dataset(64, seed=1000)
model = BCMNet()
pred = model(torch.from_numpy(xb))
assert pred.shape == (64, OUTPUT_SIZE)
loss = loss_function(pred, torch.from_numpy(yb))
assert torch.isfinite(loss)
loss.backward()
print(f"Smoke test passed: input={INPUT_SIZE}, output={OUTPUT_SIZE}, loss={loss.item():.6f}")
