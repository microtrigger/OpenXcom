#!/usr/bin/env python3
"""Bounded source contracts for the isolated /17 strategic fixture; no Game."""
from pathlib import Path
import re
import unittest
import yaml

SOURCE = Path(__file__).resolve().parents[2]
BASELINE = SOURCE.parent.parent / 'agent-control-geoscape-nav-v16-20261019/source'
FIXTURE = 'tests/agent-report-fixture/AgentReportFixture.cpp'
ALIASES = ('alien-control-v1', 'gameplay-v1', 'gameplay-armored-v1',
           'gameplay-vehicle-v1', 'gameplay-mission-v1', 'inventory-body-v1',
           'inventory-paging-v1', 'manufacture-medikit-v1')


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def block(text, signature):
    """Return the literal C++ block, skipping braces inside strings/comments."""
    start = text.index(signature)
    index = text.index('{', start) + 1
    depth = 1
    while depth:
        require(index < len(text), 'Unclosed source block: ' + signature)
        if text.startswith('//', index):
            index = text.index('\n', index) + 1
            continue
        if text.startswith('/*', index):
            index = text.index('*/', index + 2) + 2
            continue
        if text[index] in ('"', "'"):
            quote = text[index]
            index += 1
            while text[index] != quote:
                index += 2 if text[index] == '\\' else 1
            index += 1
            continue
        if text[index] == '{':
            depth += 1
        elif text[index] == '}':
            depth -= 1
        index += 1
    return text[start:index]


def read(path):
    return (SOURCE / path).read_text()


