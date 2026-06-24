# -*- coding: utf-8 -*-
"""
Vector CAN FD + USB relay tDelay sweep tool.

Purpose:
- Use Vector CANcase/CANcard/CANoe-compatible XL driver backend through python-can.
- Send one configurable CAN FD frame 10 times in 500 ms per cycle.
- Control the same USB relay used by the WUP script.
- No DBC input.

Install dependencies in the Python that runs this file:
    py -m pip install python-can pyserial

Typical sequence per cycle:
    relay ON
    send the configured CAN FD frame 10 times in 500 ms
    check for the configured ECU response within 500 ms from the first TX
    wait Relay_ON_Time_ms
    relay OFF
    wait Relay_OFF_Base_Time_ms + current_tDelay_ms
    current_tDelay_ms += 1

The tDelay sweep is inclusive:
    0, 1, 2, ... tDelay_Max_ms
"""

import sys
import re
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

try:
    import can
except ImportError:
    can = None


# -----------------------------------------------------------------------------
# Defaults
# -----------------------------------------------------------------------------

DEFAULT_RELAY_COM = "COM8"
DEFAULT_VECTOR_APP_NAME = "PythonCAN"
DEFAULT_VECTOR_CHANNEL = 0
DEFAULT_NOMINAL_BITRATE = 500000
DEFAULT_DATA_BITRATE = 2000000
DEFAULT_CAN_ID_HEX = "093"
DEFAULT_PAYLOAD_HEX = "00 00 00 00 00 00 00 00 00 00 00 00"
DEFAULT_RESPONSE_CAN_ID_HEX = "6EF"
DEFAULT_RESPONSE_PAYLOAD_HEX = ""
DEFAULT_EXTENDED_ID = False
DEFAULT_RESPONSE_EXTENDED_ID = False
DEFAULT_BRS = True
DEFAULT_PAD_TO_VALID_FD_DLC = True
DEFAULT_RESPONSE_PAD_TO_VALID_FD_DLC = True

RELAY_BAUDRATE = 9600
RELAY_BYTESIZE = 8
RELAY_PARITY = "N"
RELAY_STOPBITS = 1
RELAY_TIMEOUT_S = 0.010
RELAY_WRITE_TIMEOUT_S = 0.010

RELAY_CMD_ON = bytes([0xA0, 0x01, 0x01, 0xA2])
RELAY_CMD_OFF = bytes([0xA0, 0x01, 0x00, 0xA1])

DEFAULT_RELAY_ON_TIME_MS = 500
DEFAULT_RELAY_OFF_BASE_TIME_MS = 1000
DEFAULT_T_DELAY_MAX_MS = 3740
T_DELAY_START_MS = 0
T_DELAY_STEP_MS = 1
TX_BURST_COUNT = 10
TX_BURST_WINDOW_MS = 500
TX_BURST_PERIOD_MS = TX_BURST_WINDOW_MS / TX_BURST_COUNT
ECU_RESPONSE_TIMEOUT_MS = 500

VALID_CANFD_LENGTHS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64]

STATE_IDLE = 0
STATE_ONGOING = 1
STATE_FINISHED = 2

ERR_OK = 0
ERR_INVALID_CONFIG = 1
ERR_RELAY_OPEN = 2
ERR_RELAY_IO = 3
ERR_VECTOR_IMPORT = 4
ERR_VECTOR_CONNECT = 5
ERR_CAN_SEND = 6
ERR_ABORTED = 7
ERR_EXCEPTION = 8


# -----------------------------------------------------------------------------
# Parsing helpers
# -----------------------------------------------------------------------------

def parse_int_dec(text: str, name: str, minimum=None, maximum=None) -> int:
    value_s = str(text).strip()
    if value_s == "":
        raise ValueError(f"{name} is empty")
    try:
        value = int(value_s, 10)
    except ValueError:
        raise ValueError(f"{name} must be decimal integer")
    if minimum is not None and value < minimum:
        raise ValueError(f"{name} must be >= {minimum}")
    if maximum is not None and value > maximum:
        raise ValueError(f"{name} must be <= {maximum}")
    return value


def parse_can_id_hex(text: str, extended: bool) -> int:
    value_s = str(text).strip().lower()
    if value_s == "":
        raise ValueError("CAN ID is empty")
    if value_s.startswith("0x"):
        can_id = int(value_s, 16)
    else:
        can_id = int(value_s, 16)

    max_id = 0x1FFFFFFF if extended else 0x7FF
    if can_id < 0 or can_id > max_id:
        id_type = "extended" if extended else "standard"
        raise ValueError(f"CAN ID out of range for {id_type} ID: max 0x{max_id:X}")
    return can_id


