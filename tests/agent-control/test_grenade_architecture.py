"""Bounded /13 source-contract checks. No runtime acceptance."""
from pathlib import Path
import hashlib,json,re,unittest
S=Path(__file__).resolve().parents[2];R=S.parent
B=R.parent/'agent-control-inventory-body-v12-20261018/source'
def method(t,sig):
 a=t.index(sig);b=t.index('{',a);n=1;i=b+1
 while n:
  n+=(t[i]=='{')-(t[i]=='}');i+=1
 return t[a:i]
class Contract(unittest.TestCase):
 def test_prior_source_preserved(self):
  for p,h in json.loads((R/'SOURCE-FROM-V12-SHA256.json').read_text()).items():
   self.assertEqual(hashlib.sha256((B/p).read_bytes()).hexdigest(),h,p)
 def test_unchanged_gameplay_and_fixture(self):
  for p in ['src/Battlescape/BattlescapeGame.cpp','src/Battlescape/ProjectileFlyBState.cpp','src/Battlescape/Map.cpp','src/Battlescape/BattlescapeState.cpp','src/Observer/AgentController.cpp','src/Observer/AgentFrameCommand.h']:
   self.assertEqual((S/p).read_bytes(),(B/p).read_bytes(),p)
  for p in (B/'tests/agent-report-fixture').glob('*'):
   if p.is_file():self.assertEqual(p.read_bytes(),(S/'tests/agent-report-fixture'/p.name).read_bytes())
 def test_ordinary_menu_handler_unchanged(self):
  p='src/Battlescape/ActionMenuState.cpp';a=(S/p).read_text();b=(B/p).read_text()
  for sig in ['void ActionMenuState::activateItem','void ActionMenuState::closeMenu','void ActionMenuState::handle','void ActionMenuState::btnActionMenuItemClick']:
   self.assertEqual(method(a,sig),method(b,sig))
 def test_only_five_menu_types(self):
  t=method((S/'src/Battlescape/ActionMenuState.cpp').read_text(),'bool ActionMenuState::agentUiState')
  self.assertIn('type != BA_PRIME && type != BA_THROW',t)
  for kind in ['BA_USE','BA_HIT','BA_LAUNCH','BA_PANIC','BA_MINDCONTROL']:self.assertNotIn(kind,t)
  self.assertIn('choose_weapon_action',t);self.assertNotIn('choose_fire_mode',t)
 def test_ordinary_timer_constructor_unchanged(self):
  p='src/Battlescape/PrimeGrenadeState.cpp';a=(S/p).read_text();b=(B/p).read_text()
  self.assertEqual(method(a,'PrimeGrenadeState::PrimeGrenadeState'),method(b,'PrimeGrenadeState::PrimeGrenadeState'))
  for literal in ['new Text(192, 24, 65, 44)','new InteractiveSurface(22, 22','new Text(20, 20','i < 24','(i%8)*24','(i/8)*25']:
   self.assertIn(literal,a)
 def test_tactical_only_and_public_projection(self):
  t=method((S/'src/Battlescape/PrimeGrenadeState.cpp').read_text(),'bool PrimeGrenadeState::agentUiState')
  for literal in ['if (_inInventoryView || !_action || _modal','agentWidgetUncovered','agentTextJson','agentPaintedBounds','agentRegionsDisjoint','out.regions.empty()']:
   self.assertIn(literal,t)
  for hidden in ['getFuseTimer','getRules','getTimeUnits','getUnits','getItems','getInventory','spendTimeUnits','setFuseTimer','getPosition']:
   self.assertNotIn(hidden,t)
 def test_timer_handler_is_only_pending_selection(self):
  t=(S/'src/Battlescape/PrimeGrenadeState.cpp').read_text()
  for sig in ['void PrimeGrenadeState::selectTimer','void PrimeGrenadeState::cancelTimer']:
   body=method(t,sig);self.assertNotIn('spendTimeUnits',body);self.assertNotIn('warning',body)
  body=method(t,'void PrimeGrenadeState::selectTimer');self.assertIn('if (_inInventoryView) _grenadeInInventory->setFuseTimer(timer);',body)
  self.assertIn('else _action->value = timer;',body)
 def test_late_prime_success_follows_spend(self):
  t=method((S/'src/Battlescape/BattlescapeGame.cpp').read_text(),'void BattlescapeGame::handleNonTargetAction')
  self.assertLess(t.index('spendTimeUnits'),t.index('STR_GRENADE_IS_ACTIVATED'))
  self.assertLess(t.index('STR_GRENADE_IS_ACTIVATED'),t.index('setFuseTimer'))
  self.assertIn('STR_NOT_ENOUGH_TIME_UNITS',t)
 def test_no_transport_or_event_expansion(self):
  for p in ['src/Observer/AgentController.h','src/Observer/AgentPublicEvents.h','src/Observer/AgentProtocol.h','src/Observer/AgentUi.h']:
   self.assertEqual((S/p).read_bytes(),(B/p).read_bytes(),p)
 def test_named_fixture_aliases_unchanged(self):
  p='tests/agent-report-fixture/AgentReportFixture.cpp';self.assertEqual((S/p).read_bytes(),(B/p).read_bytes())
if __name__=='__main__':unittest.main()
