import io
import socket
import struct
import threading
import time
import unittest
from unittest import mock

import cm5_ethernet as E
from validate_cm5_ethernet import load_fcd

F = load_fcd()
DEST = "192.0.2.2"
PATH = dict(interface="Ethernet", index=10, source="192.0.2.1", prefix=24,
            route="192.0.2.0/24", speed_mbps=1000)


def tcp():
    sock = mock.Mock()
    sock.getsockname.return_value = (PATH["source"], 12345)
    sock.getpeername.return_value = (DEST, 18500)
    sock.getsockopt.return_value = PATH["index"]
    return sock


def remote():
    return dict(interface="eth0", bound=True, carrier=True, speed_mbps=1000,
                local_ip=DEST, local_port=18500, peer_ip=PATH["source"], peer_port=12345,
                route=f"{PATH['source']} from {DEST} dev eth0 uid 0 \n cache")


class EthernetTests(unittest.TestCase):
    @mock.patch.object(E, "local_paths", return_value=[PATH])
    def test_both_connected_selects_discovered_eth0(self, paths):
        path = E.EthernetPath.discover([DEST])
        self.assertEqual(path.destination, DEST)
        self.assertEqual(path.path["interface"], "Ethernet")

    @mock.patch.object(E, "local_paths", return_value=[PATH])
    def test_ethernet_only_does_not_require_wifi(self, paths):
        E.EthernetPath.discover([DEST]).verify_remote(tcp(), remote())

    @mock.patch.object(E, "local_paths", return_value=[])
    def test_wifi_only_aborts(self, paths):
        with self.assertRaisesRegex(E.EthernetError, "no verified direct Ethernet"):
            E.EthernetPath.discover([DEST])

    @mock.patch.object(E, "local_paths", return_value=[])
    def test_bound_metadata_connection_reuses_discovered_path(self, paths):
        path = E.EthernetPath(DEST, PATH)
        sock = tcp()
        with mock.patch.object(E.socket, "socket", return_value=sock), mock.patch.object(E, "bind_socket"):
            self.assertIs(path.connect(18500, 2), sock)
        paths.assert_not_called()
        sock.connect.assert_called_once_with((DEST, 18500))

    def test_local_path_loss_still_fails_before_acquisition(self):
        path = E.EthernetPath(DEST, PATH)
        with mock.patch.object(E, "local_paths", return_value=[]):
            with self.assertRaisesRegex(E.EthernetError, "link/address/route lost"):
                path.verify_remote(tcp(), remote())

    def test_bound_interface_loss_fails_even_during_metadata_connection(self):
        path = E.EthernetPath(DEST, PATH)
        sock = tcp()
        sock.getsockopt.return_value = 20
        with self.assertRaisesRegex(E.EthernetError, "interface binding lost"):
            path.verify(sock, inspect_path=False)

    @mock.patch.object(E, "run_json", return_value=[PATH])
    def test_lower_wifi_metric_uses_constrained_ethernet_route(self, run):
        with mock.patch.object(E.os, "name", "nt"):
            self.assertEqual(E.local_paths(DEST), [PATH])
        script = run.call_args.args[0][-1]
        self.assertIn("-InterfaceIndex $a.ifIndex -LocalIPAddress $p.IPAddress", script)
        self.assertIn("NdisPhysicalMedium -ne 14", script)

    def test_hostname_resolving_to_wifi_only_bootstraps_metadata(self):
        client = F.Cm5BackupClient("wifi-name", fingerprint="a" * 64)
        final = tcp()
        path = E.EthernetPath(DEST, PATH)
        hello = dict(extensions=["ethernet-only-v1"], ethernet_addresses=[DEST])
        seen = []

        def handshake(sock):
            client.sock = sock
            seen.append(sock)
            return hello

        with mock.patch.object(F.socket, "create_connection", return_value=mock.Mock()) as bootstrap, \
                mock.patch.object(client, "_connect_tls", side_effect=handshake), \
                mock.patch.object(E.EthernetPath, "discover", return_value=path) as discover, \
                mock.patch.object(path, "connect", return_value=final), \
                mock.patch.object(client, "request", return_value={}) as request, \
                mock.patch.object(client, "verify_transport"):
            client.connect()
        bootstrap.assert_called_once_with(("wifi-name", F.DEFAULT_PORT), 120.0)
        discover.assert_called_once_with([DEST])
        self.assertIs(client.sock, final)
        self.assertEqual([c.args[0] for c in request.call_args_list], [F.AUTH])
        seen[0].close.assert_called_once()

    def test_old_agent_rejected_before_auth_or_init(self):
        client = F.Cm5BackupClient("host", fingerprint="a" * 64)

        def handshake(_raw):
            client.sock = tcp()
            return {"extensions": []}

        with mock.patch.object(F.socket, "create_connection"), \
                mock.patch.object(client, "_connect_tls", side_effect=handshake), \
                mock.patch.object(client, "request") as request:
            with self.assertRaisesRegex(F.BackupError, "agent 2.3.0"):
                client.connect()
        request.assert_not_called()

    @mock.patch.object(E, "local_paths", return_value=[PATH])
    def test_wrong_remote_interfaces_addresses_and_bindings_rejected(self, paths):
        path = E.EthernetPath(DEST, PATH)
        for key, wrong in (("interface", "wlan0"), ("bound", False), ("carrier", False),
                           ("local_ip", "192.0.2.3"), ("peer_ip", "100.64.1.1"),
                           ("local_port", 22), ("peer_port", 999), ("speed_mbps", -1),
                           ("route", "192.0.2.1 dev tailscale0 ")):
            with self.subTest(key=key):
                value = remote()
                value[key] = wrong
                with self.assertRaises(E.EthernetError):
                    path.verify_remote(tcp(), value)

    @mock.patch.object(E, "local_paths", return_value=[PATH])
    def test_wrong_client_endpoint_rejected(self, paths):
        sock = tcp()
        sock.getsockname.return_value = ("100.64.1.1", 12345)
        with self.assertRaisesRegex(E.EthernetError, "endpoint mismatch"):
            E.EthernetPath(DEST, PATH).verify(sock)

    @mock.patch.object(E, "local_paths", return_value=[PATH, dict(PATH, index=20)])
    def test_ambiguous_paths_rejected(self, paths):
        with self.assertRaisesRegex(E.EthernetError, "ambiguous"):
            E.EthernetPath.discover([DEST])

    def test_windows_binding_network_order_and_host_order_readback(self):
        sock = tcp()
        sock.getsockopt.return_value = 10
        with mock.patch.object(E.os, "name", "nt"):
            E.bind_socket(sock, PATH)
        sock.setsockopt.assert_called_once_with(socket.IPPROTO_IP, 31, struct.pack("!I", 10))
        sock.bind.assert_called_once_with((PATH["source"], 0))

    def test_binding_failure_never_connects(self):
        sock = tcp()
        sock.setsockopt.side_effect = OSError("binding denied")
        with mock.patch.object(E.socket, "socket", return_value=sock):
            with self.assertRaises(OSError):
                E.EthernetPath(DEST, PATH).connect(18500, 2)
        sock.connect.assert_not_called()
        sock.close.assert_called_once()

    def test_link_lost_during_simulated_transfer_stops_socket(self):
        client = F.Cm5BackupClient("host")
        sock = tcp()
        client.sock = sock
        client.ethernet = mock.Mock()
        client.ethernet.verify.side_effect = E.EthernetError("synthetic Ethernet link lost")

        def stream(output, offset, cancel, progress):
            output.write(b"partial data")
            deadline = time.monotonic() + 4
            while client.sock is not None and time.monotonic() < deadline:
                time.sleep(0.01)
            raise OSError("connection closed")

        output = io.BytesIO()
        with mock.patch.object(client, "verify_transport"), mock.patch.object(client, "_stream", side_effect=stream):
            with self.assertRaisesRegex(F.BackupError, "synthetic Ethernet link lost"):
                client.stream(output, 0, threading.Event(), lambda *args: None)
        self.assertIsNone(client.sock)
        self.assertEqual(output.getvalue(), b"partial data")
        sock.close.assert_called_once()

    def test_verify_failure_prevents_init(self):
        client = F.Cm5BackupClient("host")
        with mock.patch.object(client, "verify_transport", side_effect=E.EthernetError("eth0 down")), \
                mock.patch.object(client, "request") as request:
            with self.assertRaises(E.EthernetError):
                client.initialize("raw", bandwidth_mbps=50)
        request.assert_not_called()


if __name__ == "__main__":
    unittest.main()
