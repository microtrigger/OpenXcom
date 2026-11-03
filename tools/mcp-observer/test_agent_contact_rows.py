# SPDX-License-Identifier: GPL-3.0-or-later
"""Wire56 printed contact cells; no native hidden-list visibility proof."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, dump, result, COMMAND, EPOCH
from test_agent_events import page, validate

def contact():
    obj=state(screen='ufo_contact')
    obj['visible'].update(details_status='partial',contact_rows=[['Size','Small'],['Altitude','Very low'],['Heading','North'],['Speed','1,200']],hyperwave_rows=None)
    return obj
class ContactRowTests(unittest.TestCase):
    def valid(self,obj):return a.validate_state(obj,obj['challenge'],obj['epoch'])
    def invalid(self,obj):
        with self.assertRaises(Unavailable):self.valid(obj)
    def test_schema_and_actions_unchanged(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/66');self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS)),(286,109));self.valid(contact())
        self.assertNotIn('center_ufo_contact',a.ACTIONS)
    def test_independent_nullable_tables_and_cells(self):
        for ordinary in (None,[],[[None,None]],[[None,'1,234']]):
            for hyper in (None,[],[['Shown label',None]]):
                obj=contact();obj['visible'].update(contact_rows=ordinary,hyperwave_rows=hyper,details_status='unavailable' if ordinary is None and hyper is None else 'partial');self.valid(obj)
    def test_status_consistency(self):
        obj=contact();obj['visible']['details_status']='unavailable';self.invalid(obj)
        obj=state(screen='ufo_contact');obj['visible']['details_status']='partial';self.invalid(obj)
        for status in ('complete','presented',None,True):
            obj=contact();obj['visible']['details_status']=status;self.invalid(obj)
    def test_four_by_two_exact_bounds(self):
        for key in ('contact_rows','hyperwave_rows'):
            for bad in ([[None,None]]*5,[[None]],[[None,None,None]],[{}],{},True,'text'):
                obj=contact();obj['visible'][key]=bad;self.invalid(obj)
            obj=contact();obj['visible'][key]=[[None,None]]*4;self.valid(obj)
    def test_strict_text_bounds(self):
        for bad in (True,1,1.0,{},[], 'x'*1025,'é'*513):
            obj=contact();obj['visible']['contact_rows'][0][1]=bad;self.invalid(obj)
        obj=contact();obj['visible']['contact_rows'][0][1]='é'*512;self.valid(obj)
    def test_no_hidden_model_or_inferred_status_fields(self):
        for key in ('race','mission','zone','ufo_id','status','altitude_id','position','speed','hidden_rows','model'):
            obj=contact();obj['visible'][key]='secret';self.invalid(obj)
        obj=contact();obj['visible']['hyperwave_rows']=[dict(label='Race',value='Shown')];self.invalid(obj)
    def test_missing_keys_old_schema_rejected(self):
        for key in ('contact_rows','hyperwave_rows'):
            obj=contact();del obj['visible'][key];self.invalid(obj)
        obj=contact();obj['schema']='openxcom-agent-control/55';self.invalid(obj)
    def test_formatted_native_values_not_coerced(self):
        obj=contact();obj['visible']['contact_rows']=[['Altitude','Grounded'],['Heading','NONE'],['Speed','1\u00a0234']];self.valid(obj)
        self.assertEqual(obj['visible']['contact_rows'][-1][1],'1\u00a0234')
    def test_result_and_retained_events(self):
        obj=contact();obj.update(challenge=None,capability=None,event_sequence=2)
        a.validate_result(dump(result(observation=obj)),COMMAND,EPOCH)
        validate(page([dict(seq=1,kind='presentation',state='ufo_contact',visible=obj['visible'])]))
    def test_partial_cells_do_not_mint_button_authority(self):
        obj=contact();obj['actions']=[];obj['visible']['buttons']=[];obj['capability']=None;self.valid(obj)

if __name__=='__main__':unittest.main()
