# SPDX-License-Identifier: GPL-3.0-or-later
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, NativeFixture, arguments, dump, result, COMMAND, EPOCH
from test_agent_events import page, validate

def stats_state():
    obj=state(screen='ufopaedia_article');obj['visible'].update(details_status='partial',article_stats=dict(rows=[['Damage','70'],['',None],['Accuracy','60%']],scroll=dict(up=False,down=True)))
    obj['actions'].append(dict(id='scroll_article_stats',label='Scroll article stats',label_source='visible_control_semantics',argument_kind='scroll_direction'));return obj
class ArticleStatsTests(unittest.TestCase):
    def valid(self,obj):return a.validate_state(obj,obj['challenge'],obj['epoch'])
    def invalid(self,obj):
        with self.assertRaises(Unavailable):self.valid(obj)
    def test_schema_catalog(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/66');self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS)),(286,109));self.valid(stats_state())
    def test_two_nullable_cells_and_native_separators(self):
        obj=stats_state();obj['visible']['article_stats']['rows']=[[None,'70'],['',None],['',''],[None,None]];self.valid(obj)
    def test_shape_and_budget(self):
        for rows in ([[None,None]]*33,[[None]],[[None,None,None]],{},[dict(label='a',value='b')]):
            obj=stats_state();obj['visible']['article_stats']['rows']=rows;self.invalid(obj)
        obj=stats_state();obj['visible']['article_stats']['rows']=[[None,None]]*32;self.valid(obj)
    def test_partial_and_item_independence(self):
        obj=stats_state();obj['visible']['item_combat']=dict(shot_headings=[None]*3,shots=None,damage_rows=None,damage_headings=[None]*2);self.valid(obj)
        obj['visible']['article_stats']=None;obj['actions']=[d for d in obj['actions'] if d['id']!='scroll_article_stats'];self.valid(obj)
        obj['visible']['item_combat']=None;self.invalid(obj);obj['visible']['details_status']='unavailable';self.valid(obj)
    def test_hidden_values_not_invented(self):
        for key in ('armor_id','rule','damage_modifiers','omitted_defaults','family','model','item_id'):
            obj=stats_state();obj['visible']['article_stats'][key]=0;self.invalid(obj)
        obj=stats_state();obj['visible']['article_stats']['rows'][0][1]=70;self.invalid(obj)
    def test_scroll_proof_and_types(self):
        for scroll in ({'up':0,'down':True},{'up':False},{'up':False,'down':False}):
            obj=stats_state();obj['visible']['article_stats']['scroll']=scroll;self.invalid(obj)
        obj=stats_state();obj['visible']['article_stats']=None;self.invalid(obj)
    def test_scroll_body_remains_independent(self):
        obj=stats_state();obj['visible']['article_stats']['scroll']=dict(up=False,down=False);obj['actions']=[d for d in obj['actions'] if d['id']!='scroll_article_stats'];self.valid(obj)
        self.assertIn('scroll_article_text',[d['id'] for d in obj['actions']])
    def test_direction_grant_and_replay(self):
        class StatsFixture(NativeFixture):
            def observation(self, challenge, capability):
                obj=super().observation(challenge,capability)
                template=stats_state();obj['visible']=template['visible'];obj['actions']=template['actions'];return obj
        peer=StatsFixture(screen='ufopaedia_article');self.addCleanup(peer.close);old=peer.client.observe();fresh=peer.client.observe()
        for request in (arguments(old,action='scroll_article_stats',args={'direction':'down'}),arguments(fresh,action='scroll_article_stats',args={'direction':'up'})):
            with self.assertRaises(Unavailable):peer.client.execute(**request)
        self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
        request=arguments(fresh,action='scroll_article_stats',args={'direction':'down'});receipt=peer.client.execute(**request);self.assertEqual(peer.client.execute(**request),receipt);self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),1)
    def test_result_event(self):
        obj=stats_state();obj.update(challenge=None,capability=None,event_sequence=2);a.validate_result(dump(result(observation=obj)),COMMAND,EPOCH);validate(page([dict(seq=1,kind='presentation',state='ufopaedia_article',visible=obj['visible'])]))
    def test_prior_schema_and_extra_args(self):
        obj=stats_state();obj['schema']='openxcom-agent-control/58';self.invalid(obj)
        for args in ({'direction':'down','repeat':2},{'direction':'page'},{'row':'r0'}):
            with self.assertRaises(Unavailable):a.canonical_action_request(arguments(action='scroll_article_stats',args=args))

if __name__=='__main__':unittest.main()
