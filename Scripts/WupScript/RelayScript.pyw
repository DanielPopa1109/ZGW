# -*- coding: utf-8 -*-
"""
WUP-only USB relay controller.

Inspired by the attached TSMaster C scripts, but intentionally standalone:
- no CAN
- no NM3
- no ECU response wait
- only WUP relay actuation on the selected USB relay COM port

Required package:
    pip install pyserial
"""

import sys
import time
import threading
import traceback
import tkinter as tk
from tkinter import ttk, messagebox

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None


# -----------------------------------------------------------------------------
# Configuration matching the attached scripts
# -----------------------------------------------------------------------------

RELAY_COM = "COM8"
RELAY_BAUDRATE = 9600
RELAY_BYTESIZE = 8
RELAY_PARITY = "N"
RELAY_STOPBITS = 1
RELAY_TIMEOUT_S = 0.010
RELAY_WRITE_TIMEOUT_S = 0.010

RELAY_CMD_ON = bytes([0xA0, 0x01, 0x01, 0xA2])
RELAY_CMD_OFF = bytes([0xA0, 0x01, 0x00, 0xA1])

RELAY_ON_TIME_MS = 500
POST_WUP_WAIT_MS = 1000
T_DELAY_START_MS = 0
T_DELAY_DEFAULT_END_MS = 3740
T_DELAY_STEP_MS = 1

STATE_IDLE = 0
STATE_ONGOING = 1
STATE_FINISHED = 2

ERR_NONE = 0
ERR_RELAY_OPEN = 1
ERR_RELAY_IO = 2
ERR_ABORTED = 3
ERR_EXCEPTION = 4
ERR_CONFIG = 5


class UsbRelay:
    def __init__(self, port: str):
        self.port = port
        self.ser = None

    def open(self) -> None:
        if serial is None:
            raise RuntimeError("pyserial is not installed. Run: pip install pyserial")

        self.close()
        self.ser = serial.Serial(
            port=self.port,
            baudrate=RELAY_BAUDRATE,
            bytesize=RELAY_BYTESIZE,
            parity=RELAY_PARITY,
            stopbits=RELAY_STOPBITS,
            timeout=RELAY_TIMEOUT_S,
            write_timeout=RELAY_WRITE_TIMEOUT_S,
            xonxoff=False,
            rtscts=False,
            dsrdtr=False,
        )

        self.ser.setRTS(False)
        self.ser.setDTR(False)
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()

    def close(self) -> None:
        if self.ser is not None:
            try:
                self.ser.close()
            finally:
                self.ser = None

    def write_raw(self, data: bytes) -> None:
        if self.ser is None or not self.ser.is_open:
            raise RuntimeError("Relay COM port is not open")

        written = self.ser.write(data)
        self.ser.flush()

        if written != len(data):
            raise RuntimeError(f"Relay write failed: wrote {written}/{len(data)} bytes")

    def on(self) -> None:
        self.write_raw(RELAY_CMD_ON)

    def off(self) -> None:
        self.write_raw(RELAY_CMD_OFF)


class WupRelayApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("WUP USB Relay Test")
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        self.root.resizable(True, True)

        self.relay = UsbRelay(RELAY_COM)
        self.worker = None
        self.worker_once = False
        self.active_tdelay_max_ms = T_DELAY_DEFAULT_END_MS
        self.stop_event = threading.Event()
        self.lock = threading.Lock()

        self.wup_state = STATE_IDLE
        self.wup_stat = 0
        self.error = ERR_NONE
        self.current_t_delay_ms = T_DELAY_START_MS
        self.cycle_count = 0

        self.build_ui()
        self.update_ui_periodic()

    def build_ui(self) -> None:
        main = ttk.Frame(self.root, padding=10)
        main.grid(row=0, column=0, sticky="nsew")
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)

        status_frame = ttk.LabelFrame(main, text="WUP test only", padding=10)
        status_frame.grid(row=0, column=0, sticky="ew")
        status_frame.columnconfigure(1, weight=1)

        ttk.Label(status_frame, text="Relay COM:").grid(row=0, column=0, sticky="w")
        self.com_var = tk.StringVar(value=RELAY_COM)
        self.com_combo = ttk.Combobox(status_frame, textvariable=self.com_var, values=self.get_available_ports())
        self.com_combo.grid(row=0, column=1, sticky="ew", padx=(0, 5))
        self.refresh_com_button = ttk.Button(status_frame, text="Refresh", command=self.refresh_com_ports)
        self.refresh_com_button.grid(row=0, column=2, sticky="ew")

        ttk.Label(status_frame, text="tDelay_Max [ms]:").grid(row=1, column=0, sticky="w")
        self.tdelay_max_var = tk.StringVar(value=str(T_DELAY_DEFAULT_END_MS))
        self.tdelay_max_entry = ttk.Entry(status_frame, textvariable=self.tdelay_max_var)
        self.tdelay_max_entry.grid(row=1, column=1, sticky="ew", padx=(0, 5))

        ttk.Label(status_frame, text="CWP_WUP_STATE:").grid(row=2, column=0, sticky="w")
        self.state_var = tk.StringVar(value="0 = idle")
        ttk.Label(status_frame, textvariable=self.state_var).grid(row=2, column=1, sticky="w")

        ttk.Label(status_frame, text="WUP_STAT:").grid(row=3, column=0, sticky="w")
        self.wup_stat_var = tk.StringVar(value="0 = relay off")
        ttk.Label(status_frame, textvariable=self.wup_stat_var).grid(row=3, column=1, sticky="w")

        ttk.Label(status_frame, text="Error:").grid(row=4, column=0, sticky="w")
        self.error_var = tk.StringVar(value="0 = OK")
        ttk.Label(status_frame, textvariable=self.error_var).grid(row=4, column=1, sticky="w")

        ttk.Label(status_frame, text="Current tDelay:").grid(row=5, column=0, sticky="w")
        self.tdelay_var = tk.StringVar(value="0 ms")
        ttk.Label(status_frame, textvariable=self.tdelay_var).grid(row=5, column=1, sticky="w")

        ttk.Label(status_frame, text="Cycles done:").grid(row=6, column=0, sticky="w")
        self.cycle_var = tk.StringVar(value="0")
        ttk.Label(status_frame, textvariable=self.cycle_var).grid(row=6, column=1, sticky="w")

        button_frame = ttk.Frame(main)
        button_frame.grid(row=1, column=0, sticky="ew", pady=(10, 10))
        button_frame.columnconfigure(0, weight=1)
        button_frame.columnconfigure(1, weight=1)
        button_frame.columnconfigure(2, weight=1)

        self.start_button = ttk.Button(button_frame, text="Start WUP", command=self.start_wup)
        self.start_button.grid(row=0, column=0, sticky="ew", padx=(0, 5))

        self.once_button = ttk.Button(button_frame, text="WUP Once", command=self.start_wup_once)
        self.once_button.grid(row=0, column=1, sticky="ew", padx=5)

        self.stop_button = ttk.Button(button_frame, text="Stop", command=self.stop_wup, state="disabled")
        self.stop_button.grid(row=0, column=2, sticky="ew", padx=(5, 0))

        log_frame = ttk.LabelFrame(main, text="Log", padding=10)
        log_frame.grid(row=2, column=0, sticky="nsew")
        main.rowconfigure(2, weight=1)
        main.columnconfigure(0, weight=1)

        self.log_text = tk.Text(log_frame, height=14, width=90, wrap="word")
        self.log_text.grid(row=0, column=0, sticky="nsew")
        log_scroll = ttk.Scrollbar(log_frame, orient="vertical", command=self.log_text.yview)
        log_scroll.grid(row=0, column=1, sticky="ns")
        self.log_text.configure(yscrollcommand=log_scroll.set)
        log_frame.rowconfigure(0, weight=1)
        log_frame.columnconfigure(0, weight=1)

    def set_config_inputs_state(self, state: str) -> None:
        self.com_combo.configure(state=state)
        self.refresh_com_button.configure(state=state)
        self.tdelay_max_entry.configure(state=state)

    def set_state(self, state: int) -> None:
        with self.lock:
            self.wup_state = state

    def set_wup_stat(self, value: int) -> None:
        with self.lock:
            self.wup_stat = value

    def set_error(self, value: int) -> None:
        with self.lock:
            self.error = value

    def set_tdelay(self, value_ms: int) -> None:
        with self.lock:
            self.current_t_delay_ms = value_ms

    def set_cycle_count(self, value: int) -> None:
        with self.lock:
            self.cycle_count = value

    def log(self, text: str) -> None:
        timestamp = time.strftime("%H:%M:%S")
        line = f"[{timestamp}] {text}\n"
        self.root.after(0, self._append_log, line)

    def _append_log(self, line: str) -> None:
        self.log_text.insert("end", line)
        self.log_text.see("end")

    def get_available_ports(self) -> list[str]:
        if list_ports is None:
            return [RELAY_COM]

        ports = [port.device for port in list_ports.comports()]
        if RELAY_COM not in ports:
            ports.insert(0, RELAY_COM)
        return ports

    def refresh_com_ports(self) -> None:
        self.com_combo.configure(values=self.get_available_ports())
        self.log("COM port list refreshed")

    def get_selected_com_port(self) -> str:
        port = self.com_var.get().strip()

        if not port:
            raise ValueError("COM port is empty")

        if port.isdigit():
            port = f"COM{port}"

        return port

    def parse_non_negative_int(self, text: str, name: str) -> int:
        text = text.strip()
        if not text:
            raise ValueError(f"{name} is empty")

        try:
            value = int(text, 10)
        except ValueError as exc:
            raise ValueError(f"{name} must be an integer") from exc

        if value < 0:
            raise ValueError(f"{name} must be >= 0")

        return value

    def get_tdelay_max_ms(self) -> int:
        tdelay_max_ms = self.parse_non_negative_int(self.tdelay_max_var.get(), "tDelay_Max")
        if tdelay_max_ms < T_DELAY_START_MS:
            raise ValueError("tDelay_Max must be >= T_DELAY_START_MS")
        return tdelay_max_ms

    def start_wup(self) -> None:
        self.start_worker(once=False)

    def start_wup_once(self) -> None:
        self.start_worker(once=True)

    def start_worker(self, once: bool) -> None:
        if self.worker is not None and self.worker.is_alive():
            return

        try:
            selected_port = self.get_selected_com_port()
            tdelay_max_ms = self.get_tdelay_max_ms()
        except ValueError as exc:
            self.set_error(ERR_CONFIG)
            self.log(str(exc))
            messagebox.showerror("Invalid configuration", str(exc))
            return

        self.relay.port = selected_port
        self.com_var.set(selected_port)
        self.active_tdelay_max_ms = tdelay_max_ms

        self.worker_once = once
        self.stop_event.clear()
        self.set_error(ERR_NONE)
        self.set_state(STATE_ONGOING)
        self.set_wup_stat(0)
        self.set_tdelay(T_DELAY_START_MS)
        self.set_cycle_count(0)

        self.worker = threading.Thread(target=self.wup_worker, name="WupRelayWorker", daemon=True)
        self.worker.start()

        self.set_config_inputs_state("disabled")
        self.start_button.configure(state="disabled")
        self.once_button.configure(state="disabled")
        self.stop_button.configure(state="normal")

        if once:
            self.log(f"Single WUP trigger started on {self.relay.port}")
        else:
            self.log(f"WUP test started on {self.relay.port}; tDelay_Max={tdelay_max_ms} ms")

    def stop_wup(self) -> None:
        self.stop_event.set()
        self.log("Stop requested")

    def relay_safe_off_and_close(self) -> None:
        try:
            try:
                if self.relay.ser is not None and self.relay.ser.is_open:
                    self.relay.off()
            finally:
                self.relay.close()
        finally:
            self.set_wup_stat(0)

    def sleep_interruptible_ms(self, duration_ms: int) -> bool:
        end_time = time.monotonic() + (duration_ms / 1000.0)
        while time.monotonic() < end_time:
            if self.stop_event.is_set():
                return False
            remaining_s = end_time - time.monotonic()
            time.sleep(min(0.001, max(0.0, remaining_s)))
        return True

    def execute_one_wup_action(self) -> bool:
        try:
            self.relay.open()
        except Exception as exc:
            self.set_error(ERR_RELAY_OPEN)
            self.log(f"Relay COM open failed on {self.relay.port}: {exc}")
            return False

        try:
            self.relay.on()
            self.set_wup_stat(1)
            self.log("Relay ON")

            if not self.sleep_interruptible_ms(RELAY_ON_TIME_MS):
                self.set_error(ERR_ABORTED)
                return False

            self.relay.off()
            self.set_wup_stat(0)
            self.log("Relay OFF")
            return True

        except Exception as exc:
            self.set_error(ERR_RELAY_IO)
            self.log(f"Relay I/O failed: {exc}")
            return False

        finally:
            self.relay.close()
            self.set_wup_stat(0)

    def run_should_finish(self, t_delay_ms: int) -> bool:
        if self.worker_once:
            return True
        return t_delay_ms >= self.active_tdelay_max_ms

    def wup_worker(self) -> None:
        finished = False
        cycle = 0

        try:
            t_delay_ms = T_DELAY_START_MS

            while not self.stop_event.is_set():
                self.set_tdelay(t_delay_ms)
                self.set_cycle_count(cycle + 1)

                ok = self.execute_one_wup_action()
                if not ok:
                    break

                cycle += 1
                self.set_cycle_count(cycle)

                if self.run_should_finish(t_delay_ms):
                    finished = True
                    break

                total_wait_ms = POST_WUP_WAIT_MS + t_delay_ms
                self.log(f"Wait {total_wait_ms} ms before next WUP cycle")

                if not self.sleep_interruptible_ms(total_wait_ms):
                    self.set_error(ERR_ABORTED)
                    break

                t_delay_ms += T_DELAY_STEP_MS

        except Exception:
            self.set_error(ERR_EXCEPTION)
            self.log("Unhandled exception:")
            self.log(traceback.format_exc())

        finally:
            self.relay_safe_off_and_close()

            if finished:
                self.set_state(STATE_FINISHED)
                self.log("WUP test finished")
            else:
                self.set_state(STATE_IDLE)
                if self.error == ERR_NONE:
                    self.set_error(ERR_ABORTED)
                self.log("WUP test stopped")

            self.worker_once = False
            self.root.after(0, self.on_worker_done)

    def on_worker_done(self) -> None:
        self.set_config_inputs_state("normal")
        self.start_button.configure(state="normal")
        self.once_button.configure(state="normal")
        self.stop_button.configure(state="disabled")

    def update_ui_periodic(self) -> None:
        with self.lock:
            state = self.wup_state
            wup_stat = self.wup_stat
            error = self.error
            tdelay = self.current_t_delay_ms
            cycles = self.cycle_count

        state_text = {
            STATE_IDLE: "0 = idle",
            STATE_ONGOING: "1 = ongoing",
            STATE_FINISHED: "2 = finished",
        }.get(state, f"{state} = unknown")

        wup_text = "1 = relay on" if wup_stat else "0 = relay off"

        error_text = {
            ERR_NONE: "0 = OK",
            ERR_RELAY_OPEN: "1 = COM open fail",
            ERR_RELAY_IO: "2 = relay I/O fail",
            ERR_ABORTED: "3 = aborted/stopped",
            ERR_EXCEPTION: "4 = exception",
            ERR_CONFIG: "5 = invalid config",
        }.get(error, f"{error} = unknown")

        self.state_var.set(state_text)
        self.wup_stat_var.set(wup_text)
        self.error_var.set(error_text)
        self.tdelay_var.set(f"{tdelay} ms")
        self.cycle_var.set(str(cycles))

        self.root.after(100, self.update_ui_periodic)

    def on_close(self) -> None:
        if self.worker is not None and self.worker.is_alive():
            self.stop_event.set()
            self.relay_safe_off_and_close()
        self.root.destroy()


def main() -> int:
    root = tk.Tk()

    if serial is None:
        messagebox.showerror(
            "Missing dependency",
            "pyserial is not installed.\n\nInstall it with:\n    pip install pyserial",
        )
        return 1

    WupRelayApp(root)
    root.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
