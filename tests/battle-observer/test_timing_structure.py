#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md.
"""Source-boundary tripwires and their own negative controls; no GUI claim."""
from pathlib import Path
import importlib.util
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
GATE = (ROOT / 'tests/battle-observer/TimingCalibrationGate.cpp').read_text()
SCREEN = (ROOT / 'src/Engine/Screen.cpp').read_text()


def body(source, start, end):
    return source.split(start, 1)[1].split(end, 1)[0]


def check_hooks(gate, screen):
    flip = screen.index('RenderPairGate::timingFlipped();')
    if not screen.index('if (SDL_Flip(_screen) == -1)') < flip < screen.index('const Observer::PixelCrop crop ='):
        raise ValueError('flip-position')
    for start, end in [('void visited(', 'void timingFlipped('),
                       ('void timingFlipped(', 'void capture('),
                       ('void capture(', 'void timingEvent(')]:
        hook = body(gate, start, end)
        if re.search(r'(require\(|\.rgb|push_back\(|reserve\(|resize\(|new |YAML|serialize|std::cout|std::cerr|memcpy|SDL_LockSurface)', hook):
            raise ValueError('hook-cost')
    measured_send = body(gate, 's.t0 = Clock::now();', '\n}\nvoid visited(')
    if not measured_send.lstrip().startswith('const ssize_t sent = send('):
        raise ValueError('send-position')
    if re.search(r'(require\(|YAML|serialize|push_back\(|std::cout|std::cerr)', measured_send):
        raise ValueError('send-hook-cost')
    receive = body(gate, 'void afterFrame(Game *game)', 'lastResult = s.t1;')
    if not re.search(r'const ssize_t size = recvmsg\([^;]+;\s*s.t1 = Clock::now\(\);', receive):
        raise ValueError('receive-position')
    if re.search(r'(require\(|copyAndVerifyFrame\(|YAML|serialize|std::string|std::cout|std::cerr)', receive):
        raise ValueError('receive-hook-cost')
    after = body(gate, 'void afterFrame(Game *game)', 'int exitStatus()')
    if not after.index('s.t1 = Clock::now();') < after.index('copyAndVerifyFrame();') < after.index('YAML::Load('):
        raise ValueError('oracle-position')
    pair = body(gate, 'void verifyPair()', '} // namespace')
    if not pair.index('"pair-final-rgb"') < pair.index('"pair-public-transcript"') < pair.index('writeRecord('):
        raise ValueError('pair-oracle-order')


