"""Command generations belong to agent-control, never legacy supervised status."""
import unittest
from unittest.mock import Mock
import adapter

class SupervisedGenerationBoundaryTests(unittest.TestCase):
    def test_generation_argument_is_rejected_before_supervised_status_io(self):
        client = type('Client', (), {})()
        client.read_status = Mock(side_effect=AssertionError('unexpected supervised I/O'))
        server = adapter.Server(supervised_stance=client)
        server.state = 'ready'
        for generation in (1, 2, True, 0, '1', None):
            with self.subTest(generation=generation):
                reply = server.handle({'jsonrpc':'2.0','id':1,'method':'tools/call',
                    'params':{'name':'read_stance_status','arguments':
                        {'command_id':'a'*32,'command_generation':generation}}})
                self.assertEqual(reply['error']['code'], -32602)
        client.read_status.assert_not_called()

if __name__ == '__main__':
    unittest.main()
