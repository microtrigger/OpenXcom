# SPDX-License-Identifier: GPL-3.0-or-later
"""64c DTO regression only; native paint/row identity require native fixtures."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, CHALLENGE, EPOCH

def rows(obj):
    obj['visible']['rows']=[dict(handle='r'+str(i),cells=c,selected=False) for i,c in enumerate([
        ['INTERCEPTOR','READY','TEST_MULTI64','2'],['INTERCEPTOR',None,'TEST_MULTI64',None],[None]*4])]
    return obj

class InterceptCells64Tests(unittest.TestCase):
    def test_independent_cells_three_controls(self):
        obj=rows(state(screen='intercept_craft'));a.validate_state(obj,CHALLENGE,EPOCH)
        for i in range(3):
            for j in range(4):
                sample=copy.deepcopy(obj);sample['visible']['rows'][i]['cells'][j]=None
                a.validate_state(sample,CHALLENGE,EPOCH)
    def test_no_implicit_identity(self):
        for key in ('craft_id','native_index','eligible','fuel'):
            obj=rows(state(screen='intercept_craft'));obj['visible']['rows'][0][key]=1
            with self.assertRaises(Unavailable):a.validate_state(obj,CHALLENGE,EPOCH)
    def test_each_handle_dispatches_once_even_null_name(self):
        class Peer(NativeFixture):
            def observation(self,challenge,capability):
                return rows(super().observation(challenge,capability))
        for i in range(3):
            peer=Peer(screen='intercept_craft');self.addCleanup(peer.close)
            old=peer.client.observe();fresh=peer.client.observe()
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action='choose_visible_row',args={'row':'r'+str(i)}))
            cmd=arguments(fresh,action='choose_visible_row',args={'row':'r'+str(i)})
            result=peer.client.execute(**cmd);self.assertEqual(result['status'],'dispatched')
            self.assertEqual(peer.client.execute(**cmd),result)
            self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),1)

if __name__=='__main__':unittest.main()
