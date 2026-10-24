import importlib.util
import copy
import sys
import tempfile
import unittest
import binascii
import struct
import threading
from types import SimpleNamespace
from pathlib import Path

FCD_PATH = Path(__file__).resolve().parent / "FCD.pyw"
FCD_SPEC = importlib.util.spec_from_file_location("fcd_app_module", FCD_PATH)
fcd_app_module = importlib.util.module_from_spec(FCD_SPEC)
sys.modules[FCD_SPEC.name] = fcd_app_module
FCD_SPEC.loader.exec_module(fcd_app_module)

BUNDLE_SCHEMA = fcd_app_module.BUNDLE_SCHEMA
build_schedule = fcd_app_module.build_schedule
create_bundle = fcd_app_module.create_bundle
discover_nodes = fcd_app_module.discover_nodes
load_bundle = fcd_app_module.load_bundle
manifest_from_targets = fcd_app_module.manifest_from_targets
validate_schedule = fcd_app_module.validate_schedule
filtered_hex_rows_for_layout = fcd_app_module.filtered_hex_rows_for_layout


class FcdParallelTests(unittest.TestCase):
    def test_sota_accepts_segmented_images_and_checks_complete_vehicle(self):
        manifest = {
            "schema": BUNDLE_SCHEMA,
            "sota_bundle_role": "new",
            "targets": [
                {"node_name": name, "payload_blocks": [
                    {"flash_kind": kind}
                    for kind in (["FBL", "APPL"] if name == "ZGW" else ["FBL", "APPL", "APPL"])
                ]}
                for name in ("CBM", "DMU", "ELC", "FRBE", "HVDCDC", "PDM1", "ZGW")
            ],
        }
        fcd_app_module.validate_sota_manifest(manifest, "new")
        with self.assertRaisesRegex(ValueError, "rollback"):
            fcd_app_module.validate_sota_manifest(manifest, "rollback")
        for mutation in ("missing_target", "missing_app", "duplicate", "bad_kind", "fake_keys"):
            invalid = copy.deepcopy(manifest)
            if mutation == "missing_target":
                invalid["targets"].pop()
            elif mutation in ("missing_app", "fake_keys"):
                invalid["targets"][-1]["payload_blocks"].pop()
                invalid["metadata"] = {"flash_kind": "APPL"}
            elif mutation == "duplicate":
                invalid["targets"][-1]["node_name"] = "CBM"
            else:
                invalid["targets"][0]["payload_blocks"][0]["flash_kind"] = "CODING"
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                fcd_app_module.validate_sota_manifest(invalid, "new")

    def test_stale_binary_path_rebases_to_workspace_binary_folder(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            binary = root / "Binary"
            binary.mkdir()
            expected = binary / "CBM_APP.hex"
            expected.write_text(":00000001FF\n", encoding="ascii")
            row = {
                "ecu": "CBM_APP",
                "hex": r"C:\Users\Daniel\Desktop\ZGW_Repo\Binary\CBM_APP.hex",
            }
            rebased = filtered_hex_rows_for_layout([row], root)
            self.assertEqual(rebased[0]["hex"], str(expected))

    def test_fallback_vehicle_layout_when_repository_is_missing(self):
        nodes = fcd_app_module.fallback_vehicle_nodes(
            {"data_editor": {"nodes": {}, "hex_rows": []}}
        )
        by_name = {node.node_name: node for node in nodes}
        self.assertEqual(
            set(by_name), {"CBM", "CM5", "DMU", "ELC", "FRBE", "G26", "HVDCDC", "PDM1", "ZGW"}
        )
        self.assertEqual(by_name["CM5"].bus_type, "ETHERNET")
        self.assertEqual(by_name["PDM1"].bus_type, "CANFD")
        self.assertEqual(by_name["HVDCDC"].bus_type, "LIN")
        self.assertTrue(by_name["ZGW"].is_zgw)

    def test_discover_nodes_from_dbc_and_ldf(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "net.dbc").write_text(
                "BU_: Tester ZGW ECU1\nBO_ 1792 DiagReq: 8 Tester\n", encoding="utf-8"
            )
            (root / "net.ldf").write_text(
                "Nodes { Master: ZGW, 10 ms, 5 ms; Slaves: HVDCDC; }", encoding="utf-8"
            )
            nodes, logs = discover_nodes(root)
            names = {node.node_name for node in nodes}
            self.assertIn("ZGW", names)
            self.assertIn("ECU1", names)
            self.assertIn("HVDCDC", names)
            self.assertIn("G26", names)
            self.assertIsInstance(logs, list)

    def test_g26_hex_gaps_make_one_valid_wire_image(self):
        base = fcd_app_module.G26_APP_VECTOR
        vector = struct.pack("<II", 0x20010000, base + 9)
        pieces = [
            fcd_app_module.HexSegment(base, vector),
            fcd_app_module.HexSegment(base + 12, b"\x11\x22\x33\x44"),
        ]
        image = fcd_app_module.coalesce_g26_hex_image(pieces, False)[0]
        self.assertEqual(image.address, base)
        self.assertEqual(image.data, vector + b"\xFF" * 4 + b"\x11\x22\x33\x44")
        with self.assertRaises(fcd_app_module.FcdError):
            fcd_app_module.validate_g26_image(base + 0x200, image.data, False)

    def test_g26_image_cannot_use_zgw_or_other_bus_descriptor(self):
        payload = {"node_name": "G26", "ecu": "G26"}
        valid = {"node_name": "G26", "bus_type": "CAN",
                 "extended_diag_address": "0x55"}
        fcd_app_module.validate_g26_target_info(valid, payload)
        for wrong in (
            {"node_name": "ZGW", "bus_type": "CAN", "is_zgw": True},
            {"node_name": "G26", "bus_type": "CANFD"},
            {"node_name": "G26", "bus_type": "CAN", "extended_diag_address": "0x54"},
        ):
            with self.subTest(target=wrong), self.assertRaises(fcd_app_module.FcdError):
                fcd_app_module.validate_g26_target_info(wrong, payload)

    def test_g26_corrupt_package_stops_before_opening_gateway(self):
        app = fcd_app_module.FcdApp.__new__(fcd_app_module.FcdApp)
        base = fcd_app_module.G26_APP_VECTOR
        image = struct.pack("<II", 0x20010000, base + 9) + b"\xAA" * 8
        app._read_payload_data = lambda _payload: image
        app._new_parallel_client = lambda _addr: self.fail("gateway opened before preflight")
        target = {"node_name": "G26", "bus_type": "CAN",
                  "extended_diag_address": "0x55"}
        payload = {"node_name": "G26", "ecu": "G26", "flash_kind": "APPL",
                   "address": f"0x{base:08X}", "size": len(image), "crc32": "0x00000000"}
        with self.assertRaisesRegex(fcd_app_module.FcdError, "CRC32"):
            app._execute_payloads_parallel_worker([payload], target, None)

    def test_g26_app_and_fbl_flash_sequences_use_target_routines(self):
        app = fcd_app_module.FcdApp.__new__(fcd_app_module.FcdApp)
        app.block_size_var = SimpleNamespace(get=lambda: "253")
        app.erase_var = SimpleNamespace(get=lambda: True)
        app.transfer_crc_var = SimpleNamespace(get=lambda: True)
        app.verify_crc_var = SimpleNamespace(get=lambda: True)
        app.fbl_erase_timeout_var = SimpleNamespace(get=lambda: "120")
        app.root = SimpleNamespace(after=lambda _delay, callback: callback())
        app.progress = SimpleNamespace(configure=lambda **_kwargs: None)
        app.log = lambda _line: None
        target = {"node_name": "G26", "bus_type": "CAN", "extended_diag_address": "0x55"}
        for is_fbl, base in ((False, fcd_app_module.G26_APP_VECTOR),
                             (True, fcd_app_module.G26_FBL_BASE)):
            image = struct.pack("<II", 0x20010000, base + 9) + b"\xAA" * 8
            requests = []
            def send(request, _label, timeout=None, allow_no_response=False):
                requests.append(bytes(request))
                if request[0] == 0x31:
                    return b"\x71" + request[1:4] + (b"\x00" if is_fbl else b"")
                if request[0] == 0x34:
                    return b"\x74\x20\x01\x00"
                if request[0] == 0x36:
                    return bytes([0x76, request[1]])
                if request[0] == 0x37:
                    return b"\x77"
                raise AssertionError(request)
            app._read_payload_data = lambda _payload: image
            app._make_target_uds_sender = lambda *_args, **_kwargs: send
            payload = {
                "ecu": "G26", "node_name": "G26", "target_logical_address": "0x1001",
                "address": f"0x{base:08X}", "size": len(image),
                "crc32": f"0x{binascii.crc32(image) & 0xFFFFFFFF:08X}",
                "flash_kind": "FBL" if is_fbl else "APPL",
            }
            app._execute_payload(object(), payload, is_fbl=is_fbl, target_info=target)
            routines = [request[2:4] for request in requests if request[0] == 0x31]
            self.assertEqual(routines, [b"\x01\x55", b"\x00\x01"] if is_fbl
                             else [b"\x00\x01"])
            self.assertEqual([request[0] for request in requests][-3:], [0x34, 0x36, 0x37])
            self.assertEqual(requests[-1], b"\x37" + struct.pack(">I", binascii.crc32(image)))

    def test_routed_response_pending_waits_for_target_final_response(self):
        client = fcd_app_module.DoipClient("localhost", 13400, 0x0710, 0x1001)
        request = b"\x55\x31\x01\x00\x01"
        frames = iter([
            (fcd_app_module.DOIP_PT_DIAG_MSG,
             struct.pack(">HH", 0x1001, 0x0710) + b"\x55\x7F\x31\x78"),
            (fcd_app_module.DOIP_PT_DIAG_MSG,
             struct.pack(">HH", 0x1001, 0x0710) + b"\x55\x71\x01\x00\x01"),
        ])
        client._send_frame = lambda *_args, **_kwargs: None
        client._recv_frame = lambda _deadline: next(frames)
        self.assertEqual(client.send_uds(request), b"\x55\x71\x01\x00\x01")
        self.assertEqual(client.last_nrc78_count, 1)

    def test_g26_simulated_descriptor_does_not_accept_missing_real_response(self):
        app = fcd_app_module.FcdApp.__new__(fcd_app_module.FcdApp)
        app.log = lambda _line: None
        app._target_is_simulated = lambda _target: True
        app._make_uds_sender = lambda *_args, **_kwargs: None
        app._routed_bus_semaphore = lambda _target: ("CAN", None)
        app._routed_node_semaphore = lambda _target: ("G26", None)
        app._pace_routed_bus_request = lambda _target: None
        app.routed_bus_pace_lock = threading.Lock()
        app.routed_bus_last_request_ts = {}
        calls = []

        class SilentClient:
            def send_uds(self, request, **kwargs):
                calls.append((request, kwargs))
                return b""

        target = {"node_name": "G26", "bus_type": "CAN", "node_kind": "Simulated",
                  "extended_diag_address": "0x55"}
        send = app._make_target_uds_sender(SilentClient(), target)
        with self.assertRaisesRegex(fcd_app_module.FcdError, "no ECU response"):
            send(b"\x22\xF1\x87", "G26 identity")
        self.assertEqual(calls[0][0], b"\x55\x22\xF1\x87")
        self.assertFalse(calls[0][1]["allow_no_response"])

    def test_g26_app_entry_checks_resident_identity(self):
        app = fcd_app_module.FcdApp.__new__(fcd_app_module.FcdApp)
        app.log = lambda _line: None
        requests = []
        mode = ["app"]

        def send(request, _label, **_kwargs):
            requests.append(request)
            if request == b"\x22\xF1\x87":
                return (fcd_app_module.G26_APP_IDENTITY if mode[0] == "app"
                        else fcd_app_module.G26_FBL_IDENTITY)
            if request[:1] == b"\x10":
                if request == b"\x10\x02":
                    mode[0] = "fbl"
                return bytes([0x50, request[1]])
            if request in (b"\x28\x01\x03", b"\x85\x02"):
                return bytes([request[0] + 0x40]) + request[1:]
            raise AssertionError(request)

        app._make_target_uds_sender = lambda *_args, **_kwargs: send
        app._execute_target_flash_preamble(
            object(), {"node_name": "G26", "bus_type": "CAN",
                       "extended_diag_address": "0x55"}
        )
        self.assertEqual(requests, [
            b"\x22\xF1\x87", b"\x10\x03", b"\x28\x01\x03",
            b"\x85\x02", b"\x10\x02", b"\x22\xF1\x87",
        ])

    def test_bundle_round_trip(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            payload = root / "block.bin"
            payload.write_bytes(b"abc")
            targets = [
                {"node_name": "ECU1", "bus_type": "CAN", "is_zgw": False, "payload_blocks": []}
            ]
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
        coding_non_zgw_last = max(
            e.time_slot for e in events if e.phase == "coding" and not e.is_zgw
        )
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
        data[base + 10 : base + 14] = (7).to_bytes(4, "big")
        data[base + 14] = 0
        data[base + 15] = 1
        data[base + 16] = 0
        data[base + 17] = 1
        data[base + 18] = 0
        data[base + 19 : base + 21] = (12).to_bytes(2, "big")
        data[base + 21 : base + 23] = (4).to_bytes(2, "big")
        data[base + 23 : base + 25] = (20).to_bytes(2, "big")
        data[base + 25 : base + 27] = (5).to_bytes(2, "big")
        data[base + 27] = 1
        detail = app._explain_dtc_detail(
            fcd_app_module.DEM_DTC_CANFD_PROTOCOL_ERROR,
            0x04,
            bytes(data),
        )
        self.assertIn("ZGW_CANFD_2 Excessive Protocol Error", detail)
        self.assertIn(
            "ZGW_CANFD_2 Excessive Protocol Error",
            app._describe_zgw_dtc(fcd_app_module.DEM_DTC_CANFD_PROTOCOL_ERROR),
        )
        self.assertIn("bus-off count=7", detail)
        self.assertIn("operational=no", detail)
        self.assertIn("normal TX enabled=no", detail)
        self.assertIn("error-passive debounce fail/pass=12/4", detail)
        self.assertIn("protocol-error debounce fail/pass=20/5", detail)
        data[base + 2] = 1
        response = b"\x59\x04\x02\x21\x05\x2f\xff" + bytes(data)
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
            (
                fcd_app_module.DEM_DTC_CAN_CLASSIC_CONTROLLER_FAULT,
                0,
                2,
                "ZGW_CAN_3 Controller Fault",
            ),
            (
                fcd_app_module.DEM_DTC_CAN_CLASSIC_PROTOCOL_ERROR,
                0,
                3,
                "ZGW_CAN_3 Excessive Protocol Error",
            ),
            (fcd_app_module.DEM_DTC_CANFD_BUS_OFF, 1, 0, "ZGW_CANFD_2 Bus-Off"),
            (fcd_app_module.DEM_DTC_CANFD_ERROR_PASSIVE, 1, 1, "ZGW_CANFD_2 Error Passive"),
            (fcd_app_module.DEM_DTC_CANFD_CONTROLLER_FAULT, 1, 2, "ZGW_CANFD_2 Controller Fault"),
            (
                fcd_app_module.DEM_DTC_CANFD_PROTOCOL_ERROR,
                1,
                3,
                "ZGW_CANFD_2 Excessive Protocol Error",
            ),
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
                data[base + 10 : base + 14] = (1 if fault == 0 else 0).to_bytes(4, "big")
                response = b"\x59\x04" + dtc.to_bytes(3, "big") + b"\x2f\xff" + bytes(data)
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
        response = b"\x59\x04\x02\x21\x05\x2f\xff" + data
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
