#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Text-only tripwires for the newly reconstructed unactivated audio slice.

This is new source, not recovered historical bytes or an execution release.
Historical audits are provenance, never evidence this source compiled or passed.
No target imports, compiler, subprocess, SDL initialization or private payloads.
"""
import hashlib
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
STAGING = ROOT.parent
FREEZE = STAGING / 'audio-reconstruction-r3'
BASELINE_SHA = '0e2d0decc0d0f720513a1e2581cf02d1ef6f4a0cd7161dd83a6a4af7963f2cac'
PLAN_SHA = '9ff9100b02a16efe5d519c0b85ae5aca2a0bbbf3750ffbf901906dc54a55b162'
EXISTING = {'CMakeLists.txt', 'src/CMakeLists.txt',
            'src/Observer/MixedAudioObserver.h', 'src/Observer/MixedAudioObserver.cpp'}
NEW = {'src/Engine/LoadingAudioHandoff.h', 'src/Engine/LoadingAudioHandoff.cpp',
       'tests/snapshot-control/loading_audio_handoff_unit.cpp',
       'tests/snapshot-control/loading_audio_observer_unit.cpp',
       'tests/snapshot-control/loading_audio_handoff_source_contract.py'}
CPP = 'src/Engine/LoadingAudioHandoff.cpp'
HEADER = 'src/Engine/LoadingAudioHandoff.h'
OBSERVER = 'src/Observer/MixedAudioObserver.cpp'


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def preimage(path):
    return (FREEZE / 'preimages/repo' / path).read_text(encoding='utf-8')


def clean(text):
    # Lexical removal only; preserve quoted strings and character literals.
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*'
    return re.sub(pattern, lambda m: '' if m.group().startswith('/') else m.group(),
                  text, flags=re.S)


def tokens(text):
    return re.findall(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\w+|[^\s]', clean(text))


def body(text, signature):
    text = clean(text)
    start = text.index('{', text.index(signature))
    depth, pos = 1, start + 1
    while depth:
        if text[pos] == '{':
            depth += 1
        elif text[pos] == '}':
            depth -= 1
        pos += 1
    return text[start + 1:pos - 1]


def ordered(test, text, fragments):
    pos = -1
    for fragment in fragments:
        pos = text.index(fragment, pos + 1)
        test.assertGreaterEqual(pos, 0, fragment)


class LoadingAudioHandoffContract(unittest.TestCase):
    def test_00_exact_nine_path_delta_and_current_pins(self):
        initial = json.loads((FREEZE / 'PREIMPLEMENTATION.json').read_text())
        self.assertEqual(initial['restored_baseline_files_verified'], 1498)
        self.assertEqual(set(initial['new_targets_initially_absent']), {'repo/' + p for p in NEW})
        baseline_raw = (FREEZE / 'BASELINE.json').read_bytes()
        self.assertEqual(digest(baseline_raw), BASELINE_SHA)
        baseline = json.loads(baseline_raw)
        self.assertEqual(baseline['repo_files'], 1498)
        self.assertEqual(len(baseline['source_members']), 1498)
        for relative, row in baseline['source_members'].items():
            path = FREEZE / 'preimages/repo' / relative if relative in EXISTING else ROOT / relative
            raw = path.read_bytes()
            self.assertEqual((len(raw), digest(raw)), (row['bytes'], row['sha256']), relative)
        paths = set()
        for path in ROOT.rglob('*'):
            relative = path.relative_to(ROOT)
            if '.git' in relative.parts:
                continue
            self.assertFalse(path.is_symlink(), str(relative))
            if path.is_file():
                paths.add(str(relative))
        self.assertEqual(paths, set(baseline['source_members']) | NEW)
        pins = json.loads((FREEZE / 'SOURCE-PINS.json').read_text())
        self.assertEqual({r['path'] for r in pins}, {'repo/' + p for p in EXISTING | NEW})
        for row in pins:
            raw = (STAGING / row['path']).read_bytes()
            self.assertEqual((len(raw), digest(raw)), (row['bytes'], row['sha256']))
            self.assertEqual(raw, (FREEZE / 'sources' / row['path']).read_bytes())

    def test_01_no_activation_or_caller_change(self):
        for path in ('src/Engine/Game.cpp', 'src/Engine/Game.h', 'src/Menu/StartState.cpp',
                     'src/Engine/LoaderSession.cpp', 'src/Engine/LoadingUiPolicy.cpp',
                     'src/Engine/LoaderThreadLinux.cpp', 'src/Engine/LoaderSdlTls.cpp'):
            self.assertNotIn('LoadingAudioHandoff', read(path))
            self.assertNotIn('OPENXCOM_LOADING_AUDIO_HANDOFF', read(path))
        callers = []
        for path in (ROOT / 'src').rglob('*.cpp'):
            if 'LoadingAudioHandoff' in clean(path.read_text()):
                callers.append(str(path.relative_to(ROOT)))
        self.assertEqual(callers, [CPP])

    def test_02_default_off_and_selected_platform(self):
        cmake = read('CMakeLists.txt')
        self.assertRegex(cmake, r'option\s*\(\s*OPENXCOM_BUILD_LOADING_AUDIO_HANDOFF\s+"[^"]*"\s+OFF\s*\)')
        self.assertIn('OPENXCOM_BUILD_LOADING_AUDIO_HANDOFF AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux"', cmake)
        block = read('src/CMakeLists.txt').split('if ( OPENXCOM_BUILD_LOADING_AUDIO_HANDOFF )', 1)[1].split('endif()', 1)[0]
        self.assertIn('target_sources ( openxcom PRIVATE Engine/LoadingAudioHandoff.cpp )', block)
        self.assertIn('target_compile_definitions ( openxcom PRIVATE OPENXCOM_LOADING_AUDIO_HANDOFF )', block)
        self.assertNotIn('LOADER_BACKEND', block)
        self.assertIn('#if !defined(__linux__) || !defined(__GLIBC__)', read(CPP))
        self.assertIn('#ifdef OPENXCOM_LOADING_AUDIO_HANDOFF', read(HEADER))

    def test_03_capture_fixed_and_nonlocking(self):
        capture = body(read(CPP), 'LoadingAudioHandoff::captureOwner()')
        for forbidden in ('SDL_GetAudioStatus', 'SDL_LockAudio', 'SDL_PauseAudio', 'Mix_',
                          'Options', 'Observer', 'new ', 'Logger', 'std::string'):
            self.assertNotIn(forbidden, capture)
        ordered(self, capture, ['std::this_thread::get_id()', 'SDL_WasInit(SDL_INIT_AUDIO)',
                               'char name[64]', 'std::memset(name, 0xff, sizeof(name))',
                               'SDL_AudioDriverName(name, sizeof(name))',
                               'reported != name', "std::memchr(name, '\\0', sizeof(name))",
                               'std::strcmp(name, "dummy") == 0'])

    def test_04_claim_dominates_all_mutable_access(self):
        prepare = body(read(CPP), 'LoadingAudioHandoff::prepare()')
        claim = 'if (_claimed.test_and_set(std::memory_order_acq_rel)) std::terminate();'
        before, after = prepare.split(claim)
        self.assertEqual(tokens(before), tokens('if (std::this_thread::get_id() == _snapshot.owner) std::terminate();'))
        self.assertTrue(after.lstrip().startswith('if (!_sealed) std::terminate();'))
        self.assertEqual(clean(read(CPP)).count('_claimed'), 1)
        self.assertIn('std::atomic_flag _claimed = ATOMIC_FLAG_INIT;', read(HEADER))
        self.assertNotIn('AlreadyAttempted', read(HEADER))
        self.assertNotIn('_claimed.clear', read(CPP) + read(HEADER))
        seal = body(read(CPP), 'LoadingAudioHandoff::sealForWorker()')
        ordered(self, seal, ['std::this_thread::get_id() != _snapshot.owner',
                             'if (_sealed) std::terminate()', '_sealed = true',
                             'Phase::WorkerReserved'])

    def test_05_progress_and_exact_native_sequence(self):
        prepare = body(read(CPP), 'LoadingAudioHandoff::prepare()')
        ordered(self, prepare, ['AudioDomain::Initialized', 'Driver::Dummy', 'Mix_QuerySpec(',
                               'Phase::Preflight', 'actual.openCount == 0', 'Reason::NoMixer',
                               'actual.openCount > 1', 'Reason::MultipleMixerOpens',
                               'actual.openCount < 0', 'Reason::InvalidMixerState',
                               'SDL_PauseAudio(1)', 'Phase::PauseIssued', 'SDL_LockAudio()',
                               'SDL_UnlockAudio()', 'Phase::Drained', 'Mix_SetPostMix(NULL, NULL)',
                               'Phase::PostmixDetached', 'resetForExclusiveLoadingAfterPostmixDetach()',
                               'Mix_HookMusic(NULL, NULL)', 'Phase::HooksDetached',
                               'Mix_HaltChannel(-1) != 0', 'Reason::HaltChannelsFailed',
                               'Phase::ChannelsHalted', 'Mix_HaltMusic() != 0',
                               'Reason::HaltMusicFailed', 'Phase::MusicHalted', 'func_mute()',
                               'Phase::PlaybackStopped', 'Phase::ResourceReady',
                               'result(Disposition::ResourceReady, Reason::None)'])
        drain = prepare.split('SDL_LockAudio();', 1)[1].split('SDL_UnlockAudio();', 1)[0]
        self.assertEqual(tokens(drain), [])
        self.assertIn('(actual.format != AUDIO_S8 && actual.format != MIX_DEFAULT_FORMAT)', prepare)
        self.assertIn('#ifndef __NO_MUSIC\n', read(CPP))

    def test_06_fixed_outcome_and_move_only_permit(self):
        header = clean(read(HEADER))
        outcome = body(header, 'struct Outcome')
        self.assertEqual(tokens(outcome), tokens('Disposition disposition; Phase lastCompletedPhase; Reason reason; ActualMixerSpec actual; ObserverDisposition observer;'))
        self.assertIn('ResourceMutationPermit(const ResourceMutationPermit &) = delete;', header)
        self.assertIn('LoadingAudioHandoff(LoadingAudioHandoff &&) = delete;', header)
        permit = header.split('class ResourceMutationPermit', 1)[1].split('struct PrepareResult', 1)[0]
        self.assertIn('private:\n        friend class LoadingAudioHandoff;\n        explicit ResourceMutationPermit(', permit)
        self.assertIn('~ResourceMutationPermit() noexcept = default;', permit)
        self.assertIn('~LoadingAudioHandoff() noexcept = default;', header)
        self.assertNotIn('outcome()', header)
        self.assertNotIn('AudioShutdownComplete', header)
        result = body(read(CPP), 'LoadingAudioHandoff::result(')
        self.assertIn('disposition == Disposition::ResourceReady ? this : nullptr', result)
        self.assertIn('{ other.invalidate(); }', read(CPP))
        self.assertIn('_handoff = nullptr;', read(CPP))
        self.assertIn('sameContextObserver ? ObserverDisposition::NotReset : ObserverDisposition::Absent', read(CPP))
        self.assertIn('ObserverDisposition::NotBuilt}', read(CPP))

    def test_07_private_exclusive_observer_reset(self):
        reset = body(read(OBSERVER), 'MixedAudioObserver::resetForExclusiveLoadingAfterPostmixDetach()')
        ordered(self, reset, ['_attached = false', '_catalog.store(0, std::memory_order_release)',
                             'if (_helloSent)', 'closeDetachedState()', 'LoadingReset::EpochClosed',
                             'if (_fd < 0)', 'LoadingReset::DisabledRetained', 'LoadingReset::BootstrapPreserved'])
        for forbidden in ('Mix_', 'SDL_', 'Clock', '_started', 'randomToken', 'diagnosticProfile',
                          'sendPacket', 'poll(', 'beforeAudioReset', 'publishCatalog', 'Options', 'Logger'):
            self.assertNotIn(forbidden, reset)
        header = read('src/Observer/MixedAudioObserver.h')
        self.assertLess(header.index('resetForExclusiveLoadingAfterPostmixDetach'), header.index('public:'))
        self.assertIn('friend class OpenXcom::LoadingAudioHandoff;', header)

    def test_08_ordinary_close_and_bootstrap_preserved(self):
        old = preimage(OBSERVER)
        current = read(OBSERVER)
        detached = body(current, 'void MixedAudioObserver::closeDetachedState()')
        close = body(current, 'void MixedAudioObserver::close()')
        self.assertEqual(tokens(close.replace('closeDetachedState();', detached)),
                         tokens(body(old, 'void MixedAudioObserver::close()')))
        for signature in ('void MixedAudioObserver::beforeAudioReset(', 'void MixedAudioObserver::poll()',
                          'void MixedAudioObserver::pollImpl()', 'bool MixedAudioObserver::sendPacket(',
                          'void MixedAudioObserver::publishCatalogSupported('):
            self.assertEqual(tokens(body(current, signature)), tokens(body(old, signature)))
        self.assertEqual(current.count('_started(Clock::now())'), 1)
        self.assertEqual(current.count('now - _started >= std::chrono::seconds(30)'), 1)
        self.assertNotIn('_started =', clean(current))
        self.assertEqual(detached.count('::close(_fd)'), 1)
        self.assertIn('_fd = -1;', detached)
        self.assertIn('_challenge.clear();', detached)
        self.assertIn('_admission.clear();', detached)

    def test_09_closed_world_registration_inventory(self):
        registration = re.compile(r'\b(Mix_SetPostMix|Mix_HookMusic|Mix_HookMusicFinished|Mix_ChannelFinished|Mix_RegisterEffect|Mix_SetMusicCMD|SDL_CD\w*)\s*\(')
        found = {}
        for path in (ROOT / 'src').rglob('*.cpp'):
            names = sorted(set(registration.findall(clean(path.read_text()))))
            if names:
                found[str(path.relative_to(ROOT))] = names
        self.assertEqual(found, {
            CPP: ['Mix_HookMusic', 'Mix_SetPostMix'],
            'src/Engine/AdlibMusic.cpp': ['Mix_HookMusic'],
            'src/Engine/FlcPlayer.cpp': ['Mix_HookMusic'],
            'src/Engine/Music.cpp': ['Mix_HookMusic'],
            OBSERVER: ['Mix_SetPostMix'],
        })

    def test_10_no_unreviewed_production_operations(self):
        code = clean(read(CPP))
        for forbidden in ('Mix_OpenAudio', 'Mix_CloseAudio', 'Mix_AllocateChannels', 'Mix_Play',
                          'Mix_Volume', 'SDL_Quit', 'SDL_Init(', 'SDL_GetAudioStatus', 'Options::',
                          'Music::stop', 'Sound::stop', 'Game::', 'Logger', 'new ', 'delete ',
                          'throw ', 'catch ', 'std::function', 'TESTING', '__wrap_', 'sleep(',
                          'poll(', 'join(', 'detach(', 'while (', 'for ('):
            self.assertNotIn(forbidden, code, forbidden)
        self.assertEqual(code.count('SDL_PauseAudio('), 1)
        self.assertIn('#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER\n#include "../Observer/MixedAudioObserver.h"', code)

    def test_11_production_linked_fixture_declarations(self):
        unit = read('tests/snapshot-control/loading_audio_handoff_unit.cpp')
        observer = read('tests/snapshot-control/loading_audio_observer_unit.cpp')
        for text in (unit, observer):
            self.assertIn('#include "../../src/Engine/LoadingAudioHandoff.h"', text)
            self.assertNotIn('#define private', text)
            self.assertNotIn('#include "../../src/Engine/LoadingAudioHandoff.cpp"', text)
            self.assertIn('prepare()', text)
        self.assertIn('LoaderSession', unit)
        self.assertIn('LoadingUiPolicy', unit)
        self.assertIn('HaltChannelsFailed', unit)
        self.assertIn('ChannelsHalted', unit)
        self.assertIn('thread_local bool expectedTargetThreadArmed = false;', unit)
        handler = body(unit, 'void terminated()')
        self.assertTrue(handler.lstrip().startswith('if (!expectedTargetThreadArmed)'))
        self.assertLess(handler.index('std::_Exit(1)'), handler.index('&& world'))
        self.assertIn('std::thread first([&] { (void)handoff.prepare(); });', unit)
        self.assertNotIn('if (nativeFault) expectedTargetThreadArmed = true;', unit)
        self.assertIn('if (repeat) expectedTargetThreadArmed = true;', unit)
        query = body(unit, '__wrap_Mix_QuerySpec(')
        ordered(self, query, ['if (world->reenter)', 'expectedTargetThreadArmed = true',
                              'world->reenter->prepare()', 'expectedTargetThreadArmed = false'])
        halt = body(unit, '__wrap_Mix_HaltChannel(')
        ordered(self, halt, ['faultHaltChannels', 'expectedTargetThreadArmed = true',
                             'std::terminate()'])
        self.assertIn('socketpair', observer)
        self.assertIn('initializeDiagnosticProfile', observer)
        self.assertIn('ObserverDisposition::NotReset', observer)
        self.assertIn('Handoff::Reason::NoMixer', observer)
        for mode in ('--death-owner', '--death-wrong-seal', '--death-unsealed',
                     '--death-reentrant', '--death-concurrent', '--death-repeat',
                     '--death-native-fault'):
            self.assertIn(mode, unit)

    def test_12_new_provenance_and_frozen_contract_inputs(self):
        plan = (FREEZE / 'contracts/AUDIO-RECONSTRUCTION-PLAN.json').read_bytes()
        self.assertEqual(digest(plan), PLAN_SHA)
        self.assertEqual(digest((STAGING / 'SOURCE-RESTORATION.json').read_bytes()), BASELINE_SHA)
        provenance = json.loads((FREEZE / 'PROVENANCE.json').read_text())
        self.assertEqual(provenance['status'], 'NEW_RECONSTRUCTION_NOT_BYTE_IDENTICAL_RECOVERY')
        self.assertFalse(provenance['historical_audio_pass_applies_to_new_source'])
        self.assertFalse(provenance['new_target_execution'])
        for row in provenance['contract_inputs']:
            raw = (FREEZE / row['package_path']).read_bytes()
            self.assertEqual((len(raw), digest(raw)), (row['bytes'], row['sha256']))
        self.assertIn('New reconstruction', read(HEADER))
        self.assertIn('New reconstruction', read(CPP))


if __name__ == '__main__':
    unittest.main()