class BoundaryTests(unittest.TestCase):
    def test_hooks_and_oracle_order(self):
        check_hooks(GATE, SCREEN)

    def test_tripwires_reject_realistic_timing_contamination(self):
        mutants = [
            GATE.replace('Frame &f = frame(); f.surface = surface;', 'frame().rgb.resize(1); Frame &f = frame(); f.surface = surface;'),
            GATE.replace('s.t0 = Clock::now();', 's.t0 = Clock::now(); require(true, "in-timing");'),
            GATE.replace('s.t1 = Clock::now();', 'copyAndVerifyFrame(); s.t1 = Clock::now();'),
            GATE.replace('sample().flip = Clock::now();', 'std::cerr << "timing"; sample().flip = Clock::now();'),
        ]
        for changed in mutants:
            with self.subTest():
                with self.assertRaises(ValueError):
                    check_hooks(changed, SCREEN)
        with self.assertRaises(ValueError):
            check_hooks(GATE, SCREEN.replace('RenderPairGate::timingFlipped();', 'wrongHook();'))

    def test_default_off_exclusive_and_existing_gate_separate(self):
        cmake = (ROOT / 'CMakeLists.txt').read_text()
        self.assertRegex(cmake, r'option \( EXPERIMENTAL_TIMING_CALIBRATION "[^"]+" OFF \)')
        self.assertIn('EXPERIMENTAL_TIMING_CALIBRATION AND NOT EXPERIMENTAL_CURSOR_OBSERVER', cmake)
        conflicts = re.search(r'if \( EXPERIMENTAL_TIMING_CALIBRATION AND \(([^\n]+)\) \)', cmake).group(1)
        for flag in ('SMOKE_CLIPPING_GATE', 'SMOKE_DENSITY_GATE', 'BATTLE_PAIR_GATE', 'OBSERVER_EFFECTS_GATE',
                     'NATURAL_FOV_GATE', 'BATTLEFIELD_AUDIO_GATE', 'HUD_OBSERVER', 'MIXED_AUDIO_OBSERVER'):
            self.assertIn('EXPERIMENTAL_' + flag, conflicts)
        sources = (ROOT / 'src/CMakeLists.txt').read_text()
        timing = body(sources, 'if ( EXPERIMENTAL_TIMING_CALIBRATION )', 'endif()')
        self.assertIn('TimingCalibrationGate.cpp', timing)
        self.assertIn('OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE OPENXCOM_EXPERIMENTAL_TIMING_CALIBRATION', timing)
        self.assertNotIn('RenderPairGate.cpp', timing)
        for source in ('src/Engine/Game.cpp', 'src/Engine/Screen.cpp'):
            stack = []
            for line in (ROOT / source).read_text().splitlines():
                if line.startswith(('#if ', '#ifdef ', '#ifndef ')):
                    stack.append(line)
                if 'RenderPairGate::timing' in line:
                    self.assertIn('#ifdef OPENXCOM_EXPERIMENTAL_TIMING_CALIBRATION', stack)
                if line.startswith('#endif'):
                    stack.pop()

    def test_genuine_dependency_and_independent_resets(self):
        for required in ('battle->resetUnitTiles()', 'battle->getUnits()->size() == 2',
                         'occupant->getTile()->getUnit() == occupant',
                         'battle->getTile(hover)->getUnit() == (occupied ? occupant : 0)',
                         'schedule[pairIndex].kind == AA || currentLabel() == \'A\'',
                         'RNG::setSeed(123456789); std::srand(12345);',
                         'std::array<Sample, 112> samples;', 'state->animate();',
                         'f.phase == 1', 'std::chrono::milliseconds(300)',
                         'totalPairs = mutant ? 20 : 56', 'mutant ? 90 : 120'):
            self.assertIn(required, GATE)
        setup = body(GATE, 'void afterCleanup(Game *game)', 'void beforeFrame(')
        self.assertIn('new SavedGame', setup)
        self.assertIn('new SavedBattleGame', setup)
        advance = body(GATE, 'void afterFrame(Game *game)', 'int exitStatus()')
        self.assertIn('pendingSetup = true; game->setState(new State);', advance)
        for forbidden in ('calculateFOV(', '#define private', '#define protected', 'bind(', 'listen(', '->save(', '->load('):
            self.assertNotIn(forbidden, GATE)

    def test_focus_history_is_checked_outside_measurement(self):
        before = body(GATE, 'void beforeFrame(Game *game)', 'void visited(')
        self.assertLess(before.index('sleep_until('), before.index('drainFocusEvents();'))
        self.assertLess(before.index('drainFocusEvents();'), before.index('s.t0 = Clock::now();'))
        after = body(GATE, 'void afterFrame(Game *game)', 'int exitStatus()')
        self.assertLess(after.index('s.t1 = Clock::now();'), after.index('copyAndVerifyFrame();'))
        self.assertLess(after.index('copyAndVerifyFrame();'), after.index('drainFocusEvents();'))
        self.assertLess(after.index('drainFocusEvents();'), after.index('verifyPair();'))
        self.assertIn('while (SDL_PollEvent(&event)) timingEvent(event);', GATE)

    def test_actual_source_mutant_exact_anchor(self):
        spec = importlib.util.spec_from_file_location('timing_mutant', Path(__file__).with_name('make-timing-mutant.py'))
        recipe = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(recipe)
        production = (ROOT / 'src/Observer/PresentationObserver.cpp').read_text()
        mutant = recipe.mutate(production)
        self.assertNotIn('timingInvisibleHoverOccupant', production)
        self.assertIn('if (RenderPairGate::timingInvisibleHoverOccupant()) SDL_Delay(5);\n#endif\n' + recipe.SEND, mutant)
        self.assertEqual(mutant.count('SDL_Delay(5)'), 1)
        accessor = body(GATE, 'bool timingInvisibleHoverOccupant()', 'void afterFrame(')
        for required in ('getSelectorPosition', 'currentBattle->getTile(selected)', 'tile->getUnit()', '!unit->getVisible()'):
            self.assertIn(required, accessor)
        for forbidden in ('currentLabel', 'schedule', 'pairIndex', 'mutant'):
            self.assertNotIn(forbidden, accessor)
        for changed in (production + '\n', mutant, production.replace('send(_fd', 'other(_fd')):
            with self.assertRaises(ValueError):
                recipe.mutate(changed)


if __name__ == '__main__':
    unittest.main()
