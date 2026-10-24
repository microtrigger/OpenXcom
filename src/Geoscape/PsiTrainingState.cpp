/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <sstream>
#include "PsiTrainingState.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "AllocatePsiTrainingState.h"
#include "../Engine/Options.h"

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#include "../Observer/AgentUi.h"
#include "../Observer/AgentProtocol.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Psi Training screen.
 * @param game Pointer to the core game.
 */
PsiTrainingState::PsiTrainingState()
{
	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_txtTitle = new Text(300, 17, 10, 16);
	_btnOk = new TextButton(160, 14, 80, 174);

	// Set palette
	setInterface("psiTraining");

	add(_window, "window", "psiTraining");
	add(_btnOk, "button2", "psiTraining");
	add(_txtTitle, "text", "psiTraining");

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK01.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&PsiTrainingState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&PsiTrainingState::btnOkClick, Options::keyCancel);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_PSIONIC_TRAINING"));

	int buttons = 0;
	for (std::vector<Base*>::const_iterator b = _game->getSavedGame()->getBases()->begin(); b != _game->getSavedGame()->getBases()->end(); ++b)
	{
		if ((*b)->getAvailablePsiLabs())
		{
			TextButton *btnBase = new TextButton(160, 14, 80, 40 + 16 * buttons);
			btnBase->onMouseClick((ActionHandler)&PsiTrainingState::btnBaseXClick);
			btnBase->setText((*b)->getName());
			add(btnBase, "button1", "psiTraining");
			_bases.push_back(*b);
			_btnBases.push_back(btnBase);
			++buttons;
			if (buttons >= 8)
			{
				break;
			}
		}
	}

	centerAllSurfaces();
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *widget : std::vector<Surface *>{_window, _txtTitle, _btnOk})
        widget->agentTrackPresentation();
    for (TextButton *widget : _btnBases) widget->agentTrackPresentation();
    // Snapshot native callback ownership only. Never reconstruct from names or
    // re-query model eligibility when observing or selecting a displayed button.
    for (size_t i = 0; i < _btnBases.size(); ++i)
        _agentConstructorBasePairs.emplace_back(_btnBases[i], _bases.at(i));
#endif
}

/**
 *
 */
PsiTrainingState::~PsiTrainingState()
{

}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void PsiTrainingState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * Goes to the allocation screen for the corresponding base.
 * @param action Pointer to an action.
 */
