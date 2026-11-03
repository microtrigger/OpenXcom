"""Regression for private daemon exception privacy; no process/game launch."""
import io,json,threading,unittest
from unittest.mock import Mock,patch
import launch_presentation as l
class PrivateLauncher63(unittest.TestCase):
 def failure(self,fn,obj,monitor):
  with patch('sys.stderr',io.StringIO()) as stream:
   t=threading.Thread(target=fn,args=(obj,));t.start();t.join(2);self.assertFalse(t.is_alive());s=stream.getvalue()
  self.assertNotIn('SENTINEL',s);self.assertNotIn('Traceback',s)
  self.assertEqual([json.loads(x)for x in s.splitlines()],[{'diagnostic':'private_monitor_failure','monitor':monitor}])
 def test_wait_error_bounded(self):
  p=Mock();p.wait.side_effect=OSError('PRIVATE-SENTINEL');self.failure(l.monitor_native_exit,p,'native_exit');p.poll.assert_not_called();p.kill.assert_not_called();p.terminate.assert_not_called()
 def test_poll_error_bounded(self):
  p=Mock();p.wait.return_value=0;p.poll.side_effect=OSError('PRIVATE-SENTINEL');self.failure(l.monitor_native_exit,p,'native_exit');p.kill.assert_not_called();p.terminate.assert_not_called()
 def test_read_error_bounded(self):
  p=Mock();p.read.side_effect=OSError('PRIVATE-SENTINEL');self.failure(l.monitor_startup,p,'startup');p.close.assert_called_once()
 def test_close_error_bounded(self):
  p=Mock();p.read.return_value=b'';p.close.side_effect=OSError('PRIVATE-SENTINEL');self.failure(l.monitor_startup,p,'startup')
 def test_no_raw_startup_echo(self):
  with patch('sys.stderr',io.StringIO())as out:l.monitor_startup(io.BytesIO(b'PRIVATE-SENTINEL\nX Error of failed request BadValue'))
  self.assertEqual(json.loads(out.getvalue()),{'diagnostic':'startup_fixed_marker','marker':'x11_badvalue'})
 def test_diagnostic_assertion_rejects_raw_or_unbounded_output(self):
  from private_launcher_fixture import assert_private_stderr
  for raw in ('PRIVATE-SENTINEL','Traceback secret',json.dumps({'diagnostic':'native_process_status','status':'exited','exit_code':0,'signal_number':None,'hidden':'SECRET'}),json.dumps({'diagnostic':'private_monitor_failure','monitor':'SECRET'})):
   with self.assertRaises((AssertionError,ValueError)):assert_private_stderr(self,raw)
 def test_diagnostic_output_failure_is_quiet(self):
  p=Mock();p.wait.side_effect=OSError('PRIVATE-SENTINEL')
  with patch('sys.stderr')as err:err.write.side_effect=OSError('OTHER-SENTINEL');l.monitor_native_exit(p)
if __name__=='__main__':unittest.main()
