#!/usr/bin/env python3
"""Bounded source contracts; not execution of states or GUI acceptance."""
import pathlib
import unittest
from test_strategy_widget_architecture import body
ROOT = pathlib.Path(__file__).resolve().parents[2] / 'src'

class ListArchitectureTest(unittest.TestCase):
    def test_only_named_newbattle_combos(self):
        p=body('Menu/NewBattleState.cpp','NewBattleState::agentUiState(')
        for name in ('_cbxMission','_cbxCraft','_cbxTerrain','_cbxDifficulty','_cbxAlienRace'):
            self.assertIn(name,p)
        for forbidden in ('_missionTypes','_terrainTypes','_alienRaces','_crafts','getCellText','getSelected','getRows'):
            self.assertNotIn(forbidden,p)
        self.assertIn('_modal != combos[active]',p)
        self.assertIn('active != -1',p)
        self.assertIn('agentProjectList',p)
    def test_local_rows_bound_privately(self):
        p=body('Observer/AgentUi.cpp','agentProjectList(')
        self.assertIn('row.logicalIndex',p)
        self.assertIn('ui.rows.push_back',p)
        self.assertIn('agentListRevision()',p)
        self.assertIn('count == 32',p)
        self.assertIn('agentRegionsUncovered(regions, owner, paintOrder)',p)
        bind=body('Observer/AgentUi.cpp','AgentUiState::sameBinding(')
        self.assertIn('widgets == o.widgets',bind)
        self.assertIn('rows == o.rows',bind)
        payload=body('Observer/AgentUi.cpp','AgentUiState::payload(')
        self.assertNotIn('widgets',payload);self.assertNotIn('rows',payload)
    def test_direction_comes_only_from_painted_arrow(self):
        p=body('Observer/AgentUi.cpp','agentProjectList(')
        self.assertIn('agentScrollControls',p)
        self.assertIn('agentWidgetVisible(arrow)',p)
        for forbidden in ('getScroll','getRows','getTexts','_scroll'):
            self.assertNotIn(forbidden,p)
        for path,cls in [('Menu/NewBattleState.cpp','NewBattleState'),('Geoscape/FundingState.cpp','FundingState')]:
            p=body(path,cls+'::agentDispatch(const Observer::AgentRequest &request)')
            self.assertIn('scrollUp(false, false)',p)
            self.assertIn('scrollDown(false, false)',p)
    def test_choose_routes_to_shared_widget(self):
        p=body('Menu/NewBattleState.cpp','NewBattleState::agentDispatch(const Observer::AgentRequest &request)')
        for expected in ('agentUiState(current)','current.offers(action)','current.rows[slot]','agentChooseVisibleRow','agentClose(nullptr, this)','agentOpen()'):
            self.assertIn(expected,p)
        for forbidden in ('getSelected','setSelected','getCellText','SDL_Event','new Action','cbxMissionChange('):
            self.assertNotIn(forbidden,p)
    def test_funding_only_drawn_cells_and_close(self):
        p=body('Geoscape/FundingState.cpp','FundingState::agentUiState(')
        self.assertIn('if (_modal) return false',p)
        self.assertIn('agentProjectList(_lstCountries',p)
        self.assertIn('agentWidgetUncovered',p)
        for forbidden in ('getCountries','getFunding','getCountryFunding','getSavedGame','getCellText','getSelectedRow','tr('):
            self.assertNotIn(forbidden,p)
        d=body('Geoscape/FundingState.cpp','FundingState::agentDispatch(')
        self.assertIn('btnOkClick(nullptr)',d)
        for forbidden in ('choose_visible_row','setFunding','popState','new Action','SDL_Event'):
            self.assertNotIn(forbidden,d)
    def test_geoscape_only_funding_route(self):
        d=body('Geoscape/GeoscapeState.cpp','GeoscapeState::agentDispatch(')
        self.assertIn('agentUiState(current)',d)
        self.assertIn('current.offers(action)',d)
        self.assertIn('btnFundingClick(nullptr)',d)
        self.assertNotIn('pushState',d)
        self.assertNotIn('setTime',d)
    def test_generic_state_does_not_discover_lists(self):
        p=(ROOT/'Engine/State.h').read_text()
        self.assertIn('virtual bool agentUiState(Observer::AgentUiState &) const { return false; }',p)
        for token in ('agentProjectList','choose_visible_row','scroll_visible_list'):
            self.assertNotIn(token,p)
    def test_dotted_core_is_private_and_draw_proven(self):
        h=(ROOT/'Interface/Text.h').read_text()
        private=h[h.index('private:'):h.index('public:')]
        self.assertIn('friend class TextList;',private)
        self.assertIn('agentMarkListContent',private)
        self.assertIn('agentListContent',private)
        draw=body('Interface/Text.cpp','Text::draw(')
        for token in ('_agentListShown.push_back(*c)', '_processedText == Unicode::convUtf8ToUtf32(_text)',
                      'if (listContent) _agentListComplete = false', 'font->agentGlyphInk'):
            self.assertIn(token,draw)
        self.assertIn('_agentListRange = _agentListComplete = false',body('Interface/Text.cpp','Text::setText('))
        capture=body('Interface/TextList.cpp','TextList::agentCaptureRow(')
        for token in ('agentListContent(shownText, rect, hasInk)', 'agentPaintedBounds(decorated)',
                      'agentListOverlap(previous, decorated)', 'agentTextComplete()'):
            self.assertIn(token,capture)
    def test_caret_partial_visibility_not_edit_constraint(self):
        for method in ('agentRenderedText(', 'agentEditable(', 'agentReplaceText('):
            p=body('Interface/TextEdit.cpp','TextEdit::'+method)
            self.assertNotIn('_caret->agentTextComplete()',p)
        p=body('Interface/TextEdit.cpp','TextEdit::agentReplaceText(')
        self.assertIn('insertInputCharacter(c)',p)
        self.assertIn('agentRenderedText(rendered)',p)
        self.assertIn('_value = previous',p)

if __name__=='__main__': unittest.main(verbosity=2)
