# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Bounded synthetic diagnostic sentinels across the real launcher process."""
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

import adapter
import launch_presentation

ROOT = Path(__file__).resolve().parent
SENTINEL = 'PRIVATE-DIAGNOSTIC-SENTINEL'
INITIALIZE = {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize',
              'params': {'protocolVersion': adapter.PROTOCOL, 'capabilities': {},
                         'clientInfo': {'name': 'synthetic', 'version': '1'}}}
READY = {'jsonrpc': '2.0', 'method': 'notifications/initialized'}
CALL = {'jsonrpc': '2.0', 'id': 2, 'method': 'tools/call',
        'params': {'name': 'read_presented_cursor', 'arguments': {}}}
INPUT = b''.join(json.dumps(obj).encode() + b'\n' for obj in (INITIALIZE, READY, CALL))
CHILD = r'''
import atexit, json, os, socket, sys
sentinel = b'PRIVATE-DIAGNOSTIC-SENTINEL\xff'
def diagnostics():
    for fd in (1, 2):
        os.write(fd, sentinel)
        for _ in range(256): os.write(fd, b'\xff' * 8192)
atexit.register(diagnostics)
diagnostics()
mode = sys.argv[1]
if mode == 'failure': sys.exit(23)
channel = socket.socket(fileno=int(os.environ['OPENXCOM_PRESENTATION_FD']))
if mode == 'invalid_hello': channel.send(sentinel); sys.exit(0)
epoch, admission = '1' * 32, '2' * 32
channel.send(('OPENXCOM-PRESENTATION/1 ' + epoch + admission).encode())
request = channel.recv(97).decode('ascii')
diagnostics()
reply = dict(schema='experimental-presentation/1', epoch=epoch, challenge=request[64:],
             next_token='3' * 32, semantics='presented_after_request',
             profile='software-single-buffer-8bit-integer-scale-no-bands',
             crop=dict(screen_width=320, screen_height=200, x=0, y=0, width=1, height=1,
                       format='rgb24-hex-row-major', pixels='010203'))
if mode == 'invalid_utf8': raw = sentinel
elif mode == 'oversized': raw = sentinel + b'x' * 110000
elif mode == 'hidden_field': reply['private_detail'] = sentinel.hex(); raw = json.dumps(reply).encode()
else: raw = json.dumps(reply).encode()
channel.send(raw)
'''


