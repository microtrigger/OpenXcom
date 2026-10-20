#!/usr/bin/env python3
"""Source-only /12 paging contracts; not runtime or paint acceptance."""
from pathlib import Path
import unittest
import yaml
from test_inventory_routing_architecture import function, body, tokens

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BASELINE = ROOT.parent.parent/'agent-control-inventory-v11b-20261018/source'


class InventoryPagingArchitectureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.inventory = (ROOT/'src/Battlescape/Inventory.cpp').read_text()
        cls.state = (ROOT/'src/Battlescape/InventoryState.cpp').read_text()
        cls.ui = (ROOT/'src/Observer/AgentUi.cpp').read_text()

    def test_packing_and_stacking_are_unchanged_ordinary_methods(self):
        old = (BASELINE/'src/Battlescape/Inventory.cpp').read_text()
        for name in ('arrangeGround', 'canBeStacked', 'fitItem', 'unload', 'overlapItems',
                     'moveItem', 'movePointer', 'activateButton', 'returnHeldItem'):
            with self.subTest(method=name):
                self.assertEqual(function(old,'Inventory::'+name),
                                 function(self.inventory,'Inventory::'+name))

    def test_actual_arrangement_and_only_draw_shim(self):
        fixture = (HERE/'AgentInventoryPagingTest.cpp').read_text()
        runner = (HERE/'run_inventory_paging.py').read_text()
        self.assertIn('void arrangeGround(bool=true);',fixture)
        self.assertNotIn('void arrangeGround(bool=true) {',fixture)
        self.assertIn("'fitItem', 'arrangeGround'",runner)
        self.assertIn("'canBeStacked'",runner)
        self.assertIn('inputs_unchanged_after_run',runner)
        self.assertIn('original_sha256',runner)

    def test_public_stock_layouts_supply_all_body_geometry(self):
        for game in ('xcom1','xcom2'):
            data=yaml.safe_load((ROOT/'bin/standard'/game/'inventories.rul').read_text())['invs']
            slots={s['id']:s for s in data}
            self.assertEqual(len(slots),9)
            self.assertEqual((slots['STR_GROUND']['x'],slots['STR_GROUND']['y']),(0,152))
            self.assertEqual(slots['STR_BELT']['slots'],[[0,0],[1,0],[2,0],[3,0],[0,1],[3,1]])
            self.assertEqual(len(slots['STR_BACK_PACK']['slots']),9)

    def test_page_action_still_uses_only_normal_handler(self):
        dispatch=body(self.state,'InventoryState::agentDispatch','const std::string &action')
        self.assertIn('action == "next_inventory_ground_page") btnGroundClick(nullptr);',dispatch)
        self.assertNotIn('arrangeGround',dispatch)
        normal=body(self.state,'InventoryState::btnGroundClick')
        self.assertIn('_inv->arrangeGround();',normal)
        old=(BASELINE/'src/Battlescape/InventoryState.cpp').read_text()
        self.assertEqual(tokens(normal),tokens(body(old,'InventoryState::btnGroundClick')))

    def test_provider_never_counts_offscreen_items_or_computes_pages(self):
        provider=body(self.state,'InventoryState::agentUiState')
        for forbidden in ('_groundOffset','_stackLevel','getInventory(', 'getTile(',
                          'getAmmoQuantity(', 'getRules(', 'canBeStacked(',
                          'page_count','stack_count','item_count','total_pages'):
            self.assertNotIn(forbidden,provider)
        self.assertIn('"next_inventory_ground_page"',provider)
        self.assertIn('_inv->_agentInventoryRevision',provider)
        self.assertIn('out.widgets.push_back(std::make_pair(_inv, _inv->_agentInventoryRevision));',provider)

    def test_private_bindings_are_excluded_from_actual_payload(self):
        payload=body(self.ui,'AgentUiState::payload')
        for forbidden in ('actor','save','widgets','context','rows','points','Revision','groundOffset'):
            self.assertNotIn(forbidden,payload)
        binding=body(self.ui,'AgentUiState::sameBinding')
        self.assertIn('widgets == o.widgets',binding)
        arrangement=body(self.inventory,'Inventory::arrangeGround')
        self.assertIn('++_agentInventoryRevision;',arrangement)
        self.assertIn('if (alterOffset)',arrangement)
        self.assertIn('xMax >= _groundOffset + slotsX',arrangement)


if __name__ == '__main__':
    unittest.main(verbosity=2)
