"""Small shared MLP intended to run once per monitored output."""
import torch
import torch.nn as nn
from io_spec import INPUT_SIZE,OUTPUT_SIZE
class BCMNet(nn.Module):
    def __init__(self):
        super().__init__(); self.net=nn.Sequential(nn.Linear(INPUT_SIZE,64),nn.ReLU(),nn.Linear(64,32),nn.ReLU(),nn.Linear(32,16),nn.ReLU(),nn.Linear(16,OUTPUT_SIZE))
    def forward(self,x):
        if x.ndim!=2 or x.shape[1]!=INPUT_SIZE: raise ValueError(f"Expected [batch, {INPUT_SIZE}]")
        return self.net(x)