void PsiTrainingState::btnBaseXClick(Action *action)
{
	for (size_t i = 0; i < _btnBases.size(); ++i)
	{
		if (action->getSender() == _btnBases[i])
		{
			_game->pushState(new AllocatePsiTrainingState(_bases.at(i)));
			break;
		}
	}
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool PsiTrainingState::agentBasePairsCurrent() const
{
    if (_btnBases.size() > 8 || _btnBases.size() != _bases.size() ||
        _btnBases.size() != _agentConstructorBasePairs.size()) return false;
    for (size_t i = 0; i < _btnBases.size(); ++i)
        if (!_btnBases[i] || !_bases[i] || _agentConstructorBasePairs[i].first != _btnBases[i] ||
            _agentConstructorBasePairs[i].second != _bases[i]) return false;
    return true;
}

bool PsiTrainingState::agentDestinationFull() const
{
    const SDL_Surface *destination = _game->getScreen()->getSurface()->getSurface();
    return destination && destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> PsiTrainingState::agentPaintRevisions() const
{
    std::vector<std::uint64_t> result = {_window->agentPresentationRevision(),
        _txtTitle->agentPresentationRevision(), _txtTitle->agentTextRevision(),
        _btnOk->agentPresentationRevision(), _btnOk->agentLabelRevision()};
    for (const TextButton *widget : _btnBases)
    {
        result.push_back(widget->agentPresentationRevision());
        result.push_back(widget->agentLabelRevision());
    }
    return result;
}

void PsiTrainingState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    if (!agentBasePairsCurrent()) return;
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool PsiTrainingState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<PsiTrainingState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        !agentBasePairsCurrent() || _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState();
    out.screen = "psi_training";
    out.actor = this; // Private active-popup identity, never serialized.
    const Surface *destination = _game->getScreen()->getSurface();
    if (!_window->agentPresentationPaintedTo(destination)) return false;
    const auto painted = agentFlattenPaintOrder(_surfaces);
    const auto include = [&](const Surface *widget, bool observationOnly) -> bool {
        if (!widget->agentPresentationPaintedTo(destination) || !agentWidgetVisible(widget)) return false;
        AgentUiState part;
        part.regions.push_back({widget->getX(), widget->getY(), widget->getWidth(), widget->getHeight()});
        // Allocation geometry only: do not require glyph-ink evidence for text.
        bool found = false;
        for (const Surface *later : painted)
        {
            if (later == widget) { found = true; continue; }
            if (!found || !later || !later->getVisible()) continue;
            const auto &rect = part.regions.front();
            if (rect.x < later->getX() + later->getWidth() && rect.x + rect.width > later->getX() &&
                rect.y < later->getY() + later->getHeight() && rect.y + rect.height > later->getY()) return false;
        }
        if (!found || !(observationOnly ? _game->agentNonCursorOverlayClear(part) : _game->agentOverlayClear(part))) return false;
        if (observationOnly) return true;
        const auto before = out.regions.size();
        out.regions.insert(out.regions.end(), part.regions.begin(), part.regions.end());
        if (agentRegionsDisjoint(out)) return true;
        out.regions.resize(before);
        return false;
    };
    // These are current active UI captions, not a glyph-pixel readability test.
    // Hidden, geometrically absent, occluded or stale widgets contribute no value.
    const auto text = [&](const Text *widget) -> std::string {
        out.widgets.emplace_back(widget, widget->agentPresentationRevision());
        out.widgets.emplace_back(widget, widget->agentTextRevision());
        out.context.insert(out.context.end(), {
            static_cast<std::uint64_t>(widget->getX()), static_cast<std::uint64_t>(widget->getY()),
            static_cast<std::uint64_t>(widget->getWidth()), static_cast<std::uint64_t>(widget->getHeight())});
        // Active logical captions: glyph/ink completeness and pointer overlap
        // are not text admission criteria. Resource bound is not a layout test.
        if (widget->getText().size() > 1024 || !include(widget, true)) return "null";
        return agentJsonString(widget->getText());
    };
    const auto title = text(_txtTitle);
    // Existing read-only captions plus actionable current native button choices.
    // Public handles enumerate only admitted controls, never native/world indices.
    std::string bases = "[", rows = "[";
    for (size_t i = 0; i < _btnBases.size(); ++i)
    {
        TextButton *widget = _btnBases[i];
        out.widgets.emplace_back(_bases[i], 0); // Private callback pairing only.
        out.widgets.emplace_back(widget, widget->agentPresentationRevision());
        out.widgets.emplace_back(widget, widget->agentLabelRevision());
        out.context.insert(out.context.end(), {
            static_cast<std::uint64_t>(widget->getX()), static_cast<std::uint64_t>(widget->getY()),
            static_cast<std::uint64_t>(widget->getWidth()), static_cast<std::uint64_t>(widget->getHeight())});
        const std::string label = widget->getText();
        if (label.size() > 1024 || !include(widget, true)) continue;
        if (bases.size() > 1) bases += ",";
        bases += agentJsonString(label);
        if (include(widget, false))
        {
            if (rows.size() > 1) rows += ",";
            rows += "{\"handle\":" + agentJsonString("r" + std::to_string(out.rows.size())) +
                ",\"cells\":[" + agentJsonString(label) + "]}";
            out.rows.push_back(i); // Private admitted-row -> exact constructor button.
        }
    }
    bases += "]";
    rows += "]";
    if (!out.rows.empty())
        out.actions.emplace_back("open_psi_training_base", "Open psi training base", "visible_control_semantics", "visible_row");
    std::string buttons = "[";
    const auto button = [&](TextButton *widget, const char *id, const char *action) {
        out.widgets.emplace_back(widget, widget->agentPresentationRevision());
        out.widgets.emplace_back(widget, widget->agentLabelRevision());
        const std::string label = widget->getText();
        if (label.empty() || label.size() > 1024 || !include(widget, false)) return;
        if (buttons.size() > 1) buttons += ",";
        buttons += "{\"id\":" + agentJsonString(id) + ",\"label\":" + agentJsonString(label) + "}";
        out.actions.emplace_back(action, label, "visible_text");
    };
    button(_btnOk, "ok", "close_psi_training");
    buttons += "]";
    out.visible = "{\"title\":" + title + ",\"bases\":" + bases + ",\"rows\":" + rows + ",\"buttons\":" + buttons + "}";
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return !out.regions.empty() && agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}

bool PsiTrainingState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "close_psi_training") { btnOkClick(nullptr); return true; }
    if (request.action == "open_psi_training_base")
    {
        for (size_t row = 0; row < current.rows.size(); ++row)
            if (request.choice == "r" + std::to_string(row))
            {
                const size_t index = current.rows[row];
                if (index >= _btnBases.size()) return false;
                SDL_Event event = {}; event.type = SDL_MOUSEBUTTONDOWN; event.button.button = SDL_BUTTON_LEFT;
                Action action(&event, 1.0, 1.0, 0, 0);
                action.setSender(_btnBases[index]);
                btnBaseXClick(&action);
                return true;
            }
    }
    return false;
}
#endif

}
