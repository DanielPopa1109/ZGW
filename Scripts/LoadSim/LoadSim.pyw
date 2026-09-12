# -*- coding: utf-8 -*-
from __future__ import annotations

def _hide_private_launcher_console() -> None:
    import os as _os
    import sys as _sys

    if _os.name != "nt" or "--keep-console" in _sys.argv:
        return

    try:
        import ctypes as _ctypes

        _kernel32 = _ctypes.WinDLL("kernel32", use_last_error=True)
        _user32 = _ctypes.WinDLL("user32", use_last_error=True)
        _hwnd = _kernel32.GetConsoleWindow()
        if _hwnd:
            _user32.ShowWindow(_hwnd, 0)
    except Exception:
        pass


_hide_private_launcher_console()

# -----------------------------------------------------------------------------
# Force 32-bit Python on Windows when this .pyw is double-clicked.
# TSMaster's Python API is typically WIN32-only, matching the existing
# CWT_CANFD_TC1012p tool in this repository.
# -----------------------------------------------------------------------------
def _restart_with_32bit_python_if_needed() -> None:
    import os as _os
    import struct as _struct
    import subprocess as _subprocess
    import sys as _sys

    if _os.name != "nt":
        return
    if (_struct.calcsize("P") * 8) == 32:
        return

    _script = _os.path.abspath(__file__)
    _args = _sys.argv[1:]
    _creationflags = 0
    if hasattr(_subprocess, "CREATE_NO_WINDOW"):
        _creationflags |= _subprocess.CREATE_NO_WINDOW
    if hasattr(_subprocess, "DETACHED_PROCESS"):
        _creationflags |= _subprocess.DETACHED_PROCESS

    _candidates = ["-3.11-32", "-3.12-32", "-3.10-32", "-3-32"]
    _check_code = "import struct; raise SystemExit(0 if struct.calcsize('P') * 8 == 32 else 1)"
    _selected = None
    _last_error = ""

    for _launcher in ("pyw", "py"):
        for _candidate in _candidates:
            try:
                _result = _subprocess.run(
                    [_launcher, _candidate, "-c", _check_code],
                    stdout=_subprocess.DEVNULL,
                    stderr=_subprocess.DEVNULL,
                    timeout=10,
                    creationflags=_creationflags,
                )
                if _result.returncode == 0:
                    _selected = [_launcher, _candidate]
                    break
            except Exception as _exc:
                _last_error = str(_exc)
        if _selected is not None:
            break

    if _selected is None:
        try:
            import tkinter as _tk
            from tkinter import messagebox as _messagebox

            _root = _tk.Tk()
            _root.withdraw()
            _messagebox.showerror(
                "32-bit Python required",
                "This tool must run with 32-bit Python because the TSMaster API is WIN32.\n\n"
                "Install 32-bit Python, for example:\n\n"
                "winget install -e --id Python.Python.3.11 --architecture x86\n\n"
                "Then install the required package:\n\n"
                "py -3.11-32 -m pip install tsmasterapi\n\n"
                f"Last launcher error: {_last_error or 'no 32-bit runtime found'}",
            )
            _root.destroy()
        finally:
            _sys.exit(1)

    try:
        _subprocess.Popen(
            _selected + [_script] + _args,
            cwd=_os.path.dirname(_script) or None,
            stdout=_subprocess.DEVNULL,
            stderr=_subprocess.DEVNULL,
            stdin=_subprocess.DEVNULL,
            creationflags=_creationflags,
        )
    except Exception as _exc:
        try:
            import tkinter as _tk
            from tkinter import messagebox as _messagebox

            _root = _tk.Tk()
            _root.withdraw()
            _messagebox.showerror(
                "32-bit Python launch failed",
                f"Failed to start this script with {' '.join(_selected)}.\n\n{_exc}",
            )
            _root.destroy()
        finally:
            _sys.exit(1)

    _sys.exit(0)


_restart_with_32bit_python_if_needed()

import math
import os
import random
import threading
import time
import traceback
import tkinter as tk
from dataclasses import dataclass
from tkinter import messagebox, scrolledtext, ttk


DEFAULT_APP_NAME = "TSMaster HW Tools"
DEFAULT_HW_NAME = "TC1012P"
DEFAULT_DEVICE_TYPE = 3
DEFAULT_DEVICE_SUBTYPE = 12
DEFAULT_HW_INDEX = 0
DEFAULT_FD_HW_CHANNEL = 0
DEFAULT_FD_APP_CHANNEL = 0
DEFAULT_NOMINAL_KBPS = 500
DEFAULT_DATA_KBPS = 2000
DEFAULT_CANFD_CONTROLLER_TYPE = 1
DEFAULT_CANFD_CONTROLLER_MODE = 0
DEFAULT_TERMINATION_120R = True
DEFAULT_BRS = True

