#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md.
"""Structural tripwire only; the asset-dependent run supplies dynamic evidence."""
from pathlib import Path
import re
import sys
if not __debug__:
    sys.exit("FAIL: structural checks require Python assertions (disable optimization)")
ROOT = Path(__file__).resolve().parents[2]
cmake = (ROOT / 'CMakeLists.txt').read_text()
assert re.search(r'option\s*\(\s*EXPERIMENTAL_BATTLE_PAIR_GATE\s+"[^"]*"\s+OFF\s*\)', cmake)
assert 'EXPERIMENTAL_BATTLE_PAIR_GATE AND NOT EXPERIMENTAL_CURSOR_OBSERVER' in cmake
for name in ('src/main.cpp', 'src/Engine/Game.cpp', 'src/Engine/Screen.cpp', 'src/Menu/StartState.cpp', 'src/Battlescape/Map.cpp'):
    text = (ROOT / name).read_text()
    # Every gate reference must be in its explicit opt-in preprocessor region.
    stack = []
    for line in text.splitlines():
        if line.startswith(('#if ', '#ifdef ', '#ifndef ')): stack.append(line)
        if 'RenderPairGate' in line: assert any('OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE' in x for x in stack), name
        if line.startswith('#endif'): stack.pop()
game = (ROOT / 'src/Engine/Game.cpp').read_text()
assert game.index('delete _deleted.back()') < game.index('RenderPairGate::afterCleanup(this)') < game.index('_states.back()->init()')
assert game.index('RenderPairGate::beforeFrame(this)') < game.index('_screen->beginObserverFrame()') < game.index('_screen->clear()')
assert game.index('_screen->flip(cursorPresented') < game.index('RenderPairGate::afterFrame(this)')
screen = (ROOT / 'src/Engine/Screen.cpp').read_text()
assert screen.index('SDL_Flip(_screen)') < screen.index('RenderPairGate::capture(') < screen.index('_presentationObserver->presented(crop,')
fixture = (ROOT / 'tests/battle-observer/RenderPairGate.cpp').read_text()
for forbidden in ('#define private', '#define protected', 'calculateFOV(', '->save(', '->load(', 'std::ofstream', 'bind(', 'listen('):
    assert forbidden not in fixture, forbidden
assert fixture.index('"hidden-empty-final-rgb"') < fixture.index('"hidden-empty-public-transcript"')
assert 'Mix_QuerySpec' in fixture and 'SDL_WasInit(SDL_INIT_AUDIO)' in fixture
assert 'state->animate()' in fixture and 'new BattlescapeState' in fixture and 'battle->resetUnitTiles()' in fixture
assert 'i % 2' in fixture and '"visible-crop-positive-control"' in fixture
print('PASS: default-off battle gate seams and oracle order (structural check only)')
