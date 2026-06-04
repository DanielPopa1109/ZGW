# -*- coding: utf-8 -*-
"""
Aurix Ethernet Gateway Logger (.pyw)

Listens for AURIX GatewaySwc Ethernet frames over UDP and/or TCP.
Default UDP path:
  192.168.1.10:30600 -> 192.168.1.255:30600

It decodes the GatewaySwc payload:
  5A 47 57 01 = summary frame
  5A 47 57 04 = DTC transition frame

It can also send a periodic PC heartbeat payload:
  PCHeartbeat

No Scapy/Npcap/admin rights required.
It will not capture ARP/ICMP; use Wireshark for that.
"""

import base64
import csv
import datetime as _dt
import os
import queue
import socket
import threading
import time
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import zlib
import json


AURIX_DEFAULT_IP = "192.168.1.255"
UDP_PORT = 30600
TCP_PORT = 30600
HEARTBEAT_PAYLOAD = b"PCHeartbeat"
HEARTBEAT_INTERVAL_S = 1.0
MAGIC = b"ZGW"

# High-throughput GUI settings. These intentionally use more CPU so the
# producer queue is drained faster than 100 ms Ethernet bursts can fill it.
QUEUE_MAX_ITEMS = 50000
GUI_POLL_MS = 1
MAX_QUEUE_ITEMS_PER_POLL = 5000
MAX_QUEUE_DRAIN_SECONDS = 0.200
STATUS_UPDATE_INTERVAL_S = 0.200
TEXT_FLUSH_MAX_LINES = 500
LOG_BUFFER_BYTES = 1024 * 1024
SOCKET_RCVBUF_BYTES = 4 * 1024 * 1024


BUS_NAMES = {
    1: "CAN",
    2: "CANFD",
    3: "LIN",
}

FRAME_TYPES = {
    0x01: "Gateway summary",
    0x02: "Command signal",
    0x03: "Command block",
    0x04: "DTC transition",
}

_SIGNAL_DB_B64 = """
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
9wEy8flKFl97b3VBfCnqY8XjPmAuPhDGB2r4UtTHisd9MLn4UBgfauFroj5WPO4D5eIzwviMGr4U9bHicR9sLj4Sxkdq+FLUx6rH
fcgVDi8sHN7qzczp1sGUo7t7/HJ6NfxlMB1fjD7OKbZeuy6ztfrl0/Pn3wdP4/vRn3OIb9Ctvnzd3ll9sTW8pK2x4nHfXTzegQ5l
0aEWuuRqrHbcd9eOd7AzsuyMFrtkaqxz3Hd3jjecjo/XwDs+LnzSfv26Hd/Xr29PWyN72rLOcd/dOd6JD6XxoRa+dOqy0nHfXTre
yc9I8zNa/JrTl9lGd+v4dHT+YTQZTK8mo/YukYsvlWs0mc5+/DF7vHl6fpxt2Sly8V3rWk0W3RiiJzFzju7u8V0gogJE1ILYnMpM
PboryHehaBQoGi2KzQnNDKS7iXwXiqRAkbQopgSVFZL77kLyXShaBYpWi2KTpLJict9dTL4LRqeA0alhTIkqKyj33QXlu2D0Chi9
GsY6WQ2sqDx0F5XvgjEoYAxaGFPCGlhheeguLN8FY1TAGNUw1klrYNXlobu6fAeMZZpR9sPYbkcRxFgnroF1mIeqhMCAgsAA6M3e
VY2RGMYSCgMKCgN6CmPTLcYyjCUcBhQcBtQcJi26DKzdPFQlJAYUJAbUJCYtvgys5TxUJSwGFCwG1CwmLcIMrO08VCUsBhQsBtQs
Ji3GDKz1PFQHWQy2MPZmMbgZY9tirGjUGFj3eYCqBMSgADFoQayjxsAq0ANACYpRgWLUolhHjYE1oQfAAhSxkqfYbluRo1hHjYFV
ogcwJSiCAkXQopiixsC60QNQCYyogBHVMLp0f2ECA7YERqOA0ahhTFEja0sP4EpgJAWMpIWxiRpZa3oAXwKjVcBo1TCmqJG1pwcI
JTA6BYxODWOKGlmLeoASAoMKAoNeb/aubzGsTT1gCYVBBYVBPYVJUSNrVQ9YwmFQwWFQzWGaqJG1qwcsITFGQWKMmsQ0USNrWQ9Y
wmKMgsUYNYtpokbWth6whMUYBYsxahbTRI2sdT3gQRZjWhh7sxizGWPbYpxs1Mi61wO6EhBJASJpQUxRI6tgD+hLULQKFK0WxRQ1
sib2gKEERadA0WlRTFEjq2QPGEtQ9AoUvRbFJmpk3ezBVCUwBgWMQQ1jihpZSXswUAJjVMAY1TCmqJG1tQeDBTBSJY+x3QYjh7GJ
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
"""


