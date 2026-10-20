"""Bounded equipped-vehicle fixture source checks. Never execute the game."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
class VehicleFixtureArchitecture(unittest.TestCase):
 def test_alias_is_explicit_and_old_equipment_stays_gated(self):
  s=(ROOT/'tests/agent-report-fixture/AgentReportFixture.cpp').read_text()
  self.assertIn('equippedVehicle=std::string(argv[i+1])=="gameplay-vehicle-v1";',s)
  self.assertIn('armored=equippedVehicle || std::string(argv[i+1])=="gameplay-armored-v1";',s)
  self.assertEqual(s.count('if (equippedVehicle)'),2)
  self.assertIn('gameplay=armored || std::string(argv[i+1])=="gameplay-v1";',s)
 def test_stock_equipment_serialized_before_ordinary_load(self):
  s=(ROOT/'tests/agent-report-fixture/AgentReportFixture.cpp').read_text()
  self.assertLess(s.index('appendVehicleArmament('),s.index('tactical->load(battle,mod,save.get())'))
  for text in ['gun->isFixed()', 'gun->getBattleType()==BT_FIREARM','gun->getCompatibleAmmo()->front()', 'rounds->getClipSize()', 'gun->getTurretType()']:
   self.assertIn(text,s)
  self.assertLess(s.index('vehicleUnit->getTurretType()==gun->getRules()->getTurretType()'),s.index('finished=true'))
 def test_no_live_controller_or_hidden_weapon_fields(self):
  s=(ROOT/'tests/agent-report-fixture/AgentVehicleFixture.h').read_text()
  for text in ['RNG::','setSeed','getUnits','getTile','agentDispatch','has_weapon','ammo_reason','weapon_name','SDL_','Game *']:
   self.assertNotIn(text,s)
  for folder in ['src/Observer','src/Battlescape']:
   self.assertFalse(any('AgentVehicleFixture.h' in p.read_text() for p in (ROOT/folder).glob('*.cpp')))
 def test_default_off_and_stock_generator_parity(self):
  c=(ROOT/'CMakeLists.txt').read_text();self.assertIn('EXPERIMENTAL_AGENT_REPORT_FIXTURE "Build isolated ordinary-AI report acceptance setup (explicit runtime alias required)" OFF',c)
  g=(ROOT/'src/Battlescape/BattlescapeGenerator.cpp').read_text()
  for text in ['getCompatibleAmmo()->front()', 'unit->setTurretType(v->getRules()->getTurretType())', 'item->setSlot(!rightWeapon ? rightHand : leftHand)']:
   self.assertIn(text,g)
  p=(ROOT/'src/Observer/AgentProtocol.cpp').read_text();self.assertIn('openxcom-agent-control/7',p)
if __name__=='__main__':unittest.main(verbosity=2)
