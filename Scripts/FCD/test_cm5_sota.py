import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from unittest.mock import Mock
from types import SimpleNamespace

spec = importlib.util.spec_from_file_location("fcd_sota_tests", Path(__file__).with_name("FCD.pyw"))
app = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = app
spec.loader.exec_module(app)


def proof(**changes):
    result = dict(capability="sota-wifi-uds-v1", interface="wlan0", bound=True, wifi=True,
                  local_ip="192.168.1.158", peer_ip="192.168.1.142")
    result.update(changes)
    return "OK " + json.dumps(result)


class Client:
    def __init__(self, transport=None):
        self.transport = transport or proof()
        self.sock = SimpleNamespace(getpeername=lambda: ("192.168.1.158", 30520),
                                    getsockname=lambda: ("192.168.1.142", 55555))
        self.sent = []
        self._last_request_ts = 0
        self.request_spacing_seconds = 0
        self.closed = False

    def connect(self):
        pass

    def routing_activation(self):
        pass

    def close(self):
        self.closed = True

    def send_uds(self, q, timeout=None):
        self.sent.append(q)
        if q == b"\x22\xf0\xa0":
            return b"\x62\xf0\xa0" + self.transport.encode()
        if q[0] == 0x10:
            return b"\x50" + q[1:2]
        if q[0] == 0x31:
            return b"\x71" + q[1:]
        if q[0] == 0x34:
            return b"\x74\x20\x80\x02"
        if q[0] == 0x36:
            return b"\x76" + q[1:2]
        if q[0] == 0x37:
            return b"\x77OK STAGED NEW 3"
        raise AssertionError(q.hex())


