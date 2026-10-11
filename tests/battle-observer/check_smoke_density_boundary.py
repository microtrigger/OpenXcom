#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md.
"""Asset-free structural tripwires only; real resource-dependent runs are separate."""
from pathlib import Path
import re
import sys

if not __debug__:
    sys.exit('FAIL: structural checks require Python assertions (disable optimization)')
ROOT = Path(__file__).resolve().parents[2]
cmake = (ROOT / 'CMakeLists.txt').read_text()
assert re.search(r'option\s*\(\s*EXPERIMENTAL_SMOKE_DENSITY_GATE\s+"[^"]*"\s+OFF\s*\)', cmake)
assert 'EXPERIMENTAL_SMOKE_DENSITY_GATE AND NOT EXPERIMENTAL_CURSOR_OBSERVER' in cmake
conflicts = re.search(r'if \( EXPERIMENTAL_SMOKE_DENSITY_GATE AND \(([^\n]+)\) \)', cmake).group(1)
for option in ('EXPERIMENTAL_BATTLE_PAIR_GATE', 'EXPERIMENTAL_OBSERVER_EFFECTS_GATE',
               'EXPERIMENTAL_NATURAL_FOV_GATE', 'EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE',
               'EXPERIMENTAL_HUD_OBSERVER', 'EXPERIMENTAL_MIXED_AUDIO_OBSERVER'):
    assert option in conflicts
sources = (ROOT / 'src/CMakeLists.txt').read_text()
assert 'if ( EXPERIMENTAL_BATTLE_PAIR_GATE OR EXPERIMENTAL_SMOKE_DENSITY_GATE )' in sources
assert 'target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_SMOKE_DENSITY_GATE )' in sources
map_source = (ROOT / 'src/Battlescape/Map.cpp').read_text()
assert map_source.count('RenderPairGate::smokeBlitted(') == 1
selection = map_source.index('getSurfaceSet("SMOKE.PCK")->getFrame(frameNumber)')
blit = map_source.index('tmpSurface->blitNShade(surface, screenPosition.x, screenPosition.y, shade)', selection)
witness = map_source.index('RenderPairGate::smokeBlitted(')
assert selection < blit < witness < map_source.index('//draw particle clouds', witness)
assert '#if defined(OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE) && defined(OPENXCOM_EXPERIMENTAL_SMOKE_DENSITY_GATE)' in map_source
fixture = (ROOT / 'tests/battle-observer/RenderPairGate.cpp').read_text()
for forbidden in ('#define private', '#define protected', 'calculateFOV(', '->save(', '->load(',
                  'std::ofstream', 'bind(', 'listen(', 'setAnimationOffset(', 'RNG::generate('):
    assert forbidden not in fixture, forbidden
assert fixture.count('RNG::setSeed(') == 1 and fixture.count('std::srand(') == 1
setup = fixture[fixture.index('void afterCleanup('):fixture.index('void beforeFrame(')]
assert setup.index('RNG::setSeed(') < setup.index('new BattleUnit(') < setup.index('->setSmoke(densities[world])')
assert 'RNG::setSeed(' not in fixture[fixture.index('void beforeFrame('):]
for required in ('const int densities[3] = {7, 8, 6}', 'state->animate()', 'SDL_LockSurface(surface)',
                 'tile->getAnimationOffset() == initialOffset', 'tile->getShade() == initialShade',
                 'RNG::getSeed() == initialSeed', 'tile->getFire() == 0', 'battle->getDepth() == 0',
                 'smokeVisits == 1', 'animationPhase == phase', 'smokeFrames[world][phase] = frameNumber',
                 'a.rgb != positive.rgb && a.crop != positive.crop',
                 'a.eligibility == positive.eligibility', '"density-boundary-pixel-positive-control"',
                 '"smoke-pair-final-rgb"', '"smoke-pair-public-transcript"',
                 'Mix_QuerySpec', 'SDL_WasInit(SDL_INIT_AUDIO)', 'OPENXCOM_SMOKE_DENSITY_GATE'):
    assert required in fixture, required
assert fixture.index('"smoke-pair-final-rgb"') < fixture.index('transcript(histories[0])')
assert fixture.index('"density-boundary-pixel-positive-control"') < fixture.index('transcript(histories[0])')
# Public producer files must remain ignorant of the smoke fixture and raw tile data.
for name in ('src/Observer/PresentationObserver.cpp', 'src/Observer/PresentedCursor.cpp',
             'src/Observer/PresentedHud.cpp'):
    text = (ROOT / name).read_text()
    for forbidden in ('SmokeDensity', 'smokeDensity', 'getSmoke(', 'RenderPairGate'):
        assert forbidden not in text, (name, forbidden)
wrapper = (ROOT / 'tests/battle-observer/run-smoke-density.py').read_text()
for required in ('TemporaryDirectory', 'stdout=subprocess.PIPE', 'stderr=subprocess.PIPE',
                 "env['OPENXCOM_SMOKE_DENSITY_GATE'] = '1'", "env['SDL_AUDIODRIVER'] = 'dummy'",
                 'timeout=45', 'result.returncode != 0'):
    assert required in wrapper
print('PASS: default-off smoke-density seams, setup and oracle order (structural only)')
