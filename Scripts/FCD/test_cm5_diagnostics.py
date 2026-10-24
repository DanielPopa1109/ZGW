import importlib.util
import json
import struct
import sys
import tempfile
import unittest
from unittest import mock
import copy
from pathlib import Path

FCD_PATH = Path(__file__).with_name("FCD.pyw")
SPEC = importlib.util.spec_from_file_location("fcd_cm5_diagnostics", FCD_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class Cm5DiagnosticsTests(unittest.TestCase):
    def test_expanded_cm5_catalog_preserves_legacy_names(self):
        names = MODULE.Cm5DiagnosticManagerPanel.DTC_NAMES
        self.assertEqual(names[0xC10604], "I4G26 Health message timeout")
        self.assertEqual(names[0xC20001], "CPU sustained high temperature")

    def test_canonical_history_survives_an_empty_current_journal(self):
        panel = object.__new__(MODULE.Cm5DiagnosticManagerPanel)
        loaded = []
        panel.app = mock.Mock()
        panel.app.root.after.side_effect = lambda _delay, callback, value: callback(value)
        panel._load_dtc_rows = loaded.extend
        panel._read_active_dtc_rows = lambda: []
        panel._run = lambda _title, operation, **_kwargs: operation()
        record = {"dtc": "C20001", "status": 40, "state": "PASSED", "name": "CPU", "occurrences": 2}
        panel._read_did_text = lambda did: json.dumps([record]) if did == 0xFCD4 else ""
        panel.read_fault_memory()
        self.assertEqual(loaded[0][0:4], ("CM5", "0xC20001", "PASSED (0x28)", "CPU sustained high temperature"))

    def test_older_manager_falls_back_to_legacy_history(self):
        panel = object.__new__(MODULE.Cm5DiagnosticManagerPanel)
        loaded = []
        panel.app = mock.Mock()
        panel.app.root.after.side_effect = lambda _delay, callback, value: callback(value)
        panel._load_dtc_rows = loaded.extend
        panel._read_active_dtc_rows = lambda: []
        panel._run = lambda _title, operation, **_kwargs: operation()
        def read(did):
            if did == 0xFCD4:
                raise MODULE.DoipError("UDS negative response 7F2231")
            return json.dumps({"dtc": "C10604", "status": 40, "state": "PASSED"})
        panel._read_did_text = read
        panel.read_fault_memory()
        self.assertEqual(loaded[0][1], "0xC10604")

    @staticmethod
    def versions_payload():
        names = (
            "cm5-miracast-native", "cm5-ethernet-agent", "cm5-health-monitor",
            "cm5-health-report", "cm5-navigation-demo", "diagnostic-manager",
            "gpslogger", "sota-handler", "cm5-backup-filesystem-backend",
            "cm5-system-update-helper", "cm5-uplink-policy", "cm5_ab_image_builder",
            "cm5_backup_agent",
        )
        versions = {"cm5-ethernet-agent": "1.5.0", "cm5-health-monitor": "1.0.0",
                    "cm5_backup_agent": "2.2.1"}
        return {
            "agent_version": "1.5.0",
            "applications": [
                {"name": name, "version": versions.get(name, "unknown"),
                 "path": "/hidden/path", "sha256": "hidden-hash", "pids": [999],
                 "running": True, "source_provenance": "probable",
                 "associated_services": ["hidden.service"]}
                for name in names
            ],
            "custom_system_services": [{"name": "hidden.service"}],
            "systemd_units": [{"name": "hidden.timer"}],
            "inventory_issues": ["hidden diagnostic"],
        }

    def test_versions_show_only_all_application_names_and_versions(self):
        payload = self.versions_payload()
        original = copy.deepcopy(payload)
        expected = "\n".join(f"{a['name']}: {a['version']}" for a in payload['applications'])
        self.assertEqual(MODULE.cm5_format_response("versions", payload), expected)
        self.assertEqual(len(expected.splitlines()), 13)
        self.assertEqual(payload, original)

    def test_versions_support_older_agents_and_unknown_versions(self):
        payload = {"agent_version": "1.4.2", "applications": [
            {"name": "CM5 Ethernet Agent", "version": "1.4.2", "available": True},
            {"name": "CM5 Health Monitor", "version": " 1.0.0 "},
            {"name": "GPS Logger", "version": None},
            {"name": "FCD Backup Agent", "version": ""},
        ]}
        self.assertEqual(MODULE.cm5_format_response("versions", payload),
            "CM5 Ethernet Agent: 1.4.2\nCM5 Health Monitor: 1.0.0\n"
            "GPS Logger: unknown\nFCD Backup Agent: unknown")
        self.assertEqual(MODULE.cm5_format_response("versions", {
            "applications": [{"name": "No version"}, {"name": "Blank", "version": "  "}]}),
            "No version: unknown\nBlank: unknown")

    def test_versions_button_uses_minimal_formatter(self):
        try:
            root = MODULE.tk.Tk()
        except MODULE.tk.TclError as exc:
            self.skipTest(f"Tk display unavailable: {exc}")
        root.withdraw()
        try:
            with mock.patch.object(MODULE, "load_or_migrate_cm5_token", return_value=""):
                app = MODULE.FcdApp(root)
            logs = []
            app.log = logs.append
            payload = self.versions_payload()
            client = mock.Mock(connected=True)
            client.request.return_value = payload
            app.cm5_panel.client = client
            app.cm5_panel._run = lambda action, work: app.cm5_panel._show(action, work())
            def descendants(widget):
                for child in widget.winfo_children():
                    yield child
                    yield from descendants(child)
            button = next(w for w in descendants(app.cm5_panel.parent)
                          if isinstance(w, MODULE.ttk.Button) and w.cget("text") == "Versions")
            button.invoke()
            client.request.assert_called_once_with(MODULE.CM5_COMMANDS['versions'], timeout=None)
            self.assertEqual(logs, [MODULE.cm5_format_response("versions", payload)])
            self.assertEqual(len(logs[0].splitlines()), 13)
        finally:
            root.destroy()

    def test_diagnostic_manager_uses_shared_trace_without_private_output_box(self):
        source = FCD_PATH.read_text(encoding="utf-8")
        self.assertNotIn('text="DiagnosticManager information and logs"', source)
        self.assertIn('self.app.log(f"CM5 Diagnostics: {title} completed\\n{value}")', source)

    def test_fault_memory_keeps_latest_non_active_history_without_trace_output(self):
        panel = object.__new__(MODULE.Cm5DiagnosticManagerPanel)
        calls = []
        panel._run = lambda title, operation, trace_result=True: calls.append(
            (title, operation, trace_result)
        )
        panel.read_fault_memory()
        self.assertEqual([call[2] for call in calls], [False])

        class Root:
            @staticmethod
            def after(_delay, callback, value):
                callback(value)

        class App:
            root = Root()

        loaded = []
        panel.app = App()
        panel._load_dtc_rows = lambda rows: loaded.extend(rows)
        panel._read_active_dtc_rows = lambda: [
            ("CM5", "0xC10202", "ACTIVE (0x2F)", "ELC VoltageCurrent message timeout", "live")
        ]
        records = [
            {
                "time": "2026-10-16T21:58:30+0300",
                "state": "FAILED",
                "dtc": "C10604",
                "status": 47,
                "source": 6,
                "name": "I4G26 Health",
                "snapshot": {
                    "systemDateTime": "2026-10-16T21:58:30+0300",
                    "cpuLoadPercent": 3.0,
                    "ramUsedBytes": 1073741824,
                    "ramTotalBytes": 4294967296,
                    "cpuTemperatureC": 49.6,
                },
            },
            {
                "time": "2026-10-16T21:59:30+0300",
                "state": "PASSED",
                "dtc": "C10604",
                "status": 40,
                "source": 6,
                "name": "I4G26 Health",
                "snapshot": {
                    "systemDateTime": "2026-10-16T21:59:30+0300",
                    "cpuLoadPercent": 2.0,
                    "ramUsedBytes": 1073741824,
                    "ramTotalBytes": 4294967296,
                    "cpuTemperatureC": 48.0,
                },
            },
            {
                "time": "2026-10-16T22:00:30+0300",
                "state": "FAILED",
                "dtc": "C10202",
                "status": 47,
                "source": 2,
                "name": "ELC VoltageCurrent",
                "snapshot": {},
            },
        ]
        panel._read_did_text = lambda _did: "\n".join(json.dumps(record) for record in records)
        calls[0][1]()
        self.assertEqual(loaded[0][2], "ACTIVE (0x2F)")
        self.assertEqual(
            loaded[1][0:4], ("CM5", "0xC10604", "PASSED (0x28)", "I4G26 Health message timeout")
        )
        self.assertNotIn("RAM total", loaded[1][4])
        self.assertEqual(len(loaded), 2)

    def test_human_readable_connect_response(self):
        text = MODULE.cm5_format_response("connect", {"pong": True})
        self.assertIn("CM5 secure connection", text)
        self.assertIn("Transport: TLS 1.3", text)
        self.assertIn("Authentication: Successful", text)
        self.assertNotIn('"pong"', text)

    def test_human_readable_file_response_has_units_and_paths(self):
        text = MODULE.cm5_format_response(
            "inventory_report",
            {
                "source": "/media/cm5healthusb/CM5_HEALTH_LOG/INVENTORY/report.txt",
                "saved_to": "C:/CM5_Reads/Inventory/report.txt",
                "size_bytes": 258704,
            },
        )
        self.assertIn("CM5 inventory retrieval", text)
        self.assertIn("Source on CM5:", text)
        self.assertIn("Saved on Windows:", text)
        self.assertIn("252.64 KiB (258,704 bytes)", text)

    def test_human_readable_nested_status(self):
        text = MODULE.cm5_format_response(
            "system_status",
            {
                "uptime_seconds": 90061,
                "agent": {"health_monitor_available": True, "socket_errors": 0},
            },
        )
        self.assertIn("Uptime: 1d 01h 01m 01s", text)
        self.assertIn("Health Monitor available: Yes", text)
        self.assertIn("Socket errors: 0", text)

    def test_empty_success_fields_are_not_rendered(self):
        text = MODULE.cm5_format_response(
            "check_updates",
            {"error": "", "output": "0 upgraded, 0 newly installed.\nEXIT_CODE=0\n"},
        )
        self.assertNotIn("Error:", text)
        self.assertIn("0 upgraded, 0 newly installed.", text)

    def test_report_sync_replaces_same_name_without_duplicate(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            first = {"path": "/usb/trace.nmea", "size_bytes": 3, "content_base64": "YWJj"}
            second = {"path": "/usb/trace.nmea", "size_bytes": 4, "content_base64": "ZGVmZw=="}
            destination, existed, size = MODULE.cm5_save_report(first, folder)
            self.assertFalse(existed)
            self.assertEqual(size, 3)
            destination_again, existed, size = MODULE.cm5_save_report(second, folder)
            self.assertTrue(existed)
            self.assertEqual(destination_again, destination)
            self.assertEqual(size, 4)
            self.assertEqual(destination.read_bytes(), b"defg")
            self.assertEqual([path.name for path in folder.iterdir()], ["trace.nmea"])

    def test_authentication_command_is_reserved_zero(self):
        self.assertEqual(MODULE.CM5_COMMANDS["authenticate"], 0)

    def test_package_update_commands_are_allowlisted(self):
        self.assertEqual(MODULE.CM5_COMMANDS["check_updates"], 14)
        self.assertEqual(MODULE.CM5_COMMANDS["install_updates"], 15)
        self.assertEqual(MODULE.cm5_offline_nrc("install_updates"), bytes.fromhex("7f bc 22"))

    def test_cm5_settings_do_not_persist_token(self):
        panel = object.__new__(MODULE.Cm5Panel)

        class Variable:
            def __init__(self, value):
                self.value = value

            def get(self):
                return self.value

        panel.host_var = Variable("cm5")
        panel.port_var = Variable("30701")
        panel.token_var = Variable("must-not-be-saved")
        panel.fingerprint_var = Variable("AA" * 32)
        panel.nmea_path_var = Variable("/nmea")
        panel.health_path_var = Variable("/health")
        panel.inventory_path_var = Variable("/inventory")
        panel.output_path_var = Variable("C:/output")
        saved = panel.settings()
        self.assertNotIn("token", saved)
        self.assertEqual(saved["fingerprint"], "AA" * 32)

    def test_disconnected_read_returns_conditions_not_correct_nrc(self):
        self.assertEqual(MODULE.cm5_offline_nrc("versions"), bytes.fromhex("7f b0 22"))
        self.assertEqual(MODULE.cm5_offline_nrc("inventory_report"), bytes.fromhex("7f b4 22"))

    def test_protocol_request_and_json_response(self):
        command = MODULE.CM5_COMMANDS["nmea_trace"]
        body = (
            struct.pack(">HH", command, 0)
            + json.dumps({"path": "trace.nmea", "size_bytes": 4}).encode()
        )
        response = MODULE.CM5E_HEADER.pack(b"CM5E", 1, 8, 0, len(body), 1, 0, 0x434D3501, 1) + body

        class FakeSocket:
            def __init__(self, incoming):
                self.incoming = bytearray(incoming)
                self.sent = bytearray()

            def sendall(self, data):
                self.sent.extend(data)

            def recv(self, size):
                data = self.incoming[:size]
                del self.incoming[:size]
                return bytes(data)

            def shutdown(self, _how):
                pass

            def close(self):
                pass

        fake = FakeSocket(response)
        client = MODULE.Cm5DiagnosticsClient("unused")
        client.sock = fake
        result = client.request(command, "/media/cm5healthusb/GPS_NMEA_LOG")
        client.disconnect()
        sent_header = MODULE.CM5E_HEADER.unpack(fake.sent[:32])
        sent_command, reserved = struct.unpack(">HH", fake.sent[32:36])
        self.assertEqual((sent_command, reserved), (command, 0))
        self.assertEqual(fake.sent[36:].decode(), "/media/cm5healthusb/GPS_NMEA_LOG")
        self.assertEqual(sent_header[8], 1)
        self.assertEqual(result["path"], "trace.nmea")
        self.assertEqual(result["size_bytes"], 4)

    def test_diagnostic_manager_binary_snapshot_is_human_readable(self):
        panel = object.__new__(MODULE.Cm5DiagnosticManagerPanel)
        timestamp = b"2026-10-16T21:44:20+0300"
        response = (
            bytes.fromhex("59 04 C1 02 02 2F 01 04 F2 00")
            + timestamp
            + bytes.fromhex("F2 01 04 D2")
            + bytes.fromhex("F2 02 00 00 00 00 40 00 00 00")
            + bytes.fromhex("F2 04 0F A0")
        )
        panel._uds = lambda request: response
        text = panel._read_dtc_snapshot(0xC10202)
        self.assertIn("System date/time: 2026-10-16T21:44:20+0300", text)
        self.assertIn("CPU load: 12.34%", text)
        self.assertIn("RAM used: 1.00 GiB", text)
        self.assertNotIn("RAM total", text)
        self.assertIn("CPU temperature: 40.00 C", text)
        self.assertEqual(panel.DTC_NAMES[0xC10202], "ELC VoltageCurrent message timeout")

    def test_diagnostic_manager_is_the_reporting_node_for_active_dtcs(self):
        panel = object.__new__(MODULE.Cm5DiagnosticManagerPanel)
        responses = iter(
            [bytes.fromhex("59 01 2F 01 00 01"), bytes.fromhex("59 02 2F C1 06 04 2F")]
        )
        panel._uds = lambda _request: next(responses)
        panel._read_dtc_snapshot = lambda _dtc: "snapshot"
        rows = panel._read_active_dtc_rows()
        self.assertEqual(rows[0][0], "CM5")
        self.assertEqual(rows[0][3], "I4G26 Health message timeout")

    def test_diagnostic_manager_clear_dtcs_has_no_confirmation(self):
        panel = object.__new__(MODULE.Cm5DiagnosticManagerPanel)
        called = []
        operations = []
        panel._run = lambda title, operation: (called.append(title), operations.append(operation))
        original = MODULE.messagebox.askyesno
        MODULE.messagebox.askyesno = lambda *args: self.fail(
            "Clear DTCs must not ask for confirmation"
        )
        try:
            panel.clear_dtcs()
        finally:
            MODULE.messagebox.askyesno = original
        self.assertEqual(called, ["Clear DTCs"])

        class Root:
            @staticmethod
            def after(_delay, callback, value):
                callback(value)

        class App:
            root = Root()

        requests = []
        loaded = []
        panel.app = App()
        panel._uds = lambda request: requests.append(request) or b"\x54"
        panel._load_dtc_rows = lambda rows: loaded.append(rows)
        self.assertEqual(
            operations[0](),
            "All active DiagnosticManager DTCs and FaultMemory history were cleared.",
        )
        self.assertEqual(requests, [b"\x14\xff\xff\xff"])
        self.assertEqual(loaded, [[]])


if __name__ == "__main__":
    unittest.main()
