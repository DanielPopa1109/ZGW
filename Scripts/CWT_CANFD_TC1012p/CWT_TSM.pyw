# -*- coding: utf-8 -*-
"""
TSMaster TC1012P CAN FD tDelay sender.

Standalone Windows .pyw GUI.

Behavior intentionally mirrors the WUP relay script timing rules:
- action phase: transmit one CAN FD frame once
- wait phase: 1000 ms + current tDelay
- tDelay starts at 0 ms
- tDelay increments by 1 ms per cycle
- test stops after tDelay_Max is reached

Frame source:
- tries to read the first CAN FD message from ZGW_CANFD_2.dbc located next to this script
- fallback message embedded from the uploaded DBC:
    PDM4_LoadStatus, standard CAN ID 0x093, payload length 12 bytes

Required TSMaster package/API on Windows:
    pip install tsmasterapi

Note:
- The official PyPI tsmasterapi package states it is for WIN32 Python.
- If your local TSMasterAPI package uses different TC1012P subtype IDs, change the
  editable "Device subtype" field in the UI.
"""

import os
import re
import sys
import time
import threading
import traceback
import tkinter as tk
from tkinter import ttk, messagebox, filedialog


# -----------------------------------------------------------------------------
# Timing configuration matching the previous WUP relay script
# -----------------------------------------------------------------------------

POST_ACTION_WAIT_MS = 1000
T_DELAY_START_MS = 0
T_DELAY_DEFAULT_END_MS = 3740
T_DELAY_STEP_MS = 1

STATE_IDLE = 0
STATE_ONGOING = 1
STATE_FINISHED = 2

ERR_NONE = 0
ERR_API_IMPORT = 1
ERR_TSM_CONFIG_CONNECT = 2
ERR_CANFD_TX = 3
ERR_ABORTED = 4
ERR_EXCEPTION = 5
ERR_CONFIG = 6
ERR_DBC = 7

DEFAULT_APP_NAME = "TSMaster HW Tools"
DEFAULT_HW_NAME = "TC1012P"
DEFAULT_DEVICE_TYPE = 3       # TS_USB_DEVICE in common TSMasterAPI examples
DEFAULT_DEVICE_SUBTYPE = 12    # TC1012P; editable in UI in case local enum differs
DEFAULT_HW_INDEX = 0
DEFAULT_HW_CHANNEL = 0
DEFAULT_APP_CHANNEL = 0
DEFAULT_NOMINAL_KBPS = 500
DEFAULT_DATA_KBPS = 2000
DEFAULT_CANFD_CONTROLLER_TYPE = 1  # ISO CAN FD in common examples
DEFAULT_CANFD_CONTROLLER_MODE = 0  # normal mode in common examples
DEFAULT_TERMINATION_120R = True
DEFAULT_BRS = True

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd()
DEFAULT_DBC_PATH = os.path.join(SCRIPT_DIR, "ZGW_CANFD_2.dbc")

# Fallback from uploaded ZGW_CANFD_2.dbc first BO_ entry:
# BO_ 147 PDM4_LoadStatus: 12 PDM4
FALLBACK_MSG_ID = 0x093
FALLBACK_MSG_NAME = "PDM4_LoadStatus"
FALLBACK_MSG_LEN = 12
FALLBACK_IS_EXTENDED = False
FALLBACK_DATA = bytes([0x00] * FALLBACK_MSG_LEN)

CANFD_ALLOWED_LENGTHS = (0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64)
LEN_TO_DLC = {
    0: 0,
    1: 1,
    2: 2,
    3: 3,
    4: 4,
    5: 5,
    6: 6,
    7: 7,
    8: 8,
    12: 9,
    16: 10,
    20: 11,
    24: 12,
    32: 13,
    48: 14,
    64: 15,
}


def normalize_canfd_length(length: int) -> int:
    if length in CANFD_ALLOWED_LENGTHS:
        return length
    for allowed in CANFD_ALLOWED_LENGTHS:
        if length <= allowed:
            return allowed
    return 64


def canfd_len_to_dlc(length: int) -> int:
    return LEN_TO_DLC[normalize_canfd_length(length)]


