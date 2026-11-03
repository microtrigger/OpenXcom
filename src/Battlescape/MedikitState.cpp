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
#include "MedikitState.h"
#include "MedikitView.h"
#include "../Engine/InteractiveSurface.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Action.h"
#include "../Engine/Palette.h"
#include "../Interface/Text.h"
#include "../Engine/Screen.h"
#include "../Savegame/BattleItem.h"
#include "../Savegame/BattleUnit.h"
#include "../Mod/RuleItem.h"
#include "../Mod/Mod.h"
#include <sstream>
#include <algorithm>
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#endif
#include "../Engine/Options.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/BattleUnitStatistics.h"

namespace OpenXcom
{

/**
 * Helper function that returns a string representation of a type (mainly used for numbers).
 * @param t The value to stringify.
 * @return A string representation of the value.
 */
template<typename type>
std::string toString (type t)
{
	std::ostringstream ss;
	ss << t;
	return ss.str();
}

/**
 * Helper class for the medikit title.
 */
class MedikitTitle : public Text
{
public:
	/// Creates a medikit title.
	MedikitTitle(int y, const std::string & title);
};

/**
 * Initializes a Medikit title.
 * @param y The title's y origin.
 * @param title The title.
 */
MedikitTitle::MedikitTitle (int y, const std::string & title) : Text (73, 9, 186, y)
{
	this->setText(title);
	this->setHighContrast(true);
	this->setAlign(ALIGN_CENTER);
}

/**
 * Helper class for the medikit value.
 */
class MedikitTxt : public Text
{
public:
	/// Creates a medikit text.
	MedikitTxt(int y);
};

/**
 * Initializes a Medikit text.
 * @param y The text's y origin.
 */
MedikitTxt::MedikitTxt(int y) : Text(33, 17, 220, y)
{
	// Note: we can't set setBig here. The needed font is only set when added to State
	this->setColor(Palette::blockOffset(1));
	this->setHighContrast(true);
	this->setAlign(ALIGN_CENTER);
}

/**
 * Helper class for the medikit button.
 */
class MedikitButton : public InteractiveSurface
{
public:
	/// Creates a medikit button.
	MedikitButton(int y);
};

/**
 * Initializes a Medikit button.
 * @param y The button's y origin.
 */
MedikitButton::MedikitButton(int y) : InteractiveSurface(30, 20, 190, y)
{
}

/**
 * Initializes the Medikit State.
 * @param game Pointer to the core game.
 * @param targetUnit The wounded unit.
 * @param action The healing action.
 */
MedikitState::MedikitState (BattleUnit *targetUnit, BattleAction *action) : _targetUnit(targetUnit), _action(action), _revivedTarget(false)
{
	if (Options::maximizeInfoScreens)
	{
		Options::baseXResolution = Screen::ORIGINAL_WIDTH;
		Options::baseYResolution = Screen::ORIGINAL_HEIGHT;
		_game->getScreen()->resetDisplay(false);
	}

	_tu = action->TU;
	_unit = action->actor;
	_item = action->weapon;
	_bg = new Surface(320, 200);

	// Set palette
	_game->getSavedGame()->getSavedBattle()->setPaletteByDepth(this);

	if (_game->getScreen()->getDY() > 50)
	{
		_screen = false;
		_bg->drawRect(67, 44, 190, 100, Palette::blockOffset(15)+15);
	}
	_partTxt = new Text(62, 9, 82, 120);
	_woundTxt = new Text(14, 9, 145, 120);
	_medikitView = new MedikitView(52, 58, 95, 60, _game, _targetUnit, _partTxt, _woundTxt);
	_endButton = new InteractiveSurface(20, 20, 220, 140);
	_stimulantButton = new MedikitButton(84);
	_pkButton = new MedikitButton(48);
	_healButton = new MedikitButton(120);
	_pkText = new MedikitTxt (52);
	_stimulantTxt = new MedikitTxt (88);
	_healTxt = new MedikitTxt (124);
	add(_bg);
	add(_medikitView, "body", "medikit", _bg);
	add(_endButton, "buttonEnd", "medikit", _bg);
	add(new MedikitTitle (37, tr("STR_PAIN_KILLER")), "textPK", "medikit", _bg);
	add(new MedikitTitle (73, tr("STR_STIMULANT")), "textStim", "medikit", _bg);
	add(new MedikitTitle (109, tr("STR_HEAL")), "textHeal", "medikit", _bg);
	add(_healButton, "buttonHeal", "medikit", _bg);
	add(_stimulantButton, "buttonStim", "medikit", _bg);
	add(_pkButton, "buttonPK", "medikit", _bg);
	add(_pkText, "numPK", "medikit", _bg);
	add(_stimulantTxt, "numStim", "medikit", _bg);
	add(_healTxt, "numHeal", "medikit", _bg);
	add(_partTxt, "textPart", "medikit", _bg);
	add(_woundTxt, "numWounds", "medikit", _bg);

	centerAllSurfaces();

	_game->getMod()->getSurface("MEDIBORD.PCK")->blit(_bg);
	_pkText->setBig();
	_stimulantTxt->setBig();
	_healTxt->setBig();
	_partTxt->setHighContrast(true);
	_woundTxt->setHighContrast(true);
	_endButton->onMouseClick((ActionHandler)&MedikitState::onEndClick);
	_endButton->onKeyboardPress((ActionHandler)&MedikitState::onEndClick, Options::keyCancel);
	_healButton->onMouseClick((ActionHandler)&MedikitState::onHealClick);
	_stimulantButton->onMouseClick((ActionHandler)&MedikitState::onStimulantClick);
	_pkButton->onMouseClick((ActionHandler)&MedikitState::onPainKillerClick);
	update();
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
 for(Surface *surface:_surfaces)surface->agentTrackPresentation();
#endif
}

/**
 * Closes the window on right-click.
 * @param action Pointer to an action.
 */
void MedikitState::handle(Action *action)
{
	State::handle(action);
	if (action->getDetails()->type == SDL_MOUSEBUTTONDOWN && action->getDetails()->button.button == SDL_BUTTON_RIGHT)
	{
		onEndClick(0);
	}
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void MedikitState::onEndClick(Action *)
{
	if (Options::maximizeInfoScreens)
	{
		Screen::updateScale(Options::battlescapeScale, Options::baseXBattlescape, Options::baseYBattlescape, true);
		_game->getScreen()->resetDisplay(false);
	}
	_game->popState();
}

/**
 * Handler for clicking on the heal button.
 * @param action Pointer to an action.
 */
void MedikitState::onHealClick(Action *)
{
	int heal = _item->getHealQuantity();
	RuleItem *rule = _item->getRules();
	if (heal == 0)
	{
		return;
	}
	if (_unit->spendTimeUnits(_tu))
	{
		_targetUnit->heal(_medikitView->getSelectedPart(), rule->getWoundRecovery(), rule->getHealthRecovery());
		_item->setHealQuantity(--heal);
		_medikitView->updateSelectedPart();
		_medikitView->invalidate();
		update();

		if (_targetUnit->getStatus() == STATUS_UNCONSCIOUS && _targetUnit->getStunlevel() < _targetUnit->getHealth() && _targetUnit->getHealth() > 0)
		{
			if (!_revivedTarget)
			{
				_targetUnit->setTimeUnits(0);
				if(_targetUnit->getOriginalFaction() == FACTION_PLAYER)
				{
					_action->actor->getStatistics()->revivedSoldier++;
				}
				else if(_targetUnit->getOriginalFaction() == FACTION_HOSTILE)
				{
					_action->actor->getStatistics()->revivedHostile++;
				}
				else
				{
					_action->actor->getStatistics()->revivedNeutral++;
				}
				_revivedTarget = true;
			}
			// if the unit has revived and has no more wounds, we quit this screen automatically
			if (_targetUnit->getFatalWounds() == 0)
			{
				onEndClick(0);
			}
		}
		_unit->getStatistics()->woundsHealed++;
	}
	else
	{
		_action->result = "STR_NOT_ENOUGH_TIME_UNITS";
		onEndClick(0);
	}
}

/**
 * Handler for clicking on the stimulant button.
 * @param action Pointer to an action.
 */
void MedikitState::onStimulantClick(Action *)
{
	int stimulant = _item->getStimulantQuantity();
	RuleItem *rule = _item->getRules();
	if (stimulant == 0)
	{
		return;
	}
	if (_unit->spendTimeUnits (_tu))
	{
		_targetUnit->stimulant(rule->getEnergyRecovery(), rule->getStunRecovery());
		_item->setStimulantQuantity(--stimulant);
		_action->actor->getStatistics()->appliedStimulant++;
		update();

		// if the unit has revived we quit this screen automatically
		if (_targetUnit->getStatus() == STATUS_UNCONSCIOUS && _targetUnit->getStunlevel() < _targetUnit->getHealth() && _targetUnit->getHealth() > 0)
		{
			_targetUnit->setTimeUnits(0);
			if(_targetUnit->getOriginalFaction() == FACTION_PLAYER)
			{
				_action->actor->getStatistics()->revivedSoldier++;
			}
			else if(_targetUnit->getOriginalFaction() == FACTION_HOSTILE)
			{
				_action->actor->getStatistics()->revivedHostile++;
			}
			else
			{
				_action->actor->getStatistics()->revivedNeutral++;
			}
			onEndClick(0);
		}
	}
	else
	{
		_action->result = "STR_NOT_ENOUGH_TIME_UNITS";
		onEndClick(0);
	}
}

/**
 * Handler for clicking on the pain killer button.
 * @param action Pointer to an action.
 */
void MedikitState::onPainKillerClick(Action *)
{
	int pk = _item->getPainKillerQuantity();
	if (pk == 0)
	{
		return;
	}
	if (_unit->spendTimeUnits (_tu))
	{
		_targetUnit->painKillers();
		_item->setPainKillerQuantity(--pk);
		_action->actor->getStatistics()->appliedPainKill++;
		update();
	}
	else
	{
		_action->result = "STR_NOT_ENOUGH_TIME_UNITS";
		onEndClick(0);
	}
}

/**
 * Updates the medikit state.
 */
void MedikitState::update()
{
	_pkText->setText(toString(_item->getPainKillerQuantity()));
	_stimulantTxt->setText(toString(_item->getStimulantQuantity()));
	_healTxt->setText(toString(_item->getHealQuantity()));
	_medikitView->invalidate();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
std::vector<std::uint64_t> MedikitState::agentRevisions() const
{
 std::vector<std::uint64_t> r;
 for(Text *t:{_pkText,_stimulantTxt,_healTxt,_partTxt,_woundTxt}){r.push_back(t->agentPresentationRevision());r.push_back(t->agentTextRevision());}
 for(Surface *s:{static_cast<Surface*>(_medikitView),static_cast<Surface*>(_endButton),static_cast<Surface*>(_healButton),static_cast<Surface*>(_pkButton),static_cast<Surface*>(_stimulantButton)})r.push_back(s->agentPresentationRevision());
 return r;
}
void MedikitState::blit()
{
 _agentPaintFull=false;const SDL_Surface *before=_game->getScreen()->getSurface()->getSurface();
 const bool fullBefore=before&&before->clip_rect.x==0&&before->clip_rect.y==0&&before->clip_rect.w==before->w&&before->clip_rect.h==before->h;
 State::blit();_agentPaintRevisions=agentRevisions();
 const SDL_Surface *d=_game->getScreen()->getSurface()->getSurface();
 _agentPaintFull=fullBefore&&d&&d->clip_rect.x==0&&d->clip_rect.y==0&&d->clip_rect.w==d->w&&d->clip_rect.h==d->h&&Observer::agentPaintHealthy();
}
bool MedikitState::agentUiState(Observer::AgentUiState &out) const
{
 using namespace Observer;
 if(!_game->isState(const_cast<MedikitState*>(this))||_modal||!_agentPaintFull||!agentPaintHealthy()||_agentPaintRevisions!=agentRevisions())return false;
 SavedBattleGame *battle=_game->getSavedGame()->getSavedBattle();
 if(!battle||!_unit||!_targetUnit||!_item||!_action||_action->actor!=_unit||_action->weapon!=_item)return false;
 const SDL_Surface *canvas=_game->getScreen()->getSurface()->getSurface();
 if(!canvas||canvas->clip_rect.x!=0||canvas->clip_rect.y!=0||canvas->clip_rect.w!=canvas->w||canvas->clip_rect.h!=canvas->h)return false;
 if(std::find(battle->getUnits()->begin(),battle->getUnits()->end(),_unit)==battle->getUnits()->end()||std::find(battle->getUnits()->begin(),battle->getUnits()->end(),_targetUnit)==battle->getUnits()->end())return false;
 if(std::find(battle->getItems()->begin(),battle->getItems()->end(),_item)==battle->getItems()->end())return false;
 out=AgentUiState();out.screen="medikit";out.actor=_unit;out.save=battle;
 out.widgets.emplace_back(_targetUnit,0);out.widgets.emplace_back(_item,0);
 const auto paint=agentFlattenPaintOrder(_surfaces);const Surface *dest=_game->getScreen()->getSurface();
 const auto shown=[&](const Surface *w){
  if(!w||!w->agentPresentationPaintedTo(dest)||!agentWidgetVisible(w)||!agentWidgetUncovered(w,paint))return false;
  AgentUiState part;part.regions.push_back({w->getX(),w->getY(),w->getWidth(),w->getHeight()});return _game->agentNonCursorOverlayClear(part);
 };
 const auto text=[&](Text *t){out.widgets.emplace_back(t,t->agentPresentationRevision());out.widgets.emplace_back(t,t->agentTextRevision());return shown(t)&&t->getText().size()<=128?agentJsonString(t->getText()):std::string("null");};
 out.visible="{\"selected_part\":"+text(_partTxt)+",\"selected_wounds\":"+text(_woundTxt)+",\"charges\":{\"heal\":"+text(_healTxt)+",\"stimulant\":"+text(_stimulantTxt)+",\"painkiller\":"+text(_pkText)+"},\"body\":";
 if(shown(_medikitView)){
  agentIncludeWidget(out,_medikitView);std::string runs="[",parts="[";
  for(int y=0;y<_medikitView->getHeight();++y)for(int x=0;x<_medikitView->getWidth();){unsigned c=_medikitView->getPixel(x,y);if(!c){++x;continue;}int start=x;while(x<_medikitView->getWidth()&&_medikitView->getPixel(x,y)==c)++x;if(runs.size()>1)runs+=",";runs+="{\"x\":"+std::to_string(start)+",\"y\":"+std::to_string(y)+",\"width\":"+std::to_string(x-start)+",\"color\":"+std::to_string(c)+"}";}
  const char *labels[]={"Head","Torso","Right arm","Left arm","Right leg","Left leg"};
  for(unsigned i=0;i<6;++i){int x=0,y=0;if(!_medikitView->agentPartPoint(i,x,y))continue;unsigned n=out.rows.size();out.rows.push_back(i);out.points.emplace_back(x,y);if(parts.size()>1)parts+=",";parts+="{\"handle\":\"r"+std::to_string(n)+"\",\"label\":"+agentJsonString(labels[i])+",\"target\":{\"x\":"+std::to_string(x)+",\"y\":"+std::to_string(y)+"}}";}
  out.visible+="{\"x\":"+std::to_string(_medikitView->getX())+",\"y\":"+std::to_string(_medikitView->getY())+",\"width\":"+std::to_string(_medikitView->getWidth())+",\"height\":"+std::to_string(_medikitView->getHeight())+",\"runs\":"+runs+"],\"parts\":"+parts+"]}";
  if(!out.rows.empty())out.actions.emplace_back("select_medikit_part","Select visible body part","visible_control_semantics","visible_row");
 }else out.visible+="null";
 out.visible+="}";
 const std::pair<InteractiveSurface*,const char*> buttons[]={{_healButton,"medikit_heal"},{_stimulantButton,"medikit_stimulant"},{_pkButton,"medikit_painkiller"},{_endButton,"close_medikit"}};
 for(auto b:buttons)if(shown(b.first)){agentIncludeWidget(out,b.first);const std::string label=std::string(b.second)=="medikit_heal"?"Heal":std::string(b.second)=="medikit_stimulant"?"Stimulant":std::string(b.second)=="medikit_painkiller"?"Painkiller":"End";out.actions.emplace_back(b.second,label,"visible_control_semantics");}
 return out.visible.size()<40000;
}
bool MedikitState::agentDispatch(const Observer::AgentRequest &request)
{
 Observer::AgentUiState shown;if(!agentUiState(shown)||!shown.offers(request.action))return false;
 if(request.action=="select_medikit_part"){
  if(request.choice.size()<2||request.choice[0]!='r')return false;unsigned n=std::stoul(request.choice.substr(1));if(n>=shown.points.size())return false;
  _medikitView->agentClickPart(shown.points[n].first,shown.points[n].second);return true;
 }
 if(request.action=="medikit_heal"){onHealClick(nullptr);return true;}
 if(request.action=="medikit_stimulant"){onStimulantClick(nullptr);return true;}
 if(request.action=="medikit_painkiller"){onPainKillerClick(nullptr);return true;}
 if(request.action=="close_medikit"){onEndClick(nullptr);return true;}
 return false;
}
#endif

}
