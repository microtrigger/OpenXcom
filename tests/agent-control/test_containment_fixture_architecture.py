#!/usr/bin/env python3
"""Source-bound containment setup checks; no game/resources/saves executed."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
class ContainmentFixtureArchitecture(unittest.TestCase):
 def setUp(self):
  self.source=(ROOT/'tests/agent-report-fixture/AgentReportFixture.cpp').read_text()
  self.branch=self.source.split('    if (containmentCampaign)\n    {',1)[1].split('    if (outfittingCampaign)',1)[0]
 def test_default_off_and_explicit_runtime_gate(self):
  cm=(ROOT/'CMakeLists.txt').read_text()
  self.assertIn('EXPERIMENTAL_AGENT_REPORT_FIXTURE "Build isolated ordinary-AI report acceptance setup (explicit runtime alias required)" OFF',cm)
  self.assertIn('EXPERIMENTAL_AGENT_REPORT_FIXTURE AND NOT EXPERIMENTAL_AGENT_CONTROL',cm)
  for text in ['--agent-report-fixture','containment-stock-v1','OPENXCOM_AGENT_CONTROL','!requested && i+1<argc','if(!requested || finished) return false;']:self.assertIn(text,self.source)
 def test_existing_save_and_debug_guards(self):
  for text in ['!game->getSavedGame()','!Options::autosave','!Options::debug','!Options::traceAI','Options::getActiveMods().size()==1','master=="xcom1" || master=="xcom2"']:self.assertIn(text,self.source)
  self.assertIn('require(!Options::debugUi)',self.branch)
 def test_stock_newsave_only_two_owned_captives(self):
  for text in ['campaign(mod->newSave())','STR_ALIEN_CONTAINMENT','STR_AQUATOID_SOLDIER','STR_SECTOID_SOLDIER','item->isAlien()','getAvailableContainment()==0','getUsedContainment()==0','addItem(captive,2)','getUsedContainment()==2']:self.assertIn(text,self.branch)
  self.assertEqual(self.branch.count('new BaseFacility('),1)
 def test_free_inbounds_connected_completed_facility(self):
  for text in ['bool occupied[6][6]={}','x+size<=6 && y+size<=6','require(!occupied[x+dx][y+dy])','if(!occupied[x][y]','occupied[x-1][y]','require(targetX>=0)','setBuildTime(0)']:self.assertIn(text,self.branch)
 def test_no_battle_research_capture_history_or_files(self):
  for text in ['SavedBattleGame>','BattleUnit','addFinishedResearch','getDiary','getDeadSoldiers','YAML::LoadFile','save->load','campaign->load','ofstream','fopen','setSeed','calculateFOV','getUnit(']:self.assertNotIn(text,self.branch)
  self.assertGreaterEqual(self.branch.count('getDiscoveredResearch().empty()'),2)
  self.assertGreaterEqual(self.branch.count('!campaign->getSavedBattle()'),2)
 def test_normal_geoscape_one_shot_and_craft_coordinates(self):
  for text in ['game->setSavedGame(campaign.release())','game->setState(new GeoscapeState)','finished=true;return true','craft->setLongitude(longitude);craft->setLatitude(latitude)']:self.assertIn(text,self.branch)
  self.assertLess(self.source.index('if (containmentCampaign)'),self.source.index('const std::string dataset='))
if __name__=='__main__':unittest.main()
