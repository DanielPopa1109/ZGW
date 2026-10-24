"""Read-only production probe, or explicitly selected /tmp fixture stream test."""
import argparse
import hashlib
import importlib.machinery
import importlib.util
import io
import json
from pathlib import Path
import ssl
import sys
import threading
import time


def load_fcd():
    loader = importlib.machinery.SourceFileLoader("fcd_ethernet_validation", str(Path(__file__).with_name("FCD.pyw")))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, help="Bootstrap address; data destination is discovered")
    parser.add_argument("--port", type=int, default=13400)
    parser.add_argument("--token-file", type=Path, required=True)
    parser.add_argument("--certificate", type=Path, required=True)
    parser.add_argument("--synthetic-fixture", action="store_true", help="Only streams /tmp/fcd-ethernet-validation/fixture.img on port 18502")
    args = parser.parse_args()
    if args.synthetic_fixture and args.port != 18502:
        parser.error("synthetic streaming is restricted to isolated test port 18502")
    module = load_fcd()
    fingerprint = hashlib.sha256(ssl.PEM_cert_to_DER_cert(args.certificate.read_text())).hexdigest()
    client = module.Cm5BackupClient(args.host, args.port, args.token_file.read_text().strip(), fingerprint)
    client.log = print
    try:
        hello = client.connect()
        print("Agent", hello["agent_version"])
        if args.synthetic_fixture:
            info = client.get_info()
            if info.get("device") != "/tmp/fcd-ethernet-validation/fixture.img" or info.get("capacity") != 8388608:
                raise RuntimeError("refusing to stream: isolated synthetic source not verified")
            expected = hashlib.sha256(bytes(8388608)).hexdigest()
            stream_times = []
            original_stream = client._stream

            def timed_stream(*values):
                start = time.monotonic()
                result = original_stream(*values)
                stream_times.append(time.monotonic() - start)
                return result

            client._stream = timed_stream
            for limit, dynamic in ((50, False), (10, True)):
                client.initialize("raw", bandwidth_mbps=limit)
                changed = False

                def progress(done, total, elapsed):
                    nonlocal changed
                    if dynamic and not changed and done >= total // 2:
                        assert client.set_bandwidth(25)
                        changed = True

                output = io.BytesIO()
                result = client.stream(output, 0, threading.Event(), progress)
                assert len(output.getvalue()) == 8388608
                assert hashlib.sha256(output.getvalue()).hexdigest() == expected == result["sha256"]
                measured = 8388608 * 8 / stream_times[-1] / 1000000
                if not dynamic:
                    assert limit * 0.90 <= measured <= limit * 1.10, (limit, measured)
                print("PASS synthetic stream", json.dumps({"initial_mbps": limit, "dynamic_25": changed,
                                                           "measured_mbps": round(measured, 2),
                                                           "bytes": len(output.getvalue()), "sha256_verified": True}))
        else:
            # Deliberately no INFO, INIT or START in the production probe.
            print("PASS Ethernet-only control-plane probe; no eMMC acquisition")
    finally:
        client.close()


if __name__ == "__main__":
    main()
