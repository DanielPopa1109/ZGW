import binascii
import json
import re
import zipfile
from dataclasses import asdict, dataclass, field
from datetime import datetime
from pathlib import Path


BUNDLE_SCHEMA = "FCD_PARALLEL_PACKAGE_V1"
BUNDLE_MANIFEST = "manifest.json"
MAX_ACTIVE_PER_BUS = 2
BUS_ORDER = {"CANFD": 0, "CAN": 1, "LIN": 2, "ETHERNET": 3, "ETH": 3, "UNKNOWN": 9}


def bus_category(bus_type: str) -> str:
    text = str(bus_type or "UNKNOWN").upper()
    if "CANFD" in text:
        return "CANFD"
    if "CAN" in text:
        return "CAN"
    if "LIN" in text:
        return "LIN"
    if text in ("ETH", "ETHERNET", "DOIP"):
        return "ETHERNET"
    return text or "UNKNOWN"


def bus_sort_key(bus_type: str) -> int:
    return BUS_ORDER.get(bus_category(bus_type), BUS_ORDER["UNKNOWN"])


@dataclass
class NodeInfo:
    node_name: str
    bus_type: str
    source_file: str
    request_id: str = ""
    response_id: str = ""
    target_logical_address: str = ""
    is_zgw: bool = False
    inferred: bool = False
    notes: list[str] = field(default_factory=list)


@dataclass
class ScheduleEvent:
    time_slot: int
    phase: str
    bus_type: str
    node_name: str
    active_on_bus: int
    is_zgw: bool


def _repo_relative(path: Path, root: Path) -> str:
    try:
        return str(path.resolve().relative_to(root.resolve()))
    except ValueError:
        return str(path)


def _bus_from_path(path: Path) -> str:
    if path.suffix.lower() in (".dbc", ".pdbc", ".ldf"):
        return path.stem
    return "UNKNOWN"


def _merge_node(nodes: dict[str, NodeInfo], node: NodeInfo) -> None:
    if node.node_name.upper() == "TESTER":
        return
    if node.node_name.upper() == "ZGW":
        node.bus_type = "ETHERNET"
        node.is_zgw = True
    existing = nodes.get(node.node_name)
    if existing is None:
        nodes[node.node_name] = node
        return
    if existing.bus_type == "UNKNOWN" and node.bus_type != "UNKNOWN":
        existing.bus_type = node.bus_type
    if node.source_file not in existing.source_file.split(";"):
        existing.source_file = existing.source_file + ";" + node.source_file
    existing.is_zgw = existing.is_zgw or node.is_zgw
    existing.inferred = existing.inferred or node.inferred
    for note in node.notes:
        if note not in existing.notes:
            existing.notes.append(note)


def _parse_dbc_nodes(path: Path, root: Path) -> list[NodeInfo]:
    text = path.read_text(encoding="latin-1", errors="ignore")
    bus = _bus_from_path(path)
    rel = _repo_relative(path, root)
    nodes: dict[str, NodeInfo] = {}
    for match in re.finditer(r"^BU_:\s*(.*)$", text, re.MULTILINE):
        for name in match.group(1).split():
            _merge_node(nodes, NodeInfo(name, bus, rel, is_zgw=(name.upper() == "ZGW")))
    for match in re.finditer(r"^BO_\s+(\d+)\s+\w+:\s+\d+\s+([A-Za-z0-9_]+)", text, re.MULTILINE):
        frame_id = int(match.group(1))
        name = match.group(2)
        node = NodeInfo(name, bus, rel, is_zgw=(name.upper() == "ZGW"), inferred=True)
        if 0x700 <= frame_id <= 0x7FF:
            node.request_id = f"0x{frame_id:X}"
            node.notes.append("Diagnostic-looking CAN identifier inferred from DBC frame ID")
        _merge_node(nodes, node)
    return sorted(nodes.values(), key=lambda n: (bus_sort_key(n.bus_type), n.node_name))


def _parse_ldf_nodes(path: Path, root: Path) -> list[NodeInfo]:
    text = path.read_text(encoding="latin-1", errors="ignore")
    rel = _repo_relative(path, root)
    nodes: dict[str, NodeInfo] = {}
    nodes_block = re.search(r"Nodes\s*\{(?P<body>.*?)\}", text, re.DOTALL)
    if nodes_block:
        body = nodes_block.group("body")
        master = re.search(r"Master:\s*([A-Za-z0-9_]+)", body)
        if master:
            name = master.group(1)
            _merge_node(nodes, NodeInfo(name, "LIN", rel, is_zgw=(name.upper() == "ZGW")))
        slaves = re.search(r"Slaves:\s*([^;]+);", body)
        if slaves:
            for name in re.split(r"[\s,]+", slaves.group(1).strip()):
                if name:
                    _merge_node(nodes, NodeInfo(name, "LIN", rel, is_zgw=(name.upper() == "ZGW")))
    return sorted(nodes.values(), key=lambda n: n.node_name)


def discover_nodes(root: Path) -> tuple[list[NodeInfo], list[str]]:
    root = Path(root)
    candidates = []
    for pattern in ("*.dbc", "*.pdbc", "*.ldf", "*.arxml", "*.xml", "*.json"):
        candidates.extend(root.rglob(pattern))
    nodes: dict[str, NodeInfo] = {}
    logs: list[str] = []
    for path in sorted(set(candidates)):
        suffix = path.suffix.lower()
        try:
            parsed = []
            if suffix in (".dbc", ".pdbc"):
                parsed = _parse_dbc_nodes(path, root)
            elif suffix == ".ldf":
                parsed = _parse_ldf_nodes(path, root)
            elif any(part.lower().startswith("diagnostic") for part in path.parts):
                continue
            for node in parsed:
                _merge_node(nodes, node)
        except Exception as exc:
            logs.append(f"{_repo_relative(path, root)} skipped: {exc}")
    out = sorted(nodes.values(), key=lambda n: (n.is_zgw, bus_sort_key(n.bus_type), n.node_name))
    if not out:
        logs.append("No nodes discovered from repository databases")
    return out, logs


