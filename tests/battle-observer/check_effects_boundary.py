#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md.
"""Structural tripwires, not a substitute for asset-dependent dynamic evidence."""
from pathlib import Path
import re
import sys

if not __debug__:
    sys.exit('FAIL: structural checks require Python assertions (disable optimization)')
ROOT = Path(__file__).resolve().parents[2]
cmake = (ROOT / 'CMakeLists.txt').read_text()
assert re.search(r'option\s*\(\s*EXPERIMENTAL_OBSERVER_EFFECTS_GATE\s+"[^"]*"\s+OFF\s*\)', cmake)
assert 'EXPERIMENTAL_OBSERVER_EFFECTS_GATE AND NOT EXPERIMENTAL_CURSOR_OBSERVER' in cmake
assert 'EXPERIMENTAL_OBSERVER_EFFECTS_GATE AND EXPERIMENTAL_BATTLE_PAIR_GATE' in cmake
for name in ('src/main.cpp', 'src/Engine/Game.cpp', 'src/Menu/StartState.cpp',
             'src/Battlescape/BattlescapeState.cpp'):
    text = (ROOT / name).read_text()
    stack = []
    for line in text.splitlines():
        if line.startswith(('#if ', '#ifdef ', '#ifndef ')):
            stack.append(line)
        if line.startswith('#elif '):
            stack[-1] = line
        if 'ObserverEffectsGate' in line:
            assert any('OPENXCOM_EXPERIMENTAL_OBSERVER_EFFECTS_GATE' in x for x in stack), name
        if line.startswith('#endif'):
            stack.pop()
for name in ('src/Engine/Screen.cpp', 'src/Observer/PresentationObserver.cpp'):
    assert 'ObserverEffectsGate' not in (ROOT / name).read_text()
game = (ROOT / 'src/Engine/Game.cpp').read_text()
assert game.index('delete _deleted.back()') < game.index('ObserverEffectsGate::afterCleanup(this)') < game.index('_states.back()->init()')
assert game.index('ObserverEffectsGate::beforeEvents(this)') < game.index('SDL_PollEvent')
assert game.index('ObserverEffectsGate::afterEvents(this)') < game.index('_states.back()->think()')
assert game.index('_timeUntilNextFrame <= 0') < game.index('ObserverEffectsGate::beforeFrame(this)')
assert game.index('ObserverEffectsGate::beforeFrame(this)') < game.index('_screen->beginObserverFrame()')
assert game.index('_screen->flip(cursorPresented') < game.index('ObserverEffectsGate::afterFrame(this)')
state = (ROOT / 'src/Battlescape/BattlescapeState.cpp').read_text()
assert state.index('_gameTimer->think(this, 0)') < state.index('ObserverEffectsGate::thought(')
assert state.index('_map->animate(!_battleGame->isBusy())') < state.index('ObserverEffectsGate::animated(')
assert state.index('_battleGame->handleState()') < state.index('ObserverEffectsGate::handledState(')
fixture = (ROOT / 'tests/battle-observer/ObserverEffectsGate.cpp').read_text()
for forbidden in ('#define private', '#define protected', '->kneel(', '->animate(', '->think(',
                  'setStateInterval(', '->setInterval(', 'SDL_Delay(', '_timeUntilNextFrame',
                  'std::ofstream', 'YAML::Emitter', 'YAML::Dump', 'memcmp(', 'bind(', 'listen('):
    assert forbidden not in fixture, forbidden
assert fixture.count('RNG::setSeed(') == 1 and fixture.count('std::srand(') == 1
assert 'RNG::generate(' not in fixture and 'RNG::seedless(' not in fixture
for required in ('SDL_PushEvent(', 'SDL_KEYDOWN', 'SDL_KEYUP', 'Options::keyBattleKneel',
                 'std::chrono::milliseconds(250)', 'new Soldier(', 'new Base(',
                 'getScorchedEarthTile()', 'loadData(mod->getMCDPatch(', 'battle->resetUnitTiles()',
                 'battle->save()', 'RNG::getSeed()', 'getVisibleUnits()', 'getVisibleTiles()',
                 'getUnitsSpottedThisTurn()', 'getShade()', 'getCurrentAction()', 'a["diff"] = action.diff',
                 'compare(setup, now)', 'compare(checkpoints.at(phase), now)', 'SDL_INIT_AUDIO'):
    assert required in fixture, required
assert 'const int cost[] = {0, 4, 12};' in fixture
assert 'w <= Observer::CursorCropSize && h <= Observer::CursorCropSize' in fixture
assert fixture.index('const YAML::Node packet = YAML::Load(receive())') < fixture.index('compare(checkpoints.at(phase), now)')
assert 'if (world == 1)' in fixture and 'responses == 3' in fixture
print('PASS: default-off observer-effects seams and comparison boundaries (structural only)')
