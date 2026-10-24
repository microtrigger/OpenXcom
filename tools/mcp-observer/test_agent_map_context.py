# SPDX-License-Identifier: GPL-3.0-or-later
"""Serialized current public map DTO checks; no native visibility/gameplay claim."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, dump, CHALLENGE, EPOCH
from test_agent_events import tactical


class MapContextTests(unittest.TestCase):
    def checked(self, obj):
        return a.validate_state(a.parse(dump(obj)), CHALLENGE, EPOCH)

    def context(self, obj):
        return obj['visible']['tactical']['map_context']

    def test_current_serialized_public_state_and_unknown_occupancy(self):
        obj = self.checked(state())
        context = self.context(obj)
        self.assertEqual([c['handle'] for c in context['cells']], ['g0', 'g1'])
        self.assertEqual(context['cells'][1]['unit'], 'unknown')
        self.assertEqual(obj['visible']['tactical']['grid'][1]['handle'], 'g1')
        for unit in ('unknown', 'own', 'visible'):
            sample=state();self.context(sample)['cells'][0]['unit']=unit;self.checked(sample)
        sample=state();self.context(sample)['selected_position']=None;self.checked(sample)
        self.assertNotIn('preview', context['cells'][0])

    def test_current_client_preserves_serialized_public_map_context(self):
        peer=NativeFixture()
        try:
            obj=peer.client.observe()
            self.assertEqual(self.context(obj), self.context(state()))
            self.assertEqual([raw.split(b' ',1)[0] for raw in peer.requests], [b'H',b'R'])
        finally:
            peer.close()

    def test_old_and_extra_shapes_fail_closed(self):
        old=state();del old['visible']['tactical']['map_context']
        with self.assertRaises(Unavailable):self.checked(old)
        for path in ((), ('cells',0), ('cells',0,'parts'), ('selected_position',)):
            sample=state();target=self.context(sample)
            for key in path:target=target[key]
            target['hidden_id']=7
            with self.subTest(path=path),self.assertRaises(Unavailable):self.checked(sample)
        for key in ('scope','selected_position','cells','truncated'):
            sample=state();del self.context(sample)[key]
            with self.subTest(key=key),self.assertRaises(Unavailable):self.checked(sample)
        sample=state();self.context(sample)['cells'][0]['preview']=None
        with self.assertRaises(Unavailable):self.checked(sample)

    def test_exact_order_counts_and_paired_truncation(self):
        mutations=(lambda c:c['cells'].reverse(),lambda c:c['cells'].pop(),
                   lambda c:c['cells'].append(copy.deepcopy(c['cells'][0])),
                   lambda c:c.update(truncated=True),lambda c:c.update(truncated=1),
                   lambda c:c.update(scope='world'))
        for mutate in mutations:
            sample=state();mutate(self.context(sample))
            with self.assertRaises(Unavailable):self.checked(sample)
        sample=state();sample['visible']['tactical']['truncated']=True
        self.context(sample)['truncated']=True;self.checked(sample)

    def test_joint_wire_budget_and_cell_count(self):
        sample=state();t=sample['visible']['tactical'];c=self.context(sample)
        grid,cell=copy.deepcopy(t['grid'][0]),copy.deepcopy(c['cells'][0])
        t['grid']=[];c['cells']=[]
        for i in range(128):
            g=copy.deepcopy(grid);g['handle']=f'g{i}';t['grid'].append(g)
            value=copy.deepcopy(cell);value['handle']=f'g{i}';c['cells'].append(value)
        self.assertGreater(len(dump(t['grid']))+len(dump(c['cells'])),23500)
        with self.assertRaises(Unavailable):self.checked(sample)
        c['cells'].append(copy.deepcopy(cell))
        with self.assertRaises(Unavailable):self.checked(sample)

    def test_strict_position_representation_bounds(self):
        for selected in (False,True):
            for axis in ('x','y','z'):
                for bad in (True,1.0,None,2**31,-2 if axis!='z' and not selected else -1):
                    sample=state();c=self.context(sample)
                    pos=c['selected_position'] if selected else c['cells'][0]['position'];pos[axis]=bad
                    with self.subTest(selected=selected,axis=axis,bad=bad),self.assertRaises(Unavailable):self.checked(sample)
        sample=state();self.context(sample)['cells'][0]['position'].update(x=-1,y=-1,z=2**31-1)
        self.context(sample)['cells'][0].update(unit='unknown',parts=dict.fromkeys(('floor','west','north','object'),'unknown'))
        self.context(sample)['selected_position'].update(x=2**31-1,y=2**31-1,z=2**31-1)
        self.checked(sample)

    def test_no_invented_occupancy_or_part_classification(self):
        sample=state();self.context(sample)['cells'][0]['position']['x']=-1
        with self.assertRaises(Unavailable):self.checked(sample)
        for field,bad in (('unit','none'),('unit','enemy'),('unit',None),('unit',True)):
            sample=state();self.context(sample)['cells'][0][field]=bad
            with self.assertRaises(Unavailable):self.checked(sample)
        for part in ('floor','west','north','object'):
            for bad in ('window','open_door','hidden',None,True):
                sample=state();self.context(sample)['cells'][0]['parts'][part]=bad
                with self.subTest(part=part,bad=bad),self.assertRaises(Unavailable):self.checked(sample)
        for part,good in (('floor','empty'),('west','door'),('north','wall'),('object','object')):
            sample=state();self.context(sample)['cells'][0]['parts'][part]=good;self.checked(sample)

    def test_hidden_and_unavailable_have_no_context(self):
        for mode in ('hidden_movement','unavailable'):
            sample=tactical(mode);a.validate_tactical(sample)
            for key,value in (('selected_position',dict(x=0,y=0,z=0)),
                              ('cells',self.context(state())['cells']),('truncated',True)):
                candidate=copy.deepcopy(sample);candidate['map_context'][key]=value
                with self.subTest(mode=mode,key=key),self.assertRaises(Unavailable):a.validate_tactical(candidate)


if __name__ == '__main__':unittest.main()