def parse_payload_hex(text: str, pad_to_valid_fd_dlc: bool) -> bytes:
    raw = str(text).strip()
    if raw == "":
        data = bytearray()
    else:
        normalized = raw.replace(",", " ").replace(";", " ").replace("_", " ")
        tokens = normalized.split()

        if len(tokens) <= 1:
            compact = raw.lower().replace("0x", "")
            compact = re.sub(r"[^0-9a-f]", "", compact)
            if len(compact) == 0:
                data = bytearray()
            elif len(compact) % 2 != 0:
                raise ValueError("Payload compact hex format must contain an even number of hex digits")
            else:
                data = bytearray(int(compact[i:i + 2], 16) for i in range(0, len(compact), 2))
        else:
            data = bytearray()
            for token in tokens:
                token_s = token.lower()
                if token_s.startswith("0x"):
                    token_s = token_s[2:]
                if token_s == "":
                    continue
                value = int(token_s, 16)
                if value < 0 or value > 0xFF:
                    raise ValueError(f"Payload byte out of range: {token}")
                data.append(value)

    if len(data) > 64:
        raise ValueError("CAN FD payload cannot exceed 64 bytes")

    if len(data) not in VALID_CANFD_LENGTHS:
        if not pad_to_valid_fd_dlc:
            valid = ", ".join(str(x) for x in VALID_CANFD_LENGTHS)
            raise ValueError(f"Invalid CAN FD payload length {len(data)}. Valid lengths: {valid}")
        target_len = None
        for valid_len in VALID_CANFD_LENGTHS:
            if valid_len >= len(data):
                target_len = valid_len
                break
        if target_len is None:
            raise ValueError("CAN FD payload cannot exceed 64 bytes")
        while len(data) < target_len:
            data.append(0x00)

    return bytes(data)


def parse_expected_response_payload_hex(text: str, pad_to_valid_fd_dlc: bool):
    raw = str(text).strip()
    if raw == "":
        return None
    return parse_payload_hex(raw, pad_to_valid_fd_dlc)


def normalize_com_port(text: str) -> str:
    value = str(text).strip().upper()
    if value == "":
        raise ValueError("Relay COM port is empty")
    if value.isdigit():
        value = "COM" + value
    if not value.startswith("COM"):
        raise ValueError("Relay COM port must look like COM8 or 8")
    return value


def hex_bytes(data: bytes) -> str:
    return " ".join(f"{b:02X}" for b in data)


# -----------------------------------------------------------------------------
# Relay driver
# -----------------------------------------------------------------------------

