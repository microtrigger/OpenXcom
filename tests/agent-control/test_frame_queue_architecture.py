"""Source integration boundaries for the pure/runtime-shim queue tests."""
from pathlib import Path
import unittest
from test_strategy_widget_architecture import body
ROOT=Path(__file__).resolve().parents[2]/'src'
class FrameQueueArchitecture(unittest.TestCase):
 def test_poll_never_invokes_action(self):
  p=body('Observer/AgentController.cpp','AgentController::poll(')
  self.assertNotIn('agentDispatch(',p);self.assertNotIn('sample(current)',p)
  self.assertLess(p.index('invalidate(); // One-use'),p.index('_queued.stage('))
  self.assertIn('_queued.active()',p);self.assertIn('_queued.expired(now())',p)
 def test_queue_deadline_is_not_refreshed(self):
  p=body('Observer/AgentController.cpp','AgentController::poll(')
  self.assertLess(p.index('_queued.expired(now())'),p.index('recvmsg('))
  self.assertLess(p.index('AgentLedger::Duplicate'),p.index('_queued.stage('))
  self.assertNotIn('_queued.stage(',body('Observer/AgentController.cpp','AgentController::eventPage('))
  q=(ROOT/'Observer/AgentFrameCommand.h').read_text();self.assertIn('time - _queuedTime > 1000',q);self.assertIn('time - _grantTime > 120000',q)
 def test_fresh_frame_required(self):
  p=body('Observer/AgentController.cpp','AgentController::presented(')
  self.assertLess(p.index('!sample(ui)'),p.index('dispatchQueued(ui)'))
  d=body('Observer/AgentController.cpp','AgentController::dispatchQueued(')
  for guard in ['agentPresentationReady()', 'agentPaintHealthy()', '_queued.matchesFrame(', 'target->agentBindingCurrent(_queued.binding())']:
   self.assertIn(guard,d)
  self.assertLess(d.index('_queued.clear()'),d.index('target->agentDispatch(request)'))
 def test_preaction_and_reentrant_frames_cannot_fulfill_receipt(self):
  p=body('Observer/AgentController.cpp','AgentController::presented(')
  self.assertIn('|| _dispatchInProgress',p);self.assertIn('_presentationSerial > _dispatchPresentation',p)
  self.assertIn('dispatchQueued(ui);\n        return;',p)
  d=body('Observer/AgentController.cpp','AgentController::dispatchQueued(')
  self.assertLess(d.index('_dispatchInProgress = true'),d.index('target->agentDispatch(request)'))
 def test_close_input_and_eof_are_terminal_for_queue(self):
  c=body('Observer/AgentController.cpp','AgentController::close(');self.assertIn('_queued.clear()',c)
  d=body('Observer/AgentController.cpp','AgentController::dispatchQueued(')
  self.assertIn('!queuedInputClear() || !focused()',d);self.assertIn('if (!transportAlive()) { close(); return; }',d)
  t=body('Observer/AgentController.cpp','AgentController::transportAlive(')
  self.assertIn('POLLHUP | POLLERR | POLLNVAL',t);self.assertIn('MSG_PEEK | MSG_DONTWAIT',t)
  i=body('Observer/AgentController.cpp','AgentController::queuedInputClear(')
  self.assertIn('SDL_PEEKEVENT',i);self.assertNotIn('SDL_PushEvent',i);self.assertNotIn('SDL_GETEVENT',i)
 def test_original_private_binding_not_refreshed(self):
  q=(ROOT/'Observer/AgentFrameCommand.h').read_text()
  self.assertIn('_binding.sameBinding(shown)',q);self.assertIn('generation == _generation',q)
  self.assertEqual(q.count('_binding = binding'),1)
  for x in ['getUnits','getTile','getSide','RNG::','SDL_Delay']:self.assertNotIn(x,q)
if __name__=='__main__':unittest.main()
