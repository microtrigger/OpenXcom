# SPDX-License-Identifier: GPL-3.0-or-later
"""Wire62 painted craft Inventory entry; fake transport, not live gameplay."""
import copy
from pathlib import Path
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, CHALLENGE, EPOCH
from test_agent_events import page, validate

ACTION = 'open_craft_inventory'

class CraftInventoryTests(unittest.TestCase):
    def checked(self, obj):
        return a.validate_state(obj, CHALLENGE, EPOCH)

    def test_closed_catalog_and_no_arguments(self):
        self.assertEqual(a.SCHEMA, 'openxcom-agent-control/66')
        self.assertEqual(len(a.ACTIONS), 286)
        self.assertEqual([s for s, ids in a.STATE_ACTIONS.items() if ACTION in ids], ['craft_equipment'])
        self.assertEqual(a.ARGUMENT_KINDS[ACTION], 'none')
        obj = state(screen='craft_equipment')
        self.checked(obj)
        self.assertTrue(a.canonical_action_request(arguments(obj, action=ACTION, args={})).endswith(ACTION.encode()))
        for extra in ({'craft': 0}, {'unit': 1}, {'repeat': 2}):
            with self.assertRaises(Unavailable):
                a.canonical_action_request(arguments(obj, action=ACTION, args=extra))

    def test_schema60_is_rejected(self):
        obj = state(screen='craft_equipment')
        obj['schema'] = 'openxcom-agent-control/60'
        with self.assertRaises(Unavailable): self.checked(obj)

    def test_absent_button_cannot_authorize_action(self):
        obj = state(screen='craft_equipment')
        obj['visible']['buttons'] = [b for b in obj['visible']['buttons'] if b['id'] != 'inventory']
        with self.assertRaises(Unavailable): self.checked(obj)
        obj['actions'] = [x for x in obj['actions'] if x['id'] != ACTION]
        self.checked(obj)

    def test_labels_and_order_remain_closed(self):
        for mutation in ('label', 'source', 'order', 'unknown'):
            obj = state(screen='craft_equipment')
            entry = next(x for x in obj['actions'] if x['id'] == ACTION)
            if mutation == 'label': entry['label'] = 'Not the painted label'
            if mutation == 'source': entry['label_source'] = 'visible_control_semantics'
            if mutation == 'order': obj['visible']['buttons'].reverse()
            if mutation == 'unknown': obj['visible']['buttons'][0]['eligible'] = True
            with self.subTest(mutation=mutation), self.assertRaises(Unavailable): self.checked(obj)

    def test_visible_events_and_unknown_text(self):
        obj = state(screen='craft_equipment')
        for k in ('title', 'available', 'used', 'crew'): obj['visible'][k] = None
        self.checked(obj)
        event = dict(seq=1, kind='presentation', state=obj['state'], visible=copy.deepcopy(obj['visible']))
        self.assertEqual(validate(page([event]))['events'][0]['visible'], obj['visible'])

    def test_one_dispatch_and_duplicate_receipt_not_second_mutation(self):
        peer = NativeFixture(screen='craft_equipment')
        self.addCleanup(peer.close)
        obj = peer.client.observe()
        request = arguments(obj, action=ACTION, args={})
        result = peer.client.execute(**request)
        self.assertEqual((result['status'], result['semantics']), ('dispatched', 'dispatch_not_success'))
        self.assertEqual(peer.client.execute(**request), result)
        self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests), 1)

    def test_stale_capability_no_dispatch(self):
        peer = NativeFixture(screen='craft_equipment')
        self.addCleanup(peer.close)
        old = peer.client.observe()
        peer.revision += 1
        peer.client.observe()
        before = len(peer.requests)
        with self.assertRaises(Unavailable): peer.client.execute(**arguments(old, action=ACTION, args={}))
        self.assertEqual(len(peer.requests), before)

    def test_native_entry_retains_painted_guard_and_stock_handler(self):
        root = Path(__file__).resolve().parents[2]
        native = (root/'src/Basescape/CraftEquipmentState.cpp').read_text()
        ui = native[native.index('bool CraftEquipmentState::agentUiState'):native.index('bool CraftEquipmentState::agentDispatch')]
        dispatch = native[native.index('bool CraftEquipmentState::agentDispatch'):]
        self.assertIn('view.button(_btnInventory,"inventory","open_craft_inventory");', ui)
        self.assertIn('if (!agentUiState(current) || !current.offers(request.action)) return false;', dispatch)
        self.assertIn('if(request.action=="open_craft_inventory"){btnInventoryClick(nullptr);return true;}', dispatch)
        self.assertIn('_btnInventory->setVisible(craftHasACrew && !isNewBattle)', native)
        self.assertIn('new InventoryState(false, 0)', native)
        protocol = (root/'src/Observer/AgentProtocol.cpp').read_text()
        self.assertIn('value == "open_craft_inventory" ||', protocol)
        self.assertNotIn('openxcom-agent-control/60', protocol)

if __name__ == '__main__': unittest.main()