class UsbRelay:
    def __init__(self):
        self.ser = None

    def open(self, port: str) -> None:
        if serial is None:
            raise RuntimeError("pyserial is not installed. Run: py -m pip install pyserial")

        self.close()
        self.ser = serial.Serial(
            port=port,
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


# -----------------------------------------------------------------------------
# Vector CAN FD driver through python-can Vector backend
# -----------------------------------------------------------------------------

class VectorCanFd:
    def __init__(self):
        self.bus = None

    def connect(self, app_name: str, channel: int, bitrate: int, data_bitrate: int) -> None:
        if can is None:
            raise ImportError("python-can is not installed. Run: py -m pip install python-can")

        self.disconnect()
        kwargs = dict(
            interface="vector",
            app_name=app_name,
            channel=channel,
            bitrate=bitrate,
            data_bitrate=data_bitrate,
            fd=True,
            receive_own_messages=False,
        )
        self.bus = can.Bus(**kwargs)

    def disconnect(self) -> None:
        if self.bus is not None:
            try:
                self.bus.shutdown()
            finally:
                self.bus = None

    def send_fd(self, arbitration_id: int, data: bytes, extended_id: bool, brs: bool) -> None:
        if self.bus is None:
            raise RuntimeError("Vector CAN FD bus is not connected")

        msg = can.Message(
            arbitration_id=arbitration_id,
            is_extended_id=extended_id,
            is_fd=True,
            bitrate_switch=brs,
            data=data,
            check=True,
        )
        self.bus.send(msg, timeout=1.0)

    def recv(self, timeout_s: float):
        if self.bus is None:
            raise RuntimeError("Vector CAN FD bus is not connected")
        return self.bus.recv(timeout=max(0.0, timeout_s))

    def flush_rx(self) -> int:
        if self.bus is None:
            raise RuntimeError("Vector CAN FD bus is not connected")
        flushed = 0
        while True:
            msg = self.bus.recv(timeout=0.0)
            if msg is None:
                break
            flushed += 1
        return flushed


# -----------------------------------------------------------------------------
# GUI application
# -----------------------------------------------------------------------------

class AppConfig:
    def __init__(self):
        self.vector_app_name = DEFAULT_VECTOR_APP_NAME
        self.vector_channel = DEFAULT_VECTOR_CHANNEL
        self.nominal_bitrate = DEFAULT_NOMINAL_BITRATE
        self.data_bitrate = DEFAULT_DATA_BITRATE
        self.can_id = 0x93
        self.extended_id = DEFAULT_EXTENDED_ID
        self.brs = DEFAULT_BRS
        self.payload = bytes([0x00] * 12)
        self.response_can_id = 0x6EF
        self.response_extended_id = DEFAULT_RESPONSE_EXTENDED_ID
        self.response_payload = None
        self.relay_com = DEFAULT_RELAY_COM
        self.relay_on_time_ms = DEFAULT_RELAY_ON_TIME_MS
        self.relay_off_base_time_ms = DEFAULT_RELAY_OFF_BASE_TIME_MS
        self.tdelay_max_ms = DEFAULT_T_DELAY_MAX_MS
        self.repeat_when_finished = False


class VectorCanFdRelayApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Vector CAN FD + Relay tDelay Test")
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        self.root.resizable(True, True)

        self.relay = UsbRelay()
        self.canfd = VectorCanFd()
        self.worker = None
        self.stop_event = threading.Event()
        self.lock = threading.Lock()

        self.state = STATE_IDLE
        self.relay_stat = 0
        self.error = ERR_OK
        self.current_tdelay_ms = 0
        self.cycle_count = 0
        self.last_tx = "-"
        self.last_rx = "-"

        self.build_ui()
        self.refresh_com_ports()
        self.update_ui_periodic()

    def build_ui(self) -> None:
        main = ttk.Frame(self.root, padding=10)
        main.grid(row=0, column=0, sticky="nsew")
        self.root.rowconfigure(0, weight=1)
        self.root.columnconfigure(0, weight=1)
        main.columnconfigure(0, weight=1)
        main.rowconfigure(4, weight=1)

        vector_frame = ttk.LabelFrame(main, text="Vector CAN FD", padding=10)
        vector_frame.grid(row=0, column=0, sticky="ew")
        for col in range(6):
            vector_frame.columnconfigure(col, weight=1)

        ttk.Label(vector_frame, text="App name").grid(row=0, column=0, sticky="w")
        self.vector_app_var = tk.StringVar(value=DEFAULT_VECTOR_APP_NAME)
        ttk.Entry(vector_frame, textvariable=self.vector_app_var, width=16).grid(row=0, column=1, sticky="ew", padx=(4, 10))

        ttk.Label(vector_frame, text="Channel").grid(row=0, column=2, sticky="w")
        self.channel_var = tk.StringVar(value=str(DEFAULT_VECTOR_CHANNEL))
        ttk.Entry(vector_frame, textvariable=self.channel_var, width=8).grid(row=0, column=3, sticky="ew", padx=(4, 10))

        ttk.Label(vector_frame, text="Nominal bitrate").grid(row=0, column=4, sticky="w")
        self.nom_bitrate_var = tk.StringVar(value=str(DEFAULT_NOMINAL_BITRATE))
        ttk.Entry(vector_frame, textvariable=self.nom_bitrate_var, width=12).grid(row=0, column=5, sticky="ew", padx=(4, 0))

        ttk.Label(vector_frame, text="Data bitrate").grid(row=1, column=0, sticky="w")
        self.data_bitrate_var = tk.StringVar(value=str(DEFAULT_DATA_BITRATE))
        ttk.Entry(vector_frame, textvariable=self.data_bitrate_var, width=12).grid(row=1, column=1, sticky="ew", padx=(4, 10))

        ttk.Label(vector_frame, text="CAN ID [hex]").grid(row=1, column=2, sticky="w")
        self.can_id_var = tk.StringVar(value=DEFAULT_CAN_ID_HEX)
        ttk.Entry(vector_frame, textvariable=self.can_id_var, width=12).grid(row=1, column=3, sticky="ew", padx=(4, 10))

        self.extended_var = tk.BooleanVar(value=DEFAULT_EXTENDED_ID)
        ttk.Checkbutton(vector_frame, text="Extended ID", variable=self.extended_var).grid(row=1, column=4, sticky="w")

        self.brs_var = tk.BooleanVar(value=DEFAULT_BRS)
        ttk.Checkbutton(vector_frame, text="BRS", variable=self.brs_var).grid(row=1, column=5, sticky="w")

        ttk.Label(vector_frame, text="Payload [hex]").grid(row=2, column=0, sticky="w")
        self.payload_var = tk.StringVar(value=DEFAULT_PAYLOAD_HEX)
        ttk.Entry(vector_frame, textvariable=self.payload_var).grid(row=2, column=1, columnspan=5, sticky="ew", padx=(4, 0))

        self.pad_var = tk.BooleanVar(value=DEFAULT_PAD_TO_VALID_FD_DLC)
        ttk.Checkbutton(vector_frame, text="Pad TX payload to valid CAN FD DLC length", variable=self.pad_var).grid(row=3, column=1, columnspan=5, sticky="w", pady=(4, 0))

        ttk.Label(vector_frame, text="Resp ID [hex]").grid(row=4, column=0, sticky="w", pady=(6, 0))
        self.response_can_id_var = tk.StringVar(value=DEFAULT_RESPONSE_CAN_ID_HEX)
        ttk.Entry(vector_frame, textvariable=self.response_can_id_var, width=12).grid(row=4, column=1, sticky="ew", padx=(4, 10), pady=(6, 0))

        self.response_extended_var = tk.BooleanVar(value=DEFAULT_RESPONSE_EXTENDED_ID)
        ttk.Checkbutton(vector_frame, text="Resp Extended ID", variable=self.response_extended_var).grid(row=4, column=2, columnspan=2, sticky="w", pady=(6, 0))

        ttk.Label(vector_frame, text="Resp payload [hex]").grid(row=5, column=0, sticky="w")
        self.response_payload_var = tk.StringVar(value=DEFAULT_RESPONSE_PAYLOAD_HEX)
        ttk.Entry(vector_frame, textvariable=self.response_payload_var).grid(row=5, column=1, columnspan=5, sticky="ew", padx=(4, 0))

        self.response_pad_var = tk.BooleanVar(value=DEFAULT_RESPONSE_PAD_TO_VALID_FD_DLC)
        ttk.Checkbutton(vector_frame, text="Pad response payload to valid CAN FD DLC length; empty = match ID only", variable=self.response_pad_var).grid(row=6, column=1, columnspan=5, sticky="w", pady=(4, 0))

        relay_frame = ttk.LabelFrame(main, text="Relay and sweep", padding=10)
        relay_frame.grid(row=1, column=0, sticky="ew", pady=(8, 0))
        for col in range(6):
            relay_frame.columnconfigure(col, weight=1)

        ttk.Label(relay_frame, text="Relay COM").grid(row=0, column=0, sticky="w")
        self.relay_com_var = tk.StringVar(value=DEFAULT_RELAY_COM)
        self.relay_com_combo = ttk.Combobox(relay_frame, textvariable=self.relay_com_var, width=12)
        self.relay_com_combo.grid(row=0, column=1, sticky="ew", padx=(4, 10))
        ttk.Button(relay_frame, text="Refresh", command=self.refresh_com_ports).grid(row=0, column=2, sticky="ew", padx=(0, 10))

        ttk.Label(relay_frame, text="Relay ON [ms]").grid(row=0, column=3, sticky="w")
        self.relay_on_var = tk.StringVar(value=str(DEFAULT_RELAY_ON_TIME_MS))
        ttk.Entry(relay_frame, textvariable=self.relay_on_var, width=10).grid(row=0, column=4, sticky="ew", padx=(4, 10))

        ttk.Label(relay_frame, text="Relay OFF base [ms]").grid(row=1, column=0, sticky="w", pady=(6, 0))
        self.relay_off_var = tk.StringVar(value=str(DEFAULT_RELAY_OFF_BASE_TIME_MS))
        ttk.Entry(relay_frame, textvariable=self.relay_off_var, width=10).grid(row=1, column=1, sticky="ew", padx=(4, 10), pady=(6, 0))

        ttk.Label(relay_frame, text="tDelay_Max [ms]").grid(row=1, column=3, sticky="w", pady=(6, 0))
        self.tdelay_max_var = tk.StringVar(value=str(DEFAULT_T_DELAY_MAX_MS))
        ttk.Entry(relay_frame, textvariable=self.tdelay_max_var, width=10).grid(row=1, column=4, sticky="ew", padx=(4, 10), pady=(6, 0))

        self.repeat_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(relay_frame, text="Restart from tDelay=0 when finished", variable=self.repeat_var).grid(row=2, column=1, columnspan=4, sticky="w", pady=(6, 0))

        status_frame = ttk.LabelFrame(main, text="Status", padding=10)
        status_frame.grid(row=2, column=0, sticky="ew", pady=(8, 0))
        for col in range(4):
            status_frame.columnconfigure(col, weight=1)

        ttk.Label(status_frame, text="State").grid(row=0, column=0, sticky="w")
        self.state_var = tk.StringVar(value="0 = idle")
        ttk.Label(status_frame, textvariable=self.state_var).grid(row=0, column=1, sticky="w")

        ttk.Label(status_frame, text="Relay_STAT").grid(row=0, column=2, sticky="w")
        self.relay_stat_var = tk.StringVar(value="0 = relay off")
        ttk.Label(status_frame, textvariable=self.relay_stat_var).grid(row=0, column=3, sticky="w")

        ttk.Label(status_frame, text="Error").grid(row=1, column=0, sticky="w")
        self.error_var = tk.StringVar(value="0 = OK")
        ttk.Label(status_frame, textvariable=self.error_var).grid(row=1, column=1, sticky="w")

        ttk.Label(status_frame, text="Current tDelay").grid(row=1, column=2, sticky="w")
        self.tdelay_var = tk.StringVar(value="0 ms")
        ttk.Label(status_frame, textvariable=self.tdelay_var).grid(row=1, column=3, sticky="w")

        ttk.Label(status_frame, text="Cycles").grid(row=2, column=0, sticky="w")
        self.cycles_var = tk.StringVar(value="0")
        ttk.Label(status_frame, textvariable=self.cycles_var).grid(row=2, column=1, sticky="w")

        ttk.Label(status_frame, text="Last TX").grid(row=2, column=2, sticky="w")
        self.last_tx_var = tk.StringVar(value="-")
        ttk.Label(status_frame, textvariable=self.last_tx_var).grid(row=2, column=3, sticky="w")

        ttk.Label(status_frame, text="Last RX").grid(row=3, column=0, sticky="w")
        self.last_rx_var = tk.StringVar(value="-")
        ttk.Label(status_frame, textvariable=self.last_rx_var).grid(row=3, column=1, columnspan=3, sticky="w")

        button_frame = ttk.Frame(main)
        button_frame.grid(row=3, column=0, sticky="ew", pady=(8, 8))
        button_frame.columnconfigure(0, weight=1)
        button_frame.columnconfigure(1, weight=1)
        button_frame.columnconfigure(2, weight=1)

        self.start_button = ttk.Button(button_frame, text="Start tDelay Test", command=self.start_sweep)
        self.start_button.grid(row=0, column=0, sticky="ew", padx=(0, 4))

        self.once_button = ttk.Button(button_frame, text="One Cycle", command=self.start_once)
        self.once_button.grid(row=0, column=1, sticky="ew", padx=(4, 4))

        self.stop_button = ttk.Button(button_frame, text="Stop", command=self.stop_test, state="disabled")
        self.stop_button.grid(row=0, column=2, sticky="ew", padx=(4, 0))

        log_frame = ttk.LabelFrame(main, text="Log", padding=10)
        log_frame.grid(row=4, column=0, sticky="nsew")
        log_frame.rowconfigure(0, weight=1)
        log_frame.columnconfigure(0, weight=1)

        self.log_text = tk.Text(log_frame, height=15, width=110, wrap="word")
        self.log_text.grid(row=0, column=0, sticky="nsew")
        scroll = ttk.Scrollbar(log_frame, orient="vertical", command=self.log_text.yview)
        scroll.grid(row=0, column=1, sticky="ns")
        self.log_text.configure(yscrollcommand=scroll.set)

    def refresh_com_ports(self) -> None:
        current = self.relay_com_var.get().strip() or DEFAULT_RELAY_COM
        ports = []
        if list_ports is not None:
            try:
                ports = [p.device for p in list_ports.comports()]
            except Exception:
                ports = []
        if current and current not in ports:
            ports.insert(0, current)
        self.relay_com_combo["values"] = ports
        self.relay_com_var.set(current)

    def read_config(self) -> AppConfig:
        cfg = AppConfig()
        cfg.vector_app_name = self.vector_app_var.get().strip() or DEFAULT_VECTOR_APP_NAME
        cfg.vector_channel = parse_int_dec(self.channel_var.get(), "Vector channel", 0, 255)
        cfg.nominal_bitrate = parse_int_dec(self.nom_bitrate_var.get(), "Nominal bitrate", 10000, 8000000)
        cfg.data_bitrate = parse_int_dec(self.data_bitrate_var.get(), "Data bitrate", 10000, 12000000)
        cfg.extended_id = bool(self.extended_var.get())
        cfg.brs = bool(self.brs_var.get())
        cfg.can_id = parse_can_id_hex(self.can_id_var.get(), cfg.extended_id)
        cfg.payload = parse_payload_hex(self.payload_var.get(), bool(self.pad_var.get()))
        cfg.response_extended_id = bool(self.response_extended_var.get())
        cfg.response_can_id = parse_can_id_hex(self.response_can_id_var.get(), cfg.response_extended_id)
        cfg.response_payload = parse_expected_response_payload_hex(
            self.response_payload_var.get(), bool(self.response_pad_var.get())
        )
        cfg.relay_com = normalize_com_port(self.relay_com_var.get())
        cfg.relay_on_time_ms = parse_int_dec(self.relay_on_var.get(), "Relay ON time", TX_BURST_WINDOW_MS, 3600000)
        cfg.relay_off_base_time_ms = parse_int_dec(self.relay_off_var.get(), "Relay OFF base time", 0, 3600000)
        cfg.tdelay_max_ms = parse_int_dec(self.tdelay_max_var.get(), "tDelay_Max", 0, 3600000)
        cfg.repeat_when_finished = bool(self.repeat_var.get())
        return cfg

    def set_state(self, value: int) -> None:
        with self.lock:
            self.state = value

    def set_relay_stat(self, value: int) -> None:
        with self.lock:
            self.relay_stat = value

    def set_error(self, value: int) -> None:
        with self.lock:
            self.error = value

    def set_tdelay(self, value: int) -> None:
        with self.lock:
            self.current_tdelay_ms = value

    def set_cycles(self, value: int) -> None:
        with self.lock:
            self.cycle_count = value

    def set_last_tx(self, value: str) -> None:
        with self.lock:
            self.last_tx = value

    def set_last_rx(self, value: str) -> None:
        with self.lock:
            self.last_rx = value

    def log(self, text: str) -> None:
        timestamp = time.strftime("%H:%M:%S")
        self.root.after(0, self._append_log, f"[{timestamp}] {text}\n")

    def _append_log(self, line: str) -> None:
        self.log_text.insert("end", line)
        self.log_text.see("end")

    def sleep_interruptible_ms(self, duration_ms: int) -> bool:
        end = time.monotonic() + (duration_ms / 1000.0)
        while time.monotonic() < end:
            if self.stop_event.is_set():
                return False
            remaining = end - time.monotonic()
            time.sleep(min(0.001, max(0.0, remaining)))
        return True

    def start_sweep(self) -> None:
        self.start_worker(run_once=False)

    def start_once(self) -> None:
        self.start_worker(run_once=True)

    def start_worker(self, run_once: bool) -> None:
        if self.worker is not None and self.worker.is_alive():
            return

        try:
            cfg = self.read_config()
        except Exception as exc:
            self.set_error(ERR_INVALID_CONFIG)
            self.log(f"Invalid config: {exc}")
            return

        self.stop_event.clear()
        self.set_error(ERR_OK)
        self.set_state(STATE_ONGOING)
        self.set_relay_stat(0)
        self.set_tdelay(0)
        self.set_cycles(0)
        self.set_last_tx("-")
        self.set_last_rx("-")

        self.worker = threading.Thread(
            target=self.worker_main,
            args=(cfg, run_once),
            name="VectorCanFdRelayWorker",
            daemon=True,
        )
        self.worker.start()

        self.start_button.configure(state="disabled")
        self.once_button.configure(state="disabled")
        self.stop_button.configure(state="normal")

        if run_once:
            self.log("One-cycle Vector CAN FD + relay test started")
        else:
            self.log("Vector CAN FD + relay tDelay test started")

    def stop_test(self) -> None:
        self.stop_event.set()
        self.log("Stop requested")

    def relay_safe_off_close(self) -> None:
        try:
            try:
                if self.relay.ser is not None and self.relay.ser.is_open:
                    self.relay.off()
            finally:
                self.relay.close()
        finally:
            self.set_relay_stat(0)

    def rx_matches_expected(self, msg, cfg: AppConfig) -> bool:
        if msg is None:
            return False
        if getattr(msg, "arbitration_id", None) != cfg.response_can_id:
            return False
        if bool(getattr(msg, "is_extended_id", False)) != bool(cfg.response_extended_id):
            return False
        if cfg.response_payload is not None and bytes(getattr(msg, "data", b"")) != cfg.response_payload:
            return False
        return True

    def format_rx(self, msg) -> str:
        if msg is None:
            return "-"
        frame_type = "FD" if bool(getattr(msg, "is_fd", False)) else "Classic"
        ext = "EXT" if bool(getattr(msg, "is_extended_id", False)) else "STD"
        data = bytes(getattr(msg, "data", b""))
        return f"{frame_type} {ext} ID=0x{msg.arbitration_id:X}, len={len(data)}, data={hex_bytes(data)}"

    def expected_response_text(self, cfg: AppConfig) -> str:
        ext = "EXT" if cfg.response_extended_id else "STD"
        if cfg.response_payload is None:
            payload = "any payload"
        else:
            payload = hex_bytes(cfg.response_payload)
        return f"{ext} ID=0x{cfg.response_can_id:X}, payload={payload}"

    def poll_expected_response_until(self, cfg: AppConfig, deadline: float):
        while time.monotonic() < deadline:
            if self.stop_event.is_set():
                return None
            remaining = deadline - time.monotonic()
            timeout_s = min(0.010, max(0.0, remaining))
            try:
                msg = self.canfd.recv(timeout_s)
            except Exception as exc:
                self.log(f"CAN FD RX check failed: {exc}")
                return None
            if msg is None:
                continue
            if self.rx_matches_expected(msg, cfg):
                rx_text = self.format_rx(msg)
                self.set_last_rx(rx_text)
                return msg
        return None

    def send_burst_and_check_response(self, cfg: AppConfig) -> bool:
        try:
            flushed = self.canfd.flush_rx()
            if flushed:
                self.log(f"RX queue flushed before TX burst: {flushed} old frame(s)")
        except Exception as exc:
            self.log(f"RX queue flush skipped/failed: {exc}")

        tx_text = f"ID=0x{cfg.can_id:X}, len={len(cfg.payload)}, data={hex_bytes(cfg.payload)}"
        self.set_last_tx(tx_text)
        self.log(f"CAN FD TX burst start: {TX_BURST_COUNT} frames / {TX_BURST_WINDOW_MS} ms, {tx_text}")
        self.log(f"Expected ECU response within {ECU_RESPONSE_TIMEOUT_MS} ms: {self.expected_response_text(cfg)}")

        start = time.monotonic()
        deadline = start + (ECU_RESPONSE_TIMEOUT_MS / 1000.0)
        response_msg = None

        for index in range(TX_BURST_COUNT):
            due_time = start + ((index * TX_BURST_PERIOD_MS) / 1000.0)
            if response_msg is None:
                response_msg = self.poll_expected_response_until(cfg, due_time)
            else:
                while time.monotonic() < due_time:
                    if self.stop_event.is_set():
                        return False
                    time.sleep(min(0.001, max(0.0, due_time - time.monotonic())))

            if self.stop_event.is_set():
                return False

            try:
                self.canfd.send_fd(cfg.can_id, cfg.payload, cfg.extended_id, cfg.brs)
            except Exception as exc:
                self.set_error(ERR_CAN_SEND)
                self.log(f"CAN FD send failed at burst frame {index + 1}/{TX_BURST_COUNT}: {exc}")
                return False

        if response_msg is None:
            response_msg = self.poll_expected_response_until(cfg, deadline)

        if response_msg is not None:
            rx_text = self.format_rx(response_msg)
            elapsed_ms = int(round((time.monotonic() - start) * 1000.0))
            self.log(f"ECU response OK in <= {ECU_RESPONSE_TIMEOUT_MS} ms: {rx_text}, observed after ~{elapsed_ms} ms")
        else:
            self.set_last_rx("NO MATCH within 500 ms")
            self.log(f"ECU response NOT seen within {ECU_RESPONSE_TIMEOUT_MS} ms: expected {self.expected_response_text(cfg)}")

        self.log("CAN FD TX burst complete")
        return True

    def execute_one_cycle(self, cfg: AppConfig, tdelay_ms: int) -> bool:
        self.set_tdelay(tdelay_ms)

        try:
            self.relay.open(cfg.relay_com)
        except Exception as exc:
            self.set_error(ERR_RELAY_OPEN)
            self.log(f"Relay COM open failed: {exc}")
            return False

        try:
            self.relay.on()
            self.set_relay_stat(1)
            self.log("Relay ON")

            burst_start = time.monotonic()
            if not self.send_burst_and_check_response(cfg):
                return False

            elapsed_ms = int(round((time.monotonic() - burst_start) * 1000.0))
            remaining_on_ms = cfg.relay_on_time_ms - elapsed_ms
            if remaining_on_ms > 0:
                if not self.sleep_interruptible_ms(remaining_on_ms):
                    self.set_error(ERR_ABORTED)
                    return False

            self.relay.off()
            self.set_relay_stat(0)
            self.log("Relay OFF")
            return True

        except Exception as exc:
            self.set_error(ERR_RELAY_IO)
            self.log(f"Relay I/O failed: {exc}")
            return False

        finally:
            self.relay_safe_off_close()

    def worker_main(self, cfg: AppConfig, run_once: bool) -> None:
        finished = False
        connected = False
        cycle = 0

        try:
            if can is None:
                self.set_error(ERR_VECTOR_IMPORT)
                self.log("python-can import failed. Install with: py -m pip install python-can")
                return

            try:
                self.canfd.connect(cfg.vector_app_name, cfg.vector_channel, cfg.nominal_bitrate, cfg.data_bitrate)
                connected = True
                self.log(
                    "Vector connected: "
                    f"app={cfg.vector_app_name}, channel={cfg.vector_channel}, "
                    f"nominal={cfg.nominal_bitrate}, data={cfg.data_bitrate}"
                )
            except Exception as exc:
                self.set_error(ERR_VECTOR_CONNECT)
                self.log(f"Vector connect/config failed: {exc}")
                return

            tdelay_ms = T_DELAY_START_MS
            max_tdelay = 0 if run_once else cfg.tdelay_max_ms

            while not self.stop_event.is_set():
                ok = self.execute_one_cycle(cfg, tdelay_ms)
                if not ok:
                    return

                cycle += 1
                self.set_cycles(cycle)

                if run_once:
                    finished = True
                    return

                wait_ms = cfg.relay_off_base_time_ms + tdelay_ms

                if tdelay_ms >= max_tdelay:
                    if cfg.repeat_when_finished:
                        self.log(
                            f"tDelay sweep finished at {tdelay_ms} ms; wait {wait_ms} ms, then restart from 0 ms"
                        )
                        if not self.sleep_interruptible_ms(wait_ms):
                            self.set_error(ERR_ABORTED)
                            return
                        tdelay_ms = T_DELAY_START_MS
                        continue

                    finished = True
                    return

                self.log(f"Wait {wait_ms} ms before next cycle")
                if not self.sleep_interruptible_ms(wait_ms):
                    self.set_error(ERR_ABORTED)
                    return

                tdelay_ms += T_DELAY_STEP_MS

        except Exception:
            self.set_error(ERR_EXCEPTION)
            self.log("Unhandled exception:")
            self.log(traceback.format_exc())

        finally:
            self.relay_safe_off_close()
            if connected:
                self.canfd.disconnect()
                self.log("Vector disconnected")

            with self.lock:
                err = self.error

            if finished:
                self.set_state(STATE_FINISHED)
                self.log("Test finished")
            else:
                self.set_state(STATE_IDLE)
                if err == ERR_OK:
                    self.set_error(ERR_ABORTED)
                self.log("Test stopped")

            self.root.after(0, self.worker_done)

    def worker_done(self) -> None:
        self.start_button.configure(state="normal")
        self.once_button.configure(state="normal")
        self.stop_button.configure(state="disabled")

    def update_ui_periodic(self) -> None:
        with self.lock:
            state = self.state
            relay_stat = self.relay_stat
            error = self.error
            tdelay = self.current_tdelay_ms
            cycles = self.cycle_count
            last_tx = self.last_tx
            last_rx = self.last_rx

        state_text = {
            STATE_IDLE: "0 = idle",
            STATE_ONGOING: "1 = ongoing",
            STATE_FINISHED: "2 = finished",
        }.get(state, f"{state} = unknown")

        relay_text = "1 = relay on" if relay_stat else "0 = relay off"

        error_text = {
            ERR_OK: "0 = OK",
            ERR_INVALID_CONFIG: "1 = invalid config",
            ERR_RELAY_OPEN: "2 = relay COM open fail",
            ERR_RELAY_IO: "3 = relay I/O fail",
            ERR_VECTOR_IMPORT: "4 = python-can import fail",
            ERR_VECTOR_CONNECT: "5 = Vector connect/config fail",
            ERR_CAN_SEND: "6 = CAN FD send fail",
            ERR_ABORTED: "7 = aborted/stopped",
            ERR_EXCEPTION: "8 = exception",
        }.get(error, f"{error} = unknown")

        self.state_var.set(state_text)
        self.relay_stat_var.set(relay_text)
        self.error_var.set(error_text)
        self.tdelay_var.set(f"{tdelay} ms")
        self.cycles_var.set(str(cycles))
        self.last_tx_var.set(last_tx)
        self.last_rx_var.set(last_rx)

        self.root.after(100, self.update_ui_periodic)

    def on_close(self) -> None:
        if self.worker is not None and self.worker.is_alive():
            self.stop_event.set()
            self.relay_safe_off_close()
            self.canfd.disconnect()
        self.root.destroy()


def main() -> int:
    root = tk.Tk()

    missing = []
    if serial is None:
        missing.append("pyserial")
    if can is None:
        missing.append("python-can")

    if missing:
        messagebox.showerror(
            "Missing dependency",
            "Missing Python package(s): " + ", ".join(missing) +
            "\n\nInstall with:\n    py -m pip install python-can pyserial"
        )

    VectorCanFdRelayApp(root)
    root.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
