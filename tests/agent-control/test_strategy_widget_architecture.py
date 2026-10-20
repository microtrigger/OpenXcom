#!/usr/bin/env python3
"""Source contracts complement AgentWidgetInputTest; no GUI/game simulation."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2] / 'src'


def body(relative, signature):
    text = (ROOT / relative).read_text()
    clean = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                   lambda match: ' ' * len(match.group()), text, flags=re.S)
    left = clean.index('{', clean.index(signature))
    depth = 1
    for right in range(left + 1, len(clean)):
        depth += (clean[right] == '{') - (clean[right] == '}')
        if not depth:
            return text[left + 1:right]
    raise AssertionError('Unclosed method')


class StrategyWidgetArchitectureTest(unittest.TestCase):
    def test_radio_and_toggle_share_normal_mutation(self):
        mouse = body('Interface/TextButton.cpp', 'TextButton::mousePress(')
        self.assertIn('selectRadio()', mouse)
        mouse = body('Interface/ToggleTextButton.cpp', 'ToggleTextButton::mousePress(')
        self.assertIn('setPressed(!_isPressed)', mouse)
        draw = body('Interface/TextButton.cpp', 'TextButton::draw(')
        self.assertIn('_agentDrawnPressed = press', draw)
        read = body('Interface/TextButton.cpp', 'TextButton::agentPressed(')
        self.assertIn('_agentDrawnPressed', read)
        self.assertNotIn('_group', read)

    def test_editor_shares_constraints_and_rolls_back(self):
        ordinary = body('Interface/TextEdit.cpp', 'TextEdit::keyboardPress(')
        native = body('Interface/TextEdit.cpp', 'TextEdit::agentReplaceText(')
        self.assertIn('insertInputCharacter(c)', ordinary)
        self.assertIn('insertInputCharacter(c)', native)
        self.assertIn('Observer::agentInputText(text)', native)
        self.assertIn('agentRenderedText(rendered)', native)
        self.assertIn('_value = previous', native)
        self.assertNotIn('setText(', native)
        self.assertNotIn('Action', native)

    def test_name_change_preserves_enter_and_hides_empty_ok(self):
        ordinary = body('Geoscape/BaseNameState.cpp', 'BaseNameState::edtNameChange(')
        shared = body('Geoscape/BaseNameState.cpp', 'BaseNameState::applyNameChange(')
        self.assertIn('applyNameChange(', ordinary)
        self.assertIn('SDLK_RETURN', ordinary)
        self.assertIn('SDLK_KP_ENTER', ordinary)
        self.assertIn('_btnOk->setVisible(!_edtName->getText().empty())', shared)
        self.assertIn('if (commit)', shared)
        typed = body('Geoscape/BaseNameState.cpp', 'BaseNameState::agentDispatch(')
        self.assertIn('applyNameChange(false)', typed)
        self.assertIn('btnOkClick(nullptr)', typed)
        for forbidden in ('setName(', 'setText(', 'SDL_Event', 'new Action'):
            self.assertNotIn(forbidden, typed)

    def test_name_modal_exception_is_only_own_focused_editor(self):
        projection = body('Geoscape/BaseNameState.cpp', 'BaseNameState::agentUiState(')
        self.assertIn('_modal != _edtName || !_edtName->isFocused()', projection)
        self.assertIn('if (!_modal && nameJson != "null"', projection)
        self.assertIn('_edtName->agentEditable()', projection)
        self.assertIn('agentWidgetUncovered(_edtName, _surfaces)', projection)

    def test_new_game_dispatch_uses_rendered_choices_and_normal_start(self):
        typed = body('Menu/NewGameState.cpp', 'NewGameState::agentDispatch(const Observer::AgentRequest &request)')
        self.assertIn('agentUiState(current)', typed)
        self.assertIn('current.offers(action)', typed)
        self.assertIn('agentPressed(selected)', typed)
        self.assertIn('selectRadio()', typed)
        self.assertIn('setPressed(request.desired)', typed)
        self.assertIn('btnOkClick(nullptr)', typed)
        for forbidden in ('SDL_Event', 'setDifficulty(', 'setIronman(', 'newSave(', 'setSavedGame('):
            self.assertNotIn(forbidden, typed)
        projection = body('Menu/NewGameState.cpp', 'NewGameState::agentUiState(')
        self.assertNotRegex(projection, r'\b_difficulty\b')
        self.assertNotIn('getPressed()', projection)

    def test_geoscape_is_rendered_read_only_hud(self):
        projection = body('Geoscape/GeoscapeState.cpp', 'GeoscapeState::agentUiState(')
        for key in ('hour', 'minute', 'second', 'weekday', 'day', 'month', 'year', 'funds'):
            self.assertIn('"' + key + '"', projection)
        for identifier in ('intercept', 'bases', 'graphs', 'ufopaedia', 'options', 'funding'):
            self.assertIn('"' + identifier + '"', projection)
        self.assertIn('agentPressed(selected)', projection)
        self.assertIn('agentWidgetUncovered(', projection)
        self.assertIn('agentRegionsDisjoint(out)', projection)
        self.assertIn('if (_modal) return false', projection)
        self.assertIn('if (!_dogfights.empty()) return false', projection)
        self.assertIn('AgentUiAction("open_funding", label, "visible_text")', projection)
        dispatch = body('Geoscape/GeoscapeState.cpp', 'GeoscapeState::agentDispatch(')
        self.assertIn('btnFundingClick(nullptr)', dispatch)
        for forbidden in ('getSavedGame', '_timeSpeed', 'getTime(', 'getFunds(', 'tr('):
            self.assertNotIn(forbidden, projection)


if __name__ == '__main__':
    unittest.main(verbosity=2)