MODEL_SAMPLE_MS = 5
MODEL_HISTORY_MS = 1000
MODEL_INFERENCE_MS = 20
MODEL_HORIZON_MS = 500

PDM1_LOADSTATUS_ID = 0x090
PDM1_CURRENT_IDS = (0x305, 0x306, 0x307, 0x308, 0x309)
PDM1_CURRENT_LENGTHS = (64, 64, 64, 64, 48)
PDM1_INPUTT30_ID = 0x30A
PDM1_INPUTT30_LEN = 2

NUM_CHANNELS = 75
ALL_CHANNELS_TARGET = 255
CHANNEL_RATINGS_A = [
    5.0, 7.5, 10.0, 12.5, 15.0, 17.5, 20.0, 22.5, 25.0, 27.5,
    30.0, 32.5, 35.0, 37.5, 40.0, 45.0, 50.0, 55.0, 60.0, 65.0,
    70.0, 75.0, 80.0, 85.0, 90.0, 95.0, 100.0, 105.0, 110.0, 115.0,
    120.0, 125.0, 130.0, 135.0, 140.0, 145.0, 150.0, 155.0, 160.0, 165.0,
    170.0, 175.0, 180.0, 185.0, 190.0, 195.0, 200.0, 205.0, 210.0, 215.0,
    220.0, 225.0, 230.0, 235.0, 240.0, 245.0, 248.0, 7.5, 10.0, 15.0,
    20.0, 25.0, 30.0, 40.0, 50.0, 60.0, 80.0, 100.0, 125.0, 150.0,
    175.0, 200.0, 225.0, 230.0, 250.0,
]

FAULT_CLASS_NAMES = (
    "normal",
    "impending_overcurrent",
    "impending_open_load",
    "impending_intermittent",
)

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


def clip(value: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, value))


def smoothstep(x: float) -> float:
    x = clip(x, 0.0, 1.0)
    return x * x * (3.0 - 2.0 * x)


def pack_u32_le(value: int) -> bytes:
    return int(value & 0xFFFFFFFF).to_bytes(4, "little")


def pack_u16_le(value: int) -> bytes:
    return int(value & 0xFFFF).to_bytes(2, "little")


def pack_current_feedback(currents_a: list[float], frame_index: int) -> bytes:
    start = frame_index * 16
    count = 16 if frame_index < 4 else 11
    payload = bytearray()
    for channel in range(start, start + count):
        raw = int(round(clip(currents_a[channel], 0.0, 250.0)))
        payload += pack_u32_le(raw)
    return bytes(payload).ljust(PDM1_CURRENT_LENGTHS[frame_index], b"\x00")


