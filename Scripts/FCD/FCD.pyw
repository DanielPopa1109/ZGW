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

    @property
    def connected(self):
        return self.sock is not None

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
        self.sock.sendall(header + payload)

    def _recv_exact(self, length, deadline=None):
        if self.sock is None:
            raise DoipError("Not connected")
        out = bytearray()
        while len(out) < length:
            if deadline is not None:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError("Timed out waiting for DoIP data")
                self.sock.settimeout(min(self.timeout, remaining))
            chunk = self.sock.recv(length - len(out))
            if not chunk:
                raise DoipError("TCP connection closed")
            out.extend(chunk)
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
                if len(uds) >= 3 and uds[0] == 0x7F and uds[2] == 0x78:
                    deadline = time.monotonic() + timeout
                    continue
                return uds

    def send_uds_suppress_positive(self, request, timeout=None):
        timeout = self.timeout if timeout is None else float(timeout)
        payload = struct.pack(">HH", self.source_addr, self.target_addr) + bytes(request)
        deadline = time.monotonic() + timeout

        with self.lock:
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

    @property
    def connected(self):
        return self.sock is not None

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
        self.generator_rows = {}
        self.package_dir = None
        self.package_payloads = []

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
        self.svt_tab = ttk.Frame(self.notebook)
        self.coding_tab = ttk.Frame(self.notebook)
        self.generator_tab = ttk.Frame(self.notebook)
        self.tal_tab = ttk.Frame(self.notebook)
        self.raw_tab = ttk.Frame(self.notebook)

        self.notebook.add(self.connection_tab, text="Connection")
        self.notebook.add(self.svt_tab, text="FA / SVT")
        self.notebook.add(self.coding_tab, text="Coding")
        self.notebook.add(self.generator_tab, text="Programming Data")
        self.notebook.add(self.tal_tab, text="TAL / Flash")
        self.notebook.add(self.raw_tab, text="Raw UDS")

        self._build_connection_tab()
        self._build_svt_tab()
        self._build_coding_tab()
        self._build_generator_tab()
        self._build_tal_tab()
        self._build_raw_tab()
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
        self.timeout_var = tk.StringVar(value="6.0")
        self.local_ip_var = tk.StringVar(value="auto")
        self.transport_var = tk.StringVar(value="DoIP")
        self.conn_status_var = tk.StringVar(value="Disconnected")

        self._entry_row(outer, 0, "ZGW host", self.host_var, 22, 0)
        self._entry_row(outer, 0, "TCP port", self.port_var, 10, 2)
        self._entry_row(outer, 1, "Tester address", self.source_var, 14, 0)
        self._entry_row(outer, 1, "Target address", self.target_var, 14, 2)
        self._entry_row(outer, 2, "PC local IP", self.local_ip_var, 22, 0)
        self._entry_row(outer, 2, "Timeout seconds", self.timeout_var, 10, 2)

        ttk.Label(outer, text="Transport").grid(row=3, column=0, sticky="w", padx=6, pady=4)
        ttk.Combobox(
            outer,
            textvariable=self.transport_var,
            values=["DoIP", "Raw TCP UDS"],
            width=14,
            state="readonly",
        ).grid(row=3, column=1, sticky="w", padx=6, pady=4)

        status = ttk.Label(outer, textvariable=self.conn_status_var, style="Bad.TLabel")
        status.grid(row=4, column=0, columnspan=4, sticky="w", padx=6, pady=8)
        self.connection_status_label = status

        self._button_row(
            outer,
            5,
            [
                ("Connect", self.connect_clicked, None),
                ("Disconnect", self.disconnect_clicked, None),
                ("Discover DoIP", self.discover_doip_clicked, None),
                ("Probe Ethernet", self.ethernet_probe_clicked, None),
                ("Vehicle ID", self.vehicle_id_clicked, None),
                ("Routing Activation", self.routing_activation_clicked, None),
                ("Alive Check", self.alive_check_clicked, None),
            ],
        )

        quick = ttk.LabelFrame(outer, text="Quick UDS")
        quick.grid(row=6, column=0, columnspan=4, sticky="ew", padx=6, pady=10)
        for col in range(8):
            quick.columnconfigure(col, weight=1)

        self._button_row(
            quick,
            0,
            [
                ("Default Session", lambda: self.send_uds_async(b"\x10\x01", "Default Session"), None),
                ("Programming Session", lambda: self.send_uds_async(b"\x10\x02", "Programming Session"), None),
                ("Extended Session", lambda: self.send_uds_async(b"\x10\x03", "Extended Session"), None),
                ("Tester Present", lambda: self.send_uds_async(b"\x3E\x00", "Tester Present"), None),
                ("Read DID F186", lambda: self.send_uds_async(b"\x22\xF1\x86", "Read DID F186"), None),
                ("ECU Reset", self.reset_clicked, "Danger.TButton"),
            ],
        )

        self.keepalive_var = tk.BooleanVar(value=True)
        self.keepalive_period_var = tk.StringVar(value="2.0")

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
        self.fa_zeitkriterium_var = tk.StringVar(value=datetime.now().strftime("%y%m"))
        self.fa_type_var = tk.StringVar(value="ZGW")
        self._entry_row(top, 0, "Project", self.fa_project_var, 18, 0)
        self._entry_row(top, 0, "VIN", self.fa_vin_var, 22, 2)
        self._entry_row(top, 1, "Zeitkriterium", self.fa_zeitkriterium_var, 12, 0)
        self._entry_row(top, 1, "Type", self.fa_type_var, 12, 2)

        svt_frame = ttk.LabelFrame(outer, text="SVT / ECU List")
        svt_frame.grid(row=1, column=0, sticky="nsew", padx=4, pady=8)
        svt_frame.columnconfigure(0, weight=1)
        svt_frame.rowconfigure(0, weight=1)

        columns = ("ecu", "target", "req", "resp", "did", "hex", "size", "crc")
        self.svt_tree = ttk.Treeview(svt_frame, columns=columns, show="headings", selectmode="browse")
        for col, text, width in [
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

        did_frame = ttk.LabelFrame(outer, text="Read / Write Data By Identifier")
        did_frame.grid(row=0, column=0, sticky="ew", padx=4, pady=4)
        for col in range(6):
            did_frame.columnconfigure(col, weight=1)

        self.coding_did_var = tk.StringVar(value="0xF186")
        self.coding_write_data_var = tk.StringVar(value="")
        self._entry_row(did_frame, 0, "DID", self.coding_did_var, 12, 0)
        self._entry_row(did_frame, 0, "Write bytes", self.coding_write_data_var, 45, 2)
        ttk.Button(did_frame, text="Read DID", command=self.read_did_clicked).grid(row=0, column=4, padx=4, pady=4)
        ttk.Button(did_frame, text="Send 2E", command=self.write_did_clicked).grid(row=0, column=5, padx=4, pady=4)

        file_frame = ttk.LabelFrame(outer, text="CAFD-like FCD Coding Data")
        file_frame.grid(row=1, column=0, sticky="ew", padx=4, pady=8)
        ttk.Button(file_frame, text="Import Coding JSON", command=self.import_coding_clicked).pack(side="left", padx=4, pady=6)
        ttk.Button(file_frame, text="Export Coding JSON", command=self.export_coding_clicked).pack(side="left", padx=4, pady=6)
        ttk.Button(file_frame, text="Add Parameter", command=self.add_coding_param_clicked).pack(side="left", padx=4, pady=6)
        ttk.Button(file_frame, text="Remove Parameter", command=self.remove_coding_param_clicked).pack(side="left", padx=4, pady=6)

        columns = ("path", "value", "comment")
        self.coding_tree = ttk.Treeview(outer, columns=columns, show="headings", selectmode="browse")
        for col, text, width in [
            ("path", "Parameter Path", 360),
            ("value", "Value", 160),
            ("comment", "Comment", 500),
        ]:
            self.coding_tree.heading(col, text=text)
            self.coding_tree.column(col, width=width, stretch=(col == "comment"))
        self.coding_tree.grid(row=2, column=0, sticky="nsew", padx=4, pady=4)
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

        self.gen_project_var = tk.StringVar(value="ZGW_LAB")
        self.gen_vin_var = tk.StringVar(value="LABTC375DOIP0001")
        self.gen_output_var = tk.StringVar(value=str(SCRIPT_DIR / "Generated"))
        self.gen_embed_var = tk.BooleanVar(value=False)
        self._entry_row(meta, 0, "Project", self.gen_project_var, 18, 0)
        self._entry_row(meta, 0, "VIN", self.gen_vin_var, 22, 2)
        self._entry_row(meta, 1, "Output folder", self.gen_output_var, 80, 0)
        ttk.Button(meta, text="Browse", command=self.browse_gen_output_clicked).grid(row=1, column=2, padx=4, pady=4)
        ttk.Checkbutton(meta, text="Embed payload in JSON", variable=self.gen_embed_var).grid(
            row=1, column=3, sticky="w", padx=6, pady=4
        )

        buttons = ttk.Frame(outer)
        buttons.grid(row=1, column=0, sticky="w", padx=4, pady=6)
        ttk.Button(buttons, text="Add HEX", command=self.add_hex_clicked).pack(side="left", padx=4)
        ttk.Button(buttons, text="Edit Selected", command=self.edit_hex_clicked).pack(side="left", padx=4)
        ttk.Button(buttons, text="Remove Selected", command=self.remove_hex_clicked).pack(side="left", padx=4)
        ttk.Button(buttons, text="Generate FCD Files", command=self.generate_files_clicked).pack(side="left", padx=12)

        columns = ("ecu", "target", "req", "resp", "did", "base", "hex")
        self.generator_tree = ttk.Treeview(outer, columns=columns, show="headings", selectmode="browse")
        for col, text, width in [
            ("ecu", "ECU", 160),
            ("target", "DoIP Target", 100),
            ("req", "CAN Req ID", 90),
            ("resp", "CAN Resp ID", 90),
            ("did", "Coding DID", 90),
            ("base", "Download Base", 130),
            ("hex", "HEX File", 520),
        ]:
            self.generator_tree.heading(col, text=text)
            self.generator_tree.column(col, width=width, stretch=(col == "hex"))
        self.generator_tree.grid(row=2, column=0, sticky="nsew", padx=4, pady=4)
        self.generator_tree.bind("<Double-1>", lambda _event: self.edit_hex_clicked())

        note = (
            "Download Base is optional. When set, HEX segment addresses are relocated relative to the "
            "lowest HEX address. For this repo FBL, application range is 0xA0030000..0xA07FFFFF."
        )
        ttk.Label(outer, text=note, wraplength=1100).grid(row=3, column=0, sticky="ew", padx=4, pady=8)

    def _build_tal_tab(self):
        outer = ttk.Frame(self.tal_tab)
        outer.pack(fill="both", expand=True, padx=10, pady=10)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(2, weight=1)

        load = ttk.LabelFrame(outer, text="TAL / Programming Package")
        load.grid(row=0, column=0, sticky="ew", padx=4, pady=4)
        for col in range(8):
            load.columnconfigure(col, weight=1)

        self.pkg_dir_var = tk.StringVar(value="")
        self._entry_row(load, 0, "Package folder", self.pkg_dir_var, 85, 0)
        ttk.Button(load, text="Browse", command=self.browse_package_clicked).grid(row=0, column=2, padx=4, pady=4)
        ttk.Button(load, text="Load", command=self.load_package_clicked).grid(row=0, column=3, padx=4, pady=4)

        options = ttk.LabelFrame(outer, text="Execution Options")
        options.grid(row=1, column=0, sticky="ew", padx=4, pady=8)
        for col in range(10):
            options.columnconfigure(col, weight=1)

        self.dry_run_var = tk.BooleanVar(value=True)
        self.arm_programming_var = tk.BooleanVar(value=False)
        self.erase_var = tk.BooleanVar(value=True)
        self.transfer_crc_var = tk.BooleanVar(value=True)
        self.verify_crc_var = tk.BooleanVar(value=True)
        self.security_var = tk.BooleanVar(value=False)
        self.security_seed_sub_var = tk.StringVar(value="0x01")
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
        ttk.Checkbutton(options, text="Project lab security", variable=self.security_var).grid(
            row=0, column=5, sticky="w", padx=6
        )
        self._entry_row(options, 1, "Session", self.session_var, 10, 0)
        self._entry_row(options, 1, "Seed sub", self.security_seed_sub_var, 10, 2)
        self._entry_row(options, 1, "Block size", self.block_size_var, 10, 4)

        columns = ("ecu", "target", "address", "size", "crc", "file")
        self.payload_tree = ttk.Treeview(outer, columns=columns, show="headings", selectmode="extended")
        for col, text, width in [
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

        actions = ttk.Frame(outer)
        actions.grid(row=3, column=0, sticky="ew", padx=4, pady=8)
        ttk.Button(actions, text="Precheck Selected", command=self.precheck_selected_clicked).pack(side="left", padx=4)
        ttk.Button(actions, text="Execute Selected", command=self.execute_selected_clicked, style="Danger.TButton").pack(
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

    def log(self, message):
        self.log_queue.put(f"[{now_text()}] {message}")

    def _drain_log_queue(self):
        try:
            while True:
                line = self.log_queue.get_nowait()
                self.trace_text.insert("end", line + "\n")
                self.trace_text.see("end")
        except queue.Empty:
            pass
        self.root.after(100, self._drain_log_queue)

    def worker(self, name, func):
        def run():
            try:
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

    def reconnect_doip(self, client, reason):
        if not isinstance(client, DoipClient):
            return

        timeout = max(1.0, float(self.timeout_var.get()))
        deadline = time.monotonic() + max(10.0, timeout * 4.0)
        last_error = None

        self.log(f"DoIP reconnect after {reason}: waiting for TCP {client.host}:{client.port}")
        client.close()
        time.sleep(0.5)

        while time.monotonic() < deadline:
            try:
                client.connect()
                self.activate_doip(client)
                self.root.after(0, self._set_connected_status, True)
                self.log(f"DoIP reconnected after {reason}")
                return
            except (OSError, TimeoutError, DoipError) as exc:
                last_error = exc
                client.close()
                time.sleep(0.5)

        self.root.after(0, self._set_connected_status, False)
        raise FcdError(f"DoIP reconnect after {reason} failed: {last_error}")

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
            self.log(f"TX {label}: {bytes_to_hex(request)}")
            response = client.send_uds(request, timeout=float(self.timeout_var.get()))
            self.log(f"RX {label}: {bytes_to_hex(response)}")
            if isinstance(client, DoipClient) and response:
                if len(request) >= 2 and request[0] == 0x10 and (request[1] & 0x7F) == 0x02 and response[0] == 0x50:
                    self.reconnect_doip(client, "programming session")
                elif len(request) >= 2 and request[0] == 0x11 and (request[1] & 0x7F) in (0x01, 0x03) and response[0] == 0x51:
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
            period = max(0.5, float(self.keepalive_period_var.get()))
        except (TypeError, ValueError):
            period = 2.0
            self.keepalive_period_var.set(f"{period:.1f}")

        def loop():
            while not self.keepalive_stop.is_set():
                try:
                    if self.client is not None and self.client.connected:
                        if isinstance(self.client, DoipClient):
                            timeout = max(float(self.timeout_var.get()), 6.0)
                            self.client.send_uds_suppress_positive(b"\x3E\x80", timeout=timeout)
                            self.log("TesterPresent ACK: 3E 80")
                        else:
                            response = self.client.send_uds(b"\x3E\x00", timeout=float(self.timeout_var.get()))
                            self.log(f"TesterPresent RX: {bytes_to_hex(response)}")
                except Exception as exc:
                    self.log(f"TesterPresent ERROR: {exc}")
                self.keepalive_stop.wait(period)

        self.keepalive_thread = threading.Thread(target=loop, daemon=True)
        self.keepalive_thread.start()
        self.log(f"Automatic tester present started: {period:.1f} s")

    def stop_keepalive(self, update_var=True):
        self.keepalive_stop.set()
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
            self.coding_tree.insert("", "end", values=(item["path"], item["value"], item["comment"]))

    def edit_coding_param_clicked(self):
        selection = self.coding_tree.selection()
        if not selection:
            return
        values = self.coding_tree.item(selection[0], "values")
        item = self._coding_param_dialog({"path": values[0], "value": values[1], "comment": values[2]})
        if item:
            self.coding_tree.item(selection[0], values=(item["path"], item["value"], item["comment"]))

    def remove_coding_param_clicked(self):
        for iid in self.coding_tree.selection():
            self.coding_tree.delete(iid)

    def _coding_param_dialog(self, existing=None):
        existing = existing or {"path": "", "value": "", "comment": ""}
        dialog = tk.Toplevel(self.root)
        dialog.title("Coding Parameter")
        dialog.transient(self.root)
        dialog.grab_set()
        result = {}

        vars_ = {
            "path": tk.StringVar(value=existing["path"]),
            "value": tk.StringVar(value=existing["value"]),
            "comment": tk.StringVar(value=existing["comment"]),
        }
        for row, key in enumerate(["path", "value", "comment"]):
            ttk.Label(dialog, text=key.title()).grid(row=row, column=0, sticky="w", padx=8, pady=6)
            ttk.Entry(dialog, textvariable=vars_[key], width=72).grid(row=row, column=1, sticky="ew", padx=8, pady=6)

        def ok():
            result.update({key: var.get().strip() for key, var in vars_.items()})
            dialog.destroy()

        ttk.Button(dialog, text="OK", command=ok).grid(row=3, column=0, padx=8, pady=10)
        ttk.Button(dialog, text="Cancel", command=dialog.destroy).grid(row=3, column=1, sticky="w", padx=8, pady=10)
        dialog.wait_window()
        return result or None

    def import_coding_clicked(self):
        path = filedialog.askopenfilename(filetypes=[("JSON", "*.json"), ("All files", "*.*")])
        if not path:
            return
        with open(path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
        self.coding_tree.delete(*self.coding_tree.get_children())
        for item in data.get("parameters", []):
            self.coding_tree.insert("", "end", values=(item.get("path", ""), item.get("value", ""), item.get("comment", "")))
        self.log(f"Imported coding JSON: {path}")

    def export_coding_clicked(self):
        path = filedialog.asksaveasfilename(defaultextension=".json", filetypes=[("JSON", "*.json")])
        if not path:
            return
        parameters = []
        for iid in self.coding_tree.get_children():
            values = self.coding_tree.item(iid, "values")
            parameters.append({"path": values[0], "value": values[1], "comment": values[2]})
        data = {
            "schema": "FCD_CAFD_v1",
            "created": datetime.now().isoformat(timespec="seconds"),
            "vin": self.fa_vin_var.get(),
            "parameters": parameters,
        }
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(data, handle, indent=2)
        self.log(f"Exported coding JSON: {path}")

    def browse_gen_output_clicked(self):
        path = filedialog.askdirectory(initialdir=self.gen_output_var.get() or str(Path.cwd()))
        if path:
            self.gen_output_var.set(path)

    def add_hex_clicked(self):
        paths = filedialog.askopenfilenames(filetypes=[("Intel HEX", "*.hex;*.ihex"), ("All files", "*.*")])
        for path in paths:
            stem = Path(path).stem
            row = {
                "ecu": stem.upper(),
                "target": int_hex(DEFAULT_TARGET_ADDR),
                "req": "0x710",
                "resp": "0x711",
                "did": "0xF186",
                "base": int_hex(DEFAULT_APP_START, 8),
                "hex": str(path),
            }
            self._insert_generator_row(row)

    def _insert_generator_row(self, row):
        iid = self.generator_tree.insert(
            "",
            "end",
            values=(row["ecu"], row["target"], row["req"], row["resp"], row["did"], row["base"], row["hex"]),
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
                    updated["target"],
                    updated["req"],
                    updated["resp"],
                    updated["did"],
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

                source_min = min(seg.address for seg in segments)
                base_override = parse_int(row["base"]) if row.get("base") else None
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
                    address = segment.address
                    if base_override is not None:
                        address = base_override + (segment.address - source_min)
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
                    if self.gen_embed_var.get():
                        payload_entry["data_base64"] = base64.b64encode(segment.data).decode("ascii")
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
                "zeitkriterium": self.fa_zeitkriterium_var.get().strip(),
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
            self.root.after(0, self.copy_generator_to_svt)

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
        self.payload_tree.delete(*self.payload_tree.get_children())
        for item in self.package_payloads:
            self.payload_tree.insert(
                "",
                "end",
                values=(
                    item.get("ecu", ""),
                    item.get("target_logical_address", ""),
                    item.get("address", ""),
                    item.get("size", ""),
                    item.get("crc32", ""),
                    item.get("file", ""),
                ),
            )
        self.log(f"Loaded package: {package_dir} ({len(self.package_payloads)} payloads)")

    def selected_payloads(self):
        selected = self.payload_tree.selection()
        if not selected:
            selected = self.payload_tree.get_children()
        index_by_iid = {iid: idx for idx, iid in enumerate(self.payload_tree.get_children())}
        return [self.package_payloads[index_by_iid[iid]] for iid in selected]

    def precheck_selected_clicked(self):
        def action():
            client = self.require_client()
            payloads = self.selected_payloads()
            if not payloads:
                raise FcdError("No payloads loaded")
            for payload in payloads:
                target = parse_int(payload["target_logical_address"])
                if isinstance(client, DoipClient):
                    client.target_addr = target
                address = parse_int(payload["address"])
                size = int(payload["size"])
                if not (DEFAULT_APP_START <= address <= DEFAULT_APP_END) or address + size - 1 > DEFAULT_APP_END:
                    self.log(
                        f"Precheck WARN {payload['ecu']}: {int_hex(address, 8)} size {size} outside default app range"
                    )
                response = client.send_uds(b"\x22\xF1\x86", timeout=float(self.timeout_var.get()))
                self.log(f"Precheck {payload['ecu']} target {int_hex(target)} RDBI F186: {bytes_to_hex(response)}")

        self.worker("Precheck", action)

    def execute_selected_clicked(self):
        payloads = self.selected_payloads()
        if not payloads:
            messagebox.showerror(APP_NAME, "Load or select payloads first")
            return
        if not self.dry_run_var.get() and not self.arm_programming_var.get():
            messagebox.showerror(APP_NAME, "Real programming requires Arm programming to be checked")
            return
        if not self.dry_run_var.get():
            ok = messagebox.askyesno(
                APP_NAME,
                "This will send erase/download/transfer services to the selected ECU target. Continue?",
            )
            if not ok:
                return

        def action():
            client = self.require_client()
            self.root.after(0, lambda: self.progress.configure(maximum=max(1, len(payloads)), value=0))
            erased_targets = set()
            for idx, payload in enumerate(payloads, start=1):
                self._execute_payload(client, payload, erased_targets)
                self.root.after(0, lambda value=idx: self.progress.configure(value=value))

        self.worker("Execute TAL", action)

    def _read_payload_data(self, payload):
        if payload.get("data_base64"):
            return base64.b64decode(payload["data_base64"])
        if self.package_dir is None:
            raise FcdError("Package directory is not loaded")
        path = self.package_dir / payload["file"]
        return path.read_bytes()

    def _execute_payload(self, client, payload, erased_targets):
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
        session = parse_int(self.session_var.get())
        dry = self.dry_run_var.get()
        label = f"{payload['ecu']} {int_hex(address, 8)} size={size}"
        self.log(f"TAL start: {label} target={int_hex(target)} dry_run={dry}")

        def send(request, name, timeout=None):
            if dry:
                self.log(f"DRY {name}: {bytes_to_hex(request)}")
                return b""
            self.log(f"TX {name}: {bytes_to_hex(request)}")
            response = client.send_uds(request, timeout=timeout or float(self.timeout_var.get()))
            self.log(f"RX {name}: {bytes_to_hex(response)}")
            require_positive_response(response, request[0])
            return response

        send(bytes([0x10, session & 0xFF]), "DiagnosticSessionControl")
        if isinstance(client, DoipClient) and not dry and session == 0x02:
            self.reconnect_doip(client, "programming session")
            send(bytes([0x10, session & 0xFF]), "DiagnosticSessionControl FBL")

        if self.security_var.get():
            seed_sub = parse_int(self.security_seed_sub_var.get())
            if (seed_sub & 1) == 0:
                raise FcdError("Security seed subfunction must be odd")
            seed_response = send(bytes([0x27, seed_sub & 0xFF]), "SecurityAccess seed")
            if not dry:
                if len(seed_response) < 6:
                    raise FcdError("Security seed response too short")
                seed = struct.unpack(">I", seed_response[2:6])[0]
                level = (seed_sub - 1) // 2
                key = project_lab_key(seed, level)
                key_request = bytes([0x27, (seed_sub + 1) & 0xFF]) + struct.pack(">I", key)
                send(key_request, "SecurityAccess key")

        target_key = (target, payload["ecu"])
        if self.erase_var.get() and target_key not in erased_targets:
            send(b"\x31\x01\x00\x01", "RoutineControl EraseApp", timeout=15.0)
            erased_targets.add(target_key)

        req_download = b"\x34\x00\x44" + struct.pack(">II", address, size)
        send(req_download, "RequestDownload")

        block_counter = 1
        offset = 0
        while offset < size:
            chunk = data[offset : offset + block_size]
            request = bytes([0x36, block_counter & 0xFF]) + chunk
            response = send(request, f"TransferData #{block_counter}", timeout=8.0)
            if not dry and len(response) >= 2 and response[1] != (block_counter & 0xFF):
                raise FcdError(f"TransferData block echo mismatch at #{block_counter}")
            offset += len(chunk)
            block_counter = (block_counter + 1) & 0xFF
            if block_counter == 0:
                block_counter = 1

        transfer_exit = b"\x37"
        if self.transfer_crc_var.get():
            transfer_exit += struct.pack(">I", expected_crc)
        send(transfer_exit, "RequestTransferExit", timeout=10.0)

        if self.verify_crc_var.get():
            request = b"\x31\x01\x00\x02" + struct.pack(">III", address, size, expected_crc)
            send(request, "RoutineControl CRC", timeout=15.0)

        self.log(f"TAL complete: {label}")

    def save_trace_clicked(self):
        path = filedialog.asksaveasfilename(defaultextension=".log", filetypes=[("Log", "*.log"), ("Text", "*.txt")])
        if not path:
            return
        Path(path).write_text(self.trace_text.get("1.0", "end"), encoding="utf-8")

    def _on_close(self):
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
