# SPDX-License-Identifier: GPL-3.0-or-later
"""Wire55 bounded item panel; fixtures do not establish native visibility."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, dump, result, COMMAND, EPOCH
from test_agent_events import page, validate

def item():
    obj=state(screen='ufopaedia_article')
    obj['visible'].update(details_status='partial',item_combat=dict(shot_headings=['Shot type','Accuracy','TU cost'],shots=[['Auto','35%','35%'],['Snap','60%','25%'],['Aimed','110%','80%']],damage_rows=[dict(type='Armor piercing',power='30')],damage_headings=['Damage','Ammo']))
    return obj

class ItemCombatTests(unittest.TestCase):
    def valid(self,obj):return a.validate_state(obj,obj['challenge'],obj['epoch'])
    def invalid(self,obj):
        with self.assertRaises(Unavailable):self.valid(obj)
    def test_catalog_unchanged_except_schema(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/66')
        self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS)),(286,109));self.valid(item())
    def test_unavailable_and_partial_not_complete(self):
        self.valid(state(screen='ufopaedia_article'))
        for status in ('unavailable','complete','available',None,True):
            obj=item();obj['visible']['details_status']=status;self.invalid(obj)
        obj=item();obj['visible']['item_combat']=None;self.invalid(obj)
    def test_nullable_independent_cells(self):
        obj=item();p=obj['visible']['item_combat'];p.update(shot_headings=[None]*3,damage_headings=[None]*2,shots=[[None]*3],damage_rows=[dict(type=None,power=None)]);self.valid(obj)
        for shots in (None,[]):
            for damage in (None,[]):p.update(shots=shots,damage_rows=damage);self.valid(obj)
    def test_bounded_rows_and_fixed_columns(self):
        for key,bad in (('shots',[[None]*3]*4),('shots',[[None]*2]),('shots',{}),('shots',[{}]),('shot_headings',[None]*4),('damage_headings',[None]),('damage_rows',[dict(type=None,power=None)]*4),('damage_rows',{})):
            obj=item();obj['visible']['item_combat'][key]=bad
            with self.subTest(key=key,bad=bad):self.invalid(obj)
        obj=item();obj['visible']['item_combat']['damage_rows']=[dict(type='Shown',power='5')]*3;self.valid(obj)
    def test_no_hidden_ids_ammo_inference_or_rule_fields(self):
        for key in ('ammo_id','ammo_name','compatible_ammo','clip_size','unit_id','weapon_id','rule','images','damage_type_id','handle'):
            for where in ('panel','damage'):
                obj=item();p=obj['visible']['item_combat'];target=p if where=='panel' else p['damage_rows'][0];target[key]='secret';self.invalid(obj)
        obj=item();obj['visible']['item_combat']['shots'][0].append('rule');self.invalid(obj)
    def test_strict_string_types_and_bounds(self):
        for value in (True,32,1.0,{},[], 'x'*1025,'é'*513):
            for where in ('heading','shot','damage'):
                obj=item();p=obj['visible']['item_combat']
                if where=='heading':p['shot_headings'][0]=value
                elif where=='shot':p['shots'][0][1]=value
                else:p['damage_rows'][0]['power']=value
                self.invalid(obj)
        obj=item();obj['visible']['item_combat']['damage_rows'][0]['power']='x'*1024;self.valid(obj)
    def test_damage_is_formatted_text_not_numeric_interpretation(self):
        obj=item();p=obj['visible']['item_combat'];p['shots']=[['Shown',None,'\x0140%']];p['damage_rows']=[dict(type='Shown type',power='2 × 30')];self.valid(obj)
        self.assertEqual(p['damage_rows'][0]['power'],'2 × 30')
    def test_missing_panel_or_old_schema_rejected(self):
        obj=item();del obj['visible']['item_combat'];self.invalid(obj)
        obj=item();obj['schema']='openxcom-agent-control/54';self.invalid(obj)
    def test_result_and_retained_event(self):
        obj=item();obj.update(challenge=None,capability=None,event_sequence=2)
        a.validate_result(dump(result(observation=obj)),COMMAND,EPOCH)
        validate(page([dict(seq=1,kind='presentation',state='ufopaedia_article',visible=obj['visible'])]))
    def test_parent_and_panel_extra_fields_closed(self):
        obj=item();obj['visible']['research_complete']=True;self.invalid(obj)
        obj=item();obj['visible']['item_combat']['shots']=[dict(mode='Auto',accuracy='35%',tu='35%')];self.invalid(obj)

if __name__=='__main__':unittest.main()