def pack_load_status(currents_a: list[float]) -> bytes:
    words = [0, 0, 0]
    for channel, current in enumerate(currents_a):
        if current >= 0.5:
            words[channel // 32] |= 1 << (channel % 32)
    return b"".join(pack_u32_le(word) for word in words)


def pack_input_t30(voltage_v: float) -> bytes:
    voltage_raw = int(round(clip(voltage_v, 0.0, 65535.0)))
    return pack_u16_le(voltage_raw).ljust(PDM1_INPUTT30_LEN, b"\x00")


@dataclass(frozen=True)
class Scenario:
    name: str
    description: str
    generator: object


class ScenarioRuntime:
    def __init__(self, target_channel: int, seed: int) -> None:
        self.seed = int(seed)
        self.rng = random.Random(seed)
        self.target_channel = None if target_channel == ALL_CHANNELS_TARGET else int(clip(target_channel, 0, NUM_CHANNELS - 1))
        self.target_channels = tuple(range(NUM_CHANNELS)) if self.target_channel is None else (self.target_channel,)
        self.base = [
            rating * self.rng.uniform(0.58, 0.82)
            for rating in CHANNEL_RATINGS_A
        ]
        self.parking_base = [
            rating * self.rng.uniform(0.0, 0.035)
            for rating in CHANNEL_RATINGS_A
        ]
        self.phase = [self.rng.uniform(0.0, 2.0 * math.pi) for _ in range(NUM_CHANNELS)]
        self.freq = [self.rng.uniform(0.35, 2.5) for _ in range(NUM_CHANNELS)]
        self.random_hold = [0.0 for _ in range(NUM_CHANNELS)]
        self.random_hold_step = -1


def _healthy_currents(state: ScenarioRuntime, t: float, load_scale: float = 1.0) -> list[float]:
    values = []
    for channel, rating in enumerate(CHANNEL_RATINGS_A):
        ripple = 0.025 * rating * math.sin((2.0 * math.pi * state.freq[channel] * t) + state.phase[channel])
        slow = 0.018 * rating * math.sin((2.0 * math.pi * 0.11 * t) + state.phase[channel] * 0.37)
        values.append(clip((state.base[channel] * load_scale) + ripple + slow, 0.0, min(250.0, rating * 0.88)))
    return values


def scenario_normal(t: float, step: int, state: ScenarioRuntime) -> tuple[float, list[float]]:
    voltage = 14.0 + 0.10 * math.sin(2.0 * math.pi * 0.25 * t)
    return voltage, _healthy_currents(state, t, 0.95)


def scenario_driving(t: float, step: int, state: ScenarioRuntime) -> tuple[float, list[float]]:
    voltage = 13.4 + 0.8 * smoothstep((t % 3.0) / 1.5)
    voltage += 0.25 * math.sin(2.0 * math.pi * 1.1 * t)

    values = _healthy_currents(
        state,
        t,
        0.78 + 0.08 * math.sin(2.0 * math.pi * 0.18 * t),
    )

    for channel in (2, 7, 14, 25, 41, 58, 66):
        if int(t * 1.2 + channel) % 4 in (0, 1):
            values[channel] += 0.06 * CHANNEL_RATINGS_A[channel]

    for channel, rating in enumerate(CHANNEL_RATINGS_A):
        values[channel] = clip(values[channel], 0.0, min(250.0, rating * 0.70))

    return voltage, values


def scenario_parking(t: float, step: int, state: ScenarioRuntime) -> tuple[float, list[float]]:
    voltage = 12.7 - min(0.6, 0.02 * t) + 0.03 * math.sin(2.0 * math.pi * 0.07 * t)
    values = []
    for channel, rating in enumerate(CHANNEL_RATINGS_A):
        keep_awake = channel in (0, 3, 10, 29, 57)
        base = state.parking_base[channel] if keep_awake else state.parking_base[channel] * 0.25
        pulse = 0.0
        if keep_awake and int(t * 0.5 + channel) % 7 == 0:
            pulse = 0.015 * rating
        values.append(clip(base + pulse, 0.0, 250.0))
    return voltage, values


def scenario_warning_high_load(t: float, step: int, state: ScenarioRuntime) -> tuple[float, list[float]]:
    voltage = 13.2 + 0.25 * math.sin(2.0 * math.pi * 0.3 * t)
    values = _healthy_currents(state, t, 1.05)
    channels = state.target_channels if state.target_channel is None else (state.target_channel, 20, 34, 60)
    for channel in channels:
        rating = CHANNEL_RATINGS_A[channel]
        values[channel] = clip(0.78 * rating + 0.08 * rating * math.sin(2.0 * math.pi * 0.8 * t), 0.0, 250.0)
    return voltage, values


def scenario_cranking_undervoltage(t: float, step: int, state: ScenarioRuntime) -> tuple[float, list[float]]:
    dip = smoothstep((t - 1.0) / 0.25) * (1.0 - smoothstep((t - 1.55) / 0.55))
    recovery = smoothstep((t - 2.1) / 1.2)
    voltage = 12.5 - 5.3 * dip + 1.8 * recovery
    values = _healthy_currents(state, t, 0.85)
    for channel in (1, 8, 12, 18, 28, 42):
        values[channel] = clip(values[channel] * (1.0 + 0.45 * dip), 0.0, 250.0)
    return voltage, values


def scenario_overvoltage_load_dump(t: float, step: int, state: ScenarioRuntime) -> tuple[float, list[float]]:
    spike = smoothstep((t - 1.2) / 0.3) * (1.0 - smoothstep((t - 2.0) / 0.7))
    voltage = 14.1 + 3.4 * spike + 0.15 * math.sin(2.0 * math.pi * 0.4 * t)
    values = _healthy_currents(state, t, 0.90 + 0.10 * spike)
    return voltage, values


def scenario_impending_overcurrent(t: float, step: int, state: ScenarioRuntime) -> tuple[float, list[float]]:
    voltage = 13.8 - 0.5 * smoothstep((t - 3.2) / 2.0)
    values = _healthy_currents(state, t, 0.82)
    ramp = smoothstep((t - 1.0) / 4.0)
    for channel in state.target_channels:
        rating = CHANNEL_RATINGS_A[channel]
        precursor = rating * (0.42 + 0.83 * ramp)
        values[channel] = clip(precursor + 0.025 * rating * math.sin(2.0 * math.pi * 2.0 * t), 0.0, 250.0)
    return voltage, values


def scenario_impending_open_load(t: float, step: int, state: ScenarioRuntime) -> tuple[float, list[float]]:
    voltage = 13.9 + 0.08 * math.sin(2.0 * math.pi * 0.2 * t)
    values = _healthy_currents(state, t, 0.92)
    fade = smoothstep((t - 1.2) / 3.2)
    for channel in state.target_channels:
        rating = CHANNEL_RATINGS_A[channel]
        values[channel] = clip((0.58 * rating * (1.0 - fade)) + (0.01 * rating * fade), 0.0, 250.0)
    return voltage, values


def scenario_impending_intermittent(t: float, step: int, state: ScenarioRuntime) -> tuple[float, list[float]]:
    voltage = 13.7 + 0.12 * math.sin(2.0 * math.pi * 0.55 * t)
    values = _healthy_currents(state, t, 0.88)
    severity = smoothstep((t - 1.0) / 4.0)
    for channel in state.target_channels:
        rating = CHANNEL_RATINGS_A[channel]
        wave = math.sin(2.0 * math.pi * (2.4 + 2.5 * severity) * t + state.phase[channel])
        dropout = severity > 0.05 and wave > (0.90 - 0.72 * severity)
        if dropout:
            values[channel] = clip(0.02 * rating * (1.0 - severity), 0.0, 250.0)
        else:
            values[channel] = clip(0.52 * rating + 0.05 * rating * math.sin(2.0 * math.pi * 0.9 * t), 0.0, 250.0)
    return voltage, values


def scenario_random_trace(t: float, step: int, state: ScenarioRuntime) -> tuple[float, list[float]]:
    voltage = 12.2 + 2.8 * (0.5 + 0.5 * math.sin(2.0 * math.pi * 0.09 * t + 0.3))
    voltage += 0.25 * math.sin(2.0 * math.pi * 1.7 * t)
    update_bucket = step // 10
    if update_bucket != state.random_hold_step:
        state.random_hold_step = update_bucket
        for channel, rating in enumerate(CHANNEL_RATINGS_A):
            state.random_hold[channel] = state.rng.uniform(-0.12 * rating, 0.16 * rating)
    values = _healthy_currents(state, t, 0.75 + 0.25 * math.sin(2.0 * math.pi * 0.05 * t))
    for channel, rating in enumerate(CHANNEL_RATINGS_A):
        values[channel] = clip(values[channel] + state.random_hold[channel], 0.0, min(250.0, rating * 0.95))
    return voltage, values


def scenario_short_fault(t: float, step: int, state: ScenarioRuntime) -> tuple[float, list[float]]:
    fault = smoothstep((t - 1.7) / 0.25)
    voltage = 13.6 - 2.0 * fault + 0.08 * math.sin(2.0 * math.pi * 0.4 * t)
    values = _healthy_currents(state, t, 0.80)
    for channel in state.target_channels:
        rating = CHANNEL_RATINGS_A[channel]
        values[channel] = clip((0.35 * rating * (1.0 - fault)) + (1.45 * rating * fault), 0.0, 250.0)
    return voltage, values


SCENARIOS = [
    Scenario("Normal", "Stable alternator voltage and healthy moderate PDM loads.", scenario_normal),
    Scenario("Driving", "Dynamic drive cycle with accessory steps and alternator regulation.", scenario_driving),
    Scenario("Parking", "Key-off style low-current parking loads and slow battery drift.", scenario_parking),
    Scenario("Warning High Load", "High utilization but below deterministic overcurrent.", scenario_warning_high_load),
    Scenario("Cranking Undervoltage", "Short voltage dip below the 9 V deterministic UV threshold.", scenario_cranking_undervoltage),
    Scenario("Overvoltage Load Dump", "Transient voltage above the 15 V deterministic OV threshold.", scenario_overvoltage_load_dump),
    Scenario("Impending Overcurrent", "Target channel ramps toward and then beyond its current rating.", scenario_impending_overcurrent),
    Scenario("Impending Open Load", "Target channel current decays toward open-load behavior.", scenario_impending_open_load),
    Scenario("Impending Intermittent", "Target channel develops repeated dropouts.", scenario_impending_intermittent),
    Scenario("Random Trace", "Seeded noisy trace with random but plausible load changes.", scenario_random_trace),
    Scenario("Short Fault", "Abrupt target-channel overload with supply sag.", scenario_short_fault),
]


class TSMasterBusBackend:
    def __init__(self) -> None:
        self.tsm = None
        self.connected = False

    def import_api(self) -> None:
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
            f"for the active 32-bit Python runtime. Last error: {last_error}"
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
    def _check_ret(ret, label: str, allow_none: bool = True) -> None:
        if ret is None and allow_none:
            return
        if ret not in (0, True, None):
            raise RuntimeError(f"{label} failed, return={ret!r}")

    def connect(self, cfg: dict[str, object]) -> None:
        self.import_api()
        self.disconnect(ignore_errors=True)

        app_name = str(cfg["app_name"])
        hw_name = str(cfg["hw_name"])
        fd_app_channel = int(cfg["fd_app_channel"])
        fd_hw_channel = int(cfg["fd_hw_channel"])

        ret = self._call_optional("initialize_lib_tsmaster", self._to_bytes(app_name))
        self._check_ret(ret, "initialize_lib_tsmaster")
        ret = self._call_optional("tsapp_disconnect")
        self._check_ret(ret, "tsapp_disconnect")
        ret = self._call_optional("tsapp_set_can_channel_count", fd_app_channel + 1)
        self._check_ret(ret, "tsapp_set_can_channel_count")
        self._check_ret(self._call_optional("tsapp_set_lin_channel_count", 0), "tsapp_set_lin_channel_count")
        self._check_ret(self._call_optional("tsapp_set_flexray_channel_count", 0), "tsapp_set_flexray_channel_count")

        self._map_channel(
            app_name,
            hw_name,
            fd_app_channel,
            int(cfg["hw_index"]),
            fd_hw_channel,
            int(cfg["device_subtype"]),
        )

        ret = self._call_optional(
            "tsapp_configure_baudrate_canfd",
            fd_app_channel,
            int(cfg["nominal_kbps"]),
            int(cfg["data_kbps"]),
            DEFAULT_CANFD_CONTROLLER_TYPE,
            DEFAULT_CANFD_CONTROLLER_MODE,
            bool(cfg["termination_120r"]),
        )
        self._check_ret(ret, "tsapp_configure_baudrate_canfd", allow_none=False)

        ret = self._call_optional("tsapp_connect")
        self._check_ret(ret, "tsapp_connect", allow_none=False)
        self.connected = True

    def _map_channel(
        self,
        app_name: str,
        hw_name: str,
        app_channel: int,
        hw_index: int,
        hw_channel: int,
        device_subtype: int,
    ) -> None:
        ret = self._call_optional(
            "tsapp_set_mapping_verbose",
            self._to_bytes(app_name),
            0,
            app_channel,
            self._to_bytes(hw_name),
            DEFAULT_DEVICE_TYPE,
            device_subtype,
            hw_index,
            hw_channel,
            True,
        )
        self._check_ret(ret, "tsapp_set_mapping_verbose")

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

        if hasattr(msg, "FIdxChn"):
            msg.FIdxChn = channel
        if hasattr(msg, "FIdentifier"):
            msg.FIdentifier = msg_id
        if hasattr(msg, "FDLC"):
            msg.FDLC = dlc
        if hasattr(msg, "FProperties"):
            msg.FProperties = 0x01 | (0x04 if is_extended else 0x00)
        if hasattr(msg, "FReserved"):
            msg.FReserved = 0
        if hasattr(msg, "FTimeUs"):
            msg.FTimeUs = 0
        if hasattr(msg, "FFDProperties"):
            msg.FFDProperties = 0x01 | (0x02 if brs else 0x00)
        self._fill_data(msg, padded_data)
        return msg

    @staticmethod
    def _fill_data(msg, data: bytes) -> None:
        if not hasattr(msg, "FData"):
            return
        for index, value in enumerate(data):
            try:
                msg.FData[index] = value
            except Exception:
                break

    def transmit_canfd(self, channel: int, msg_id: int, data: bytes, brs: bool) -> None:
        if not self.connected:
            raise RuntimeError("TSMaster is not connected")
        msg = self._make_canfd_msg(channel, msg_id, data, False, brs)
        func = getattr(self.tsm, "tsapp_transmit_canfd_async", None)
        if func is None:
            raise RuntimeError("tsapp_transmit_canfd_async not found in TSMasterAPI module")
        ret = func(msg)
        self._check_ret(ret, "tsapp_transmit_canfd_async", allow_none=True)

class LoadSimApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("LoadSim")
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        self.root.geometry("1040x720")
        self.backend = TSMasterBusBackend()
        self.worker: threading.Thread | None = None
        self.stop_event = threading.Event()

        self.app_name_var = tk.StringVar(value=DEFAULT_APP_NAME)
        self.hw_name_var = tk.StringVar(value=DEFAULT_HW_NAME)
        self.hw_index_var = tk.StringVar(value=str(DEFAULT_HW_INDEX))
        self.device_subtype_var = tk.StringVar(value=str(DEFAULT_DEVICE_SUBTYPE))
        self.fd_app_ch_var = tk.StringVar(value=str(DEFAULT_FD_APP_CHANNEL))
        self.fd_hw_ch_var = tk.StringVar(value=str(DEFAULT_FD_HW_CHANNEL))
        self.nominal_var = tk.StringVar(value=str(DEFAULT_NOMINAL_KBPS))
        self.data_var = tk.StringVar(value=str(DEFAULT_DATA_KBPS))
        self.period_var = tk.StringVar(value=str(MODEL_SAMPLE_MS))
        self.duration_var = tk.StringVar(value="8.0")
        self.seed_var = tk.StringVar(value="1234")
        self.target_channel_var = tk.StringVar(value="11")
        self.term_var = tk.BooleanVar(value=DEFAULT_TERMINATION_120R)
        self.brs_var = tk.BooleanVar(value=DEFAULT_BRS)
        self.send_load_status_var = tk.BooleanVar(value=True)
        self.loop_var = tk.BooleanVar(value=False)
        self.scenario_var = tk.StringVar(value=SCENARIOS[0].name)
        self.status_var = tk.StringVar(value="Idle")
        self.last_tx_var = tk.StringVar(value="-")
        self.currents_var = tk.StringVar(value="Currents: -")

        self._build_ui()

    def _build_ui(self) -> None:
        main = ttk.Frame(self.root, padding=10)
        main.pack(fill="both", expand=True)
        main.columnconfigure(0, weight=1)
        main.columnconfigure(1, weight=1)
        main.rowconfigure(3, weight=1)

        run = ttk.LabelFrame(main, text="Scenario")
        run.grid(row=0, column=0, columnspan=2, sticky="nsew", pady=(0, 8))
        for col in (1, 3, 5):
            run.columnconfigure(col, weight=1)

        ttk.Label(run, text="Scenario").grid(row=0, column=0, sticky="w", padx=4, pady=3)
        ttk.Combobox(
            run,
            textvariable=self.scenario_var,
            values=[scenario.name for scenario in SCENARIOS],
            state="readonly",
            width=26,
        ).grid(row=0, column=1, columnspan=5, sticky="ew", padx=4, pady=3)
        self._entry(run, 1, 0, "Period ms", self.period_var, 8)
        self._entry(run, 1, 2, "Duration s", self.duration_var, 8)
        self._entry(run, 1, 4, "Seed", self.seed_var, 8)
        self._entry(run, 2, 0, "Fault ch 1..75/255", self.target_channel_var, 8)
        ttk.Checkbutton(run, text="Loop", variable=self.loop_var).grid(row=2, column=2, sticky="w", padx=4, pady=3)
        ttk.Checkbutton(run, text="Send PDM1_LoadStatus 0x090", variable=self.send_load_status_var).grid(
            row=2, column=3, columnspan=3, sticky="w", padx=4, pady=3
        )
        ttk.Button(run, text="Start", command=self.start).grid(row=3, column=0, columnspan=2, sticky="ew", padx=4, pady=(8, 4))
        ttk.Button(run, text="Stop", command=self.stop).grid(row=3, column=2, columnspan=2, sticky="ew", padx=4, pady=(8, 4))
        ttk.Button(run, text="Clear Log", command=self.clear_log).grid(row=3, column=4, columnspan=2, sticky="ew", padx=4, pady=(8, 4))

        desc = ttk.LabelFrame(main, text="Available Scenarios")
        desc.grid(row=1, column=0, columnspan=2, sticky="nsew", pady=(0, 8))
        desc.columnconfigure(0, weight=1)
        self.scenario_table = ttk.Treeview(desc, columns=("name", "description"), show="headings", height=8)
        self.scenario_table.heading("name", text="Name")
        self.scenario_table.heading("description", text="Description")
        self.scenario_table.column("name", width=190, stretch=False)
        self.scenario_table.column("description", width=760, stretch=True)
        self.scenario_table.grid(row=0, column=0, sticky="nsew")
        scroll = ttk.Scrollbar(desc, orient="vertical", command=self.scenario_table.yview)
        scroll.grid(row=0, column=1, sticky="ns")
        self.scenario_table.configure(yscrollcommand=scroll.set)
        for scenario in SCENARIOS:
            self.scenario_table.insert("", "end", iid=scenario.name, values=(scenario.name, scenario.description))
        self.scenario_table.bind("<<TreeviewSelect>>", self._on_scenario_selected)

        status = ttk.Frame(main)
        status.grid(row=2, column=0, columnspan=2, sticky="ew", pady=(0, 8))
        status.columnconfigure(1, weight=1)
        status.columnconfigure(3, weight=1)
        ttk.Label(status, text="Status").grid(row=0, column=0, sticky="w", padx=4)
        ttk.Label(status, textvariable=self.status_var).grid(row=0, column=1, sticky="ew", padx=4)
        ttk.Label(status, text="Last TX").grid(row=0, column=2, sticky="w", padx=4)
        ttk.Label(status, textvariable=self.last_tx_var).grid(row=0, column=3, sticky="ew", padx=4)
        ttk.Label(
            status,
            textvariable=self.currents_var,
            font=("Consolas", 9),
            justify="left",
            anchor="w",
        ).grid(row=1, column=0, columnspan=4, sticky="ew", padx=4, pady=(3, 0))

        log_frame = ttk.LabelFrame(main, text="Log")
        log_frame.grid(row=3, column=0, columnspan=2, sticky="nsew")
        log_frame.columnconfigure(0, weight=1)
        log_frame.rowconfigure(0, weight=1)
        self.log_text = scrolledtext.ScrolledText(log_frame, height=12, wrap="word")
        self.log_text.grid(row=0, column=0, sticky="nsew")
        self.log(
            "The AI model needs 1.0 s of valid 5 ms samples before inference output becomes valid. "
            "Default scenario duration is 8 s to cover warm-up and response."
        )
        self.log(
            "PDM1 currents are sent on CAN FD IDs 0x305..0x309. Voltage is sent on CAN FD "
            "ID 0x30A as PDM1_InputT30.InputT30."
        )

    @staticmethod
    def _entry(parent, row: int, col: int, label: str, variable: tk.StringVar, width: int) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=col, sticky="w", padx=4, pady=3)
        ttk.Entry(parent, textvariable=variable, width=width).grid(row=row, column=col + 1, sticky="ew", padx=4, pady=3)

    def _on_scenario_selected(self, _event=None) -> None:
        selection = self.scenario_table.selection()
        if selection:
            self.scenario_var.set(selection[0])

    def log(self, text: str) -> None:
        timestamp = time.strftime("%H:%M:%S")
        self.log_text.insert("end", f"[{timestamp}] {text}\n")
        self.log_text.see("end")

    def log_threadsafe(self, text: str) -> None:
        self.root.after(0, lambda: self.log(text))

    def clear_log(self) -> None:
        self.log_text.delete("1.0", "end")

    @staticmethod
    def _parse_int(value: str, label: str, lo: int, hi: int) -> int:
        try:
            parsed = int(str(value).strip(), 0)
        except Exception:
            raise ValueError(f"{label} must be an integer")
        if parsed < lo or parsed > hi:
            raise ValueError(f"{label} must be in range {lo}..{hi}")
        return parsed

    @staticmethod
    def _parse_float(value: str, label: str, lo: float, hi: float) -> float:
        try:
            parsed = float(str(value).strip())
        except Exception:
            raise ValueError(f"{label} must be a number")
        if parsed < lo or parsed > hi:
            raise ValueError(f"{label} must be in range {lo:g}..{hi:g}")
        return parsed

    def get_config(self) -> dict[str, object]:
        app_name = self.app_name_var.get().strip()
        hw_name = self.hw_name_var.get().strip()
        if not app_name:
            raise ValueError("App name is empty")
        if not hw_name:
            raise ValueError("HW name is empty")

        scenario_name = self.scenario_var.get().strip()
        scenario = next((item for item in SCENARIOS if item.name == scenario_name), None)
        if scenario is None:
            raise ValueError("Select a valid scenario")

        target_channel_1_based = self._parse_int(self.target_channel_var.get(), "Fault channel", 1, ALL_CHANNELS_TARGET)
        if target_channel_1_based > NUM_CHANNELS and target_channel_1_based != ALL_CHANNELS_TARGET:
            raise ValueError(f"Fault channel must be in range 1..{NUM_CHANNELS}, or {ALL_CHANNELS_TARGET} for all channels")
        period_ms = self._parse_int(self.period_var.get(), "Period ms", 1, 1000)
        if period_ms != MODEL_SAMPLE_MS:
            self.log(
                f"Period is {period_ms} ms. The model was trained for {MODEL_SAMPLE_MS} ms samples, "
                "so use 5 ms when you need the closest training-profile replay."
            )

        return {
            "app_name": app_name,
            "hw_name": hw_name,
            "hw_index": self._parse_int(self.hw_index_var.get(), "HW index", 0, 31),
            "device_subtype": self._parse_int(self.device_subtype_var.get(), "Device subtype", 0, 255),
            "fd_app_channel": self._parse_int(self.fd_app_ch_var.get(), "FD App CH", 0, 31),
            "fd_hw_channel": self._parse_int(self.fd_hw_ch_var.get(), "FD HW CH", 0, 31),
            "nominal_kbps": self._parse_int(self.nominal_var.get(), "Nominal kbps", 1, 10000),
            "data_kbps": self._parse_int(self.data_var.get(), "Data kbps", 1, 10000),
            "period_ms": period_ms,
            "duration_s": self._parse_float(self.duration_var.get(), "Duration s", 0.0, 86400.0),
            "seed": self._parse_int(self.seed_var.get(), "Seed", 0, 2**31 - 1),
            "target_channel": ALL_CHANNELS_TARGET if target_channel_1_based == ALL_CHANNELS_TARGET else target_channel_1_based - 1,
            "termination_120r": bool(self.term_var.get()),
            "brs": bool(self.brs_var.get()),
            "send_load_status": bool(self.send_load_status_var.get()),
            "loop": bool(self.loop_var.get()),
            "scenario": scenario,
        }

    def start(self) -> None:
        if self.worker is not None and self.worker.is_alive():
            return
        try:
            cfg = self.get_config()
        except Exception as exc:
            messagebox.showerror("LoadSim configuration", str(exc))
            return

        self.stop_event.clear()
        self.worker = threading.Thread(target=self._run_worker, args=(cfg,), daemon=True)
        self.worker.start()

    def stop(self) -> None:
        self.stop_event.set()
        self.status_var.set("Stopping...")

    def _run_worker(self, cfg: dict[str, object]) -> None:
        scenario: Scenario = cfg["scenario"]
        runtime = ScenarioRuntime(int(cfg["target_channel"]), int(cfg["seed"]))
        duration_s = float(cfg["duration_s"])
        period_s = float(cfg["period_ms"]) / 1000.0
        fd_channel = int(cfg["fd_app_channel"])
        target_ch = int(cfg["target_channel"])
        target_all = target_ch == ALL_CHANNELS_TARGET
        tx_count = 0
        last_status_s = -999.0
        last_ui = 0.0

        try:
            self.root.after(0, lambda: self.status_var.set("Connecting..."))
            self.log_threadsafe(f"Connecting to {cfg['hw_name']} for scenario '{scenario.name}'")
            self.backend.connect(cfg)
            self.root.after(0, lambda: self.status_var.set(f"Running {scenario.name}"))
            if target_all:
                self.log_threadsafe(f"Started {scenario.name}; fault target all PDM1 current channels")
            else:
                self.log_threadsafe(
                    f"Started {scenario.name}; fault target PDM1_CurrFb_{target_ch + 1:03d}, "
                    f"rating={CHANNEL_RATINGS_A[target_ch]:g} A"
                )

            scenario_start = time.perf_counter()
            next_tick = scenario_start
            step = 0
            while not self.stop_event.is_set():
                now = time.perf_counter()
                if now < next_tick:
                    time.sleep(min(0.001, next_tick - now))
                    continue

                elapsed = now - scenario_start
                if duration_s > 0.0 and elapsed >= duration_s:
                    if bool(cfg["loop"]):
                        scenario_start = now
                        next_tick = now
                        step = 0
                        runtime = ScenarioRuntime(int(cfg["target_channel"]), int(cfg["seed"]))
                        last_status_s = -999.0
                        self.log_threadsafe(f"Looping scenario '{scenario.name}'")
                        continue
                    break

                voltage, currents = scenario.generator(elapsed, step, runtime)
                currents = [clip(float(value), 0.0, 250.0) for value in currents]
                voltage = clip(float(voltage), 0.0, 65535.0)

                self.backend.transmit_canfd(fd_channel, PDM1_INPUTT30_ID, pack_input_t30(voltage), bool(cfg["brs"]))
                tx_count += 1

                for frame_index, msg_id in enumerate(PDM1_CURRENT_IDS):
                    self.backend.transmit_canfd(
                        fd_channel,
                        msg_id,
                        pack_current_feedback(currents, frame_index),
                        bool(cfg["brs"]),
                    )
                    tx_count += 1

                if bool(cfg["send_load_status"]) and ((elapsed - last_status_s) >= 1.0):
                    self.backend.transmit_canfd(fd_channel, PDM1_LOADSTATUS_ID, pack_load_status(currents), bool(cfg["brs"]))
                    tx_count += 1
                    last_status_s = elapsed

                if (now - last_ui) >= 0.20:
                    currents_snapshot = tuple(currents)
                    self.root.after(
                        0,
                        lambda v=voltage, c=currents_snapshot, n=tx_count: self._set_live_status(
                            scenario.name,
                            v,
                            c,
                            n,
                            target_all,
                            target_ch,
                        ),
                    )
                    last_ui = now

                step += 1
                next_tick += period_s
                if next_tick < time.perf_counter() - 0.050:
                    next_tick = time.perf_counter()

            self.log_threadsafe(f"Stopped {scenario.name}; transmitted {tx_count} frames")
            self.root.after(0, lambda: self.status_var.set("Idle"))
        except Exception as exc:
            error_text = str(exc)
            details = "".join(traceback.format_exception(type(exc), exc, exc.__traceback__))
            self.log_threadsafe(details)
            self.root.after(0, lambda: self.status_var.set("Error"))
            self.root.after(0, lambda text=error_text: messagebox.showerror("LoadSim error", text))
        finally:
            try:
                self.backend.disconnect(ignore_errors=True)
            except Exception:
                pass

    def _set_live_status(
        self,
        scenario_name: str,
        voltage_v: float,
        currents_a: tuple[float, ...],
        tx_count: int,
        target_all: bool,
        target_ch: int,
    ) -> None:
        if target_all:
            target_text = f"max={max(currents_a):.1f} A"
        else:
            target_text = f"target CH{target_ch + 1:02d}={currents_a[target_ch]:.1f} A"

        self.status_var.set(f"Running {scenario_name}: Vin={voltage_v:.1f} V, {target_text}")
        self.last_tx_var.set(f"0x30A + 0x305..0x309, count={tx_count}")

        items = [f"{ch + 1:02d}:{current:5.1f}A" for ch, current in enumerate(currents_a)]
        rows = ["  ".join(items[start:start + 15]) for start in range(0, len(items), 15)]
        self.currents_var.set("Currents  " + "\n          ".join(rows))

    def on_close(self) -> None:
        self.stop_event.set()
        try:
            self.backend.disconnect(ignore_errors=True)
        except Exception:
            pass
        self.root.destroy()


def main() -> None:
    root = tk.Tk()
    try:
        style = ttk.Style(root)
        if "vista" in style.theme_names():
            style.theme_use("vista")
    except Exception:
        pass
    LoadSimApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
