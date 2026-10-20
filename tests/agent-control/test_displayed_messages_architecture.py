#!/usr/bin/env python3
"""Source boundary assertions, not a renderer or live enemy-action test."""
import pathlib,unittest
from test_strategy_widget_architecture import body
ROOT=pathlib.Path(__file__).resolve().parents[2]/'src'
class DisplayedMessagesArchitecture(unittest.TestCase):
 def test_providers_use_only_complete_drawn_widgets(self):
  for name,widget in [('InfoboxState','_text'),('InfoboxOKState','_txtTitle')]:
   s=body('Battlescape/'+name+'.cpp',name+'::agentUiState(')
   for token in ('_modal','_game->isState','agentWidgetVisible(_frame)','agentWidgetUncovered('+widget,'agentTextJson('+widget,'message != "null"','out.displayedMessage = '+widget+'->getText()'):self.assertIn(token,s)
   for token in ('getUnits','getFaction','getSide','getId','getPosition','tr(','STR_','getHealth','getTimeUnits'):self.assertNotIn(token,s)
 def test_ok_only_ordinary_visible_handler(self):
  s=body('Battlescape/InfoboxOKState.cpp','InfoboxOKState::agentDispatch(')
  for token in ('current.offers(action)','btnOkClick(nullptr)','acknowledge_message'):self.assertIn(token,s)
  p=body('Battlescape/InfoboxOKState.cpp','InfoboxOKState::agentUiState(')
  self.assertIn('agentWidgetUncovered(_btnOk, _surfaces)',p)
  self.assertIn('agentButtonLabel(_btnOk)',p)
  self.assertNotIn('agentDispatch', (ROOT/'Battlescape/InfoboxState.h').read_text())
 def test_real_two_second_timer_unchanged(self):
  self.assertIn('INFOBOX_DELAY = 2000',(ROOT/'Battlescape/InfoboxState.h').read_text())
  self.assertIn('_timer->think(this, 0)',body('Battlescape/InfoboxState.cpp','InfoboxState::think('))
  self.assertIn('_game->popState()',body('Battlescape/InfoboxState.cpp','InfoboxState::close('))
 def test_final_frame_gate_before_message_capture(self):
  s=body('Observer/AgentController.cpp','AgentController::presented(')
  self.assertLess(s.index('!agentPaintHealthy()'),s.index('_publicEvents.displayedMessage'))
  self.assertLess(s.index('_frameGeneration != _generation || !sample(ui)'),s.index('_publicEvents.displayedMessage'))
  p=body('Observer/AgentController.cpp','AgentController::sample(')
  for token in ('agentPresentationReady()','agentRegionsDisjoint(out)','agentOverlayClear(out)'):self.assertIn(token,p)
  g=body('Engine/Game.cpp','Game::run(')
  self.assertLess(g.index('_screen->clear()'),g.index('_agentController.presented()'))
  self.assertLess(g.index('_screen->flip()'),g.index('_agentController.presented()'))
 def test_absence_only_admitted_map_not_private_screen_flag(self):
  p=body('Battlescape/BattlescapeState.cpp','BattlescapeState::agentUiState(')
  self.assertIn('out.displayedMessageAbsent = tactical.find("{\\"mode\\":\\"map\\",") == 0',p)
  self.assertNotIn('isScreen()',body('Observer/AgentController.cpp','AgentController::presented('))
 def test_reducer_has_no_hidden_cause_or_clock(self):
  p=(ROOT/'Observer/AgentDisplayedMessages.h').read_text()
  for token in ('getUnits','getFaction','getId','getPosition','SDL_','RNG::','STR_','BattleUnit','PsiAttack','InfoboxState'):self.assertNotIn(token,p)
  self.assertIn('if (knownAbsent) _last.clear()',p)
  self.assertIn('text == _last',p)
 def test_no_poll_dependency(self):
  for method in ('poll','eventPage','statePacket','receipt'):
   self.assertNotIn('displayedMessage(',body('Observer/AgentController.cpp','AgentController::'+method+'('))
if __name__=='__main__':unittest.main(verbosity=2)
