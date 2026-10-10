#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Structural tripwires, not a proof of runtime accuracy or noninterference."""
from pathlib import Path
import re
import sys

if not __debug__:
    sys.exit('FAIL: battlefield-audio checks require Python assertions (disable optimization)')

ROOT = Path(__file__).resolve().parents[2]
FLAG = 'OPENXCOM_EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE'


def source(name):
    return (ROOT / name).read_text(encoding='utf-8')


def guarded_seams(name, text):
    stack = []
    for line in text.splitlines():
        line = line.strip()
        if re.match(r'#\s*(if|ifdef|ifndef)\b', line):
            stack.append(line)
        elif re.match(r'#\s*(elif|else)\b', line):
            assert stack, 'unbalanced-preprocessor'
            stack[-1] = line
        if 'BattlefieldAudioGate' in line:
            assert any(re.fullmatch(r'#\s*ifdef\s+' + FLAG, level)
                       or re.fullmatch(r'#\s*(?:if|elif)\s+defined\s*\(\s*' + FLAG + r'\s*\)', level)
                       for level in stack), 'unguarded-fixture-seam'
        if re.match(r'#\s*endif\b', line):
            assert stack, 'unbalanced-preprocessor'
            stack.pop()
    assert not stack, 'unbalanced-preprocessor'


