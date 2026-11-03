# SPDX-License-Identifier: GPL-3.0-or-later
"""Non-TU visible inventory contracts; synthetic, not live equipment proof."""
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import CHALLENGE, EPOCH
from test_agent_inventory_control import inventory, projection

class PremissionInventoryTests(unittest.TestCase):
    def test_painted_controls_without_tu_text(self):
        visible=projection();visible['time_units']=None
        obj=inventory(visible)
        a.validate_state(obj,CHALLENGE,EPOCH)
        self.assertIn('inventory_activate',{x['id'] for x in obj['actions']})
        self.assertEqual(obj['visible']['selected_name'],'Jane')

    def test_hidden_tu_does_not_license_model_fields(self):
        for key in ('tu_mode','actual_tu','unit_id','ground_items'):
            visible=projection();visible['time_units']=None;visible[key]=0
            with self.subTest(key=key),self.assertRaises(Unavailable):
                a.validate_state(inventory(visible),CHALLENGE,EPOCH)

    def test_non_tu_still_requires_fresh_visible_target(self):
        obj=inventory();obj['visible']['time_units']=None;obj['visible']['targets']=[]
        with self.assertRaises(Unavailable):a.validate_state(obj,CHALLENGE,EPOCH)

if __name__=='__main__':unittest.main()