def _load_signal_db():
    raw = zlib.decompress(base64.b64decode(_SIGNAL_DB_B64))
    return json.loads(raw.decode("utf-8"))


SIGNALS = _load_signal_db()


def u16_be(data, offset):
    return (data[offset] << 8) | data[offset + 1]


def u32_be(data, offset):
    return ((data[offset] << 24) |
            (data[offset + 1] << 16) |
            (data[offset + 2] << 8) |
            data[offset + 3])


def format_number(value):
    if isinstance(value, float):
        if abs(value - round(value)) < 1e-9:
            return str(int(round(value)))
        return f"{value:.6g}"
    return str(value)


def decode_value(raw, info):
    factor = info.get("factor", 1.0)
    offset = info.get("offset", 0.0)
    unit = info.get("unit", "") or ""
    choices = info.get("choices", {}) or {}

    choice_text = choices.get(str(raw))
    physical = (float(raw) * float(factor)) + float(offset)

    if choice_text:
        if unit:
            return f"{choice_text} (raw={raw}, physical={format_number(physical)} {unit})"
        return f"{choice_text} (raw={raw}, physical={format_number(physical)})"

    if factor != 1.0 or offset != 0.0:
        if unit:
            return f"{format_number(physical)} {unit} (raw={raw})"
        return f"{format_number(physical)} (raw={raw})"

    if unit:
        return f"{raw} {unit}"
    return str(raw)


def signal_plain_text(signal_id, entry_bus, raw):
    info = SIGNALS.get(str(signal_id))
    bus_name = BUS_NAMES.get(entry_bus, f"bus{entry_bus}")

    if info is None:
        return {
            "bus": bus_name,
            "message": "unknown message",
            "signal": f"unknown COM signal {signal_id}",
            "frame_id": "",
            "value_text": str(raw),
            "raw": raw,
            "macro": "",
        }

    db_bus = info.get("bus") or bus_name
    message = info.get("message") or "unknown message"
    signal = info.get("signal") or info.get("macro", f"COM_{signal_id}")
    frame_id = info.get("frame_id")
    macro = info.get("macro", "")
    value_text = decode_value(raw, info)

    if frame_id is None or frame_id == "":
        frame_text = ""
    else:
        frame_text = f"0x{int(frame_id):X}/{int(frame_id)}"

    return {
        "bus": db_bus,
        "message": message,
        "signal": signal,
        "frame_id": frame_text,
        "value_text": value_text,
        "raw": raw,
        "macro": macro,
    }


