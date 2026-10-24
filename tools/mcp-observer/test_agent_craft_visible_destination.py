# SPDX-License-Identifier: GPL-3.0-or-later
"""Visible-marker protocol fixtures; native paint/fog/lifetime proof is separate."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, CHALLENGE, EPOCH

ACTION = 'choose_visible_craft_destination'

class VisibleCraftDestinationTests(unittest.TestCase):
    def checked(self, obj):
        return a.validate_state(obj, CHALLENGE, EPOCH)

    def test_exact_shape_and_canvas_bounds(self):
        obj = state(screen='craft_destination_selection'); self.checked(obj)
        for key, values in {'row': ('r1', 'r32', 0, None), 'x': (-1, 640, True, 1.0),
                            'y': (-1, 400, False), 'width': (0, 541, -1),
                            'height': (0, 331, -1)}.items():
            for value in values:
                bad = copy.deepcopy(obj); bad['visible']['targets'][0][key] = value
                with self.subTest(key=key, value=value), self.assertRaises(Unavailable): self.checked(bad)
        edge = copy.deepcopy(obj); edge['visible']['targets'] = [dict(row='r0', x=639, y=399, width=1, height=1)]
        self.checked(edge)
        for key in ('name', 'target_id', 'world_x', 'race', 'hidden', 'detected'):
            bad = copy.deepcopy(obj); bad['visible']['targets'][0][key] = 1
            with self.assertRaises(Unavailable): self.checked(bad)
        for key in obj['visible']['targets'][0]:
            bad = copy.deepcopy(obj); del bad['visible']['targets'][0][key]
            with self.assertRaises(Unavailable): self.checked(bad)

    def test_sequential_handles_and_action_presence(self):
        obj = state(screen='craft_destination_selection')
        obj['visible']['targets'] = [dict(row='r'+str(i), x=i*5, y=40, width=4, height=4) for i in range(32)]
        self.checked(obj)
        bad = copy.deepcopy(obj); bad['visible']['targets'].append(dict(row='r32', x=160, y=40, width=4, height=4))
        with self.assertRaises(Unavailable): self.checked(bad)
        bad = copy.deepcopy(obj); bad['visible']['targets'][1]['row'] = 'r0'
        with self.assertRaises(Unavailable): self.checked(bad)
        obj['visible']['targets'] = []
        with self.assertRaises(Unavailable): self.checked(obj)
        obj['actions'] = [x for x in obj['actions'] if x['id'] != ACTION]; self.checked(obj)
        obj['visible']['targets'] = [dict(row='r0', x=0, y=40, width=4, height=4)]
        with self.assertRaises(Unavailable): self.checked(obj)

    def test_exact_semantic_label_and_argument_shape(self):
        obj = state(screen='craft_destination_selection')
        for key, value in (('label', 'Hidden UFO name'), ('label_source', 'visible_text'), ('argument_kind', 'none')):
            bad = copy.deepcopy(obj); next(x for x in bad['actions'] if x['id'] == ACTION)[key] = value
            with self.assertRaises(Unavailable): self.checked(bad)
        req = arguments(action=ACTION, args={'row':'r0'})
        self.assertTrue(a.canonical_action_request(req).endswith(b'choose_visible_craft_destination r0'))
        for args in ({}, {'row':'r32'}, {'row':0}, {'row':'r0','x':0}, {'target_id':1}):
            with self.assertRaises(Unavailable): a.validate_action_arguments(arguments(action=ACTION,args=args))

    def test_neighbour_cannot_advertise_marker_action(self):
        descriptor = next(x for x in state(screen='craft_destination_selection')['actions'] if x['id'] == ACTION)
        for screen in ('geoscape','geoscape_craft_navigation','landing_confirmation','unsupported'):
            obj=state(screen=screen); obj['actions']=[descriptor]
            with self.subTest(screen=screen), self.assertRaises(Unavailable): self.checked(obj)

    def test_fresh_visible_row_authority_and_receipt_only(self):
        peer=NativeFixture(screen='craft_destination_selection'); self.addCleanup(peer.close)
        old=peer.client.observe(); fresh=peer.client.observe()
        with self.assertRaises(Unavailable): peer.client.execute(**arguments(old,action=ACTION,args={'row':'r0'}))
        with self.assertRaises(Unavailable): peer.client.execute(**arguments(fresh,action=ACTION,args={'row':'r1'}))
        self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
        request=arguments(fresh,action=ACTION,args={'row':'r0'})
        receipt=peer.client.execute(**request)
        self.assertEqual(receipt['status'],'dispatched')
        self.assertEqual(peer.client.execute(**request),receipt)
        self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),1)
        self.assertNotIn('arrived', receipt)

if __name__ == '__main__': unittest.main()
