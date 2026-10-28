# SPDX-License-Identifier: GPL-3.0-or-later
"""Cannot Reequip painted captions and bounded controls, not re-equipment outcomes."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, CHALLENGE, EPOCH


class CannotReequipTests(unittest.TestCase):
    def shown(self):
        return state(screen='cannot_reequip')

    def checked(self, obj):
        return a.validate_state(obj, CHALLENGE, EPOCH)

    def test_catalog_and_exact_shape(self):
        self.assertEqual(a.SCHEMA, 'openxcom-agent-control/47')
        self.assertEqual(a.STATE_ACTIONS['cannot_reequip'], frozenset(('acknowledge_cannot_reequip','scroll_visible_list')))
        obj = self.shown(); self.checked(obj)
        for key in obj['visible']:
            bad = copy.deepcopy(obj); del bad['visible'][key]
            with self.assertRaises(Unavailable): self.checked(bad)
        for key in ('total', 'craft_id', 'item_id', 'selected', 'missing_count', 'success'):
            bad = self.shown(); bad['visible'][key] = 1
            with self.assertRaises(Unavailable): self.checked(bad)

    def test_nullable_captions_and_quantity_is_text(self):
        for value in (None, '', 'Shown\ncaption', 'é'*512):
            obj = self.shown(); obj['visible'].update(title=value, headings=[value]*3, rows=[dict(cells=[value]*3)])
            self.checked(obj)
        for value in (True, 1, [], {}, 'é'*513):
            for field in ('title', 'headings', 'rows'):
                obj = self.shown()
                obj['visible'][field] = value if field == 'title' else [value]*3 if field == 'headings' else [dict(cells=['Rifle', value, 'Craft'])]
                with self.subTest(field=field, value=value), self.assertRaises(Unavailable): self.checked(obj)
        for headings in (None, {}, [], ['a'], ['a']*4):
            obj=self.shown(); obj['visible']['headings']=headings
            with self.assertRaises(Unavailable): self.checked(obj)

    def test_rows_readonly_exact_three_cells_bounded(self):
        for rows in (None, {}, [dict(cells=[])], [dict(cells=['a']*4)], [dict(cells=['a']*3)]*129):
            obj=self.shown(); obj['visible']['rows']=rows
            with self.assertRaises(Unavailable): self.checked(obj)
        for key in ('handle','index','selected','logicalIndex','item_id','craft_id','total'):
            obj=self.shown(); obj['visible']['rows'][0][key]=0
            with self.assertRaises(Unavailable): self.checked(obj)
        obj=self.shown(); obj['visible']['rows']=[]; self.checked(obj)

    def test_buttons_labels_and_scroll_booleans(self):
        for buttons in (None, {}, [dict(id='ok',label=None)], [dict(id='ok',label='')],
                        [dict(id='ok',label='Ok',hidden=1)], [dict(id='cancel',label='Cancel')],
                        [dict(id='ok',label='Ok')]*2):
            obj=self.shown(); obj['visible']['buttons']=buttons
            with self.assertRaises(Unavailable): self.checked(obj)
        obj=self.shown(); obj['actions'][0]['label']='invented'
        with self.assertRaises(Unavailable): self.checked(obj)
        obj=self.shown(); obj['visible']['buttons']=[]
        with self.assertRaises(Unavailable): self.checked(obj)
        obj['actions']=[x for x in obj['actions'] if x['id']!='acknowledge_cannot_reequip']; self.checked(obj)
        for scroll in (None, {}, dict(up=1,down=False), dict(up=False,down=None), dict(up=True,down=True,extra=True), dict(up=False,down=False)):
            obj=self.shown();obj['visible']['scroll']=scroll
            with self.assertRaises(Unavailable): self.checked(obj)
        obj=self.shown();obj['visible']['scroll']=dict(up=False,down=False)
        obj['actions']=[x for x in obj['actions'] if x['id']!='scroll_visible_list'];self.checked(obj)

    def test_screen_scoping_and_extra_arguments(self):
        for action in set(a.ACTIONS)-{'acknowledge_cannot_reequip','scroll_visible_list','move_cursor'}:
            obj=self.shown();obj['actions']=[dict(id=action,label='Shown',label_source=a.label_source('cannot_reequip',action),argument_kind=a.ARGUMENT_KINDS[action])]
            with self.subTest(action=action), self.assertRaises(Unavailable):self.checked(obj)
        for args in ({'row':'r0'}, {'count':2}, {'confirm':True}, [], None):
            request=arguments(action='acknowledge_cannot_reequip',args={});request['args']=args
            with self.assertRaises(Unavailable):a.validate_action_arguments(request)
        self.assertTrue(a.canonical_action_request(arguments(action='acknowledge_cannot_reequip',args={})).endswith(b'acknowledge_cannot_reequip'))

    def test_fresh_single_use_ok_and_scroll_direction_authority(self):
        for action,args in [('acknowledge_cannot_reequip',{}),('scroll_visible_list',{'direction':'down'})]:
            peer=NativeFixture(screen='cannot_reequip');self.addCleanup(peer.close)
            old=peer.client.observe();fresh=peer.client.observe()
            self.assertFalse(peer.client.visible_rows)
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action=action,args=args))
            self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))
            request=arguments(fresh,action=action,args=args);receipt=peer.client.execute(**request)
            self.assertEqual(receipt['status'],'dispatched');self.assertEqual(peer.client.execute(**request),receipt)
            self.assertEqual(sum(p.startswith(b'E ') for p in peer.requests),1)
        def transform(op,obj):
            if op=='R':obj['visible']['scroll']['up']=False
            return obj
        peer=NativeFixture(screen='cannot_reequip',transform=transform);self.addCleanup(peer.close)
        obj=peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='scroll_visible_list',args={'direction':'up'}))
        self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))


if __name__ == '__main__': unittest.main()
