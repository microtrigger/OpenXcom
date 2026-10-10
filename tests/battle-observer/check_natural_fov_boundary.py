#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md.
"""Structural tripwires only; real input, resources, and presentation still need runtime evidence."""
from pathlib import Path
import re
import sys

if not __debug__:
    sys.exit('FAIL: structural checks require Python assertions (disable optimization)')
ROOT = Path(__file__).resolve().parents[2]
cmake = (ROOT / 'CMakeLists.txt').read_text()
assert re.search(r'option\s*\(\s*EXPERIMENTAL_NATURAL_FOV_GATE\s+"[^"]*"\s+OFF\s*\)', cmake)
assert 'EXPERIMENTAL_NATURAL_FOV_GATE AND NOT EXPERIMENTAL_CURSOR_OBSERVER' in cmake
assert 'EXPERIMENTAL_BATTLE_PAIR_GATE OR EXPERIMENTAL_OBSERVER_EFFECTS_GATE OR EXPERIMENTAL_HUD_OBSERVER' in cmake
for name in ('src/main.cpp', 'src/Engine/Game.cpp', 'src/Menu/StartState.cpp', 'src/Battlescape/BattlescapeState.cpp'):
    stack = []
    for line in (ROOT / name).read_text().splitlines():
        if line.startswith(('#if ', '#ifdef ', '#ifndef ')):
            stack.append(line)
        if line.startswith('#elif '):
            stack[-1] = line
        if 'NaturalFovGate' in line:
            assert any('OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE' in level for level in stack), name
        if line.startswith('#endif'):
            stack.pop()
for name in ('src/Engine/Screen.cpp', 'src/Observer/PresentationObserver.cpp',
             'src/Observer/PresentedCursor.cpp', 'src/Observer/PresentedHud.cpp',
             'src/Interface/NumberText.cpp', 'src/Interface/Window.cpp'):
    assert 'NaturalFovGate' not in (ROOT / name).read_text()
game = (ROOT / 'src/Engine/Game.cpp').read_text()
assert game.index('delete _deleted.back()') < game.index('NaturalFovGate::afterCleanup(this)') < game.index('_states.back()->init()')
assert game.index('NaturalFovGate::beforeEvents(this)') < game.index('SDL_PollEvent')
assert game.index('NaturalFovGate::afterEvents(this)') < game.index('_states.back()->think()')
assert game.index('_timeUntilNextFrame <= 0') < game.index('NaturalFovGate::beforeFrame(this, _states.back())') < game.index('_screen->beginObserverFrame()')
assert game.index('_screen->flip(cursorPresented') < game.index('NaturalFovGate::afterFrame(this)')
state = (ROOT / 'src/Battlescape/BattlescapeState.cpp').read_text()
assert state.index('_gameTimer->start()') < state.index('NaturalFovGate::initialized(this)')
assert state.index('_gameTimer->think(this, 0)') < state.index('NaturalFovGate::thought(')
assert state.index('_map->animate(!_battleGame->isBusy())') < state.index('NaturalFovGate::animated(')
assert state.index('_battleGame->handleState()') < state.index('NaturalFovGate::handledState(')
fixture = (ROOT / 'tests/battle-observer/NaturalFovGate.cpp').read_text()
for forbidden in ('#define private', '#define protected', 'setVisible(true);\n    hostile',
                  '->animate(', '->think(', 'setStateInterval(', '->setInterval(', 'SDL_Delay(',
                  '_timeUntilNextFrame', 'std::ofstream', 'YAML::Emitter', 'YAML::Dump',
                  'capturePresentedCursor(', 'serializeCrop(', 'provePresentedHud(', 'decodeHudDigits(',
                  '->getValue(', '->setSelectorPosition(', '->lookAt(', '->turn(', '->spendTimeUnits(',
                  'bind(', 'listen('):
    assert forbidden not in fixture, forbidden
setup = fixture[fixture.index('void afterCleanup('):fixture.index('void initialized(')]
after_setup = fixture[fixture.index('void initialized('):]
for call in ('setPosition(', 'setDirection(', 'recalculateFOV(', 'calculateFOV(', 'setVisible('):
    assert call not in after_setup, call
assert setup.count('setPosition(') == 2 and setup.count('setDirection(') == 2
assert 'Window::soundPopup' not in fixture
assert 'Mix_CloseAudio(' not in fixture and 'SDL_QuitSubSystem(' not in fixture
assert 'setenv("SDL_AUDIODRIVER", "dummy", 1)' in fixture
assert 'SDL_AudioDriverName(' in fixture and 'Mix_QuerySpec(&frequency, &format, &channels) > 0' in fixture
assert fixture.count('RNG::setSeed(') == 1 and fixture.count('std::srand(') == 1
for required in ('SDL_PushEvent(', 'SDL_MOUSEBUTTONDOWN', 'SDL_MOUSEBUTTONUP', 'SDL_BUTTON_RIGHT',
                 'Options::keyBattleOptions', 'Options::keyCancel', 'std::chrono::milliseconds(250)',
                 'new Soldier(', 'new Base(', 'getScorchedEarthTile()', 'SDL_GetVideoSurface()',
                 'const int direction[] = {0, 1, 0, 1, 1, 1}', 'const int cost[] = {0, 1, 2, 3, 3, 3}',
                 'const bool inFov[] = {false, true, false, true, true, true}',
                 'std::max(0, cx - 64)', 'std::min(640, cx + 64)', 'actualByte == expectedByte',
                 'packet["hud"].IsNull()', '== visibleValue', 'candidate <= 999',
                 'dynamic_cast<PauseState*>', 'responses == 6', 'recv(peer, &byte, 1, MSG_DONTWAIT) == 0'):
    assert required in fixture, required
# Independently reconstruct every upstream 3x5 glyph from its setPixel calls.
number = (ROOT / 'src/Interface/NumberText.cpp').read_text()
actual = [[0] * 5 for _ in range(10)]
for digit, x, y in re.findall(r'_chars\[(\d)\]->setPixel\((\d), (\d), 1\)', number):
    actual[int(digit)][int(y)] |= 1 << (2 - int(x))
table = re.search(r'rows\[10\]\[5\] = \{(.*?)\n    \};', fixture, re.S).group(1)
expected = [list(map(int, re.findall(r'\d+', row))) for row in re.findall(r'\{([^{}]+)\}', table)]
assert actual == expected, 'independent glyph masks drifted from NumberText'
print('PASS: default-off natural-FOV seams and independent oracle boundaries (structural only)')
