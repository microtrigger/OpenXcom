# SPDX-License-Identifier: GPL-3.0-or-later
"""Debrief STATS/SCORE projection and fresh ordinary controls, no model statistics."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, CHALLENGE, EPOCH


def score():
    obj=state(screen='debriefing')
    obj['actions'].append(dict(id='toggle_debriefing_stats',label=obj['visible']['stats_label'],label_source='visible_text',argument_kind='none'))
    return obj


class DebriefStatsTests(unittest.TestCase):
    def shown(self):return state(screen='debriefing_soldier_stats')
    def checked(self,obj):return a.validate_state(obj,CHALLENGE,EPOCH)

    def test_exact_shape_nullable_captions_and_twelve_columns(self):
        obj=self.shown();self.checked(obj)
        for key in obj['visible']:
            bad=copy.deepcopy(obj);del bad['visible'][key]
            with self.assertRaises(Unavailable):self.checked(bad)
        for key in ('soldier_id','stats','total','selected','model_values'):
            bad=self.shown();bad['visible'][key]=0
            with self.assertRaises(Unavailable):self.checked(bad)
        for value in (None,'','+1','é'*100):
            obj=self.shown();obj['visible'].update(title=value,tooltip=value,headings=[value]*12,table=dict(status='shown',rows=[dict(cells=[value]*12)]));self.checked(obj)
        for value in (True,0,[],{},'é'*513):
            for key in ('title','tooltip','headings','table'):
                obj=self.shown();obj['visible'][key]=[value]*12 if key=='headings' else dict(status='shown',rows=[dict(cells=[value]*12)]) if key=='table' else value
                with self.subTest(key=key),self.assertRaises(Unavailable):self.checked(obj)
        for count in (0,11,13):
            obj=self.shown();obj['visible']['headings']=[None]*count
            with self.assertRaises(Unavailable):self.checked(obj)
            obj=self.shown();obj['visible']['table']['rows']=[dict(cells=[None]*count)]
            with self.assertRaises(Unavailable):self.checked(obj)

    def test_table_status_row_bounds_and_no_identity(self):
        obj=self.shown();obj['visible']['table']=dict(status='unavailable',rows=[]);self.checked(obj)
        for table in (dict(status='unavailable',rows=[dict(cells=[None]*12)]),dict(status='partial',rows=[]),dict(status='shown',rows=[dict(cells=[None]*12)]*129),dict(status='shown',rows=[],total=1)):
            obj=self.shown();obj['visible']['table']=table
            with self.assertRaises(Unavailable):self.checked(obj)
        for field in ('handle','logicalIndex','selected','soldier_id','index'):
            obj=self.shown();obj['visible']['table']['rows'][0][field]=0
            with self.assertRaises(Unavailable):self.checked(obj)

    def test_buttons_scroll_and_action_label_consistency(self):
        for buttons in ([],None,[dict(id='stats',label='SCORE'),dict(id='ok',label='OK')],[dict(id='ok',label=None)],[dict(id='stats',label='')]):
            obj=self.shown();obj['visible']['buttons']=buttons
            with self.assertRaises(Unavailable):self.checked(obj)
        for entry in self.shown()['actions']:
            obj=self.shown();next(x for x in obj['actions'] if x['id']==entry['id'])['label']='invented'
            with self.assertRaises(Unavailable):self.checked(obj)
        for scroll in ({},dict(up=1,down=False),dict(up=False,down=False),dict(up=True,down=True,extra=True)):
            obj=self.shown();obj['visible']['scroll']=scroll
            with self.assertRaises(Unavailable):self.checked(obj)
        obj=self.shown();obj['visible']['scroll']=dict(up=False,down=False);obj['actions']=[x for x in obj['actions'] if x['id']!='scroll_visible_list'];self.checked(obj)
        obj=self.shown();obj['visible']['buttons']=[];obj['visible']['scroll']=dict(up=False,down=False);obj.update(actions=[],capability=None);self.checked(obj)

    def test_score_shape_unchanged_and_toggle_requires_caption(self):
        self.checked(score())
        self.assertEqual(set(score()['visible']),{'title','rating','stats_label','table_status','tables'})
        for value in (None,''):
            obj=score();obj['visible']['stats_label']=value
            with self.assertRaises(Unavailable):self.checked(obj)
        obj=score();obj['actions'][-1]['label']='invented'
        with self.assertRaises(Unavailable):self.checked(obj)
        for args in ({'mode':'stats'},{'row':'r0'},{'value':True},[],None):
            request=arguments(action='toggle_debriefing_stats',args={});request['args']=args
            with self.assertRaises(Unavailable):a.validate_action_arguments(request)
        for action in ('choose_visible_row','next_soldier','acknowledge_cannot_reequip'):
            obj=self.shown();obj['actions']=[dict(id=action,label='Shown',label_source=a.label_source(obj['state'],action),argument_kind=a.ARGUMENT_KINDS[action])]
            with self.assertRaises(Unavailable):self.checked(obj)

    def test_fresh_scroll_toggle_close_and_duplicate_receipt(self):
        for action,args in [('toggle_debriefing_stats',{}),('close_debriefing',{}),('scroll_visible_list',{'direction':'down'})]:
            peer=NativeFixture(screen='debriefing_soldier_stats');self.addCleanup(peer.close)
            old=peer.client.observe();fresh=peer.client.observe();self.assertFalse(peer.client.visible_rows)
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action=action,args=args))
            self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))
            request=arguments(fresh,action=action,args=args);receipt=peer.client.execute(**request)
            self.assertEqual(receipt['status'],'dispatched');self.assertEqual(peer.client.execute(**request),receipt)
            self.assertEqual(sum(p.startswith(b'E ') for p in peer.requests),1)


if __name__=='__main__':unittest.main()
