import base64
import binascii
import json
import queue
import select
import socket
import struct
import threading
import time
import traceback
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, ttk


APP_NAME = "FCD"
APP_TITLE = "FCD - Flash Coding Diagnostics"
DEFAULT_HOST = "192.168.1.10"
DEFAULT_PORT = 13400
DEFAULT_SOURCE_ADDR = 0x0710
DEFAULT_TARGET_ADDR = 0x1001
DEFAULT_APP_START = 0xA0030000
DEFAULT_APP_END = 0xA07FFFFF
DEFAULT_BLOCK_SIZE = 512

# Minimum spacing between consecutive UDS requests on a connection. Requests are fully
# serialized (each waits for its response before the next is sent), and this enforces a
# 100 ms gap on top of that so the ZGW is never flooded back-to-back.
REQUEST_SPACING_SECONDS = 0.1

# Fault-memory detail readout can require hundreds of short 0x19 04/06 requests.
# Keep the normal conservative pacing for writes/programming, but use a tighter
# serialized gap for this read-only sequence.
FAULT_MEMORY_REQUEST_SPACING_SECONDS = 0.02
FAULT_MEMORY_DETAIL_TIMEOUT_SECONDS = 8.0

POST_RESET_RECONNECT_DELAY_SECONDS = 1.0
POST_RESET_UDS_READY_TIMEOUT_SECONDS = 30.0
POST_RESET_UDS_READY_RETRY_SECONDS = 0.5

# Lab ZGW CodingApp identifiers. These mirror APP/CodingApp/CodingApp.h.
CODING_DID_STATUS = 0xF1C0
CODING_DID_IMAGE = 0xF1C1
CODING_DID_RX_MESSAGE_EXPECTED = 0xF1C2
CODING_DID_VERSION = 0xF1C3
CODING_ROUTINE_VALIDATE = 0x0201
CODING_ROUTINE_WRITE_ALL = 0x0202
CODING_ROUTINE_READ_NVM = 0x0203
CODING_ROUTINE_LOAD_DEFAULTS = 0x0204
TIMESYNC_ROUTINE_SET_UTC = 0xF190
TIMESYNC_ROUTINE_GET_STATUS = 0xF191

# The CodingApp routine result is a 10-byte status block (status/state/validation/
# dirty/rxMessageExpectedCount[2]/generation[4]) followed by the coding bitmask -
# see CodingApp_FillRoutineResponse / the read-NVM routine in CodingApp.c.
CODING_ROUTINE_STATUS_LEN = 10
CODING_ROUTINE_STATUS_OK = 0x00
CODING_ROUTINE_STATUS_PENDING = 0x01
CODING_ROUTINE_STATUS_NOT_CHANGED = 0x03

CODING_ROUTINE_STATUS_TEXT = {
    0x00: "OK",
    0x01: "PENDING",
    0x02: "FAILED",
    0x03: "NOT_CHANGED",
}

CODING_STATE_TEXT = {
    0x00: "NOT_CODED",
    0x01: "CODED",
    0x02: "INVALID",
}

CODING_VALIDATION_TEXT = {
    0x00: "OK",
    0x01: "NOT_CODED",
    0x02: "BAD_MAGIC",
    0x03: "BAD_VERSION",
    0x04: "BAD_LENGTH",
    0x05: "BAD_MESSAGE_COUNT",
    0x06: "BAD_CRC",
    0x07: "NVM_ERROR",
}

CODING_NVM_JOB_TEXT = {
    0x00: "NONE",
    0x01: "WRITE_ALL",
    0x02: "READ_BLOCK",
}

NVM_RESULT_TEXT = {
    0x00: "OK",
    0x01: "NOT_OK",
    0x02: "PENDING",
    0x03: "INTEGRITY_FAILED",
    0x04: "BLOCK_SKIPPED",
    0x05: "NV_INVALIDATED",
    0x06: "RESTORED_FROM_ROM",
}

NVM_STATUS_TEXT = {
    0x00: "UNINIT",
    0x01: "IDLE",
    0x02: "BUSY",
    0x03: "BUSY_INTERNAL",
}

MEMIF_STATUS_TEXT = {
    0x00: "UNINIT",
    0x01: "IDLE",
    0x02: "BUSY",
    0x03: "BUSY_INTERNAL",
}

MEMIF_JOB_TEXT = {
    0x00: "OK",
    0x01: "FAILED",
    0x02: "PENDING",
    0x03: "CANCELED",
    0x04: "BLOCK_INCONSISTENT",
    0x05: "BLOCK_INVALID",
}


def _build_coding_parameter_names():
    """Ordered coding parameter names.

    The first mask bytes are the CodingApp rxMessageExpected bits, in the same
    order as GatewaySwc_RxMessageDiagRanges. The remaining bytes are the
    txPduEnabled bits indexed by COM TX PDU ID. XCP and diagnostic request TX
    PDUs are intentionally not exposed because CodingApp always treats them as
    enabled.
    """
    rx_names = [
        # CAN: COM_RX_PDU_CENTRALLOCKDATA .. COM_RX_PDU_DMU_ALIVE
        "CENTRALLOCKDATA", "LIGHTDATA1", "STATUSACTUATOR", "OUTSIDETEMPERATURESTATUS",
        "CENTRALCOMMAND1", "DMUSTATUS", "ENGINEDATA7", "BATTFULLSTAT", "ENGINEDATA6",
        "ENGINEDATA5", "ENGINEDATA4", "ENGINEDATA3", "ENGINEDATA2", "DSCDATA3", "DSCDATA2",
        "DSCDATA1", "ENGINEDATA1", "ASGDATA1", "PDCSTAT", "MILEAGE", "DMU_ALIVE",
        # CAN: COM_RX_PDU_VOLTAGECURRENT .. COM_RX_PDU_L1_I2T_COUNTER
        "VOLTAGECURRENT", "TEMPMEAS", "LOADSTATUS", "L1_I2T_COUNTER",
        # CAN: COM_RX_PDU_BATTSOCSOH .. COM_RX_PDU_BATTCAPRES
        "BATTSOCSOH", "BATTSOC", "BATTDIAGNOSIS", "BATTCURRENT", "BATTCAPDISCHARGE", "BATTCAPRES",
    ]
    # CAN-FD: COM_RX_PDU_CANFD_PDM4_LOADSTATUS .. COM_RX_PDU_CANFD_PDM4_TEMPERATUREFEEDBACK_5
    rx_names += ["CANFD_PDM4_LOADSTATUS", "CANFD_PDM3_LOADSTATUS",
                 "CANFD_PDM2_LOADSTATUS", "CANFD_PDM1_LOADSTATUS"]
    for pdm in (1, 2, 3, 4):
        rx_names += [f"CANFD_PDM{pdm}_VOLTAGEFEEDBACK_{i}" for i in range(1, 6)]
        rx_names += [f"CANFD_PDM{pdm}_CURRENTFEEDBACK_{i}" for i in range(1, 6)]
        rx_names += [f"CANFD_PDM{pdm}_STUCKATONEVENT", f"CANFD_PDM{pdm}_STUCKATOFFEVENT"]
        rx_names += [f"CANFD_PDM{pdm}_TEMPERATUREFEEDBACK_{i}" for i in range(1, 6)]
    # LIN: COM_RX_PDU_LIN_ALT_STATUS .. COM_RX_PDU_LIN_PCU48_STATUS
    rx_names += ["LIN_ALT_STATUS", "LIN_HVDCDC_STATUS", "LIN_PCU48_STATUS"]

    tx_names = {
        10: "TX_VEHICLESTATE",
        11: "TX_DISPLAYOUTTEMP",
        12: "TX_STATUSBODYDATA1",
        13: "TX_COMMANDDISPLAYSTATUS",
        19: "TX_SDAT",
        20: "TX_NM3",
        21: "TX_LOADREQUEST",
        100: "TX_CANFD_INFOTAINMENTDATA1",
        101: "TX_CANFD_ENERGYMANAGEMENTDATA2",
        102: "TX_CANFD_ENERGYMANAGEMENTDATA1",
        103: "TX_CANFD_VEHICLESTATE",
        104: "TX_CANFD_NM3",
        105: "TX_CANFD_SDAT",
        106: "TX_CANFD_LIGHTDATA1",
        107: "TX_CANFD_POWERTRAINDATA2",
        108: "TX_CANFD_POWERTRAINDATA1",
        109: "TX_CANFD_BODYDATA1",
        114: "TX_CANFD_COMMANDLOAD_PDM1",
        115: "TX_CANFD_COMMANDLOAD_PDM2",
        116: "TX_CANFD_COMMANDLOAD_PDM3",
        117: "TX_CANFD_COMMANDLOAD_PDM4",
        118: "TX_CANFD_ENERGYMANAGEMENTDATA3",
        200: "TX_LIN_ZGW_NM3",
        201: "TX_LIN_ZGW_REQUEST_ALT",
        202: "TX_LIN_ZGW_REQUEST_HVDCDC",
        203: "TX_LIN_ZGW_REQUEST_PCU48",
    }

    rx_mask_bytes = (len(rx_names) + 7) // 8
    tx_bit_offset = rx_mask_bytes * 8
    names = list(rx_names)
    names.extend(f"(reserved bit {idx})" for idx in range(len(names), tx_bit_offset + max(tx_names) + 1))
    for pdu_id, name in tx_names.items():
        names[tx_bit_offset + pdu_id] = name
    return names


