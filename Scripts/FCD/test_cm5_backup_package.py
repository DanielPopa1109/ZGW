import importlib.util
import base64
import json
import sys
import tempfile
import shutil
import unittest
import zipfile
from pathlib import Path
from unittest import mock

FCD_PATH = Path(__file__).resolve().parent / "FCD.pyw"
FCD_SPEC = importlib.util.spec_from_file_location("fcd_backup_module", FCD_PATH)
fcd_backup_module = importlib.util.module_from_spec(FCD_SPEC)
sys.modules[FCD_SPEC.name] = fcd_backup_module
FCD_SPEC.loader.exec_module(fcd_backup_module)

BackupPackageWriter = fcd_backup_module.BackupPackageWriter
MANIFEST_NAME = fcd_backup_module.MANIFEST_NAME
PackageError = fcd_backup_module.PackageError
read_manifest = fcd_backup_module.read_manifest
validate_restore_plan = fcd_backup_module.validate_restore_plan
verify_package = fcd_backup_module.verify_package


class BackupPackageTests(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp(prefix="cm5-test-", dir=Path(__file__).parent))
        self.package = self.root / "test.fcdbackup"
        self.target = self.root / "target.bin"
        self.target.write_bytes(b"\0" * 8192)

    def tearDown(self):
        shutil.rmtree(self.root)

    def make_package(self):
        manifest = {
            "backup_mode": "filesystem-aware",
            "disk": {"size_bytes": 4096, "pttype": "gpt", "logical_sector_size": 512},
            "partitions": [
                {"number": 1, "filesystem": "vfat", "uuid": "BOOT"},
                {"number": 2, "filesystem": "ext4", "uuid": "ROOT"},
            ],
            "consistency_classification": "offline-unmounted",
        }
        writer = BackupPackageWriter(self.package, manifest)
        with writer.entry(
            "partitions/01-vfat.pcl.zst",
            role="partition-image",
            partition=1,
            backend="partclone.vfat",
            compression="zstd",
        ) as out:
            out.write(b"fat-image")
        with writer.entry(
            "partitions/02-ext4.pcl.zst",
            role="partition-image",
            partition=2,
            backend="partclone.ext4",
            compression="zstd",
        ) as out:
            out.write(b"ext4-image")
        writer.complete()

    def test_ab_builder_terminal_uses_visible_interactive_ssh(self):
        panel = object.__new__(fcd_backup_module.Cm5BackupPanel)
        statuses = []
        logs = []
        panel._set = lambda _variable, value: statuses.append(value)
        panel.log = logs.append
        values = {
            "package": "",
            "ssh_host": "danielpopa1109@192.168.1.157",
            "remote_workspace": "/mnt/fcd-build",
            "slot": "b",
            "version": "2026.10.13",
        }
        with mock.patch.object(fcd_backup_module.subprocess, "Popen") as popen:
            panel._launch_ab_builder_terminal(values, True, object())

        command = popen.call_args.args[0]
        self.assertEqual(command[0], "powershell.exe")
        self.assertIn("-NoExit", command)
        encoded = command[command.index("-EncodedCommand") + 1]
        script = base64.b64decode(encoded).decode("utf-16-le")
        self.assertIn("ssh.exe -t", script)
        self.assertIn("danielpopa1109@192.168.1.157", script)
        self.assertIn("sudo /usr/local/sbin/cm5_ab_image_builder", script)
        self.assertIn("--preflight", script)
        self.assertTrue(statuses)
        self.assertTrue(logs)

    def test_workspace_mount_terminal_installs_cifs_and_prompts_interactively(self):
        panel = object.__new__(fcd_backup_module.Cm5BackupPanel)
        statuses = []
        logs = []
        panel._set = lambda _variable, value: statuses.append(value)
        panel.log = logs.append
        values = {
            "ssh_host": "danielpopa1109@192.168.1.157",
            "remote_workspace": "/mnt/fcd-build",
            "windows_share": "//192.168.1.142/FCD_BUILD",
            "windows_user": "Daniel",
        }
        with mock.patch.object(fcd_backup_module.subprocess, "Popen") as popen:
            panel._launch_mount_workspace_terminal(values, object())

        command = popen.call_args.args[0]
        self.assertEqual(command[0], "powershell.exe")
        self.assertIn("-NoExit", command)
        encoded = command[command.index("-EncodedCommand") + 1]
        script = base64.b64decode(encoded).decode("utf-16-le")
        self.assertIn("ssh.exe -t", script)
        self.assertIn("apt-get install -y cifs-utils", script)
        self.assertIn("mount -t cifs", script)
        self.assertIn("//192.168.1.142/FCD_BUILD", script)
        self.assertIn("username=Daniel", script)
        self.assertNotIn("password=", script.lower())
        self.assertTrue(statuses)
        self.assertTrue(logs)

    def test_fat_ext4_package_and_dry_run(self):
        self.make_package()
        manifest = verify_package(self.package)
        plan = validate_restore_plan(manifest, self.target)
        self.assertTrue(plan["dry_run"])
        self.assertEqual(plan["partition_count"], 2)

    def test_incomplete_package_rejected(self):
        with zipfile.ZipFile(self.package, "w") as archive:
            archive.writestr("data", b"x")
        with self.assertRaises(PackageError):
            read_manifest(self.package)

    def test_corrupted_member_detected(self):
        self.make_package()
        with zipfile.ZipFile(self.package, "a") as archive:
            archive.writestr("partitions/01-vfat.pcl.zst", b"corrupt")
        with self.assertRaises(PackageError):
            verify_package(self.package)

    def test_mounted_and_small_targets_rejected(self):
        self.make_package()
        manifest = read_manifest(self.package)
        with self.assertRaisesRegex(PackageError, "mounted"):
            validate_restore_plan(manifest, self.target, mounted_devices=[self.target])
        self.target.write_bytes(b"x")
        with self.assertRaisesRegex(PackageError, "too small"):
            validate_restore_plan(manifest, self.target)

    def test_missing_partition_image_rejected(self):
        self.make_package()
        manifest = read_manifest(self.package)
        manifest["entries"] = manifest["entries"][:1]
        with self.assertRaisesRegex(PackageError, "missing partition"):
            validate_restore_plan(manifest, self.target)

    def test_mbr_metadata_supported(self):
        self.make_package()
        manifest = read_manifest(self.package)
        manifest["disk"]["pttype"] = "dos"
        self.assertEqual(validate_restore_plan(manifest, self.target)["partition_count"], 2)

    def test_live_root_and_boot_archives_validate(self):
        writer = BackupPackageWriter(
            self.package,
            {
                "backup_mode": "live-filesystem",
                "disk": {"size_bytes": 4096},
                "partitions": [
                    {"number": 1, "filesystem": "vfat"},
                    {"number": 2, "filesystem": "ext4"},
                ],
                "changed_file_count": 2,
                "acquisition_error_count": 0,
                "consistency_classification": "LIVE_WITH_CHANGED_FILES",
            },
        )
        with writer.entry(
            "filesystems/root.tar.zst", role="live-root-archive", compression="zstd"
        ) as out:
            out.write(b"root")
        with writer.entry(
            "filesystems/boot.tar.zst", role="live-boot-archive", compression="zstd"
        ) as out:
            out.write(b"boot")
        writer.complete()
        manifest = verify_package(self.package)
        self.assertEqual(validate_restore_plan(manifest, self.target)["partition_count"], 2)

    def test_live_package_with_errors_is_not_restorable(self):
        manifest = {
            "backup_mode": "live-filesystem",
            "disk": {"size_bytes": 4096},
            "partitions": [],
            "acquisition_error_count": 1,
            "entries": [{"role": "live-root-archive"}, {"role": "live-boot-archive"}],
        }
        with self.assertRaisesRegex(PackageError, "acquisition errors"):
            validate_restore_plan(manifest, self.target)


if __name__ == "__main__":
    unittest.main()
