import tempfile
import unittest
from pathlib import Path

from fcd_parallel import (
    BUNDLE_SCHEMA,
    build_schedule,
    create_bundle,
    discover_nodes,
    load_bundle,
    manifest_from_targets,
    validate_schedule,
)


class FcdParallelTests(unittest.TestCase):
    def test_discover_nodes_from_dbc_and_ldf(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "net.dbc").write_text("BU_: Tester ZGW ECU1\nBO_ 1792 DiagReq: 8 Tester\n", encoding="utf-8")
            (root / "net.ldf").write_text("Nodes { Master: ZGW, 10 ms, 5 ms; Slaves: ALT, HVDCDC; }", encoding="utf-8")
            nodes, logs = discover_nodes(root)
            names = {node.node_name for node in nodes}
            self.assertIn("ZGW", names)
            self.assertIn("ECU1", names)
            self.assertIn("ALT", names)
            self.assertIsInstance(logs, list)

    def test_bundle_round_trip(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            payload = root / "block.bin"
            payload.write_bytes(b"abc")
            targets = [{"node_name": "ECU1", "bus_type": "CAN", "is_zgw": False, "payload_blocks": []}]
            manifest = manifest_from_targets("P", "VIN", ["net.dbc"], targets)
            path = create_bundle(root / "job.pfpkg", manifest, [(payload, "payloads/block.bin")])
            loaded = load_bundle(path)
            self.assertEqual(loaded["schema"], BUNDLE_SCHEMA)
            self.assertIn("payloads/block.bin", loaded["integrity"])

    def test_scheduler_limits_two_per_bus_and_zgw_last(self):
        targets = [
            {"node_name": "A", "bus_type": "CAN", "is_zgw": False},
            {"node_name": "B", "bus_type": "CAN", "is_zgw": False},
            {"node_name": "C", "bus_type": "CAN", "is_zgw": False},
            {"node_name": "ZGW", "bus_type": "CAN", "is_zgw": True},
        ]
        events = build_schedule(targets, include_flash=True, include_coding=True)
        self.assertEqual(validate_schedule(events), [])
        flash_zgw_slot = min(e.time_slot for e in events if e.phase == "flash" and e.is_zgw)
        flash_non_zgw_last = max(e.time_slot for e in events if e.phase == "flash" and not e.is_zgw)
        coding_zgw_slot = min(e.time_slot for e in events if e.phase == "coding" and e.is_zgw)
        coding_non_zgw_last = max(e.time_slot for e in events if e.phase == "coding" and not e.is_zgw)
        self.assertGreater(flash_zgw_slot, flash_non_zgw_last)
        self.assertGreater(coding_zgw_slot, coding_non_zgw_last)
        for event in events:
            self.assertLessEqual(event.active_on_bus, 2)


if __name__ == "__main__":
    unittest.main()
