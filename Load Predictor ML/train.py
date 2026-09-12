from __future__ import annotations

import argparse
import json
import os
import random
import sys
import time

import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader

from dataset import DATASET_SCHEMA_VERSION, SyntheticDataset
from io_spec import MODEL_SCHEMA_VERSION, OUTPUT_FAULT_OFFSET, OUTPUT_FAULT_SOON, OUTPUT_PREDICTED_CURRENT
from model import BCMNet

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

ARTIFACT_DIR = "training_artifacts"
LAST_CHECKPOINT = os.path.join(ARTIFACT_DIR, "last_checkpoint.pth")
BEST_MODEL = "bcm.pth"
STATE_JSON = os.path.join(ARTIFACT_DIR, "training_state.json")


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--forever", action="store_true")
    parser.add_argument("--samples", type=int, default=100000)
    parser.add_argument("--batch", type=int, default=1024)
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--max-epochs-per-dataset", type=int, default=300)
    parser.add_argument("--patience", type=int, default=35)
    parser.add_argument("--lr", type=float, default=1e-3)
    return parser.parse_args()


def setup_device():
    try:
        torch.set_float32_matmul_precision("high")
    except Exception:
        pass
    cpu_threads = max(1, (os.cpu_count() or 2) - 1)
    torch.set_num_threads(cpu_threads)
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print("Device:", device)
    print("CPU threads:", cpu_threads)
    if device == "cuda":
        print("GPU:", torch.cuda.get_device_name(0))
    return device


def make_loader(samples, batch_size, seed, workers, device):
    print(f"Streaming synthetic dataset: samples/epoch={samples}, seed={seed}, workers={workers}")
    return DataLoader(
        SyntheticDataset(seed=seed, samples_per_epoch=samples),
        batch_size=batch_size,
        shuffle=False,
        num_workers=workers,
        pin_memory=(device == "cuda"),
        persistent_workers=False,
        drop_last=False,
    )


def split_outputs(pred, target):
    pred_current = pred[:, OUTPUT_PREDICTED_CURRENT]
    pred_fault_soon = pred[:, OUTPUT_FAULT_SOON]
    pred_fault = pred[:, OUTPUT_FAULT_OFFSET:]

    target_current = target[:, OUTPUT_PREDICTED_CURRENT]
    target_fault_soon = target[:, OUTPUT_FAULT_SOON]
    target_fault = target[:, OUTPUT_FAULT_OFFSET:].argmax(dim=1)
    return pred_current, pred_fault_soon, pred_fault, target_current, target_fault_soon, target_fault


def loss_function(pred, target):
    pred_current, pred_fault_soon, pred_fault, target_current, target_fault_soon, target_fault = split_outputs(pred, target)
    current_loss = nn.functional.mse_loss(torch.sigmoid(pred_current), target_current)
    soon_loss = nn.functional.binary_cross_entropy_with_logits(pred_fault_soon, target_fault_soon)
    class_loss = nn.functional.cross_entropy(pred_fault, target_fault)
    return current_loss + 2.0 * soon_loss + 3.0 * class_loss


