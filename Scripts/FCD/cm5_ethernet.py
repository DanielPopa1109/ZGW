"""Fail-closed IPv4 Ethernet sockets for FCD backup; never changes OS routes."""
import ipaddress
import json
import os
import socket
import struct
import subprocess


class EthernetError(RuntimeError):
    pass


def run_json(argv):
    result = subprocess.run(argv, capture_output=True, text=True, timeout=15,
                            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    if result.returncode:
        raise EthernetError("Ethernet-only: adapter/route inspection failed: " + result.stderr.strip())
    try:
        return json.loads(result.stdout)
    except ValueError as exc:
        raise EthernetError("Ethernet-only: invalid adapter/route inspection result") from exc


def local_paths(destination):
    """Select only direct, physical, operational Ethernet paths, regardless of metric."""
    remote = ipaddress.IPv4Address(destination)
    if remote.is_loopback or remote.is_multicast or remote.is_unspecified:
        raise EthernetError("Ethernet-only: invalid discovered Ethernet destination")
    if os.name == "nt":
        # Numeric validated destination only. Physical NDIS medium 14 is 802.3;
        # Wi-Fi also reports MediaType 802.3, so MediaType alone is insufficient.
        script = r"""$ErrorActionPreference='Stop'; $items=@();
foreach($a in @(Get-NetAdapter -Physical)) {
 if($a.Status -ne 'Up' -or [int]$a.NdisPhysicalMedium -ne 14 -or [long]$a.TransmitLinkSpeed -le 0){continue}
 foreach($p in @(Get-NetIPAddress -InterfaceIndex $a.ifIndex -AddressFamily IPv4 -AddressState Preferred)) {
  $r=@(Find-NetRoute -RemoteIPAddress 'DESTINATION' -InterfaceIndex $a.ifIndex -LocalIPAddress $p.IPAddress -ErrorAction SilentlyContinue);
  $route=@($r | Where-Object {$null -ne $_.DestinationPrefix});
  if($route.Count -eq 1 -and $route[0].NextHop -eq '0.0.0.0') {
   $items+=@{interface=$a.Name; index=[int]$a.ifIndex; source=$p.IPAddress; prefix=[int]$p.PrefixLength; status='Up'; carrier=$true;
    speed_mbps=([long]$a.TransmitLinkSpeed / 1000000); route=$route[0].DestinationPrefix}
  }
 }
}; ConvertTo-Json -InputObject @($items) -Compress""".replace("DESTINATION", str(remote))
        items = run_json(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script])
    elif os.name == "posix":
        items = []
        from pathlib import Path
        for a in run_json(["ip", "-j", "-4", "addr", "show"]):
            name = a["ifname"]
            root = Path("/sys/class/net") / name
            if (a.get("operstate") != "UP" or not (root / "device").exists()
                    or (root / "wireless").exists() or (root / "phy80211").exists()
                    or (root / "type").read_text().strip() != "1"):
                continue
            speed = int((root / "speed").read_text())
            if (root / "carrier").read_text().strip() != "1" or speed <= 0:
                continue
            for p in a.get("addr_info", []):
                if p.get("family") != "inet":
                    continue
                route = run_json(["ip", "-j", "-4", "route", "get", str(remote),
                                  "from", p["local"], "oif", name])
                if len(route) == 1 and route[0].get("dev") == name and not route[0].get("gateway"):
                    items.append(dict(interface=name, index=a["ifindex"], source=p["local"],
                                      prefix=p["prefixlen"], speed_mbps=speed, route=route[0], status="UP", carrier=True))
    else:
        raise EthernetError("Ethernet-only: unsupported operating system")
    result = []
    for item in items:
        network = ipaddress.IPv4Network(f"{item['source']}/{item['prefix']}", strict=False)
        if remote in network and int(item["index"]) > 0 and float(item["speed_mbps"]) > 0:
            result.append(item)
    return result


def bind_socket(sock, path):
    """Pin device AND address before connect. Never retry without either binding."""
    if os.name == "nt":
        # IP_UNICAST_IF is a DWORD interface index in network byte order.
        sock.setsockopt(socket.IPPROTO_IP, 31, struct.pack("!I", path["index"]))
        # Winsock returns this value in host byte order (setter uses network order).
        actual = sock.getsockopt(socket.IPPROTO_IP, 31)
        if actual != path["index"]:
            raise EthernetError("Ethernet-only: Windows socket interface binding failed")
    elif os.name == "posix":
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, path["interface"].encode() + b"\0")
        actual = sock.getsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, 64).rstrip(b"\0")
        if actual != path["interface"].encode():
            raise EthernetError("Ethernet-only: Linux socket device binding failed")
    else:
        raise EthernetError("Ethernet-only: unsupported operating system")
    sock.bind((path["source"], 0))


class EthernetPath:
    def __init__(self, destination, path):
        self.destination = str(ipaddress.IPv4Address(destination))
        self.path = path

    @classmethod
    def discover(cls, addresses):
        paths = []
        for address in addresses:
            for path in local_paths(address):
                paths.append(cls(address, path))
        if not paths:
            raise EthernetError("Ethernet-only: no verified direct Ethernet path to CM5 eth0; dump aborted")
        # Do not guess among multiple directly connected Ethernet networks.
        unique = {(p.destination, p.path["source"], p.path["index"]): p for p in paths}
        if len(unique) != 1:
            raise EthernetError("Ethernet-only: ambiguous Ethernet paths; dump aborted")
        return next(iter(unique.values()))

    def connect(self, port, timeout):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            sock.settimeout(timeout)
            bind_socket(sock, self.path)
            sock.connect((self.destination, port))
            self.verify(sock, inspect_path=False)
            return sock
        except Exception:
            sock.close()
            raise

    def verify(self, sock, inspect_path=True):
        if inspect_path and not any(p["index"] == self.path["index"] and p["source"] == self.path["source"]
                                    and p["interface"] == self.path["interface"] for p in local_paths(self.destination)):
            raise EthernetError("Ethernet-only: local Ethernet link/address/route lost; dump interrupted")
        if os.name == "nt":
            if sock.getsockopt(socket.IPPROTO_IP, 31) != self.path["index"]:
                raise EthernetError("Ethernet-only: Windows socket interface binding lost")
        elif os.name == "posix":
            bound = sock.getsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, 64).rstrip(b"\0")
            if bound != self.path["interface"].encode():
                raise EthernetError("Ethernet-only: Linux socket device binding lost")
        if sock.getsockname()[0] != self.path["source"] or sock.getpeername()[0] != self.destination:
            raise EthernetError("Ethernet-only: TCP endpoint mismatch")
        return dict(self.path, destination=self.destination,
                    tcp_source=list(sock.getsockname()), tcp_destination=list(sock.getpeername()))

    def verify_remote(self, sock, remote, inspect_path=True):
        local = self.verify(sock, inspect_path=inspect_path)
        if (remote.get("interface") != "eth0" or remote.get("bound") is not True
                or remote.get("carrier") is not True or float(remote.get("speed_mbps", 0)) <= 0
                or remote.get("local_ip") != sock.getpeername()[0]
                or remote.get("local_port") != sock.getpeername()[1]
                or remote.get("peer_ip") != sock.getsockname()[0]
                or remote.get("peer_port") != sock.getsockname()[1]
                or " dev eth0 " not in remote.get("route", "")
                or " via " in remote.get("route", "")):
            raise EthernetError("Ethernet-only: CM5 TCP endpoints, device binding or route not verified")
        return {"local": local, "cm5": remote}