class SotaTests(unittest.TestCase):
    def test_acknowledgements_give_fine_progress_without_double_counting(self):
        live = app.SotaLiveJob()
        events = [dict(sequence=1, timestamp_us=1000000, kind='PHASE'),
                  dict(sequence=2, timestamp_us=1100000, kind='UDS_RESPONSE', sid=0x36,
                       target='CBM', route=0x54, wire_size=256, subfunction_or_bsc=1, response_hex='7601'),
                  dict(sequence=3, timestamp_us=1200000, kind='UDS_RESPONSE', sid=0x36,
                       target='ZGW', route=-1, wire_size=32770, subfunction_or_bsc=1, response_hex='7601')]
        live.consume(events)
        live.consume(events)
        status = dict(state='INSTALLING', completed_bytes=0, total_bytes=40000,
                      targets={'CBM': dict(completed_bytes=0)})
        result = live.status(status)
        self.assertEqual(result['completed_bytes'], 253 + 32768)
        self.assertEqual(result['targets']['CBM']['completed_bytes'], 253)
        self.assertAlmostEqual(result['elapsed_seconds'], .2)
        self.assertEqual(status['completed_bytes'], 0)
        self.assertEqual(status['targets']['CBM']['completed_bytes'], 0)
        with self.assertRaisesRegex(RuntimeError, 'gap'):
            live.consume([dict(sequence=5, timestamp_us=1400000, kind='PHASE')])

    def test_invalid_transfer_response_does_not_advance_progress(self):
        live = app.SotaLiveJob()
        live.consume([dict(sequence=1, timestamp_us=1000000, kind='UDS_RESPONSE', sid=0x36,
                           target='CBM', route=0x54, wire_size=256, subfunction_or_bsc=1, response_hex='7602')])
        self.assertEqual(live.status({})['completed_bytes'], 0)

    def test_completed_duration_does_not_follow_partial_startup_history(self):
        live = app.SotaLiveJob()
        snapshot = dict(job_id='1000000', updated_at=181, state='COMPLETED')
        self.assertEqual(live.status(snapshot)['elapsed_seconds'], 180)
        live.consume([dict(sequence=1, timestamp_us=1000000, kind='PHASE', state='VALIDATING'),
                      dict(sequence=2, timestamp_us=20000000, kind='PHASE', state='INSTALLING')])
        self.assertEqual(live.status(snapshot)['elapsed_seconds'], 180)
        live.consume([dict(sequence=3, timestamp_us=181200000, kind='PHASE', state='COMPLETED')])
        self.assertAlmostEqual(live.status(snapshot)['elapsed_seconds'], 180.2)

    def test_elapsed_is_logged_from_cm5_event_times(self):
        event = dict(job_id='42', sequence=2, timestamp_us=3724123000, kind='NODE_READY', target='CBM')
        self.assertIn('[+01:02:03.123]', app.format_sota_event(event, 1000000))

    def test_elapsed_timer_advances_between_polls_and_freezes_at_terminal_duration(self):
        instance = self.progress_app()
        instance._sota_elapsed_clock = None
        instance._sota_monitor_closed = False
        instance.sota_elapsed_var = Mock()
        instance.root = Mock()
        with patch.object(app.time, 'monotonic', return_value=100):
            instance._sota_set_elapsed(dict(job_id='42', state='INSTALLING', elapsed_seconds=5, elapsed_event_us=10))
        with patch.object(app.time, 'monotonic', return_value=103):
            instance._sota_tick_elapsed()
        instance.sota_elapsed_var.set.assert_called_with('Elapsed: 00:00:08')
        with patch.object(app.time, 'monotonic', return_value=103):
            instance._sota_set_elapsed(dict(job_id='42', state='INSTALLING', elapsed_seconds=5, elapsed_event_us=10))
        instance.sota_elapsed_var.set.assert_called_with('Elapsed: 00:00:08')
        with patch.object(app.time, 'monotonic', return_value=104):
            instance._sota_set_elapsed(dict(job_id='42', state='COMPLETED', elapsed_seconds=8.5, elapsed_event_us=20))
        with patch.object(app.time, 'monotonic', return_value=110):
            instance._sota_tick_elapsed()
        instance.sota_elapsed_var.set.assert_called_with('Elapsed: 00:00:08')
        self.assertFalse(instance._sota_elapsed_clock['running'])

    def test_status_is_published_before_event_fetch_then_after_acknowledgement(self):
        with tempfile.TemporaryDirectory() as directory:
            instance = object.__new__(app.FcdApp)
            instance._sota_events_lock = app.threading.Lock()
            instance._sota_event_journal = app.SotaEventJournal(directory)
            instance._sota_observed_jobs, instance._sota_pending_event_jobs = set(), set()
            instance.log, instance.log_batch = Mock(), Mock(side_effect=lambda messages: list(messages))
            published = []
            def request(host, port, command, *args, **kwargs):
                if command == app.SOTA_STATUS:
                    return 'OK ' + json.dumps(dict(job_id='42', state='INSTALLING', completed_bytes=0,
                                                   event_stream='sota-events-v1'))
                self.assertEqual(published[0]['completed_bytes'], 0)
                event = dict(job_id='42', sequence=1, timestamp_us=1000000, kind='UDS_RESPONSE',
                             sid=0x36, target='CBM', wire_size=256, route=0x54,
                             subfunction_or_bsc=1, response_hex='7601')
                return 'OK ' + json.dumps(dict(job_id='42', events=[event], next_cursor=1, more=False))
            with patch.object(app, 'sota_request', side_effect=request):
                result = instance._sota_read_status('192.168.1.158', 30520, publish=published.append)
            self.assertEqual([value['completed_bytes'] for value in published], [0, 253])
            self.assertEqual(result['completed_bytes'], 253)

    def test_log_batch_keeps_all_lines_with_one_disk_sync(self):
        instance = object.__new__(app.FcdApp)
        instance.log_queue = app.queue.Queue()
        with tempfile.TemporaryDirectory() as directory, patch.object(app, 'SCRIPT_DIR', Path(directory)), \
                patch.object(app.os, 'fsync', wraps=app.os.fsync) as sync:
            instance.log_batch(iter(['CM5 SOTA event 1', 'Other event', 'CM5 SOTA event 2']))
            self.assertEqual(sync.call_count, 1)
            saved = (Path(directory) / 'SOTALogs/fcd-sota.log').read_text()
        self.assertIn('CM5 SOTA event 1', saved)
        self.assertIn('CM5 SOTA event 2', saved)
        self.assertNotIn('Other event', saved)
        self.assertEqual(instance.log_queue.qsize(), 3)

    def test_trace_queue_renders_one_batch_without_dropping_lines(self):
        instance = object.__new__(app.FcdApp)
        instance.root, instance.trace_text, instance.trace_autoscroll_var = Mock(), Mock(), Mock()
        instance.log_queue = app.queue.Queue()
        for i in range(105):
            instance.log_queue.put(str(i))
        instance._drain_log_queue()
        instance.trace_text.insert.assert_called_once_with('end', ''.join(f'{i}\n' for i in range(100)))
        self.assertEqual(instance.log_queue.qsize(), 5)

    def test_monitor_polls_fast_while_active_or_catching_up_and_slows_when_idle(self):
        for state, backlog, expected in [('INSTALLING', False, 250), ('COMPLETED', True, 250), ('COMPLETED', False, 3000)]:
            instance = object.__new__(app.FcdApp)
            instance._sota_monitor_closed = instance._sota_poll_running = False
            instance.root = Mock()
            instance.sota_host_var, instance.sota_port_var = Mock(), Mock()
            instance.sota_port_var.get.return_value = '30520'
            instance._sota_read_status = Mock(return_value=dict(state=state, event_backlog=backlog))
            with patch.object(app.threading, 'Thread') as thread:
                thread.return_value.start.side_effect = lambda: thread.call_args.kwargs['target']()
                instance._sota_poll()
            instance.root.after.assert_called_with(expected, instance._sota_poll)

    def test_explicit_status_refresh_starts_monitor_once(self):
        instance = object.__new__(app.FcdApp)
        instance._sota_monitor_enabled = instance._sota_monitor_closed = False
        instance.sota_host_var, instance.sota_port_var = Mock(), Mock()
        instance.sota_port_var.get.return_value = '30520'
        instance._sota_read_status, instance._sota_poll = Mock(), Mock()
        instance.root = SimpleNamespace(after=lambda delay, callback: callback())
        instance.worker = lambda name, action: action()
        instance.sota_status_clicked()
        instance.sota_status_clicked()
        self.assertEqual(instance._sota_read_status.call_count, 2)
        instance._sota_poll.assert_called_once()

    def progress_app(self):
        instance = object.__new__(app.FcdApp)
        instance.sota_progress, instance.sota_status_var = Mock(), Mock()
        instance.sota_node_tree = Mock()
        instance.sota_node_tree.get_children.return_value = []
        instance._refresh_sota_tree, instance.log = Mock(), Mock()
        instance._sota_current_job, instance._sota_upload_view = '42', None
        instance._sota_last_status = None
        instance.root = SimpleNamespace(after=lambda delay, callback: callback())
        return instance

    def test_upload_progress_counts_only_acknowledged_blocks(self):
        client, updates = Client(), []
        payload = b'x' * 32769
        def progress(done, total):
            updates.append((done, total))
            if done:
                self.assertEqual(client.sent[-1][0], 0x36)
        with patch.object(app, 'DoipClient', return_value=client):
            app.sota_request('192.168.1.158', 30520, app.SOTA_UPLOAD_NEW, payload, progress=progress)
        self.assertEqual(updates, [(0, 32769), (32768, 32769), (32769, 32769)])

    def test_bad_counter_does_not_advance_upload_progress(self):
        client, updates = Client(), []
        original = client.send_uds
        client.send_uds = lambda q, timeout=None: b'\x76\x02' if q[0] == 0x36 else original(q, timeout)
        with patch.object(app, 'DoipClient', return_value=client), self.assertRaisesRegex(RuntimeError, 'counter'):
            app.sota_request('192.168.1.158', 30520, app.SOTA_UPLOAD_NEW, b'abc',
                             progress=lambda done, total: updates.append((done, total)))
        self.assertEqual(updates, [(0, 3)])

    def test_two_bundle_upload_progress_and_poll_ownership(self):
        instance = self.progress_app()
        with tempfile.TemporaryDirectory() as directory:
            paths = [Path(directory) / name for name in ('new.pfpkg', 'recovery.pfpkg')]
            paths[0].write_bytes(b'abc')
            paths[1].write_bytes(b'12345')
            for name, value in dict(sota_new_bundle_var=str(paths[0]), sota_rollback_bundle_var=str(paths[1]),
                                    sota_host_var='192.168.1.158', sota_port_var='30520').items():
                setattr(instance, name, SimpleNamespace(get=lambda saved=value: saved))
            instance.worker = lambda name, action: action()
            def upload(host, port, command, payload, **kwargs):
                kwargs['progress'](0, len(payload))
                kwargs['progress'](len(payload), len(payload))
                before = instance.sota_progress.configure.call_args
                instance._sota_apply_status(dict(job_id='42', state='COMPLETED', completed_bytes=9,
                                                  total_bytes=9, event_stream='sota-events-v1'))
                self.assertEqual(instance.sota_progress.configure.call_args, before)
                return 'OK STAGED'
            with patch.object(app, 'load_bundle'), patch.object(app, 'validate_sota_manifest'), \
                    patch.object(app, 'sota_request', side_effect=upload):
                instance.sota_transfer_clicked()
        values = [call.kwargs['value'] for call in instance.sota_progress.configure.call_args_list]
        self.assertEqual(values, [0, 0.0, 37.5, 37.5, 99.9, 100.0])
        self.assertIn('Both bundles transferred and staged', instance.sota_status_var.set.call_args.args[0])
        instance._sota_apply_status(dict(job_id='42', state='COMPLETED', completed_bytes=9,
                                        total_bytes=9, event_stream='sota-events-v1'))
        self.assertIn('Both bundles', instance.sota_status_var.set.call_args.args[0])
        instance._sota_apply_status(dict(job_id='43', state='TRANSFERRING', completed_bytes=20,
                                        total_bytes=100, event_stream='sota-events-v1'))
        instance.sota_progress.configure.assert_called_with(value=20, maximum=100)
        self.assertIn('(20.0%)', instance.sota_status_var.set.call_args.args[0])

    def test_upload_failure_keeps_acknowledged_progress_and_stale_callbacks_are_ignored(self):
        instance = self.progress_app()
        token = object()
        instance._sota_begin_upload(token)
        instance._sota_update_upload(token, 'Bundle transfer failed: CRC mismatch', 8, 8, outcome='failed')
        instance.sota_progress.configure.assert_called_with(value=99.9, maximum=100)
        self.assertFalse(instance._sota_upload_view['active'])
        instance._sota_begin_upload(object())
        instance._sota_update_upload(token, 'stale success', 8, 8, outcome='complete')
        instance.sota_progress.configure.assert_called_with(value=0, maximum=100)

    def test_recovery_staging_failure_never_reports_both_bundles_complete(self):
        instance = self.progress_app()
        with tempfile.TemporaryDirectory() as directory:
            paths = [Path(directory) / name for name in ('new.pfpkg', 'recovery.pfpkg')]
            for path in paths:
                path.write_bytes(b'abc')
            for name, value in dict(sota_new_bundle_var=str(paths[0]), sota_rollback_bundle_var=str(paths[1]),
                                    sota_host_var='192.168.1.158', sota_port_var='30520').items():
                setattr(instance, name, SimpleNamespace(get=lambda saved=value: saved))
            instance.worker = lambda name, action: action()
            def upload(host, port, command, payload, **kwargs):
                kwargs['progress'](len(payload), len(payload))
                if command == app.SOTA_UPLOAD_ROLLBACK:
                    raise RuntimeError('CRC mismatch')
                return 'OK STAGED'
            with patch.object(app, 'load_bundle'), patch.object(app, 'validate_sota_manifest'), \
                    patch.object(app, 'sota_request', side_effect=upload), self.assertRaisesRegex(RuntimeError, 'CRC mismatch'):
                instance.sota_transfer_clicked()
        instance.sota_progress.configure.assert_called_with(value=99.9, maximum=100)
        self.assertIn('failed: CRC mismatch', instance.sota_status_var.set.call_args.args[0])
        self.assertFalse(instance._sota_upload_view['active'])
        instance._refresh_sota_tree.assert_not_called()

    def test_event_pages_are_durable_and_resume_without_duplicates(self):
        with tempfile.TemporaryDirectory() as directory:
            journal = app.SotaEventJournal(directory)
            event = dict(job_id='42', sequence='1', kind='NODE_READY', target='CBM', detail='Readiness passed')
            page = 'OK ' + json.dumps(dict(job_id='42', events=[event], next_cursor='1', more='false'))
            self.assertEqual(journal.append_page('42', page), ([event], False))
            self.assertEqual(journal.append_page('42', page), ([], False))
            reopened = app.SotaEventJournal(directory)
            self.assertEqual(reopened.cursor('42'), 1)
            self.assertEqual(reopened.records('42'), [event])
            bad = dict(event, sequence='3')
            with self.assertRaisesRegex(RuntimeError, 'gap'):
                reopened.append_page('42', 'OK ' + json.dumps(dict(job_id='42', events=[bad], next_cursor=3)))
            self.assertEqual(reopened.cursor('42'), 1)

    def test_event_cursor_does_not_advance_on_local_write_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            journal = app.SotaEventJournal(directory)
            event = dict(job_id='42', sequence='1', kind='NODE_READY', target='CBM')
            page = 'OK ' + json.dumps(dict(job_id='42', events=[event], next_cursor=1, more=False))
            with patch.object(app.os, 'replace', side_effect=OSError('disk error')), self.assertRaises(OSError):
                journal.append_page('42', page)
            self.assertEqual(journal.cursor('42'), 0)
            self.assertEqual(journal.append_page('42', page)[0], [event])
            self.assertEqual(app.SotaEventJournal(directory).cursor('42'), 1)

    def test_all_seven_readiness_results_are_logged_even_when_one_poll_sees_ready(self):
        with tempfile.TemporaryDirectory() as directory:
            instance = object.__new__(app.FcdApp)
            instance._sota_events_lock = app.threading.Lock()
            instance._sota_session_jobs = {'42'}
            instance._sota_event_journal = app.SotaEventJournal(directory)
            instance._sota_observed_jobs, instance._sota_pending_event_jobs = set(), set()
            instance.log = Mock()
            instance.log_batch = lambda messages: [instance.log(message) for message in messages]
            records = [dict(job_id='42', sequence=str(i), timestamp_us='1792527401000000',
                            kind='NODE_READY', target=node, detail='Readiness passed')
                       for i, node in enumerate(sorted(app.SOTA_VEHICLE_NODES), 1)]
            status = 'OK ' + json.dumps(dict(job_id='42', state='READY', event_stream='sota-events-v1'))
            full = 'OK ' + json.dumps(dict(job_id='42', events=records, next_cursor='7', more='false'))
            empty = 'OK ' + json.dumps(dict(job_id='42', events='', next_cursor='7', more='false'))
            with patch.object(app, 'sota_request', side_effect=[status, full, status, empty]):
                instance._sota_read_status('192.168.1.158', 30520)
                instance._sota_read_status('192.168.1.158', 30520)
            self.assertEqual(instance.log.call_count, 7)
            for node in app.SOTA_VEHICLE_NODES:
                self.assertTrue(any('NODE_READY ' + node + ':' in call.args[0] for call in instance.log.call_args_list))

    def test_startup_summarizes_saved_history_without_replaying_or_changing_it(self):
        with tempfile.TemporaryDirectory() as directory:
            journal = app.SotaEventJournal(directory)
            events = [dict(job_id='42', sequence=str(i), kind='PROGRESS', completed_bytes=i)
                      for i in range(1, 3602)]
            journal.append_page('42', 'OK ' + json.dumps(dict(job_id='42', events=events,
                                                              next_cursor=3601, more=False)))
            path = Path(directory) / '42.jsonl'
            before = path.read_bytes()
            instance = object.__new__(app.FcdApp)
            instance._sota_events_lock = app.threading.Lock()
            instance._sota_event_journal = app.SotaEventJournal(directory)
            instance._sota_observed_jobs, instance._sota_pending_event_jobs = set(), set()
            instance.log = Mock()
            instance.log_batch = lambda messages: [instance.log(message) for message in messages]
            status = 'OK ' + json.dumps(dict(job_id='42', state='COMPLETED', event_stream='sota-events-v1'))
            empty = 'OK ' + json.dumps(dict(job_id='42', events=[], next_cursor=3601, more=False))
            with patch.object(app, 'sota_request', side_effect=[status, empty, status, empty]) as request:
                instance._sota_read_status('192.168.1.158', 30520)
                instance._sota_read_status('192.168.1.158', 30520)
            instance.log.assert_called_once()
            self.assertIn('COMPLETED; 3601 saved events retained', instance.log.call_args.args[0])
            self.assertEqual(json.loads(request.call_args_list[1].args[3])['after'], 3601)
            self.assertEqual(path.read_bytes(), before)

    def test_restart_keeps_new_events_and_drains_previous_job_after_next_job_appears(self):
        with tempfile.TemporaryDirectory() as directory:
            old = dict(job_id='42', sequence='1', kind='PHASE', detail='Old phase')
            fresh = dict(job_id='42', sequence='2', kind='NODE_READY', target='CBM', detail='New result')
            new_job = dict(job_id='43', sequence='1', kind='NODE_READY', target='DMU', detail='New job result')
            journal = app.SotaEventJournal(directory)
            def page(job, events, cursor, more=False):
                return 'OK ' + json.dumps(dict(job_id=job, events=events, next_cursor=cursor, more=more))
            journal.append_page('42', page('42', [old], 1))
            instance = object.__new__(app.FcdApp)
            instance._sota_events_lock = app.threading.Lock()
            instance._sota_event_journal = app.SotaEventJournal(directory)
            instance._sota_observed_jobs, instance._sota_pending_event_jobs = set(), set()
            instance.log = Mock()
            instance.log_batch = lambda messages: [instance.log(message) for message in messages]
            statuses = ['OK ' + json.dumps(dict(job_id=job, state='TRANSFERRING', event_stream='sota-events-v1'))
                        for job in ('42', '43')]
            with patch.object(app, 'sota_request', side_effect=[statuses[0], page('42', [fresh], 2),
                    statuses[1], page('42', [], 2), page('43', [new_job], 1)]):
                instance._sota_read_status('192.168.1.158', 30520)
                instance._sota_read_status('192.168.1.158', 30520)
            lines = [call.args[0] for call in instance.log.call_args_list]
            self.assertEqual(len(lines), 3)
            self.assertFalse(any('Old phase' in line for line in lines))
            self.assertTrue(any('NODE_READY CBM: New result' in line for line in lines))
            self.assertTrue(any('NODE_READY DMU: New job result' in line for line in lines))
            self.assertEqual(instance._sota_event_journal.cursor('42'), 2)
            self.assertEqual(instance._sota_pending_event_jobs, {'43'})

    def test_first_attach_to_completed_job_archives_uncached_pages_without_trace_storm(self):
        with tempfile.TemporaryDirectory() as directory:
            instance = object.__new__(app.FcdApp)
            instance._sota_events_lock = app.threading.Lock()
            instance._sota_event_journal = app.SotaEventJournal(directory)
            instance._sota_observed_jobs, instance._sota_pending_event_jobs = set(), set()
            instance.log = Mock()
            instance.log_batch = Mock(side_effect=lambda messages: list(messages))
            status = 'OK ' + json.dumps(dict(job_id='42', state='COMPLETED', event_stream='sota-events-v1'))
            events = [dict(job_id='42', sequence=i, timestamp_us=1000000+i*100000,
                           kind='PHASE', state='COMPLETED') for i in (1, 2)]
            pages = ['OK ' + json.dumps(dict(job_id='42', events=[event], next_cursor=i, more=i==1))
                     for i, event in enumerate(events, 1)]
            with patch.object(app, 'sota_request', side_effect=[status, *pages]):
                instance._sota_read_status('192.168.1.158', 30520)
            self.assertEqual(instance._sota_event_journal.records('42'), events)
            instance.log_batch.assert_not_called()
            instance.log.assert_called_once()
            self.assertIn('2 saved events retained', instance.log.call_args.args[0])

    def test_node_editor_modes_override_old_bundles_and_exclude_cm5(self):
        rows = [dict(node_name="CM5", node_kind="Real"), dict(node_name="CBM", node_kind="Simulated"),
                dict(node_name="ZGW", node_kind="Simulated")]
        self.assertEqual(app.sota_node_modes(rows), {"CBM": "Simulated", "ZGW": "Simulated"})
        rows[1]["node_kind"] = "Real"
        self.assertEqual(app.sota_node_modes(rows)["CBM"], "Real")

    def test_invalid_node_mode_is_rejected(self):
        with self.assertRaises(ValueError):
            app.sota_node_modes([dict(node_name="CBM", node_kind="Maybe")])

    def test_current_modes_use_saved_settings_before_discovery_then_editor_overrides(self):
        instance = object.__new__(app.FcdApp)
        instance.settings = {"data_editor": {"nodes": {"CM5": {"node_kind": "Real"}, "CBM": {"node_kind": "Simulated"}}}}
        instance.node_rows = {}
        self.assertEqual(instance._sota_node_modes(), {"CBM": "Simulated"})
        instance.node_rows = {"row": dict(node_name="CBM", node_kind="Real")}
        self.assertEqual(instance._sota_node_modes(), {"CBM": "Real"})

    def test_launch_sends_current_node_modes(self):
        instance = object.__new__(app.FcdApp)
        for name, value in dict(sota_host_var="192.168.1.158", sota_port_var="30520",
                                sota_gateway_host_var="192.168.1.10", sota_gateway_port_var="13400",
                                source_var="0x0710", target_var="0x1001").items():
            setattr(instance, name, SimpleNamespace(get=lambda saved=value: saved))
        instance.node_rows = {"a": dict(node_name="CM5", node_kind="Real"),
                              "b": dict(node_name="CBM", node_kind="Simulated")}
        instance.worker = lambda name, action: action()
        instance.log, instance.root = Mock(), Mock()
        instance._sota_events_lock = app.threading.Lock()
        instance._sota_monitor_closed = False
        with patch.object(app, "sota_request", side_effect=["OK JOB 42", 'OK {"state":"VALIDATING"}']) as request:
            instance._sota_launch(app.SOTA_EXECUTE)
        payload = json.loads(request.call_args_list[0].args[3])
        self.assertEqual(payload["node_modes"], {"CBM": "Simulated"})
        self.assertEqual(payload["tester_address"], 0x710)

    def test_wifi_proof_requires_matching_both_tcp_endpoints_and_device(self):
        app.validate_sota_transport(proof(), "192.168.1.158", "192.168.1.142")
        for changes in ({"interface": "eth0"}, {"bound": False}, {"wifi": False},
                        {"local_ip": "192.168.1.157"}, {"peer_ip": "192.168.1.99"},
                        {"capability": "old-handler"}):
            with self.subTest(changes=changes), self.assertRaises(RuntimeError):
                app.validate_sota_transport(proof(**changes), "192.168.1.158", "192.168.1.142")

    def test_upload_uses_uds_sequence_after_same_socket_proof(self):
        client = Client()
        with patch.object(app, "DoipClient", return_value=client):
            self.assertEqual(app.sota_request("192.168.1.158", 30520, app.SOTA_UPLOAD_NEW, b"abc"),
                             "OK STAGED NEW 3")
        self.assertGreaterEqual(client.request_spacing_seconds, .1)
        self.assertEqual([q[0] for q in client.sent], [0x22, 0x10, 0x10, 0x31, 0x34, 0x36, 0x37, 0x10])
        self.assertEqual(client.sent[5], b"\x36\x01abc")
        self.assertEqual(client.sent[6], b"\x37\x35\x24\x41\xc2")
        self.assertTrue(client.closed)

    def test_wrong_interface_sends_no_upload_and_does_not_retry_ethernet(self):
        client = Client(proof(interface="eth0"))
        with patch.object(app, "DoipClient", return_value=client) as create:
            with self.assertRaises(RuntimeError):
                app.sota_request("192.168.1.158", 30520, app.SOTA_UPLOAD_NEW, b"abc")
        create.assert_called_once()
        self.assertEqual(client.sent, [b"\x22\xf0\xa0"])

    def test_connection_failure_has_no_fallback(self):
        client = Client()
        client.connect = Mock(side_effect=TimeoutError("Wi-Fi down"))
        with patch.object(app, "DoipClient", return_value=client) as create:
            with self.assertRaises(TimeoutError):
                app.sota_request("192.168.1.158", 30520, app.SOTA_STATUS)
        create.assert_called_once()
        self.assertTrue(client.closed)

    def test_upload_rejects_wrong_counter_echo(self):
        client = Client()
        original = client.send_uds
        client.send_uds = lambda q, timeout=None: b"\x76\x02" if q[0] == 0x36 else original(q, timeout)
        with patch.object(app, "DoipClient", return_value=client), self.assertRaisesRegex(RuntimeError, "counter"):
            app.sota_request("192.168.1.158", 30520, app.SOTA_UPLOAD_NEW, b"abc")
        self.assertFalse(any(q[0] == 0x37 for q in client.sent))

    def test_persistent_status_preserves_actual_failure_and_progress(self):
        result = app.parse_sota_status('OK {"state":"RECOVERY_REQUIRED","completed_bytes":"1024","total_bytes":"2048"}')
        self.assertEqual(result["state"], "RECOVERY_REQUIRED")
        self.assertEqual(int(result["completed_bytes"]), 1024)
        with self.assertRaises(RuntimeError):
            app.parse_sota_status("ERROR update unavailable")
        with self.assertRaises(RuntimeError):
            app.parse_sota_status("OK {}")


if __name__ == "__main__":
    unittest.main()
