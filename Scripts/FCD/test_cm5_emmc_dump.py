import hashlib
import importlib.util
import json
import shutil
import sys
import tempfile
import threading
import unittest
from pathlib import Path
from unittest import mock


FCD_PATH = Path(__file__).resolve().parent / "FCD.pyw"
SPEC = importlib.util.spec_from_file_location("fcd_emmc_dump_module", FCD_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class Variable:
    def __init__(self, value=""):
        self.value = value

    def get(self):
        return self.value

    def set(self, value):
        self.value = value


class Progress:
    def __init__(self):
        self.value = 0

    def configure(self, **values):
        self.value = values.get("value", self.value)


class FakeClient:
    def __init__(self, payloads, bad_hash=False, cancel=False):
        self.payloads = payloads
        self.bad_hash = bad_hash
        self.cancel = cancel
        self.mode = None
        self.transport_evidence = {"test": "synthetic transport"}

    def initialize(self, mode, bandwidth_mbps=50):
        self.mode = mode
        return {
            "size": len(self.payloads[mode]),
            "sector_size": 512,
            "bandwidth_mbps": bandwidth_mbps,
        }

    def stream(self, output, offset, cancel_event, progress):
        data = self.payloads[self.mode]
        output.write(data[: len(data) // 2])
        progress(len(data) // 2, len(data), 0.01)
        if self.cancel:
            raise MODULE.BackupCancelled("synthetic cancellation")
        output.write(data[len(data) // 2 :])
        progress(len(data), len(data), 0.02)
        digest = hashlib.sha256(data).hexdigest()
        return {"size": len(data), "sha256": "0" * 64 if self.bad_hash else digest}


class EmmcDumpTests(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp(prefix="emmc-dump-", dir=Path(__file__).parent))
        user = bytearray(4096)
        user[510:512] = b"\x55\xaa"
        self.payloads = {"raw": bytes(user), "raw_boot0": b"boot-zero", "raw_boot1": b"boot-one"}

    def tearDown(self):
        shutil.rmtree(self.root)

    def panel(self, client):
        panel = object.__new__(MODULE.Cm5BackupPanel)
        panel.client = client
        panel.cancel_event = threading.Event()
        panel.destination_var = Variable(str(self.root))
        panel.status_var = Variable()
        panel.metrics_var = Variable()
        panel.detail_var = Variable()
        panel.hash_var = Variable()
        panel.bandwidth_var = Variable("50 Mbps")
        panel.progress = Progress()
        panel.info = {
            "capacity": len(self.payloads["raw"]),
            "device": "/dev/mmcblk0",
            "agent_version": "2.1.0",
            "protocol_version": 1,
            "partitions": [],
            "mounts": [],
            "supplementary_devices": [
                {"name": "mmcblk0boot0", "size": len(self.payloads["raw_boot0"]), "readable": True},
                {"name": "mmcblk0boot1", "size": len(self.payloads["raw_boot1"]), "readable": True},
            ],
        }
        panel._set = lambda variable, value: variable.set(value)
        panel._ui = lambda function: function()
        panel.log = lambda _message: None
        return panel

    @mock.patch.object(MODULE, "supports_large_files", return_value=True)
    @mock.patch.object(MODULE, "available_bytes", return_value=10**9)
    def test_complete_dump_verifies_all_components(self, _free, _large):
        panel = self.panel(FakeClient(self.payloads))
        panel._backup()
        metadata_path = next(self.root.glob("CM5_eMMC_*.json"))
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        self.assertEqual(metadata["completion_status"], "complete")
        self.assertEqual([item["name"] for item in metadata["components"]], ["user", "boot0", "boot1"])
        self.assertEqual(len(list(self.root.glob("*.img"))), 3)
        self.assertFalse(list(self.root.glob("*.partial")))
        self.assertEqual(panel.progress.value, 100)

    @mock.patch.object(MODULE, "supports_large_files", return_value=True)
    @mock.patch.object(MODULE, "available_bytes", return_value=10**9)
    def test_hash_mismatch_never_promotes_partial(self, _free, _large):
        panel = self.panel(FakeClient(self.payloads, bad_hash=True))
        with self.assertRaises(MODULE.BackupError):
            panel._backup()
        metadata = json.loads(next(self.root.glob("*.json")).read_text(encoding="utf-8"))
        self.assertEqual(metadata["completion_status"], "incomplete")
        self.assertTrue(list(self.root.glob("*.img.partial")))
        self.assertFalse(list(self.root.glob("*.img")))

    @mock.patch.object(MODULE, "supports_large_files", return_value=True)
    @mock.patch.object(MODULE, "available_bytes", return_value=10**9)
    def test_ethernet_loss_retains_partial_and_incomplete_metadata(self, _free, _large):
        client = FakeClient(self.payloads)

        def interrupted(output, *_args):
            output.write(self.payloads["raw"][:512])
            raise MODULE.BackupError("Ethernet-only: synthetic link lost")

        client.stream = interrupted
        with self.assertRaisesRegex(MODULE.BackupError, "synthetic link lost"):
            self.panel(client)._backup()
        metadata = json.loads(next(self.root.glob("*.json")).read_text(encoding="utf-8"))
        self.assertEqual(metadata["completion_status"], "incomplete")
        self.assertIn("synthetic link lost", metadata["errors"][0])
        self.assertEqual(next(self.root.glob("*.partial")).stat().st_size, 512)
        self.assertFalse(list(self.root.glob("*.img")))

    def test_settings_never_include_authentication_token(self):
        panel = object.__new__(MODULE.Cm5BackupPanel)
        panel.host_var = Variable("raspberrypi")
        panel.port_var = Variable("18500")
        panel.fingerprint_var = Variable("ab" * 32)
        panel.destination_var = Variable(str(self.root))
        panel.token_var = Variable("must-not-persist")
        panel.remember_token_var = Variable(False)
        panel.bandwidth_var = Variable("50 Mbps")
        with mock.patch.object(MODULE, "delete_cm5_token"), mock.patch.object(
            MODULE, "SCRIPT_DIR", self.root
        ):
            self.assertNotIn("token", panel.settings())

    def test_bandwidth_selector_values_and_persistence(self):
        panel = object.__new__(MODULE.Cm5BackupPanel)
        panel.bandwidth_var = Variable("Unlimited")
        self.assertEqual(panel.bandwidth_mbps(), 0)
        panel.bandwidth_var.set("75 Mbps")
        self.assertEqual(panel.bandwidth_mbps(), 75)

    def test_plaintext_lab_token_is_loaded_and_saved(self):
        token = "b2" * 32
        with mock.patch.object(MODULE, "SCRIPT_DIR", self.root):
            (self.root / "token.txt").write_text(token + "\n", encoding="utf-8")
            self.assertEqual(MODULE.load_or_migrate_cm5_token(), token)
            panel = self.panel(FakeClient(self.payloads))
            panel.token_var = Variable("c3" * 32)
            panel.remember_token_var = Variable(True)
            with mock.patch.object(MODULE, "save_cm5_token"):
                panel.persist_token()
            self.assertEqual((self.root / "token.txt").read_text(encoding="utf-8").strip(), "c3" * 32)

    def test_token_round_trip_uses_dpapi_not_plaintext(self):
        credential = self.root / "token.dpapi"
        token = "a1" * 32
        with mock.patch.dict("os.environ", {"FCD_CM5_TOKEN_CREDENTIAL_PATH": str(credential)}):
            MODULE.save_cm5_token(token)
            self.assertNotEqual(credential.read_bytes(), token.encode("utf-8"))
            self.assertEqual(MODULE.load_cm5_token(), token)
            MODULE.delete_cm5_token()
            self.assertFalse(credential.exists())


if __name__ == "__main__":
    unittest.main()