def decode_gateway_payload(data):
    """
    Returns list of event dictionaries.
    One UDP datagram can produce one packet-summary event and many signal events.
    """
    events = []

    if len(data) < 4:
        events.append({"kind": "unknown", "text": f"Short payload: {len(data)} bytes"})
        return events

    if data[0:3] != MAGIC:
        events.append({
            "kind": "unknown",
            "text": f"Non-Gateway payload: {len(data)} bytes, first bytes={data[:16].hex(' ')}"
        })
        return events

    frame_type = data[3]
    frame_name = FRAME_TYPES.get(frame_type, f"Unknown frame type 0x{frame_type:02X}")

    if frame_type == 0x01:
        if len(data) < 12:
            events.append({"kind": "unknown", "text": f"Malformed summary frame: {len(data)} bytes"})
            return events

        main_cycles = u32_be(data, 4)
        header_bus = data[8]
        header_bus_name = BUS_NAMES.get(header_bus, f"bus{header_bus}")
        entry_count = max(0, (len(data) - 12) // 7)
        trailing = (len(data) - 12) % 7

        events.append({
            "kind": "packet",
            "frame_type": frame_type,
            "frame_name": frame_name,
            "main_cycles": main_cycles,
            "bus": header_bus_name,
            "entry_count": entry_count,
            "length": len(data),
            "trailing": trailing,
            "text": f"Gateway summary: bus={header_bus_name}, mainCycles={main_cycles}, "
                    f"signals={entry_count}, payload={len(data)} bytes"
        })

        offset = 12
        index = 0
        while offset + 7 <= len(data):
            signal_id = u16_be(data, offset)
            entry_bus = data[offset + 2]
            raw = u32_be(data, offset + 3)
            decoded = signal_plain_text(signal_id, entry_bus, raw)

            if decoded["frame_id"]:
                frame = f"frame {decoded['frame_id']} {decoded['message']}"
            else:
                frame = decoded["message"]

            text = (f"{decoded['bus']} {frame}: {decoded['signal']} "
                    f"(COM {signal_id} / 0x{signal_id:04X}) = {decoded['value_text']}")

            events.append({
                "kind": "signal",
                "index": index,
                "main_cycles": main_cycles,
                "header_bus": header_bus_name,
                "entry_bus": decoded["bus"],
                "signal_id": signal_id,
                "macro": decoded["macro"],
                "message": decoded["message"],
                "signal": decoded["signal"],
                "frame_id": decoded["frame_id"],
                "raw": raw,
                "value_text": decoded["value_text"],
                "text": text,
            })

            offset += 7
            index += 1

        if trailing:
            events.append({
                "kind": "warning",
                "text": f"Warning: summary frame has {trailing} trailing undecoded byte(s)."
            })

        return events

    if frame_type == 0x04:
        if len(data) < 17:
            events.append({"kind": "unknown", "text": f"Malformed DTC transition frame: {len(data)} bytes"})
            return events

        main_cycles = u32_be(data, 4)
        dtc = u32_be(data, 12) & 0x00FFFFFF
        status = data[16]
        events.append({
            "kind": "dtc",
            "main_cycles": main_cycles,
            "dtc": dtc,
            "status": status,
            "length": len(data),
            "text": f"DTC transition: DTC=0x{dtc:06X}, UDS status=0x{status:02X}, mainCycles={main_cycles}"
        })
        return events

    if frame_type == 0x02:
        events.append({
            "kind": "command",
            "text": f"Command-signal frame seen: {len(data)} bytes"
        })
        return events

    if frame_type == 0x03:
        events.append({
            "kind": "command",
            "text": f"Command-block frame seen: {len(data)} bytes"
        })
        return events

    events.append({
        "kind": "unknown",
        "text": f"Unknown GatewaySwc frame 0x{frame_type:02X}: {len(data)} bytes, data={data[:64].hex(' ')}"
    })
    return events


class UdpListener(threading.Thread):
    def __init__(self, out_queue, stop_event, bind_ip, port, source_filter):
        super().__init__(daemon=True)
        self.out_queue = out_queue
        self.stop_event = stop_event
        self.bind_ip = bind_ip
        self.port = port
        self.source_filter = source_filter.strip()

    def run(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            try:
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, SOCKET_RCVBUF_BYTES)
            except Exception:
                pass
            sock.bind((self.bind_ip, self.port))
            sock.settimeout(0.2)
            self.out_queue.put(("status", f"UDP listening on {self.bind_ip}:{self.port}"))
        except Exception as exc:
            self.out_queue.put(("error", f"Cannot bind UDP socket on {self.bind_ip}:{self.port}: {exc}"))
            try:
                sock.close()
            except Exception:
                pass
            return

        while not self.stop_event.is_set():
            try:
                data, addr = sock.recvfrom(65535)
            except socket.timeout:
                continue
            except OSError as exc:
                self.out_queue.put(("error", f"UDP receive error: {exc}"))
                break

            src_ip, src_port = addr
            if self.source_filter and src_ip != self.source_filter:
                continue

            now = _dt.datetime.now()
            events = decode_gateway_payload(data)
            try:
                self.out_queue.put_nowait(("packet", now, "UDP", src_ip, src_port, data, events))
            except queue.Full:
                pass

        try:
            sock.close()
        except Exception:
            pass
        self.out_queue.put(("status", "UDP listener stopped."))


class TcpClientWorker(threading.Thread):
    def __init__(self, out_queue, stop_event, conn, addr):
        super().__init__(daemon=True)
        self.out_queue = out_queue
        self.stop_event = stop_event
        self.conn = conn
        self.addr = addr

    def run(self):
        src_ip, src_port = self.addr
        try:
            try:
                self.conn.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, SOCKET_RCVBUF_BYTES)
            except Exception:
                pass
            self.conn.settimeout(0.2)
            self.out_queue.put(("status", f"TCP client connected: {src_ip}:{src_port}"))
            while not self.stop_event.is_set():
                try:
                    data = self.conn.recv(65535)
                except socket.timeout:
                    continue
                except OSError as exc:
                    self.out_queue.put(("status", f"TCP receive stopped for {src_ip}:{src_port}: {exc}"))
                    break

                if not data:
                    break

                now = _dt.datetime.now()
                events = decode_gateway_payload(data)
                try:
                    self.out_queue.put_nowait(("packet", now, "TCP", src_ip, src_port, data, events))
                except queue.Full:
                    pass
        finally:
            try:
                self.conn.close()
            except Exception:
                pass
            self.out_queue.put(("status", f"TCP client disconnected: {src_ip}:{src_port}"))