class ProjectsFixtureArchitecture(unittest.TestCase):
    def setUp(self):
        self.fixture = read(FIXTURE)
        self.branch = block(self.fixture, 'if (manufactureCampaign)')

    def test_only_four_narrow_additions_preserve_entire_old_fixture(self):
        restored = self.fixture.replace(', manufactureCampaign = false;', ';', 1)
        restored = restored.replace(' || std::string(argv[i+1])=="manufacture-medikit-v1"', '', 1)
        restored = restored.replace('        manufactureCampaign=std::string(argv[i+1])=="manufacture-medikit-v1";\n', '', 1)
        restored = restored.replace('    ' + self.branch + '\n', '', 1)
        require(restored == (BASELINE / FIXTURE).read_text(),
                'Removing only new flag, alias, flag assignment and early branch must recover exact /16 fixture')

    def test_default_off_runtime_guard_and_existing_call_sites(self):
        cm = read('CMakeLists.txt')
        require('EXPERIMENTAL_AGENT_REPORT_FIXTURE "Build isolated ordinary-AI report acceptance setup (explicit runtime alias required)" OFF' in cm,
                'Fixture must remain default-off')
        require('EXPERIMENTAL_AGENT_REPORT_FIXTURE AND NOT EXPERIMENTAL_AGENT_CONTROL' in cm,
                'Fixture requires separate agent ABI')
        for path in ('CMakeLists.txt', 'src/CMakeLists.txt', 'src/main.cpp',
                     'src/Menu/MainMenuState.cpp', 'tests/agent-report-fixture/AgentReportFixture.h'):
            require((SOURCE / path).read_bytes() == (BASELINE / path).read_bytes(),
                    'Existing fixture integration changed: ' + path)
        prepare = block(self.fixture, 'void prepare(')
        require('!requested && i+1<argc' in prepare, 'Alias requires value and single admission')
        require('require(mode && std::string(mode)=="1");' in prepare, 'Explicit runtime opt-in missing')
        require('bool waiting() { return requested && !finished; }' in self.fixture, 'Waiting contract changed')
        poll = block(read('src/Observer/AgentController.cpp'), 'void AgentController::poll(')
        require(poll.index('AgentReportFixture::waiting()') < poll.index('recvmsg('),
                'Transport must wait until fixture setup is finished')

    def test_isolated_strategic_branch_before_any_tactical_setup(self):
        fixture = self.fixture
        require(fixture.count('if (manufactureCampaign)') == 1, 'Strategic branch must be unique')
        require(fixture.index(self.branch) < fixture.index('const std::string dataset='),
                'Branch must precede terrain and tactical setup')
        require('return true;' in self.branch, 'Strategic branch must not fall into tactical setup')
        for text in ('!game->getSavedGame()', '!Options::autosave', '!Options::debug', '!Options::traceAI',
                     'Options::getActiveMods().size()==1', 'master=="xcom1" || master=="xcom2"'):
            require(fixture.index(text) < fixture.index(self.branch), 'Startup guard missing: ' + text)
        require(self.branch.index('mod->newSave()') < self.branch.index('addFinishedResearchSimple('),
                'Ordinary newSave must own initial state')
        require('campaign->getBases()->size()==1 && campaign->getDiscoveredResearch().empty()' in self.branch,
                'Require stock one-base undiscovered campaign')
        require('campaign->getDiscoveredResearch().size()==1 && !campaign->getSavedBattle()' in self.branch,
                'Require exactly one discovery and no battle')
        require('for (Craft *craft:*base->getCrafts()) { craft->setLongitude(longitude); craft->setLatitude(latitude); }' in self.branch,
                'All home crafts must follow ordinary first-base placement')
        require(self.branch.index('game->setSavedGame(campaign.release())') < self.branch.index('new GeoscapeState')
                < self.branch.index('finished=true') < self.branch.index('return true;'),
                'Ownership/Geoscape/completion order changed')

    def test_only_explicit_research_precondition_no_seed_save_or_resource_override(self):
        require(self.branch.count('addFinishedResearchSimple(') == 1, 'Exactly one isolated research precondition')
        require('campaign->addFinishedResearchSimple(mod->getResearch("STR_MEDI_KIT",true));' in self.branch,
                'Only Medi-Kit precondition is allowed')
        require('never a completed MCP' in self.branch and 'research claim' in self.branch,
                'Artificial setup provenance must remain explicit')
        for forbidden in ('RNG::', 'setSeed', 'srand(', 'YAML::LoadFile', 'ifstream', 'ofstream',
                          'campaign->load(', 'campaign->save(', 'setFunds', 'setScientists', 'setEngineers',
                          'addItem(', 'new Production', 'new ResearchProject', 'setTime(', 'setMonthsPassed',
                          'agentDispatch', 'getenv("OPENXCOM_AGENT_CONTROL_FD")'):
            require(forbidden not in self.branch, 'Forbidden strategic fixture operation: ' + forbidden)
        simple = block(read('src/Savegame/SavedGame.cpp'), 'void SavedGame::addFinishedResearchSimple(')
        require(simple[simple.index('{') + 1:simple.rfind('}')].strip() == '_discovered.push_back(research);',
                'Simple precondition must not perform research scoring/unlocks/simulation')

    def test_unchanged_stock_research_and_manufacture_in_both_games(self):
        for game in ('xcom1', 'xcom2'):
            for filename in ('research.rul', 'manufacture.rul', 'startingBase.rul', 'facilities.rul', 'items.rul', 'vars.rul', 'globe.rul'):
                path = 'bin/standard/' + game + '/' + filename
                require((SOURCE / path).read_bytes() == (BASELINE / path).read_bytes(), 'Stock rules changed: ' + path)
            prefix = SOURCE / 'bin/standard' / game
            research = [r for r in yaml.safe_load((prefix / 'research.rul').read_text())['research'] if r['name'] == 'STR_MEDI_KIT']
            manufacture = [r for r in yaml.safe_load((prefix / 'manufacture.rul').read_text())['manufacture'] if r['name'] == 'STR_MEDI_KIT']
            require(research == [{'name': 'STR_MEDI_KIT', 'cost': 210, 'points': 20}], 'Unexpected research definition: ' + game)
            require(manufacture == [{'name': 'STR_MEDI_KIT', 'category': 'STR_EQUIPMENT', 'requires': ['STR_MEDI_KIT'],
                                      'space': 4, 'time': 420, 'cost': 28000}], 'Unexpected manufacture definition/required items: ' + game)
            base = yaml.safe_load((prefix / 'startingBase.rul').read_text())['startingBase']
            require(base['scientists'] == 10 and base['engineers'] == 10 and base['randomSoldiers'] == 8,
                    'Stock staff/soldiers changed: ' + game)
            require('STR_MEDI_KIT' not in base['items'] and all('STR_MEDI_KIT' not in c.get('items', {}) for c in base['crafts']),
                    'Stock Medi-Kit inventory must start absent')
            facilities = yaml.safe_load((prefix / 'facilities.rul').read_text())['facilities']
            workshop = next(f for f in facilities if f['type'] == 'STR_WORKSHOP')
            require(workshop['workshops'] == 50 and sum(f['type'] == 'STR_WORKSHOP' for f in base['facilities']) == 1,
                    'Stock workshop precondition changed')

    def test_ordinary_new_save_and_first_geoscape_init_are_unchanged(self):
        for path in ('src/Mod/Mod.cpp', 'src/Savegame/SavedGame.cpp', 'src/Savegame/Base.cpp',
                     'src/Mod/RuleManufacture.cpp', 'src/Geoscape/Globe.cpp', 'src/Mod/RuleGlobe.cpp',
                     'src/Geoscape/GeoscapeState.cpp', 'src/Geoscape/BuildNewBaseState.cpp'):
            require((SOURCE / path).read_bytes() == (BASELINE / path).read_bytes(), 'Ordinary logic changed: ' + path)
        new_save = block(read('src/Mod/Mod.cpp'), 'SavedGame *Mod::newSave() const')
        for text in ('new SavedGame()', 'new Country(country)', 'save->setFunds(save->getCountryFunding())',
                     'new Region(region)', 'base->load(_startingBase, save, true)', 'genSoldier(save, randomTypes[i])',
                     'save->getAlienStrategy().init(this)', 'save->setTime(_startingTime)'):
            require(text in new_save, 'Ordinary newSave behavior missing: ' + text)
        require('setSeed' not in new_save and 'RNG::generate' in new_save, 'No injected seed; ordinary generation retained')
        init = block(read('src/Geoscape/GeoscapeState.cpp'), 'void GeoscapeState::init()')
        for text in ('getMonthsPassed() == -1', '!_game->getSavedGame()->getBases()->front()->getName().empty()',
                     '_game->getSavedGame()->addMonth()', 'determineAlienMissions()',
                     'getBaseMaintenance() - _game->getSavedGame()->getBases()->front()->getPersonnelMaintenance()'):
            require(text in init, 'Normal named-first-base initialization changed: ' + text)
        require('campaign->setIronman(false)' in self.branch, 'Fixture must avoid new Ironman save popup')
        availability = block(read('src/Savegame/SavedGame.cpp'), 'void SavedGame::getAvailableProductions (')
        require('!isResearched(m->getRequirements())' in availability, 'Normal manufacture eligibility must remain research-gated')


if __name__ == '__main__':
    unittest.main(verbosity=2)