def parse_first_canfd_message_from_dbc(path: str):
    """Return (name, arbitration_id, length, is_extended, data_bytes)."""
    if not path or not os.path.isfile(path):
        raise FileNotFoundError(path)

    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        content = f.read()

    bo_matches = re.findall(r"^BO_\s+(\d+)\s+([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(\d+)\s+\S+", content, re.MULTILINE)
    if not bo_matches:
        raise ValueError("No BO_ messages found in DBC")

    # Prefer a message explicitly marked StandardCAN_FD/ExtendedCAN_FD.
    canfd_ids = set()
    extended_ids = set()
    for msg_id_txt, fmt_txt in re.findall(r'BA_\s+"VFrameFormat"\s+BO_\s+(\d+)\s+(\d+)\s*;', content):
        msg_id = int(msg_id_txt)
        fmt = int(fmt_txt)
        if fmt in (14, 15):
            canfd_ids.add(msg_id)
        if fmt == 15:
            extended_ids.add(msg_id)

    selected = None
    for msg_id_txt, name, length_txt in bo_matches:
        msg_id = int(msg_id_txt)
        if not canfd_ids or msg_id in canfd_ids:
            selected = (msg_id, name, int(length_txt))
            break

    if selected is None:
        msg_id, name, length = int(bo_matches[0][0]), bo_matches[0][1], int(bo_matches[0][2])
    else:
        msg_id, name, length = selected

    is_extended = msg_id in extended_ids
    frame_len = normalize_canfd_length(length)
    data = bytes([0x00] * frame_len)
    return name, msg_id, frame_len, is_extended, data


