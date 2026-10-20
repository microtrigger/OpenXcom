#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Source-only tripwires for the staged, default-OFF Game integration.

This does not execute/parse/import application code, invoke a compiler, prove
native identity, or replace the independently linked behavioral fixtures.
The separate command release must pin this file, every input and its real
outer exit. OFF object/call-family parity requires separate compiler evidence.
"""
import argparse
import hashlib
import json
from pathlib import Path

PRODUCTION = (
    'CMakeLists.txt', 'src/CMakeLists.txt', 'src/Engine/Game.h', 'src/Engine/Game.cpp',
    'src/Menu/StartState.h', 'src/Menu/StartState.cpp', 'src/main.cpp',
    'src/Menu/OptionsBaseState.cpp', 'src/Menu/ModListState.cpp',
    'src/Engine/LoadingAudioHandoff.h', 'src/Engine/LoadingAudioHandoff.cpp',
    'src/Observer/MixedAudioObserver.h', 'src/Observer/MixedAudioObserver.cpp',
    'src/Engine/AdlibMusic.h', 'src/Engine/AdlibMusic.cpp', 'src/Engine/Adlib/fmopl.cpp',
    'src/Engine/FlcPlayer.h', 'src/Engine/FlcPlayer.cpp',
    'src/Menu/VideoState.h', 'src/Menu/VideoState.cpp',
    'src/Engine/Music.h', 'src/Engine/Music.cpp', 'src/Mod/Mod.h', 'src/Mod/Mod.cpp',
)
TESTS = tuple('tests/snapshot-control/' + name for name in (
    'loading_game_integration_unit.cpp', 'loading_audio_lifecycle_unit.cpp',
    'loading_adlib_failure_unit.cpp', 'loading_video_audio_unit.cpp',
    'loading_video_integration_unit.cpp', 'loading_game_integration_fixture.h',
    'loading_game_integration_source_contract.py',
))
UNCHANGED = (
    'src/Engine/LoaderSession.h', 'src/Engine/LoaderSession.cpp',
    'src/Engine/LoaderThreadLinux.h', 'src/Engine/LoaderThreadLinux.cpp',
    'src/Engine/LoaderSdlTls.cpp', 'src/Engine/LoadingUiPolicy.h',
    'src/Engine/LoadingUiPolicy.cpp', 'src/Engine/Screen.h', 'src/Engine/Screen.cpp',
    'src/Engine/Zoom.h', 'src/Engine/Zoom.cpp', 'src/Engine/Logger.h',
)


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def read(repo, name):
    path = repo / name
    require(path.is_file() and not path.is_symlink(), 'missing/nonregular input: ' + name)
    return path.read_text(encoding='utf-8')


def body(source, signature, following):
    start = source.index(signature)
    return source[start:source.index(following, start + len(signature))]


def check(repo, pins_path):
    sources = {name: read(repo, name) for name in PRODUCTION + TESTS}
    pins = json.loads(pins_path.read_text(encoding='utf-8'))
    focused = {entry['path']: entry for entry in pins['focused_sources']}
    for name in UNCHANGED:
        data = (repo / name).read_bytes()
        require(hashlib.sha256(data).hexdigest() == focused[name]['sha256'], 'changed reused primitive: ' + name)
    cmake = sources['CMakeLists.txt']
    require('option ( OPENXCOM_BUILD_RESPONSIVE_LOADING ' in cmake, 'missing feature option')
    option = next(line for line in cmake.splitlines() if line.startswith('option ( OPENXCOM_BUILD_RESPONSIVE_LOADING '))
    require(option.endswith(' OFF )'), 'integration must default OFF')
    require('NOT OPENXCOM_BUILD_LINUX_LOADER_BACKEND' in cmake and
            'NOT OPENXCOM_BUILD_LOADING_PRESENTATION_HELPERS' in cmake and
            'NOT OPENXCOM_BUILD_LOADING_AUDIO_HANDOFF' in cmake, 'missing explicit dependency rejection')
    require(all(macro not in sources['src/CMakeLists.txt'] for macro in
                ('OPENXCOM_RESPONSIVE_TESTING', 'OPENXCOM_RESPONSIVE_LOADING_TESTING')),
            'shipping target enables test access')
    require('PRIVATE OPENXCOM_RESPONSIVE_LOADING' in sources['src/CMakeLists.txt'], 'missing shipping feature definition')
    for name, origin in (
        ('src/main.cpp', 'Startup'),
        ('src/Menu/OptionsBaseState.cpp', 'OptionsMenu'),
        ('src/Menu/ModListState.cpp', 'ModListMenu'),
    ):
        source = sources[name]
        require('reserveResponsiveLoad(Game::LoadingOrigin::' + origin in source, 'real caller absent: ' + name)
        require('new StartState' in source, 'OFF caller removed: ' + name)
        if origin != 'Startup':
            callback = source[source.index('::btnOkClick('):]
            require(callback.index('reserveResponsiveLoad') < callback.index('Options::save()'), 'save precedes admission: ' + name)
            require('loadingAdmissionMessage(result)' in callback, 'missing retained-menu rejection: ' + name)
    game = sources['src/Engine/Game.cpp']
    run = body(game, 'void Game::run()', 'void Game::quit()')
    require(run.index('loadingBoundary()') < run.index('// Clean up states'), 'operation branch does not dominate loop')
    require('runResponsiveVideoTick()' in run, 'shipping loop omits actual video branch')
    require(run.count('loadingBoundary()') >= 4, 'missing callback-return boundaries')
    require('responsiveLoopYielded' in run and 'const Uint8 focus = SDL_GetAppState()' in run,
            'ordinary loop does not reconcile consumed responsive focus events')
    video_tick = body(game, 'bool Game::runResponsiveVideoTick()', 'std::shared_ptr<void> Game::retainResponsiveEpoch()')
    require('responsiveVideoTick(SDL_GetTicks(), _responsive->renderAllowed)' in video_tick,
            'video decode/presentation does not receive immutable focus intent')
    video_reserve = body(game, 'Game::VideoAudioResult Game::reserveVideoAudio', 'LoaderSession::WorkResult Game::transitionResponsiveVideo')
    require('operation->allowResize = config.allowResize' in video_reserve,
            'attach/detach operation drops captured resize intent')
    boundary = body(game, 'bool Game::loadingBoundary()', 'LoaderSession::WorkResult Game::loadResponsiveResources')
    require(boundary.index('_deleted.splice') < boundary.index('delete retired') < boundary.index('workerOwned = true') < boundary.index('startPrepared'), 'commit/deletion/transfer/start order changed')
    require('setState(' not in boundary and 'popState(' not in boundary, 'allocating stack API in commit')
    pump = body(game, 'void Game::runOneOperationTick()', 'Game::VideoAudioResult Game::reserveVideoAudio')
    require('Logger::ScopedSuppression' in pump and 'remainingEventBudget()' in pump, 'active owner snapshots/budget absent')
    require(pump.index('SDL_PollEvent') < pump.index('policy->tick()'), 'poll precedes events')
    for prohibited in ('Options::', '->_mod', '->_lang', '_screen->handle(', '_screen->flip(', 'SDL_LockAudio(', 'Mix_CloseAudio(', 'SDL_WaitThread(', 'SDL_KillThread('):
        require(prohibited not in pump, 'active tick contains prohibited access: ' + prohibited)
    require('cleanupSession->requestClose' not in game, 'reusable cleanup admission was closed')
    require('cleanupResult.reason == LoaderSession::Reason::NoThread' in pump and
            'LoaderSession::Reason::ThreadSetupFailure' in pump and '!cleanup.entered' in pump,
            'retry requires consumed proven-unentered attempt')
    require('OPENXCOM_RESPONSIVE_NATIVE_PROFILE_VERIFIED' in game, 'native activation gate absent')
    reservation = body(game, 'Game::LoadingAdmission Game::reserveResponsiveLoad', 'void Game::applyResponsiveResize')
    require('Options::switchDisplay' not in reservation and '_screen->resetDisplay' not in reservation,
            'fallible display mutation escaped the committed boundary')
    load = body(game, 'LoaderSession::WorkResult Game::loadResponsiveResources', 'LoaderSession::WorkResult Game::cleanupResponsiveResources')
    require(load.index('epoch.unknownNativeResources = true') < load.index('new Mod()') <
            load.index('epoch.mod->loadAll') < load.index('epoch.mod->responsiveMusicCatalog()') <
            load.index('epoch.unknownNativeResources = false'), 'partial native-provider graph lost retention authority')
    require('openedChunkRequest != work.plan.chunkSize' in load, 'approved chunk changes bypass reopen')
    require('ResponsiveReceiptSlot<FlcPlayer::AudioLease::AttachReceipt>' in game and
            'ResponsiveReceiptSlot<FlcPlayer::AudioLease::DetachReceipt>' in game,
            'video typed receipts are not retained with exact worker context')
    require('videoResumeWork' in boundary and 'menuResumePending' in boundary and
            'responsiveNoHookFailure' in boundary, 'menu resume lost its exact retired transition')
    require('videoResumeUnentered' in boundary and 'continuation.session->quiescent()' in boundary and
            'LoaderSession::Reason::ThreadSetupFailure' in boundary,
            'later unentered clip lost its independently retired no-mutation chain')
    terminal = sources['src/Menu/StartState.cpp']
    constructor = body(terminal, 'StartState::StartState() : _font', 'StartState::~StartState()')
    require(constructor.index('add(text.get())') < constructor.index('_text->initText') < constructor.index('_text->setPalette'),
            'terminal retained default Mod font/language/palette after State::add')
    for name in TESTS[:-1]:
        source = sources[name]
        require('#include "../../src/Engine/Game.cpp"' not in source and '#include "../../src/Menu/VideoState.cpp"' not in source,
                'fixture includes production implementation: ' + name)
    return {
        'status': 'source_tripwires_only',
        'not_proved': ['compilation', 'OFF object parity', 'behavioral cases', 'native/provider/converter fidelity', 'full application'],
        'source_pins': [{
            'path': name, 'bytes': len((repo / name).read_bytes()),
            'sha256': hashlib.sha256((repo / name).read_bytes()).hexdigest(),
        } for name in PRODUCTION + TESTS],
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--design-source-pins', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(check(args.repo.resolve(), args.design_source_pins.resolve()), indent=2))