class TcpListener(threading.Thread):
    def __init__(self, out_queue, stop_event, bind_ip, port, source_filter):
        super().__init__(daemon=True)
        self.out_queue = out_queue
        self.stop_event = stop_event
        self.bind_ip = bind_ip
        self.port = port
        self.source_filter = source_filter.strip()
        self.workers = []

    def run(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, SOCKET_RCVBUF_BYTES)
            except Exception:
                pass
            sock.bind((self.bind_ip, self.port))
            sock.listen(16)
            sock.settimeout(0.2)
            self.out_queue.put(("status", f"TCP listening on {self.bind_ip}:{self.port}"))
        except Exception as exc:
            self.out_queue.put(("error", f"Cannot bind TCP socket on {self.bind_ip}:{self.port}: {exc}"))
            try:
                sock.close()
            except Exception:
                pass
            return

        while not self.stop_event.is_set():
            try:
                conn, addr = sock.accept()
            except socket.timeout:
                continue
            except OSError as exc:
                self.out_queue.put(("error", f"TCP accept error: {exc}"))
                break

            src_ip, _ = addr
            if self.source_filter and src_ip != self.source_filter:
                try:
                    conn.close()
                except Exception:
                    pass
                continue

            worker = TcpClientWorker(self.out_queue, self.stop_event, conn, addr)
            self.workers.append(worker)
            worker.start()

        try:
            sock.close()
        except Exception:
            pass
        self.out_queue.put(("status", "TCP listener stopped."))


