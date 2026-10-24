"""Nine real synthetic SDK peers; no SDK service or game process is involved."""
import os
from pathlib import Path
import socket
import subprocess
import sys
import unittest
import supervised_stance as s

class SdkStatusQueryMigrationTests(unittest.TestCase):
    def test_all_nine_synthetic_peers_echo_queries_and_initial_null(self):
        cases=(('stance','observe','request',{'observed':'stand','desired':'kneel'}),
               ('turn','observe_tactical','request_turn',{'x':16,'y':40}),
               ('map_left_click','observe_tactical','request_map_left_click',{'x':16,'y':40}),
               ('inventory','observe_tactical','request_open_inventory',{}),
               ('inventory_close','observe_inventory_close','request_close_inventory',{}),
               ('inventory_ground','observe_inventory_ground','request_inventory_ground',{}),
               ('inventory_click','observe_inventory_click','request_inventory_click',{'x':16,'y':80}),
               ('inventory_right_click','observe_inventory_right_click','request_inventory_right_click',{'x':16,'y':80}),
               ('inventory_unload','observe_inventory_unload','request_inventory_unload',{}))
        for index,(name,observe,request,extra) in enumerate(cases):
            with self.subTest(peer=name):
                parent,peer=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET)
                env=dict(os.environ,OPENXCOM_STANCE_FD=str(peer.fileno()))
                for flag in ('TURN','MAP_LEFT_CLICK','INVENTORY','INVENTORY_CLOSE','INVENTORY_GROUND','INVENTORY_CLICK','INVENTORY_RIGHT_CLICK','INVENTORY_UNLOAD'):
                    env['OPENXCOM_'+flag+'_ENABLED']='1'
                proc=subprocess.Popen([sys.executable,*(['-O'] if not __debug__ else []),str(Path(__file__).with_name('sdk_supervised_'+name+'_smoke.py')),'--synthetic-producer'],
                                      pass_fds=(peer.fileno(),),env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
                peer.close()
                client=s.StanceClient(parent,turn_enabled=True,map_left_click_enabled=True,inventory_enabled=True,
                    inventory_close_enabled=True,inventory_ground_enabled=True,inventory_click_enabled=True,
                    inventory_right_click_enabled=True,inventory_unload_enabled=True)
                try:
                    context,_=getattr(client,observe)();command=f'{index+1:032x}'
                    result=getattr(client,request)(capability=context['capability'],control_epoch=context['control_epoch'],command_id=command,**extra)
                    self.assertEqual(set(result),{'schema','command_id','status'})
                    self.assertEqual(result['status'],'pending_confirmation')
                    for ordinal in (1,2):
                        result=client.read_status(command)
                        self.assertEqual(set(result),{'schema','command_id','status'})
                        self.assertEqual(result['command_id'],command)
                        self.assertEqual(client._query_sequence,ordinal)
                finally:
                    client.close()
                    # Owned synthetic peer exits on ordinary EOF; never launches a game.
                    proc.wait(timeout=5)
                self.assertEqual(proc.returncode,0)

if __name__=='__main__':unittest.main()