def build_schedule(targets: list[dict], include_flash=True, include_coding=True) -> list[ScheduleEvent]:
    events: list[ScheduleEvent] = []
    for phase, enabled in (("flash", include_flash), ("coding", include_coding)):
        if not enabled:
            continue
        slot = 0
        non_zgw = [t for t in targets if not t.get("is_zgw")]
        zgw = [t for t in targets if t.get("is_zgw")]
        for group in (non_zgw, zgw):
            queues: dict[str, list[dict]] = {}
            for target in sorted(group, key=lambda t: (bus_sort_key(t.get("bus_type", "UNKNOWN")), t.get("node_name", ""))):
                queues.setdefault(bus_category(target.get("bus_type", "UNKNOWN")), []).append(target)
            while any(queues.values()):
                for bus in sorted(queues, key=bus_sort_key):
                    active = queues[bus][:MAX_ACTIVE_PER_BUS]
                    queues[bus] = queues[bus][MAX_ACTIVE_PER_BUS:]
                    for target in active:
                        events.append(
                            ScheduleEvent(slot, phase, bus, target.get("node_name", ""), len(active), bool(target.get("is_zgw")))
                        )
                slot += 1
    return events


def validate_schedule(events: list[ScheduleEvent]) -> list[str]:
    errors = []
    by_slot_phase_bus: dict[tuple[int, str, str], int] = {}
    for event in events:
        key = (event.time_slot, event.phase, event.bus_type)
        by_slot_phase_bus[key] = by_slot_phase_bus.get(key, 0) + 1
    for key, count in by_slot_phase_bus.items():
        if count > MAX_ACTIVE_PER_BUS:
            errors.append(f"{key}: {count} active jobs exceeds {MAX_ACTIVE_PER_BUS}")
    for phase in ("flash", "coding"):
        zgw_slots = [e.time_slot for e in events if e.phase == phase and e.is_zgw]
        non_zgw_slots = [e.time_slot for e in events if e.phase == phase and not e.is_zgw]
        if zgw_slots and non_zgw_slots and min(zgw_slots) <= max(non_zgw_slots):
            errors.append(f"{phase}: ZGW is not scheduled after all non-ZGW targets")
    return errors


def create_bundle(bundle_path: Path, manifest: dict, payload_files: list[tuple[Path, str]]) -> Path:
    bundle_path = Path(bundle_path)
    bundle_path.parent.mkdir(parents=True, exist_ok=True)
    manifest = dict(manifest)
    manifest["schema"] = BUNDLE_SCHEMA
    manifest["generated_timestamp"] = manifest.get("generated_timestamp") or datetime.now().isoformat(timespec="seconds")
    manifest.setdefault("integrity", {})
    with zipfile.ZipFile(bundle_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for source, arcname in payload_files:
            data = Path(source).read_bytes()
            manifest["integrity"][arcname.replace("\\", "/")] = {
                "size": len(data),
                "crc32": f"0x{binascii.crc32(data) & 0xFFFFFFFF:08X}",
            }
            archive.writestr(arcname.replace("\\", "/"), data)
        manifest["integrity_note"] = "Payload entries are covered here; manifest.json is protected by the ZIP container."
        archive.writestr(BUNDLE_MANIFEST, json.dumps(manifest, indent=2, sort_keys=True))
    return bundle_path


def load_bundle(bundle_path: Path) -> dict:
    with zipfile.ZipFile(bundle_path, "r") as archive:
        manifest = json.loads(archive.read(BUNDLE_MANIFEST).decode("utf-8"))
        if manifest.get("schema") != BUNDLE_SCHEMA:
            raise ValueError(f"Unsupported bundle schema {manifest.get('schema')}")
        return manifest


def node_to_target(node: NodeInfo, extended_diag_address: str = "", simulated_enabled: bool = True) -> dict:
    return {
        "node_name": node.node_name,
        "bus_type": node.bus_type,
        "node_kind": "Real" if node.is_zgw else "Simulated",
        "simulated_enabled": simulated_enabled,
        "extended_diag_address": extended_diag_address,
        "route_metadata": {
            "source_file": node.source_file,
            "request_id": node.request_id,
            "response_id": node.response_id,
            "target_logical_address": node.target_logical_address,
            "inferred": node.inferred,
            "notes": node.notes,
        },
        "is_zgw": node.is_zgw,
        "flash_order_group": 99 if node.is_zgw else 10,
        "coding_order_group": 99 if node.is_zgw else 10,
        "payload_blocks": [],
        "coding_descriptor": {},
    }


def manifest_from_targets(project: str, vin: str, source_db_files: list[str], targets: list[dict]) -> dict:
    ordered = sorted(targets, key=lambda t: (t.get("is_zgw", False), bus_sort_key(t.get("bus_type", "UNKNOWN")), t.get("node_name", "")))
    return {
        "project": project,
        "vin": vin,
        "source_db_files": sorted(source_db_files),
        "targets": ordered,
        "assumptions": [
            "Extended diagnostic addressing is treated as opaque per-target metadata.",
            "ZGW flash/coding is skipped when any prerequisite non-ZGW target fails.",
            "Missing route metadata is logged and left blank rather than guessed.",
        ],
    }


def to_jsonable_events(events: list[ScheduleEvent]) -> list[dict]:
    return [asdict(event) for event in events]
