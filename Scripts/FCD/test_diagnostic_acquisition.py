import importlib.machinery
import importlib.util
import pathlib
import sys
import unittest


MODULE_PATH = pathlib.Path(__file__).with_name("FCD.pyw")
sys.path.insert(0, str(MODULE_PATH.parent))
LOADER = importlib.machinery.SourceFileLoader("fcd_pyw", str(MODULE_PATH))
SPEC = importlib.util.spec_from_loader(LOADER.name, LOADER)
fcd = importlib.util.module_from_spec(SPEC)
LOADER.exec_module(fcd)


class _Var:
    def get(self):
        return "0.1"


class _Harness:
    timeout_var = _Var()

    def __init__(self):
        self.messages = []

    def _target_label(self, target):
        return target.get("node_name", "ZGW")

    def _target_is_simulated(self, target):
        return bool(target.get("simulated", False))

    def log(self, message):
        self.messages.append(message)


class AcquisitionTests(unittest.TestCase):
    def acquire(self, send, decoder=lambda response: "decoded", target=None):
        target = target or {"node_name": "ZGW"}
        return fcd.FcdApp._acquire_uds_value(
            self.harness,
            send,
            target,
            "item_key",
            "Item",
            "RoutineControl (0x31)",
            "Routine 0xF192",
            b"\x31\x03\xF1\x92",
            decoder,
            timeout=0.1,
        )

    def setUp(self):
        self.harness = _Harness()
        for name in [
            "_make_acquisition_result",
            "_acq_log",
            "_finalize_acquisition_duration",
            "_classify_acquisition_exception",
            "_report_unavailable_lines",
            "_report_minor",
            "_report_field",
            "_report_bool",
            "_decode_eth_startup_timing_chunk",
        ]:
            setattr(self.harness, name, getattr(fcd.FcdApp, name).__get__(self.harness, _Harness))

    def test_positive_response_is_persisted(self):
        def send(_request, _name, timeout=None, allow_no_response=False):
            send.last_nrc78_count = 0
            return b"\x71\x03\xF1\x92"

        result = self.acquire(send)

        self.assertEqual(result.state, "SUCCESS")
        self.assertTrue(result.attempted)
        self.assertTrue(result.request_sent)
        self.assertEqual(result.response, b"\x71\x03\xF1\x92")

    def test_negative_response_is_terminal(self):
        def send(_request, _name, timeout=None, allow_no_response=False):
            raise fcd.NegativeResponse(0x31, 0x31)

        result = self.acquire(send)

        self.assertEqual(result.state, "FEATURE_NOT_SUPPORTED")
        self.assertTrue(result.negative_response)
        self.assertEqual(result.nrc, 0x31)

    def test_response_pending_count_is_retained(self):
        def send(_request, _name, timeout=None, allow_no_response=False):
            send.last_nrc78_count = 3
            return b"\x71\x03\xF1\x92"

        result = self.acquire(send)

        self.assertEqual(result.state, "SUCCESS")
        self.assertEqual(result.response_pending_count, 3)

    def test_timeout_is_terminal(self):
        def send(_request, _name, timeout=None, allow_no_response=False):
            raise TimeoutError("timed out")

        result = self.acquire(send)

        self.assertEqual(result.state, "FAILED_TIMEOUT")
        self.assertTrue(result.timed_out)
        self.assertIn("timed out", result.error_text)

    def test_parser_failure_is_terminal(self):
        def send(_request, _name, timeout=None, allow_no_response=False):
            return b"\x71\x03\xF1\x92"

        result = self.acquire(send, decoder=lambda _response: (_ for _ in ()).throw(fcd.FcdError("short payload")))

        self.assertEqual(result.state, "FAILED_RESPONSE_LENGTH")
        self.assertTrue(result.invalid_response)

    def test_report_regression_phrase_is_absent(self):
        lines = fcd.FcdApp._report_unavailable_lines(
            self.harness,
            "Ethernet Startup Timing",
            "RoutineControl 0x31 Routine 0xF192",
        )

        self.assertNotIn("parser-failure record", "\n".join(lines))

    def test_eth_startup_timing_accepts_captured_little_endian_magic(self):
        response = bytes.fromhex(
            "71 03 F1 92 "
            "54 48 54 45 "  # 0x45544854 stored little-endian/native
            "01 00 "        # version = 1
            "1E 00 "        # event capacity = 30
            "00 E1 F5 05 00 00 00 00 "  # STM frequency = 100000000 Hz
            "88 77 66 55 44 33 22 11 "  # reference ticks
            "00 01 02 03 "              # start, returned, flags, missed locks
            "00 01 01 00 "              # event id, valid byte, raw valid uint16
            "99 88 77 66 55 44 33 22 "  # timestamp ticks
            "40 42 0F 00"               # elapsed = 1000000 us
        )

        timing = fcd.FcdApp._decode_eth_startup_timing_chunk(self.harness, response)

        self.assertEqual(timing["magic"], 0x45544854)
        self.assertEqual(timing["byte_order"], "little")
        self.assertEqual(timing["version"], 1)
        self.assertEqual(timing["total_events"], 30)
        self.assertEqual(timing["stm_hz"], 100_000_000)
        self.assertEqual(timing["returned_count"], 1)
        self.assertEqual(timing["flags"], 2)
        self.assertEqual(timing["missed_locks"], 3)
        self.assertEqual(timing["entries"][0]["raw_valid"], 1)
        self.assertEqual(timing["entries"][0]["elapsed_us"], 1_000_000)

    def test_parser_error_can_retain_positive_response(self):
        response = bytes.fromhex("71 03 F1 92 00 00 00 00 00 00 00 00")
        exc = fcd.DiagnosticParserError("positive response received, but payload parsing failed", response)
        result = fcd.DiagnosticAcquisitionResult(
            key="eth_startup_timing",
            item="Ethernet Startup Timing",
            service="RoutineControl (0x31)",
            identifier="Routine 0xF192",
            request=bytes.fromhex("31 03 F1 92 00 0E"),
        )

        self.harness._classify_acquisition_exception(result, exc)

        self.assertEqual(result.state, "FAILED_PARSER")
        self.assertTrue(result.positive_response)
        self.assertTrue(result.parser_failure)
        self.assertEqual(result.response, response)


if __name__ == "__main__":
    unittest.main()
