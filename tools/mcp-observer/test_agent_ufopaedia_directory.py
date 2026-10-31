# SPDX-License-Identifier: GPL-3.0-or-later
"""Known encyclopedia-directory wire surface; sockets do not prove game behavior."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, EPOCH, CHALLENGE
from test_agent_events import page, validate

SCREENS = ('ufopaedia_start', 'ufopaedia_select')

def checked(obj):
    return a.validate_state(obj, CHALLENGE, EPOCH)

def event(obj):
    return dict(seq=1,kind='presentation',state=obj['state'],visible=copy.deepcopy(obj['visible']))

class UfopaediaDirectoryTests(unittest.TestCase):
    def fixture(self, **kwargs):
        peer=NativeFixture(**kwargs);self.addCleanup(peer.close);return peer

    def test_closed_catalog(self):
        self.assertEqual((a.SCHEMA,len(a.ACTIONS),len(a.STATE_ACTIONS)),('openxcom-agent-control/50-craft-stats1',261,106))
        for screen in SCREENS:
            checked(state(screen=screen));validate(page([event(state(screen=screen))]))

    def test_empty_known_directory_allows_return_not_selection(self):
        for screen in SCREENS:
            obj=state(screen=screen);obj['visible']['rows']=[]
            with self.assertRaises(Unavailable):checked(obj)
            select,_,_=a.UFOPAEDIA_DIRECTORY[screen]
            obj['actions']=[d for d in obj['actions'] if d['id']!=select]
            checked(obj);validate(page([event(obj)]))

    def test_nullable_painted_labels_do_not_disclose_ids(self):
        for screen in SCREENS:
            obj=state(screen=screen);obj['visible']['title']=None;obj['visible']['rows'][0]['cells']=[None]
            checked(obj)
            for key in ('article_id','category_id','requirements','all_articles','research','section'):
                bad=copy.deepcopy(obj);bad['visible'][key]=[]
                with self.assertRaises(Unavailable):checked(bad)
                with self.assertRaises(Unavailable):validate(page([event(bad)]))

    def test_rows_bounded_contiguous_single_column_and_boolean_selection(self):
        for screen in SCREENS:
            _,_,limit=a.UFOPAEDIA_DIRECTORY[screen]
            obj=state(screen=screen);obj['visible']['rows']=[dict(handle=f'r{i}',cells=['Known'],selected=False)for i in range(limit)]
            checked(obj)
            obj['visible']['rows'].append(dict(handle=f'r{limit}',cells=['Not painted'],selected=False))
            with self.assertRaises(Unavailable):checked(obj)
            for key,value in (('handle','r9'),('selected',1),('cells',['A','B'])):
                bad=state(screen=screen);bad['visible']['rows'][0][key]=value
                with self.assertRaises(Unavailable):checked(bad)

    def test_article_list_retains_painted_selection_after_article_close(self):
        selected=state(screen='ufopaedia_select')
        selected['visible']['rows'][0]['selected']=True
        checked(selected);validate(page([event(selected)]))
        category=state(screen='ufopaedia_start')
        category['visible']['rows'][0]['selected']=True
        with self.assertRaises(Unavailable):checked(category)
        def transform(op,obj):
            observation=obj.get('observation') if op=='E' else obj
            if observation and observation.get('state')=='ufopaedia_select':
                observation['visible']['rows'][0]['selected']=True
            return obj
        peer=self.fixture(screen='ufopaedia_article',transform=transform,
            transitions={('ufopaedia_article','close_ufopaedia_article'):'ufopaedia_select'})
        observed=peer.client.observe()
        result=peer.client.execute(**arguments(observed,action='close_ufopaedia_article'))
        self.assertEqual(result['status'],'dispatched')
        self.assertTrue(result['observation']['visible']['rows'][0]['selected'])
        fresh=peer.client.observe()
        self.assertTrue(fresh['visible']['rows'][0]['selected'])
        self.assertEqual(peer.client.execute(**arguments(fresh,command_id='a'*32,action='close_ufopaedia_select'))['status'],'dispatched')
        self.assertEqual(sum(r.startswith(b'E ')for r in peer.requests),2)

    def test_exact_close_pair_and_semantic_label(self):
        for screen in SCREENS:
            select,close,_=a.UFOPAEDIA_DIRECTORY[screen]
            obj=state(screen=screen);obj['visible']['buttons']=[]
            with self.assertRaises(Unavailable):checked(obj)
            obj=state(screen=screen);obj['actions']=[d for d in obj['actions'] if d['id']!=close]
            with self.assertRaises(Unavailable):checked(obj)
            obj=state(screen=screen);next(d for d in obj['actions'] if d['id']==select)['label']='Invented label'
            with self.assertRaises(Unavailable):checked(obj)

    def test_directory_native_request_uses_only_visible_row(self):
        for screen in SCREENS:
            select,close,_=a.UFOPAEDIA_DIRECTORY[screen]
            for action in (select,close):
                peer=self.fixture(screen=screen);obj=peer.client.observe()
                self.assertEqual(peer.client.execute(**arguments(obj,action=action))['status'],'dispatched')
                self.assertEqual(sum(r.startswith(b'E ') for r in peer.requests),1)
                self.assertFalse(peer.client.outfitting_rows)

    def test_old_capability_and_unseen_row_are_not_sent(self):
        for screen in SCREENS:
            select,_,_=a.UFOPAEDIA_DIRECTORY[screen]
            peer=self.fixture(screen=screen);old=peer.client.observe();fresh=peer.client.observe()
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action=select))
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(fresh,action=select,args={'row':'r31'}))
            self.assertFalse(any(r.startswith(b'E ')for r in peer.requests))

    def test_scroll_direction_bound_to_painted_authority(self):
        for screen in SCREENS:
            def transform(op,obj):
                if op=='R':obj['visible']['scroll']={'up':False,'down':True}
                return obj
            peer=self.fixture(screen=screen,transform=transform);obj=peer.client.observe()
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='scroll_visible_list',args={'direction':'up'}))
            self.assertEqual(peer.client.execute(**arguments(obj,action='scroll_visible_list',args={'direction':'down'}))['status'],'dispatched')

    def test_geoscape_entry_requires_the_visible_button(self):
        obj=state(screen='geoscape')
        obj['actions'].append(dict(id='open_ufopaedia',label='Ufopaedia',label_source='visible_text',argument_kind='none'))
        checked(obj)
        obj['visible']['buttons']=[b for b in obj['visible']['buttons']if b['id']!='ufopaedia']
        with self.assertRaises(Unavailable):checked(obj)

    def test_catalog_only_surface_cannot_invent_article_control(self):
        for screen in SCREENS:
            obj=state(screen=screen)
            obj['actions'].append(dict(id='next_ufopaedia_article',label='Next',label_source='visible_text',argument_kind='none'))
            with self.assertRaises(Unavailable):checked(obj)

if __name__=='__main__':unittest.main()
