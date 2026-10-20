#!/usr/bin/env python3
"""Source contracts complement the stub-mixer ring test; no GUI/output proof."""
import pathlib
import re
import unittest
from test_strategy_widget_architecture import body
ROOT = pathlib.Path(__file__).resolve().parents[2]

class AudioEventsArchitectureTest(unittest.TestCase):
    def test_callback_boundary(self):
        callback = body('Observer/AgentAudioEvents.cpp', 'AgentAudioEvents::observe(')
        callback = re.sub(r'//[^\n]*', '', callback)
        for forbidden in ('new ', 'delete ', 'malloc', 'mutex', 'lock(', 'Clock', 'chrono',
                          'SDL_', 'Mix_', 'Game', 'Sound', 'Music', 'send(', 'printf', 'json'):
            self.assertNotIn(forbidden, callback)
        for required in ('MaxCallbackBytes', 'len % FrameBytes', '_skipFirst', '_partialBytes',
                         '_write.store(next, std::memory_order_release)', '_produced.store'):
            self.assertIn(required, callback)
    def test_only_pcm_facts_in_summary(self):
        h = (ROOT / 'src/Observer/AgentAudioEvents.h').read_text()
        self.assertIn('FramesPerWindow = 2205', h)
        self.assertIn('FrameBytes = 4', h)
        self.assertIn('RingCapacity = 32', h)
        self.assertIn('struct Summary { std::uint64_t sequence; bool nonzero; };', h)
        for forbidden in ('BattleUnit', 'Position', 'Sound *', 'Music *', 'channelId', 'timestamp'):
            self.assertNotIn(forbidden, h)
    def test_qualification_and_single_owner(self):
        poll = body('Observer/AgentAudioEvents.cpp', 'AgentAudioEvents::poll(')
        for expected in ('OPENXCOM_AGENT_CONTROL', 'diagnosticProfileQualified()', 'lockFree()',
                         'version->patch != 12', 'rate != 22050', 'format != AUDIO_S16LSB', 'channels != 2'):
            self.assertIn(expected, poll)
        cmake = (ROOT / 'CMakeLists.txt').read_text()
        self.assertIn('if ( EXPERIMENTAL_AGENT_CONTROL AND EXPERIMENTAL_MIXED_AUDIO_OBSERVER )', cmake)
        close = body('Observer/AgentAudioEvents.cpp', 'AgentAudioEvents::close(')
        close = re.sub(r'//[^\n]*', '', close)
        self.assertIn('if (_attached)', close)
        self.assertIn('Mix_SetPostMix(NULL, NULL)', close)
        self.assertNotIn('SDL_LockAudio', close)
    def test_lifecycle_and_independent_poll(self):
        game = (ROOT / 'src/Engine/Game.cpp').read_text()
        self.assertIn('_agentAudioEvents.poll(_agentController.enabled());\n        _agentController.poll();', game)
        self.assertIn('_agentAudioEvents.publishCatalogSupported(_mod->isMusicCatalogSoftwareMixed())', game)
        self.assertIn('_agentAudioEvents.close()', body('Engine/Game.cpp', 'Game::~Game('))
        self.assertIn('resetAgentAudioEvents()', body('Engine/Game.cpp', 'Game::initAudio('))
        self.assertIn('resetAgentAudioEvents(true)', body('Menu/StartState.cpp', 'void StartState::init()\n'))
        for method in ('FlcPlayer::initAudio(', 'FlcPlayer::deInitAudio('):
            self.assertIn('resetAgentAudioEvents()', body('Engine/FlcPlayer.cpp', method))

if __name__ == '__main__': unittest.main(verbosity=2)
