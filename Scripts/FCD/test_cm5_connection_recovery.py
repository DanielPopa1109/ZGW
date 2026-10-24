import importlib.util
from pathlib import Path
import struct
import sys
import json
import unittest
from unittest import mock

SPEC=importlib.util.spec_from_file_location('fcd_recovery_tests',Path(__file__).with_name('FCD.pyw'))
M=importlib.util.module_from_spec(SPEC);sys.modules[SPEC.name]=M;SPEC.loader.exec_module(M)

def frame(command,sequence,data,result=0):
    body=struct.pack('>HH',command,result)+json.dumps(data).encode()
    return M.CM5E_HEADER.pack(b'CM5E',1,9 if result else 8,0,len(body),sequence,0,0x434D3501,sequence)+body

class Socket:
    def __init__(self,data=b''):self.data=bytearray(data);self.closed=False;self.sent=[]
    def sendall(self,data):self.sent.append(data)
    def recv(self,n):
        value=self.data[:n];del self.data[:n];return bytes(value)
    def shutdown(self,_):pass
    def close(self):self.closed=True

class RecoveryTests(unittest.TestCase):
    def client(self,sock):
        client=M.Cm5DiagnosticsClient('unused');client.sock=sock;client._allow_reconnect=True;return client

    def test_optional_service_error_does_not_disconnect_or_reconnect(self):
        sock=Socket(frame(2,1,{'error':'cm5-health-monitor unavailable'},4)+frame(8,2,{'applications':[]}))
        client=self.client(sock)
        with mock.patch.object(client,'_connect') as reconnect:
            with self.assertRaises(M.Cm5AgentResponseError):client.request(2)
            self.assertTrue(client.connected);self.assertFalse(sock.closed)
            self.assertEqual(client.request(8),{'applications':[]})
            reconnect.assert_not_called()

    def test_partial_response_is_discarded_and_safe_read_reauthenticated_once(self):
        old=Socket(frame(8,1,{'stale':'must not leak'})[:35])
        client=self.client(old)
        new=Socket(frame(8,2,{'fresh':True}))
        def reconnect():client.sock=new
        with mock.patch.object(client,'_connect',side_effect=reconnect) as connect:
            self.assertEqual(client.request(8),{'fresh':True})
            connect.assert_called_once()
        self.assertTrue(old.closed)

    def test_persistent_transport_failure_stops_after_one_reconnect(self):
        client=self.client(Socket())
        with mock.patch.object(client,'_connect',side_effect=OSError('offline')) as connect:
            with self.assertRaises(OSError):client.request(8)
            with self.assertRaises(ConnectionError):client.request(8)
            connect.assert_called_once()
        self.assertFalse(client.can_reconnect)

    def test_mutating_command_is_never_replayed(self):
        client=self.client(Socket())
        with mock.patch.object(client,'_connect') as connect:
            with self.assertRaises(ConnectionError):client.request(15)
            connect.assert_not_called()
        self.assertFalse(client.can_reconnect)

    def test_protocol_correlation_failure_requires_explicit_connection(self):
        client=self.client(Socket(frame(8,999,{'wrong':True})))
        with mock.patch.object(client,'_connect') as connect:
            with self.assertRaises(M.Cm5AgentError):client.request(8)
            connect.assert_not_called()
        self.assertFalse(client.connected);self.assertFalse(client.can_reconnect)

    def test_explicit_disconnect_disables_recovery(self):
        client=self.client(Socket());client.disconnect()
        with mock.patch.object(client,'_connect') as connect:
            with self.assertRaises(ConnectionError):client.request(8)
            connect.assert_not_called()

if __name__=='__main__':unittest.main()
