# SPDX-License-Identifier: GPL-3.0-or-later
"""Craft armament wire contracts; synthetic sockets are not native gameplay."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, EPOCH, CHALLENGE
from test_agent_outfitting import all_unknown, event
from test_agent_events import page, validate


class CraftWeaponsContractTests(unittest.TestCase):
    def checked(self, obj):
        return a.validate_state(obj, CHALLENGE, EPOCH)

    def fixture(self, **kw):
        peer = NativeFixture(**kw)
        self.addCleanup(peer.close)
        return peer

    def test_none_row_preserves_native_blanks_and_nullable_padding(self):
        for cells in (['NONE', '', ''], ['NONE', None, None]):
            obj = state(screen='craft_weapons')
            obj['visible']['rows'][0]['cells'] = cells
            self.assertEqual(self.checked(obj)['visible']['rows'][0]['cells'], cells)
            self.assertEqual(validate(page([event(obj)]))['events'][0]['visible']['rows'][0]['cells'], cells)

    def test_clipped_fields_are_nullable_not_synthetic_values(self):
        obj = all_unknown(state(screen='craft_weapons'))
        self.checked(obj)
        self.assertEqual(obj['visible']['headings'], [None] * 3)
        self.assertEqual(obj['visible']['rows'][0]['cells'], [None] * 3)

    def test_exact_width_and_private_fields_fail_closed(self):
        for cells in (['NONE'], ['NONE', '', '', 'secret']):
            obj = state(screen='craft_weapons'); obj['visible']['rows'][0]['cells'] = cells
            with self.assertRaises(Unavailable): self.checked(obj)
        for key in ('weapon_id', 'stock_id', 'available_weapons', 'craft_index', 'slot'):
            obj = state(screen='craft_weapons'); obj['visible'][key] = 0
            with self.assertRaises(Unavailable): self.checked(obj)

    def test_hidden_slots_remove_exact_buttons_and_actions(self):
        for hidden in (('weapon_1',), ('weapon_2',), ('weapon_1', 'weapon_2')):
            obj = state(screen='craft_info')
            obj['visible']['buttons'] = [b for b in obj['visible']['buttons'] if b['id'] not in hidden]
            with self.assertRaises(Unavailable): self.checked(obj)
            obj['actions'] = [x for x in obj['actions'] if x['id'] not in {'open_craft_' + s for s in hidden}]
            self.checked(obj)

    def test_slot_button_action_pair_cannot_be_forged(self):
        for action in ('open_craft_weapon_1', 'open_craft_weapon_2'):
            obj = state(screen='craft_info')
            obj['actions'] = [x for x in obj['actions'] if x['id'] != action]
            with self.assertRaises(Unavailable): self.checked(obj)

    def test_all_new_actions_dispatch_on_their_visible_surfaces(self):
        for screen, action in (('craft_info', 'open_craft_weapon_1'), ('craft_info', 'open_craft_weapon_2'),
                               ('craft_weapons', 'select_craft_weapon'), ('craft_weapons', 'cancel_craft_weapons')):
            peer = self.fixture(screen=screen); obj = peer.client.observe()
            reply = peer.client.execute(**arguments(obj, action=action))
            self.assertEqual(reply['status'], 'dispatched')
            self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests), 1)

    def test_stale_grant_and_unoffered_row_never_send(self):
        peer = self.fixture(screen='craft_weapons'); old = peer.client.observe(); fresh = peer.client.observe()
        with self.assertRaises(Unavailable): peer.client.execute(**arguments(old, action='select_craft_weapon'))
        with self.assertRaises(Unavailable): peer.client.execute(**arguments(fresh, action='select_craft_weapon', args={'row': 'r31'}))
        self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))

    def test_scroll_authority_matches_native_direction_projection(self):
        def transform(op, obj):
            if op == 'R': obj['visible']['scroll'] = {'up': False, 'down': True}
            return obj
        peer = self.fixture(screen='craft_weapons', transform=transform); obj = peer.client.observe()
        with self.assertRaises(Unavailable): peer.client.execute(**arguments(obj, action='scroll_visible_list', args={'direction': 'up'}))
        self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
        reply = peer.client.execute(**arguments(obj, action='scroll_visible_list', args={'direction': 'down'}))
        self.assertEqual(reply['status'], 'dispatched')

    def test_empty_list_cannot_keep_selection_authority(self):
        obj = state(screen='craft_weapons'); obj['visible']['rows'] = []
        with self.assertRaises(Unavailable): self.checked(obj)
        obj['actions'] = [x for x in obj['actions'] if x['id'] != 'select_craft_weapon']
        self.checked(obj)


if __name__ == '__main__':
    unittest.main()
