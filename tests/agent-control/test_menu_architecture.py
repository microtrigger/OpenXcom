#!/usr/bin/env python3
"""Source-contract checks, not execution of game states or GUI integration."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / 'src'


def read(relative):
    return (SRC / relative).read_text()


def body(relative, method):
    text = read(relative)
    # Strip comments and literals for balanced-brace scanning only.
    clean = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                   lambda m: ' ' * len(m.group()), text, flags=re.S)
    start = re.search(re.escape(method) + r'\s*\(', clean).start()
    left = clean.index('{', start)
    depth = 1
    for right in range(left + 1, len(clean)):
        depth += (clean[right] == '{') - (clean[right] == '}')
        if depth == 0:
            return text[left + 1:right]
    raise AssertionError('Unclosed function: ' + method)


class MenuArchitectureTest(unittest.TestCase):
    def test_current_offer_required_before_menu_dispatch(self):
        for cls in ('MainMenuState', 'NewBattleState', 'NewGameState', 'OptionsBaseState',
                    'PauseState', 'ListLoadState', 'ListSaveState', 'ErrorMessageState'):
            with self.subTest(state=cls):
                code = body('Menu/' + cls + '.cpp', cls + '::agentDispatch')
                self.assertIn('agentUiState(current)', code)
                self.assertIn('current.offers(action)', code)

    def test_exact_menu_handler_allowlists(self):
        allowed = {
            'MainMenuState': {'btnNewBattleClick', 'btnNewGameClick', 'btnLoadClick', 'btnOptionsClick'},
            'NewBattleState': {'btnOkClick', 'btnCancelClick'},
            'NewGameState': {'btnCancelClick'},
            'OptionsBaseState': {'btnCancelClick'},
            'PauseState': {'btnLoadClick', 'btnSaveClick', 'btnOptionsClick', 'btnCancelClick'},
            'ListLoadState': {'btnCancelClick'}, 'ListSaveState': {'btnCancelClick'},
            'ErrorMessageState': {'btnOkClick'},
        }
        for cls, expected in allowed.items():
            with self.subTest(state=cls):
                code = body('Menu/' + cls + '.cpp', cls + '::agentDispatch')
                self.assertEqual(set(re.findall(r'\b(btn\w+)\((?:nullptr|0)\)', code)), expected)
                for forbidden in ('new ', 'saveGame(', 'loadGame(', 'setSavedGame(', 'pushState(', 'popState('):
                    self.assertNotIn(forbidden, code)

    def test_top_state_modal_guards(self):
        for cls, method in (('MainMenuState', 'agentUiState'), ('NewGameState', 'agentUiState'),
                            ('OptionsBaseState', 'agentUiState'), ('PauseState', 'agentUiState'),
                            ('ListGamesState', 'agentListUiState'), ('ErrorMessageState', 'agentUiState')):
            with self.subTest(state=cls):
                code = body('Menu/' + cls + '.cpp', cls + '::' + method)
                self.assertRegex(code, r'if\s*\(_modal\)\s*return false;')
                self.assertIn('agentWidgetUncovered(', code)
                self.assertIn('agentIncludeWidget(', code)

    def test_new_dialog_fields_do_not_read_game_inventory(self):
        for cls, method in (('MainMenuState', 'agentUiState'), ('NewGameState', 'agentUiState'),
                            ('OptionsBaseState', 'agentUiState'), ('PauseState', 'agentUiState'),
                            ('ErrorMessageState', 'agentUiState')):
            with self.subTest(state=cls):
                code = body('Menu/' + cls + '.cpp', cls + '::' + method)
                for forbidden in ('getSavedGame', 'getSelectedUnit', 'isIronman', 'getStates()', 'getUnits()',
                                  '_difficulty ==', '_selectedRow', '_currentTooltip', 'tr('):
                    self.assertNotIn(forbidden, code)

    def test_list_projection_has_no_row_or_file_access(self):
        code = body('Menu/ListGamesState.cpp', 'ListGamesState::agentListUiState')
        code = re.sub(r'//[^\n]*', '', code)
        for forbidden in ('_saves', '_lstSaves', '_firstValidRow', 'fileName', 'getSelectedRow', 'getCellText'):
            self.assertNotIn(forbidden, code)
        for field in ('title', 'name_heading', 'date_heading', 'details'):
            self.assertIn('"' + field + '"', code)

    def test_list_base_does_not_accidentally_support_other_derived_lists(self):
        header = read('Menu/ListGamesState.h')
        self.assertIn('agentListUiState', header)
        self.assertNotIn('agentUiState(', header)
        self.assertNotIn('agentDispatch(', header)
        for cls, kind in (('ListLoadState', 'load_list'), ('ListSaveState', 'save_list')):
            code = body('Menu/' + cls + '.cpp', cls + '::agentUiState')
            self.assertIn('agentListUiState(out, "' + kind + '")', code)

    def test_ordinary_cancel_side_effects_are_preserved(self):
        code = body('Menu/NewBattleState.cpp', 'NewBattleState::btnCancelClick')
        self.assertIn('save();', code)
        self.assertIn('_game->setSavedGame(0);', code)
        self.assertIn('_game->popState();', code)
        code = body('Menu/OptionsBaseState.cpp', 'OptionsBaseState::btnCancelClick')
        self.assertIn('Options::load();', code)
        self.assertIn('_game->popState();', code)

    def test_pause_uses_normal_help_handler(self):
        code = body('Battlescape/BattlescapeState.cpp', 'BattlescapeState::agentDispatch')
        self.assertIn('action == "open_pause_menu") btnHelpClick(0)', code)
        normal = body('Battlescape/BattlescapeState.cpp', 'BattlescapeState::btnHelpClick')
        self.assertIn('allowButtons(true)', normal)
        self.assertIn('new PauseState(OPT_BATTLESCAPE)', normal)

    def test_version_three_is_explicit(self):
        code = read('Observer/AgentProtocol.cpp')
        self.assertIn('openxcom-agent-control/3', code)
        self.assertNotIn('openxcom-agent-control/2', code)
        self.assertNotIn('openxcom-agent-control/1', code)
        self.assertIn('packet == std::string("H ") + agentProtocolSchema()', code)


if __name__ == '__main__':
    unittest.main(verbosity=2)
