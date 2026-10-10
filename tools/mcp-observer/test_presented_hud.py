# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Synthetic displayed-HUD protocol tests; no game process or assets required."""
import json
import os
import sys
import unittest
from unittest.mock import patch

import adapter
import launch_presentation
import test_presentation as legacy
from presentation import PresentationClient
from snapshots import Unavailable


class PresentedHudTests(legacy.PresentationTests):
    # Repeat all existing request binding, crop, timeout, replay and rate tests
    # against version 2 as well as the unchanged version 1 fixture.
    VERSION = 2

    def call(self, name='read_presented_hud', **configuration):
        server = adapter.Server(presentation=self.client, **configuration)
        server.state = 'ready'
        return server.handle({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/call',
                              'params': {'name': name, 'arguments': {}}})['result']

    def test_exact_public_hud_envelope(self):
        self.producer()
        result = self.call()
        self.assertFalse(result['isError'])
        self.assertEqual(len(result['content']), 1)
        obj = json.loads(result['content'][0]['text'])
        self.assertEqual(set(obj), {'schema', 'epoch', 'challenge', 'semantics', 'profile', 'hud'})
        self.assertEqual(obj['schema'], 'openxcom-mcp-presented-hud/1')
        self.assertEqual(obj['semantics'], 'presented_after_request')
        self.assertEqual(obj['hud'], dict(time_units=58, energy=62, health=33, morale=100))
        self.assertEqual(obj['epoch'], legacy.EPOCH)
        self.assertRegex(obj['challenge'], '^[0-9a-f]{32}$')
        self.assertEqual(obj['profile'], legacy.PROFILE)

    def test_null_hud_is_generic_successful_observation(self):
        self.producer(lambda o: o.update(hud=None))
        result = self.call()
        self.assertFalse(result['isError'])
        obj = json.loads(result['content'][0]['text'])
        self.assertIsNone(obj['hud'])
        self.assertNotIn('reason', obj)
        self.assertEqual(set(obj), {'schema', 'epoch', 'challenge', 'semantics', 'profile', 'hud'})

    def test_four_displayed_integer_boundaries(self):
        hud = dict(time_units=0, energy=999, health=999, morale=0)
        self.producer(lambda o: o.update(hud=hud))
        obj, candidate = self.client.observe()
        self.assertEqual(obj['hud'], hud)
        self.assertEqual(candidate['crop'], legacy.CROP)

    def test_hud_tool_never_reads_historical_files(self):
        self.producer()
        with patch('snapshots.read_candidate', side_effect=AssertionError('historical read')):
            result = self.call(hud_dir='unused', cursor_dir='unused')
        self.assertFalse(result['isError'])

    def test_failed_hud_never_falls_back_to_history(self):
        self.producer(lambda o: o.update(challenge='4' * 32))
        with patch('snapshots.read_candidate', side_effect=AssertionError('historical fallback')):
            result = self.call(hud_dir='unused')
        self.assertTrue(result['isError'])
        self.assertEqual(json.loads(result['content'][0]['text']),
                         {'available': False, 'reason': 'observation_unavailable'})

    def test_invalid_crop_rejects_otherwise_valid_hud(self):
        self.producer(lambda o: o['crop'].update(pixels='ff'))
        self.assertTrue(self.call()['isError'])

    def test_invalid_hud_rejects_cursor_too(self):
        self.producer(lambda o: o['hud'].update(health=1000))
        self.assertTrue(self.call('read_presented_cursor')['isError'])

    def test_cursor_public_schema_unchanged_under_version_two(self):
        self.producer()
        result = self.call('read_presented_cursor')
        self.assertFalse(result['isError'])
        obj = json.loads(result['content'][0]['text'])
        self.assertEqual(set(obj), {'schema', 'epoch', 'challenge', 'semantics', 'profile', 'crop'})
        self.assertEqual(obj['schema'], 'openxcom-mcp-presented-cursor/1')
        self.assertEqual(result['content'][1]['type'], 'image')

    def test_hud_requests_are_independent_and_bound(self):
        self.producer()
        first = json.loads(self.call()['content'][0]['text'])
        self.client.next_allowed = 0
        self.producer(lambda o: o.update(next_token='4' * 32, hud=None))
        second = json.loads(self.call()['content'][0]['text'])
        self.assertIsNone(second['hud'])
        self.assertNotEqual(first['challenge'], second['challenge'])
        self.assertEqual(first['epoch'], second['epoch'])

    def test_historical_hud_remains_historical_under_version_two(self):
        import test_adapter
        historical = test_adapter.fixture('hud')
        with patch('snapshots.read_candidate', return_value=historical), \
                patch.object(self.client, 'observe', side_effect=AssertionError('fresh request')):
            result = self.call('read_last_hud')
        self.assertEqual(json.loads(result['content'][0]['text']), historical)
        self.assertEqual(historical['freshness'], 'unverified')

    def test_hud_tool_rejects_all_arguments(self):
        server = adapter.Server(presentation=self.client)
        server.state = 'ready'
        for arguments in ({'require_current': True}, {'path': 'unused'}, [], None, True):
            with self.subTest(arguments=arguments):
                reply = server.handle({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/call',
                                       'params': {'name': 'read_presented_hud', 'arguments': arguments}})
                self.assertEqual(reply['error']['code'], -32602)

    def test_version_one_hello_rejected(self):
        self.client.channel.recv(100)
        self.peer.send(('OPENXCOM-PRESENTATION/1 ' + legacy.EPOCH + legacy.ADMISSION).encode())
        self.unavailable()

    def test_unknown_hello_rejected(self):
        self.client.channel.recv(100)
        self.peer.send(('OPENXCOM-PRESENTATION/3 ' + legacy.EPOCH + legacy.ADMISSION).encode())
        self.unavailable()

    def test_version_one_reply_rejected(self):
        def downgrade(obj):
            obj['schema'] = 'experimental-presentation/1'
            del obj['hud']
        self.producer(downgrade)
        self.unavailable()

    def test_unknown_reply_version_rejected(self):
        self.producer(lambda o: o.update(schema='experimental-presentation/3'))
        self.unavailable()

    def test_missing_hud_rejected(self):
        self.producer(lambda o: o.pop('hud'))
        self.unavailable()

    def test_extra_hud_field_rejected(self):
        self.producer(lambda o: o['hud'].update(armor=12))
        self.unavailable()

    def test_missing_hud_field_rejected(self):
        self.producer(lambda o: o['hud'].pop('morale'))
        self.unavailable()

    def test_duplicate_hud_field_rejected(self):
        self.producer(raw=lambda o: json.dumps(o).replace('"health": 33', '"health": 33, "health": 33').encode())
        self.unavailable()

    def test_duplicate_hud_object_rejected(self):
        self.producer(raw=lambda o: (json.dumps(o)[:-1] + ', "hud": null}').encode())
        self.unavailable()

    def test_four_digit_hud_rejected(self):
        self.producer(lambda o: o['hud'].update(morale=9999))
        self.unavailable()