@torch.no_grad()
def validate(model, device, samples, batch_size, workers, seed):
    model.eval()
    val_samples = max(batch_size * 4, samples // 10)
    loader = make_loader(val_samples, batch_size, seed, workers, device)
    total_loss = 0.0
    batch_count = 0
    class_correct = 0
    soon_correct = 0
    total = 0

    for xb, yb in loader:
        xb = xb.to(device, non_blocking=True)
        yb = yb.to(device, non_blocking=True)
        pred = model(xb)
        total_loss += float(loss_function(pred, yb).item())
        batch_count += 1

        class_pred = pred[:, OUTPUT_FAULT_OFFSET:].argmax(dim=1)
        class_target = yb[:, OUTPUT_FAULT_OFFSET:].argmax(dim=1)
        soon_pred = torch.sigmoid(pred[:, OUTPUT_FAULT_SOON]) >= 0.5
        soon_target = yb[:, OUTPUT_FAULT_SOON] >= 0.5
        class_correct += int((class_pred == class_target).sum().item())
        soon_correct += int((soon_pred == soon_target).sum().item())
        total += int(xb.shape[0])

    return total_loss / max(1, batch_count), class_correct / max(1, total), soon_correct / max(1, total)


def save_state(state):
    os.makedirs(ARTIFACT_DIR, exist_ok=True)
    with open(STATE_JSON, "w", encoding="utf-8") as file:
        json.dump(state, file, indent=2)


def save_checkpoint(model, optimizer, scheduler, scaler, state):
    os.makedirs(ARTIFACT_DIR, exist_ok=True)
    torch.save(
        {
            "schema_version": MODEL_SCHEMA_VERSION,
            "dataset_schema_version": DATASET_SCHEMA_VERSION,
            "model": model.state_dict(),
            "optimizer": optimizer.state_dict(),
            "scheduler": scheduler.state_dict(),
            "scaler": scaler.state_dict() if scaler is not None else None,
            "state": state,
        },
        LAST_CHECKPOINT,
    )
    save_state(state)


def initial_state():
    return {"global_epoch": 0, "dataset_round": 0, "best_val_loss": float("inf"), "last_seed": None}


def load_checkpoint(model, optimizer, scheduler, scaler, device):
    if not os.path.exists(LAST_CHECKPOINT):
        return initial_state()
    checkpoint = torch.load(LAST_CHECKPOINT, map_location=device, weights_only=False)
    if checkpoint.get("schema_version") != MODEL_SCHEMA_VERSION:
        print("Ignoring incompatible checkpoint from an older BCM model schema.")
        return initial_state()
    if checkpoint.get("dataset_schema_version") != DATASET_SCHEMA_VERSION:
        print("Ignoring checkpoint trained on an older synthetic dataset distribution.")
        return initial_state()
    try:
        model.load_state_dict(checkpoint["model"])
        optimizer.load_state_dict(checkpoint["optimizer"])
        scheduler.load_state_dict(checkpoint["scheduler"])
        if scaler is not None and checkpoint.get("scaler") is not None:
            scaler.load_state_dict(checkpoint["scaler"])
    except (KeyError, RuntimeError, ValueError) as exc:
        print(f"Ignoring incompatible checkpoint: {exc}")
        return initial_state()
    state = checkpoint.get("state", initial_state())
    for key, value in initial_state().items():
        state.setdefault(key, value)
    print("Resumed from:", LAST_CHECKPOINT)
    return state


def save_best_model(model):
    torch.save({"schema_version": MODEL_SCHEMA_VERSION, "state_dict": model.state_dict()}, BEST_MODEL)


def main():
    args = parse_args()
    device = setup_device()
    os.makedirs(ARTIFACT_DIR, exist_ok=True)

    model = BCMNet().to(device)
    optimizer = optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-5)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, mode="min", factor=0.5, patience=8)
    scaler = torch.amp.GradScaler("cuda", enabled=(device == "cuda"))
    state = load_checkpoint(model, optimizer, scheduler, scaler, device)

    while True:
        dataset_seed = random.randint(0, 2**31 - 1)
        state["last_seed"] = dataset_seed
        state["dataset_round"] += 1
        train_loader = make_loader(args.samples, args.batch, dataset_seed, args.workers, device)
        validation_seed = dataset_seed + 99991
        no_improve = 0

        for epoch_in_dataset in range(args.max_epochs_per_dataset):
            model.train()
            epoch_loss = 0.0
            batch_count = 0
            started = time.time()

            for xb, yb in train_loader:
                xb = xb.to(device, non_blocking=True)
                yb = yb.to(device, non_blocking=True)
                optimizer.zero_grad(set_to_none=True)
                with torch.amp.autocast("cuda", enabled=(device == "cuda")):
                    pred = model(xb)
                    loss = loss_function(pred, yb)
                scaler.scale(loss).backward()
                scaler.step(optimizer)
                scaler.update()
                epoch_loss += float(loss.item())
                batch_count += 1

            train_loss = epoch_loss / max(1, batch_count)
            val_loss, val_class_accuracy, val_soon_accuracy = validate(
                model, device, args.samples, args.batch, max(0, min(args.workers, 2)), validation_seed
            )
            scheduler.step(val_loss)

            state["global_epoch"] += 1
            state["last_train_loss"] = train_loss
            state["last_val_loss"] = val_loss
            state["last_val_class_accuracy"] = val_class_accuracy
            state["last_val_soon_accuracy"] = val_soon_accuracy
            state["lr"] = optimizer.param_groups[0]["lr"]
            improved = val_loss < state["best_val_loss"]

            if improved:
                state["best_val_loss"] = val_loss
                save_best_model(model)
                no_improve = 0
                best_text = "BEST"
            else:
                no_improve += 1
                best_text = f"no_improve={no_improve}/{args.patience}"

            save_checkpoint(model, optimizer, scheduler, scaler, state)
            print(
                f"round={state['dataset_round']} global_epoch={state['global_epoch']} "
                f"epoch={epoch_in_dataset} train={train_loss:.6f} val={val_loss:.6f} "
                f"class_acc={val_class_accuracy:.4f} soon_acc={val_soon_accuracy:.4f} "
                f"best={state['best_val_loss']:.6f} lr={state['lr']:.8f} "
                f"time={time.time() - started:.1f}s {best_text}",
                flush=True,
            )

            if no_improve >= args.patience:
                print("Validation plateau reached. Regenerating synthetic dataset.", flush=True)
                break

        if not args.forever:
            break


if __name__ == "__main__":
    main()
