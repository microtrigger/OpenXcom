# SPDX-License-Identifier: GPL-3.0-or-later
"""Strict player-visible graph geometry and fresh actions."""
import copy,unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state,arguments,NativeFixture
class GraphsTests(unittest.TestCase):
 def check(self,x):return a.validate_state(x,x['challenge'],x['epoch'])
 def test_modes(self):
  for mode in a.GRAPH_MODES:
   x=state(screen='graphs');x['visible']['mode']=mode;self.check(x)
 def test_geometry_invalid(self):
  for key,values in {'x1':[True,124,313,1.0],'x2':[124,313,False],'y1':[44,179,True],'y2':[46,False],'color':[-1,256,True]}.items():
   for value in values:
    x=state(screen='graphs');x['visible']['plot']['segments'][0][key]=value
    with self.subTest(key=key,value=value),self.assertRaises(Unavailable):self.check(x)
 def test_hidden_extra(self):
  for scope in ('visible','plot','line'):
   x=state(screen='graphs');target=x['visible'] if scope=='visible' else x['visible']['plot'] if scope=='plot' else x['visible']['plot']['segments'][0];target['raw_history']=[1,2]
   with self.assertRaises(Unavailable):self.check(x)
 def test_no_target_no_action(self):
  x=state(screen='graphs');x['visible']['legend']=[]
  with self.assertRaises(Unavailable):self.check(x)
  x['actions']=[d for d in x['actions'] if d['id']!='toggle_graph_series'];self.check(x)
 def test_exact_rows_scales_and_order(self):
  for field,value in (('scale',['0']*9),('legend',[dict(handle='r0',label='R',pressed=1,visible_palette=[5])]),('months',[dict(handle='r0',cells=['Jan'],selected=False)]),('years',[dict(handle='r0',cells=['1999']*6,selected=True)]),('scroll',dict(up=1,down=False))):
   x=state(screen='graphs');x['visible'][field]=value
   with self.subTest(field=field),self.assertRaises(Unavailable):self.check(x)
  x=state(screen='graphs');x['visible']['plot']['segments'].append(dict(x1=128,y1=45,x2=132,y2=45,color=7))
  with self.assertRaises(Unavailable):self.check(x)
 def test_action_descriptors(self):
  for key,value in (('label','Hidden history'),('label_source','visible_text'),('argument_kind','text')):
   x=state(screen='graphs');x['actions'][0][key]=value
   with self.assertRaises(Unavailable):self.check(x)
 def test_palette(self):
  for palette in ([True],[0],[256],[2,1],[1,1],None):
   x=state(screen='graphs');x['visible']['legend'][0]['visible_palette']=palette
   with self.assertRaises(Unavailable):self.check(x)
 def test_partial_axis_cell_preserves_other_labels(self):
  x=state(screen='graphs');cell=x['visible']['months'][0];cell.update(label='Jan',clipped=True,bounds=dict(x=125,y=183,width=5,height=8));self.check(x)
  self.assertEqual(x['visible']['months'][1]['label'],'Jan')
  cell.update(label=None,clipped=True,bounds=None);self.check(x)
  self.assertEqual(x['visible']['months'][1]['label'],'Jan')
 def test_axis_visibility_requires_exact_bounds(self):
  for field,value in (('label',None),('bounds',None),('clipped',1),('bounds',dict(x=319,y=199,width=2,height=1)),('bounds',dict(x=0,y=0,width=True,height=1))):
   x=state(screen='graphs');x['visible']['years'][0][field]=value
   with self.subTest(field=field),self.assertRaises(Unavailable):self.check(x)
  for key,n in (('months',12),('years',6)):
   x=state(screen='graphs');x['visible'][key]=x['visible'][key][:-1]
   with self.assertRaises(Unavailable):self.check(x)
 def test_canonical(self):
  self.assertTrue(a.canonical_action_request(arguments(action='toggle_graph_series',args={'row':'r0'})).endswith(b'toggle_graph_series r0'))
  self.assertTrue(a.canonical_action_request(arguments(action='scroll_graph_legend',args={'direction':'down'})).endswith(b'scroll_graph_legend down'))
 def test_fresh_authority(self):
  p=NativeFixture(screen='graphs');self.addCleanup(p.close);old=p.client.observe();fresh=p.client.observe()
  with self.assertRaises(Unavailable):p.client.execute(**arguments(old,action='toggle_graph_series',args={'row':'r0'}))
  with self.assertRaises(Unavailable):p.client.execute(**arguments(fresh,action='toggle_graph_series',args={'row':'r1'}))
  self.assertEqual(p.client.execute(**arguments(fresh,action='toggle_graph_series',args={'row':'r0'}))['status'],'dispatched')
if __name__=='__main__':unittest.main()
