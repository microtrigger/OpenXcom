"""Bounded /7 source contracts; no game execution."""
from pathlib import Path
import unittest
R=Path(__file__).resolve().parents[2]
def read(f): return (R/f).read_text()
class Gameplay(unittest.TestCase):
 def test_shared_map_dispatch(self):
  s=read('src/Battlescape/BattlescapeState.cpp');a=s[s.index('bool BattlescapeState::agentDispatch(const Observer::AgentRequest'):]
  self.assertIn('if (!cancelMapAction(button)) activateMapButton(button)',a);self.assertIn('_map->setSelectorPosition',a)
  for x in ('SDL_Event','SDL_PushEvent','new UnitWalkBState','new ProjectileFlyBState','primaryAction('):self.assertNotIn(x,a)
  self.assertIn('const Uint8 button = action->getDetails()->button.button',s)
 def test_private_camera_binding(self):
  s=read('src/Battlescape/BattlescapeState.cpp');self.assertIn('agentViewRevision()',s);self.assertIn('turnControlPoseRevision()',s)
  self.assertIn('out.points.push_back',read('src/Battlescape/BattlescapeState.cpp').replace('ui.points.push_back','out.points.push_back'))
  self.assertIn('points == o.points && context == o.context',read('src/Observer/AgentUi.cpp'))
 def test_no_hidden_target_projection(self):
  s=read('src/Battlescape/BattlescapeState.cpp');a=s[s.index('std::string agentTacticalJson'):s.index('bool BattlescapeState::agentBindingCurrent')]
  for x in ('getUnits()', 'getPosition()', 'getFaction()', 'getHealth()', 'getTimeUnits()', 'insideLand(', 'calculateLine(', 'isDiscovered('):self.assertNotIn(x,a)
  self.assertIn('_numVisibleUnit[i]->agentPaintedBounds',a)
  self.assertNotIn('agentWidgetVisible(_numVisibleUnit[i])',a)
 def test_normal_enemy_center(self):
  s=read('src/Battlescape/BattlescapeState.cpp');self.assertIn('centerVisibleUnit(static_cast<std::size_t>(btnID))',s);self.assertIn('centerVisibleUnit(shown.rows[index])',s)
 def test_normal_menu_shared(self):
  s=read('src/Battlescape/ActionMenuState.cpp');self.assertIn('activateItem(static_cast<int>(i))',s);self.assertIn('activateItem(static_cast<int>(shown.rows[row]))',s)
  a=s[s.index('bool ActionMenuState::agentUiState'):];self.assertNotIn('getTimeUnits()',a);self.assertNotIn('getAmmoItem()',a);self.assertNotIn('SDL_Event',a)
 def test_composite_clipping(self):
  s=read('src/Observer/AgentUi.cpp');self.assertIn('ink.x + ink.w > width',s);self.assertIn('ink.y + ink.h > height',s)
  self.assertIn('agentCompositeTextJson(_txtDescription',read('src/Battlescape/ActionMenuItem.cpp'))
 def test_warnings_only_drawn(self):
  s=read('src/Battlescape/WarningMessage.cpp');self.assertIn('Observer::agentTextJson(_text)',s)
  s=read('src/Battlescape/BattlescapeState.cpp');self.assertIn('out.displayedMessage = _warning->agentMessage()',s);self.assertIn('agentWidgetUncovered(_warning, _surfaces)',s)
  self.assertNotIn('displayedMessage = current->result',s)
 def test_pose_aba_hooks_active(self):
  s=read('src/Savegame/BattleUnit.cpp');self.assertIn('#if defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_TURN) || defined(OPENXCOM_EXPERIMENTAL_AGENT_CONTROL)',s)
  self.assertGreaterEqual(s.count('TurnPoseRevision poseRevision'),7)
 def test_fixture_is_separate_normal_setup(self):
  s=read('tests/agent-report-fixture/AgentReportFixture.cpp');self.assertIn('"gameplay-v1"',s)
  for x in ('RNG::','setSeed','PsiAttackBState','InfoboxState','ProjectileFlyBState','UnitWalkBState','YAML::LoadFile','std::ofstream'):self.assertNotIn(x,s)
  self.assertIn('"STR_RIGHT_HAND"',s);self.assertIn('"STR_LEFT_HAND"',s);self.assertIn('"BLANKS"',s)
 def test_physical_hover_stays_in_mouse_wrapper(self):
  s=read('src/Battlescape/BattlescapeState.cpp');physical=s[s.index('void BattlescapeState::mapClick'):s.index('bool BattlescapeState::cancelMapAction')]
  self.assertLess(physical.index('if (cancelMapAction(button)) return;'),physical.index('if (_mouseOverIcons) return;'))
  self.assertLess(physical.index('if (_mouseOverIcons) return;'),physical.index('activateMapButton(button);'))
 def test_explicit_target_does_not_inherit_hover(self):
  s=read('src/Battlescape/BattlescapeState.cpp');provider=s[s.index('bool BattlescapeState::agentUiState'):]
  self.assertNotIn('_mouseOverIcons',provider);self.assertNotIn('SDL_WarpMouse',provider)
  self.assertIn('!_isMouseScrolling && !_isMouseScrolled',provider)
 def test_shared_map_keeps_busy_cursor_and_ordinary_validity(self):
  s=read('src/Battlescape/BattlescapeState.cpp');shared=s[s.index('void BattlescapeState::activateMapButton'):s.index('void BattlescapeState::mapIn')]
  self.assertNotIn('_mouseOverIcons',shared)
  self.assertIn('_map->getCursorType() == CT_NONE || _battleGame->isBusy()',shared)
  self.assertIn('_save->getTile(pos) != 0',shared)
 def test_ambient_modifiers_rejected_before_dispatch(self):
  s=read('src/Observer/AgentController.cpp')
  self.assertIn('SDL_GetModState() == KMOD_NONE',s)
  self.assertIn('!top || !focused()',s)
  self.assertIn('target && target == _game->agentTopState() && focused()',s)
  self.assertIn('if (!queuedInputClear() || !focused())',s)
  self.assertNotIn('SDL_SetModState',s)
 def test_armored_fixture_is_explicit_stock_equipment_only(self):
  s=read('tests/agent-report-fixture/AgentReportFixture.cpp')
  self.assertIn('armored=equippedVehicle || std::string(argv[i+1])=="gameplay-armored-v1"',s)
  self.assertIn('gameplay=armored || std::string(argv[i+1])=="gameplay-v1"',s)
  self.assertIn('armored?(underwater?"STR_MAGNETIC_ION_ARMOR_UC":"STR_POWER_SUIT_UC"):rules->getArmor()',s)
  self.assertIn('person["armor"]=soldierArmor',s)
  self.assertIn('unitNode(1,FACTION_PLAYER,"",soldierArmor',s)
  self.assertIn('type: STR_POWER_SUIT_UC',read('bin/standard/xcom1/armors.rul'))
  self.assertIn('type: STR_MAGNETIC_ION_ARMOR_UC',read('bin/standard/xcom2/armors.rul'))
 def test_composite_dirty_state_only_completes_after_healthy_paint(self):
  s=read('src/Battlescape/ActionMenuItem.cpp');draw=s[s.index('void ActionMenuItem::draw()'):s.index('void ActionMenuItem::mouseIn')]
  self.assertIn('#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL',draw)
  self.assertIn('if (Observer::agentPaintHealthy()) _redraw = false;',draw)
  self.assertLess(draw.index('_txtTU->blit(this);'),draw.index('_redraw = false;'))
  self.assertNotIn('Surface::draw();',draw)
 def test_version_capacity(self):
  self.assertIn('openxcom-agent-control/8',read('src/Observer/AgentProtocol.cpp'));self.assertIn('_entries.size() >= 256',read('src/Observer/AgentProtocol.cpp'))
if __name__=='__main__':unittest.main()
