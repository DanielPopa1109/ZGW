import os
import queue
import signal
import subprocess
import sys
import threading
import tkinter as tk
from tkinter import messagebox, scrolledtext

PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
PYTHON_EXE = sys.executable
if PYTHON_EXE.lower().endswith("pythonw.exe"):
    PYTHON_EXE = PYTHON_EXE[:-11] + "python.exe"

log_queue = queue.Queue()
process_lock = threading.Lock()
current_process = None


def log(message):
    log_queue.put(str(message) + "\n")


def set_status(message):
    log_queue.put(("__STATUS__", message))


def run_cmd(arguments):
    global current_process
    log(f">>> {' '.join(arguments)}")
    creationflags = subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0
    process = subprocess.Popen(
        arguments,
        cwd=PROJECT_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="ignore",
        creationflags=creationflags,
    )
    with process_lock:
        current_process = process
    try:
        for line in process.stdout:
            log(line.rstrip())
        process.wait()
        log(f"Exit code: {process.returncode}\n")
        if process.returncode != 0:
            raise RuntimeError(f"Command failed: {' '.join(arguments)}")
    finally:
        with process_lock:
            if current_process is process:
                current_process = None


def train_forever_pipeline():
    try:
        set_status("Training")
        run_cmd([
            PYTHON_EXE, "train.py", "--forever",
            "--samples", samples_var.get(),
            "--batch", batch_var.get(),
            "--workers", workers_var.get(),
            "--max-epochs-per-dataset", epochs_var.get(),
            "--patience", patience_var.get(),
        ])
    except Exception as exc:
        log(f"TRAINING STOPPED/FAILED: {exc}")
    finally:
        set_status("Idle")


def export_pipeline():
    try:
        set_status("Exporting")
        run_cmd([PYTHON_EXE, "export_c.py"])
        run_cmd([PYTHON_EXE, "export.py"])
        log("EXPORT COMPLETE")
    except Exception as exc:
        log(f"EXPORT FAILED: {exc}")
    finally:
        set_status("Idle")


def start_training():
    with process_lock:
        if current_process is not None:
            messagebox.showwarning("Busy", "A process is already running.")
            return
    threading.Thread(target=train_forever_pipeline, daemon=True).start()


def start_export():
    with process_lock:
        if current_process is not None:
            messagebox.showwarning("Busy", "A process is already running.")
            return
    threading.Thread(target=export_pipeline, daemon=True).start()


def stop_process():
    with process_lock:
        process = current_process
    if process is None:
        log("No process running.")
        return
    log("Stopping process. The latest compatible checkpoint is retained.")
    try:
        if os.name == "nt":
            process.send_signal(signal.CTRL_BREAK_EVENT)
        else:
            process.terminate()
    except Exception:
        process.kill()


def update_log():
    while not log_queue.empty():
        item = log_queue.get()
        if isinstance(item, tuple) and item[0] == "__STATUS__":
            status_var.set(f"Status: {item[1]}")
        else:
            text.insert(tk.END, item)
            text.see(tk.END)
    root.after(100, update_log)


root = tk.Tk()
root.title("Per-Consumer Predictive BCM ML Trainer")
status_var = tk.StringVar(value="Status: Idle")
samples_var = tk.StringVar(value="200000")
batch_var = tk.StringVar(value="1024")
workers_var = tk.StringVar(value="4")
epochs_var = tk.StringVar(value="300")
patience_var = tk.StringVar(value="35")

tk.Label(root, textvariable=status_var).pack(anchor="w", padx=10, pady=(8, 0))
config = tk.Frame(root)
config.pack(anchor="w", padx=10, pady=8)
for column, (label, variable, width) in enumerate([
    ("Samples/dataset", samples_var, 10),
    ("Batch", batch_var, 6),
    ("Workers", workers_var, 4),
    ("Max epochs/dataset", epochs_var, 6),
    ("Plateau patience", patience_var, 6),
]):
    tk.Label(config, text=label).grid(row=0, column=column, padx=4, sticky="w")
    tk.Entry(config, textvariable=variable, width=width).grid(row=1, column=column, padx=4, sticky="w")

buttons = tk.Frame(root)
buttons.pack(anchor="w", padx=10, pady=5)
tk.Button(buttons, text="Start Unattended Training", command=start_training, width=24).pack(side=tk.LEFT, padx=4)
tk.Button(buttons, text="Stop", command=stop_process, width=10).pack(side=tk.LEFT, padx=4)
tk.Button(buttons, text="Export Best Model", command=start_export, width=18).pack(side=tk.LEFT, padx=4)
text = scrolledtext.ScrolledText(root, width=130, height=36)
text.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
log("Ready. Schema v3: 5 ms cycle, 1 s history, per-channel current prediction, shared input-voltage context.")
log("Old schema-v2 checkpoints/models are intentionally incompatible and must not be reused.")
root.after(100, update_log)
root.mainloop()