def invalid_hud_value(value):
    def test(self):
        self.producer(lambda o: o['hud'].update(health=value))
        self.unavailable()
    return test


for label, value in [('boolean', True), ('false_boolean', False), ('float', 1.0),
                     ('negative', -1), ('overflow', 1000), ('wide_integer', 2**64),
                     ('string', '33'), ('null', None), ('list', []), ('object', {})]:
    setattr(PresentedHudTests, f'test_hud_value_{label}_rejected', invalid_hud_value(value))


def invalid_hud_container(value):
    def test(self):
        self.producer(lambda o: o.update(hud=value))
        self.unavailable()
    return test


for label, value in [('boolean', True), ('integer', 0), ('string', '0'),
                     ('list', []), ('empty_object', {})]:
    setattr(PresentedHudTests, f'test_hud_container_{label}_rejected', invalid_hud_container(value))


class VersionOneCompatibilityTests(legacy.PresentationTests):
    def test_version_two_hello_rejected_by_default(self):
        self.client.channel.recv(100)
        self.peer.send(('OPENXCOM-PRESENTATION/2 ' + legacy.EPOCH + legacy.ADMISSION).encode())
        self.unavailable()

    def test_version_two_reply_rejected(self):
        self.producer(lambda o: o.update(schema='experimental-presentation/2', hud=None))
        self.unavailable()

    def test_unnegotiated_hud_field_rejected(self):
        self.producer(lambda o: o.update(hud=None))
        self.unavailable()

    def test_fresh_hud_tool_not_advertised_or_callable(self):
        for presentation in (None, self.client):
            server = adapter.Server(presentation=presentation)
            server.state = 'ready'
            listed = server.handle({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/list'})
            self.assertNotIn('read_presented_hud', [t['name'] for t in listed['result']['tools']])
            reply = server.handle({'jsonrpc': '2.0', 'id': 2, 'method': 'tools/call',
                                   'params': {'name': 'read_presented_hud', 'arguments': {}}})
            self.assertEqual(reply['error']['code'], -32602)


class LauncherTests(unittest.TestCase):
    def test_version_configuration_is_strict(self):
        for value in (0, 3, True, 2.0, '2', None):
            with self.subTest(value=value), self.assertRaises(Unavailable):
                # Version validation precedes access to any channel.
                PresentationClient(None, version=value)

    def test_launcher_negotiates_only_explicit_hud_flag(self):
        for flags, expected in (([], 1), (['--presented-hud'], 2)):
            with self.subTest(flags=flags), \
                    patch.object(sys, 'argv', ['launch_presentation.py', *flags, '--', 'synthetic-game']), \
                    patch.dict(os.environ, {'OPENXCOM_PRESENTATION_VERSION': '2'}), \
                    patch('launch_presentation.subprocess.Popen') as spawn, \
                    patch('launch_presentation.serve') as serve:
                launch_presentation.main()
                self.assertEqual(spawn.call_args.args[0], ['synthetic-game'])
                env = spawn.call_args.kwargs['env']
                self.assertEqual(env.get('OPENXCOM_PRESENTATION_VERSION'), '2' if expected == 2 else None)
                self.assertEqual(serve.call_args.args[4].version, expected)
                self.assertIsNone(serve.call_args.args[4].channel)
                spawn.return_value.terminate.assert_not_called()
                spawn.return_value.kill.assert_not_called()


if __name__ == '__main__':
    unittest.main()
