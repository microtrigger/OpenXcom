# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic Scanner53 contract tests; no native visibility or gameplay proof."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, arguments, NativeFixture, result, dump, COMMAND, EPOCH
from test_agent_events import page, validate

class ScannerTests(unittest.TestCase):
    def valid(self,obj):return a.validate_state(obj,obj['challenge'],obj['epoch'])
    def invalid(self,obj):
        with self.assertRaises(Unavailable):self.valid(obj)
    def test_current_closed_catalog(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/66')
        self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS)),(286,109))
        self.assertEqual(a.STATE_ACTIONS['scanner'],{'close_scanner'})
        self.assertEqual(a.ARGUMENT_KINDS['close_scanner'],'none')
        self.valid(state(screen='scanner'))
    def test_empty_painted_ink_is_not_unavailable(self):
        obj=state(screen='scanner');obj['visible'].update(palette=[],pixels='A'*(152*152))
        self.valid(obj)
        self.assertEqual(obj['visible']['content_status'],'bounded')
    def test_unavailable_keeps_independent_close(self):
        for viewport in (None,dict(x=56,y=24,width=152,height=152)):
            obj=state(screen='scanner');obj['visible'].update(viewport=viewport,palette=[],pixels='',content_status='unavailable')
            self.valid(obj)
            self.assertEqual(obj['actions'][0]['id'],'close_scanner')
    def test_unavailable_cannot_smuggle_raster(self):
        obj=state(screen='scanner');obj['visible']['content_status']='unavailable';self.invalid(obj)
        obj['visible']['pixels']='';self.invalid(obj)
    def test_no_hidden_or_classification_fields(self):
        for key in ('units','unit_id','faction','z','world_x','motion_points','multiplicity','direction','detected_count','empty','truncated','source_palette'):
            obj=state(screen='scanner');obj['visible'][key]=0
            with self.subTest(key=key):self.invalid(obj)
        obj=state(screen='scanner');obj['visible']['viewport']['world_y']=0;self.invalid(obj)
    def test_viewport_requires_known_bounded_local_geometry(self):
        for value in (None,{},dict(x=0,y=0,width=153,height=152),dict(x=639,y=399,width=2,height=1),dict(x=True,y=0,width=1,height=1),dict(x=-1,y=0,width=1,height=1)):
            obj=state(screen='scanner');obj['visible']['viewport']=value
            with self.subTest(value=value):self.invalid(obj)
        obj=state(screen='scanner');obj['visible'].update(viewport=dict(x=639,y=399,width=1,height=1),pixels='B');self.valid(obj)
    def test_color_channels_are_strict_rgb_only(self):
        for rgb in ([True,0,0],[0,-1,0],[0,256,0],[0,0,1.0],[0,0],{'r':0,'g':0,'b':0},[0,0,0,255]):
            obj=state(screen='scanner');obj['visible']['palette']=[rgb]
            with self.subTest(rgb=rgb):self.invalid(obj)
    def raster(self,palette,pixels):
        obj=state(screen='scanner');obj['visible'].update(palette=palette,pixels=pixels+'A'*(152*152-len(pixels)));return obj
    def test_palette_only_unique_used_first_use_entries(self):
        for palette,pixels in (([[1,2,3],[1,2,3]],'BC'),([[1,2,3],[4,5,6]],'CB'),([[1,2,3],[4,5,6]],'B'),([],'B')):
            with self.subTest(palette=palette,pixels=pixels):self.invalid(self.raster(palette,pixels))
        self.valid(self.raster([[1,2,3],[4,5,6]],'BCB'))
    def test_pixel_types_alphabet_and_no_semantic_payload(self):
        for pixels in ([],{},None,True,3,'?', 'B='+'A'*(152*152-2),'é'+'A'*(152*152-1)):
            obj=state(screen='scanner');obj['visible']['pixels']=pixels
            with self.subTest(pixels=str(pixels)[:20]):self.invalid(obj)
    def test_fixed_length_row_major_raster(self):
        for length in (0,1,152*152-1,152*152+1):
            obj=state(screen='scanner');obj['visible']['pixels']='B'*length
            with self.subTest(length=length):self.invalid(obj)
        self.valid(self.raster([[1,2,3]],'B'*(152*152)))
    def test_dense_raster_has_no_run_limit(self):
        obj=self.raster([[1,2,3]],'AB'*(152*152//2));self.valid(obj)
        self.assertLess(len(dump(obj)),a.MAX_TEXT_PACKET)
        obj['visible']['runs']=[];self.invalid(obj)
    def test_palette_budget(self):
        alphabet='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
        obj=self.raster([[i,0,0] for i in range(63)],alphabet[1:]);self.valid(obj)
        obj['visible']['palette'].append([63,0,0]);self.invalid(obj)
    def test_action_labels_scope_and_arguments_closed(self):
        obj=state(screen='scanner');obj['actions'][0]['label']='Close detected aliens';self.invalid(obj)
        obj=state(screen='scanner');obj['actions'][0]['label_source']='visible_text';self.invalid(obj)
        self.assertTrue(a.canonical_action_request(arguments(action='close_scanner',args={})).endswith(b'close_scanner'))
        for args in ({'unit':1},{'target':'r0'},{'repeat':2}):
            with self.subTest(args=args),self.assertRaises(Unavailable):a.canonical_action_request(arguments(action='close_scanner',args=args))
    def test_read_result_and_retained_event(self):
        obj=state(screen='scanner',challenge=None,capability=None,event_sequence=2)
        a.validate_result(dump(result(observation=obj)),COMMAND,EPOCH)
        validate(page([dict(seq=1,kind='presentation',state='scanner',visible=obj['visible'])]))
    def test_capability_freshness_and_single_use(self):
        native=NativeFixture(screen='scanner');self.addCleanup(native.close)
        old=native.client.observe();fresh=native.client.observe()
        with self.assertRaises(Unavailable):native.client.execute(**arguments(old,action='close_scanner',args={}))
        self.assertEqual(native.client.execute(**arguments(fresh,action='close_scanner',args={}))['status'],'dispatched')

if __name__=='__main__':unittest.main()
