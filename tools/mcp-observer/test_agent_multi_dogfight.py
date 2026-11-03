# SPDX-License-Identifier: GPL-3.0-or-later
"""Version-scoped composite dogfight wire62 validation; private native identity proof is separate."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, NativeFixture, arguments, dump, result, COMMAND, EPOCH
from test_agent_events import page, validate

def multi(count=2,minimized=False):
    obj=state(screen='dogfight');w=obj['visible']['windows'][0]
    obj['visible']['windows']=[dict(copy.deepcopy(w),handle='w'+str(i),bounds=dict(x=160*(i%2),y=96*(i//2),width=160,height=96),can_minimize=True) for i in range(count)]
    obj['actions'].append(dict(id='minimize_dogfight',label='Minimize dogfight',label_source='visible_control_semantics',argument_kind='dogfight_window'))
    if minimized:
        for w in obj['visible']['windows']:
            w.update(minimized=True,status=None,distance=None,ammo1=None,ammo2=None,interception='1',buttons=[],can_minimize=False,can_restore=True)
        obj['actions']=[dict(id='restore_dogfight',label='Restore dogfight',label_source='visible_control_semantics',argument_kind='dogfight_window')]
    return obj

def with_geo(obj):
    obj['visible']['geoscape']=dict(clock=dict.fromkeys(a.DOGFIGHT_CLOCK),speed='5 Sec',speeds=[dict(handle='s0',label='5 Sec',selected=True)],buttons=[dict(id='options',label='Options')])
    obj['actions'] += [dict(id='choose_geoscape_speed',label='5 Sec',label_source='visible_text',argument_kind='geoscape_speed'),dict(id='open_pause_menu',label='Options',label_source='visible_text',argument_kind='none')];return obj

class MultiFixture(NativeFixture):
    def __init__(self,template,**kwargs):self.template=template;super().__init__(screen='dogfight',**kwargs)
    def observation(self,challenge,capability):
        obj=super().observation(challenge,capability);obj['visible']=copy.deepcopy(self.template['visible']);obj['actions']=copy.deepcopy(self.template['actions']);return obj

class MultiDogfightTests(unittest.TestCase):
    def valid(self,obj):return a.validate_state(obj,obj['challenge'],obj['epoch'])
    def invalid(self,obj):
        with self.assertRaises(Unavailable):self.valid(obj)
    def test_catalog_and_one_to_four(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/66');self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS)),(286,109))
        for n in (1,2,3,4):self.valid(multi(n))
        for n in (0,5):self.invalid(multi(n))
    def test_handles_unique_canonical_local(self):
        for handle in ('w0','w4','craft1',None,1):
            obj=multi();obj['visible']['windows'][1]['handle']=handle;self.invalid(obj)
        obj=multi();obj['visible']['windows'].reverse();self.invalid(obj)
    def test_bounds_strict_and_canvas(self):
        for key,bad in (('x',-1),('x',True),('y',400),('width',0),('width',641),('height',401)):
            obj=multi();obj['visible']['windows'][0]['bounds'][key]=bad;self.invalid(obj)
    def test_minimized_never_hidden_expanded_fields(self):
        self.valid(multi(4,True))
        for key,value in (('status','Hidden'),('distance','560'),('ammo1','10'),('ammo2','5'),('buttons',[dict(id='standoff',selected=True)]),('can_minimize',True)):
            obj=multi(2,True);obj['visible']['windows'][0][key]=value;self.invalid(obj)
    def test_expanded_number_hidden_and_no_restore(self):
        for key,value in (('interception','1'),('can_restore',True),('damage',50)):
            obj=multi();obj['visible']['windows'][0][key]=value;self.invalid(obj)
    def test_no_hidden_identity_or_order_fields(self):
        for key in ('craft_name','craft_id','ufo_id','faction','target','slot','damage_percent','roster_order'):
            obj=multi();obj['visible']['windows'][0][key]='hidden';self.invalid(obj)
    def test_all_minimized_background_only(self):
        self.valid(with_geo(multi(2,True)));self.invalid(with_geo(multi(2)))
        obj=with_geo(multi(2,True));obj['visible']['geoscape']['buttons'].append(dict(id='bases',label='Bases'));self.invalid(obj)
        obj=with_geo(multi(2,True));obj['visible']['geoscape']['clock']['target_id']='hidden';self.invalid(obj)
    def test_modes_independent_flags_not_success(self):
        obj=multi();obj['visible']['windows'][0]['buttons']=[];self.valid(obj)
        obj['visible']['windows'][1]['buttons']=[];self.invalid(obj)
        obj['actions']=[d for d in obj['actions'] if d['id']!='choose_dogfight_mode'];self.valid(obj)
    def test_exact_explicit_window_args_no_legacy_default(self):
        for action,args in [('choose_dogfight_mode',{'window':'w3','mode':'cautious'}),('minimize_dogfight',{'window':'w2'}),('restore_dogfight',{'window':'w1'})]:
            self.assertIn((' '+args['window']).encode(),a.canonical_action_request(arguments(action=action,args=args)))
        for args in ({'mode':'standard'},{'window':'w4','mode':'standard'},{'window':'w0','mode':'standard','target':'hidden'},{'window':0,'mode':'standard'}):
            with self.assertRaises(Unavailable):a.canonical_action_request(arguments(action='choose_dogfight_mode',args=args))
    def test_per_window_mode_target_grants(self):
        obj=multi();obj['visible']['windows'][1]['buttons']=[dict(id='standoff',selected=True)]
        peer=MultiFixture(obj);self.addCleanup(peer.close);fresh=peer.client.observe()
        for window,mode in (('w1','aggressive'),('w2','standoff')):
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(fresh,action='choose_dogfight_mode',args=dict(window=window,mode=mode)))
        self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))
        command=arguments(fresh,action='choose_dogfight_mode',args=dict(window='w0',mode='aggressive'));receipt=peer.client.execute(**command);self.assertEqual(peer.client.execute(**command),receipt);self.assertEqual(sum(p.startswith(b'E ') for p in peer.requests),1)
    def test_replacement_stale_grant_never_resolves_new_nth(self):
        peer=MultiFixture(multi());self.addCleanup(peer.close);old=peer.client.observe();peer.revision+=1;peer.template=multi(3);fresh=peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action='minimize_dogfight',args={'window':'w1'}))
        self.assertFalse(any(p.startswith(b'E ') for p in peer.requests));self.assertEqual(peer.client.capability,fresh['capability'])
    def test_minimize_restore_direction_permissions(self):
        for minimized,action,other in ((False,'minimize_dogfight','restore_dogfight'),(True,'restore_dogfight','minimize_dogfight')):
            peer=MultiFixture(multi(2,minimized));self.addCleanup(peer.close);fresh=peer.client.observe()
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(fresh,action=other,args={'window':'w0'}))
            self.assertEqual(peer.client.execute(**arguments(fresh,action=action,args={'window':'w1'}))['status'],'dispatched')
    def test_background_speed_and_pause_capability(self):
        peer=MultiFixture(with_geo(multi(2,True)));self.addCleanup(peer.close);fresh=peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(fresh,action='choose_geoscape_speed',args={'speed':'s1'}))
        self.assertEqual(peer.client.execute(**arguments(fresh,action='choose_geoscape_speed',args={'speed':'s0'}))['status'],'dispatched')
    def test_result_and_event(self):
        obj=multi(4);obj.update(challenge=None,capability=None,event_sequence=2);a.validate_result(dump(result(observation=obj)),COMMAND,EPOCH);validate(page([dict(seq=1,kind='presentation',state='dogfight',visible=obj['visible'])]))
    def test_prior_wire_rejected(self):
        obj=multi();obj['schema']='openxcom-agent-control/59';self.invalid(obj)

if __name__=='__main__':unittest.main()
