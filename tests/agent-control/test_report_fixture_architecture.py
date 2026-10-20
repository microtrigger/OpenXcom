#!/usr/bin/env python3
"""Test-only setup source contracts. No fixture/game/AI execution."""
import pathlib,unittest
from test_strategy_widget_architecture import body
ROOT=pathlib.Path(__file__).resolve().parents[2]
class ReportFixtureArchitecture(unittest.TestCase):
 def test_default_off_and_explicit_alias(self):
  cm=(ROOT/'CMakeLists.txt').read_text();self.assertIn('EXPERIMENTAL_AGENT_REPORT_FIXTURE "Build isolated ordinary-AI report acceptance setup (explicit runtime alias required)" OFF',cm)
  f=(ROOT/'tests/agent-report-fixture/AgentReportFixture.cpp').read_text()
  for s in ('--agent-report-fixture','alien-control-v1','!requested && i+1<argc','OPENXCOM_AGENT_CONTROL'):self.assertIn(s,f)
 def test_only_synthetic_in_memory_no_save_seed_or_injected_action(self):
  f=(ROOT/'tests/agent-report-fixture/AgentReportFixture.cpp').read_text()
  for s in ('new SavedGame','new SavedBattleGame','tactical->load(battle,mod,save.get())','new BattlescapeState','finished=true'):self.assertIn(s,f)
  for s in ('YAML::LoadFile','save->load(','.save(', 'RNG::','setSeed','InfoboxState','PsiAttackBState','BA_MINDCONTROL','statePushBack','statePushFront','ofstream','getenv("OPENXCOM_AGENT_CONTROL_FD")'):self.assertNotIn(s,f)
 def test_guards_before_transport_activation(self):
  f=(ROOT/'tests/agent-report-fixture/AgentReportFixture.cpp').read_text()
  for s in ('!game->getSavedGame()','!Options::autosave','Options::getActiveMods().size()==1','master=="xcom1" || master=="xcom2"','path->getTotalTUCost()<=24','alien->getTimeUnits()>49'):self.assertIn(s,f)
  c=body('Observer/AgentController.cpp','AgentController::poll(')
  self.assertLess(c.index('AgentReportFixture::waiting()'),c.index('recvmsg('))
 def test_turn_one_morale_path_is_ordinary(self):
  f=(ROOT/'tests/agent-report-fixture/AgentReportFixture.cpp').read_text();self.assertIn('battle["turn"]=1',f);self.assertIn('victim["morale"]=0',f)
  g=(ROOT/'src/Battlescape/BattlescapeGame.cpp').read_text();self.assertIn('_playerPanicHandled(true)',g)
  init=body('Battlescape/BattlescapeGame.cpp','BattlescapeGame::init(');self.assertIn('_save->getTurn() > 1',init)
  end=body('Savegame/SavedBattleGame.cpp','SavedBattleGame::endTurn(');self.assertIn('if ((*i)->getFaction() == _side)',end)
 def test_ordinary_patrol_map_is_not_empty(self):
  f=(ROOT/'tests/agent-report-fixture/AgentReportFixture.cpp').read_text()
  self.assertIn('battle["nodes"].push_back(route)',f);self.assertIn('tactical->getNodes()->size()==1',f)
 def test_real_report_only_from_normal_success_path(self):
  p=body('Battlescape/PsiAttackBState.cpp','PsiAttackBState::psiAttack(')
  self.assertLess(p.index('attackStrength > defenseStrength'),p.index('STR_IS_UNDER_ALIEN_CONTROL'))
  self.assertLess(p.index('convertToFaction'),p.index('STR_IS_UNDER_ALIEN_CONTROL'))
  a=body('Battlescape/BattlescapeGame.cpp','BattlescapeGame::handleAI(')
  self.assertIn('unit->think(&action)',a);self.assertIn('statePushBack(new PsiAttackBState(this, action))',a)
if __name__=='__main__':unittest.main(verbosity=2)
