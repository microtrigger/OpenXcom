# SPDX-License-Identifier: GPL-3.0-or-later
"""The shared row guard retains fixed pre-send reasons for newer UI controls."""
import unittest
import adapter
from test_agent_control import NativeFixture, arguments, call

class RowTargetClassificationTests(unittest.TestCase):
    def test_all_visible_row_controls_reject_unoffered_target_without_send(self):
        cases = (
            ('new_battle_dropdown', 'choose_visible_row'),
            ('psi_training', 'open_psi_training_base'),
            ('craft_destination_selection', 'choose_visible_craft_destination'),
            ('load_list', 'load_saved_game'),
        )
        for screen, action in cases:
            with self.subTest(screen=screen, action=action):
                native = NativeFixture(screen=screen)
                try:
                    observed = native.client.observe()
                    self.assertIn(action, native.client.available_actions)
                    native.client.visible_rows = frozenset()
                    response = call(adapter.Server(agent_control=native.client),
                                    'execute_ui_action', arguments(observed, action=action, args={'row':'r0'}))['result']
                    self.assertTrue(response['isError'])
                    failure = response['structuredContent']['failure']
                    self.assertEqual(failure['code'], 'target_not_offered')
                    self.assertEqual(failure['action_outcome'], 'not_sent')
                    self.assertFalse(any(packet.startswith(b'E ') for packet in native.requests))
                finally:
                    native.close()

if __name__ == '__main__':
    unittest.main()
