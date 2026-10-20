#!/usr/bin/env python3
"""Bounded source contracts only; no Game/renderer/mixer execution."""
import pathlib
import unittest
from test_strategy_widget_architecture import body
ROOT=pathlib.Path(__file__).resolve().parents[2]/'src'
class TacticalEventsArchitectureTest(unittest.TestCase):
    def test_end_turn_only_normal_handler(self):
        d=body('Battlescape/BattlescapeState.cpp','BattlescapeState::agentDispatch(')
        self.assertIn('action == "end_turn") btnEndTurnClick(0)',d)
        self.assertNotIn('requestEndTurn',d)
        normal=body('Battlescape/BattlescapeState.cpp','BattlescapeState::btnEndTurnClick(')
        self.assertIn('allowButtons()',normal);self.assertIn('requestEndTurn()',normal)
        p=body('Battlescape/BattlescapeState.cpp','BattlescapeState::agentUiState(')
        self.assertIn('agentWidgetUncovered(_btnEndTurn, _surfaces)',p)
        self.assertNotIn('allowButtons',p)
    def test_read_only_tactical_projection(self):
        p=body('Battlescape/BattlescapeState.cpp','agentTacticalJson(')
        for s in ('agentMapSnapshot','agentRegionsUncovered','agentOverlayClear','view.hiddenMovement','23500'):
            self.assertIn(s,p)
        for s in ('getUnits','getTile','getFaction','getVisible','getHealth','getTimeUnits','getPosition','getMapSize','RNG','getSide'):
            self.assertNotIn(s,p)
        self.assertIn('fragment_status',p);self.assertIn('unavailable',p)
    def test_mode_requires_post_overlay_public_witness(self):
        p=body('Battlescape/BattlescapeState.cpp','agentTacticalJson(')
        self.assertIn('if (!agentMapModeAdmitted(view, label != "null", count)) return unavailable;',p)
        self.assertLess(p.index('if (!clear(left, top, width, height)) continue;'),p.index('agentMapModeAdmitted'))
        self.assertLess(p.index('if (count++) grid += ",";'),p.index('agentMapModeAdmitted'))
        self.assertLess(p.index('clear(view.messageRegion.x'),p.index('agentMapModeAdmitted'))
        predicate=body('Battlescape/CameraProjection.h','agentMapModeAdmitted(')
        self.assertIn('completeMessageUncovered && !snapshot.hiddenMovementLabel.empty()',predicate)
        self.assertIn('visibleGridCount != 0',predicate)

    def test_tactical_projection_not_control_authority(self):
        p=body('Battlescape/BattlescapeState.cpp','BattlescapeState::agentUiState(')
        self.assertIn('out.controlVisible = out.visible',p)
        self.assertLess(p.index('out.controlVisible = out.visible'),p.index('agentTacticalJson'))
        b=body('Observer/AgentUi.cpp','AgentUiState::sameBinding(')
        self.assertIn('authorityPayload() == o.authorityPayload()',b)
        for s in ('actor == o.actor','selectionRevision == o.selectionRevision','widgets == o.widgets','regions == o.regions'):
            self.assertIn(s,b)
    def test_event_read_does_not_touch_authority(self):
        p=body('Observer/AgentController.cpp','AgentController::eventPage(')
        for s in ('invalidate(', '_capability', '_ledger', 'statePacket(', 'agentDispatch(', '_eventSequence'):
            self.assertNotIn(s,p)
        poll=body('Observer/AgentController.cpp','AgentController::poll(')
        self.assertLess(poll.index("request.kind == 'V'"),poll.index('_ledger.begin'))
        self.assertIn('drainAudioEvents()',poll)
    def test_presented_and_public_only_journal(self):
        p=body('Observer/AgentController.cpp','AgentController::presented(')
        self.assertIn('_publicEvents.presentation(ui.screen, ui.visible)',p)
        self.assertIn('publicState != _lastPublic',p)
        ring=(ROOT/'Observer/AgentPublicEvents.h').read_text()
        for s in ('SavedGame','BattleUnit','SDL_GetTicks','RNG::','getSide','getUnits'):
            self.assertNotIn(s,ring)
        self.assertIn('body == _lastPresentation',ring)
        self.assertIn('Capacity = 256',ring)
    def test_version_and_bounded_messages(self):
        p=(ROOT/'Observer/AgentProtocol.cpp').read_text()
        self.assertIn('openxcom-agent-control/5',p)
        self.assertIn('parts.size() == 5 && parts[0] == "V"',p)
        self.assertIn('9007199254740991ULL, request.after',p)
        c=(ROOT/'Observer/AgentController.cpp').read_text()
        self.assertIn('char bytes[512]',c);self.assertIn('packet.size() <= 49152',c)
if __name__=='__main__':unittest.main(verbosity=2)