CODING_PARAMETER_NAMES = _build_coding_parameter_names()
CODING_PARAMETER_INDEX = {name: idx for idx, name in enumerate(CODING_PARAMETER_NAMES)}
CODING_RX_PARAMETER_COUNT = len([name for name in CODING_PARAMETER_NAMES if not name.startswith("TX_") and not name.startswith("(reserved")])
CODING_RX_MASK_BYTES = (CODING_RX_PARAMETER_COUNT + 7) // 8
CODING_TX_PDU_MAX_ID = 203
CODING_MASK_BYTES = CODING_RX_MASK_BYTES + ((CODING_TX_PDU_MAX_ID + 8) // 8)

DTC_STATUS_TEXT = [
    (0x01, "testFailed"),
    (0x02, "testFailedThisOperationCycle"),
    (0x04, "pendingDTC"),
    (0x08, "confirmedDTC"),
    (0x10, "testNotCompletedSinceLastClear"),
    (0x20, "testFailedSinceLastClear"),
    (0x40, "testNotCompletedThisOperationCycle"),
    (0x80, "warningIndicatorRequested"),
]

DEM_DTC_MCUSM_SW_ERROR = 0x010101
DEM_DTC_CODING_ECU_NOT_CODED = 0x023000
DEM_DTC_CODING_INVALID = 0x023001
DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT = 0x021000
DEM_DTC_GATEWAY_RX_SIGNAL_INVALID = 0x022000
DEM_GATEWAY_RX_MESSAGE_EVENT_COUNT = CODING_RX_PARAMETER_COUNT

STATIC_DTC_DESCRIPTIONS = {
    DEM_DTC_MCUSM_SW_ERROR: "MCUSM software error",
    DEM_DTC_CODING_ECU_NOT_CODED: "Coding ECU not coded",
    DEM_DTC_CODING_INVALID: "Coding invalid",
}

# Plain-language maps used to decode the ZGW Dem freeze-frame (snapshot) and
# extended-data capture buffers. The byte layouts mirror the firmware capture
# callbacks and every multi-byte field is big-endian:
#   GatewaySwc_CaptureRxDiagFreezeFrame / ...ExtendedData (GatewaySwc.c)
#   SysMgr_CaptureMcuSmFreezeFrame      / ...ExtendedData (SysMgr.c)
#   Dem_DefaultFreezeFrameCapture       / ...Capture      (Dem_Cfg.c)
GATEWAY_BUS_TEXT = {1: "CAN", 2: "CAN-FD", 3: "LIN"}
GATEWAY_RX_DIAG_STATUS_TEXT = {0x00: "OK", 0x01: "TIMEOUT", 0x02: "INVALID"}
MCUSM_FAULT_SOURCE_BITS = [
    (0x01, "MCU reset latched"),
    (0x02, "SCR ECC double-bit error"),
    (0x04, "SCR watchdog"),
    (0x08, "SafetyKit failure"),
]
MCUSM_SCR_FAULT_PENDING_BITS = [
    (0x01, "ECC double-bit error"),
    (0x02, "watchdog"),
]
DEM_EVENT_ID_NAMES = {
    1: "MCUSM software error",
    2: "Coding ECU not coded",
    3: "Coding invalid",
}

# Lab DCM identifier. Mirrors DCM_ROUTINE_SELECT_FBL_INTERFACE in Dcm.c.
ROUTINE_SELECT_FBL_INTERFACE = 0x0205
FBL_INTERFACE_DOIP = 0x01
ROUTINE_ERASE_MEMORY = 0x0001
ROUTINE_CHECK_MEMORY_CRC = 0x0002
ROUTINE_START_FBL_RAM_UPDATER = 0x0155

DID_ACTIVE_SW_BLOCK = 0xF100
DID_APP_SW_VERSION = 0xF101
DID_ACTIVE_DIAG_SESSION = 0xF186

SESSION_DEFAULT = 0x01
SESSION_PROGRAMMING = 0x02
SESSION_EXTENDED = 0x03
SESSION_CODING_REQUESTED = 0x41


SCRIPT_DIR = Path(__file__).resolve().parent

DOIP_PROTO_VER = 0x02
DOIP_INV_PROTO_VER = 0xFD
DOIP_PT_VID_REQ = 0x0001
DOIP_PT_VID_RES = 0x0004
DOIP_PT_ROUTING_ACT_REQ = 0x0005
DOIP_PT_ROUTING_ACT_RES = 0x0006
DOIP_PT_ALIVE_CHECK_REQ = 0x0007
DOIP_PT_ALIVE_CHECK_RES = 0x0008
DOIP_PT_DIAG_MSG = 0x8001
DOIP_PT_DIAG_ACK = 0x8002
DOIP_PT_DIAG_NACK = 0x8003

NRC_TEXT = {
    0x10: "General Reject",
    0x11: "Service Not Supported",
    0x12: "SubFunction Not Supported",
    0x13: "Incorrect Message Length",
    0x22: "Conditions Not Correct",
    0x24: "Request Sequence Error",
    0x31: "Request Out Of Range",
    0x33: "Security Access Denied",
    0x35: "Invalid Key",
    0x36: "Exceeded Number Of Attempts",
    0x37: "Required Time Delay Not Expired",
    0x70: "Upload/Download Not Accepted",
    0x71: "Transfer Data Suspended",
    0x72: "General Programming Failure",
    0x73: "Wrong Block Sequence Counter",
    0x78: "Response Pending",
    0x7E: "SubFunction Not Supported In Active Session",
    0x7F: "Service Not Supported In Active Session",
}


class FcdError(Exception):
    pass


class DoipError(FcdError):
    pass


class NegativeResponse(FcdError):
    def __init__(self, sid, nrc):
        self.sid = sid
        self.nrc = nrc
        super().__init__(
            f"Negative response for 0x{sid:02X}: NRC 0x{nrc:02X} "
            f"({NRC_TEXT.get(nrc, 'Unknown')})"
        )


@dataclass
class HexSegment:
    address: int
    data: bytes

    @property
    def end(self):
        return self.address + len(self.data)

    @property
    def crc32(self):
        return binascii.crc32(self.data) & 0xFFFFFFFF


def now_text():
    return datetime.now().strftime("%H:%M:%S")


def parse_int(value, default=None):
    text = str(value).strip()
    if not text:
        if default is not None:
            return default
        raise ValueError("Missing numeric value")
    return int(text, 0)


def int_hex(value, width=4):
    return f"0x{int(value):0{width}X}"


def bytes_to_hex(data):
    return data.hex(" ").upper()

def u16_be(data, offset):
    return (data[offset] << 8) | data[offset + 1]

def u32_be(data, offset):
    return (
        (data[offset] << 24)
        | (data[offset + 1] << 16)
        | (data[offset + 2] << 8)
        | data[offset + 3]
    )


def flag_names(value, table, empty="none"):
    names = [name for mask, name in table if (value & mask) != 0]
    return ", ".join(names) if names else empty




def local_ipv4_addresses():
    ips = set()
    try:
        host_name = socket.gethostname()
        for info in socket.getaddrinfo(host_name, None, socket.AF_INET, socket.SOCK_DGRAM):
            ip = info[4][0]
            if ip and not ip.startswith("127."):
                ips.add(ip)
    except OSError:
        pass

    for target in ("192.168.1.10", "8.8.8.8"):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
                sock.connect((target, 9))
                ip = sock.getsockname()[0]
                if ip and not ip.startswith("127."):
                    ips.add(ip)
        except OSError:
            pass

    return ips


def same_ipv4_24(left, right):
    try:
        left_parts = [int(part) for part in str(left).split(".")]
        right_parts = [int(part) for part in str(right).split(".")]
    except ValueError:
        return False
    return len(left_parts) == 4 and len(right_parts) == 4 and left_parts[:3] == right_parts[:3]


def local_bind_candidates(target_host, preferred="auto"):
    text = str(preferred or "").strip()
    if text and text.lower() not in ("auto", "all", "any", "0.0.0.0"):
        return [text]

    candidates = [""]
    local_ips = sorted(local_ipv4_addresses(), key=lambda ip: (0 if same_ipv4_24(ip, target_host) else 1, ip))
    for ip in local_ips:
        if ip not in candidates:
            candidates.append(ip)
    return candidates


def is_own_udp_packet(addr):
    return addr and addr[0] in local_ipv4_addresses()

def hex_to_bytes(text):
    cleaned = (
        text.replace("0x", "")
        .replace("0X", "")
        .replace(",", " ")
        .replace(";", " ")
        .replace("\n", " ")
        .replace("\r", " ")
        .replace("\t", " ")
    )
    parts = [p for p in cleaned.split(" ") if p]
    if not parts and cleaned.strip():
        cleaned = cleaned.strip()
        if len(cleaned) % 2:
            raise ValueError("Hex string has an odd number of digits")
        return bytes.fromhex(cleaned)
    return bytes(int(p, 16) for p in parts)


def require_positive_response(response, request_sid):
    if not response:
        raise FcdError("Empty UDS response")
    if response[0] == 0x7F:
        if len(response) >= 3:
            raise NegativeResponse(response[1], response[2])
        raise FcdError("Malformed negative response")
    expected = (request_sid + 0x40) & 0xFF
    if response[0] != expected:
        raise FcdError(
            f"Unexpected positive SID 0x{response[0]:02X}; expected 0x{expected:02X}"
        )


def uds_response_matches_request(response, request):
    if not response or not request:
        return True

    sid = request[0]

    if response[0] == 0x7F:
        return len(response) >= 2 and response[1] == sid

    if response[0] != ((sid + 0x40) & 0xFF):
        return False

    if sid in (0x10, 0x11, 0x27, 0x28, 0x3E, 0x85):
        return len(request) < 2 or (len(response) >= 2 and response[1] == request[1])

    if sid == 0x19:
        if len(request) < 2:
            return True
        if len(response) < 2 or response[1] != request[1]:
            return False
        if request[1] in (0x04, 0x06):
            return len(request) < 5 or (len(response) >= 5 and response[2:5] == request[2:5])
        return True

    if sid in (0x22, 0x2E, 0x2F):
        return len(request) < 3 or (len(response) >= 3 and response[1:3] == request[1:3])

    if sid == 0x31:
        return len(request) < 4 or (
            len(response) >= 4
            and response[1] == request[1]
            and response[2:4] == request[2:4]
        )

    if sid == 0x36:
        return len(request) < 2 or (len(response) >= 2 and response[1] == request[1])

    return True


def project_lab_key(seed, level):
    key = seed ^ 0x6A09E667
    key = (key + 0x13572468 + ((level & 0xFF) * 0x1F3D5B79)) & 0xFFFFFFFF
    key = ((key << 3) | (key >> 29)) & 0xFFFFFFFF
    key ^= (((seed << 16) & 0xFFFFFFFF) | (seed >> 16))
    return key & 0xFFFFFFFF


def parse_intel_hex(path):
    path = Path(path)
    chunks = []
    base = 0

    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line_no, raw_line in enumerate(handle, start=1):
            line = raw_line.strip()
            if not line:
                continue
            if not line.startswith(":"):
                raise ValueError(f"{path.name}:{line_no}: Intel HEX line must start with ':'")

            try:
                raw = bytes.fromhex(line[1:])
            except ValueError as exc:
                raise ValueError(f"{path.name}:{line_no}: invalid HEX characters") from exc

            if len(raw) < 5:
                raise ValueError(f"{path.name}:{line_no}: record too short")

            count = raw[0]
            if len(raw) != count + 5:
                raise ValueError(f"{path.name}:{line_no}: byte count mismatch")

            if (sum(raw) & 0xFF) != 0:
                raise ValueError(f"{path.name}:{line_no}: checksum mismatch")

            offset = (raw[1] << 8) | raw[2]
            rectype = raw[3]
            data = raw[4 : 4 + count]

            if rectype == 0x00:
                chunks.append((base + offset, data))
            elif rectype == 0x01:
                break
            elif rectype == 0x02:
                if count != 2:
                    raise ValueError(f"{path.name}:{line_no}: invalid segment base record")
                base = (((data[0] << 8) | data[1]) << 4) & 0xFFFFFFFF
            elif rectype == 0x04:
                if count != 2:
                    raise ValueError(f"{path.name}:{line_no}: invalid linear base record")
                base = (((data[0] << 8) | data[1]) << 16) & 0xFFFFFFFF
            elif rectype in (0x03, 0x05):
                continue
            else:
                raise ValueError(f"{path.name}:{line_no}: unsupported record type 0x{rectype:02X}")

    if not chunks:
        return []

    chunks.sort(key=lambda item: item[0])
    segments = []
    cur_start = None
    cur_data = bytearray()

    for start, data in chunks:
        if cur_start is None:
            cur_start = start
            cur_data = bytearray(data)
            continue

        cur_end = cur_start + len(cur_data)
        if start <= cur_end:
            offset = start - cur_start
            needed = offset + len(data) - len(cur_data)
            if needed > 0:
                cur_data.extend(b"\xFF" * needed)
            cur_data[offset : offset + len(data)] = data
        else:
            segments.append(HexSegment(cur_start, bytes(cur_data)))
            cur_start = start
            cur_data = bytearray(data)

    segments.append(HexSegment(cur_start, bytes(cur_data)))
    return segments


def download_address_from_hex_address(address):
    address = int(address)
    if 0x80000000 <= address < 0xA0000000:
        return address + 0x20000000
    return address


def hex_source_base(segments):
    if not segments:
        raise ValueError("No HEX data segments")
    return min(seg.address for seg in segments) & 0xFFFF0000


def hex_download_base(segments):
    return download_address_from_hex_address(hex_source_base(segments))


class DoipClient:
    def __init__(self, host, port, source_addr, target_addr, timeout=3.0, local_ip="auto"):
        self.host = host
        self.port = int(port)
        self.source_addr = int(source_addr)
        self.target_addr = int(target_addr)
        self.timeout = float(timeout)
        self.local_ip = str(local_ip or "auto").strip()
        self.bound_local_ip = ""
        self.sock = None
        self.lock = threading.Lock()
        self._last_request_ts = 0.0
        self.request_spacing_seconds = REQUEST_SPACING_SECONDS

    @property
    def connected(self):
        return self.sock is not None

    def _pace(self):
        # Enforce the minimum gap since the previous request was issued on this socket.
        # Called inside self.lock so it also serializes concurrent callers.
        wait = self.request_spacing_seconds - (time.monotonic() - self._last_request_ts)
        if wait > 0:
            time.sleep(wait)
        self._last_request_ts = time.monotonic()

    def drain(self):
        # Discard any frames left unread in the socket buffer (e.g. a stale response
        # after a reset or a race) so the next request reads its own reply.
        with self.lock:
            if self.sock is None:
                return
            try:
                self.sock.setblocking(False)
                while True:
                    try:
                        if not self.sock.recv(4096):
                            break
                    except (BlockingIOError, OSError):
                        break
            finally:
                if self.sock is not None:
                    self.sock.setblocking(True)
                    self.sock.settimeout(self.timeout)

    def connect(self):
        self.close()
        last_error = None
        for bind_ip in local_bind_candidates(self.host, self.local_ip):
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            try:
                sock.settimeout(self.timeout)
                if bind_ip:
                    sock.bind((bind_ip, 0))
                sock.connect((self.host, self.port))
                sock.settimeout(self.timeout)
                self.sock = sock
                self.bound_local_ip = bind_ip
                return
            except OSError as exc:
                last_error = exc
                try:
                    sock.close()
                except OSError:
                    pass
        if last_error is not None:
            raise last_error
        raise TimeoutError("No local TCP bind candidates available")

    def close(self):
        sock = self.sock
        self.sock = None
        if sock is not None:
            try:
                sock.close()
            except OSError:
                pass

    def _send_frame(self, payload_type, payload=b""):
        if self.sock is None:
            raise DoipError("Not connected")
        header = struct.pack(">BBHI", DOIP_PROTO_VER, DOIP_INV_PROTO_VER, payload_type, len(payload))
        try:
            self.sock.sendall(header + payload)
        except OSError:
            self.close()
            raise

    def _recv_exact(self, length, deadline=None):
        if self.sock is None:
            raise DoipError("Not connected")
        out = bytearray()
        try:
            while len(out) < length:
                if deadline is not None:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise TimeoutError("Timed out waiting for DoIP data")
                    self.sock.settimeout(remaining)
                else:
                    self.sock.settimeout(self.timeout)
                try:
                    chunk = self.sock.recv(length - len(out))
                except TimeoutError as exc:
                    raise TimeoutError("Timed out waiting for DoIP data")
                except OSError:
                    self.close()
                    raise
                if not chunk:
                    self.close()
                    raise DoipError("TCP connection closed")
                out.extend(chunk)
        finally:
            if self.sock is not None:
                self.sock.settimeout(self.timeout)
        return bytes(out)

    def _recv_frame(self, deadline=None):
        header = self._recv_exact(8, deadline)
        proto, inv, payload_type, payload_len = struct.unpack(">BBHI", header)
        if proto != DOIP_PROTO_VER or inv != DOIP_INV_PROTO_VER:
            raise DoipError(f"Invalid DoIP header {proto:02X} {inv:02X}")
        if payload_len > 1024 * 1024:
            raise DoipError(f"DoIP payload too large: {payload_len}")
        payload = self._recv_exact(payload_len, deadline) if payload_len else b""
        return payload_type, payload

    def routing_activation(self, activation_type=0x00):
        payload = struct.pack(">HB4s", self.source_addr, activation_type, b"\x00\x00\x00\x00")
        with self.lock:
            self._send_frame(DOIP_PT_ROUTING_ACT_REQ, payload)
            payload_type, response = self._recv_frame(time.monotonic() + self.timeout)
        if payload_type != DOIP_PT_ROUTING_ACT_RES or len(response) < 5:
            raise DoipError(f"Unexpected routing activation response 0x{payload_type:04X}")
        tester, ecu, code = struct.unpack(">HHB", response[:5])
        if tester != self.source_addr:
            raise DoipError(f"Routing response for source 0x{tester:04X}, expected 0x{self.source_addr:04X}")
        if code != 0x10:
            raise DoipError(f"Routing activation denied: 0x{code:02X}")
        return ecu, code

    def alive_check(self):
        with self.lock:
            self._send_frame(DOIP_PT_ALIVE_CHECK_REQ, b"")
            payload_type, response = self._recv_frame(time.monotonic() + self.timeout)
        if payload_type != DOIP_PT_ALIVE_CHECK_RES or len(response) < 2:
            raise DoipError("Unexpected alive-check response")
        return struct.unpack(">H", response[:2])[0]

    def send_uds(self, request, timeout=None):
        timeout = self.timeout if timeout is None else float(timeout)
        payload = struct.pack(">HH", self.source_addr, self.target_addr) + bytes(request)
        deadline = time.monotonic() + timeout

        with self.lock:
            self._pace()
            self._send_frame(DOIP_PT_DIAG_MSG, payload)
            while True:
                payload_type, response = self._recv_frame(deadline)
                if payload_type == DOIP_PT_ALIVE_CHECK_REQ:
                    self._send_frame(DOIP_PT_ALIVE_CHECK_RES, struct.pack(">H", self.source_addr))
                    continue
                if payload_type == DOIP_PT_DIAG_ACK:
                    if len(response) >= 5 and response[4] != 0:
                        raise DoipError(f"Diagnostic ACK code 0x{response[4]:02X}")
                    continue
                if payload_type == DOIP_PT_DIAG_NACK:
                    code = response[4] if len(response) >= 5 else 0xFF
                    raise DoipError(f"Diagnostic NACK code 0x{code:02X}")
                if payload_type != DOIP_PT_DIAG_MSG:
                    continue
                if len(response) < 5:
                    raise DoipError("Short diagnostic message")

                source, target = struct.unpack(">HH", response[:4])
                uds = response[4:]
                if target != self.source_addr:
                    raise DoipError(f"Diagnostic response target 0x{target:04X} does not match tester")
                if source != self.target_addr:
                    raise DoipError(f"Diagnostic response source 0x{source:04X} does not match target")

                if not uds_response_matches_request(uds, request):
                    continue

                if len(uds) >= 3 and uds[0] == 0x7F and uds[2] == 0x78:
                    deadline = time.monotonic() + timeout
                    continue
                return uds

    def send_uds_suppress_positive(self, request, timeout=None):
        timeout = self.timeout if timeout is None else float(timeout)
        payload = struct.pack(">HH", self.source_addr, self.target_addr) + bytes(request)
        deadline = time.monotonic() + timeout

        with self.lock:
            self._pace()
            self._send_frame(DOIP_PT_DIAG_MSG, payload)
            while True:
                payload_type, response = self._recv_frame(deadline)
                if payload_type == DOIP_PT_ALIVE_CHECK_REQ:
                    self._send_frame(DOIP_PT_ALIVE_CHECK_RES, struct.pack(">H", self.source_addr))
                    continue
                if payload_type == DOIP_PT_DIAG_ACK:
                    if len(response) >= 5 and response[4] != 0:
                        raise DoipError(f"Diagnostic ACK code 0x{response[4]:02X}")
                    return None
                if payload_type == DOIP_PT_DIAG_NACK:
                    code = response[4] if len(response) >= 5 else 0xFF
                    raise DoipError(f"Diagnostic NACK code 0x{code:02X}")
                if payload_type != DOIP_PT_DIAG_MSG:
                    continue
                if len(response) < 5:
                    raise DoipError("Short diagnostic message")

                source, target = struct.unpack(">HH", response[:4])
                uds = response[4:]
                if target != self.source_addr:
                    raise DoipError(f"Diagnostic response target 0x{target:04X} does not match tester")
                if source != self.target_addr:
                    raise DoipError(f"Diagnostic response source 0x{source:04X} does not match target")
                return uds

    @staticmethod
    def vehicle_identification(host, port=DEFAULT_PORT, timeout=2.0, local_ip="auto"):
        req = struct.pack(">BBHI", DOIP_PROTO_VER, DOIP_INV_PROTO_VER, DOIP_PT_VID_REQ, 0)
        host = str(host).strip()
        port = int(port)
        deadline = time.monotonic() + float(timeout)
        local_ips = local_ipv4_addresses()

        sockets = []

        def make_sock(bind_port=None, bind_ip=""):
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.setblocking(False)
            if bind_port is not None:
                try:
                    sock.bind((bind_ip, int(bind_port)))
                except OSError:
                    sock.bind((bind_ip, 0))
            else:
                sock.bind((bind_ip, 0))
            sockets.append(sock)
            return sock

        try:
            # Some DoIP stacks answer to the request source port, some lab stacks
            # are easier to test when the tester also listens on 13400. Use both.
            make_sock(port)
            for bind_ip in local_bind_candidates(host, local_ip):
                sock = make_sock(None, bind_ip)
                try:
                    sock.sendto(req, (host, port))
                except OSError:
                    pass

            while time.monotonic() < deadline:
                remaining = max(0.01, deadline - time.monotonic())
                readable, _, _ = select.select(sockets, [], [], remaining)
                if not readable:
                    continue

                for sock in readable:
                    while True:
                        try:
                            data, addr = sock.recvfrom(4096)
                        except BlockingIOError:
                            break
                        except OSError:
                            break

                        if addr[0] in local_ips:
                            continue

                        try:
                            return DoipClient._parse_vehicle_identification(data, addr)
                        except DoipError:
                            continue

            raise TimeoutError(f"No DoIP vehicle-identification response from {host}:{port}")

        finally:
            for sock in sockets:
                try:
                    sock.close()
                except OSError:
                    pass

    @staticmethod
    def discover(port=DEFAULT_PORT, timeout=2.0, extra_hosts=None, local_ip="auto", include_broadcast=True):
        req = struct.pack(">BBHI", DOIP_PROTO_VER, DOIP_INV_PROTO_VER, DOIP_PT_VID_REQ, 0)
        port = int(port)
        targets = []
        candidate_targets = [*(extra_hosts or []), "192.168.1.10"]
        if include_broadcast:
            candidate_targets.extend(["192.168.1.255", "255.255.255.255"])
        for target in candidate_targets:
            target = str(target).strip()
            if target and target not in targets:
                targets.append(target)

        local_ips = local_ipv4_addresses()
        found = {}
        sockets = []

        def make_sock(bind_port=None, bind_ip=""):
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.setblocking(False)
            if bind_port is not None:
                try:
                    sock.bind((bind_ip, int(bind_port)))
                except OSError:
                    sock.bind((bind_ip, 0))
            else:
                sock.bind((bind_ip, 0))
            sockets.append(sock)
            return sock

        try:
            make_sock(port)
            tx_sockets = []
            for bind_ip in local_bind_candidates(targets[0] if targets else DEFAULT_HOST, local_ip):
                tx_sockets.append(make_sock(None, bind_ip))

            for sock in tx_sockets:
                for target in targets:
                    try:
                        sock.sendto(req, (target, port))
                    except OSError:
                        continue

            deadline = time.monotonic() + float(timeout)
            while time.monotonic() < deadline:
                remaining = max(0.01, deadline - time.monotonic())
                readable, _, _ = select.select(sockets, [], [], remaining)
                if not readable:
                    continue

                for sock in readable:
                    while True:
                        try:
                            data, addr = sock.recvfrom(4096)
                        except BlockingIOError:
                            break
                        except OSError:
                            break

                        if addr[0] in local_ips:
                            continue

                        try:
                            info = DoipClient._parse_vehicle_identification(data, addr)
                            found[info["ip"]] = info
                        except DoipError:
                            continue

            return list(found.values())

        finally:
            for sock in sockets:
                try:
                    sock.close()
                except OSError:
                    pass

    @staticmethod
    def _parse_vehicle_identification(data, addr):
        if len(data) < 8:
            raise DoipError("Short DoIP vehicle identification response")
        proto, inv, payload_type, payload_len = struct.unpack(">BBHI", data[:8])
        if proto != DOIP_PROTO_VER or inv != DOIP_INV_PROTO_VER or payload_type != DOIP_PT_VID_RES:
            raise DoipError("Unexpected DoIP vehicle identification response")
        payload = data[8 : 8 + payload_len]
        if len(payload) < 31:
            raise DoipError("Short vehicle identification payload")
        vin = payload[:17].decode("ascii", errors="replace").strip("\x00 ")
        logical_addr = struct.unpack(">H", payload[17:19])[0]
        eid = payload[19:25].hex(":").upper()
        gid = payload[25:31].hex(":").upper()
        return {
            "from": f"{addr[0]}:{addr[1]}",
            "ip": addr[0],
            "vin": vin,
            "logical_address": logical_addr,
            "eid": eid,
            "gid": gid,
        }

    @staticmethod
    def ethernet_probe(host, doip_port=DEFAULT_PORT, timeout=3.0, local_ip="auto"):
        doip_req = struct.pack(">BBHI", DOIP_PROTO_VER, DOIP_INV_PROTO_VER, DOIP_PT_VID_REQ, 0)
        heartbeat = b"PCHeartbeat"
        doip_port = int(doip_port)

        targets = []
        for target in [host, "192.168.1.10", "192.168.1.255", "255.255.255.255"]:
            target = str(target).strip()
            if target and target not in targets:
                targets.append(target)

        local_ips = local_ipv4_addresses()
        sockets = []
        results = []

        def add_socket(bind_port=None, bind_ip=""):
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.setblocking(False)
            if bind_port is not None:
                try:
                    sock.bind((bind_ip, int(bind_port)))
                except OSError as exc:
                    results.append({
                        "kind": "bind-error",
                        "from": f"{bind_ip or '0.0.0.0'}:{bind_port}",
                        "detail": str(exc),
                    })
                    sock.bind((bind_ip, 0))
            else:
                sock.bind((bind_ip, 0))
            sockets.append(sock)
            return sock

        # Bind RX before TX. Keep one ephemeral socket too, because the ECU may reply
        # to the tester source port instead of fixed 13400.
        rx_ports = []
        for p in (doip_port, 30490, 30500, 30600):
            if p not in rx_ports:
                rx_ports.append(p)
        for p in rx_ports:
            add_socket(p)
        tx_sockets = []
        for bind_ip in local_bind_candidates(host or DEFAULT_HOST, local_ip):
            tx_sockets.append(add_socket(None, bind_ip))

        for target in targets:
            for tx_sock in tx_sockets:
                local = tx_sock.getsockname()[0]
                for port, payload in ((30600, heartbeat), (doip_port, heartbeat), (doip_port, doip_req)):
                    try:
                        tx_sock.sendto(payload, (target, int(port)))
                    except OSError as exc:
                        results.append({
                            "kind": "send-error",
                            "from": f"{local}->{target}:{port}",
                            "detail": str(exc),
                        })

        deadline = time.monotonic() + float(timeout)

        try:
            while time.monotonic() < deadline:
                remaining = max(0.01, deadline - time.monotonic())
                readable, _, _ = select.select(sockets, [], [], remaining)
                if not readable:
                    continue

                for sock in readable:
                    while True:
                        try:
                            data, addr = sock.recvfrom(4096)
                        except BlockingIOError:
                            break
                        except OSError:
                            break

                        if addr[0] in local_ips:
                            # Ignore our own broadcast packets. They previously looked like
                            # false DoIP 0x0001 responses in the log.
                            continue

                        item = {
                            "kind": "udp",
                            "from": f"{addr[0]}:{addr[1]}",
                            "ip": addr[0],
                            "length": len(data),
                            "data": bytes_to_hex(data[:32]),
                        }

                        if data == b"AURIXHeartbeatAck":
                            item["kind"] = "aurix-heartbeat"

                        elif len(data) >= 8:
                            proto, inv, payload_type, payload_len = struct.unpack(">BBHI", data[:8])
                            if proto == DOIP_PROTO_VER and inv == DOIP_INV_PROTO_VER:
                                item["kind"] = f"doip-0x{payload_type:04X}"
                                item["payload_len"] = payload_len
                                if payload_type == DOIP_PT_VID_RES:
                                    try:
                                        item.update(DoipClient._parse_vehicle_identification(data, addr))
                                    except DoipError as exc:
                                        item["detail"] = str(exc)

                        results.append(item)

        finally:
            for sock in sockets:
                try:
                    sock.close()
                except OSError:
                    pass

        return results


class RawTcpUdsClient:
    def __init__(self, host, port, timeout=3.0, local_ip="auto"):
        self.host = host
        self.port = int(port)
        self.timeout = float(timeout)
        self.local_ip = str(local_ip or "auto").strip()
        self.bound_local_ip = ""
        self.sock = None
        self.lock = threading.Lock()
        self._last_request_ts = 0.0
        self.request_spacing_seconds = REQUEST_SPACING_SECONDS

    @property
    def connected(self):
        return self.sock is not None

    def _pace(self):
        wait = self.request_spacing_seconds - (time.monotonic() - self._last_request_ts)
        if wait > 0:
            time.sleep(wait)
        self._last_request_ts = time.monotonic()

    def drain(self):
        with self.lock:
            if self.sock is None:
                return
            try:
                self.sock.setblocking(False)
                while True:
                    try:
                        if not self.sock.recv(4096):
                            break
                    except (BlockingIOError, OSError):
                        break
            finally:
                if self.sock is not None:
                    self.sock.setblocking(True)
                    self.sock.settimeout(self.timeout)

    def connect(self):
        self.close()
        last_error = None
        for bind_ip in local_bind_candidates(self.host, self.local_ip):
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            try:
                sock.settimeout(self.timeout)
                if bind_ip:
                    sock.bind((bind_ip, 0))
                sock.connect((self.host, self.port))
                sock.settimeout(self.timeout)
                self.sock = sock
                self.bound_local_ip = bind_ip
                return
            except OSError as exc:
                last_error = exc
                try:
                    sock.close()
                except OSError:
                    pass
        if last_error is not None:
            raise last_error
        raise TimeoutError("No local TCP bind candidates available")

    def close(self):
        sock = self.sock
        self.sock = None
        if sock is not None:
            try:
                sock.close()
            except OSError:
                pass

    def send_uds(self, request, timeout=None):
        if self.sock is None:
            raise FcdError("Not connected")
        with self.lock:
            self._pace()
            self.sock.settimeout(self.timeout if timeout is None else timeout)
            self.sock.sendall(bytes(request))
            return self.sock.recv(4096)


class FcdApp:
    def __init__(self, root):
        self.root = root
        self.root.title(APP_TITLE)
        self.root.geometry("1280x820")
        self.root.minsize(1020, 680)

        self.log_queue = queue.Queue()
        self.client = None
        self.keepalive_stop = threading.Event()
        self.keepalive_thread = None
        self.worker_stop = threading.Event()
        self.generator_rows = {}
        self.package_dir = None
        self.package_payloads = []
        self.payload_enabled = {}

        self._build_style()
        self._build_ui()
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)
        self.root.after(100, self._drain_log_queue)

    def _build_style(self):
        style = ttk.Style()
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass
        style.configure("Treeview", rowheight=24)
        style.configure("TButton", padding=(8, 4))
        style.configure("Danger.TButton", foreground="#9B1C1C")
        style.configure("Good.TLabel", foreground="#0F766E")
        style.configure("Bad.TLabel", foreground="#B91C1C")

    def _build_ui(self):
        self.main_pane = ttk.PanedWindow(self.root, orient=tk.VERTICAL)
        self.main_pane.pack(fill="both", expand=True, padx=8, pady=8)

        self.upper_frame = ttk.Frame(self.main_pane)
        self.trace_frame = ttk.Frame(self.main_pane)
        self.main_pane.add(self.upper_frame, weight=1)
        self.main_pane.add(self.trace_frame, weight=1)

        self.notebook = ttk.Notebook(self.upper_frame)
        self.notebook.pack(fill="both", expand=True)

        self.connection_tab = ttk.Frame(self.notebook)
        self.diagnostics_tab = ttk.Frame(self.notebook)
        self.coding_tab = ttk.Frame(self.notebook)
        self.generator_tab = ttk.Frame(self.notebook)
        self.tal_tab = ttk.Frame(self.notebook)

        self.notebook.add(self.connection_tab, text="Connection")
        self.notebook.add(self.diagnostics_tab, text="Diagnostics")
        self.notebook.add(self.coding_tab, text="Coding")
        self.notebook.add(self.generator_tab, text="Data Editor")
        self.notebook.add(self.tal_tab, text="Flash")

        self._build_connection_tab()
        self._build_diagnostics_tab()
        self._build_coding_tab()
        self._build_generator_tab()
        self._build_tal_tab()
        self._build_trace_tab()
        self.root.after(100, self._set_trace_split)

    def _set_trace_split(self):
        try:
            height = self.main_pane.winfo_height()
            if height > 80:
                self.main_pane.sashpos(0, height // 2)
        except tk.TclError:
            pass

    def _entry_row(self, parent, row, label, variable, width=18, column=0):
        ttk.Label(parent, text=label).grid(row=row, column=column, sticky="w", padx=6, pady=4)
        entry = ttk.Entry(parent, textvariable=variable, width=width)
        entry.grid(row=row, column=column + 1, sticky="ew", padx=6, pady=4)
        return entry

    def _button_row(self, parent, row, buttons):
        frame = ttk.Frame(parent)
        frame.grid(row=row, column=0, columnspan=8, sticky="w", padx=4, pady=8)
        for text, command, style in buttons:
            ttk.Button(frame, text=text, command=command, style=style or "TButton").pack(side="left", padx=4)
        return frame

    def _build_connection_tab(self):
        outer = ttk.Frame(self.connection_tab)
        outer.pack(fill="both", expand=True, padx=10, pady=10)
        outer.columnconfigure(1, weight=1)
        outer.columnconfigure(3, weight=1)

        self.host_var = tk.StringVar(value=DEFAULT_HOST)
        self.port_var = tk.StringVar(value=str(DEFAULT_PORT))
        self.source_var = tk.StringVar(value=int_hex(DEFAULT_SOURCE_ADDR))
        self.target_var = tk.StringVar(value=int_hex(DEFAULT_TARGET_ADDR))
        self.timeout_var = tk.StringVar(value="1.0")
        self.local_ip_var = tk.StringVar(value="auto")
        self.transport_var = tk.StringVar(value="DoIP")
        self.conn_status_var = tk.StringVar(value="Disconnected")

        self._entry_row(outer, 0, "ZGW host", self.host_var, 22, 0)
        self._entry_row(outer, 0, "TCP port", self.port_var, 10, 2)
        self._entry_row(outer, 1, "Tester address", self.source_var, 14, 0)
        self._entry_row(outer, 1, "Target address", self.target_var, 14, 2)
        self._entry_row(outer, 2, "PC local IP", self.local_ip_var, 22, 0)
        self._entry_row(outer, 2, "Timeout seconds", self.timeout_var, 10, 2)

        status = ttk.Label(outer, textvariable=self.conn_status_var, style="Bad.TLabel")
        status.grid(row=3, column=0, columnspan=4, sticky="w", padx=6, pady=8)
        self.connection_status_label = status

        self._button_row(
            outer,
            4,
            [
                ("Connect", self.connect_clicked, None),
                ("Disconnect", self.disconnect_clicked, None),
                ("Sync Time", self.sync_time_clicked, None),
            ],
        )

        self.keepalive_var = tk.BooleanVar(value=True)
        self.keepalive_period_var = tk.StringVar(value="2.0")

    def _build_diagnostics_tab(self):
        outer = ttk.Frame(self.diagnostics_tab)
        outer.pack(fill="both", expand=True, padx=10, pady=10)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(1, weight=1)

        controls = ttk.LabelFrame(outer, text="Fault memory")
        controls.grid(row=0, column=0, sticky="ew", padx=4, pady=4)
        controls.columnconfigure(1, weight=1)

        self.dtc_status_mask_var = tk.StringVar(value="0x09")
        self._entry_row(controls, 0, "Status mask", self.dtc_status_mask_var, 10, 0)
        ttk.Button(controls, text="Read Fault Memory", command=self.read_fault_memory_clicked).grid(
            row=0, column=2, sticky="w", padx=4, pady=4
        )
        ttk.Button(controls, text="Clear Results", command=self.clear_dtc_results_clicked).grid(
            row=0, column=3, sticky="w", padx=4, pady=4
        )
        ttk.Button(controls, text="Clear DTCs", command=self.clear_diagnostic_information_clicked).grid(
            row=0, column=4, sticky="w", padx=4, pady=4
        )

        columns = ("dtc", "status", "decoded", "description", "snapshot", "extended")
        self.dtc_tree = ttk.Treeview(outer, columns=columns, show="headings", selectmode="browse")
        for col, text, width in [
            ("dtc", "DTC", 100),
            ("status", "Status", 90),
            ("decoded", "Status Bits", 360),
            ("description", "DTC Name", 520),
            ("snapshot", "Snapshot / Freeze Frame", 720),
            ("extended", "Extended Data", 720),
        ]:
            self.dtc_tree.heading(col, text=text)
            # Fixed widths (no stretch) so the columns keep their full size and the
            # horizontal scrollbar can pan across them instead of squeezing them to fit.
            self.dtc_tree.column(col, width=width, stretch=False)
        self.dtc_tree.grid(row=1, column=0, sticky="nsew", padx=(4, 0), pady=6)
        scroll = ttk.Scrollbar(outer, orient="vertical", command=self.dtc_tree.yview)
        scroll.grid(row=1, column=1, sticky="ns", padx=(0, 4), pady=6)
        hscroll = ttk.Scrollbar(outer, orient="horizontal", command=self.dtc_tree.xview)
        hscroll.grid(row=2, column=0, sticky="ew", padx=(4, 0))
        self.dtc_tree.configure(yscrollcommand=scroll.set, xscrollcommand=hscroll.set)
        # Treeview clips cell text at the column edge, so the long Snapshot/Extended
        # decodes are only partly visible inline. A hover tooltip shows the full text.
        self._attach_tree_tooltip(self.dtc_tree)

        self.dtc_summary_var = tk.StringVar(value="No fault memory read yet")

    def _attach_tree_tooltip(self, tree):
        """Show the full text of the cell under the cursor in a hover tooltip.
        ttk.Treeview otherwise clips cell content at the column boundary, which
        would hide most of the Snapshot / Extended plain-language decodes."""
        state = {"window": None, "cell": None}

        def hide(_event=None):
            if state["window"] is not None:
                state["window"].destroy()
                state["window"] = None
            state["cell"] = None

        def on_motion(event):
            row = tree.identify_row(event.y)
            col = tree.identify_column(event.x)
            if (not row) or (not col):
                hide()
                return
            try:
                col_index = int(col.replace("#", "")) - 1
            except ValueError:
                hide()
                return
            values = tree.item(row, "values")
            if col_index < 0 or col_index >= len(values):
                hide()
                return
            text = values[col_index]
            if text in ("", None):
                hide()
                return
            if state["cell"] == (row, col):
                return
            hide()
            state["cell"] = (row, col)
            window = tk.Toplevel(tree)
            window.wm_overrideredirect(True)
            window.wm_geometry(f"+{event.x_root + 14}+{event.y_root + 16}")
            tk.Label(
                window,
                text=str(text),
                justify="left",
                wraplength=760,
                background="#FFFFE0",
                relief="solid",
                borderwidth=1,
                font=("Segoe UI", 9),
            ).pack(ipadx=4, ipady=2)
            state["window"] = window

        tree.bind("<Motion>", on_motion, add="+")
        tree.bind("<Leave>", hide, add="+")

    def _build_svt_tab(self):
        outer = ttk.Frame(self.svt_tab)
        outer.pack(fill="both", expand=True, padx=10, pady=10)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(1, weight=1)

        top = ttk.LabelFrame(outer, text="Vehicle Order")
        top.grid(row=0, column=0, sticky="ew", padx=4, pady=4)
        for col in range(6):
            top.columnconfigure(col, weight=1)

        self.fa_project_var = tk.StringVar(value="ZGW_LAB")
        self.fa_vin_var = tk.StringVar(value="LABTC375DOIP0001")
        self.fa_type_var = tk.StringVar(value="ZGW")
        self._entry_row(top, 0, "Project", self.fa_project_var, 18, 0)
        self._entry_row(top, 0, "VIN", self.fa_vin_var, 22, 2)
        self._entry_row(top, 1, "Type", self.fa_type_var, 12, 0)

        svt_frame = ttk.LabelFrame(outer, text="SVT / ECU List")
        svt_frame.grid(row=1, column=0, sticky="nsew", padx=4, pady=8)
        svt_frame.columnconfigure(0, weight=1)
        svt_frame.rowconfigure(0, weight=1)

        columns = ("ecu", "target", "req", "resp", "did", "hex", "size", "crc")
        self.svt_tree = ttk.Treeview(svt_frame, columns=columns, show="headings", selectmode="browse")
        for col, text, width in [
            ("use", "Use", 55),
            ("ecu", "ECU", 150),
            ("target", "Target", 90),
            ("req", "Req ID", 80),
            ("resp", "Resp ID", 80),
            ("did", "DID", 80),
            ("hex", "Source HEX", 360),
            ("size", "Size", 90),
            ("crc", "CRC32", 110),
        ]:
            self.svt_tree.heading(col, text=text)
            self.svt_tree.column(col, width=width, stretch=(col == "hex"))
        self.svt_tree.grid(row=0, column=0, sticky="nsew")
        svt_scroll = ttk.Scrollbar(svt_frame, orient="vertical", command=self.svt_tree.yview)
        svt_scroll.grid(row=0, column=1, sticky="ns")
        self.svt_tree.configure(yscrollcommand=svt_scroll.set)

        buttons = ttk.Frame(outer)
        buttons.grid(row=2, column=0, sticky="w", padx=4, pady=6)
        ttk.Button(buttons, text="Import SVT JSON", command=self.import_svt_clicked).pack(side="left", padx=4)
        ttk.Button(buttons, text="Export SVT JSON", command=self.export_svt_clicked).pack(side="left", padx=4)
        ttk.Button(buttons, text="Copy From Generator", command=self.copy_generator_to_svt).pack(side="left", padx=4)

    def _build_coding_tab(self):
        outer = ttk.Frame(self.coding_tab)
        outer.pack(fill="both", expand=True, padx=10, pady=10)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(2, weight=1)

        self.coding_mask_did_var = tk.StringVar(value=int_hex(CODING_DID_RX_MESSAGE_EXPECTED))
        self.coding_image_did_var = tk.StringVar(value=int_hex(CODING_DID_IMAGE))
        self.coding_status_did_var = tk.StringVar(value=int_hex(CODING_DID_STATUS))
        self.coding_version_did_var = tk.StringVar(value=int_hex(CODING_DID_VERSION))
        self.coding_validate_rid_var = tk.StringVar(value=int_hex(CODING_ROUTINE_VALIDATE))
        self.coding_write_all_rid_var = tk.StringVar(value=int_hex(CODING_ROUTINE_WRITE_ALL))
        self.coding_read_nvm_rid_var = tk.StringVar(value=int_hex(CODING_ROUTINE_READ_NVM))
        self.coding_defaults_rid_var = tk.StringVar(value=int_hex(CODING_ROUTINE_LOAD_DEFAULTS))

        file_frame = ttk.LabelFrame(outer, text="Coding configuration")
        file_frame.grid(row=0, column=0, sticky="ew", padx=4, pady=8)

        rc_frame = ttk.Frame(file_frame)
        rc_frame.pack(side="left", padx=4, pady=6)
        ttk.Button(rc_frame, text="Code ECU", command=self.code_ecu_clicked, style="Danger.TButton").pack(side="left", padx=4)
        ttk.Button(rc_frame, text="Read Coding", command=self.read_current_coding_clicked).pack(side="left", padx=4)
        ttk.Button(rc_frame, text="Load Coding Default", command=self.load_default_coding_clicked).pack(side="left", padx=4)
        ttk.Button(rc_frame, text="Check Coding", command=self.check_coding_clicked).pack(side="left", padx=4)

        file_ops = ttk.Frame(file_frame)
        file_ops.pack(side="left", padx=16, pady=6)
        ttk.Button(file_ops, text="Save Current", command=self.save_current_coding_clicked).pack(side="left", padx=4)
        ttk.Button(file_ops, text="Import", command=self.import_coding_clicked).pack(side="left", padx=4)
        ttk.Button(file_ops, text="Export", command=self.export_coding_clicked).pack(side="left", padx=4)
        ttk.Button(file_ops, text="Add Parameter", command=self.add_coding_param_clicked).pack(side="left", padx=4)
        ttk.Button(file_ops, text="Edit Selected", command=self.edit_coding_param_clicked).pack(side="left", padx=4)
        ttk.Button(file_ops, text="Remove Selected", command=self.remove_coding_param_clicked).pack(side="left", padx=4)

        columns = ("param", "index", "expected", "comment")
        self.coding_tree = ttk.Treeview(outer, columns=columns, show="headings", selectmode="extended")
        for col, text, width in [
            ("param", "Coding Parameters", 320),
            ("index", "Coding Data Position", 200),
            ("expected", "Value", 100),
            ("comment", "Comment", 600),
        ]:
            self.coding_tree.heading(col, text=text)
            self.coding_tree.column(col, width=width, stretch=(col == "comment"))
        self.coding_tree.grid(row=2, column=0, sticky="nsew", padx=(4, 0), pady=4)
        coding_scroll = ttk.Scrollbar(outer, orient="vertical", command=self.coding_tree.yview)
        coding_scroll.grid(row=2, column=1, sticky="ns", padx=(0, 4), pady=4)
        self.coding_tree.configure(yscrollcommand=coding_scroll.set)
        self.coding_tree.bind("<Double-1>", lambda _event: self.edit_coding_param_clicked())

    def _build_generator_tab(self):
        outer = ttk.Frame(self.generator_tab)
        outer.pack(fill="both", expand=True, padx=10, pady=10)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(2, weight=1)

        meta = ttk.LabelFrame(outer, text="FCD Input File Generator")
        meta.grid(row=0, column=0, sticky="ew", padx=4, pady=4)
        for col in range(8):
            meta.columnconfigure(col, weight=1)

        self.fa_project_var = tk.StringVar(value="ZGW_LAB")
        self.fa_vin_var = tk.StringVar(value="LABTC375DOIP0001")
        self.fa_type_var = tk.StringVar(value="ZGW")
        self.gen_project_var = self.fa_project_var
        self.gen_vin_var = self.fa_vin_var
        self.gen_output_var = tk.StringVar(value=str(SCRIPT_DIR / "Generated"))
        self._entry_row(meta, 0, "VIN", self.gen_vin_var, 22, 0)
        self._entry_row(meta, 1, "Output folder", self.gen_output_var, 80, 0)
        ttk.Button(meta, text="Browse", command=self.browse_gen_output_clicked).grid(row=1, column=2, padx=4, pady=4)
        buttons = ttk.Frame(outer)
        buttons.grid(row=1, column=0, sticky="w", padx=4, pady=6)
        ttk.Button(buttons, text="Add HEX", command=self.add_hex_clicked).pack(side="left", padx=4)
        ttk.Button(buttons, text="Remove Selected", command=self.remove_hex_clicked).pack(side="left", padx=4)
        ttk.Button(buttons, text="Generate FCD Files", command=self.generate_files_clicked).pack(side="left", padx=12)

        columns = ("ecu", "base", "hex")
        self.generator_tree = ttk.Treeview(outer, columns=columns, show="headings", selectmode="browse")
        for col, text, width in [
            ("ecu", "ECU", 160),
            ("base", "Download Base", 130),
            ("hex", "HEX File", 520),
        ]:
            self.generator_tree.heading(col, text=text)
            self.generator_tree.column(col, width=width, stretch=(col == "hex"))
        self.generator_tree.grid(row=2, column=0, sticky="nsew", padx=4, pady=4)
        self.generator_tree.bind("<Double-1>", lambda _event: self.edit_hex_clicked())

    def _build_tal_tab(self):
        outer = ttk.Frame(self.tal_tab)
        outer.pack(fill="both", expand=True, padx=10, pady=10)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(2, weight=1)

        load = ttk.LabelFrame(outer, text="Flash package")
        load.grid(row=0, column=0, sticky="ew", padx=4, pady=4)
        for col in range(8):
            load.columnconfigure(col, weight=1)

        self.pkg_dir_var = tk.StringVar(value="")
        self._entry_row(load, 0, "Package folder", self.pkg_dir_var, 85, 0)
        ttk.Button(load, text="Browse", command=self.browse_package_clicked).grid(row=0, column=2, padx=4, pady=4)
        ttk.Button(load, text="Load", command=self.load_package_clicked).grid(row=0, column=3, padx=4, pady=4)

        options = ttk.LabelFrame(outer, text="Flash execution options")
        options.grid(row=1, column=0, sticky="ew", padx=4, pady=8)
        for col in range(10):
            options.columnconfigure(col, weight=1)

        self.dry_run_var = tk.BooleanVar(value=True)
        self.arm_programming_var = tk.BooleanVar(value=False)
        self.erase_var = tk.BooleanVar(value=True)
        self.transfer_crc_var = tk.BooleanVar(value=True)
        self.verify_crc_var = tk.BooleanVar(value=True)
        self.flash_fbl_var = tk.BooleanVar(value=False)
        self.flash_appl_var = tk.BooleanVar(value=True)
        self.flash_coding_var = tk.BooleanVar(value=True)
        self.block_size_var = tk.StringVar(value=str(DEFAULT_BLOCK_SIZE))
        self.session_var = tk.StringVar(value="0x02")

        ttk.Checkbutton(options, text="Dry run", variable=self.dry_run_var).grid(row=0, column=0, sticky="w", padx=6)
        ttk.Checkbutton(options, text="Arm programming", variable=self.arm_programming_var).grid(
            row=0, column=1, sticky="w", padx=6
        )
        ttk.Checkbutton(options, text="Erase first", variable=self.erase_var).grid(row=0, column=2, sticky="w", padx=6)
        ttk.Checkbutton(options, text="TransferExit CRC", variable=self.transfer_crc_var).grid(
            row=0, column=3, sticky="w", padx=6
        )
        ttk.Checkbutton(options, text="CRC routine", variable=self.verify_crc_var).grid(row=0, column=4, sticky="w", padx=6)
        ttk.Checkbutton(options, text="FBL", variable=self.flash_fbl_var).grid(row=0, column=5, sticky="w", padx=6)
        ttk.Checkbutton(options, text="APPL", variable=self.flash_appl_var).grid(row=0, column=6, sticky="w", padx=6)
        ttk.Checkbutton(options, text="Coding", variable=self.flash_coding_var).grid(row=0, column=7, sticky="w", padx=6)
        self._entry_row(options, 1, "Session", self.session_var, 10, 0)
        self._entry_row(options, 1, "Block size", self.block_size_var, 10, 2)
        ttk.Label(
            options,
            text="Dry run logs the complete service sequence without sending it. Arm programming is the second safety latch required for real flashing.",
            wraplength=1120,
        ).grid(row=2, column=0, columnspan=10, sticky="ew", padx=6, pady=6)

        columns = ("use", "ecu", "target", "address", "size", "crc", "file")
        self.payload_tree = ttk.Treeview(outer, columns=columns, show="headings", selectmode="extended")
        for col, text, width in [
            ("use", "Use", 55),
            ("ecu", "ECU", 150),
            ("target", "Target", 90),
            ("address", "Address", 120),
            ("size", "Size", 90),
            ("crc", "CRC32", 110),
            ("file", "Payload File", 560),
        ]:
            self.payload_tree.heading(col, text=text)
            self.payload_tree.column(col, width=width, stretch=(col == "file"))
        self.payload_tree.grid(row=2, column=0, sticky="nsew", padx=4, pady=4)
        self.payload_tree.bind("<Button-1>", self.payload_tree_click)

        actions = ttk.Frame(outer)
        actions.grid(row=3, column=0, sticky="ew", padx=4, pady=8)
        ttk.Button(actions, text="Start Flashing", command=self.execute_selected_clicked, style="Danger.TButton").pack(
            side="left", padx=4
        )
        self.progress = ttk.Progressbar(actions, mode="determinate", length=300)
        self.progress.pack(side="left", padx=18)

    def _build_raw_tab(self):
        outer = ttk.Frame(self.raw_tab)
        outer.pack(fill="both", expand=True, padx=10, pady=10)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(1, weight=1)

        top = ttk.Frame(outer)
        top.grid(row=0, column=0, sticky="ew", padx=4, pady=4)
        top.columnconfigure(1, weight=1)
        ttk.Label(top, text="UDS request bytes").grid(row=0, column=0, sticky="w", padx=4)
        self.raw_request_var = tk.StringVar(value="10 03")
        ttk.Entry(top, textvariable=self.raw_request_var).grid(row=0, column=1, sticky="ew", padx=4)
        ttk.Button(top, text="Send", command=self.raw_send_clicked).grid(row=0, column=2, padx=4)

        presets = ttk.Frame(outer)
        presets.grid(row=2, column=0, sticky="w", padx=4, pady=8)
        for label, data in [
            ("10 01", "10 01"),
            ("10 02", "10 02"),
            ("10 03", "10 03"),
            ("22 F1 86", "22 F1 86"),
            ("27 01", "27 01"),
            ("3E 00", "3E 00"),
            ("85 02", "85 02"),
        ]:
            ttk.Button(presets, text=label, command=lambda value=data: self.raw_request_var.set(value)).pack(
                side="left", padx=3
            )

        self.raw_response_text = tk.Text(outer, height=18, wrap="word", font=("Consolas", 10))
        self.raw_response_text.grid(row=1, column=0, sticky="nsew", padx=4, pady=6)

    def _build_trace_tab(self):
        outer = ttk.LabelFrame(self.trace_frame, text="Trace")
        outer.pack(fill="both", expand=True, padx=2, pady=(8, 2))
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(0, weight=1)

        self.trace_text = tk.Text(outer, wrap="word", font=("Consolas", 10))
        self.trace_text.grid(row=0, column=0, sticky="nsew")
        scroll = ttk.Scrollbar(outer, orient="vertical", command=self.trace_text.yview)
        scroll.grid(row=0, column=1, sticky="ns")
        self.trace_text.configure(yscrollcommand=scroll.set)

        bottom = ttk.Frame(outer)
        bottom.grid(row=1, column=0, columnspan=2, sticky="w", pady=6)
        ttk.Button(bottom, text="Clear Trace", command=lambda: self.trace_text.delete("1.0", "end")).pack(side="left", padx=4)
        ttk.Button(bottom, text="Save Trace", command=self.save_trace_clicked).pack(side="left", padx=4)
        self.trace_autoscroll_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(bottom, text="Auto-scroll", variable=self.trace_autoscroll_var).pack(side="left", padx=12)

    def log(self, message):
        self.log_queue.put(f"[{now_text()}] {message}")

    def _drain_log_queue(self):
        drained = 0
        try:
            while drained < 500:
                line = self.log_queue.get_nowait()
                self.trace_text.insert("end", line + "\n")
                drained += 1
        except queue.Empty:
            pass
        if drained and self.trace_autoscroll_var.get():
            self.trace_text.see("end")
        self.root.after(10 if drained else 50, self._drain_log_queue)

    def worker(self, name, func):
        def run():
            try:
                self.worker_stop.clear()
                self.log(f"{name}: started")
                func()
                self.log(f"{name}: done")
            except FcdError as exc:
                self.log(f"{name}: ERROR: {exc}")
            except Exception as exc:
                self.log(f"{name}: ERROR: {exc}")
                self.log(traceback.format_exc().strip())

        threading.Thread(target=run, daemon=True).start()

    def make_client_from_fields(self):
        host = self.host_var.get().strip()
        port = parse_int(self.port_var.get())
        timeout = float(self.timeout_var.get())
        local_ip = self.local_ip_var.get().strip() or "auto"
        if self.transport_var.get() == "DoIP":
            return DoipClient(
                host=host,
                port=port,
                source_addr=parse_int(self.source_var.get()),
                target_addr=parse_int(self.target_var.get()),
                timeout=timeout,
                local_ip=local_ip,
            )
        return RawTcpUdsClient(host=host, port=port, timeout=timeout, local_ip=local_ip)

    def activate_doip(self, client):
        if isinstance(client, DoipClient):
            ecu, code = client.routing_activation()
            self.log(f"Routing activation OK: ecu={int_hex(ecu)} code=0x{code:02X}")

    def _reconnect_cancelled(self, client, stop_event=None):
        if self.worker_stop.is_set():
            return True
        if stop_event is not None and stop_event.is_set():
            return True
        return self.client is not client

    def reconnect_doip(self, client, reason, stop_event=None, log_success=True, deadline=None):
        if not isinstance(client, DoipClient):
            return

        timeout = max(1.0, float(self.timeout_var.get()))
        last_error = None
        attempt = 0

        if log_success:
            self.log(
                f"DoIP reconnect after {reason}: waiting for TCP {client.host}:{client.port} "
                "until Disconnect"
            )
        client.close()
        self.root.after(0, self._set_connected_status, False)
        if self._reconnect_cancelled(client, stop_event):
            raise FcdError(f"DoIP reconnect after {reason} cancelled by Disconnect button")

        while not self._reconnect_cancelled(client, stop_event):
            if (deadline is not None) and (time.monotonic() >= deadline):
                break

            attempt += 1
            try:
                client.connect()
                if self._reconnect_cancelled(client, stop_event):
                    client.close()
                    break
                self.activate_doip(client)
                if self._reconnect_cancelled(client, stop_event):
                    client.close()
                    break
                self.root.after(0, self._set_connected_status, True)
                if log_success:
                    self.log(f"DoIP reconnected after {reason}")
                return
            except (OSError, TimeoutError, DoipError) as exc:
                last_error = exc
                client.close()
                if attempt == 1 or attempt % 10 == 0:
                    self.log(f"DoIP reconnect after {reason}: attempt {attempt} failed: {exc}")
                if self._reconnect_cancelled(client, stop_event):
                    break
                wait_time = 0.5
                if deadline is not None:
                    wait_time = min(wait_time, max(0.0, deadline - time.monotonic()))
                    if wait_time <= 0.0:
                        break
                if stop_event is not None:
                    if stop_event.wait(wait_time):
                        break
                elif self.worker_stop.wait(wait_time):
                    break

        self.root.after(0, self._set_connected_status, False)
        if (deadline is not None) and (time.monotonic() >= deadline):
            raise FcdError(f"DoIP reconnect after {reason} timed out: {last_error}")
        if last_error is None:
            raise FcdError(f"DoIP reconnect after {reason} cancelled by Disconnect button")
        raise FcdError(f"DoIP reconnect after {reason} cancelled by Disconnect button: {last_error}")

    def _send_uds_with_reconnect(self, client, request, label, timeout=None):
        request = bytes(request)
        while True:
            if self._reconnect_cancelled(client):
                raise FcdError(f"{label}: cancelled by Disconnect button")
            try:
                return client.send_uds(request, timeout=timeout)
            except (OSError, TimeoutError, DoipError) as exc:
                if not isinstance(client, DoipClient) or self.client is not client:
                    raise
                self.log(f"{label}: connection lost ({exc}); reconnecting and retrying")
                self.reconnect_doip(client, label)
                self.log(f"TX {label} retry: {bytes_to_hex(request)}")

    def connect_clicked(self):
        def action():
            # One button sequence: Discover DoIP -> Vehicle Identification -> TCP Connect + Routing Activation.
            # Logging intentionally keeps the three phases visible.
            host_before = self.host_var.get().strip()
            port = parse_int(self.port_var.get())
            timeout = float(self.timeout_var.get())
            local_ip = self.local_ip_var.get().strip() or "auto"
            selected_host = host_before
            selected_target = parse_int(self.target_var.get())
            self.log(
                "Local bind candidates: "
                + ", ".join(ip or "OS default" for ip in local_bind_candidates(host_before, local_ip))
            )

            self.log("Discover DoIP: started")
            try:
                results = DoipClient.discover(
                    port=port,
                    timeout=timeout,
                    extra_hosts=[host_before],
                    local_ip=local_ip,
                    include_broadcast=False,
                )
                if results:
                    for info in results:
                        self.log(
                            "DoIP discovered: "
                            f"ip={info['ip']} vin={info['vin']} logical={int_hex(info['logical_address'])} "
                            f"eid={info['eid']} gid={info['gid']}"
                        )
                    first = results[0]
                    selected_host = first["ip"]
                    selected_target = first["logical_address"]
                    self.root.after(0, lambda: self.host_var.set(first["ip"]))
                    self.root.after(0, lambda: self.target_var.set(int_hex(first["logical_address"])))
                    self.root.after(0, lambda: self.fa_vin_var.set(first["vin"] or self.fa_vin_var.get()))
                else:
                    self.log("Discover DoIP: no response; continuing with configured host")
            except Exception as exc:
                self.log(f"Discover DoIP: ERROR: {exc}; continuing with configured host")
            self.log("Discover DoIP: done")

            selected_host = selected_host or self.host_var.get().strip() or host_before
            if results:
                self.log("Vehicle Identification: skipped; using discovery response")
            else:
                self.log("Vehicle Identification: started")
                try:
                    info = DoipClient.vehicle_identification(selected_host, port=port, timeout=timeout, local_ip=local_ip)
                    self.log(
                        f"Vehicle ID: from={info['ip']}:13400 vin={info['vin']} "
                        f"logical={int_hex(info['logical_address'])} eid={info['eid']} gid={info['gid']}"
                    )
                    selected_host = info["ip"]
                    selected_target = info["logical_address"]
                    self.root.after(0, lambda: self.host_var.set(info["ip"]))
                    self.root.after(0, lambda: self.target_var.set(int_hex(info["logical_address"])))
                    if info.get("vin"):
                        self.root.after(0, lambda: self.fa_vin_var.set(info["vin"]))
                except Exception as exc:
                    self.log(f"Vehicle Identification: ERROR: {exc}; continuing with TCP connect")
                self.log("Vehicle Identification: done")

            # Close any existing client before opening a new one.
            self.stop_keepalive(update_var=False)
            if self.client is not None:
                self.client.close()
                self.client = None
                self.root.after(0, self._set_connected_status, False)

            client = self.make_client_from_fields()
            client.host = selected_host
            if isinstance(client, DoipClient):
                client.target_addr = selected_target
            for attempt in range(2):
                try:
                    client.connect()
                    break
                except TimeoutError as exc:
                    client.close()
                    if attempt == 0:
                        self.log("TCP connect timed out; retrying once after ARP warm-up")
                        time.sleep(0.75)
                        continue
                    raise FcdError(
                        "TCP connect timed out. No TCP listener answered at that IP:port. "
                        "The target may have a stale DoIP TCP socket, or the TCP listener is closed."
                    ) from exc
                except OSError as exc:
                    raise FcdError(f"TCP connect failed: {exc}") from exc

            try:
                self.activate_doip(client)
            except TimeoutError as exc:
                client.close()
                raise FcdError("DoIP routing activation timed out after TCP connect") from exc
            except DoipError as exc:
                client.close()
                raise FcdError(f"DoIP routing activation failed after TCP connect: {exc}") from exc

            self.client = client
            self.root.after(0, self._set_connected_status, True)
            bound = getattr(client, "bound_local_ip", "") or "OS default"
            self.log(
                f"Connected to {self.host_var.get()}:{self.port_var.get()} using "
                f"{self.transport_var.get()} from {bound}"
            )
            if self.keepalive_var.get():
                self.start_keepalive()

        self.worker("Connect", action)

    def discover_doip_clicked(self):
        def action():
            local_ip = self.local_ip_var.get().strip() or "auto"
            self.log(
                "Local bind candidates: "
                + ", ".join(ip or "OS default" for ip in local_bind_candidates(self.host_var.get().strip(), local_ip))
            )
            results = DoipClient.discover(
                port=parse_int(self.port_var.get()),
                timeout=float(self.timeout_var.get()),
                extra_hosts=[self.host_var.get().strip(), "192.168.1.1", "192.168.1.10"],
                local_ip=local_ip,
            )
            if not results:
                raise FcdError(
                    "No DoIP vehicle-identification response received on UDP 13400. "
                    "That usually means the target is not running the DoIP/FBL server, "
                    "the PC is on the wrong interface/subnet, or firewall/broadcast filtering is in the way."
                )
            for info in results:
                self.log(
                    "DoIP discovered: "
                    f"ip={info['ip']} vin={info['vin']} logical={int_hex(info['logical_address'])} "
                    f"eid={info['eid']} gid={info['gid']}"
                )
            first = results[0]
            self.root.after(0, lambda: self.host_var.set(first["ip"]))
            self.root.after(0, lambda: self.target_var.set(int_hex(first["logical_address"])))
            self.root.after(0, lambda: self.fa_vin_var.set(first["vin"] or self.fa_vin_var.get()))

        self.worker("Discover DoIP", action)

    def ethernet_probe_clicked(self):
        def action():
            local_ip = self.local_ip_var.get().strip() or "auto"
            self.log(
                "Local bind candidates: "
                + ", ".join(ip or "OS default" for ip in local_bind_candidates(self.host_var.get().strip(), local_ip))
            )
            results = DoipClient.ethernet_probe(
                self.host_var.get().strip(),
                doip_port=parse_int(self.port_var.get()),
                timeout=float(self.timeout_var.get()),
                local_ip=local_ip,
            )

            probe_reply_count = 0
            generic_udp_count = 0
            generic_udp_samples = []
            for item in results:
                kind = item.get("kind", "udp")
                origin = item.get("from", "?")

                if kind in ("bind-error", "send-error"):
                    self.log(f"Probe {kind}: {origin} {item.get('detail', '')}".rstrip())
                    continue

                if kind == "aurix-heartbeat":
                    probe_reply_count += 1
                    self.log(f"Probe RX: AURIX heartbeat from {origin}")
                elif kind == f"doip-0x{DOIP_PT_VID_RES:04X}":
                    probe_reply_count += 1
                    self.log(
                        "Probe RX: DoIP Vehicle ID "
                        f"from={origin} vin={item.get('vin', '')} "
                        f"logical={int_hex(item.get('logical_address', 0))} "
                        f"eid={item.get('eid', '')} gid={item.get('gid', '')}"
                    )
                    self.root.after(0, lambda value=item["ip"]: self.host_var.set(value))
                    self.root.after(0, lambda value=item["logical_address"]: self.target_var.set(int_hex(value)))
                    if item.get("vin"):
                        self.root.after(0, lambda value=item["vin"]: self.fa_vin_var.set(value))
                else:
                    generic_udp_count += 1
                    if len(generic_udp_samples) < 5:
                        generic_udp_samples.append(
                            f"{kind} from={origin} len={item.get('length', 0)} data={item.get('data', '')}"
                        )

            for sample in generic_udp_samples:
                self.log(f"Probe RX: {sample}")
            if generic_udp_count > len(generic_udp_samples):
                self.log(
                    f"Probe RX: {generic_udp_count - len(generic_udp_samples)} additional non-DoIP UDP frames suppressed"
                )

            if probe_reply_count == 0:
                raise FcdError(
                    f"No DoIP or heartbeat response to this probe. Received {generic_udp_count} background UDP "
                    "frame(s), so PC RX sees the ZGW, but the ZGW did not answer FCD traffic. "
                    "If ping reports 'Destination host unreachable' from the PC address, fix the ZGW ARP/RX path "
                    "before retrying TCP DoIP."
                )

        self.worker("Probe Ethernet", action)

    def _set_connected_status(self, connected):
        if connected:
            self.conn_status_var.set("Connected")
            self.connection_status_label.configure(style="Good.TLabel")
        else:
            self.conn_status_var.set("Disconnected")
            self.connection_status_label.configure(style="Bad.TLabel")

    def disconnect_clicked(self):
        self.worker_stop.set()
        self.stop_keepalive()
        if self.client is not None:
            self.client.close()
            self.client = None
        self._set_connected_status(False)
        self.log("Disconnected")

    def require_client(self):
        if self.client is None or not self.client.connected:
            raise FcdError("Connect first")
        return self.client

    def routing_activation_clicked(self):
        def action():
            client = self.require_client()
            if not isinstance(client, DoipClient):
                raise FcdError("Routing activation is only for DoIP")
            ecu, code = client.routing_activation()
            self.log(f"Routing active: ECU 0x{ecu:04X}, code 0x{code:02X}")

        self.worker("Routing Activation", action)

    def alive_check_clicked(self):
        def action():
            client = self.require_client()
            if not isinstance(client, DoipClient):
                raise FcdError("Alive check is only for DoIP")
            ecu = client.alive_check()
            self.log(f"Alive response from ECU 0x{ecu:04X}")

        self.worker("Alive Check", action)

    def _log_time_sync_status(self, response, prefix):
        if not isinstance(response, bytes) or len(response) < 44:
            self.log(f"{prefix}: Time status raw: {bytes_to_hex(response) if isinstance(response, bytes) else response}")
            return

        data = response[4:]
        vehicle_ns = int.from_bytes(data[0:8], "big")
        utc_ns = int.from_bytes(data[8:16], "big")
        utc_valid = data[16]
        source = data[17]
        sync_status = int.from_bytes(data[18:22], "big")
        utc_dt = datetime.utcfromtimestamp(utc_ns / 1_000_000_000)
        self.log(
            f"{prefix}: Time status utc={utc_dt.isoformat(timespec='milliseconds')}Z "
            f"utcNs={utc_ns} vehicleNs={vehicle_ns} valid={utc_valid} "
            f"source={source} syncStatus=0x{sync_status:08X}"
        )

    def sync_time_clicked(self):
        def action():
            client = self.require_client()
            keepalive_was_on = bool(self.keepalive_var.get()) or self.keepalive_is_running()
            if keepalive_was_on:
                self.stop_keepalive(update_var=False)
                self.log("Sync Time: paused automatic tester present")

            try:
                send = self._make_uds_sender(client, dry=False)
                self._send_session(send, SESSION_EXTENDED, "Extended Session for time sync")
                pc_now = datetime.now()
                pc_ms = pc_now.microsecond // 1000
                request_payload = struct.pack(
                    ">HBBBBBHhB",
                    pc_now.year,
                    pc_now.month,
                    pc_now.day,
                    pc_now.hour,
                    pc_now.minute,
                    pc_now.second,
                    pc_ms,
                    0,
                    0,
                )
                request = b"\x31\x01" + struct.pack(">H", TIMESYNC_ROUTINE_SET_UTC) + request_payload
                response = send(
                    request,
                    f"RoutineControl F190 Set ZGW time from PC local {pc_now.isoformat(timespec='milliseconds')}",
                    timeout=5.0,
                )
                self._log_time_sync_status(response, "Sync Time")
                status = send(
                    b"\x31\x03" + struct.pack(">H", TIMESYNC_ROUTINE_GET_STATUS),
                    "RoutineControl F191 Time Status",
                    timeout=5.0,
                )
                self._log_time_sync_status(status, "Sync Time")
            finally:
                if keepalive_was_on and self.client is not None and self.client.connected:
                    self.start_keepalive()

        self.worker("Sync Time", action)

    def vehicle_id_clicked(self):
        def action():
            try:
                info = DoipClient.vehicle_identification(
                    self.host_var.get().strip(),
                    parse_int(self.port_var.get()),
                    float(self.timeout_var.get()),
                    local_ip=self.local_ip_var.get().strip() or "auto",
                )
            except TimeoutError as exc:
                raise FcdError(
                    "No DoIP vehicle-identification response from the selected host. "
                    "If the target is sending other Ethernet frames, APP/FBL is still not exposing DoIP UDP 13400."
                ) from exc
            self.log(
                "Vehicle ID: "
                f"from={info['from']} vin={info['vin']} logical={int_hex(info['logical_address'])} "
                f"eid={info['eid']} gid={info['gid']}"
            )
            self.root.after(0, lambda: self.fa_vin_var.set(info["vin"] or self.fa_vin_var.get()))

        self.worker("Vehicle Identification", action)

    def send_uds_async(self, request, label="UDS"):
        def action():
            client = self.require_client()
            request_bytes = bytes(request)
            self.log(f"TX {label}: {bytes_to_hex(request_bytes)}")
            response = client.send_uds(request_bytes, timeout=float(self.timeout_var.get()))
            self.log(f"RX {label}: {bytes_to_hex(response)}")
            require_positive_response(response, request_bytes[0])
            if isinstance(client, DoipClient) and response:
                if len(request_bytes) >= 2 and request_bytes[0] == 0x10 and (request_bytes[1] & 0x7F) == 0x02 and response[0] == 0x50:
                    self.reconnect_doip(client, "programming session")
                elif len(request_bytes) >= 2 and request_bytes[0] == 0x11 and (request_bytes[1] & 0x7F) in (0x01, 0x03) and response[0] == 0x51:
                    self.reconnect_doip(client, "ECU reset")

        self.worker(label, action)

    def reset_clicked(self):
        if not messagebox.askyesno(APP_NAME, "Send ECU Reset (0x11 0x01) to the selected target?"):
            return
        self.send_uds_async(b"\x11\x01", "ECU Reset")

    def toggle_keepalive(self):
        # Kept for compatibility with older UI callbacks; tester present is automatic while connected.
        if self.client is not None and self.client.connected:
            self.start_keepalive()
        else:
            self.stop_keepalive(update_var=False)

    def start_keepalive(self):
        self.stop_keepalive(update_var=False)
        self.keepalive_stop.clear()
        try:
            period = max(1.0, float(self.keepalive_period_var.get()))
        except (TypeError, ValueError):
            period = 1.0
            self.keepalive_period_var.set(f"{period:.1f}")

        last_success_log = 0.0

        def log_keepalive_success(message):
            nonlocal last_success_log
            now = time.monotonic()
            if now - last_success_log >= 60.0:
                self.log(message)
                last_success_log = now

        def loop():
            while not self.keepalive_stop.is_set():
                client = self.client
                try:
                    if client is not None and client.connected:
                        if isinstance(client, DoipClient):
                            timeout = max(float(self.timeout_var.get()), 3.0)
                            client.send_uds_suppress_positive(b"\x3E\x80", timeout=timeout)
                            log_keepalive_success("TesterPresent OK: 3E 80 DoIP ACK")
                        else:
                            response = client.send_uds(b"\x3E\x00", timeout=float(self.timeout_var.get()))
                            require_positive_response(response, 0x3E)
                            log_keepalive_success(f"TesterPresent RX: {bytes_to_hex(response)}")
                except Exception as exc:
                    if self.keepalive_stop.is_set():
                        break
                    if isinstance(client, DoipClient) and self.client is client:
                        try:
                            self.reconnect_doip(
                                client,
                                "TesterPresent TCP refresh",
                                stop_event=self.keepalive_stop,
                                log_success=False,
                            )
                            if self.keepalive_stop.is_set():
                                break
                            log_keepalive_success("TesterPresent refreshed DoIP connection")
                        except Exception as reconnect_exc:
                            if self.keepalive_stop.is_set() or self.worker_stop.is_set() or self.client is not client:
                                break
                            self.log(f"TesterPresent ERROR: {exc}; reconnect stopped: {reconnect_exc}")
                            break
                    elif client is not None and self.client is client:
                        self.log(f"TesterPresent ERROR: {exc}")
                        client.close()
                        self.root.after(0, self._set_connected_status, False)
                self.keepalive_stop.wait(period)

        self.keepalive_thread = threading.Thread(target=loop, daemon=True)
        self.keepalive_thread.start()
        self.log(f"Automatic tester present started: {period:.1f} s")

    def keepalive_is_running(self):
        thread = self.keepalive_thread
        return thread is not None and thread.is_alive() and not self.keepalive_stop.is_set()

    def stop_keepalive(self, update_var=True):
        self.keepalive_stop.set()
        # Wait for the keepalive thread to actually exit before returning. Without this
        # join an in-flight TesterPresent could still be mid-exchange on the shared DoIP
        # socket and race the next request, desyncing request/response frames.
        thread = self.keepalive_thread
        if thread is not None and thread is not threading.current_thread() and thread.is_alive():
            try:
                join_timeout = max(8.0, float(self.timeout_var.get()) + 2.0)
            except (TypeError, ValueError):
                join_timeout = 8.0
            thread.join(timeout=join_timeout)
        self.keepalive_thread = None
        if update_var and hasattr(self, "keepalive_var"):
            self.keepalive_var.set(False)

    def raw_send_clicked(self):
        try:
            request = hex_to_bytes(self.raw_request_var.get())
        except Exception as exc:
            messagebox.showerror(APP_NAME, str(exc))
            return

        def action():
            client = self.require_client()
            self.log(f"TX Raw: {bytes_to_hex(request)}")
            response = client.send_uds(request, timeout=float(self.timeout_var.get()))
            self.log(f"RX Raw: {bytes_to_hex(response)}")
            self.root.after(0, self._append_raw_response, request, response)

        self.worker("Raw UDS", action)

    def _append_raw_response(self, request, response):
        self.raw_response_text.insert("end", f"> {bytes_to_hex(request)}\n< {bytes_to_hex(response)}\n\n")
        self.raw_response_text.see("end")

    def clear_dtc_results_clicked(self):
        self.dtc_tree.delete(*self.dtc_tree.get_children())
        self.dtc_summary_var.set("No fault memory read yet")

    def clear_diagnostic_information_clicked(self):
        def action():
            client = self.require_client()
            keepalive_was_on = bool(self.keepalive_var.get()) or self.keepalive_is_running()
            if keepalive_was_on:
                self.stop_keepalive(update_var=False)
                self.log("ClearDiagnosticInformation: paused automatic tester present")

            sequence_ok = False
            try:
                for request, label, timeout in [
                    (bytes([0x10, SESSION_EXTENDED]), "Extended Session for ClearDiagnosticInformation", 10.0),
                    (b"\x14\xff\xff\xff", "ClearDiagnosticInformation 14 FF FF FF", max(5.0, float(self.timeout_var.get()))),
                ]:
                    self.log(f"TX {label}: {bytes_to_hex(request)}")
                    response = self._send_uds_with_reconnect(client, request, label, timeout=timeout)
                    self.log(f"RX {label}: {bytes_to_hex(response)}")
                    require_positive_response(response, request[0])

                self.log("ClearDiagnosticInformation: positive response accepted; active faults can be reported again if still present")
                sequence_ok = True
            finally:
                if keepalive_was_on and self.client is not None and self.client.connected:
                    self.start_keepalive()
                elif (not sequence_ok) and (self.client is client) and (not client.connected):
                    self.client = None
                    self.root.after(0, self._set_connected_status, False)

        self.worker("ClearDiagnosticInformation 14 FF FF FF", action)

    def read_fault_memory_clicked(self):
        try:
            status_mask = parse_int(self.dtc_status_mask_var.get())
            if status_mask < 0 or status_mask > 0xFF:
                raise ValueError("Status mask must be one byte")
        except Exception as exc:
            messagebox.showerror(APP_NAME, str(exc))
            return

        def action():
            client = self.require_client()
            keepalive_was_on = bool(self.keepalive_var.get()) or self.keepalive_is_running()
            if keepalive_was_on:
                self.stop_keepalive(update_var=False)
                self.log("Diagnostics: paused automatic tester present for fault-memory read")

            rows = []
            summaries = []
            sequence_ok = False
            old_request_spacing = getattr(client, "request_spacing_seconds", REQUEST_SPACING_SECONDS)
            if hasattr(client, "request_spacing_seconds"):
                client.request_spacing_seconds = FAULT_MEMORY_REQUEST_SPACING_SECONDS
            try:
                for request, label in [
                    (bytes([0x19, 0x01, 0xFF]), "19 01 FF reportNumberOfDTCByStatusMask"),
                    (bytes([0x19, 0x02, status_mask]), f"19 02 {status_mask:02X} reportDTCByStatusMask"),
                ]:
                    self.log(f"TX {label}: {bytes_to_hex(request)}")
                    response = self._send_uds_with_reconnect(
                        client,
                        request,
                        label,
                        timeout=max(2.0, float(self.timeout_var.get())),
                    )
                    self.log(f"RX {label}: {bytes_to_hex(response)}")
                    require_positive_response(response, 0x19)
                    parsed_rows, summary = self._decode_read_dtc_response(label, response)
                    rows.extend(parsed_rows)
                    summaries.append(summary)
                    if request[1] == 0x02:
                        for row in parsed_rows:
                            dtc = row["dtc_value"]
                            row["snapshot"] = self._read_dtc_detail(client, dtc, 0x04, "snapshot/freeze frame")
                            row["extended"] = self._read_dtc_detail(client, dtc, 0x06, "extended data")
                sequence_ok = True
            finally:
                if hasattr(client, "request_spacing_seconds"):
                    client.request_spacing_seconds = old_request_spacing
                if keepalive_was_on and sequence_ok and self.client is not None and self.client.connected:
                    self.start_keepalive()
                elif (not sequence_ok) and (self.client is client) and (not client.connected):
                    self.client = None
                    self.root.after(0, self._set_connected_status, False)

            self.root.after(0, self._load_dtc_rows, rows, " | ".join(summaries))

        self.worker("Read Fault Memory", action)

    def _load_dtc_rows(self, rows, summary):
        self.dtc_tree.delete(*self.dtc_tree.get_children())
        for row in rows:
            self.dtc_tree.insert(
                "",
                "end",
                values=(
                    row["dtc"],
                    row["status"],
                    row["decoded"],
                    row["description"],
                    row.get("snapshot", ""),
                    row.get("extended", ""),
                ),
            )
        self.dtc_summary_var.set(summary if summary else "Fault memory read completed")

    def _read_dtc_detail(self, client, dtc, subfunction, detail_name):
        request = bytes([
            0x19,
            subfunction,
            (dtc >> 16) & 0xFF,
            (dtc >> 8) & 0xFF,
            dtc & 0xFF,
            0xFF,
        ])
        label = f"19 {subfunction:02X} {dtc:06X} FF {detail_name}"
        try:
            self.log(f"TX {label}: {bytes_to_hex(request)}")
            response = self._send_uds_with_reconnect(
                client,
                request,
                label,
                timeout=max(FAULT_MEMORY_DETAIL_TIMEOUT_SECONDS, float(self.timeout_var.get())),
            )
            self.log(f"RX {label}: {bytes_to_hex(response)}")
            require_positive_response(response, 0x19)
            return self._decode_dtc_detail_response(label, response, dtc, subfunction)
        except NegativeResponse as exc:
            text = f"NRC 0x{exc.nrc:02X} {NRC_TEXT.get(exc.nrc, 'Unknown')}"
            self.log(f"{label}: {text}")
            return text
        except Exception as exc:
            if self.worker_stop.is_set():
                raise
            self.log(f"{label}: {exc}")
            return f"ERROR: {exc}"

    def _decode_dtc_detail_response(self, label, response, expected_dtc, expected_subfunction):
        if len(response) < 7 or response[0] != 0x59 or response[1] != expected_subfunction:
            raise FcdError(f"{label}: malformed detail response {bytes_to_hex(response)}")
        dtc = (response[2] << 16) | (response[3] << 8) | response[4]
        if dtc != expected_dtc:
            raise FcdError(f"{label}: response DTC 0x{dtc:06X} does not match request 0x{expected_dtc:06X}")
        status = response[5]
        record = response[6]
        data = bytes(response[7:])
        raw = bytes_to_hex(data) if data else "(none)"
        # A positive response carries the actual captured buffer, so translate it
        # into plain words. (NRCs never reach here; they raise NegativeResponse.)
        explanation = self._explain_dtc_detail(dtc, expected_subfunction, data)
        if explanation:
            return f"{explanation}   |   [rec 0x{record:02X}, raw {raw}]"
        return f"status=0x{status:02X} record=0x{record:02X} data={raw}"

    def _explain_dtc_detail(self, dtc, subfunction, data):
        """Translate a freeze-frame (0x04) or extended-data (0x06) capture buffer
        into a one-line plain-English summary. Returns "" when the DTC has no
        specific decoder so the caller falls back to the raw hex."""
        is_snapshot = (subfunction == 0x04)
        try:
            if dtc == DEM_DTC_MCUSM_SW_ERROR:
                return self._explain_mcusm_detail(data, is_snapshot)
            if DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT <= dtc < (
                    DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT + DEM_GATEWAY_RX_MESSAGE_EVENT_COUNT):
                return self._explain_gateway_detail(dtc, data, is_snapshot, is_message=True)
            if DEM_DTC_GATEWAY_RX_SIGNAL_INVALID <= dtc < DEM_DTC_CODING_ECU_NOT_CODED:
                return self._explain_gateway_detail(dtc, data, is_snapshot, is_message=False)
            if dtc in (DEM_DTC_CODING_ECU_NOT_CODED, DEM_DTC_CODING_INVALID):
                return self._explain_default_detail(data, is_snapshot)
        except (IndexError, struct.error):
            return ""
        return ""

    def _explain_gateway_detail(self, dtc, data, is_snapshot, is_message):
        # GatewaySwc_CaptureRxDiagFreezeFrame / ...ExtendedData layouts.
        if is_message:
            index = dtc - DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT
            name = CODING_PARAMETER_NAMES[index] if index < len(CODING_PARAMETER_NAMES) else f"index {index}"
        if is_snapshot:
            if len(data) < 30:
                return ""
            bus_text = GATEWAY_BUS_TEXT.get(data[1], f"bus {data[1]}")
            status_text = GATEWAY_RX_DIAG_STATUS_TEXT.get(data[6], f"0x{data[6]:02X}")
            object_id = u16_be(data, 4)
            main_cycles = u32_be(data, 8)
            active_count = u16_be(data, 24)
            event_count = u32_be(data, 26)
            if is_message:
                cycle_ticks = u16_be(data, 12)
                timeout_ticks = u16_be(data, 14)
                return " | ".join([
                    f"RX message timeout — {name} (COM PDU id {object_id}) on {bus_text}",
                    f"current status: {status_text}",
                    f"expected RX every {cycle_ticks} ticks, declared lost after {timeout_ticks} ticks",
                    f"ECU main-loop count when captured: {main_cycles}",
                    f"messages timed out right now: {active_count}",
                    f"total timeout events since power-up: {event_count}",
                ])
            value = u32_be(data, 16)
            invalid_value = u32_be(data, 20)
            return " | ".join([
                f"RX signal invalid — signal id {object_id} on {bus_text}",
                f"current status: {status_text}",
                f"last received value: {value} (0x{value:08X})",
                f"value rejected as invalid: {invalid_value} (0x{invalid_value:08X})",
                f"ECU main-loop count when captured: {main_cycles}",
                f"signals invalid right now: {active_count}",
                f"total invalid events since power-up: {event_count}",
            ])
        # Extended data (16 bytes): a smaller subset of the same fields.
        if len(data) < 16:
            return ""
        bus_text = GATEWAY_BUS_TEXT.get(data[1], f"bus {data[1]}")
        object_id = u16_be(data, 2)
        value = u32_be(data, 4)
        threshold = u32_be(data, 8)
        main_cycles = u32_be(data, 12)
        if is_message:
            return " | ".join([
                f"RX message timeout — {name} (COM PDU id {object_id}) on {bus_text}",
                f"timeout limit: {threshold} ticks",
                f"ECU main-loop count when captured: {main_cycles}",
            ])
        return " | ".join([
            f"RX signal invalid — signal id {object_id} on {bus_text}",
            f"last received value: {value} (0x{value:08X})",
            f"value rejected as invalid: {threshold} (0x{threshold:08X})",
            f"ECU main-loop count when captured: {main_cycles}",
        ])

    def _explain_mcusm_detail(self, data, is_snapshot):
        # SysMgr_CaptureMcuSmFreezeFrame / ...ExtendedData layouts.
        if is_snapshot:
            if len(data) < 32:
                return ""
            event_id = u16_be(data, 0)
            fault_source = data[2]
            wake_reason = data[3]
            reset_reason = u32_be(data, 4)
            info_field = u32_be(data, 8)
            pms_wcr2 = u32_be(data, 12)
            pms_wstat2 = u32_be(data, 16)
            scr_pending = data[23]
            # buffer[8] holds the SafetyKit failure mask only when there was no MCU
            # reset and the mask is set; otherwise it is the last reset information.
            if reset_reason == 0 and info_field != 0:
                info_label = "SafetyKit failure mask"
            else:
                info_label = "last reset information"
            return " | ".join([
                f"MCU safety-manager software error (event id {event_id})",
                f"fault source: {flag_names(fault_source, MCUSM_FAULT_SOURCE_BITS)} (0x{fault_source:02X})",
                f"last MCU reset reason: 0x{reset_reason:08X}",
                f"{info_label}: 0x{info_field:08X}",
                f"PMS wake-config reg2 (PMSWCR2): 0x{pms_wcr2:08X}",
                f"PMS wake-status reg2 (PMSWSTAT2): 0x{pms_wstat2:08X}",
                f"SCR fault/NMI/RST status: 0x{data[20]:02X}/0x{data[21]:02X}/0x{data[22]:02X}",
                f"SCR fault pending: {flag_names(scr_pending, MCUSM_SCR_FAULT_PENDING_BITS)} (0x{scr_pending:02X})",
                f"SCR wake reason: 0x{wake_reason:02X}",
                f"SCR-fault wake-ups: {u32_be(data, 24)}",
                f"go-to-sleep count: {u32_be(data, 28)}",
            ])
        if len(data) < 16:
            return ""
        event_id = u16_be(data, 0)
        fault_source = data[2]
        scr_pending = data[15]
        return " | ".join([
            f"MCU safety-manager software error (event id {event_id})",
            f"fault source: {flag_names(fault_source, MCUSM_FAULT_SOURCE_BITS)} (0x{fault_source:02X})",
            f"SCR wake reason: 0x{data[3]:02X}",
            f"PMS wake-config reg2 (PMSWCR2): 0x{u32_be(data, 4):08X}",
            f"PMS wake-status reg2 (PMSWSTAT2): 0x{u32_be(data, 8):08X}",
            f"SCR fault/NMI/RST status: 0x{data[12]:02X}/0x{data[13]:02X}/0x{data[14]:02X}",
            f"SCR fault pending: {flag_names(scr_pending, MCUSM_SCR_FAULT_PENDING_BITS)} (0x{scr_pending:02X})",
        ])

    def _explain_default_detail(self, data, is_snapshot):
        # Dem_DefaultFreezeFrameCapture / ...Capture: only the event id is meaningful.
        if len(data) < 4:
            return ""
        event_id = u16_be(data, 0)
        name = DEM_EVENT_ID_NAMES.get(event_id, f"event id {event_id}")
        if is_snapshot:
            return (
                f"Generic snapshot — {name} (event id {event_id}); "
                "no signal data is captured for this event, remaining bytes are reserved/zero"
            )
        return (
            f"Generic extended record #{data[3]} — {name} (event id {event_id}); "
            "occurrence/aging bookkeeping only, no payload"
        )

    def _decode_read_dtc_response(self, label, response):
        if len(response) < 2 or response[0] != 0x59:
            raise FcdError(f"{label}: malformed ReadDTCInformation response {bytes_to_hex(response)}")

        subfunction = response[1]
        payload = response[2:]
        if subfunction == 0x01:
            if len(payload) < 4:
                raise FcdError(f"{label}: short DTC count response {bytes_to_hex(response)}")
            availability = payload[0]
            fmt = payload[1]
            count = u16_be(payload, 2)
            summary = (
                f"19 01 FF: count={count}, availability=0x{availability:02X}, "
                f"format=0x{fmt:02X}"
            )
            return [], summary

        if subfunction == 0x02:
            if len(payload) < 1:
                raise FcdError(f"{label}: missing DTC status availability mask")
            availability = payload[0]
            records = payload[1:]
            if (len(records) % 4) != 0:
                raise FcdError(f"{label}: DTC record length is not a multiple of 4")
            rows = []
            for offset in range(0, len(records), 4):
                dtc = (records[offset] << 16) | (records[offset + 1] << 8) | records[offset + 2]
                status = records[offset + 3]
                rows.append(
                    {
                        "source": "19 02",
                        "dtc_value": dtc,
                        "dtc": f"0x{dtc:06X}",
                        "status": f"0x{status:02X}",
                        "decoded": self._decode_dtc_status(status),
                        "description": self._describe_zgw_dtc(dtc),
                    }
                )
            return rows, f"19 02: records={len(rows)}, availability=0x{availability:02X}"

        raise FcdError(f"{label}: unsupported positive subfunction 0x{subfunction:02X}")

    def _decode_dtc_status(self, status):
        bits = [name for mask, name in DTC_STATUS_TEXT if (status & mask) != 0]
        return ", ".join(bits) if bits else "none"

    def _describe_zgw_dtc(self, dtc):
        if dtc in STATIC_DTC_DESCRIPTIONS:
            return STATIC_DTC_DESCRIPTIONS[dtc]

        if DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT <= dtc < (DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT + DEM_GATEWAY_RX_MESSAGE_EVENT_COUNT):
            index = dtc - DEM_DTC_GATEWAY_RX_MESSAGE_TIMEOUT
            name = CODING_PARAMETER_NAMES[index] if index < len(CODING_PARAMETER_NAMES) else f"RX message index {index}"
            return f"Message Timeout: {name}"

        if DEM_DTC_GATEWAY_RX_SIGNAL_INVALID <= dtc:
            index = dtc - DEM_DTC_GATEWAY_RX_SIGNAL_INVALID
            return f"Gateway RX signal invalid: signal diagnostic index {index}"

        return "Unknown ZGW APP DTC"

    def read_did_clicked(self):
        try:
            did = parse_int(self.coding_did_var.get())
            request = bytes([0x22, (did >> 8) & 0xFF, did & 0xFF])
        except Exception as exc:
            messagebox.showerror(APP_NAME, str(exc))
            return
        self.send_uds_async(request, f"Read DID {int_hex(did)}")

    def write_did_clicked(self):
        try:
            did = parse_int(self.coding_did_var.get())
            data = hex_to_bytes(self.coding_write_data_var.get())
            request = bytes([0x2E, (did >> 8) & 0xFF, did & 0xFF]) + data
        except Exception as exc:
            messagebox.showerror(APP_NAME, str(exc))
            return
        if not messagebox.askyesno(APP_NAME, f"Send WriteDataByIdentifier for DID {int_hex(did)}?"):
            return
        self.send_uds_async(request, f"Write DID {int_hex(did)}")

    def add_coding_param_clicked(self):
        item = self._coding_param_dialog()
        if item:
            self.coding_tree.insert("", "end", values=(item["param"], item["index"], item["expected"], item["comment"]))

    def edit_coding_param_clicked(self):
        selection = self.coding_tree.selection()
        if not selection:
            return
        values = self.coding_tree.item(selection[0], "values")
        item = self._coding_param_dialog({"param": values[0], "index": values[1], "expected": values[2], "comment": values[3]})
        if item:
            self.coding_tree.item(selection[0], values=(item["param"], item["index"], item["expected"], item["comment"]))

    def remove_coding_param_clicked(self):
        for iid in self.coding_tree.selection():
            self.coding_tree.delete(iid)

    def _coding_param_dialog(self, existing=None):
        existing = existing or {"param": "", "index": "", "expected": "1", "comment": ""}
        dialog = tk.Toplevel(self.root)
        dialog.title("Coding Parameter")
        dialog.transient(self.root)
        dialog.grab_set()
        result = {}

        vars_ = {
            "param": tk.StringVar(value=existing.get("param", "")),
            "index": tk.StringVar(value=existing["index"]),
            "expected": tk.StringVar(value=existing["expected"]),
            "comment": tk.StringVar(value=existing["comment"]),
        }

        # Auto-fill the Coding Parameters name from the index (and vice versa) so a
        # manually added row stays in sync with CODING_PARAMETER_NAMES.
        def sync_name_from_index(*_args):
            try:
                idx = parse_int(vars_["index"].get())
            except (ValueError, TypeError):
                return
            if 0 <= idx < len(CODING_PARAMETER_NAMES):
                vars_["param"].set(CODING_PARAMETER_NAMES[idx])

        def sync_index_from_name(*_args):
            idx = CODING_PARAMETER_INDEX.get(vars_["param"].get().strip())
            if idx is not None:
                vars_["index"].set(str(idx))

        vars_["index"].trace_add("write", sync_name_from_index)
        vars_["param"].trace_add("write", sync_index_from_name)

        labels = {"param": "Coding Parameter", "index": "Index", "expected": "Expected", "comment": "Comment"}
        for row, key in enumerate(["param", "index", "expected", "comment"]):
            ttk.Label(dialog, text=labels[key]).grid(row=row, column=0, sticky="w", padx=8, pady=6)
            ttk.Entry(dialog, textvariable=vars_[key], width=72).grid(row=row, column=1, sticky="ew", padx=8, pady=6)

        def ok():
            result.update({key: var.get().strip() for key, var in vars_.items()})
            dialog.destroy()

        ttk.Button(dialog, text="OK", command=ok).grid(row=4, column=0, padx=8, pady=10)
        ttk.Button(dialog, text="Cancel", command=dialog.destroy).grid(row=4, column=1, sticky="w", padx=8, pady=10)
        dialog.wait_window()
        return result or None

    def import_coding_clicked(self):
        path = filedialog.askopenfilename(filetypes=[("JSON", "*.json"), ("All files", "*.*")])
        if not path:
            return
        with open(path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
        self.coding_tree.delete(*self.coding_tree.get_children())
        mask_hex = data.get("mask_hex") or data.get("mask")
        if mask_hex:
            self._load_mask_to_coding_tree(hex_to_bytes(mask_hex))
            return
        for item in data.get("parameters", []):
            index_value = item.get("index", item.get("path", ""))
            param_name = item.get("param", "")
            if not param_name:
                try:
                    idx = parse_int(index_value)
                    param_name = CODING_PARAMETER_NAMES[idx] if 0 <= idx < len(CODING_PARAMETER_NAMES) else ""
                except (ValueError, TypeError):
                    param_name = ""
            self.coding_tree.insert("", "end", values=(param_name, index_value, item.get("expected", item.get("value", "")), item.get("comment", "")))
        self.log(f"Imported coding JSON: {path}")

    def export_coding_clicked(self):
        path = filedialog.asksaveasfilename(defaultextension=".json", filetypes=[("JSON", "*.json")])
        if not path:
            return
        parameters = []
        for iid in self.coding_tree.get_children():
            values = self.coding_tree.item(iid, "values")
            parameters.append({"param": values[0], "index": values[1], "expected": values[2], "comment": values[3]})
        data = {
            "schema": "FCD_CODING_CONFIG_v1",
            "created": datetime.now().isoformat(timespec="seconds"),
            "vin": self.fa_vin_var.get(),
            "coding_did": self.coding_mask_did_var.get(),
            "mask_hex": bytes_to_hex(self._coding_rows_to_mask()) if parameters else "",
            "parameters": parameters,
        }
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(data, handle, indent=2)
        self.log(f"Exported coding JSON: {path}")

    def _coding_rows_to_mask(self):
        bits = {}
        max_index = -1
        for iid in self.coding_tree.get_children():
            values = self.coding_tree.item(iid, "values")
            if not str(values[1]).strip():
                continue
            idx = parse_int(values[1])
            expected_text = str(values[2]).strip().lower()
            expected = expected_text in ("1", "true", "yes", "y", "on", "expected")
            if idx < 0:
                raise FcdError("Coding index must be non-negative")
            bits[idx] = expected
            max_index = max(max_index, idx)
        if max_index < 0:
            raise FcdError("No coding parameters defined")
        mask = bytearray((max_index + 8) // 8)
        for idx, expected in bits.items():
            if expected:
                mask[idx >> 3] |= 1 << (idx & 0x07)
        return bytes(mask)

    def _load_mask_to_coding_tree(self, mask, source="ECU coding routine 0x0203"):
        self.coding_tree.delete(*self.coding_tree.get_children())
        if not mask:
            self.coding_tree.insert("", "end", values=("", "", "", "ECU returned no coding mask bytes"))
            return
        total_bits = len(mask) * 8
        # Show every known Coding Parameter plus any extra mask bits so the table is a
        # complete, named view of the coding values read back from the ECU.
        row_count = max(total_bits, len(CODING_PARAMETER_NAMES))
        for idx in range(row_count):
            if idx < total_bits:
                expected = (mask[idx >> 3] & (1 << (idx & 0x07))) != 0
            else:
                expected = False
            if idx < len(CODING_PARAMETER_NAMES):
                name = CODING_PARAMETER_NAMES[idx]
            else:
                name = f"(reserved bit {idx})"
            self.coding_tree.insert(
                "", "end",
                values=(name, str(idx), "1" if expected else "0", f"read from {source}"),
            )

    def _tolerant_ecu_reset(self, client, reason):
        # A hard reset response is best-effort: it may be a clean 51 01, a stale
        # frame left in the buffer, or nothing at all. The ZGW defers the actual
        # reset briefly after 51 01 so Ethernet can transmit the response, so wait
        # before reconnecting or we can reconnect to the old pre-reset application.
        req = b"\x11\x01"
        self.log(f"TX ECUReset hardReset {reason}: {bytes_to_hex(req)}")
        try:
            resp = client.send_uds(req, timeout=5.0)
            self.log(f"RX ECUReset hardReset {reason}: {bytes_to_hex(resp)}")
        except Exception as exc:
            self.log(f"ECUReset hardReset {reason}: no clean response ({exc})")
        if isinstance(client, DoipClient):
            self.log(
                f"ECUReset hardReset {reason}: waiting {POST_RESET_RECONNECT_DELAY_SECONDS:.1f} s "
                "before DoIP reconnect"
            )
            if self.worker_stop.wait(POST_RESET_RECONNECT_DELAY_SECONDS):
                raise FcdError(f"ECUReset hardReset {reason}: cancelled by Disconnect button")
            self.reconnect_doip(client, reason)
            self._wait_for_post_reset_uds_ready(client, reason)

    def _wait_for_post_reset_uds_ready(self, client, reason, timeout=POST_RESET_UDS_READY_TIMEOUT_SECONDS):
        request = bytes([0x10, SESSION_DEFAULT])
        deadline = time.monotonic() + float(timeout)
        attempt = 0
        last_error = None

        if not isinstance(client, DoipClient):
            return

        self.log(f"Post-reset UDS readiness after {reason}: waiting up to {timeout:.1f} s")

        while not self._reconnect_cancelled(client):
            remaining = deadline - time.monotonic()
            if remaining <= 0.0:
                break

            attempt += 1
            try:
                if attempt == 1 or attempt % 5 == 0:
                    self.log(f"TX Post-reset Default Session probe: {bytes_to_hex(request)}")
                response = client.send_uds(request, timeout=max(0.5, min(2.0, remaining)))
                if attempt == 1 or attempt % 5 == 0:
                    self.log(f"RX Post-reset Default Session probe: {bytes_to_hex(response)}")
                require_positive_response(response, 0x10)
                self.log(f"Post-reset UDS ready after {reason}")
                return
            except Exception as exc:
                last_error = exc
                if attempt == 1 or attempt % 5 == 0:
                    self.log(f"Post-reset UDS not ready after {reason}: attempt {attempt} failed ({exc})")

                client.close()
                self.root.after(0, self._set_connected_status, False)

                remaining = deadline - time.monotonic()
                if remaining <= 0.0:
                    break
                if self.worker_stop.wait(min(POST_RESET_UDS_READY_RETRY_SECONDS, remaining)):
                    raise FcdError(f"Post-reset UDS readiness after {reason}: cancelled by Disconnect button")

                try:
                    self.reconnect_doip(
                        client,
                        f"{reason} UDS readiness",
                        deadline=deadline,
                    )
                except FcdError as reconnect_exc:
                    last_error = reconnect_exc
                    break

        raise FcdError(f"Post-reset UDS did not become ready after {reason}: {last_error}")

    def _parse_read_coding_result(self, result_resp):
        # Strip 0x71 + sub-function echo + 2-byte routine id, then the 10-byte status
        # block (CodingApp_FillRoutineResponse); the remainder is the coding bitmask.
        routine_data = result_resp[4:] if len(result_resp) > 4 else b""
        if len(routine_data) < CODING_ROUTINE_STATUS_LEN:
            raise FcdError(
                f"Read Coding routine returned {len(routine_data)} bytes; "
                f"expected at least {CODING_ROUTINE_STATUS_LEN} status bytes"
            )
        return routine_data[CODING_ROUTINE_STATUS_LEN:]

    def _coding_routine_status(self, response, label):
        require_positive_response(response, 0x31)
        if len(response) < 5:
            raise FcdError(f"{label} returned no CodingApp routine status byte")
        return response[4]

    def _describe_coding_routine_response(self, response):
        if not isinstance(response, bytes) or len(response) < 14:
            return f"raw={bytes_to_hex(response) if isinstance(response, bytes) else response}"

        status = response[4]
        state = response[5]
        validation = response[6]
        dirty = response[7]
        rx_expected = u16_be(response, 8)
        generation = u32_be(response, 10)
        return (
            f"status={CODING_ROUTINE_STATUS_TEXT.get(status, int_hex(status, 2))}, "
            f"state={CODING_STATE_TEXT.get(state, int_hex(state, 2))}, "
            f"validation={CODING_VALIDATION_TEXT.get(validation, int_hex(validation, 2))}, "
            f"dirty={dirty}, rxExpected={rx_expected}, generation=0x{generation:08X}"
        )

    def _decode_coding_status_payload(self, payload):
        if len(payload) < 20:
            return f"short payload len={len(payload)} data={bytes_to_hex(payload)}"

        initialized = payload[0]
        state = payload[1]
        validation = payload[2]
        dirty = payload[3]
        rx_count = u16_be(payload, 4)
        rx_expected = u16_be(payload, 6)
        nv_len = u16_be(payload, 8)
        generation = u32_be(payload, 10)
        validation_counter = u32_be(payload, 14)
        last_nvm_result = payload[18]
        pending_job = payload[19]
        text = (
            f"initialized={initialized}, "
            f"state={CODING_STATE_TEXT.get(state, int_hex(state, 2))}, "
            f"validation={CODING_VALIDATION_TEXT.get(validation, int_hex(validation, 2))}, "
            f"dirty={dirty}, rxCount={rx_count}, rxExpected={rx_expected}, "
            f"nvLen={nv_len}, generation=0x{generation:08X}, "
            f"validationCounter={validation_counter}, "
            f"lastNvMResult={NVM_RESULT_TEXT.get(last_nvm_result, int_hex(last_nvm_result, 2))}, "
            f"pendingNvMJob={CODING_NVM_JOB_TEXT.get(pending_job, int_hex(pending_job, 2))}"
        )

        if len(payload) >= 39:
            nvm_status = payload[20]
            fee_status = payload[21]
            fee_result = payload[22]
            fls_status = payload[23]
            fls_result = payload[24]
            poll_count = payload[25]
            poll_limit = payload[26]
            active_index = u16_be(payload, 27)
            active_block = u16_be(payload, 29)
            blocks_planned = u16_be(payload, 31)
            blocks_started = u16_be(payload, 33)
            blocks_written = u16_be(payload, 35)
            blocks_failed = u16_be(payload, 37)
            text += (
                f", NvM={NVM_STATUS_TEXT.get(nvm_status, int_hex(nvm_status, 2))}, "
                f"Fee={MEMIF_STATUS_TEXT.get(fee_status, int_hex(fee_status, 2))}/"
                f"{MEMIF_JOB_TEXT.get(fee_result, int_hex(fee_result, 2))}, "
                f"Fls={MEMIF_STATUS_TEXT.get(fls_status, int_hex(fls_status, 2))}/"
                f"{MEMIF_JOB_TEXT.get(fls_result, int_hex(fls_result, 2))}, "
                f"pendingPolls={poll_count}/{poll_limit}, "
                f"writeAllActiveIndex={active_index}, writeAllActiveBlock={active_block}, "
                f"blocks planned/started/written/failed="
                f"{blocks_planned}/{blocks_started}/{blocks_written}/{blocks_failed}"
            )

        return text

    def _read_coding_app_status(self, send, prefix, required=False):
        try:
            response = send(
                b"\x22" + struct.pack(">H", CODING_DID_STATUS),
                f"{prefix}: Read CodingApp Status F1C0",
                timeout=2.0,
            )
            payload = response[3:] if len(response) >= 3 else b""
            self.log(f"{prefix}: CodingApp status decoded: {self._decode_coding_status_payload(payload)}")
        except Exception as exc:
            self.log(f"{prefix}: CodingApp status read failed: {exc}")
            if required:
                raise FcdError(f"{prefix}: CodingApp status read failed after reset") from exc

    def _run_coding_routine(self, send, rid, option, label, timeout=60.0, max_pending_polls=10):
        start_resp = send(
            b"\x31\x01" + struct.pack(">H", rid) + bytes(option),
            f"RoutineControl {rid:04X} {label} start",
            timeout=timeout,
        )
        status = self._coding_routine_status(start_resp, label)
        last_resp = start_resp
        deadline = time.monotonic() + timeout
        pending_polls = 0

        while status == CODING_ROUTINE_STATUS_PENDING:
            if self.worker_stop.is_set():
                raise FcdError(f"{label} cancelled")
            if pending_polls >= max_pending_polls:
                raise FcdError(
                    f"{label} stayed pending after {pending_polls} result polls; "
                    f"{self._describe_coding_routine_response(last_resp)}"
                )
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise FcdError(f"{label} did not finish before timeout")
            time.sleep(min(1.0, remaining))
            try:
                result_resp = send(
                    b"\x31\x03" + struct.pack(">H", rid),
                    f"RoutineControl {rid:04X} {label} result",
                    timeout=max(1.0, min(2.0, remaining)),
                )
                pending_polls += 1
                last_resp = result_resp
            except TimeoutError:
                pending_polls += 1
                self.log(f"{label}: result poll timed out; still waiting")
                continue
            except NegativeResponse as exc:
                if exc.sid == 0x31 and exc.nrc == 0x7F:
                    raise FcdError(f"{label}: coding session expired while routine was pending") from exc
                raise
            status = self._coding_routine_status(result_resp, label)

        if status not in (CODING_ROUTINE_STATUS_OK, CODING_ROUTINE_STATUS_NOT_CHANGED):
            raise FcdError(f"{label} failed with CodingApp routine status 0x{status:02X}")

        return start_resp

    def code_ecu_clicked(self):
        write_rid = parse_int(self.coding_write_all_rid_var.get())
        check_rid = parse_int(self.coding_validate_rid_var.get())
        read_rid = parse_int(self.coding_read_nvm_rid_var.get())
        # Build the mask from the table up front so a malformed row aborts before we
        # touch the ECU. The mask is pushed as the Write Coding routine option record.
        mask = self._coding_rows_to_mask()

        def action():
            client = self.require_client()
            # Pause automatic tester present for the whole sequence: it shares the DoIP
            # socket and would race the worker's requests (causing frame desync), and it
            # would flood errors while the ECU is reset and TCP is briefly down.
            keepalive_was_on = bool(self.keepalive_var.get()) or self.keepalive_is_running()
            if keepalive_was_on:
                self.stop_keepalive(update_var=False)
                self.log("Code ECU: paused automatic tester present for the coding sequence")

            # Clear any frame left buffered before the keepalive was stopped, so the
            # first request below reads its own response and not a stale one.
            client.drain()
            sequence_ok = False
            send = None

            try:
                # The full coding procedure is always sent for real - coding is never a
                # dry run. dry=False guards against an active Flash-tab dry-run checkbox.
                send = self._make_uds_sender(client, dry=False)

                # --- Code the ECU from the coding session ---
                self._ensure_session(send, SESSION_CODING_REQUESTED, "Coding Session requested as 10 41")
                self._run_coding_routine(send, write_rid, mask, "Write Coding", timeout=90.0)
                self._run_coding_routine(send, check_rid, b"", "Check Coding", timeout=20.0)

                # --- Hard reset so the ECU re-evaluates the persisted coding ---
                self._tolerant_ecu_reset(client, "Code ECU reset")
                # A fresh sender after reconnect keeps using the live socket.
                send = self._make_uds_sender(client, dry=False)

                # --- Read back the standard status and the persisted coding ---
                self._send_session(send, SESSION_EXTENDED, "Extended Session after coding reset")
                self._read_coding_app_status(send, "Code ECU", required=True)
                for did, name in [
                    (DID_APP_SW_VERSION, "Read Software Version F101"),
                    (DID_ACTIVE_SW_BLOCK, "Read Active Software Block F100"),
                    (DID_ACTIVE_DIAG_SESSION, "Read Active Diagnostic Session F186"),
                ]:
                    send(b"\x22" + struct.pack(">H", did), f"Code ECU: {name}")

                send(
                    b"\x31\x01" + struct.pack(">H", read_rid),
                    "RoutineControl 0203 Read Coding start",
                    timeout=10.0,
                )
                result_resp = send(
                    b"\x31\x03" + struct.pack(">H", read_rid),
                    "RoutineControl 0203 Read Coding result",
                    timeout=10.0,
                )
                coding_payload = self._parse_read_coding_result(result_resp)
                self.root.after(0, self._load_mask_to_coding_tree, coding_payload)

                # --- Leave the ECU in the default session ---
                self._send_session(send, SESSION_DEFAULT, "Default Session")
                sequence_ok = True
            except Exception:
                if send is not None and self.client is client and client.connected:
                    self._read_coding_app_status(send, "Code ECU")
                raise
            finally:
                if keepalive_was_on and sequence_ok and self.client is not None and self.client.connected:
                    self.start_keepalive()
                elif (not sequence_ok) and (self.client is client) and (not client.connected):
                    self.client = None
                    self.root.after(0, self._set_connected_status, False)

        self.worker("Code ECU", action)

    def read_current_coding_clicked(self):
        rid = parse_int(self.coding_read_nvm_rid_var.get())

        def action():
            client = self.require_client()
            keepalive_was_on = bool(self.keepalive_var.get()) or self.keepalive_is_running()
            if keepalive_was_on:
                self.stop_keepalive(update_var=False)
                self.log("Read Coding: paused automatic tester present for the read sequence")

            client.drain()
            sequence_ok = False

            try:
                # Enter the extended session first, the same way the post-flash readback
                # flow (_execute_final_readback) does, so coding is read from a diagnostic
                # session rather than the default session of a fresh ethernet connection.
                session_req = bytes([0x10, SESSION_EXTENDED])
                self.log(f"TX Extended Session: {bytes_to_hex(session_req)}")
                session_resp = client.send_uds(session_req, timeout=10.0)
                self.log(f"RX Extended Session: {bytes_to_hex(session_resp)}")
                require_positive_response(session_resp, 0x10)

                # Coding is read back through the CodingApp read-NVM RoutineControl
                # (0x0203): 31 01 starts it, 31 03 fetches the result. CodingApp returns a
                # 10-byte status block (CodingApp_FillRoutineResponse) followed by the
                # rxMessageExpected bitmask - see CodingApp_RoutineControl in CodingApp.c.
                start_req = b"\x31\x01" + struct.pack(">H", rid)
                self.log(f"TX Read Coding start: {bytes_to_hex(start_req)}")
                start_resp = client.send_uds(start_req, timeout=20.0)
                self.log(f"RX Read Coding start: {bytes_to_hex(start_resp)}")
                require_positive_response(start_resp, 0x31)

                result_req = b"\x31\x03" + struct.pack(">H", rid)
                self.log(f"TX Read Coding result: {bytes_to_hex(result_req)}")
                result_resp = client.send_uds(result_req, timeout=20.0)
                self.log(f"RX Read Coding result: {bytes_to_hex(result_resp)}")
                require_positive_response(result_resp, 0x31)

                coding_payload = self._parse_read_coding_result(result_resp)
                self.root.after(0, self._load_mask_to_coding_tree, coding_payload)
                sequence_ok = True
            finally:
                if keepalive_was_on and sequence_ok and self.client is not None and self.client.connected:
                    self.start_keepalive()
                elif (not sequence_ok) and (self.client is client) and (not client.connected):
                    self.client = None
                    self.root.after(0, self._set_connected_status, False)

        self.worker("Read Coding", action)

    def save_current_coding_clicked(self):
        def action():
            client = self.require_client()
            # Coding DIDs are only readable from a non-default session; enter the
            # extended session first (a fresh ethernet connection starts in default).
            # Coding is never a dry run, so send the session request for real.
            send = self._make_uds_sender(client, dry=False)
            self._send_session(send, SESSION_EXTENDED, "Extended Session")
            dids = [parse_int(self.coding_status_did_var.get()), parse_int(self.coding_mask_did_var.get()), parse_int(self.coding_version_did_var.get())]
            data = {"schema": "FCD_CODING_CURRENT_v1", "created": datetime.now().isoformat(timespec="seconds"), "vin": self.fa_vin_var.get(), "responses": {}}
            for did in dids:
                req = bytes([0x22, (did >> 8) & 0xFF, did & 0xFF])
                resp = client.send_uds(req, timeout=float(self.timeout_var.get()))
                require_positive_response(resp, 0x22)
                data["responses"][int_hex(did)] = bytes_to_hex(resp[3:] if len(resp) >= 3 else resp)
            path = filedialog.asksaveasfilename(defaultextension=".json", filetypes=[("JSON", "*.json")])
            if path:
                Path(path).write_text(json.dumps(data, indent=2), encoding="utf-8")
                self.log(f"Saved current coding: {path}")

        self.worker("Save Current Coding", action)

    def write_coding_config_clicked(self):
        if not messagebox.askyesno(APP_NAME, "Write coding via CodingApp routine 0x0202?"):
            return

        def action():
            client = self.require_client()
            # Write/check coding run from the coding session (10 41), matching the
            # post-flash coding flow (_execute_coding_after_flash). Coding is never a
            # dry run, so the session request is always sent for real.
            send = self._make_uds_sender(client, dry=False)
            try:
                self._ensure_session(send, SESSION_CODING_REQUESTED, "Coding Session requested as 10 41")
                mask = self._coding_rows_to_mask()
                # The coding mask is delivered as the option record of the Write Coding
                # routine start (0x0202). WriteDataByIdentifier (0x2E) is no longer
                # supported by the ZGW - writes go through RoutineControl. Check Coding
                # (0x0201) takes no option record.
                self._run_coding_routine(send, parse_int(self.coding_write_all_rid_var.get()), mask, "Write Coding", timeout=90.0)
                self._run_coding_routine(send, parse_int(self.coding_validate_rid_var.get()), b"", "Check Coding", timeout=20.0)
            except Exception:
                if self.client is client and client.connected:
                    self._read_coding_app_status(send, "Write Coding")
                raise

        self.worker("Write Coding", action)

    def load_default_coding_clicked(self):
        if not messagebox.askyesno(APP_NAME, "Load ECU default coding via routine 0x0204?"):
            return
        rid = parse_int(self.coding_defaults_rid_var.get())

        def action():
            client = self.require_client()
            # Loading default coding is a coding write routine; run it from the coding
            # session (10 41) so the ZGW does not reject 0x31 in the default session.
            # Coding is never a dry run - send the session and routine for real.
            send = self._make_uds_sender(client, dry=False)
            self._ensure_session(send, SESSION_CODING_REQUESTED, "Coding Session requested as 10 41")
            send(b"\x31\x01" + struct.pack(">H", rid), "Load Coding Default", timeout=20.0)

        self.worker("Load Coding Default", action)

    def check_coding_clicked(self):
        rid = parse_int(self.coding_validate_rid_var.get())
        def action():
            client = self.require_client()
            # Check coding runs from the coding session (10 41), matching the
            # post-flash coding flow (_execute_coding_after_flash). Coding is never a
            # dry run, so the session request is always sent for real.
            send = self._make_uds_sender(client, dry=False)
            self._ensure_session(send, SESSION_CODING_REQUESTED, "Coding Session requested as 10 41")
            for control_type, label in [(0x01, "start"), (0x03, "result")]:
                req = bytes([0x31, control_type]) + struct.pack(">H", rid)
                self.log(f"TX Check Coding {label}: {bytes_to_hex(req)}")
                resp = client.send_uds(req, timeout=20.0)
                self.log(f"RX Check Coding {label}: {bytes_to_hex(resp)}")
                require_positive_response(resp, 0x31)
        self.worker("Check Coding", action)

    def browse_gen_output_clicked(self):
        path = filedialog.askdirectory(initialdir=self.gen_output_var.get() or str(Path.cwd()))
        if path:
            self.gen_output_var.set(path)

    def add_hex_clicked(self):
        paths = filedialog.askopenfilenames(filetypes=[("Intel HEX", "*.hex;*.ihex"), ("All files", "*.*")])
        for path in paths:
            hex_path = Path(path)
            try:
                segments = parse_intel_hex(hex_path)
                if not segments:
                    raise FcdError(f"No data records in {hex_path}")
                base = int_hex(hex_download_base(segments), 8)
            except Exception as exc:
                messagebox.showerror(APP_NAME, f"{hex_path}: {exc}")
                continue
            stem = hex_path.stem
            row = {
                "ecu": stem.upper(),
                "target": int_hex(DEFAULT_TARGET_ADDR),
                "req": "0x710",
                "resp": "0x711",
                "did": "0xF186",
                "base": base,
                "hex": str(hex_path),
            }
            self._insert_generator_row(row)

    def _insert_generator_row(self, row):
        iid = self.generator_tree.insert(
            "",
            "end",
            values=(row["ecu"], row["base"], row["hex"]),
        )
        self.generator_rows[iid] = row

    def edit_hex_clicked(self):
        selection = self.generator_tree.selection()
        if not selection:
            return
        iid = selection[0]
        row = self.generator_rows.get(iid)
        if not row:
            return
        updated = self._hex_row_dialog(row)
        if updated:
            self.generator_rows[iid] = updated
            self.generator_tree.item(
                iid,
                values=(
                    updated["ecu"],
                    updated["base"],
                    updated["hex"],
                ),
            )

    def remove_hex_clicked(self):
        for iid in self.generator_tree.selection():
            self.generator_rows.pop(iid, None)
            self.generator_tree.delete(iid)

    def _hex_row_dialog(self, row):
        dialog = tk.Toplevel(self.root)
        dialog.title("HEX ECU Mapping")
        dialog.transient(self.root)
        dialog.grab_set()
        result = {}
        fields = ["ecu", "target", "req", "resp", "did", "base", "hex"]
        labels = {
            "ecu": "ECU name",
            "target": "DoIP target",
            "req": "CAN request ID",
            "resp": "CAN response ID",
            "did": "Coding DID",
            "base": "Download base",
            "hex": "HEX file",
        }
        vars_ = {key: tk.StringVar(value=row.get(key, "")) for key in fields}
        for idx, key in enumerate(fields):
            ttk.Label(dialog, text=labels[key]).grid(row=idx, column=0, sticky="w", padx=8, pady=5)
            ttk.Entry(dialog, textvariable=vars_[key], width=78).grid(row=idx, column=1, sticky="ew", padx=8, pady=5)
            if key == "hex":
                ttk.Button(dialog, text="Browse", command=lambda: self._browse_dialog_file(vars_["hex"])).grid(
                    row=idx, column=2, padx=6, pady=5
                )

        def ok():
            candidate = {key: vars_[key].get().strip() for key in fields}
            try:
                parse_int(candidate["target"])
                if candidate["req"]:
                    parse_int(candidate["req"])
                if candidate["resp"]:
                    parse_int(candidate["resp"])
                if candidate["did"]:
                    parse_int(candidate["did"])
                if candidate["base"]:
                    parse_int(candidate["base"])
                if not candidate["ecu"]:
                    raise ValueError("ECU name is required")
                if not candidate["hex"]:
                    raise ValueError("HEX file is required")
            except Exception as exc:
                messagebox.showerror(APP_NAME, str(exc), parent=dialog)
                return
            result.update(candidate)
            dialog.destroy()

        ttk.Button(dialog, text="OK", command=ok).grid(row=len(fields), column=0, padx=8, pady=10)
        ttk.Button(dialog, text="Cancel", command=dialog.destroy).grid(row=len(fields), column=1, sticky="w", padx=8, pady=10)
        dialog.wait_window()
        return result or None

    def _browse_dialog_file(self, var):
        path = filedialog.askopenfilename(filetypes=[("Intel HEX", "*.hex;*.ihex"), ("All files", "*.*")])
        if path:
            var.set(path)

    def generate_files_clicked(self):
        def action():
            if not self.generator_rows:
                raise FcdError("Add at least one HEX file")
            out_root = Path(self.gen_output_var.get()).expanduser()
            package_dir = out_root / f"FCD_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
            payload_dir = package_dir / "payloads"
            payload_dir.mkdir(parents=True, exist_ok=True)

            ecus = []
            payloads = []
            tal_steps = []
            step_no = 1

            for row in self.generator_rows.values():
                hex_path = Path(row["hex"])
                segments = parse_intel_hex(hex_path)
                if not segments:
                    raise FcdError(f"No data records in {hex_path}")

                derived_base = hex_download_base(segments)
                download_base = parse_int(row["base"]) if row.get("base") else derived_base
                base_delta = download_base - derived_base
                ecu = {
                    "name": row["ecu"],
                    "target_logical_address": int_hex(parse_int(row["target"])),
                    "can_request_id": int_hex(parse_int(row["req"])) if row.get("req") else "",
                    "can_response_id": int_hex(parse_int(row["resp"])) if row.get("resp") else "",
                    "coding_did": int_hex(parse_int(row["did"])) if row.get("did") else "",
                    "source_hex": str(hex_path),
                    "segments": [],
                }
                ecus.append(ecu)

                tal_steps.append({"step": step_no, "ecu": row["ecu"], "service": "DiagnosticSessionControl", "request": "10 02"})
                step_no += 1
                tal_steps.append({"step": step_no, "ecu": row["ecu"], "service": "RoutineControl EraseApp", "request": "31 01 00 01"})
                step_no += 1

                for index, segment in enumerate(segments):
                    address = download_address_from_hex_address(segment.address) + base_delta
                    name = f"{row['ecu']}_{index}_{address:08X}.bin".replace(" ", "_")
                    payload_path = payload_dir / name
                    payload_path.write_bytes(segment.data)
                    payload_entry = {
                        "ecu": row["ecu"],
                        "target_logical_address": int_hex(parse_int(row["target"])),
                        "address": int_hex(address, 8),
                        "size": len(segment.data),
                        "crc32": int_hex(binascii.crc32(segment.data) & 0xFFFFFFFF, 8),
                        "file": str(Path("payloads") / name),
                        "source_hex": str(hex_path),
                        "source_address": int_hex(segment.address, 8),
                    }
                    payloads.append(payload_entry)
                    ecu_segment = {key: value for key, value in payload_entry.items() if key != "data_base64"}
                    ecu["segments"].append(ecu_segment)
                    tal_steps.append(
                        {
                            "step": step_no,
                            "ecu": row["ecu"],
                            "service": "RequestDownload",
                            "address": int_hex(address, 8),
                            "size": len(segment.data),
                            "crc32": payload_entry["crc32"],
                        }
                    )
                    step_no += 1
                    tal_steps.append(
                        {
                            "step": step_no,
                            "ecu": row["ecu"],
                            "service": "TransferData",
                            "source": str(Path("payloads") / name),
                        }
                    )
                    step_no += 1
                    tal_steps.append({"step": step_no, "ecu": row["ecu"], "service": "RequestTransferExit"})
                    step_no += 1
                    tal_steps.append(
                        {
                            "step": step_no,
                            "ecu": row["ecu"],
                            "service": "RoutineControl CRC",
                            "rid": "0x0002",
                            "address": int_hex(address, 8),
                            "size": len(segment.data),
                            "crc32": payload_entry["crc32"],
                        }
                    )
                    step_no += 1

            fa = {
                "schema": "FCD_FA_v1",
                "created": datetime.now().isoformat(timespec="seconds"),
                "project": self.gen_project_var.get().strip(),
                "vin": self.gen_vin_var.get().strip(),
                "type": self.fa_type_var.get().strip(),
            }
            svt = {"schema": "FCD_SVT_v1", "created": fa["created"], "ecus": ecus}
            tal = {"schema": "FCD_TAL_v1", "created": fa["created"], "steps": tal_steps}
            psdz = {
                "schema": "FCD_PSDZDATA_v1",
                "created": fa["created"],
                "payload_format": "raw_bin",
                "payloads": payloads,
            }

            (package_dir / "fcd_fa.json").write_text(json.dumps(fa, indent=2), encoding="utf-8")
            (package_dir / "fcd_svt.json").write_text(json.dumps(svt, indent=2), encoding="utf-8")
            (package_dir / "fcd_tal.json").write_text(json.dumps(tal, indent=2), encoding="utf-8")
            (package_dir / "fcd_psdzdata.json").write_text(json.dumps(psdz, indent=2), encoding="utf-8")
            self.log(f"Generated FCD package: {package_dir}")
            self.root.after(0, lambda: self.pkg_dir_var.set(str(package_dir)))

        self.worker("Generate FCD Files", action)

    def copy_generator_to_svt(self):
        self.svt_tree.delete(*self.svt_tree.get_children())
        for row in self.generator_rows.values():
            size = ""
            crc = ""
            try:
                segments = parse_intel_hex(row["hex"])
                total = sum(len(seg.data) for seg in segments)
                crc_value = 0
                for seg in segments:
                    crc_value = binascii.crc32(seg.data, crc_value)
                size = str(total)
                crc = int_hex(crc_value & 0xFFFFFFFF, 8)
            except Exception:
                pass
            self.svt_tree.insert(
                "",
                "end",
                values=(row["ecu"], row["target"], row["req"], row["resp"], row["did"], row["hex"], size, crc),
            )

    def import_svt_clicked(self):
        path = filedialog.askopenfilename(filetypes=[("JSON", "*.json"), ("All files", "*.*")])
        if not path:
            return
        with open(path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
        self.svt_tree.delete(*self.svt_tree.get_children())
        for ecu in data.get("ecus", []):
            self.svt_tree.insert(
                "",
                "end",
                values=(
                    ecu.get("name", ""),
                    ecu.get("target_logical_address", ""),
                    ecu.get("can_request_id", ""),
                    ecu.get("can_response_id", ""),
                    ecu.get("coding_did", ""),
                    ecu.get("source_hex", ""),
                    sum(int(seg.get("size", 0)) for seg in ecu.get("segments", [])),
                    "",
                ),
            )
        self.log(f"Imported SVT: {path}")

    def export_svt_clicked(self):
        path = filedialog.asksaveasfilename(defaultextension=".json", filetypes=[("JSON", "*.json")])
        if not path:
            return
        ecus = []
        for iid in self.svt_tree.get_children():
            values = self.svt_tree.item(iid, "values")
            ecus.append(
                {
                    "name": values[0],
                    "target_logical_address": values[1],
                    "can_request_id": values[2],
                    "can_response_id": values[3],
                    "coding_did": values[4],
                    "source_hex": values[5],
                }
            )
        data = {"schema": "FCD_SVT_v1", "created": datetime.now().isoformat(timespec="seconds"), "ecus": ecus}
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(data, handle, indent=2)
        self.log(f"Exported SVT: {path}")

    def browse_package_clicked(self):
        path = filedialog.askdirectory(initialdir=self.pkg_dir_var.get() or self.gen_output_var.get() or str(Path.cwd()))
        if path:
            self.pkg_dir_var.set(path)

    def load_package_clicked(self):
        package_dir = Path(self.pkg_dir_var.get()).expanduser()
        psdz_path = package_dir / "fcd_psdzdata.json"
        if not psdz_path.exists():
            messagebox.showerror(APP_NAME, f"Missing {psdz_path}")
            return
        with psdz_path.open("r", encoding="utf-8") as handle:
            psdz = json.load(handle)
        self.package_dir = package_dir
        self.package_payloads = psdz.get("payloads", [])
        self.payload_enabled = {}
        self.payload_tree.delete(*self.payload_tree.get_children())
        for index, item in enumerate(self.package_payloads):
            iid = self.payload_tree.insert(
                "",
                "end",
                values=(
                    "☑",
                    item.get("ecu", ""),
                    item.get("target_logical_address", ""),
                    item.get("address", ""),
                    item.get("size", ""),
                    item.get("crc32", ""),
                    item.get("file", ""),
                ),
            )
            self.payload_enabled[iid] = True
        self.log(f"Loaded package: {package_dir} ({len(self.package_payloads)} payloads)")

    def payload_tree_click(self, event):
        region = self.payload_tree.identify("region", event.x, event.y)
        if region != "cell":
            return
        column = self.payload_tree.identify_column(event.x)
        if column != "#1":
            return
        iid = self.payload_tree.identify_row(event.y)
        if not iid:
            return
        enabled = not self.payload_enabled.get(iid, True)
        self.payload_enabled[iid] = enabled
        values = list(self.payload_tree.item(iid, "values"))
        values[0] = "☑" if enabled else "☐"
        self.payload_tree.item(iid, values=values)
        return "break"

    def selected_payloads(self):
        children = list(self.payload_tree.get_children())
        index_by_iid = {iid: idx for idx, iid in enumerate(children)}
        enabled_iids = [iid for iid in children if self.payload_enabled.get(iid, True)]
        payloads = [self.package_payloads[index_by_iid[iid]] for iid in enabled_iids]
        out = []
        for payload in payloads:
            is_fbl = self._payload_is_fbl(payload)
            if is_fbl and self.flash_fbl_var.get():
                out.append(payload)
            elif (not is_fbl) and self.flash_appl_var.get():
                out.append(payload)
        return out

    def execute_selected_clicked(self):
        payloads = self.selected_payloads()
        if not payloads and not self.flash_coding_var.get():
            messagebox.showerror(APP_NAME, "Load/select payloads or enable Coding first")
            return
        if not self.dry_run_var.get() and not self.arm_programming_var.get():
            messagebox.showerror(APP_NAME, "Real programming requires Arm programming to be checked")
            return

        fbl_payloads = [p for p in payloads if self._payload_is_fbl(p)]
        appl_payloads = [p for p in payloads if not self._payload_is_fbl(p)]
        selected_steps = (
            (len(fbl_payloads) if self.flash_fbl_var.get() else 0)
            + (len(appl_payloads) if self.flash_appl_var.get() else 0)
            + (1 if self.flash_coding_var.get() else 0)
        )

        def action():
            client = self.require_client()
            self.root.after(0, lambda: self.progress.configure(maximum=max(1, selected_steps), value=0))
            done = 0

            self._execute_zgw_programming_preamble(client)

            if self.flash_fbl_var.get() and fbl_payloads:
                self._execute_start_fbl_ram_updater(client)
                self._execute_status_readback(client, "after FBL RAM updater")
                for payload in fbl_payloads:
                    self._execute_payload(client, payload, is_fbl=True)
                    done += 1
                    self.root.after(0, lambda value=done: self.progress.configure(value=value))
                self._execute_hard_reset(client, "after FBL flash")
                self._execute_status_readback(client, "after FBL reset")

            if self.flash_appl_var.get() and appl_payloads:
                for payload in appl_payloads:
                    self._execute_payload(client, payload, is_fbl=False)
                    done += 1
                    self.root.after(0, lambda value=done: self.progress.configure(value=value))
                self._execute_hard_reset(client, "after APPL flash")

            self._execute_post_programming_extended(client)

            if self.flash_coding_var.get():
                self._execute_coding_after_flash(client)
                done += 1
                self.root.after(0, lambda value=done: self.progress.configure(value=value))

            self._execute_final_readback(client)
            send = self._make_uds_sender(client)
            send(bytes([0x10, SESSION_DEFAULT]), "Default Session final")

        self.worker("Execute ZGW Sequence", action)

    def _payload_is_fbl(self, payload):
        name = str(payload.get("ecu", "") + " " + payload.get("file", "")).upper()
        address = parse_int(payload.get("address", "0"))
        return "FBL" in name or address < DEFAULT_APP_START

    def _make_uds_sender(self, client, dry=None):
        # Dry run is a Flash-tab safety latch. Coding-tab actions pass dry=False so an
        # active Flash dry-run checkbox never silently suppresses a coding request
        # (e.g. the coding session change or Load Default routine).
        if dry is None:
            dry = self.dry_run_var.get()

        def send(request, name, timeout=None, allow_no_response=False):
            request = bytes(request)
            if dry:
                self.log(f"DRY {name}: {bytes_to_hex(request)}")
                time.sleep(0.001)
                return b""
            self.log(f"TX {name}: {bytes_to_hex(request)}")
            try:
                response = client.send_uds(request, timeout=timeout or float(self.timeout_var.get()))
            except Exception:
                if allow_no_response:
                    self.log(f"RX {name}: no response accepted")
                    return b""
                raise
            self.log(f"RX {name}: {bytes_to_hex(response)}")
            require_positive_response(response, request[0])
            return response

        return send

    def _send_session(self, send, session, label, allow_no_response=False, timeout=10.0):
        response = send(
            bytes([0x10, session & 0xFF]),
            label,
            timeout=timeout,
            allow_no_response=allow_no_response,
        )
        if isinstance(response, bytes) and len(response) >= 2:
            self.log(f"{label}: active_session_response=0x{response[1]:02X}")
            if response[1] != (session & 0xFF):
                raise FcdError(
                    f"{label}: requested session 0x{session:02X}, response echoed 0x{response[1]:02X}"
                )
        return response

    def _read_active_diag_session(self, send, prefix, timeout=2.0):
        response = send(
            b"\x22" + struct.pack(">H", DID_ACTIVE_DIAG_SESSION),
            f"{prefix}: Read Active Diagnostic Session F186",
            timeout=timeout,
        )
        require_positive_response(response, 0x22)
        if (len(response) < 4) or (response[1] != 0xF1) or (response[2] != 0x86):
            raise FcdError(f"{prefix}: malformed active-session DID response {bytes_to_hex(response)}")
        session = response[3]
        self.log(f"{prefix}: active_session=0x{session:02X}")
        return session

    def _ensure_session(self, send, session, label, timeout=3.0):
        try:
            return self._send_session(send, session, label, timeout=timeout)
        except Exception as session_exc:
            self.log(f"{label}: no positive session response ({session_exc}); verifying active session")
            try:
                active_session = self._read_active_diag_session(send, label, timeout=2.0)
            except Exception as verify_exc:
                raise FcdError(
                    f"{label}: session response missing and active session could not be verified"
                ) from verify_exc

            if active_session == (session & 0xFF):
                self.log(f"{label}: active session is 0x{active_session:02X}; continuing")
                return b""

            raise FcdError(
                f"{label}: requested session 0x{session:02X}, active session is 0x{active_session:02X}"
            ) from session_exc

    def _try_send_session(self, send, session, label, timeout=3.0):
        try:
            return send(bytes([0x10, session & 0xFF]), label, timeout=timeout, allow_no_response=True)
        except Exception as exc:
            self.log(f"{label}: no usable response ({exc}); continuing")
            return b""

    def _try_send(self, send, request, label, timeout=3.0):
        try:
            return send(request, label, timeout=timeout, allow_no_response=True)
        except Exception as exc:
            self.log(f"{label}: no usable response ({exc}); continuing")
            return b""

    def _read_standard_status(self, send, prefix):
        for did, name in [
            (DID_APP_SW_VERSION, "Read Software Version F101"),
            (DID_ACTIVE_SW_BLOCK, "Read Active Software Block F100"),
            (DID_ACTIVE_DIAG_SESSION, "Read Active Diagnostic Session F186"),
        ]:
            send(b"\x22" + struct.pack(">H", did), f"{prefix}: {name}")

    def _execute_zgw_programming_preamble(self, client):
        send = self._make_uds_sender(client)
        self._send_session(send, SESSION_DEFAULT, "Default Session")
        self._send_session(send, SESSION_EXTENDED, "Extended Session")
        self._read_standard_status(send, "Preamble")
        send(
            b"\x31\x01" + struct.pack(">H", ROUTINE_SELECT_FBL_INTERFACE) + bytes([FBL_INTERFACE_DOIP]),
            "RoutineControl 0205 Select Communication Interface Ethernet",
            timeout=10.0,
        )
        send(b"\x28\x01", "CommunicationControl enableRxAndDisableTx")
        send(b"\x85\x02", "ControlDTCSetting off")
        self._send_session(send, SESSION_PROGRAMMING, "Programming Session")
        if isinstance(client, DoipClient) and not self.dry_run_var.get():
            self.reconnect_doip(client, "programming session")
            send = self._make_uds_sender(client)
        self._read_standard_status(send, "Programming pre-flash")

    def _execute_start_fbl_ram_updater(self, client):
        send = self._make_uds_sender(client)
        send(
            b"\x31\x01" + struct.pack(">H", ROUTINE_START_FBL_RAM_UPDATER),
            "RoutineControl 0155 Start FBL RAM Updater",
            timeout=20.0,
        )

    def _execute_status_readback(self, client, prefix):
        send = self._make_uds_sender(client)
        self._read_standard_status(send, prefix)

    def _execute_hard_reset(self, client, reason):
        if self.dry_run_var.get():
            send = self._make_uds_sender(client)
            send(b"\x11\x01", f"ECUReset hardReset {reason}", timeout=5.0, allow_no_response=True)
            return

        self._tolerant_ecu_reset(client, reason)

    def _execute_post_programming_extended(self, client):
        send = self._make_uds_sender(client)
        self._send_session(send, SESSION_DEFAULT, "Default Session after programming")
        self._send_session(send, SESSION_EXTENDED, "Extended Session after programming")
        self._read_standard_status(send, "Post-programming")
        send(b"\x28\x00", "CommunicationControl enableRxAndTx")
        send(b"\x85\x01", "ControlDTCSetting on")

    def _execute_coding_after_flash(self, client):
        send = self._make_uds_sender(client)
        self._ensure_session(send, SESSION_CODING_REQUESTED, "Coding Session requested as 10 41")
        self._run_coding_routine(send, CODING_ROUTINE_WRITE_ALL, b"", "Write Coding", timeout=90.0)
        self._run_coding_routine(send, CODING_ROUTINE_VALIDATE, b"", "Check Coding", timeout=20.0)
        self._execute_hard_reset(client, "after coding")

    def _execute_final_readback(self, client):
        send = self._make_uds_sender(client)
        self._send_session(send, SESSION_DEFAULT, "Default Session after coding reset")
        self._send_session(send, SESSION_EXTENDED, "Extended Session after coding reset")
        self._read_coding_app_status(send, "Final", required=True)
        for did, name in [
            (DID_APP_SW_VERSION, "Read Software Version F101"),
            (DID_ACTIVE_SW_BLOCK, "Read Active Software Block F100"),
            (DID_ACTIVE_DIAG_SESSION, "Read Active Diagnostic Session F186"),
        ]:
            send(b"\x22" + struct.pack(">H", did), f"Final: {name}")
        send(
            b"\x31\x01" + struct.pack(">H", CODING_ROUTINE_READ_NVM),
            "RoutineControl 0203 Read Coding start",
            timeout=10.0,
        )
        send(
            b"\x31\x03" + struct.pack(">H", CODING_ROUTINE_READ_NVM),
            "RoutineControl 0203 Read Coding result",
            timeout=10.0,
        )

    def _read_payload_data(self, payload):
        if payload.get("data_base64"):
            return base64.b64decode(payload["data_base64"])
        if self.package_dir is None:
            raise FcdError("Package directory is not loaded")
        path = self.package_dir / payload["file"]
        return path.read_bytes()

    def _execute_payload(self, client, payload, is_fbl=False):
        target = parse_int(payload["target_logical_address"])
        address = parse_int(payload["address"])
        size = int(payload["size"])
        expected_crc = parse_int(payload["crc32"])
        data = self._read_payload_data(payload)
        if len(data) != size:
            raise FcdError(f"{payload['ecu']}: payload size mismatch {len(data)} != {size}")
        if (binascii.crc32(data) & 0xFFFFFFFF) != expected_crc:
            raise FcdError(f"{payload['ecu']}: payload CRC mismatch")
        if isinstance(client, DoipClient):
            client.target_addr = target

        block_size = max(8, min(4093, parse_int(self.block_size_var.get())))
        dry = self.dry_run_var.get()
        block_name = "FBL" if is_fbl else "APPL"
        label = f"{payload['ecu']} {block_name} {int_hex(address, 8)} size={size}"
        self.log(f"Flash start: {label} target={int_hex(target)} dry_run={dry}")

        send = self._make_uds_sender(client)

        if self.erase_var.get():
            erase_request = b"\x31\x01" + struct.pack(">H", ROUTINE_ERASE_MEMORY) + struct.pack(">II", address, size)
            send(erase_request, f"RoutineControl Erase {block_name}", timeout=30.0)

        req_download = b"\x34\x00\x44" + struct.pack(">II", address, size)
        send(req_download, f"RequestDownload {block_name}", timeout=15.0)

        block_counter = 1
        offset = 0
        while offset < size:
            chunk = data[offset : offset + block_size]
            request = bytes([0x36, block_counter & 0xFF]) + chunk
            response = send(request, f"TransferData {block_name} #{block_counter}", timeout=8.0)
            if not dry and len(response) >= 2 and response[1] != (block_counter & 0xFF):
                raise FcdError(f"TransferData block echo mismatch at #{block_counter}")
            offset += len(chunk)
            self.root.after(0, lambda done=offset, total=size, name=block_name: self.progress.configure(value=min(done, total), maximum=max(1, total)))
            if dry:
                time.sleep(0.001)
            block_counter = (block_counter + 1) & 0xFF
            if block_counter == 0:
                block_counter = 1

        transfer_exit = b"\x37"
        if self.transfer_crc_var.get():
            transfer_exit += struct.pack(">I", expected_crc)
        send(transfer_exit, f"RequestTransferExit {block_name}", timeout=15.0)

        if self.verify_crc_var.get():
            crc_request = (
                b"\x31\x01"
                + struct.pack(">H", ROUTINE_CHECK_MEMORY_CRC)
                + struct.pack(">III", address, size, expected_crc)
            )
            send(crc_request, f"RoutineControl CRC {block_name}", timeout=30.0)

        self.log(f"Flash complete: {label}")

    def save_trace_clicked(self):
        path = filedialog.asksaveasfilename(defaultextension=".log", filetypes=[("Log", "*.log"), ("Text", "*.txt")])
        if not path:
            return
        Path(path).write_text(self.trace_text.get("1.0", "end"), encoding="utf-8")

    def _on_close(self):
        self.worker_stop.set()
        self.stop_keepalive()
        if self.client is not None:
            self.client.close()
        self.root.destroy()


def main():
    root = tk.Tk()
    app = FcdApp(root)
    app.log(f"{APP_TITLE} started")
    root.mainloop()


if __name__ == "__main__":
    main()