class HeartbeatSender(threading.Thread):
    def __init__(self, out_queue, stop_event, target_ip, port, protocol, interval_s, payload):
        super().__init__(daemon=True)
        self.out_queue = out_queue
        self.stop_event = stop_event
        self.target_ip = target_ip.strip()
        self.port = int(port)
        self.protocol = protocol.strip().upper()
        self.interval_s = max(0.05, float(interval_s))
        self.payload = payload
        self.udp_sock = None
        self.tcp_sock = None
        self.last_error_text = None
        self.last_error_time = 0.0

    def _uses_udp(self):
        return self.protocol in ("UDP", "UDP+TCP", "BOTH")

    def _uses_tcp(self):
        return self.protocol in ("TCP", "UDP+TCP", "BOTH")

    def _put_status(self, text):
        self.out_queue.put(("status", text))

    def _put_limited_error(self, text):
        now = time.time()
        if text != self.last_error_text or (now - self.last_error_time) >= 5.0:
            self.last_error_text = text
            self.last_error_time = now
            self.out_queue.put(("status", text))

    def _send_udp(self):
        if self.udp_sock is None:
            self.udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.udp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.udp_sock.sendto(self.payload, (self.target_ip, self.port))

    def _connect_tcp(self):
        if self.tcp_sock is not None:
            return
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(0.8)
        sock.connect((self.target_ip, self.port))
        sock.settimeout(0.8)
        self.tcp_sock = sock
        self._put_status(f"TCP heartbeat connected to {self.target_ip}:{self.port}")

    def _send_tcp(self):
        self._connect_tcp()
        self.tcp_sock.sendall(self.payload)

    def _close_tcp(self):
        if self.tcp_sock is not None:
            try:
                self.tcp_sock.close()
            except Exception:
                pass
            self.tcp_sock = None

    def run(self):
        if not self.target_ip:
            self._put_status("Heartbeat disabled: no target IP.")
            return

        self._put_status(
            f"Heartbeat started: {self.protocol} -> {self.target_ip}:{self.port}, "
            f"payload={self.payload.decode('ascii', errors='replace')!r}, interval={self.interval_s:g}s"
        )

        while not self.stop_event.is_set():
            if self._uses_udp():
                try:
                    self._send_udp()
                except Exception as exc:
                    self._put_limited_error(f"UDP heartbeat send failed: {exc}")

            if self._uses_tcp():
                try:
                    self._send_tcp()
                except Exception as exc:
                    self._put_limited_error(f"TCP heartbeat send failed: {exc}")
                    self._close_tcp()

            deadline = time.time() + self.interval_s
            while not self.stop_event.is_set() and time.time() < deadline:
                time.sleep(min(0.05, deadline - time.time()))

        try:
            if self.udp_sock:
                self.udp_sock.close()
        except Exception:
            pass
        self._close_tcp()
        self._put_status("Heartbeat stopped.")


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("AURIX Gateway Ethernet Logger")
        self.geometry("1180x760")

        self.q = queue.Queue(maxsize=QUEUE_MAX_ITEMS)
        self.stop_event = threading.Event()
        self.listeners = []
        self.heartbeat_sender = None
        self.log_file = None
        self.csv_file = None
        self.csv_writer = None
        self.last_values = {}
        self.packet_count = 0
        self.signal_count = 0
        self.dropped_packets = 0
        self.preview_lines = 0
        self._last_file_flush = time.time()
        self._last_status_update = 0.0
        self._text_buffer = []

        self.bind_ip = tk.StringVar(value="0.0.0.0")
        self.port = tk.IntVar(value=UDP_PORT)
        self.source_ip = tk.StringVar(value=AURIX_DEFAULT_IP)
        self.rx_protocol = tk.StringVar(value="UDP")
        self.heartbeat_enabled = tk.BooleanVar(value=True)
        self.heartbeat_protocol = tk.StringVar(value="UDP")
        self.heartbeat_target_ip = tk.StringVar(value=AURIX_DEFAULT_IP)
        self.heartbeat_port = tk.IntVar(value=UDP_PORT)
        self.heartbeat_interval = tk.DoubleVar(value=HEARTBEAT_INTERVAL_S)
        self.heartbeat_payload = tk.StringVar(value=HEARTBEAT_PAYLOAD.decode("ascii"))
        self.only_changes = tk.BooleanVar(value=True)
        self.show_packet_summary = tk.BooleanVar(value=True)
        self.show_signals = tk.BooleanVar(value=False)
        self.write_csv = tk.BooleanVar(value=True)
        self.write_text = tk.BooleanVar(value=True)

        self._build_ui()
        self.after(GUI_POLL_MS, self._poll_queue)

    def _build_ui(self):
        frm = ttk.Frame(self, padding=8)
        frm.pack(fill=tk.BOTH, expand=True)

        top = ttk.LabelFrame(frm, text="Receiver")
        top.pack(fill=tk.X)

        ttk.Label(top, text="Bind IP").grid(row=0, column=0, sticky="w", padx=4, pady=4)
        ttk.Entry(top, textvariable=self.bind_ip, width=16).grid(row=0, column=1, padx=4, pady=4)

        ttk.Label(top, text="Port").grid(row=0, column=2, sticky="w", padx=4, pady=4)
        ttk.Entry(top, textvariable=self.port, width=8).grid(row=0, column=3, padx=4, pady=4)

        ttk.Label(top, text="AURIX source IP filter").grid(row=0, column=4, sticky="w", padx=4, pady=4)
        ttk.Entry(top, textvariable=self.source_ip, width=18).grid(row=0, column=5, padx=4, pady=4)

        ttk.Label(top, text="RX protocol").grid(row=0, column=6, sticky="w", padx=4, pady=4)
        ttk.Combobox(top, textvariable=self.rx_protocol, values=("UDP", "TCP", "UDP+TCP"), width=8, state="readonly").grid(row=0, column=7, padx=4, pady=4)

        self.start_btn = ttk.Button(top, text="Start logging", command=self.start)
        self.start_btn.grid(row=0, column=8, padx=4, pady=4)

        self.stop_btn = ttk.Button(top, text="Stop", command=self.stop, state=tk.DISABLED)
        self.stop_btn.grid(row=0, column=9, padx=4, pady=4)

        hb = ttk.LabelFrame(frm, text="PC heartbeat sender")
        hb.pack(fill=tk.X, pady=(6, 0))

        ttk.Checkbutton(hb, text="Enable", variable=self.heartbeat_enabled).grid(row=0, column=0, padx=4, pady=4)
        ttk.Label(hb, text="Target IP").grid(row=0, column=1, sticky="w", padx=4, pady=4)
        ttk.Entry(hb, textvariable=self.heartbeat_target_ip, width=18).grid(row=0, column=2, padx=4, pady=4)
        ttk.Label(hb, text="Port").grid(row=0, column=3, sticky="w", padx=4, pady=4)
        ttk.Entry(hb, textvariable=self.heartbeat_port, width=8).grid(row=0, column=4, padx=4, pady=4)
        ttk.Label(hb, text="Protocol").grid(row=0, column=5, sticky="w", padx=4, pady=4)
        ttk.Combobox(hb, textvariable=self.heartbeat_protocol, values=("UDP", "TCP", "UDP+TCP"), width=8, state="readonly").grid(row=0, column=6, padx=4, pady=4)
        ttk.Label(hb, text="Interval s").grid(row=0, column=7, sticky="w", padx=4, pady=4)
        ttk.Entry(hb, textvariable=self.heartbeat_interval, width=8).grid(row=0, column=8, padx=4, pady=4)
        ttk.Label(hb, text="Payload").grid(row=0, column=9, sticky="w", padx=4, pady=4)
        ttk.Entry(hb, textvariable=self.heartbeat_payload, width=18).grid(row=0, column=10, padx=4, pady=4)

        opts = ttk.Frame(frm)
        opts.pack(fill=tk.X, pady=(6, 0))

        ttk.Checkbutton(opts, text="Text: packet summary", variable=self.show_packet_summary).pack(side=tk.LEFT, padx=4)
        ttk.Checkbutton(opts, text="Preview: signal entries (heavy)", variable=self.show_signals).pack(side=tk.LEFT, padx=4)
        ttk.Checkbutton(opts, text="Preview only changed values", variable=self.only_changes).pack(side=tk.LEFT, padx=4)
        ttk.Checkbutton(opts, text="Write .csv", variable=self.write_csv).pack(side=tk.LEFT, padx=4)
        ttk.Checkbutton(opts, text="Write .log", variable=self.write_text).pack(side=tk.LEFT, padx=4)

        mid = ttk.Frame(frm)
        mid.pack(fill=tk.BOTH, expand=True, pady=(6, 0))

        self.text = tk.Text(mid, wrap="none", font=("Consolas", 10))
        self.text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        yscroll = ttk.Scrollbar(mid, orient=tk.VERTICAL, command=self.text.yview)
        yscroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.text.configure(yscrollcommand=yscroll.set)

        bottom = ttk.Frame(frm)
        bottom.pack(fill=tk.X, pady=(6, 0))

        self.status = tk.StringVar(value="Idle.")
        ttk.Label(bottom, textvariable=self.status).pack(side=tk.LEFT)

        ttk.Button(bottom, text="Clear window", command=self.clear_window).pack(side=tk.RIGHT, padx=4)
        ttk.Button(bottom, text="Open log folder", command=self.open_log_folder).pack(side=tk.RIGHT, padx=4)

    def _open_files(self):
        stamp = _dt.datetime.now().strftime("%Y%m%d_%H%M%S")
        base = os.path.join(os.path.dirname(os.path.abspath(__file__)), f"aurix_gateway_eth_{stamp}")
        if self.write_text.get():
            self.log_file = open(base + ".log", "w", encoding="utf-8", newline="", buffering=LOG_BUFFER_BYTES)
        if self.write_csv.get():
            self.csv_file = open(base + ".csv", "w", encoding="utf-8", newline="", buffering=LOG_BUFFER_BYTES)
            self.csv_writer = csv.writer(self.csv_file)
            self.csv_writer.writerow([
                "timestamp", "protocol", "src_ip", "src_port", "frame_type", "main_cycles",
                "entry_index", "bus", "frame_id", "message", "signal",
                "com_signal_id_dec", "com_signal_id_hex", "raw_value",
                "decoded_value", "macro"
            ])

    def _close_files(self):
        for f in (self.log_file, self.csv_file):
            if f:
                try:
                    f.flush()
                    f.close()
                except Exception:
                    pass
        self.log_file = None
        self.csv_file = None
        self.csv_writer = None

    def start(self):
        if any(listener.is_alive() for listener in self.listeners):
            return
        self.stop_event.clear()
        self.last_values.clear()
        self.packet_count = 0
        self.signal_count = 0
        self._open_files()

        bind_ip = self.bind_ip.get().strip() or "0.0.0.0"
        port = int(self.port.get())
        source_ip = self.source_ip.get().strip()
        rx_protocol = self.rx_protocol.get().strip().upper()

        self.listeners = []
        if rx_protocol in ("UDP", "UDP+TCP", "BOTH"):
            self.listeners.append(UdpListener(self.q, self.stop_event, bind_ip, port, source_ip))
        if rx_protocol in ("TCP", "UDP+TCP", "BOTH"):
            self.listeners.append(TcpListener(self.q, self.stop_event, bind_ip, port, source_ip))

        for listener in self.listeners:
            listener.start()

        self.heartbeat_sender = None
        if self.heartbeat_enabled.get():
            payload_text = self.heartbeat_payload.get()
            payload = payload_text.encode("utf-8") if payload_text else HEARTBEAT_PAYLOAD
            self.heartbeat_sender = HeartbeatSender(
                self.q,
                self.stop_event,
                self.heartbeat_target_ip.get().strip(),
                int(self.heartbeat_port.get()),
                self.heartbeat_protocol.get().strip(),
                float(self.heartbeat_interval.get()),
                payload,
            )
            self.heartbeat_sender.start()

        self.start_btn.configure(state=tk.DISABLED)
        self.stop_btn.configure(state=tk.NORMAL)
        self._append_line("Logger started.")

    def stop(self):
        self.stop_event.set()
        self._flush_text_buffer()
        self.start_btn.configure(state=tk.NORMAL)
        self.stop_btn.configure(state=tk.DISABLED)
        self._close_files()

    def clear_window(self):
        self._text_buffer.clear()
        self.text.delete("1.0", tk.END)
        self.preview_lines = 0

    def open_log_folder(self):
        folder = os.path.dirname(os.path.abspath(__file__))
        try:
            os.startfile(folder)
        except Exception as exc:
            messagebox.showerror("Open folder failed", str(exc))

    def _append_line(self, line):
        self._text_buffer.append(line)
        if len(self._text_buffer) >= TEXT_FLUSH_MAX_LINES:
            self._flush_text_buffer()

    def _flush_text_buffer(self):
        if not self._text_buffer:
            return
        lines = self._text_buffer
        self._text_buffer = []
        self.text.insert(tk.END, "\n".join(lines) + "\n")
        self.preview_lines += len(lines)
        # Keep preview bounded without doing an expensive line count for every insert.
        if self.preview_lines > 8000:
            self.text.delete("1.0", "2000.0")
            self.preview_lines = max(0, self.preview_lines - 2000)
        self.text.see(tk.END)

    def _write_text_line(self, line):
        if self.log_file:
            self.log_file.write(line + "\n")

    def _write_csv_event(self, timestamp, protocol, src_ip, src_port, event):
        if not self.csv_writer or event.get("kind") != "signal":
            return
        self.csv_writer.writerow([
            timestamp,
            protocol,
            src_ip,
            src_port,
            "summary",
            event.get("main_cycles", ""),
            event.get("index", ""),
            event.get("entry_bus", ""),
            event.get("frame_id", ""),
            event.get("message", ""),
            event.get("signal", ""),
            event.get("signal_id", ""),
            f"0x{int(event.get('signal_id', 0)):04X}",
            event.get("raw", ""),
            event.get("value_text", ""),
            event.get("macro", ""),
        ])

    def _handle_packet(self, now, protocol, src_ip, src_port, data, events):
        self.packet_count += 1
        ts = now.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]

        for ev in events:
            kind = ev.get("kind")

            if kind == "packet":
                line = f"[{ts}] {protocol} {src_ip}:{src_port} - {ev['text']}"
                if self.show_packet_summary.get():
                    self._append_line(line)
                self._write_text_line(line)
                continue

            if kind == "signal":
                self.signal_count += 1
                key = ev.get("signal_id")
                raw = ev.get("raw")
                changed = (self.last_values.get(key) != raw)
                self.last_values[key] = raw

                line = f"[{ts}]   {ev['text']}"
                if self.show_signals.get() and ((not self.only_changes.get()) or changed):
                    self._append_line(line)
                self._write_text_line(line)
                self._write_csv_event(ts, protocol, src_ip, src_port, ev)
                continue

            line = f"[{ts}] {protocol} {src_ip}:{src_port} - {ev.get('text', str(ev))}"
            self._append_line(line)
            self._write_text_line(line)

        now_mono = time.time()
        if now_mono - self._last_file_flush >= 1.0:
            if self.csv_file:
                self.csv_file.flush()
            if self.log_file:
                self.log_file.flush()
            self._last_file_flush = now_mono

        if now_mono - self._last_status_update >= STATUS_UPDATE_INTERVAL_S:
            self.status.set(f"Packets: {self.packet_count} | Signal rows: {self.signal_count} | Last from {protocol} {src_ip}:{src_port} | Queue: {self.q.qsize()}")
            self._last_status_update = now_mono

    def _poll_queue(self):
        # Aggressively drain the producer queue. Text output is batched, so this
        # can process thousands of received packets per GUI cycle without doing
        # thousands of expensive Tk insert/see/status operations.
        processed = 0
        start = time.time()
        max_items = MAX_QUEUE_ITEMS_PER_POLL
        max_seconds = MAX_QUEUE_DRAIN_SECONDS

        while processed < max_items and (time.time() - start) < max_seconds:
            try:
                item = self.q.get_nowait()
            except queue.Empty:
                break

            processed += 1
            typ = item[0]
            if typ == "status":
                self.status.set(item[1])
                self._append_line(item[1])
            elif typ == "error":
                self.status.set(item[1])
                self._append_line("ERROR: " + item[1])
                self._flush_text_buffer()
                messagebox.showerror("Logger error", item[1])
                self.stop()
            elif typ == "packet":
                _, now, protocol, src_ip, src_port, data, events = item
                self._handle_packet(now, protocol, src_ip, src_port, data, events)

        self.after(GUI_POLL_MS, self._poll_queue)

    def destroy(self):
        try:
            self.stop()
        finally:
            super().destroy()


if __name__ == "__main__":
    app = App()
    app.mainloop()
