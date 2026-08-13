import numpy as np
import onnxruntime as ort

from io_spec import INPUT_SIZE, OUTPUT_SIZE

session = ort.InferenceSession("bcm.onnx", providers=["CPUExecutionProvider"])
x = np.zeros((1, INPUT_SIZE), dtype=np.float32)
y = session.run(None, {"consumer_history": x})[0]
assert y.shape == (1, OUTPUT_SIZE), y.shape
print("input:", x.shape)
print("output:", y.shape)