class DiagnosticIsolationTests(unittest.TestCase):
    def run_launcher(self, command, data=INPUT, flags=()):
        result = subprocess.run([sys.executable, str(ROOT / 'launch_presentation.py'),
                                 *flags, '--', *command], input=data, capture_output=True, timeout=8)
        self.assertNotIn(SENTINEL.encode(), result.stdout + result.stderr)
        self.assertNotIn(b'Traceback', result.stdout + result.stderr)
        self.assertLessEqual(len(result.stderr), 128)
        self.assertLessEqual(len(result.stdout), adapter.MAX_RESPONSE * 2)
        return result

    def assert_reply(self, result, available):
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stderr, b'')
        replies = [json.loads(line) for line in result.stdout.splitlines()]
        self.assertEqual(len(replies), 2)
        self.assertEqual(replies[0]['id'], 1)
        self.assertEqual(replies[1]['id'], 2)
        self.assertEqual(replies[1]['result']['isError'], not available)
        if not available:
            self.assertEqual(replies[1]['result']['content'], [{'type': 'text', 'text':
                '{"available":false,"reason":"observation_unavailable"}'}])

    def test_sentinel_positive_control_is_present_without_launcher(self):
        result = subprocess.run([sys.executable, '-c', CHILD, 'failure'],
                                capture_output=True, timeout=3)
        self.assertEqual(result.returncode, 23)
        for output in (result.stdout, result.stderr):
            self.assertIn(SENTINEL.encode(), output)
            self.assertIn(b'\xff', output)
            self.assertGreater(len(output), 4 * 1024 * 1024)
            self.assertLess(len(output), 5 * 1024 * 1024)

    def test_startup_runtime_shutdown_invalid_bytes_and_large_output_discarded(self):
        self.assert_reply(self.run_launcher([sys.executable, '-c', CHILD, 'valid']), True)

    def test_child_failure_is_generic_unavailable(self):
        self.assert_reply(self.run_launcher([sys.executable, '-c', CHILD, 'failure']), False)

    def test_invalid_child_hello_is_generic_unavailable(self):
        self.assert_reply(self.run_launcher([sys.executable, '-c', CHILD, 'invalid_hello']), False)

    def test_invalid_child_utf8_is_generic_unavailable(self):
        self.assert_reply(self.run_launcher([sys.executable, '-c', CHILD, 'invalid_utf8']), False)

    def test_oversized_child_packet_is_generic_unavailable(self):
        self.assert_reply(self.run_launcher([sys.executable, '-c', CHILD, 'oversized']), False)

    def test_extra_child_secret_field_is_generic_unavailable(self):
        self.assert_reply(self.run_launcher([sys.executable, '-c', CHILD, 'hidden_field']), False)

    def test_missing_executable_hides_path_and_exception(self):
        result = self.run_launcher(['/nonexistent/' + SENTINEL], data=b'')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, b'')
        self.assertEqual(result.stderr, b'Observer launch or transport unavailable.\n')

    def test_invalid_launcher_arguments_hide_values(self):
        for args in (['--' + SENTINEL], ['--hud-dir']):
            result = subprocess.run([sys.executable, str(ROOT / 'launch_presentation.py'), *args],
                                    input=b'', capture_output=True, timeout=3)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, b'')
            self.assertEqual(result.stderr, b'Invalid observer launch arguments; use --help.\n')

    def test_invalid_standalone_arguments_hide_values(self):
        result = subprocess.run([sys.executable, str(ROOT / 'adapter.py'), '--' + SENTINEL],
                                input=b'', capture_output=True, timeout=3)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, b'')
        self.assertEqual(result.stderr, b'Invalid observer arguments; use --help.\n')

    def test_eof_does_not_echo_python_live_child_warning(self):
        result = subprocess.run([sys.executable, '-Walways', str(ROOT / 'launch_presentation.py'),
                                 '--', sys.executable, '-c', 'import time; time.sleep(.2)'],
                                input=b'', capture_output=True, timeout=3)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b'')
        self.assertEqual(result.stderr, b'')

    def test_child_streams_have_no_pipes_or_inherited_model_stream(self):
        with patch.object(sys, 'argv', ['launcher', '--', 'synthetic-game']), \
                patch('launch_presentation.subprocess.Popen') as spawn, \
                patch('launch_presentation.serve'):
            self.assertIsNone(launch_presentation.main())
        for name in ('stdin', 'stdout', 'stderr'):
            self.assertEqual(spawn.call_args.kwargs[name], subprocess.DEVNULL)
        spawn.return_value.wait.assert_not_called()
        spawn.return_value.communicate.assert_not_called()
        spawn.return_value.kill.assert_not_called()
        spawn.return_value.terminate.assert_not_called()

    def test_socket_and_spawn_exceptions_are_bounded_generic(self):
        for target in ('launch_presentation.socket.socketpair', 'launch_presentation.subprocess.Popen'):
            with self.subTest(target=target), patch.object(sys, 'argv', ['launcher', '--', 'game']), \
                    patch(target, side_effect=OSError(SENTINEL)), \
                    patch('launch_presentation.serve') as serve, \
                    contextlib.redirect_stderr(io.StringIO()) as err:
                self.assertEqual(launch_presentation.main(), 1)
                self.assertEqual(err.getvalue(), 'Observer launch or transport unavailable.\n')
                serve.assert_not_called()

    def test_serve_and_cleanup_exceptions_never_echo_private_detail(self):
        for target in ('serve', 'cleanup'):
            with self.subTest(target=target), patch.object(sys, 'argv', ['launcher', '--', 'game']), \
                    patch('launch_presentation.subprocess.Popen') as spawn, \
                    patch('launch_presentation.serve') as serve, \
                    contextlib.redirect_stderr(io.StringIO()) as err:
                if target == 'serve': serve.side_effect = RuntimeError(SENTINEL)
                else: spawn.return_value.poll.side_effect = OSError(SENTINEL)
                self.assertEqual(launch_presentation.main(), 1)
                self.assertEqual(err.getvalue(), 'Observer launch or transport unavailable.\n')

    def test_real_broken_model_pipe_does_not_emit_shutdown_diagnostics(self):
        for script, extra in (('adapter.py', []), ('launch_presentation.py',
                              ['--', sys.executable, '-c', 'pass'])):
            with self.subTest(script=script):
                process = subprocess.Popen([sys.executable, str(ROOT / script), *extra],
                                           stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                           stderr=subprocess.PIPE)
                process.stdout.close()
                process.stdin.write(b'{"jsonrpc":"2.0","id":1,"method":"ping"}\n')
                process.stdin.close()
                self.assertEqual(process.wait(timeout=3), 0)
                self.assertEqual(process.stderr.read(), b'')
                process.stderr.close()

    def test_broken_model_pipe_is_quiet(self):
        with patch.object(sys, 'argv', ['launcher', '--', 'game']), \
                patch('launch_presentation.subprocess.Popen'), \
                patch('launch_presentation.serve', side_effect=BrokenPipeError(SENTINEL)), \
                patch('launch_presentation.silence_broken_stdout'), \
                contextlib.redirect_stderr(io.StringIO()) as err:
            self.assertEqual(launch_presentation.main(), 0)
            self.assertEqual(err.getvalue(), '')

    @unittest.skipUnless(os.environ.get('OPENXCOM_DIAGNOSTIC_TEST_CHILD'),
                         'native Logger positive control is exercised by the native runner')
    def test_native_logger_positive_control_proves_failed_file_fallback(self):
        result = subprocess.run([os.environ['OPENXCOM_DIAGNOSTIC_TEST_CHILD'], '--logger-child'],
                                capture_output=True, timeout=3,
                                env={k: v for k, v in os.environ.items()
                                     if k != 'OPENXCOM_PRESENTATION_FD'})
        self.assertEqual(result.returncode, 1)
        self.assertIn(b'[INFO]\t' + SENTINEL.encode(), result.stderr)
        self.assertIn(SENTINEL.encode(), result.stdout)
        self.assertLess(len(result.stdout) + len(result.stderr), 5 * 1024 * 1024)

    @unittest.skipUnless(os.environ.get('OPENXCOM_DIAGNOSTIC_TEST_CHILD'),
                         'native Logger sentinel is exercised by tests/diagnostic-isolation/run.sh')
    def test_actual_native_logger_failed_file_isolated_at_info_level(self):
        self.assert_reply(self.run_launcher([os.environ['OPENXCOM_DIAGNOSTIC_TEST_CHILD'],
                                            '--logger-child']), True)


if __name__ == '__main__':
    unittest.main()
