#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md.
"""Asset-free structural tripwires; neither compilation nor real-render evidence."""
from pathlib import Path
import re
import sys

if not __debug__:
    sys.exit('FAIL: structural checks require Python assertions (disable optimization)')
ROOT = Path(__file__).resolve().parents[2]
cmake = (ROOT / 'CMakeLists.txt').read_text()
assert re.search(r'option\s*\(\s*EXPERIMENTAL_SMOKE_CLIPPING_GATE\s+"[^"]*"\s+OFF\s*\)', cmake)
assert 'EXPERIMENTAL_SMOKE_CLIPPING_GATE AND NOT EXPERIMENTAL_CURSOR_OBSERVER' in cmake
conflicts = re.search(r'if \( EXPERIMENTAL_SMOKE_CLIPPING_GATE AND \(([^\n]+)\) \)', cmake).group(1)
for option in ('EXPERIMENTAL_SMOKE_DENSITY_GATE', 'EXPERIMENTAL_BATTLE_PAIR_GATE',
               'EXPERIMENTAL_OBSERVER_EFFECTS_GATE', 'EXPERIMENTAL_NATURAL_FOV_GATE',
               'EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE', 'EXPERIMENTAL_HUD_OBSERVER',
               'EXPERIMENTAL_MIXED_AUDIO_OBSERVER'):
    assert option in conflicts, option
sources = (ROOT / 'src/CMakeLists.txt').read_text()
assert 'target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_SMOKE_CLIPPING_GATE )' in sources
assert 'if ( EXPERIMENTAL_BATTLE_PAIR_GATE OR EXPERIMENTAL_SMOKE_DENSITY_GATE OR EXPERIMENTAL_SMOKE_CLIPPING_GATE )' in sources
map_source = (ROOT / 'src/Battlescape/Map.cpp').read_text()
assert map_source.count('RenderPairGate::smokeClipped(') == 1
selection = map_source.index('getSurfaceSet("SMOKE.PCK")->getFrame(frameNumber)')
blit = map_source.index('tmpSurface->blitNShade(surface, screenPosition.x, screenPosition.y, shade)', selection)
witness = map_source.index('RenderPairGate::smokeClipped(')
assert selection < blit < witness < map_source.index('//draw particle clouds', witness)
assert '#elif defined(OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE) && defined(OPENXCOM_EXPERIMENTAL_SMOKE_CLIPPING_GATE)' in map_source
assert 'tmpSurface, surface, screenPosition.x, screenPosition.y, _spriteWidth, _spriteHeight' in map_source
# The witness reconstructs exactly this unmasked, full-domain production shader.
surface = (ROOT / 'src/Engine/Surface.cpp').read_text()
shade = surface[surface.index('void Surface::blitNShade(Surface *surface, int x, int y, int off, bool half'):]
assert 'ShaderMove<Uint8> src(this, x, y)' in shade
assert 'ShaderDraw<StandardShade>(ShaderSurface(surface), src, ShaderScalar(off))' in shade
fixture = (ROOT / 'tests/battle-observer/RenderPairGate.cpp').read_text()
for forbidden in ('#define private', '#define protected', 'calculateFOV(', '->save(', '->load(',
                  'std::ofstream', 'bind(', 'listen(', 'setAnimationOffset(', 'RNG::generate('):
    assert forbidden not in fixture, forbidden
assert fixture.count('RNG::setSeed(') == 1 and fixture.count('std::srand(') == 1
setup = fixture[fixture.index('void afterCleanup('):fixture.index('void beforeFrame(')]
assert setup.index('RNG::setSeed(') < setup.index('new BattleUnit(') < setup.index('->setSmoke(densities[world])')
assert 'RNG::setSeed(' not in fixture[fixture.index('void beforeFrame('):]
for required in ('const int scenarioCount = 4, equalFirst = 2, equalSecond = 3',
                 'const int densities[4] = {6, 7, 6, 7}',
                 'clippedCenter(14, 27, 0), clippedHover(14, 27, 0)',
                 'hover(20, 22, 0)', 'return world >= 2 ? clippedHover : hover',
                 'centerOnPosition(clippedCenter, false)', 'convertMapToScreen(cursorHover(), &pixel)',
                 'selected == cursorHover()', 'state->animate()', 'SDL_LockSurface(surface)',
                 'tile->getAnimationOffset() == initialOffset', 'tile->getShade() == initialShade',
                 'RNG::getSeed() == initialSeed', 'tile->getFire() == 0', 'battle->getDepth() == 0',
                 'smokeVisits == 1', 'animationPhase == phase', 'smokeFrames[world][phase] = frameNumber',
                 'sprite->getWidth() == 32', 'destination->getWidth() == 320',
                 'const ShaderMove<Uint8> source(sprite, x, y), target(destination)',
                 'domain.beg_x == 0 && domain.beg_y == 0',
                 'domain.end_x == sprite->getWidth()', 'domain.end_y == sprite->getHeight()',
                 'x < destination->getWidth() + tileWidth', 'y < destination->getHeight() + tileHeight',
                 'x == 336 && image.beg_x >= destinationDomain.end_x && intersection.size_x() == 0',
                 'intersection.size_x() == sprite->getWidth() && intersection.size_y() == sprite->getHeight()',
                 'a.rgb != b.rgb && a.crop != b.crop', 'smokeGeometry[2][i] == smokeGeometry[3][i]',
                 '"clipped-pair-final-rgb"', '"centered-density-pixel-positive-control"',
                 '"clipped-pair-public-transcript"', 'Mix_QuerySpec', 'SDL_WasInit(SDL_INIT_AUDIO)',
                 'OPENXCOM_SMOKE_CLIPPING_GATE', 'if (++world == scenarioCount)'):
    assert required in fixture, required
verify = fixture[fixture.index('void verify()'):fixture.index('void prepare()')]
assert verify.index('"clipped-pair-final-rgb"') < verify.index('"smoke-clipping-pair-eligibility"')
assert verify.index('"centered-density-pixel-positive-control"') < verify.index('transcript(histories[equalFirst])')
assert verify.index('"smoke-clipping-pair-eligibility"') < verify.index('transcript(histories[equalFirst])')
for name in ('src/Observer/PresentationObserver.cpp', 'src/Observer/PresentedCursor.cpp', 'src/Observer/PresentedHud.cpp'):
    text = (ROOT / name).read_text()
    for forbidden in ('smokeDensity', 'getSmoke(', 'RenderPairGate', 'SmokeClipping'):
        assert forbidden not in text, (name, forbidden)
wrapper = (ROOT / 'tests/battle-observer/run-smoke-clipping.py').read_text()
for required in ('TemporaryDirectory', 'stdout=subprocess.PIPE', 'stderr=subprocess.PIPE',
                 "env['OPENXCOM_SMOKE_CLIPPING_GATE'] = '1'", "env['SDL_AUDIODRIVER'] = 'dummy'",
                 'timeout=45', 'result.returncode != 0'):
    assert required in wrapper, required
print('PASS: default-off smoke-clipping seams, domains and oracle order (structural only)')
