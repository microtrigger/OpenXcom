"""Narrow source boundaries; no game execution."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
CPP=(ROOT/'src/Battlescape/DebriefingState.cpp').read_text()
HEAD=(ROOT/'src/Battlescape/DebriefingState.h').read_text()
PROTO=(ROOT/'src/Observer/AgentProtocol.cpp').read_text()
PROVIDER=CPP.split('bool DebriefingState::agentUiState',1)[1].split('#endif',1)[0]
class DebriefArchitecture(unittest.TestCase):
 def test_explicit_flag(self): self.assertIn('#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL\n    bool agentUiState',HEAD)
 def test_top_modal(self): self.assertIn('_modal || !_game->isState',PROVIDER)
 def test_painted(self): self.assertIn('agentWidgetVisible(_window)',PROVIDER);self.assertIn('agentFlattenPaintOrder(_surfaces)',PROVIDER)
 def test_ink(self): self.assertIn('agentTextJson(widget)',PROVIDER);self.assertIn('agentWidgetUncovered',PROVIDER)
 def test_no_world_or_tables(self):
  for x in ['_stats','_soldierStats','_missionStatistics','getSavedGame','getSavedBattle','agentProjectList','_positiveScore','_showSoldierStats']:self.assertNotIn(x,PROVIDER)
 def test_table_limit(self): self.assertIn('table_status',PROVIDER);self.assertIn('unavailable',PROVIDER)
 def test_normal_handler(self): self.assertIn('btnOkClick(nullptr);',PROVIDER);self.assertNotIn('setState(',PROVIDER)
 def test_skirmish_main_menu(self): self.assertIn('getMonthsPassed() == -1',CPP);self.assertIn('_game->setState(new MainMenuState);',CPP)
 def test_schema(self): self.assertIn('openxcom-agent-control/8',PROTO);self.assertNotIn('openxcom-agent-control/7',PROTO)
 def test_no_unwitnessed_state(self): self.assertIn("return !out.regions.empty()",PROVIDER)
 def test_no_stats_action(self): self.assertNotIn('AgentUiAction("show',PROVIDER)
if __name__=='__main__':unittest.main()
