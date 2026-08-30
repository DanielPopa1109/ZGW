import tempfile
import unittest
import importlib.util
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

FCD_PATH = Path(__file__).resolve().parent / "FCD.pyw"
FCD_SPEC = importlib.util.spec_from_file_location("fcd_app_module", FCD_PATH)
fcd_app_module = importlib.util.module_from_spec(FCD_SPEC)
FCD_SPEC.loader.exec_module(fcd_app_module)


class FcdParallelTests(unittest.TestCase):
    def test_discover_nodes_from_dbc_and_ldf(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "net.dbc").write_text("BU_: Tester ZGW ECU1\nBO_ 1792 DiagReq: 8 Tester\n", encoding="utf-8")
            (root / "net.ldf").write_text("Nodes { Master: ZGW, 10 ms, 5 ms; Slaves: HVDCDC; }", encoding="utf-8")
            nodes, logs = discover_nodes(root)
            names = {node.node_name for node in nodes}
            self.assertIn("ZGW", names)
            self.assertIn("ECU1", names)
            self.assertIn("HVDCDC", names)
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

    def test_lin_dtc_descriptions_and_snapshot_detail(self):
        app = object.__new__(fcd_app_module.FcdApp)
        self.assertEqual(
            app._describe_zgw_dtc(fcd_app_module.DEM_DTC_LIN1_HVDCDC_NO_COMMUNICATION),
            "LIN1 Slave HVDCDC - No Communication",
        )
        data = bytearray(36)
        base = fcd_app_module.DEM_DTC_TIMESTAMP_DATA_SIZE
        data[base + 1] = 0
        data[base + 2] = 0
        data[base + 3] = 0x0B
        data[base + 4] = 0x8B
        data[base + 5] = 4
        data[base + 6] = 0
        data[base + 7] = 2
        data[base + 8] = 2
        data[base + 9] = 2
        data[base + 10] = 7
        data[base + 11] = 8
        detail = app._explain_dtc_detail(
            fcd_app_module.DEM_DTC_LIN1_HVDCDC_NO_COMMUNICATION,
            0x04,
            bytes(data),
        )
        self.assertIn("LIN1 Slave HVDCDC - No Communication", detail)
        self.assertIn("Last PID=0x8B", detail)
        self.assertIn("Slave NAD=4", detail)
        self.assertIn("Error type=TIMEOUT", detail)
        self.assertIn("No-response counter=8", detail)

    def test_can_dtc_snapshot_detail(self):
        app = object.__new__(fcd_app_module.FcdApp)
        data = bytearray(50)
        base = fcd_app_module.DEM_DTC_TIMESTAMP_DATA_SIZE
        data[base + 1] = 1
        data[base + 2] = 3
        data[base + 3] = 1
        data[base + 4] = 2
        data[base + 5] = 170
        data[base + 6] = 33
        data[base + 7] = 5
        data[base + 8] = 2
        data[base + 9] = 0
        data[base + 10:base + 14] = (7).to_bytes(4, "big")
        data[base + 14] = 0
        data[base + 15] = 1
        data[base + 16] = 0
        data[base + 17] = 1
        data[base + 18] = 0
        data[base + 19:base + 21] = (12).to_bytes(2, "big")
        data[base + 21:base + 23] = (4).to_bytes(2, "big")
        data[base + 23:base + 25] = (20).to_bytes(2, "big")
        data[base + 25:base + 27] = (5).to_bytes(2, "big")
        data[base + 27] = 1
        detail = app._explain_dtc_detail(
            fcd_app_module.DEM_DTC_CANFD_PROTOCOL_ERROR,
            0x04,
            bytes(data),
        )
        self.assertIn("ZGW_CANFD_2 Excessive Protocol Error", detail)
        self.assertIn("ZGW_CANFD_2 Excessive Protocol Error", app._describe_zgw_dtc(fcd_app_module.DEM_DTC_CANFD_PROTOCOL_ERROR))
        self.assertIn("bus-off count=7", detail)
        self.assertIn("operational=no", detail)
        self.assertIn("normal TX enabled=no", detail)
        self.assertIn("error-passive debounce fail/pass=12/4", detail)
        self.assertIn("protocol-error debounce fail/pass=20/5", detail)
        data[base + 2] = 1
        response = (
            b"\x59\x04\x02\x21\x05\x2F\xFF"
            + bytes(data)
        )
        display = app._decode_dtc_detail_response(
            "CANFD snapshot",
            response,
            fcd_app_module.DEM_DTC_CANFD_ERROR_PASSIVE,
            0x04,
        )
        self.assertIn("ZGW_CANFD_2 Error Passive", display)
        self.assertNotIn("data=", display)

    def test_all_can_bus_dtc_snapshot_details_are_readable(self):
        app = object.__new__(fcd_app_module.FcdApp)
        cases = [
            (fcd_app_module.DEM_DTC_CAN_CLASSIC_BUS_OFF, 0, 0, "ZGW_CAN_3 Bus-Off"),
            (fcd_app_module.DEM_DTC_CAN_CLASSIC_ERROR_PASSIVE, 0, 1, "ZGW_CAN_3 Error Passive"),
            (fcd_app_module.DEM_DTC_CAN_CLASSIC_CONTROLLER_FAULT, 0, 2, "ZGW_CAN_3 Controller Fault"),
            (fcd_app_module.DEM_DTC_CAN_CLASSIC_PROTOCOL_ERROR, 0, 3, "ZGW_CAN_3 Excessive Protocol Error"),
            (fcd_app_module.DEM_DTC_CANFD_BUS_OFF, 1, 0, "ZGW_CANFD_2 Bus-Off"),
            (fcd_app_module.DEM_DTC_CANFD_ERROR_PASSIVE, 1, 1, "ZGW_CANFD_2 Error Passive"),
            (fcd_app_module.DEM_DTC_CANFD_CONTROLLER_FAULT, 1, 2, "ZGW_CANFD_2 Controller Fault"),
            (fcd_app_module.DEM_DTC_CANFD_PROTOCOL_ERROR, 1, 3, "ZGW_CANFD_2 Excessive Protocol Error"),
        ]
        for dtc, controller, fault, expected_name in cases:
            with self.subTest(dtc=f"0x{dtc:06X}"):
                data = bytearray(50)
                base = fcd_app_module.DEM_DTC_TIMESTAMP_DATA_SIZE
                data[20] = 2
                data[21] = 4
                data[base + 1] = controller
                data[base + 2] = fault
                data[base + 3] = 1
                data[base + 4] = 2 if fault == 1 else 0
                data[base + 5] = 128 if fault in (1, 3) else 0
                data[base + 6] = 0
                data[base + 7] = 3
                data[base + 8] = 2
                data[base + 9] = 3
                data[base + 10:base + 14] = (1 if fault == 0 else 0).to_bytes(4, "big")
                response = (
                    b"\x59\x04"
                    + dtc.to_bytes(3, "big")
                    + b"\x2F\xFF"
                    + bytes(data)
                )
                display = app._decode_dtc_detail_response("CAN snapshot", response, dtc, 0x04)
                self.assertIn(expected_name, display)
                self.assertIn("CAN bus DTC occurrence time", display)
                self.assertNotIn("data=", display)

    def test_screenshot_canfd_error_passive_snapshot_is_readable(self):
        app = object.__new__(fcd_app_module.FcdApp)
        data = bytes.fromhex(
            "00 07 A1 BC 34 CC 2C C2 18 D0 62 93 FA 44 10 A4 "
            "01 01 10 CF 02 04 04 01 01 01 02 80 00 03 02 03 "
            "00 00 00 00 01 01 01 00 00 00 64 00 00 00 00"
        )
        response = (
            b"\x59\x04\x02\x21\x05\x2F\xFF"
            + data
        )
        display = app._decode_dtc_detail_response(
            "CANFD snapshot",
            response,
            fcd_app_module.DEM_DTC_CANFD_ERROR_PASSIVE,
            0x04,
        )
        self.assertIn("ZGW_CANFD_2 Error Passive", display)
        self.assertIn("TEC=128", display)
        self.assertNotIn("data=", display)


if __name__ == "__main__":
    unittest.main()
