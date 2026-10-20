"""Fixture-only ownership regression; source checks, not live debrief acceptance."""
from pathlib import Path
import unittest
R=Path(__file__).resolve().parents[2]
class MissionContext(unittest.TestCase):
 def setUp(self): self.s=(R/'tests/agent-report-fixture/AgentReportFixture.cpp').read_text()
 def test_separate_alias_preserves_old_profiles(self):
  self.assertIn('missionContext=std::string(argv[i+1])=="gameplay-mission-v1";',self.s)
  self.assertIn('equippedVehicle=missionContext || std::string(argv[i+1])=="gameplay-vehicle-v1";',self.s)
  self.assertEqual(self.s.count('if (missionContext)'),1)
 def test_craft_owned_by_base_and_soldier_assigned_before_load(self):
  for x in ['craft->setInBattlescape(true);','base->getSoldiers()->front()->setCraft(craft.get());','base->getCrafts()->push_back(craft.release());']:
   self.assertLess(self.s.index(x),self.s.index('tactical->load(battle,mod,save.get())'))
  self.assertIn('new Craft(mod->getCraft(underwater?"STR_TRITON":"STR_SKYRANGER",true),base,1)',self.s)
 def test_not_base_defense_or_persistent_campaign(self):
  self.assertIn('save->getMonthsPassed()==-1 && !base->isInBattlescape()',self.s)
  for x in ['base->setInBattlescape','setMonthsPassed','setBattleAborted','setDebugMode','setSeed','RNG::']:
   self.assertNotIn(x,self.s)
 def test_stock_craft_ids_exist(self):
  for master,name in [('xcom1','STR_SKYRANGER'),('xcom2','STR_TRITON')]:
   self.assertIn('type: '+name,(R/'bin/standard'/master/'crafts.rul').read_text())
 def test_ordinary_debrief_discovers_owner_before_recovery(self):
  s=(R/'src/Battlescape/DebriefingState.cpp').read_text();start=s.index('for (std::vector<Base*>::iterator i = save->getBases()->begin()')
  part=s[start:s.index('bool lostCraft = false;',start)]
  self.assertLess(part.index('if ((*j)->isInBattlescape())'),part.index('base = (*i);'))
  self.assertLess(part.index('base = (*i);'),part.index('recoverItems('))
  self.assertIn('craft->setInBattlescape(false);',part)
 def test_ordinary_loss_and_abort_keep_owner_for_recovery(self):
  s=(R/'src/Battlescape/DebriefingState.cpp').read_text();a=s.index('bool lostCraft = false;');b=s.index('// calculate the clips',a);p=s[a:b]
  self.assertIn('((playersInExitArea == 0 && aborted) || (playersSurvived == 0))',p)
  self.assertIn('base->removeCraft(craft, false);',p)
  self.assertIn('craft = 0;',p);self.assertNotIn('base = 0;',p)
if __name__=='__main__':unittest.main(verbosity=2)