def main():
    cmake = source('CMakeLists.txt')
    assert re.search(r'option\s*\(\s*EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE\s+"[^"]*"\s+OFF\s*\)',
                     cmake), 'default-off-option'
    assert 'EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE AND NOT EXPERIMENTAL_MIXED_AUDIO_OBSERVER' in cmake, \
        'native-observer-dependency'
    gate_option = cmake[cmake.index('option ( EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE'):]
    for incompatible in ('EXPERIMENTAL_BATTLE_PAIR_GATE', 'EXPERIMENTAL_OBSERVER_EFFECTS_GATE',
                         'EXPERIMENTAL_NATURAL_FOV_GATE'):
        assert incompatible in gate_option, 'incompatible-fixture-guard'
    build = source('src/CMakeLists.txt')
    match = re.search(r'if\s*\(\s*EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE\s*\)(.*?)endif\s*\(\s*\)',
                      build, re.S)
    assert match, 'isolated-build-block'
    for required in ('BattlefieldAudioGate.cpp', FLAG, '--wrap=Mix_SetPostMix'):
        assert required in match.group(1), 'isolated-build-content'

    permitted = {'src/main.cpp', 'src/Engine/Game.cpp', 'src/Menu/StartState.cpp',
                 'src/Engine/Sound.cpp', 'src/Battlescape/BattlescapeState.cpp'}
    for path in (ROOT / 'src').rglob('*'):
        if path.suffix not in ('.cpp', '.h'):
            continue
        text = path.read_text(encoding='utf-8')
        if 'BattlefieldAudioGate' in text:
            name = path.relative_to(ROOT).as_posix()
            assert name in permitted, 'unexpected-fixture-seam'
            guarded_seams(name, text)
    for path in (ROOT / 'src/Observer').glob('*'):
        if path.suffix in ('.cpp', '.h'):
            assert 'BattlefieldAudioGate' not in path.read_text(encoding='utf-8'), \
                'production-observer-test-hook'

    game = source('src/Engine/Game.cpp')
    assert game.index('delete _deleted.back()') < game.index('BattlefieldAudioGate::afterCleanup(this)') \
        < game.index('_states.back()->init()'), 'cleanup-order'
    assert game.index('BattlefieldAudioGate::beforeEvents(this)') < game.index('SDL_PollEvent'), \
        'normal-event-dispatch'
    assert 'BattlefieldAudioGate::acceptEvent(_event)' in game, 'fixed-input-filter'
    assert game.index('BattlefieldAudioGate::afterEvents(this)') < game.index('_states.back()->think()'), \
        'normal-think-order'
    assert game.index('_timeUntilNextFrame <= 0') \
        < game.index('BattlefieldAudioGate::beforeFrame(this, _states.back())') \
        < game.index('_screen->clear()'), 'ordinary-frame-scheduling'
    assert game.index('BattlefieldAudioGate::beforePoll()') < game.index('_mixedAudioObserver.poll()') \
        < game.index('BattlefieldAudioGate::afterPoll(this)'), 'ordinary-native-poll'
    sound = source('src/Engine/Sound.cpp')
    assert sound.index('Mix_PlayChannel(') < sound.index('Mix_SetPosition(') \
        < sound.index('BattlefieldAudioGate::played(this, chan)'), 'post-playback-witness'
    assert 'BattlefieldAudioGate::loaded(_game)' in source('src/Menu/StartState.cpp'), 'normal-resource-load'
    main_source = source('src/main.cpp')
    assert 'BattlefieldAudioGate::prepare()' in main_source, 'preparation-seam'
    assert 'BattlefieldAudioGate::exitStatus()' in main_source, 'completion-status'

    fixture = source('tests/battle-observer/BattlefieldAudioGate.cpp')
    for forbidden in ('#define private', '#define protected', 'MixedAudioObserver.h', 'MixedAudioSlot.h',
                      'MixedAudioSlot::', 'readyData(', 'std::ofstream', 'YAML::Emitter', 'YAML::Dump',
                      'Mix_OpenAudio(', 'Mix_CloseAudio(', 'Mix_HaltMusic(',
                      'Mix_HookMusic(', 'Mix_PlayChannel(', 'Mix_PlayChannelTimed(',
                      'SDL_QuitSubSystem(', 'SDL_Delay(', '_timeUntilNextFrame', 'RNG::generate(',
                      'new ExplosionBState', 'bind(', 'listen('):
        assert forbidden not in fixture, 'forbidden-fixture-operation'
    assert not re.search(r'->(?:think|animate|init|requestEndTurn|endTurn|btnEndTurnClick|play|loop|'
                         r'setStateInterval|setInterval|lookAt|turn|spendTimeUnits)\s*\(', fixture), \
        'direct-gameplay-or-audio-injection'
    for required in ('SDL_PushEvent(', 'SDL_KEYDOWN', 'SDL_KEYUP', 'Options::keyBattleEndTurn',
                     'Options::skipNextTurnScreen', 'STR_GRENADE', 'STR_MAGNA_BLAST_GRENADE',
                     'getScorchedEarthTile()', 'setFuseTimer(0)', 'getVisible()', 'NextTurnState',
                     'RNG::getSeed()', '->save()', 'SDL_LockAudio()', 'SDL_UnlockAudio()',
                     '__wrap_Mix_SetPostMix', '__real_Mix_SetPostMix', 'std::memcpy(', 'std::memcmp(',
                     '44100', '262144', 'Mix_QuerySpec(', 'SDL_AudioDriverName(',
                     'experimental-mixed-audio/1', 'pcm_hex'):
        assert required in fixture, 'missing-fixture-witness'
    assert fixture.count('RNG::setSeed(') == 1, 'setup-only-engine-seed'
    assert fixture.count('std::srand(') <= 1, 'setup-only-libc-seed'
    # Setup is followed by the ordinary event hooks. Runtime phases must not
    # manufacture direction, position, visibility, or a new FOV result.
    after_setup = fixture[fixture.index('void beforeEvents('):]
    for forbidden in ('setPosition(', 'setDirection(', 'setVisible(', 'setDiscovered(',
                      'calculateFOV(', 'recalculateFOV(', 'RNG::setSeed(', 'std::srand(', 'Mix_HaltChannel('):
        assert forbidden not in after_setup, 'post-setup-state-setter'
    setup = fixture[fixture.index('void afterCleanup('):fixture.index('void beforeEvents(')]
    assert fixture.count('Mix_HaltChannel(') == setup.count('Mix_HaltChannel(') <= 1, \
        'setup-only-old-channel-cleanup'

    runner = source('tests/battle-observer/run-audio.py')
    assert "env['SDL_AUDIODRIVER'] = 'dummy'" in runner, 'dummy-runner-profile'
    assert "'options:\\n'" in runner and "'  soundVolume: 84\\n'" in runner \
        and "'  musicVolume: 84\\n'" in runner and "'  uiVolume: 42\\n'" in runner, \
        'nested-normal-volume-options'
    assert "'  skipNextTurnScreen: false\\n'" in runner, 'natural-input-wait-profile'
    assert 'if not __debug__:' in runner and 'timeout=90' in runner, 'bounded-asserting-runner'
    print('PASS: default-off battlefield-audio seams and independent oracle boundaries (structural only)')


if __name__ == '__main__':
    try:
        main()
    except (AssertionError, OSError, ValueError):
        sys.exit('FAIL: battlefield-audio boundary assertion')
