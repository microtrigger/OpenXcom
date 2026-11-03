"""Finite typed doubles and exact private-diagnostic assertions; test-only."""
import io,json

def finite_process():
 return {'return_value.stderr':io.BytesIO(b''),'return_value.poll.return_value':0,'return_value.wait.return_value':0}

def assert_private_stderr(test,data,tail=b'',allow_failure=False):
 if type(data)is str:data=data.encode()
 if type(tail)is str:tail=tail.encode()
 if tail:
  test.assertEqual(data.count(tail),1);data=data.replace(tail,b'',1)
 lines=data.splitlines();test.assertLessEqual(len(lines),2)
 for line in lines:
  q=json.loads(line)
  if allow_failure and q.get('diagnostic')=='private_monitor_failure':
   test.assertEqual(set(q),{'diagnostic','monitor'});test.assertIn(q['monitor'],('native_exit','startup'));continue
  test.assertEqual(set(q),{'diagnostic','status','exit_code','signal_number'});test.assertEqual(q['diagnostic'],'native_process_status')
  test.assertIn(q['status'],('running_at_observer_close','exited'))
  test.assertTrue(q['exit_code'] is None or type(q['exit_code'])is int and q['exit_code']>=0)
  test.assertTrue(q['signal_number'] is None or type(q['signal_number'])is int and q['signal_number']>0)
  if q['status']=='running_at_observer_close':test.assertIsNone(q['exit_code']);test.assertIsNone(q['signal_number'])
 test.assertNotIn(b'PRIVATE',data);test.assertNotIn(b'Traceback',data)
