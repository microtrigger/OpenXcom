"""Setup-only stock inventory contracts; no game, GUI or packing acceptance."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class InventoryFixtureArchitecture(unittest.TestCase):
    def setUp(self):
        self.fixture = (ROOT / "tests/agent-report-fixture/AgentReportFixture.cpp").read_text()
        self.stock = (ROOT / "tests/agent-report-fixture/AgentInventoryFixture.h").read_text()

    def test_explicit_aliases_keep_old_profiles_and_mission_ownership(self):
        for alias in ("alien-control-v1", "gameplay-v1", "gameplay-armored-v1",
                      "gameplay-vehicle-v1", "gameplay-mission-v1",
                      "inventory-body-v1", "inventory-paging-v1"):
            self.assertIn('std::string(argv[i+1])=="' + alias + '"', self.fixture)
        chain = ['inventoryPaging=std::string(argv[i+1])=="inventory-paging-v1";',
                 'inventoryBody=inventoryPaging || std::string(argv[i+1])=="inventory-body-v1";',
                 'missionContext=std::string(argv[i+1])=="gameplay-mission-v1";',
                 'missionContext=missionContext || inventoryBody;',
                 'equippedVehicle=missionContext || std::string(argv[i+1])=="gameplay-vehicle-v1";',
                 'armored=equippedVehicle || std::string(argv[i+1])=="gameplay-armored-v1";',
                 'gameplay=armored || std::string(argv[i+1])=="gameplay-v1";']
        locations = [self.fixture.index(line) for line in chain]
        self.assertEqual(locations, sorted(locations))
        self.assertEqual(self.fixture.count('if (inventoryBody)'), 1)
        self.assertEqual(self.fixture.count('if (missionContext)'), 1)
        for line in ['craft->setInBattlescape(true);', 'base->getSoldiers()->front()->setCraft(craft.get());',
                     'base->getCrafts()->push_back(craft.release());']:
            self.assertLess(self.fixture.index(line), self.fixture.index('tactical->load(battle,mod,save.get())'))

    def test_default_off_and_setup_only_before_normal_load(self):
        cmake = (ROOT / "CMakeLists.txt").read_text()
        self.assertIn('EXPERIMENTAL_AGENT_REPORT_FIXTURE "Build isolated ordinary-AI report acceptance setup (explicit runtime alias required)" OFF', cmake)
        self.assertIn('require(mode && std::string(mode)=="1");', self.fixture)
        self.assertLess(self.fixture.index('appendInventoryStock('), self.fixture.index('tactical->load(battle,mod,save.get())'))
        tactical_commit = self.fixture.index('save->setBattleGame(tactical.release())')
        self.assertLess(self.fixture.index('tactical->load(battle,mod,save.get())'), self.fixture.index('finished=true;', tactical_commit))
        for folder in ("src/Observer", "src/Battlescape", "src/Savegame"):
            for path in (ROOT / folder).glob("*.[ch]*"):
                self.assertNotIn('AgentInventoryFixture', path.read_text())

    def test_unchanged_rules_and_no_live_outcome_or_ui_mutation(self):
        for forbidden in ('RNG::', 'setSeed', 'setTimeUnits', 'setHealth', 'setFaction',
                          'setAmmoItem', 'setFuseTimer', 'arrangeGround(', 'setSlotX', 'setSlotY',
                          'statePushBack', 'statePushFront', 'agentDispatch', 'AgentController',
                          'YAML::LoadFile', 'ofstream', 'SDL_'):
            self.assertNotIn(forbidden, self.stock)
        for forbidden in ('RNG::', 'setSeed', 'setTimeUnits', 'setHealth', 'setFaction',
                          'setFuseTimer', 'arrangeGround(', 'statePushBack', 'statePushFront'):
            self.assertNotIn(forbidden, self.fixture)
        for check in ('grenade->getBattleType()==BT_GRENADE', 'small->getInventoryWidth()==1',
                      'small->getInventoryHeight()==1', 'gun->getInventoryWidth()==2',
                      'gun->getInventoryHeight()==3', 'ammo->getClipSize()>=10'):
            self.assertIn(check, self.fixture)

    def test_ground_and_linked_ammo_follow_ordinary_serialization(self):
        loader = (ROOT / 'src/Savegame/SavedBattleGame.cpp').read_text()
        item = (ROOT / 'src/Savegame/BattleItem.cpp').read_text()
        generator = (ROOT / 'src/Battlescape/BattlescapeGenerator.cpp').read_text()
        self.assertIn('_fuseTimer(-1)', item)
        self.assertIn('getTile(pos)->addItem(item, mod->getInventory("STR_GROUND", true));', loader)
        self.assertIn('(*weaponi)->setAmmoItem((*ammoi));', loader)
        loading = generator[generator.index('void BattlescapeGenerator::loadWeapons()'):]
        self.assertIn('(*j)->setSlot(_game->getMod()->getInventory("STR_RIGHT_HAND", true));', loading)
        self.assertIn('ammo["inventoryslot"]="STR_RIGHT_HAND";', self.stock)
        self.assertNotIn('ammo["owner"]', self.stock)
        self.assertNotIn('ammo["position"]', self.stock)
        self.assertNotIn('["inventoryX"]', self.stock)
        self.assertNotIn('["inventoryY"]', self.stock)

    def test_distinct_quantities_match_stock_non_stacking_criterion(self):
        inventory = (ROOT / 'src/Battlescape/Inventory.cpp').read_text()
        stacking = inventory[inventory.index('bool Inventory::canBeStacked('):]
        self.assertIn('itemA->getAmmoItem()->getAmmoQuantity() == itemB->getAmmoItem()->getAmmoQuantity()', stacking)
        self.assertIn('for (int quantity=1; quantity<=10; ++quantity)', self.stock)
        self.assertIn('ammo["ammoqty"]=quantity;', self.stock)
        self.assertIn('gun["ammoItem"]=ammoId;', self.stock)


if __name__ == '__main__':
    unittest.main(verbosity=2)
