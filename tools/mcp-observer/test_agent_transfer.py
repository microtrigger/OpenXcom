# SPDX-License-Identifier: GPL-3.0-or-later
"""Transfer public DTO/authority tests; synthetic transport, not gameplay proof."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, EPOCH, CHALLENGE

class TransferTests(unittest.TestCase):
    def checked(self, obj): return a.validate_state(obj, CHALLENGE, EPOCH)
    def fixture(self, **kwargs):
        peer=NativeFixture(**kwargs); self.addCleanup(peer.close); return peer

    def test_screens_exact_fields_no_hidden_capacity(self):
        for screen in a.TRANSFER_BUTTON_ACTIONS:
            obj=state(screen=screen); self.checked(obj)
            for key in obj['visible']:
                bad=copy.deepcopy(obj); del bad['visible'][key]
                with self.subTest(screen=screen,key=key),self.assertRaises(Unavailable):self.checked(bad)
            for key in ('capacity','eligible','item_id','destination_id','max_quantity','category','success'):
                bad=copy.deepcopy(obj);bad['visible'][key]=None
                with self.assertRaises(Unavailable):self.checked(bad)
            for descriptor in obj['actions']:
                bad=copy.deepcopy(obj);bad['actions'][obj['actions'].index(descriptor)]['label']='fabricated'
                with self.assertRaises(Unavailable):self.checked(bad)

    def test_rows_nullable_but_handles_controls_bounded(self):
        for screen,columns in (('transfer_destination',2),('transfer_items',4)):
            obj=state(screen=screen);obj['visible']['rows'][0]['cells']=[None]*columns;self.checked(obj)
            for key,value in [('handle','r32'),('handle','r00'),('selected',1),('cells',[None]*(columns+1))]:
                bad=copy.deepcopy(obj);bad['visible']['rows'][0][key]=value
                with self.assertRaises(Unavailable):self.checked(bad)
            bad=copy.deepcopy(obj);bad['visible']['rows']=[]
            with self.assertRaises(Unavailable):self.checked(bad)
        for controls in ([],['max'],['decrease','increase'],['increase','increase']):
            obj=state(screen='transfer_items');obj['visible']['rows'][0]['controls']=controls
            with self.assertRaises(Unavailable):self.checked(obj)

    def test_buttons_and_scroll_must_be_shown(self):
        for screen in a.TRANSFER_BUTTON_ACTIONS:
            obj=state(screen=screen);obj['visible']['buttons']=[]
            with self.assertRaises(Unavailable):self.checked(obj)
        for screen in ('transfer_destination','transfer_items'):
            obj=state(screen=screen);obj['visible']['scroll']=dict(up=False,down=False)
            with self.assertRaises(Unavailable):self.checked(obj)

    def test_transfer_actions_rejected_in_wrong_screens(self):
        actions={'open_transfer','change_transfer_quantity'}
        for mapping in a.TRANSFER_BUTTON_ACTIONS.values():actions.update(mapping)
        for screen in a.STATE_ACTIONS:
            for action in actions-a.STATE_ACTIONS[screen]:
                obj=state(screen=screen)
                obj['actions']=[dict(id=action,label='Shown',label_source=a.label_source(screen,action),argument_kind=a.ARGUMENT_KINDS[action])]
                with self.subTest(screen=screen,action=action),self.assertRaises(Unavailable):self.checked(obj)

    def test_row_limit_and_disjoint_selection(self):
        for screen in ('transfer_destination','transfer_items'):
            obj=state(screen=screen);row=obj['visible']['rows'][0]
            obj['visible']['rows']=[dict(copy.deepcopy(row),handle=f'r{i}',selected=i==31) for i in range(32)]
            self.checked(obj)
            obj['visible']['rows'][0]['selected']=True
            with self.assertRaises(Unavailable):self.checked(obj)
            obj['visible']['rows'][0]['selected']=False
            obj['visible']['rows'].append(dict(copy.deepcopy(row),handle='r32'))
            with self.assertRaises(Unavailable):self.checked(obj)

    def test_closed_canonical_arguments(self):
        for row in a.ROW_HANDLES:
            for operation,values in [('adjust',(-1000,-1,1,1000)),('set',(0,1,100000)),('max',(None,)),('clear',(None,))]:
                for value in values:
                    args=dict(row=row,operation=operation)
                    if value is not None:args['value']=value
                    wire=a.canonical_action_request(arguments(action='change_transfer_quantity',args=args))
                    self.assertTrue(wire.endswith((' '+row+' '+operation+(' '+str(value) if value is not None else '')).encode()))
        for args in (dict(row='r32',operation='max'),dict(row='r00',operation='clear'),dict(row='r0',operation='adjust',value=0),dict(row='r0',operation='adjust',value=1001),dict(row='r0',operation='set',value=-1),dict(row='r0',operation='set',value=100001),dict(row='r0',operation='set',value=True),dict(row='r0',operation='max',value=1)):
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='change_transfer_quantity',args=args))

    def test_every_action_fresh_dispatch_and_cache_clear(self):
        scenarios={'open_transfer':'basescape'}
        for screen,actions in a.STATE_ACTIONS.items():
            if screen in a.TRANSFER_BUTTON_ACTIONS:scenarios.update(dict.fromkeys(actions,screen))
        for action,screen in scenarios.items():
            peer=NativeFixture(screen=screen)
            try:
                obj=peer.client.observe()
                self.assertEqual(peer.client.execute(**arguments(obj,action=action))['status'],'dispatched')
                self.assertFalse(peer.client.transfer_rows or peer.client.visible_rows or peer.client.scroll_directions)
                self.assertIsNone(peer.client.capability)
            finally:
                peer.close()

    def test_direction_gate_and_unoffered_rows_before_send(self):
        for controls in (['increase'],['decrease'],['increase','decrease']):
            def transform(op,obj):
                if op=='R':obj['visible']['rows'][0]['controls']=controls
                return obj
            for args,required in [(dict(operation='max'),{'increase'}),(dict(operation='clear'),{'decrease'}),(dict(operation='set',value=0),{'increase','decrease'}),(dict(operation='adjust',value=-1),{'decrease'})]:
                peer=NativeFixture(screen='transfer_items',transform=transform)
                try:
                    obj=peer.client.observe()
                    kwargs=arguments(obj,action='change_transfer_quantity',args=dict(row='r0',**args))
                    if required<=set(controls):self.assertEqual(peer.client.execute(**kwargs)['status'],'dispatched')
                    else:
                        with self.assertRaises(Unavailable):peer.client.execute(**kwargs)
                        self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
                finally:
                    peer.close()
        peer=self.fixture(screen='transfer_items');old=peer.client.observe();fresh=peer.client.observe()
        for obj,row in ((old,'r0'),(fresh,'r31')):
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='change_transfer_quantity',args=dict(row=row,operation='max')))
        self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))

if __name__=='__main__':unittest.main()