class TSMasterCanFdBackend:
    def __init__(self):
        self.tsm = None
        self.connected = False

    def import_api(self):
        if self.tsm is not None:
            return

        last_error = None
        for module_name in ("TSMasterAPI", "tsmasterapi"):
            try:
                self.tsm = __import__(module_name)
                return
            except Exception as exc:
                last_error = exc

        raise RuntimeError(
            "Could not import TSMasterAPI/tsmasterapi. Install the TSMaster Python API package "
            "and use the Python bitness required by your installed package. Last error: "
            f"{last_error}"
        )

    def _call_optional(self, name: str, *args):
        func = getattr(self.tsm, name, None)
        if func is None:
            return None
        return func(*args)

    @staticmethod
    def _to_bytes(text: str) -> bytes:
        return text.encode("utf-8")

    @staticmethod
    def _check_ret(ret, label: str, allow_none: bool = True):
        if ret is None and allow_none:
            return
        if ret not in (0, True, None):
            raise RuntimeError(f"{label} failed, return={ret!r}")

    def connect(
        self,
        app_name: str,
        hw_name: str,
        app_channel: int,
        hw_index: int,
        hw_channel: int,
        device_type: int,
        device_subtype: int,
        nominal_kbps: int,
        data_kbps: int,
        termination_120r: bool,
    ) -> None:
        self.import_api()
        self.disconnect(ignore_errors=True)

        ret = self._call_optional("initialize_lib_tsmaster", self._to_bytes(app_name))
        self._check_ret(ret, "initialize_lib_tsmaster")

        ret = self._call_optional("tsapp_disconnect")
        self._check_ret(ret, "tsapp_disconnect")

        ret = self._call_optional("tsapp_set_can_channel_count", max(app_channel + 1, 1))
        self._check_ret(ret, "tsapp_set_can_channel_count")

        self._check_ret(self._call_optional("tsapp_set_lin_channel_count", 0), "tsapp_set_lin_channel_count")
        self._check_ret(self._call_optional("tsapp_set_flexray_channel_count", 0), "tsapp_set_flexray_channel_count")

        # Mapping must normally be done before tsapp_connect(). If the local API does not expose
        # this function, the script assumes mapping was already done in TSMaster configuration.
        ret = self._call_optional(
            "tsapp_set_mapping_verbose",
            self._to_bytes(app_name),
            0,  # APP_CAN in common TSMasterAPI examples
            app_channel,
            self._to_bytes(hw_name),
            device_type,
            device_subtype,
            hw_index,
            hw_channel,
            True,
        )
        self._check_ret(ret, "tsapp_set_mapping_verbose")

        ret = self._call_optional(
            "tsapp_configure_baudrate_canfd",
            app_channel,
            nominal_kbps,
            data_kbps,
            DEFAULT_CANFD_CONTROLLER_TYPE,
            DEFAULT_CANFD_CONTROLLER_MODE,
            bool(termination_120r),
        )
        self._check_ret(ret, "tsapp_configure_baudrate_canfd", allow_none=False)

        ret = self._call_optional("tsapp_connect")
        self._check_ret(ret, "tsapp_connect", allow_none=False)
        self.connected = True

    def disconnect(self, ignore_errors: bool = False) -> None:
        try:
            if self.tsm is not None:
                ret = self._call_optional("tsapp_disconnect")
                if not ignore_errors:
                    self._check_ret(ret, "tsapp_disconnect")
        finally:
            self.connected = False

    def _make_canfd_msg(self, channel: int, msg_id: int, data: bytes, is_extended: bool, brs: bool):
        msg_cls = getattr(self.tsm, "TLIBCANFD", None)
        if msg_cls is None:
            raise RuntimeError("TLIBCANFD class not found in TSMasterAPI module")

        dlc = canfd_len_to_dlc(len(data))
        padded_len = normalize_canfd_length(len(data))
        padded_data = bytes(data[:padded_len]).ljust(padded_len, b"\x00")

        try:
            msg = msg_cls(FIdxChn=channel, FIdentifier=msg_id, FDLC=dlc, FData=list(padded_data))
        except Exception:
            msg = msg_cls()

        # Common TSMaster/TOSUN fields. Set only if present in local API version.
        if hasattr(msg, "FIdxChn"):
            msg.FIdxChn = channel
        if hasattr(msg, "FIdentifier"):
            msg.FIdentifier = msg_id
        if hasattr(msg, "FDLC"):
            msg.FDLC = dlc
        if hasattr(msg, "FProperties"):
            # bit0 = TX direction, bit2 = extended ID in common examples
            msg.FProperties = 0x01 | (0x04 if is_extended else 0x00)
        if hasattr(msg, "FReserved"):
            msg.FReserved = 0
        if hasattr(msg, "FTimeUs"):
            msg.FTimeUs = 0
        if hasattr(msg, "FFDProperties"):
            # Common convention: EDL/FD + optional BRS. Local API may ignore if not needed.
            msg.FFDProperties = 0x01 | (0x02 if brs else 0x00)

        if hasattr(msg, "FData"):
            for i, value in enumerate(padded_data):
                msg.FData[i] = value

        return msg

    def transmit_once(self, channel: int, msg_id: int, data: bytes, is_extended: bool, brs: bool) -> None:
        if not self.connected:
            raise RuntimeError("TSMaster is not connected")

        msg = self._make_canfd_msg(channel, msg_id, data, is_extended, brs)

        func = getattr(self.tsm, "tsapp_transmit_canfd_async", None)
        if func is None:
            raise RuntimeError("tsapp_transmit_canfd_async not found in TSMasterAPI module")

        ret = func(msg)
        self._check_ret(ret, "tsapp_transmit_canfd_async", allow_none=True)


class CanFdTDelayApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("TSMaster CAN FD tDelay Test")
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        self.root.resizable(True, True)

        self.backend = TSMasterCanFdBackend()
        self.worker = None
        self.stop_event = threading.Event()
        self.lock = threading.Lock()

        self.state = STATE_IDLE
        self.error = ERR_NONE
        self.current_t_delay_ms = T_DELAY_START_MS
        self.cycles_done = 0
        self.last_tx = "-"
        self.message_name = FALLBACK_MSG_NAME
        self.message_id = FALLBACK_MSG_ID
        self.message_len = FALLBACK_MSG_LEN
        self.message_extended = FALLBACK_IS_EXTENDED
        self.message_data = FALLBACK_DATA

        self.build_ui()
        self.load_dbc_silent(DEFAULT_DBC_PATH)
        self.update_ui_periodic()

    def build_ui(self) -> None:
        main = ttk.Frame(self.root, padding=10)
        main.grid(row=0, column=0, sticky="nsew")
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)

        cfg = ttk.LabelFrame(main, text="CAN FD tDelay test", padding=10)
        cfg.grid(row=0, column=0, sticky="ew")
        cfg.columnconfigure(1, weight=1)

        row = 0
        ttk.Label(cfg, text="Application:").grid(row=row, column=0, sticky="w")
        self.app_name_var = tk.StringVar(value=DEFAULT_APP_NAME)
        ttk.Entry(cfg, textvariable=self.app_name_var).grid(row=row, column=1, sticky="ew")

        row += 1
        ttk.Label(cfg, text="Hardware:").grid(row=row, column=0, sticky="w")
        self.hw_name_var = tk.StringVar(value=DEFAULT_HW_NAME)
        ttk.Entry(cfg, textvariable=self.hw_name_var).grid(row=row, column=1, sticky="ew")

        row += 1
        ttk.Label(cfg, text="Device subtype:").grid(row=row, column=0, sticky="w")
        self.device_subtype_var = tk.StringVar(value=str(DEFAULT_DEVICE_SUBTYPE))
        ttk.Entry(cfg, textvariable=self.device_subtype_var).grid(row=row, column=1, sticky="ew")

        row += 1
        ch_frame = ttk.Frame(cfg)
        ch_frame.grid(row=row, column=1, sticky="ew")
        ch_frame.columnconfigure(1, weight=1)
        ch_frame.columnconfigure(3, weight=1)
        ttk.Label(cfg, text="Channels:").grid(row=row, column=0, sticky="w")
        ttk.Label(ch_frame, text="App CH").grid(row=0, column=0, sticky="w")
        self.app_channel_var = tk.StringVar(value=str(DEFAULT_APP_CHANNEL))
        ttk.Entry(ch_frame, textvariable=self.app_channel_var, width=8).grid(row=0, column=1, sticky="w", padx=(4, 12))
        ttk.Label(ch_frame, text="HW CH").grid(row=0, column=2, sticky="w")
        self.hw_channel_var = tk.StringVar(value=str(DEFAULT_HW_CHANNEL))
        ttk.Entry(ch_frame, textvariable=self.hw_channel_var, width=8).grid(row=0, column=3, sticky="w", padx=(4, 12))
        ttk.Label(ch_frame, text="HW index").grid(row=0, column=4, sticky="w")
        self.hw_index_var = tk.StringVar(value=str(DEFAULT_HW_INDEX))
        ttk.Entry(ch_frame, textvariable=self.hw_index_var, width=8).grid(row=0, column=5, sticky="w", padx=(4, 0))

        row += 1
        br_frame = ttk.Frame(cfg)
        br_frame.grid(row=row, column=1, sticky="ew")
        ttk.Label(cfg, text="Bitrates:").grid(row=row, column=0, sticky="w")
        ttk.Label(br_frame, text="Nominal kbps").grid(row=0, column=0, sticky="w")
        self.nominal_kbps_var = tk.StringVar(value=str(DEFAULT_NOMINAL_KBPS))
        ttk.Entry(br_frame, textvariable=self.nominal_kbps_var, width=8).grid(row=0, column=1, sticky="w", padx=(4, 12))
        ttk.Label(br_frame, text="Data kbps").grid(row=0, column=2, sticky="w")
        self.data_kbps_var = tk.StringVar(value=str(DEFAULT_DATA_KBPS))
        ttk.Entry(br_frame, textvariable=self.data_kbps_var, width=8).grid(row=0, column=3, sticky="w", padx=(4, 12))
        self.brs_var = tk.BooleanVar(value=DEFAULT_BRS)
        ttk.Checkbutton(br_frame, text="BRS", variable=self.brs_var).grid(row=0, column=4, sticky="w")
        self.term_var = tk.BooleanVar(value=DEFAULT_TERMINATION_120R)
        ttk.Checkbutton(br_frame, text="120R", variable=self.term_var).grid(row=0, column=5, sticky="w", padx=(8, 0))

        row += 1
        ttk.Label(cfg, text="tDelay_Max [ms]:").grid(row=row, column=0, sticky="w")
        self.tdelay_max_var = tk.StringVar(value=str(T_DELAY_DEFAULT_END_MS))
        ttk.Entry(cfg, textvariable=self.tdelay_max_var).grid(row=row, column=1, sticky="ew")

        row += 1
        self.repeat_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(cfg, text="Restart from tDelay=0 when finished", variable=self.repeat_var).grid(row=row, column=1, sticky="w")

        row += 1
        dbc_frame = ttk.Frame(cfg)
        dbc_frame.grid(row=row, column=1, sticky="ew")
        dbc_frame.columnconfigure(0, weight=1)
        ttk.Label(cfg, text="DBC file:").grid(row=row, column=0, sticky="w")
        self.dbc_path_var = tk.StringVar(value=DEFAULT_DBC_PATH)
        ttk.Entry(dbc_frame, textvariable=self.dbc_path_var).grid(row=0, column=0, sticky="ew")
        ttk.Button(dbc_frame, text="Browse", command=self.browse_dbc).grid(row=0, column=1, padx=(5, 0))
        ttk.Button(dbc_frame, text="Load", command=self.load_dbc_from_ui).grid(row=0, column=2, padx=(5, 0))

        row += 1
        ttk.Label(cfg, text="Selected msg:").grid(row=row, column=0, sticky="w")
        self.msg_var = tk.StringVar(value="-")
        ttk.Label(cfg, textvariable=self.msg_var).grid(row=row, column=1, sticky="w")

        row += 1
        ttk.Label(cfg, text="State:").grid(row=row, column=0, sticky="w")
        self.state_var = tk.StringVar(value="0 = idle")
        ttk.Label(cfg, textvariable=self.state_var).grid(row=row, column=1, sticky="w")

        row += 1
        ttk.Label(cfg, text="Error:").grid(row=row, column=0, sticky="w")
        self.error_var = tk.StringVar(value="0 = OK")
        ttk.Label(cfg, textvariable=self.error_var).grid(row=row, column=1, sticky="w")

        row += 1
        ttk.Label(cfg, text="Current tDelay:").grid(row=row, column=0, sticky="w")
        self.tdelay_var = tk.StringVar(value="0 ms")
        ttk.Label(cfg, textvariable=self.tdelay_var).grid(row=row, column=1, sticky="w")

        row += 1
        ttk.Label(cfg, text="Cycles done:").grid(row=row, column=0, sticky="w")
        self.cycles_var = tk.StringVar(value="0")
        ttk.Label(cfg, textvariable=self.cycles_var).grid(row=row, column=1, sticky="w")

        row += 1
        ttk.Label(cfg, text="Last TX:").grid(row=row, column=0, sticky="w")
        self.last_tx_var = tk.StringVar(value="-")
        ttk.Label(cfg, textvariable=self.last_tx_var).grid(row=row, column=1, sticky="w")

        button_frame = ttk.Frame(main)
        button_frame.grid(row=1, column=0, sticky="ew", pady=(10, 10))
        button_frame.columnconfigure(0, weight=1)
        button_frame.columnconfigure(1, weight=1)
        button_frame.columnconfigure(2, weight=1)

        self.start_button = ttk.Button(button_frame, text="Start CANFD", command=self.start_test)
        self.start_button.grid(row=0, column=0, sticky="ew", padx=(0, 5))

        self.once_button = ttk.Button(button_frame, text="CANFD Once", command=self.start_once)
        self.once_button.grid(row=0, column=1, sticky="ew", padx=(5, 5))

        self.stop_button = ttk.Button(button_frame, text="Stop", command=self.stop_test, state="disabled")
        self.stop_button.grid(row=0, column=2, sticky="ew", padx=(5, 0))

        log_frame = ttk.LabelFrame(main, text="Log", padding=10)
        log_frame.grid(row=2, column=0, sticky="nsew")
        main.rowconfigure(2, weight=1)
        main.columnconfigure(0, weight=1)

        self.log_text = tk.Text(log_frame, height=14, width=110, wrap="word")
        self.log_text.grid(row=0, column=0, sticky="nsew")
        log_scroll = ttk.Scrollbar(log_frame, orient="vertical", command=self.log_text.yview)
        log_scroll.grid(row=0, column=1, sticky="ns")
        self.log_text.configure(yscrollcommand=log_scroll.set)
        log_frame.rowconfigure(0, weight=1)
        log_frame.columnconfigure(0, weight=1)

    def browse_dbc(self):
        path = filedialog.askopenfilename(title="Select DBC", filetypes=[("DBC files", "*.dbc"), ("All files", "*.*")])
        if path:
            self.dbc_path_var.set(path)
            self.load_dbc_from_ui()

    def load_dbc_silent(self, path: str):
        try:
            self.load_dbc(path)
            self.log(f"DBC loaded: {path}")
        except Exception:
            self.message_name = FALLBACK_MSG_NAME
            self.message_id = FALLBACK_MSG_ID
            self.message_len = FALLBACK_MSG_LEN
            self.message_extended = FALLBACK_IS_EXTENDED
            self.message_data = FALLBACK_DATA
            self.update_msg_label()

    def load_dbc_from_ui(self):
        try:
            self.load_dbc(self.dbc_path_var.get().strip())
            self.set_error(ERR_NONE)
            self.log(f"DBC loaded: {self.dbc_path_var.get().strip()}")
        except Exception as exc:
            self.set_error(ERR_DBC)
            self.log(f"DBC load failed, using fallback {FALLBACK_MSG_NAME}: {exc}")
            self.message_name = FALLBACK_MSG_NAME
            self.message_id = FALLBACK_MSG_ID
            self.message_len = FALLBACK_MSG_LEN
            self.message_extended = FALLBACK_IS_EXTENDED
            self.message_data = FALLBACK_DATA
            self.update_msg_label()

    def load_dbc(self, path: str):
        name, msg_id, length, is_extended, data = parse_first_canfd_message_from_dbc(path)
        self.message_name = name
        self.message_id = msg_id
        self.message_len = length
        self.message_extended = is_extended
        self.message_data = data
        self.update_msg_label()

    def update_msg_label(self):
        ext = "EXT" if self.message_extended else "STD"
        self.msg_var.set(f"{self.message_name}, {ext} ID 0x{self.message_id:X}, len={self.message_len}, DLC={canfd_len_to_dlc(self.message_len)}")

    def set_state(self, value: int) -> None:
        with self.lock:
            self.state = value

    def set_error(self, value: int) -> None:
        with self.lock:
            self.error = value

    def set_tdelay(self, value_ms: int) -> None:
        with self.lock:
            self.current_t_delay_ms = value_ms

    def set_cycles(self, value: int) -> None:
        with self.lock:
            self.cycles_done = value

    def set_last_tx(self, text: str) -> None:
        with self.lock:
            self.last_tx = text

    def log(self, text: str) -> None:
        timestamp = time.strftime("%H:%M:%S")
        line = f"[{timestamp}] {text}\n"
        self.root.after(0, self._append_log, line)

    def _append_log(self, line: str) -> None:
        self.log_text.insert("end", line)
        self.log_text.see("end")

    def parse_int_field(self, var: tk.StringVar, name: str, minimum: int = 0, maximum: int = None) -> int:
        text = var.get().strip()
        if text == "":
            raise ValueError(f"{name} is empty")
        value = int(text, 0)
        if value < minimum:
            raise ValueError(f"{name} must be >= {minimum}")
        if maximum is not None and value > maximum:
            raise ValueError(f"{name} must be <= {maximum}")
        return value

    def get_config(self):
        app_name = self.app_name_var.get().strip()
        hw_name = self.hw_name_var.get().strip()
        if not app_name:
            raise ValueError("Application name is empty")
        if not hw_name:
            raise ValueError("Hardware name is empty")

        return {
            "app_name": app_name,
            "hw_name": hw_name,
            "app_channel": self.parse_int_field(self.app_channel_var, "App CH", 0, 31),
            "hw_channel": self.parse_int_field(self.hw_channel_var, "HW CH", 0, 31),
            "hw_index": self.parse_int_field(self.hw_index_var, "HW index", 0, 31),
            "device_subtype": self.parse_int_field(self.device_subtype_var, "Device subtype", 0, 255),
            "nominal_kbps": self.parse_int_field(self.nominal_kbps_var, "Nominal bitrate", 1, 10000),
            "data_kbps": self.parse_int_field(self.data_kbps_var, "Data bitrate", 1, 10000),
            "t_delay_max_ms": self.parse_int_field(self.tdelay_max_var, "tDelay_Max", 0, 24 * 60 * 60 * 1000),
            "repeat_when_finished": bool(self.repeat_var.get()),
            "termination_120r": bool(self.term_var.get()),
            "brs": bool(self.brs_var.get()),
        }

    def sleep_interruptible_ms(self, duration_ms: int) -> bool:
        end_time = time.monotonic() + (duration_ms / 1000.0)
        while time.monotonic() < end_time:
            if self.stop_event.is_set():
                return False
            remaining_s = end_time - time.monotonic()
            time.sleep(min(0.001, max(0.0, remaining_s)))
        return True

    def connect_backend(self, cfg):
        self.backend.connect(
            app_name=cfg["app_name"],
            hw_name=cfg["hw_name"],
            app_channel=cfg["app_channel"],
            hw_index=cfg["hw_index"],
            hw_channel=cfg["hw_channel"],
            device_type=DEFAULT_DEVICE_TYPE,
            device_subtype=cfg["device_subtype"],
            nominal_kbps=cfg["nominal_kbps"],
            data_kbps=cfg["data_kbps"],
            termination_120r=cfg["termination_120r"],
        )

    def transmit_action_once(self, cfg):
        self.backend.transmit_once(
            channel=cfg["app_channel"],
            msg_id=self.message_id,
            data=self.message_data,
            is_extended=self.message_extended,
            brs=cfg["brs"],
        )
        text = f"{self.message_name} ID=0x{self.message_id:X} len={self.message_len}"
        self.set_last_tx(text)
        self.log(f"TX CANFD {text}")

    def start_test(self):
        if self.worker is not None and self.worker.is_alive():
            return
        try:
            cfg = self.get_config()
        except Exception as exc:
            self.set_error(ERR_CONFIG)
            self.log(f"Invalid config: {exc}")
            return

        self.stop_event.clear()
        self.set_error(ERR_NONE)
        self.set_state(STATE_ONGOING)
        self.set_tdelay(T_DELAY_START_MS)
        self.set_cycles(0)
        self.set_last_tx("-")

        self.worker = threading.Thread(target=self.test_worker, args=(cfg, False), name="CanFdTDelayWorker", daemon=True)
        self.worker.start()
        self.start_button.configure(state="disabled")
        self.once_button.configure(state="disabled")
        self.stop_button.configure(state="normal")
        self.log("CANFD tDelay test started")

    def start_once(self):
        if self.worker is not None and self.worker.is_alive():
            return
        try:
            cfg = self.get_config()
        except Exception as exc:
            self.set_error(ERR_CONFIG)
            self.log(f"Invalid config: {exc}")
            return

        self.stop_event.clear()
        self.set_error(ERR_NONE)
        self.set_state(STATE_ONGOING)
        self.set_tdelay(0)
        self.set_cycles(0)
        self.set_last_tx("-")

        self.worker = threading.Thread(target=self.test_worker, args=(cfg, True), name="CanFdOnceWorker", daemon=True)
        self.worker.start()
        self.start_button.configure(state="disabled")
        self.once_button.configure(state="disabled")
        self.stop_button.configure(state="normal")
        self.log("CANFD once started")

    def stop_test(self):
        self.stop_event.set()
        self.log("Stop requested")

    def test_worker(self, cfg, once: bool):
        finished = False
        cycles = 0
        try:
            try:
                self.connect_backend(cfg)
                self.log(
                    f"Connected: app='{cfg['app_name']}', hw='{cfg['hw_name']}', "
                    f"appCH={cfg['app_channel']}, hwCH={cfg['hw_channel']}, "
                    f"{cfg['nominal_kbps']}/{cfg['data_kbps']} kbps"
                )
            except RuntimeError as exc:
                if "import" in str(exc).lower() or "TSMasterAPI" in str(exc):
                    self.set_error(ERR_API_IMPORT)
                else:
                    self.set_error(ERR_TSM_CONFIG_CONNECT)
                self.log(f"TSMaster connect/config failed: {exc}")
                return

            t_delay_ms = T_DELAY_START_MS
            while not self.stop_event.is_set():
                self.set_tdelay(t_delay_ms)
                try:
                    self.transmit_action_once(cfg)
                except Exception as exc:
                    self.set_error(ERR_CANFD_TX)
                    self.log(f"CANFD TX failed: {exc}")
                    break

                cycles += 1
                self.set_cycles(cycles)

                if once:
                    finished = True
                    break

                total_wait_ms = POST_ACTION_WAIT_MS + t_delay_ms

                if t_delay_ms >= cfg["t_delay_max_ms"]:
                    if cfg.get("repeat_when_finished", False):
                        self.log(
                            f"tDelay sweep finished at {t_delay_ms} ms; wait {total_wait_ms} ms, then restart from 0 ms"
                        )
                        if not self.sleep_interruptible_ms(total_wait_ms):
                            self.set_error(ERR_ABORTED)
                            break
                        t_delay_ms = T_DELAY_START_MS
                        continue

                    finished = True
                    break

                self.log(f"Wait {total_wait_ms} ms before next CANFD cycle")
                if not self.sleep_interruptible_ms(total_wait_ms):
                    self.set_error(ERR_ABORTED)
                    break

                t_delay_ms += T_DELAY_STEP_MS

        except Exception:
            self.set_error(ERR_EXCEPTION)
            self.log("Unhandled exception:")
            self.log(traceback.format_exc())
        finally:
            try:
                self.backend.disconnect(ignore_errors=True)
                self.log("Disconnected")
            except Exception:
                pass

            if finished:
                self.set_state(STATE_FINISHED)
                if once:
                    self.log("CANFD once finished")
                else:
                    self.log("CANFD tDelay test finished")
            else:
                self.set_state(STATE_IDLE)
                if self.error == ERR_NONE:
                    self.set_error(ERR_ABORTED)
                self.log("CANFD test stopped")

            self.root.after(0, self.on_worker_done)

    def on_worker_done(self):
        self.start_button.configure(state="normal")
        self.once_button.configure(state="normal")
        self.stop_button.configure(state="disabled")

    def update_ui_periodic(self):
        with self.lock:
            state = self.state
            error = self.error
            tdelay = self.current_t_delay_ms
            cycles = self.cycles_done
            last_tx = self.last_tx

        state_text = {
            STATE_IDLE: "0 = idle",
            STATE_ONGOING: "1 = ongoing",
            STATE_FINISHED: "2 = finished",
        }.get(state, f"{state} = unknown")

        error_text = {
            ERR_NONE: "0 = OK",
            ERR_API_IMPORT: "1 = TSMasterAPI import fail",
            ERR_TSM_CONFIG_CONNECT: "2 = TSMaster config/connect fail",
            ERR_CANFD_TX: "3 = CANFD TX fail",
            ERR_ABORTED: "4 = aborted/stopped",
            ERR_EXCEPTION: "5 = exception",
            ERR_CONFIG: "6 = invalid config",
            ERR_DBC: "7 = DBC parse/load fail",
        }.get(error, f"{error} = unknown")

        self.state_var.set(state_text)
        self.error_var.set(error_text)
        self.tdelay_var.set(f"{tdelay} ms")
        self.cycles_var.set(str(cycles))
        self.last_tx_var.set(last_tx)
        self.update_msg_label()

        self.root.after(100, self.update_ui_periodic)

    def on_close(self):
        if self.worker is not None and self.worker.is_alive():
            self.stop_event.set()
            try:
                self.backend.disconnect(ignore_errors=True)
            except Exception:
                pass
        self.root.destroy()


def main() -> int:
    root = tk.Tk()
    app = CanFdTDelayApp(root)
    root.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
