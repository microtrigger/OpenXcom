#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentMapInputTrace.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_OPEN_CONTROL_GATE
#include "../../tests/inventory-open-control/InventoryOpenControlGate.h"
#endif
// Fork modification 2026-10-12: default-off supervised turn pilot; GPL-3.0-or-later.
// Fork modification 2026-10-12: default-off supervised map left-click pilot; GPL-3.0-or-later.
#ifdef OPENXCOM_EXPERIMENTAL_STANCE_CONTROL_GATE
#include "../../tests/stance-control/StanceControlGate.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_TURN_CONTROL_GATE
#include "../../tests/turn-control/TurnControlGate.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_MAP_LEFT_CLICK_CONTROL_GATE
#include "../../tests/map-left-click-control/MapLeftClickControlGate.h"
#endif
// Fork modification 2026-10-11: default-off supervised stance pilot; GPL-3.0-or-later.
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
// Fork modification 2026-10-10: default-off bounded observer-effects and natural-FOV test hooks.
// See LICENSE-NOTICES.md; existing upstream license retained.
#ifdef OPENXCOM_EXPERIMENTAL_OBSERVER_EFFECTS_GATE
#include "../../tests/battle-observer/ObserverEffectsGate.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE
#include "../../tests/battle-observer/NaturalFovGate.h"
#endif
// Fork modification 2026-10-10: opt-in read-only public observation hooks;
// see LICENSE-NOTICES.md. Existing upstream copyright and license retained.
#ifdef OPENXCOM_EXPERIMENTAL_HUD_OBSERVER
#include "../Observer/HudFileObserver.h"
#include <cstdlib>
#endif
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <SDL_gfxPrimitives.h>
#include "Map.h"
#include "Camera.h"
#include "BattlescapeState.h"
#include "AbortMissionState.h"
#include "TileEngine.h"
#include "ActionMenuState.h"
#include "UnitInfoState.h"
#include "InventoryState.h"
#include "Pathfinding.h"
#include "BattlescapeGame.h"
#include "WarningMessage.h"
#include "DebriefingState.h"
#include "MiniMapState.h"
#include "BattlescapeGenerator.h"
#include "BriefingState.h"
#include "../lodepng.h"
#include "../fmath.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Palette.h"
#include "../Engine/Surface.h"
#include "../Engine/SurfaceSet.h"
#include "../Engine/Screen.h"
#include "../Engine/Sound.h"
#include "../Engine/Action.h"
#include "../Engine/Logger.h"
#include "../Engine/Timer.h"
#include "../Engine/CrossPlatform.h"
#include "../Interface/Cursor.h"
#include "../Interface/Text.h"
#include "../Interface/Bar.h"
#include "../Interface/BattlescapeButton.h"
#include "../Interface/NumberText.h"
#include "../Menu/CutsceneState.h"
#include "../Menu/PauseState.h"
#include "../Menu/LoadGameState.h"
#include "../Menu/SaveGameState.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleItem.h"
#include "../Mod/AlienDeployment.h"
#include "../Mod/Armor.h"
#include "../Mod/RuleUfo.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/Tile.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/BattleItem.h"
#include "../Savegame/Ufo.h"
#include "../Mod/RuleInterface.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Battlescape screen.
 * @param game Pointer to the core game.
 */
BattlescapeState::BattlescapeState() : _reserve(0), _firstInit(true), _isMouseScrolling(false), _isMouseScrolled(false), _xBeforeMouseScrolling(0), _yBeforeMouseScrolling(0), _totalMouseMoveX(0), _totalMouseMoveY(0), _mouseMovedOverThreshold(0), _mouseOverIcons(false), _autosave(false)
{
#ifdef OPENXCOM_EXPERIMENTAL_HUD_OBSERVER
	_hudObserver = 0;
#endif
	std::fill_n(_visibleUnit, 10, (BattleUnit*)(0));

	const int screenWidth = Options::baseXResolution;
	const int screenHeight = Options::baseYResolution;
	const int iconsWidth = _game->getMod()->getInterface("battlescape")->getElement("icons")->w;
	const int iconsHeight = _game->getMod()->getInterface("battlescape")->getElement("icons")->h;
	const int visibleMapHeight = screenHeight - iconsHeight;
	const int x = screenWidth/2 - iconsWidth/2;
	const int y = screenHeight - iconsHeight;

	// Create buttonbar - this should be on the centerbottom of the screen
	_icons = new InteractiveSurface(iconsWidth, iconsHeight, x, y);

	// Create the battlemap view
	// the actual map height is the total height minus the height of the buttonbar
	_map = new Map(_game, screenWidth, screenHeight, 0, 0, visibleMapHeight);

	_numLayers = new NumberText(3, 5, x + 232, y + 6);
	_rank = new Surface(26, 23, x + 107, y + 33);

	// Create buttons
	_btnUnitUp = new BattlescapeButton(32, 16, x + 48, y);
	_btnUnitDown = new BattlescapeButton(32, 16, x + 48, y + 16);
	_btnMapUp = new BattlescapeButton(32, 16, x + 80, y);
	_btnMapDown = new BattlescapeButton(32, 16, x + 80, y + 16);
	_btnShowMap = new BattlescapeButton(32, 16, x + 112, y);
	_btnKneel = new BattlescapeButton(32, 16, x + 112, y + 16);
	_btnInventory = new BattlescapeButton(32, 16, x + 144, y);
	_btnCenter = new BattlescapeButton(32, 16, x + 144, y + 16);
	_btnNextSoldier = new BattlescapeButton(32, 16, x + 176, y);
	_btnNextStop = new BattlescapeButton(32, 16, x + 176, y + 16);
	_btnShowLayers = new BattlescapeButton(32, 16, x + 208, y);
	_btnHelp = new BattlescapeButton(32, 16, x + 208, y + 16);
	_btnEndTurn = new BattlescapeButton(32, 16, x + 240, y);
	_btnAbort = new BattlescapeButton(32, 16, x + 240, y + 16);
	_btnStats = new InteractiveSurface(164, 23, x + 107, y + 33);
	_btnReserveNone = new BattlescapeButton(17, 11, x + 60, y + 33);
	_btnReserveSnap = new BattlescapeButton(17, 11, x + 78, y + 33);
	_btnReserveAimed = new BattlescapeButton(17, 11, x + 60, y + 45);
	_btnReserveAuto = new BattlescapeButton(17, 11, x + 78, y + 45);
	_btnReserveKneel = new BattlescapeButton(10, 23, x + 96, y + 33);
	_btnZeroTUs = new BattlescapeButton(10, 23, x + 49, y + 33);
	_btnLeftHandItem = new InteractiveSurface(32, 48, x + 8, y + 4);
	_numAmmoLeft = new NumberText(30, 5, x + 8, y + 4);
	_btnRightHandItem = new InteractiveSurface(32, 48, x + 280, y + 4);
	_numAmmoRight = new NumberText(30, 5, x + 280, y + 4);
	const int visibleUnitX = _game->getMod()->getInterface("battlescape")->getElement("visibleUnits")->x;
	const int visibleUnitY = _game->getMod()->getInterface("battlescape")->getElement("visibleUnits")->y;
	for (int i = 0; i < VISIBLE_MAX; ++i)
	{
		_btnVisibleUnit[i] = new InteractiveSurface(15, 12, x + visibleUnitX, y + visibleUnitY - (i * 13));
		_numVisibleUnit[i] = new NumberText(15, 12, _btnVisibleUnit[i]->getX() + 6 , _btnVisibleUnit[i]->getY() + 4);
	}
	_numVisibleUnit[9]->setX(_numVisibleUnit[9]->getX() - 2); // center number 10
	_warning = new WarningMessage(224, 24, x + 48, y + 32);
	_btnLaunch = new BattlescapeButton(32, 24, screenWidth - 32, 0); // we need screenWidth, because that is independent of the black bars on the screen
	_btnLaunch->setVisible(false);
	_btnPsi = new BattlescapeButton(32, 24, screenWidth - 32, 25); // we need screenWidth, because that is independent of the black bars on the screen
	_btnPsi->setVisible(false);

	// Create soldier stats summary
	_txtName = new Text(136, 10, x + 135, y + 32);

	_numTimeUnits = new NumberText(15, 5, x + 136, y + 42);
	_barTimeUnits = new Bar(102, 3, x + 170, y + 41);

	_numEnergy = new NumberText(15, 5, x + 154, y + 42);
	_barEnergy = new Bar(102, 3, x + 170, y + 45);

	_numHealth = new NumberText(15, 5, x + 136, y + 50);
	_barHealth= new Bar(102, 3, x + 170, y + 49);

	_numMorale = new NumberText(15, 5, x + 154, y + 50);
	_barMorale = new Bar(102, 3, x + 170, y + 53);

	_txtDebug = new Text(300, 10, 20, 0);
	_txtTooltip = new Text(300, 10, x + 2, y - 10);

	// Set palette
	_game->getSavedGame()->getSavedBattle()->setPaletteByDepth(this);

	if (_game->getMod()->getInterface("battlescape")->getElement("pathfinding"))
	{
		Element *pathing = _game->getMod()->getInterface("battlescape")->getElement("pathfinding");

		Pathfinding::green = pathing->color;
		Pathfinding::yellow = pathing->color2;
		Pathfinding::red = pathing->border;
	}

	add(_map);
	add(_icons);

	// Add in custom reserve buttons
	Surface *icons = _game->getMod()->getSurface("ICONS.PCK");
	if (_game->getMod()->getSurface("TFTDReserve", false))
	{
		Surface *tftdIcons = _game->getMod()->getSurface("TFTDReserve");
		tftdIcons->setX(48);
		tftdIcons->setY(176);
		tftdIcons->blit(icons);
	}

	// there is some cropping going on here, because the icons image is 320x200 while we only need the bottom of it.
	SDL_Rect *r = icons->getCrop();
	r->x = 0;
	r->y = 200 - iconsHeight;
	r->w = iconsWidth;
	r->h = iconsHeight;
	// we need to blit the icons before we add the battlescape buttons, as they copy the underlying parent surface.
	icons->blit(_icons);

	// this is a hack to fix the single transparent pixel on TFTD's icon panel.
	if (_game->getMod()->getInterface("battlescape")->getElement("icons")->TFTDMode)
	{
		_icons->setPixel(46, 44, 8);
	}

	add(_rank, "rank", "battlescape", _icons);
	add(_btnUnitUp, "buttonUnitUp", "battlescape", _icons);
	add(_btnUnitDown, "buttonUnitDown", "battlescape", _icons);
	add(_btnMapUp, "buttonMapUp", "battlescape", _icons);
	add(_btnMapDown, "buttonMapDown", "battlescape", _icons);
	add(_btnShowMap, "buttonShowMap", "battlescape", _icons);
	add(_btnKneel, "buttonKneel", "battlescape", _icons);
	add(_btnInventory, "buttonInventory", "battlescape", _icons);
	add(_btnCenter, "buttonCenter", "battlescape", _icons);
	add(_btnNextSoldier, "buttonNextSoldier", "battlescape", _icons);
	add(_btnNextStop, "buttonNextStop", "battlescape", _icons);
	add(_btnShowLayers, "buttonShowLayers", "battlescape", _icons);
	add(_numLayers, "numLayers", "battlescape", _icons);
	add(_btnHelp, "buttonHelp", "battlescape", _icons);
	add(_btnEndTurn, "buttonEndTurn", "battlescape", _icons);
	add(_btnAbort, "buttonAbort", "battlescape", _icons);
	add(_btnStats, "buttonStats", "battlescape", _icons);
	add(_txtName, "textName", "battlescape", _icons);
	add(_numTimeUnits, "numTUs", "battlescape", _icons);
	add(_numEnergy, "numEnergy", "battlescape", _icons);
	add(_numHealth, "numHealth", "battlescape", _icons);
	add(_numMorale, "numMorale", "battlescape", _icons);
	add(_barTimeUnits, "barTUs", "battlescape", _icons);
	add(_barEnergy, "barEnergy", "battlescape", _icons);
	add(_barHealth, "barHealth", "battlescape", _icons);
	add(_barMorale, "barMorale", "battlescape", _icons);
	add(_btnReserveNone, "buttonReserveNone", "battlescape", _icons);
	add(_btnReserveSnap, "buttonReserveSnap", "battlescape", _icons);
	add(_btnReserveAimed, "buttonReserveAimed", "battlescape", _icons);
	add(_btnReserveAuto, "buttonReserveAuto", "battlescape", _icons);
	add(_btnReserveKneel, "buttonReserveKneel", "battlescape", _icons);
	add(_btnZeroTUs, "buttonZeroTUs", "battlescape", _icons);
	add(_btnLeftHandItem, "buttonLeftHand", "battlescape", _icons);
	add(_numAmmoLeft, "numAmmoLeft", "battlescape", _icons);
	add(_btnRightHandItem, "buttonRightHand", "battlescape", _icons);
	add(_numAmmoRight, "numAmmoRight", "battlescape", _icons);
	for (int i = 0; i < VISIBLE_MAX; ++i)
	{
		add(_btnVisibleUnit[i]);
		add(_numVisibleUnit[i]);
	}
	add(_warning, "warning", "battlescape", _icons);
	add(_txtDebug);
	add(_txtTooltip, "textTooltip", "battlescape", _icons);
	add(_btnLaunch);
	_game->getMod()->getSurfaceSet("SPICONS.DAT")->getFrame(0)->blit(_btnLaunch);
	add(_btnPsi);
	_game->getMod()->getSurfaceSet("SPICONS.DAT")->getFrame(1)->blit(_btnPsi);

	// Set up objects
	_save = _game->getSavedGame()->getSavedBattle();
	_map->init();
	_map->onMouseOver((ActionHandler)&BattlescapeState::mapOver);
	_map->onMousePress((ActionHandler)&BattlescapeState::mapPress);
	_map->onMouseClick((ActionHandler)&BattlescapeState::mapClick, 0);
	_map->onMouseIn((ActionHandler)&BattlescapeState::mapIn);

	_numLayers->setColor(Palette::blockOffset(1)-2);
	_numLayers->setValue(1);

	_numAmmoLeft->setValue(999);

	_numAmmoRight->setValue(999);

	_icons->onMouseIn((ActionHandler)&BattlescapeState::mouseInIcons);
	_icons->onMouseOut((ActionHandler)&BattlescapeState::mouseOutIcons);

	_btnUnitUp->onMouseClick((ActionHandler)&BattlescapeState::btnUnitUpClick);
	_btnUnitUp->setTooltip("STR_UNIT_LEVEL_ABOVE");
	_btnUnitUp->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnUnitUp->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnUnitDown->onMouseClick((ActionHandler)&BattlescapeState::btnUnitDownClick);
	_btnUnitDown->setTooltip("STR_UNIT_LEVEL_BELOW");
	_btnUnitDown->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnUnitDown->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnMapUp->onMouseClick((ActionHandler)&BattlescapeState::btnMapUpClick);
	_btnMapUp->onKeyboardPress((ActionHandler)&BattlescapeState::btnMapUpClick, Options::keyBattleLevelUp);
	_btnMapUp->setTooltip("STR_VIEW_LEVEL_ABOVE");
	_btnMapUp->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnMapUp->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnMapDown->onMouseClick((ActionHandler)&BattlescapeState::btnMapDownClick);
	_btnMapDown->onKeyboardPress((ActionHandler)&BattlescapeState::btnMapDownClick, Options::keyBattleLevelDown);
	_btnMapDown->setTooltip("STR_VIEW_LEVEL_BELOW");
	_btnMapDown->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnMapDown->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnShowMap->onMouseClick((ActionHandler)&BattlescapeState::btnShowMapClick);
	_btnShowMap->onKeyboardPress((ActionHandler)&BattlescapeState::btnShowMapClick, Options::keyBattleMap);
	_btnShowMap->setTooltip("STR_MINIMAP");
	_btnShowMap->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnShowMap->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnKneel->onMouseClick((ActionHandler)&BattlescapeState::btnKneelClick);
	_btnKneel->onKeyboardPress((ActionHandler)&BattlescapeState::btnKneelClick, Options::keyBattleKneel);
	_btnKneel->setTooltip("STR_KNEEL");
	_btnKneel->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnKneel->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);
	_btnKneel->allowToggleInversion();

	_btnInventory->onMouseClick((ActionHandler)&BattlescapeState::btnInventoryClick);
	_btnInventory->onKeyboardPress((ActionHandler)&BattlescapeState::btnInventoryClick, Options::keyBattleInventory);
	_btnInventory->setTooltip("STR_INVENTORY");
	_btnInventory->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnInventory->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnCenter->onMouseClick((ActionHandler)&BattlescapeState::btnCenterClick);
	_btnCenter->onKeyboardPress((ActionHandler)&BattlescapeState::btnCenterClick, Options::keyBattleCenterUnit);
	_btnCenter->setTooltip("STR_CENTER_SELECTED_UNIT");
	_btnCenter->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnCenter->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnNextSoldier->onMouseClick((ActionHandler)&BattlescapeState::btnNextSoldierClick);
	_btnNextSoldier->onKeyboardPress((ActionHandler)&BattlescapeState::btnNextSoldierClick, Options::keyBattleNextUnit);
	_btnNextSoldier->onKeyboardPress((ActionHandler)&BattlescapeState::btnPrevSoldierClick, Options::keyBattlePrevUnit);
	_btnNextSoldier->setTooltip("STR_NEXT_UNIT");
	_btnNextSoldier->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnNextSoldier->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnNextStop->onMouseClick((ActionHandler)&BattlescapeState::btnNextStopClick);
	_btnNextStop->onKeyboardPress((ActionHandler)&BattlescapeState::btnNextStopClick, Options::keyBattleDeselectUnit);
	_btnNextStop->setTooltip("STR_DESELECT_UNIT");
	_btnNextStop->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnNextStop->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnShowLayers->onMouseClick((ActionHandler)&BattlescapeState::btnShowLayersClick);
	_btnShowLayers->setTooltip("STR_MULTI_LEVEL_VIEW");
	_btnShowLayers->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnShowLayers->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnHelp->onMouseClick((ActionHandler)&BattlescapeState::btnHelpClick);
	_btnHelp->onKeyboardPress((ActionHandler)&BattlescapeState::btnHelpClick, Options::keyBattleOptions);
	_btnHelp->setTooltip("STR_OPTIONS");
	_btnHelp->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnHelp->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnEndTurn->onMouseClick((ActionHandler)&BattlescapeState::btnEndTurnClick);
	_btnEndTurn->onKeyboardPress((ActionHandler)&BattlescapeState::btnEndTurnClick, Options::keyBattleEndTurn);
	_btnEndTurn->setTooltip("STR_END_TURN");
	_btnEndTurn->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnEndTurn->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnAbort->onMouseClick((ActionHandler)&BattlescapeState::btnAbortClick);
	_btnAbort->onKeyboardPress((ActionHandler)&BattlescapeState::btnAbortClick, Options::keyBattleAbort);
	_btnAbort->setTooltip("STR_ABORT_MISSION");
	_btnAbort->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnAbort->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnStats->onMouseClick((ActionHandler)&BattlescapeState::btnStatsClick);
	_btnStats->onKeyboardPress((ActionHandler)&BattlescapeState::btnStatsClick, Options::keyBattleStats);
	_btnStats->setTooltip("STR_UNIT_STATS");
	_btnStats->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnStats->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnLeftHandItem->onMouseClick((ActionHandler)&BattlescapeState::btnLeftHandItemClick);
	_btnLeftHandItem->onKeyboardPress((ActionHandler)&BattlescapeState::btnLeftHandItemClick, Options::keyBattleUseLeftHand);
	_btnLeftHandItem->setTooltip("STR_USE_LEFT_HAND");
	_btnLeftHandItem->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnLeftHandItem->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnRightHandItem->onMouseClick((ActionHandler)&BattlescapeState::btnRightHandItemClick);
	_btnRightHandItem->onKeyboardPress((ActionHandler)&BattlescapeState::btnRightHandItemClick, Options::keyBattleUseRightHand);
	_btnRightHandItem->setTooltip("STR_USE_RIGHT_HAND");
	_btnRightHandItem->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnRightHandItem->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnReserveNone->onMouseClick((ActionHandler)&BattlescapeState::btnReserveClick);
	_btnReserveNone->onKeyboardPress((ActionHandler)&BattlescapeState::btnReserveClick, Options::keyBattleReserveNone);
	_btnReserveNone->setTooltip("STR_DONT_RESERVE_TIME_UNITS");
	_btnReserveNone->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnReserveNone->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnReserveSnap->onMouseClick((ActionHandler)&BattlescapeState::btnReserveClick);
	_btnReserveSnap->onKeyboardPress((ActionHandler)&BattlescapeState::btnReserveClick, Options::keyBattleReserveSnap);
	_btnReserveSnap->setTooltip("STR_RESERVE_TIME_UNITS_FOR_SNAP_SHOT");
	_btnReserveSnap->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnReserveSnap->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnReserveAimed->onMouseClick((ActionHandler)&BattlescapeState::btnReserveClick);
	_btnReserveAimed->onKeyboardPress((ActionHandler)&BattlescapeState::btnReserveClick, Options::keyBattleReserveAimed);
	_btnReserveAimed->setTooltip("STR_RESERVE_TIME_UNITS_FOR_AIMED_SHOT");
	_btnReserveAimed->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnReserveAimed->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnReserveAuto->onMouseClick((ActionHandler)&BattlescapeState::btnReserveClick);
	_btnReserveAuto->onKeyboardPress((ActionHandler)&BattlescapeState::btnReserveClick, Options::keyBattleReserveAuto);
	_btnReserveAuto->setTooltip("STR_RESERVE_TIME_UNITS_FOR_AUTO_SHOT");
	_btnReserveAuto->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnReserveAuto->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);

	_btnReserveKneel->onMouseClick((ActionHandler)&BattlescapeState::btnReserveKneelClick);
	_btnReserveKneel->onKeyboardPress((ActionHandler)&BattlescapeState::btnReserveKneelClick, Options::keyBattleReserveKneel);
	_btnReserveKneel->setTooltip("STR_RESERVE_TIME_UNITS_FOR_KNEEL");
	_btnReserveKneel->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnReserveKneel->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);
	_btnReserveKneel->allowToggleInversion();

	_btnZeroTUs->onMouseClick((ActionHandler)&BattlescapeState::btnZeroTUsClick, SDL_BUTTON_RIGHT);
	_btnZeroTUs->onKeyboardPress((ActionHandler)&BattlescapeState::btnZeroTUsClick, Options::keyBattleZeroTUs);
	_btnZeroTUs->setTooltip("STR_EXPEND_ALL_TIME_UNITS");
	_btnZeroTUs->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
	_btnZeroTUs->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);
	_btnZeroTUs->allowClickInversion();

	// shortcuts without a specific button
	_btnStats->onKeyboardPress((ActionHandler)&BattlescapeState::btnReloadClick, Options::keyBattleReload);
	_btnStats->onKeyboardPress((ActionHandler)&BattlescapeState::btnPersonalLightingClick, Options::keyBattlePersonalLighting);

	SDLKey buttons[] = {Options::keyBattleCenterEnemy1,
						Options::keyBattleCenterEnemy2,
						Options::keyBattleCenterEnemy3,
						Options::keyBattleCenterEnemy4,
						Options::keyBattleCenterEnemy5,
						Options::keyBattleCenterEnemy6,
						Options::keyBattleCenterEnemy7,
						Options::keyBattleCenterEnemy8,
						Options::keyBattleCenterEnemy9,
						Options::keyBattleCenterEnemy10};
	Uint8 color = _game->getMod()->getInterface("battlescape")->getElement("visibleUnits")->color;
	for (int i = 0; i < VISIBLE_MAX; ++i)
	{
		std::ostringstream tooltip;
		_btnVisibleUnit[i]->onMouseClick((ActionHandler)&BattlescapeState::btnVisibleUnitClick);
		_btnVisibleUnit[i]->onKeyboardPress((ActionHandler)&BattlescapeState::btnVisibleUnitClick, buttons[i]);
		tooltip << "STR_CENTER_ON_ENEMY_" << (i+1);
		_btnVisibleUnit[i]->setTooltip(tooltip.str());
		_btnVisibleUnit[i]->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
		_btnVisibleUnit[i]->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);
		_numVisibleUnit[i]->setColor(color);
		_numVisibleUnit[i]->setValue(i+1);
	}
	_warning->setColor(_game->getMod()->getInterface("battlescape")->getElement("warning")->color2);
	_warning->setTextColor(_game->getMod()->getInterface("battlescape")->getElement("warning")->color);
	_btnLaunch->onMouseClick((ActionHandler)&BattlescapeState::btnLaunchClick);
	_btnPsi->onMouseClick((ActionHandler)&BattlescapeState::btnPsiClick);

	_txtName->setHighContrast(true);

	_barTimeUnits->setScale(1.0);
	_barEnergy->setScale(1.0);
	_barHealth->setScale(1.0);
	_barMorale->setScale(1.0);

	_txtDebug->setColor(Palette::blockOffset(8));
	_txtDebug->setHighContrast(true);

	_txtTooltip->setHighContrast(true);

	_btnReserveNone->setGroup(&_reserve);
	_btnReserveSnap->setGroup(&_reserve);
	_btnReserveAimed->setGroup(&_reserve);
	_btnReserveAuto->setGroup(&_reserve);

	// Set music
	if (_save->getMusic().empty())
	{
		_game->getMod()->playMusic("GMTACTIC");
	}
	else
	{
		_game->getMod()->playMusic(_save->getMusic());
	}

	_animTimer = new Timer(DEFAULT_ANIM_SPEED, true);
	_animTimer->onTimer((StateHandler)&BattlescapeState::animate);

	_gameTimer = new Timer(DEFAULT_ANIM_SPEED, true);
	_gameTimer->onTimer((StateHandler)&BattlescapeState::handleState);

	_battleGame = new BattlescapeGame(_save, this);

	_barHealthColor = _barHealth->getColor();
}


/**
 * Deletes the battlescapestate.
 */
BattlescapeState::~BattlescapeState()
{
#ifdef OPENXCOM_EXPERIMENTAL_HUD_OBSERVER
	delete _hudObserver;
#endif
	delete _animTimer;
	delete _gameTimer;
	delete _battleGame;
}

/**
 * Initializes the battlescapestate.
 */
void BattlescapeState::init()
{
	if (_save->getAmbientSound() != -1)
	{
		_game->getMod()->getSoundByDepth(_save->getDepth(), _save->getAmbientSound())->loop();
		_game->setVolume(Options::soundVolume, Options::musicVolume, Options::uiVolume);
	}

	State::init();
	_animTimer->start();
	_gameTimer->start();
	_map->setFocus(true);
	_map->cacheUnits();
	_map->draw();
	_battleGame->init();
	updateSoldierInfo();

	switch (_save->getTUReserved())
	{
	case BA_SNAPSHOT:
		_reserve = _btnReserveSnap;
		break;
	case BA_AIMEDSHOT:
		_reserve = _btnReserveAimed;
		break;
	case BA_AUTOSHOT:
		_reserve = _btnReserveAuto;
		break;
	default:
		_reserve = _btnReserveNone;
		break;
	}
	if (_firstInit)
	{
		if (!playableUnitSelected())
		{
			selectNextPlayerUnit();
		}
		if (playableUnitSelected())
		{
			_battleGame->setupCursor();
			_map->getCamera()->centerOnPosition(_save->getSelectedUnit()->getPosition());
		}
		_firstInit = false;
		_btnReserveNone->setGroup(&_reserve);
		_btnReserveSnap->setGroup(&_reserve);
		_btnReserveAimed->setGroup(&_reserve);
		_btnReserveAuto->setGroup(&_reserve);
	}
	_txtTooltip->setText("");
	_btnReserveKneel->toggle(_save->getKneelReserved());
	_battleGame->setKneelReserved(_save->getKneelReserved());
	if (_autosave)
	{
		_autosave = false;
		if (_game->getSavedGame()->isIronman())
		{
			_game->pushState(new SaveGameState(OPT_BATTLESCAPE, SAVE_IRONMAN, _palette));
		}
		else if (Options::autosave)
		{
			_game->pushState(new SaveGameState(OPT_BATTLESCAPE, SAVE_AUTO_BATTLESCAPE, _palette));
		}
	}
#ifdef OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE
	NaturalFovGate::initialized(this);
#endif
}

/**
 * Runs the timers and handles popups.
 */
void BattlescapeState::think()
{
	static bool popped = false;

	if (_gameTimer->isRunning())
	{
		if (_popups.empty())
		{
			State::think();
			_battleGame->think();
			_animTimer->think(this, 0);
			_gameTimer->think(this, 0);
			if (popped)
			{
				_battleGame->handleNonTargetAction();
				popped = false;
			}
		}
		else
		{
			// Handle popups
			_game->pushState(*_popups.begin());
			_popups.erase(_popups.begin());
			popped = true;
			return;
		}
	}
#ifdef OPENXCOM_EXPERIMENTAL_OBSERVER_EFFECTS_GATE
	ObserverEffectsGate::thought(this, _popups.empty() && _gameTimer->isRunning());
#endif
#ifdef OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE
	NaturalFovGate::thought(this, _popups.empty() && _gameTimer->isRunning());
#endif
#ifdef OPENXCOM_EXPERIMENTAL_HUD_OBSERVER
	samplePublicHud();
#endif
}

#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
void BattlescapeState::blit()
{
    Screen *screen = _game->getScreen();
    if (!_game->isState(this) || !screen->wantsPresentedHud()) {
        State::blit();
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
        _game->getStanceController()->arm(this);
#endif
        return;
    }
    Observer::HudFrame frame;
    NumberText *widgets[4] = { _numTimeUnits, _numEnergy, _numHealth, _numMorale };
    struct DisarmDigits
    {
        NumberText **widgets;
        ~DisarmDigits() { for (int i = 0; i < 4; ++i) widgets[i]->armPresentedDigits(0); }
    } disarm = { widgets };
    for (int i = 0; i < 4; ++i) widgets[i]->armPresentedDigits(&frame[i]);
    State::blit();
    for (int i = 0; i < 4; ++i) widgets[i]->armPresentedDigits(0);
    screen->setPresentedHud(frame);
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    _game->getStanceController()->arm(this);
#endif
}
#endif


#ifdef OPENXCOM_EXPERIMENTAL_HUD_OBSERVER
/** Copy only values already present in the visible HUD, never hidden state. */
void BattlescapeState::samplePublicHud()
{
	// Retire the historical lock even when no new stable HUD can be sampled.
	if (_hudObserver && !_hudObserver->enabled()) return;
	if (!_game->isState(this) || !_gameTimer->isRunning()
		|| _firstInit || !_popups.empty() || _save->getSide() != FACTION_PLAYER
		|| _save->getDebugMode() || _battleGame->isBusy() || !_battleGame->getPanicHandled()
		|| _save->getUnitsFalling() || !_save->getFallingUnits()->empty()
		|| _map->getProjectile() || !_map->getExplosions()->empty())
		return; // Preserve the last immutable observation; no busy/hidden reason.
	if (!_hudObserver)
		_hudObserver = new Observer::HudFileObserver(std::getenv("OPENXCOM_EXPERIMENTAL_HUD_DIR"));
	if (!_hudObserver->enabled()) return;
	Observer::PublicHud hud;
	const BattleUnit *selected = _save->getSelectedUnit();
	if (selected && selected->getFaction() == FACTION_PLAYER && !selected->isOut()
		&& _numTimeUnits->getVisible() && _numEnergy->getVisible()
		&& _numHealth->getVisible() && _numMorale->getVisible())
	{
		hud.selected = true;
		hud.timeUnits = _numTimeUnits->getValue();
		hud.energy = _numEnergy->getValue();
		hud.health = _numHealth->getValue();
		hud.morale = _numMorale->getValue();
	}
	_hudObserver->observe(hud);
}
#endif

/**
 * Processes any mouse moving over the map.
 * @param action Pointer to an action.
 */
void BattlescapeState::mapOver(Action *action)
{
	if (_isMouseScrolling && action->getDetails()->type == SDL_MOUSEMOTION)
	{
		// The following is the workaround for a rare problem where sometimes
		// the mouse-release event is missed for any reason.
		// (checking: is the dragScroll-mouse-button still pressed?)
		// However if the SDL is also missed the release event, then it is to no avail :(
		if ((SDL_GetMouseState(0,0)&SDL_BUTTON(Options::battleDragScrollButton)) == 0)
		{ // so we missed again the mouse-release :(
			// Check if we have to revoke the scrolling, because it was too short in time, so it was a click
			if ((!_mouseMovedOverThreshold) && ((int)(SDL_GetTicks() - _mouseScrollingStartTime) <= (Options::dragScrollTimeTolerance)))
			{
				_map->getCamera()->setMapOffset(_mapOffsetBeforeMouseScrolling);
			}
			_isMouseScrolled = _isMouseScrolling = false;
			stopScrolling(action);
			return;
		}

		_isMouseScrolled = true;

		if (Options::touchEnabled == false)
		{
			// Set the mouse cursor back
			SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
			SDL_WarpMouse(_game->getScreen()->getWidth() / 2, _game->getScreen()->getHeight() / 2 - _map->getIconHeight() / 2);
			SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
		}

		// Check the threshold
		_totalMouseMoveX += action->getDetails()->motion.xrel;
		_totalMouseMoveY += action->getDetails()->motion.yrel;
		if (!_mouseMovedOverThreshold)
		{
			_mouseMovedOverThreshold = ((std::abs(_totalMouseMoveX) > Options::dragScrollPixelTolerance) || (std::abs(_totalMouseMoveY) > Options::dragScrollPixelTolerance));
		}

		// Scrolling
		if (Options::battleDragScrollInvert)
		{
			_map->getCamera()->setMapOffset(_mapOffsetBeforeMouseScrolling);
			int scrollX = -(int)((double)_totalMouseMoveX / action->getXScale());
			int scrollY = -(int)((double)_totalMouseMoveY / action->getYScale());
			Position delta2 = _map->getCamera()->getMapOffset();
			_map->getCamera()->scrollXY(scrollX, scrollY, true);
			delta2 = _map->getCamera()->getMapOffset() - delta2;

			// Keep the limits...
			if (scrollX != delta2.x || scrollY != delta2.y)
			{
				_totalMouseMoveX = -(int) (delta2.x * action->getXScale());
				_totalMouseMoveY = -(int) (delta2.y * action->getYScale());
			}

			if (Options::touchEnabled == false)
			{
				action->getDetails()->motion.x = _xBeforeMouseScrolling;
				action->getDetails()->motion.y = _yBeforeMouseScrolling;
			}
			_map->setCursorType(CT_NONE);
		}
		else
		{
			Position delta = _map->getCamera()->getMapOffset();
			_map->getCamera()->setMapOffset(_mapOffsetBeforeMouseScrolling);
			int scrollX = (int)((double)_totalMouseMoveX / action->getXScale());
			int scrollY = (int)((double)_totalMouseMoveY / action->getYScale());
			Position delta2 = _map->getCamera()->getMapOffset();
			_map->getCamera()->scrollXY(scrollX, scrollY, true);
			delta2 = _map->getCamera()->getMapOffset() - delta2;
			delta = _map->getCamera()->getMapOffset() - delta;

			// Keep the limits...
			if (scrollX != delta2.x || scrollY != delta2.y)
			{
				_totalMouseMoveX = (int) (delta2.x * action->getXScale());
				_totalMouseMoveY = (int) (delta2.y * action->getYScale());
			}

			int barWidth = _game->getScreen()->getCursorLeftBlackBand();
			int barHeight = _game->getScreen()->getCursorTopBlackBand();
			int cursorX = _cursorPosition.x + Round(delta.x * action->getXScale());
			int cursorY = _cursorPosition.y + Round(delta.y * action->getYScale());
			_cursorPosition.x = Clamp(cursorX, barWidth, _game->getScreen()->getWidth() - barWidth - (int)(Round(action->getXScale())));
			_cursorPosition.y = Clamp(cursorY, barHeight, _game->getScreen()->getHeight() - barHeight - (int)(Round(action->getYScale())));

			if (Options::touchEnabled == false)
			{
				action->getDetails()->motion.x = _cursorPosition.x;
				action->getDetails()->motion.y = _cursorPosition.y;
			}
		}

		// We don't want to look the mouse-cursor jumping :)
		_game->getCursor()->handle(action);
	}
}

/**
 * Processes any presses on the map.
 * @param action Pointer to an action.
 */
void BattlescapeState::mapPress(Action *action)
{
	// don't handle mouseclicks over the buttons (it overlaps with map surface)
	if (_mouseOverIcons) return;

	if (action->getDetails()->button.button == Options::battleDragScrollButton)
	{
		_isMouseScrolling = true;
		_isMouseScrolled = false;
		SDL_GetMouseState(&_xBeforeMouseScrolling, &_yBeforeMouseScrolling);
		_mapOffsetBeforeMouseScrolling = _map->getCamera()->getMapOffset();
		if (!Options::battleDragScrollInvert && _cursorPosition.z == 0)
		{
			_cursorPosition.x = action->getDetails()->motion.x;
			_cursorPosition.y = action->getDetails()->motion.y;
			// the Z is irrelevant to our mouse position, but we can use it as a boolean to check if the position is set or not
			_cursorPosition.z = 1;
		}
		_totalMouseMoveX = 0; _totalMouseMoveY = 0;
		_mouseMovedOverThreshold = false;
		_mouseScrollingStartTime = SDL_GetTicks();
	}
}

/**
 * Processes any clicks on the map to
 * command units.
 * @param action Pointer to an action.
 */
void BattlescapeState::mapClick(Action *action)
{
#ifdef OPENXCOM_EXPERIMENTAL_MAP_LEFT_CLICK_CONTROL_GATE
    if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
        MapLeftClickControlGate::mapClickEntered(this);
#endif
	// The following is the workaround for a rare problem where sometimes
	// the mouse-release event is missed for any reason.
	// However if the SDL is also missed the release event, then it is to no avail :(
	// (this part handles the release if it is missed and now an other button is used)
	if (_isMouseScrolling)
	{
		if (action->getDetails()->button.button != Options::battleDragScrollButton
		&& (SDL_GetMouseState(0,0)&SDL_BUTTON(Options::battleDragScrollButton)) == 0)
		{   // so we missed again the mouse-release :(
			// Check if we have to revoke the scrolling, because it was too short in time, so it was a click
			if ((!_mouseMovedOverThreshold) && ((int)(SDL_GetTicks() - _mouseScrollingStartTime) <= (Options::dragScrollTimeTolerance)))
			{
				_map->getCamera()->setMapOffset(_mapOffsetBeforeMouseScrolling);
			}
			_isMouseScrolled = _isMouseScrolling = false;
			stopScrolling(action);
		}
	}

	// DragScroll-Button release: release mouse-scroll-mode
	if (_isMouseScrolling)
	{
		// While scrolling, other buttons are ineffective
		if (action->getDetails()->button.button == Options::battleDragScrollButton)
		{
			_isMouseScrolling = false;
			stopScrolling(action);
		}
		else
		{
			return;
		}
		// Check if we have to revoke the scrolling, because it was too short in time, so it was a click
		if ((!_mouseMovedOverThreshold) && ((int)(SDL_GetTicks() - _mouseScrollingStartTime) <= (Options::dragScrollTimeTolerance)))
		{
			_isMouseScrolled = false;
			stopScrolling(action);
		}
		if (_isMouseScrolled) return;
	}

	const Uint8 button = action->getDetails()->button.button;
	// Physical routing only: preserve cancellation before the HUD hover test.
	if (cancelMapAction(button)) return;
	if (_mouseOverIcons) return;
	activateMapButton(button);
}

bool BattlescapeState::cancelMapAction(Uint8 button)
{
	// Ordinary right-click cancellation is shared by both input routes.
	return button == SDL_BUTTON_RIGHT && _battleGame->cancelCurrentAction();
}

void BattlescapeState::activateMapButton(Uint8 button)
{
	// don't accept leftclicks if there is no cursor or there is an action busy
	if (_map->getCursorType() == CT_NONE || _battleGame->isBusy())
    {
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
        if (_agentMapInputTrace) _agentMapInputTrace->emit("guarded");
#endif
        return;
    }

	Position pos;
	_map->getSelectorPosition(&pos);

	if (_save->getDebugMode())
	{
		std::ostringstream ss;
		ss << "Clicked " << pos;
		debug(ss.str());
	}

	if (_save->getTile(pos) != 0) // don't allow to click into void
	{
		if ((button == SDL_BUTTON_RIGHT || (button == SDL_BUTTON_LEFT && (SDL_GetModState() & KMOD_ALT) != 0)) && playableUnitSelected())
		{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
            if (_agentMapInputTrace) _agentMapInputTrace->emit("secondary",true,pos.x,pos.y,pos.z);
#endif
			_battleGame->secondaryAction(pos);
#ifdef OPENXCOM_EXPERIMENTAL_TURN_CONTROL_GATE
            TurnControlGate::mapClicked(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_MAP_LEFT_CLICK_CONTROL_GATE
            MapLeftClickControlGate::rightClicked(this);
#endif
		}
		else if (button == SDL_BUTTON_LEFT)
		{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
            if (_agentMapInputTrace) _agentMapInputTrace->emit("primary",true,pos.x,pos.y,pos.z);
#endif
			_battleGame->primaryAction(pos);
#ifdef OPENXCOM_EXPERIMENTAL_MAP_LEFT_CLICK_CONTROL_GATE
            MapLeftClickControlGate::mapClicked(this);
#endif
		}
	}
}

/**
 * Handles mouse entering the map surface.
 * @param action Pointer to an action.
 */
void BattlescapeState::mapIn(Action *)
{
	_isMouseScrolling = false;
	_map->setButtonsPressed(Options::battleDragScrollButton, false);
}

/**
 * Moves the selected unit up.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnUnitUpClick(Action *)
{
	if (playableUnitSelected() && _save->getPathfinding()->validateUpDown(_save->getSelectedUnit(), _save->getSelectedUnit()->getPosition(), Pathfinding::DIR_UP))
	{
		_battleGame->cancelAllActions();
		_battleGame->moveUpDown(_save->getSelectedUnit(), Pathfinding::DIR_UP);
	}
}

/**
 * Moves the selected unit down.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnUnitDownClick(Action *)
{
	if (playableUnitSelected() && _save->getPathfinding()->validateUpDown(_save->getSelectedUnit(), _save->getSelectedUnit()->getPosition(), Pathfinding::DIR_DOWN))
	{
		_battleGame->cancelAllActions();
		_battleGame->moveUpDown(_save->getSelectedUnit(), Pathfinding::DIR_DOWN);
	}
}

/**
 * Shows the next map layer.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnMapUpClick(Action *)
{
	if (_save->getSide() == FACTION_PLAYER || _save->getDebugMode())
		_map->getCamera()->up();
}

/**
 * Shows the previous map layer.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnMapDownClick(Action *)
{
	if (_save->getSide() == FACTION_PLAYER || _save->getDebugMode())
		_map->getCamera()->down();
}

/**
 * Shows the minimap.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnShowMapClick(Action *)
{
	//MiniMapState
	if (allowButtons())
		_game->pushState (new MiniMapState (_map->getCamera(), _save));
}

void BattlescapeState::toggleKneelButton(BattleUnit* unit)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    // The exact same value chooses the visibly rendered kneel/stand icon.
    _agentShownActor = unit;
    _agentShownKneeled = unit && unit->isKneeled();
    ++_agentUiRevision;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    _stanceShownActor = unit;
    _stanceShownKneeled = unit && unit->isKneeled();
#endif
	if (_btnKneel->isTFTDMode())
	{
		_btnKneel->toggle(unit && unit->isKneeled());
	}
	else
	{
		_game->getMod()->getSurfaceSet("KneelButton")->getFrame((unit && unit->isKneeled()) ? 1 : 0)->blit(_btnKneel);
	}
}

/**
 * Toggles the current unit's kneel/standup status.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnKneelClick(Action *)
{
	if (allowButtons())
	{
		BattleUnit *bu = _save->getSelectedUnit();
		if (bu)
		{
			_battleGame->kneel(bu);
			toggleKneelButton(bu);

			// update any path preview when unit kneels
			if (_battleGame->getPathfinding()->isPathPreviewed())
			{
				_battleGame->getPathfinding()->calculate(_battleGame->getCurrentAction()->actor, _battleGame->getCurrentAction()->target);
				_battleGame->getPathfinding()->removePreview();
				_battleGame->getPathfinding()->previewPath();
			}
		}
	}
#ifdef OPENXCOM_EXPERIMENTAL_STANCE_CONTROL_GATE
    StanceControlGate::kneelClicked(this);
#endif
}

/**
 * Goes to the soldier info screen.
 * Additionally resets TUs for current side in debug mode.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnInventoryClick(Action *)
{
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_OPEN_CONTROL_GATE
    InventoryOpenControlGate::inventoryOpenEntered(this);
#endif
	if (_save->getDebugMode())
	{
		for (std::vector<BattleUnit*>::iterator i = _save->getUnits()->begin(); i != _save->getUnits()->end(); ++i)
			if ((*i)->getFaction() == _save->getSide())
				(*i)->prepareNewTurn();
		updateSoldierInfo();
	}
	if (playableUnitSelected()
		&& (_save->getSelectedUnit()->hasInventory() || _save->getDebugMode()))
	{
		_battleGame->cancelAllActions();
		_game->pushState(new InventoryState(!_save->getDebugMode(), this));
	}
}

/**
 * Centers on the currently selected soldier.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnCenterClick(Action *)
{
	if (playableUnitSelected())
	{
		_map->getCamera()->centerOnPosition(_save->getSelectedUnit()->getPosition());
		_map->refreshSelectorPosition();
	}
}

/**
 * Selects the next soldier.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnNextSoldierClick(Action *)
{
	if (allowButtons())
	{
		selectNextPlayerUnit(true, false);
		_map->refreshSelectorPosition();
	}
#if defined(OPENXCOM_EXPERIMENTAL_STANCE_CONTROL_GATE) && defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_SELECTION)
    StanceControlGate::selectionClicked(this, true);
#endif
}

/**
 * Disables reselection of the current soldier and selects the next soldier.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnNextStopClick(Action *)
{
	if (allowButtons())
	{
		selectNextPlayerUnit(true, true);
		_map->refreshSelectorPosition();
	}
}

/**
 * Selects next soldier.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnPrevSoldierClick(Action *)
{
	if (allowButtons())
	{
		selectPreviousPlayerUnit(true);
		_map->refreshSelectorPosition();
	}
#if defined(OPENXCOM_EXPERIMENTAL_STANCE_CONTROL_GATE) && defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_SELECTION)
    StanceControlGate::selectionClicked(this, false);
#endif
}

/**
 * Selects the next soldier.
 * @param checkReselect When true, don't select a unit that has been previously flagged.
 * @param setReselect When true, flag the current unit first.
 * @param checkInventory When true, don't select a unit that has no inventory.
 */
void BattlescapeState::selectNextPlayerUnit(bool checkReselect, bool setReselect, bool checkInventory, bool checkFOV)
{
	if (allowButtons())
	{
		BattleUnit *unit = _save->selectNextPlayerUnit(checkReselect, setReselect, checkInventory);
		updateSoldierInfo(checkFOV);
		if (unit) _map->getCamera()->centerOnPosition(unit->getPosition());
		_battleGame->cancelAllActions();
		_battleGame->getCurrentAction()->actor = unit;
		_battleGame->setupCursor();
	}
}

/**
 * Selects the previous soldier.
 * @param checkReselect When true, don't select a unit that has been previously flagged.
 * @param setReselect When true, flag the current unit first.
 * @param checkInventory When true, don't select a unit that has no inventory.
 */
void BattlescapeState::selectPreviousPlayerUnit(bool checkReselect, bool setReselect, bool checkInventory)
{
	if (allowButtons())
	{
		BattleUnit *unit = _save->selectPreviousPlayerUnit(checkReselect, setReselect, checkInventory);
		updateSoldierInfo();
		if (unit) _map->getCamera()->centerOnPosition(unit->getPosition());
		_battleGame->cancelAllActions();
		_battleGame->getCurrentAction()->actor = unit;
		_battleGame->setupCursor();
	}
}

/**
 * Shows/hides all map layers.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnShowLayersClick(Action *)
{
	_numLayers->setValue(_map->getCamera()->toggleShowAllLayers());
}

/**
 * Shows options.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnHelpClick(Action *)
{
	if (allowButtons(true))
		_game->pushState(new PauseState(OPT_BATTLESCAPE));
}

/**
 * Requests the end of turn. This will add a 0 to the end of the state queue,
 * so all ongoing actions, like explosions are finished first before really switching turn.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnEndTurnClick(Action *)
{
	if (allowButtons())
	{
		_txtTooltip->setText("");
		_battleGame->requestEndTurn();
	}
}

/**
 * Aborts the game.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnAbortClick(Action *)
{
	if (allowButtons())
		_game->pushState(new AbortMissionState(_save, this));
}

/**
 * Shows the selected soldier's info.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnStatsClick(Action *action)
{
	if (playableUnitSelected())
	{
		bool scroll = false;
		if (SCROLL_TRIGGER == Options::battleEdgeScroll &&
			SDL_MOUSEBUTTONUP == action->getDetails()->type && SDL_BUTTON_LEFT == action->getDetails()->button.button)
		{
			int posX = action->getXMouse();
			int posY = action->getYMouse();
			if ((posX < (Camera::SCROLL_BORDER * action->getXScale()) && posX > 0)
				|| (posX > (_map->getWidth() - Camera::SCROLL_BORDER) * action->getXScale())
				|| (posY < (Camera::SCROLL_BORDER * action->getYScale()) && posY > 0)
				|| (posY > (_map->getHeight() - Camera::SCROLL_BORDER) * action->getYScale()))
				// To avoid handling this event as a click
				// on the stats button when the mouse is on the scroll-border
				scroll = true;
		}
		if (!scroll)
		{
			_battleGame->cancelAllActions();
			popup(new UnitInfoState(_save->getSelectedUnit(), this, false, false));
		}
	}
}

/**
 * Shows an action popup menu. When clicked, creates the action.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnLeftHandItemClick(Action *)
{
	if (playableUnitSelected())
	{
		// concession for touch devices:
		// click on the item to cancel action, and don't pop up a menu to select a new one
		// TODO: wrap this in an IFDEF ?
		if (_battleGame->getCurrentAction()->targeting)
		{
			_battleGame->cancelCurrentAction();
			return;
		}

		_battleGame->cancelCurrentAction();

		BattleUnit *unit = _save->getSelectedUnit();
		BattleItem *leftHandItem = getLeftHandItem(unit);

		if (leftHandItem != getSpecialMeleeWeapon(unit))
		{
			unit->setActiveHand("STR_LEFT_HAND");
		}

		_map->cacheUnits();
		_map->draw();
		handleItemClick(leftHandItem);
	}
}

/**
 * Shows an action popup menu. When clicked, create the action.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnRightHandItemClick(Action *)
{
	if (playableUnitSelected())
	{
		// concession for touch devices:
		// click on the item to cancel action, and don't pop up a menu to select a new one
		// TODO: wrap this in an IFDEF ?
		if (_battleGame->getCurrentAction()->targeting)
		{
			_battleGame->cancelCurrentAction();
			return;
		}

		_battleGame->cancelCurrentAction();

		BattleUnit *unit = _save->getSelectedUnit();
		BattleItem *rightHandItem = getRightHandItem(unit);

		if (rightHandItem != getSpecialMeleeWeapon(unit))
		{
			unit->setActiveHand("STR_RIGHT_HAND");
		}

		_map->cacheUnits();
		_map->draw();
		handleItemClick(rightHandItem);
	}
}

/**
 * Centers on the unit corresponding to this button.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnVisibleUnitClick(Action *action)
{
	int btnID = -1;

	// got to find out which button was pressed
	for (int i = 0; i < VISIBLE_MAX && btnID == -1; ++i)
	{
		if (action->getSender() == _btnVisibleUnit[i])
		{
			btnID = i;
		}
	}

	if (btnID != -1)
	{
		centerVisibleUnit(static_cast<std::size_t>(btnID));
	}

	action->getDetails()->type = SDL_NOEVENT; // consume the event
}

void BattlescapeState::centerVisibleUnit(std::size_t index)
{
    if (index < VISIBLE_MAX && _visibleUnit[index])
        _map->getCamera()->centerOnPosition(_visibleUnit[index]->getPosition());
}

/**
 * Launches the blaster bomb.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnLaunchClick(Action *action)
{
	_battleGame->launchAction();
	action->getDetails()->type = SDL_NOEVENT; // consume the event
}

/**
 * Uses psionics.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnPsiClick(Action *action)
{
	_battleGame->psiButtonAction();
	action->getDetails()->type = SDL_NOEVENT; // consume the event
}

/**
 * Reserves time units.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnReserveClick(Action *action)
{
	if (allowButtons())
	{
		SDL_Event ev;
		ev.type = SDL_MOUSEBUTTONDOWN;
		ev.button.button = SDL_BUTTON_LEFT;
		Action a = Action(&ev, 0.0, 0.0, 0, 0);
		action->getSender()->mousePress(&a, this);

		if (_reserve == _btnReserveNone)
			_battleGame->setTUReserved(BA_NONE);
		else if (_reserve == _btnReserveSnap)
			_battleGame->setTUReserved(BA_SNAPSHOT);
		else if (_reserve == _btnReserveAimed)
			_battleGame->setTUReserved(BA_AIMEDSHOT);
		else if (_reserve == _btnReserveAuto)
			_battleGame->setTUReserved(BA_AUTOSHOT);

		// update any path preview
		if (_battleGame->getPathfinding()->isPathPreviewed())
		{
			_battleGame->getPathfinding()->removePreview();
			_battleGame->getPathfinding()->previewPath();
		}
	}
}

/**
 * Reloads the weapon in hand.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnReloadClick(Action *)
{
	if (playableUnitSelected() && _save->getSelectedUnit()->checkAmmo())
	{
		_game->getMod()->getSoundByDepth(_save->getDepth(), Mod::ITEM_RELOAD)->play(-1, getMap()->getSoundAngle(_save->getSelectedUnit()->getPosition()));
		updateSoldierInfo();
	}
}

/**
 * Toggles soldier's personal lighting (purely cosmetic).
 * @param action Pointer to an action.
 */
void BattlescapeState::btnPersonalLightingClick(Action *)
{
	if (allowButtons())
		_save->getTileEngine()->togglePersonalLighting();
}

/**
 * Determines whether a playable unit is selected. Normally only player side units can be selected, but in debug mode one can play with aliens too :)
 * Is used to see if action buttons will work.
 * @return Whether a playable unit is selected.
 */
bool BattlescapeState::playableUnitSelected()
{
	return _save->getSelectedUnit() != 0 && allowButtons();
}

/**
 * Updates a soldier's name/rank/tu/energy/health/morale.
 */
void BattlescapeState::updateSoldierInfo(bool checkFOV)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    ++_agentUiRevision;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    ++_stanceUiRevision;
#endif
	BattleUnit *battleUnit = _save->getSelectedUnit();

	for (int i = 0; i < VISIBLE_MAX; ++i)
	{
		_btnVisibleUnit[i]->setVisible(false);
		_numVisibleUnit[i]->setVisible(false);
		_visibleUnit[i] = 0;
	}

	bool playableUnit = _battleGame->playableUnitSelected();
	_rank->setVisible(playableUnit);
	_numTimeUnits->setVisible(playableUnit);
	_barTimeUnits->setVisible(playableUnit);
	_barTimeUnits->setVisible(playableUnit);
	_numEnergy->setVisible(playableUnit);
	_barEnergy->setVisible(playableUnit);
	_barEnergy->setVisible(playableUnit);
	_numHealth->setVisible(playableUnit);
	_barHealth->setVisible(playableUnit);
	_barHealth->setVisible(playableUnit);
	_numMorale->setVisible(playableUnit);
	_barMorale->setVisible(playableUnit);
	_barMorale->setVisible(playableUnit);
	_btnLeftHandItem->setVisible(playableUnit);
	_btnRightHandItem->setVisible(playableUnit);
	_numAmmoLeft->setVisible(playableUnit);
	_numAmmoRight->setVisible(playableUnit);
	if (!playableUnit)
	{
		_txtName->setText("");
		showPsiButton(false);
		toggleKneelButton(0);
		return;
	}

	_txtName->setText(battleUnit->getName(_game->getLanguage(), false));
	Soldier *soldier = battleUnit->getGeoscapeSoldier();
	if (soldier != 0)
	{
		SurfaceSet *texture = _game->getMod()->getSurfaceSet("SMOKE.PCK");
		texture->getFrame(20 + soldier->getRank())->blit(_rank);
	}
	else
	{
		_rank->clear();
	}
	_numTimeUnits->setValue(battleUnit->getTimeUnits());
	_barTimeUnits->setMax(battleUnit->getBaseStats()->tu);
	_barTimeUnits->setValue(battleUnit->getTimeUnits());
	_numEnergy->setValue(battleUnit->getEnergy());
	_barEnergy->setMax(battleUnit->getBaseStats()->stamina);
	_barEnergy->setValue(battleUnit->getEnergy());
	_numHealth->setValue(battleUnit->getHealth());
	_barHealth->setMax(battleUnit->getBaseStats()->health);
	_barHealth->setValue(battleUnit->getHealth());
	_barHealth->setValue2(battleUnit->getStunlevel());
	_numMorale->setValue(battleUnit->getMorale());
	_barMorale->setMax(100);
	_barMorale->setValue(battleUnit->getMorale());

	toggleKneelButton(battleUnit);

	BattleItem *leftHandItem = getLeftHandItem(battleUnit);
	_btnLeftHandItem->clear();
	_numAmmoLeft->setVisible(false);
	if (leftHandItem)
	{
		leftHandItem->getRules()->drawHandSprite(_game->getMod()->getSurfaceSet("BIGOBS.PCK"), _btnLeftHandItem);
		if (leftHandItem->getRules()->getBattleType() == BT_FIREARM && (leftHandItem->needsAmmo() || leftHandItem->getRules()->getClipSize() > 0))
		{
			_numAmmoLeft->setVisible(true);
			if (leftHandItem->getAmmoItem())
				_numAmmoLeft->setValue(leftHandItem->getAmmoItem()->getAmmoQuantity());
			else
				_numAmmoLeft->setValue(0);
		}
	}
	BattleItem *rightHandItem = getRightHandItem(battleUnit);
	_btnRightHandItem->clear();
	_numAmmoRight->setVisible(false);
	if (rightHandItem)
	{
		rightHandItem->getRules()->drawHandSprite(_game->getMod()->getSurfaceSet("BIGOBS.PCK"), _btnRightHandItem);
		if (rightHandItem->getRules()->getBattleType() == BT_FIREARM && (rightHandItem->needsAmmo() || rightHandItem->getRules()->getClipSize() > 0))
		{
			_numAmmoRight->setVisible(true);
			if (rightHandItem->getAmmoItem())
				_numAmmoRight->setValue(rightHandItem->getAmmoItem()->getAmmoQuantity());
			else
				_numAmmoRight->setValue(0);
		}
	}

	if (checkFOV)
	{
		_save->getTileEngine()->calculateFOV(_save->getSelectedUnit());
	}
	int j = 0;
	for (std::vector<BattleUnit*>::iterator i = battleUnit->getVisibleUnits()->begin(); i != battleUnit->getVisibleUnits()->end() && j < VISIBLE_MAX; ++i)
	{
		_btnVisibleUnit[j]->setVisible(true);
		_numVisibleUnit[j]->setVisible(true);
		_visibleUnit[j] = (*i);
		++j;
	}

	showPsiButton(battleUnit->getSpecialWeapon(BT_PSIAMP) != 0);
}

/**
 * Gets the item currently accessible through the left hand slot in the battlescape UI.
 */
BattleItem *BattlescapeState::getLeftHandItem(BattleUnit *battleUnit)
{
	BattleItem *melee = getSpecialMeleeWeapon(battleUnit);
	BattleItem *leftHand = battleUnit->getItem("STR_LEFT_HAND");

	// If the unit has a melee weapon, and the right hand is already occupied,
	// allow access to the melee weapon through the left hand slot,
	// provided that the left hand is empty.
	return melee && battleUnit->getItem("STR_RIGHT_HAND") && !leftHand
		? melee
		: leftHand;
}

/**
 * Gets the item currently accessible through the right hand slot in the battlescape UI.
 */
BattleItem *BattlescapeState::getRightHandItem(BattleUnit *battleUnit)
{
	BattleItem *melee = getSpecialMeleeWeapon(battleUnit);
	BattleItem *rightHand = battleUnit->getItem("STR_RIGHT_HAND");

	// If the unit has a built-in melee weapon, and the right hand is not occupied,
	// allow access to the melee weapon through the right hand slot.
	return melee && !rightHand ? melee : rightHand;
}

/**
 * Gets the built-in melee weapon of a unit, if any.
 */
BattleItem *BattlescapeState::getSpecialMeleeWeapon(BattleUnit *battleUnit)
{
	return battleUnit->getSpecialWeapon(BT_MELEE);
}

/**
 * Shifts the red colors of the visible unit buttons backgrounds.
 */
void BattlescapeState::blinkVisibleUnitButtons()
{
	static int delta = 1, color = 32;

	for (int i = 0; i < VISIBLE_MAX;  ++i)
	{
		if (_btnVisibleUnit[i]->getVisible() == true)
		{
			_btnVisibleUnit[i]->drawRect(0, 0, 15, 12, 15);
			_btnVisibleUnit[i]->drawRect(1, 1, 13, 10, color);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
            _agentEnemyPainted[i] = true;
#endif
		}
	}

	if (color == 44) delta = -2;
	if (color == 32) delta = 1;

	color += delta;
}

/**
 * Shifts the colors of the health bar when unit has fatal wounds.
 */
void BattlescapeState::blinkHealthBar()
{
	static Uint8 color = 0, maxcolor = 3, step = 0;

	step = 1 - step;	// 1, 0, 1, 0, ...
	BattleUnit *bu = _save->getSelectedUnit();
	if (step == 0 || bu == 0 || !_barHealth->getVisible()) return;

	if (++color > maxcolor) color = maxcolor - 3;

	for (int i = 0; i < 6; i++)
	{
		if (bu->getFatalWound(i) > 0)
		{
			_barHealth->setColor(_barHealthColor + color);
			return;
		}
	}
	if (_barHealth->getColor() != _barHealthColor) // avoid redrawing if we don't have to
		_barHealth->setColor(_barHealthColor);
}

/**
 * Popups a context sensitive list of actions the user can choose from.
 * Some actions result in a change of gamestate.
 * @param item Item the user clicked on (righthand/lefthand)
 */
void BattlescapeState::handleItemClick(BattleItem *item)
{
	// make sure there is an item, and the battlescape is in an idle state
	if (item && !_battleGame->isBusy())
	{
		_battleGame->getCurrentAction()->weapon = item;
		popup(new ActionMenuState(_battleGame->getCurrentAction(), _icons->getX(), _icons->getY()+16));
	}
}

/**
 * Animates map objects on the map, also smoke,fire, ...
 */
void BattlescapeState::animate()
{
	_map->animate(!_battleGame->isBusy());

	blinkVisibleUnitButtons();
	blinkHealthBar();
#ifdef OPENXCOM_EXPERIMENTAL_OBSERVER_EFFECTS_GATE
	ObserverEffectsGate::animated(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE
	NaturalFovGate::animated(this);
#endif
}

/**
 * Handles the battle game state.
 */
void BattlescapeState::handleState()
{
	_battleGame->handleState();
#ifdef OPENXCOM_EXPERIMENTAL_OBSERVER_EFFECTS_GATE
	ObserverEffectsGate::handledState(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE
	NaturalFovGate::handledState(this);
#endif
}

/**
 * Sets the timer interval for think() calls of the state.
 * @param interval An interval in ms.
 */
void BattlescapeState::setStateInterval(Uint32 interval)
{
	_gameTimer->setInterval(interval);
}

/**
 * Gets pointer to the game. Some states need this info.
 * @return Pointer to game.
 */
Game *BattlescapeState::getGame() const
{
	return _game;
}

/**
 * Gets pointer to the map. Some states need this info.
 * @return Pointer to map.
 */
Map *BattlescapeState::getMap() const
{
	return _map;
}

/**
 * Shows a debug message in the topleft corner.
 * @param message Debug message.
 */
void BattlescapeState::debug(const std::string &message)
{
	if (_save->getDebugMode())
	{
		_txtDebug->setText(message);
	}
}

/**
 * Shows a warning message.
 * @param message Warning message.
 */
void BattlescapeState::warning(const std::string &message)
{
	_warning->showMessage(tr(message));
}

/**
 * Takes care of any events from the core game engine.
 * @param action Pointer to an action.
 */
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool BattlescapeState::agentCursorMotionSafe() const
{
    return !_firstInit && !_isMouseScrolling && !_isMouseScrolled && State::agentCursorMotionSafe();
}
#endif
inline void BattlescapeState::handle(Action *action)
{
	if (!_firstInit)
	{
		if (_game->getCursor()->getVisible() || ((action->getDetails()->type == SDL_MOUSEBUTTONDOWN || action->getDetails()->type == SDL_MOUSEBUTTONUP) && action->getDetails()->button.button == SDL_BUTTON_RIGHT))
		{
			State::handle(action);

			if (Options::touchEnabled == false && _isMouseScrolling && !Options::battleDragScrollInvert)
			{
				_map->setSelectorPosition((_cursorPosition.x - _game->getScreen()->getCursorLeftBlackBand()) / action->getXScale(), (_cursorPosition.y - _game->getScreen()->getCursorTopBlackBand()) / action->getYScale());
			}

			if (action->getDetails()->type == SDL_MOUSEBUTTONDOWN)
			{
				if (action->getDetails()->button.button == SDL_BUTTON_X1)
				{
					btnNextSoldierClick(action);
				}
				else if (action->getDetails()->button.button == SDL_BUTTON_X2)
				{
					btnPrevSoldierClick(action);
				}
			}

			if (action->getDetails()->type == SDL_KEYDOWN)
			{
				if (Options::debug)
				{
					// "ctrl-d" - enable debug mode
					if (action->getDetails()->key.keysym.sym == SDLK_d && (SDL_GetModState() & KMOD_CTRL) != 0)
					{
						_save->setDebugMode();
						debug("Debug Mode");
					}
					// "ctrl-v" - reset tile visibility
					else if (_save->getDebugMode() && action->getDetails()->key.keysym.sym == SDLK_v && (SDL_GetModState() & KMOD_CTRL) != 0)
					{
						debug("Resetting tile visibility");
						_save->resetTiles();
					}
					// "ctrl-k" - kill all aliens
					else if (_save->getDebugMode() && action->getDetails()->key.keysym.sym == SDLK_k && (SDL_GetModState() & KMOD_CTRL) != 0)
					{
						debug("Influenza bacterium dispersed");
						for (std::vector<BattleUnit*>::iterator i = _save->getUnits()->begin(); i !=_save->getUnits()->end(); ++i)
						{
							if ((*i)->getOriginalFaction() == FACTION_HOSTILE && !(*i)->isOut())
							{
								(*i)->damage(Position(0,0,0), 1000, DT_AP, true);
							}
							_save->getBattleGame()->checkForCasualties(0, 0, true, false);
							_save->getBattleGame()->handleState();
						}
					}
					// "ctrl-j" - stun all aliens
					else if (_save->getDebugMode() && action->getDetails()->key.keysym.sym == SDLK_j && (SDL_GetModState() & KMOD_CTRL) != 0)
					{
						debug("Deploying Celine Dion album");
						for (std::vector<BattleUnit*>::iterator i = _save->getUnits()->begin(); i !=_save->getUnits()->end(); ++i)
						{
							if ((*i)->getOriginalFaction() == FACTION_HOSTILE && !(*i)->isOut())
							{
								(*i)->damage(Position(0,0,0), 1000, DT_STUN, true);
							}
						}
						_save->getBattleGame()->checkForCasualties(0, 0, true, false);
						_save->getBattleGame()->handleState();
					}
					// "ctrl-w" - warp unit
					else if (_save->getDebugMode() && action->getDetails()->key.keysym.sym == SDLK_w && (SDL_GetModState() & KMOD_CTRL) != 0)
					{
						debug("Beam me up Scotty");
						BattleUnit *unit = _save->getSelectedUnit();
						Position newPos;
						_map->getSelectorPosition(&newPos);
						if (unit != 0 && newPos.x >= 0)
						{
							unit->getTile()->setUnit(0);
							unit->setPosition(newPos);
							_save->getTile(newPos)->setUnit(unit);
							_save->getTileEngine()->calculateUnitLighting();
							_save->getBattleGame()->handleState();
						}
					}
					// f11 - voxel map dump
					else if (action->getDetails()->key.keysym.sym == SDLK_F11)
					{
						saveVoxelMap();
					}
					// f9 - ai
					else if (action->getDetails()->key.keysym.sym == SDLK_F9 && Options::traceAI)
					{
						saveAIMap();
					}
				}
				// quick save and quick load
				if (!_game->getSavedGame()->isIronman())
				{
					if (action->getDetails()->key.keysym.sym == Options::keyQuickSave)
					{
						_game->pushState(new SaveGameState(OPT_BATTLESCAPE, SAVE_QUICK, _palette));
					}
					else if (action->getDetails()->key.keysym.sym == Options::keyQuickLoad)
					{
						_game->pushState(new LoadGameState(OPT_BATTLESCAPE, SAVE_QUICK, _palette));
					}
				}

				// voxel view dump
				if (action->getDetails()->key.keysym.sym == Options::keyBattleVoxelView)
				{
					saveVoxelView();
				}
			}
		}
	}
}

/**
 * Saves a map as used by the AI.
 */
void BattlescapeState::saveAIMap()
{
	Uint32 start = SDL_GetTicks();
	BattleUnit *unit = _save->getSelectedUnit();
	if (!unit) return;

	int w = _save->getMapSizeX();
	int h = _save->getMapSizeY();

	SDL_Surface *img = SDL_AllocSurface(0, w * 8, h * 8, 24, 0xff, 0xff00, 0xff0000, 0);
	Log(LOG_INFO) << "unit = " << unit->getId();
	memset(img->pixels, 0, img->pitch * img->h);

	Position tilePos(unit->getPosition());
	SDL_Rect r;
	r.h = 8;
	r.w = 8;

	for (int y = 0; y < h; ++y)
	{
		tilePos.y = y;
		for (int x = 0; x < w; ++x)
		{
			tilePos.x = x;
			Tile *t = _save->getTile(tilePos);

			if (!t) continue;
			if (!t->isDiscovered(2)) continue;

		}
	}

	for (int y = 0; y < h; ++y)
	{
		tilePos.y = y;
		for (int x = 0; x < w; ++x)
		{
			tilePos.x = x;
			Tile *t = _save->getTile(tilePos);

			if (!t) continue;
			if (!t->isDiscovered(2)) continue;

			r.x = x * r.w;
			r.y = y * r.h;

			if (t->getTUCost(O_FLOOR, MT_FLY) != 255 && t->getTUCost(O_OBJECT, MT_FLY) != 255)
			{
				SDL_FillRect(img, &r, SDL_MapRGB(img->format, 255, 0, 0x20));
				characterRGBA(img, r.x, r.y,'*' , 0x7f, 0x7f, 0x7f, 0x7f);
			} else
			{
				if (!t->getUnit()) SDL_FillRect(img, &r, SDL_MapRGB(img->format, 0x50, 0x50, 0x50)); // gray for blocked tile
			}

			for (int z = tilePos.z; z >= 0; --z)
			{
				Position pos(tilePos.x, tilePos.y, z);
				t = _save->getTile(pos);
				BattleUnit *wat = t->getUnit();
				if (wat)
				{
					switch(wat->getFaction())
					{
					case FACTION_HOSTILE:
						// #4080C0 is Volutar Blue
						characterRGBA(img, r.x, r.y, (tilePos.z - z) ? 'a' : 'A', 0x40, 0x80, 0xC0, 0xff);
						break;
					case FACTION_PLAYER:
						characterRGBA(img, r.x, r.y, (tilePos.z - z) ? 'x' : 'X', 255, 255, 127, 0xff);
						break;
					case FACTION_NEUTRAL:
						characterRGBA(img, r.x, r.y, (tilePos.z - z) ? 'c' : 'C', 255, 127, 127, 0xff);
						break;
					}
					break;
				}
				pos.z--;
				if (z > 0 && !t->hasNoFloor(_save->getTile(pos))) break; // no seeing through floors
			}

			if (t->getMapData(O_NORTHWALL) && t->getMapData(O_NORTHWALL)->getTUCost(MT_FLY) == 255)
			{
				lineRGBA(img, r.x, r.y, r.x+r.w, r.y, 0x50, 0x50, 0x50, 255);
			}

			if (t->getMapData(O_WESTWALL) && t->getMapData(O_WESTWALL)->getTUCost(MT_FLY) == 255)
			{
				lineRGBA(img, r.x, r.y, r.x, r.y+r.h, 0x50, 0x50, 0x50, 255);
			}
		}
	}

	std::ostringstream ss;

	ss.str("");
	ss << "z = " << tilePos.z;
	stringRGBA(img, 12, 12, ss.str().c_str(), 0, 0, 0, 0x7f);

	int i = 0;
	do
	{
		ss.str("");
		ss << Options::getMasterUserFolder() << "AIExposure" << std::setfill('0') << std::setw(3) << i << ".png";
		i++;
	}
	while (CrossPlatform::fileExists(ss.str()));


	unsigned error = lodepng::encode(ss.str(), (const unsigned char*)img->pixels, img->w, img->h, LCT_RGB);
	if (error)
	{
		Log(LOG_ERROR) << "Saving to PNG failed: " << lodepng_error_text(error);
	}

	SDL_FreeSurface(img);

	Log(LOG_INFO) << "saveAIMap() completed in " << SDL_GetTicks() - start << "ms.";
}

/**
 * Saves a first-person voxel view of the battlescape.
 */
void BattlescapeState::saveVoxelView()
{
	static const unsigned char pal[30]=
	//			ground		west wall	north wall		object		enem unit						xcom unit	neutr unit
	{0,0,0, 224,224,224,  192,224,255,  255,224,192, 128,255,128, 192,0,255,  0,0,0, 255,255,255,  224,192,0,  255,64,128 };

	BattleUnit * bu = _save->getSelectedUnit();
	if (bu==0) return; //no unit selected
	std::vector<Position> _trajectory;

	double ang_x,ang_y;
	bool black;
	Tile *tile = 0;
	std::ostringstream ss;
	std::vector<unsigned char> image;
	int test;
	Position originVoxel = getBattleGame()->getTileEngine()->getSightOriginVoxel(bu);

	Position targetVoxel,hitPos;
	double dist = 0;
	bool _debug = _save->getDebugMode();
	double dir = ((double)bu->getDirection()+4)/4*M_PI;
	image.clear();
	for (int y = -256+32; y < 256+32; ++y)
	{
		ang_y = (((double)y)/640*M_PI+M_PI/2);
		for (int x = -256; x < 256; ++x)
		{
			ang_x = ((double)x/1024)*M_PI+dir;

			targetVoxel.x=originVoxel.x + (int)(-sin(ang_x)*1024*sin(ang_y));
			targetVoxel.y=originVoxel.y + (int)(cos(ang_x)*1024*sin(ang_y));
			targetVoxel.z=originVoxel.z + (int)(cos(ang_y)*1024);

			_trajectory.clear();
			test = _save->getTileEngine()->calculateLine(originVoxel, targetVoxel, false, &_trajectory, bu, true, !_debug) +1;
			black = true;
			if (test!=0 && test!=6)
			{
				tile = _save->getTile(Position(_trajectory.at(0).x/16, _trajectory.at(0).y/16, _trajectory.at(0).z/24));
				if (_debug
					|| (tile->isDiscovered(0) && test == 2)
					|| (tile->isDiscovered(1) && test == 3)
					|| (tile->isDiscovered(2) && (test == 1 || test == 4))
					|| test==5
					)
				{
					if (test==5)
					{
						if (tile->getUnit())
						{
							if (tile->getUnit()->getFaction()==FACTION_NEUTRAL) test=9;
							else
							if (tile->getUnit()->getFaction()==FACTION_PLAYER) test=8;
						}
						else
						{
							tile = _save->getTile(Position(_trajectory.at(0).x/16, _trajectory.at(0).y/16, _trajectory.at(0).z/24-1));
							if (tile && tile->getUnit())
							{
								if (tile->getUnit()->getFaction()==FACTION_NEUTRAL) test=9;
								else
								if (tile->getUnit()->getFaction()==FACTION_PLAYER) test=8;
							}
						}
					}
					hitPos = Position(_trajectory.at(0).x, _trajectory.at(0).y, _trajectory.at(0).z);
					dist = sqrt((double)((hitPos.x-originVoxel.x)*(hitPos.x-originVoxel.x)
						+ (hitPos.y-originVoxel.y)*(hitPos.y-originVoxel.y)
						+ (hitPos.z-originVoxel.z)*(hitPos.z-originVoxel.z)) );
					black = false;
				}
			}

			if (black)
			{
				dist = 0;
			}
			else
			{
				if (dist>1000) dist=1000;
				if (dist<1) dist=1;
				dist=(1000-(log(dist))*140)/700;//140

				if (hitPos.x%16==15)
				{
					dist*=0.9;
				}
				if (hitPos.y%16==15)
				{
					dist*=0.9;
				}
				if (hitPos.z%24==23)
				{
					dist*=0.9;
				}
				if (dist > 1) dist = 1;
				if (tile) dist *= (16 - (double)tile->getShade())/16;
			}

			image.push_back((int)((double)(pal[test*3+0])*dist));
			image.push_back((int)((double)(pal[test*3+1])*dist));
			image.push_back((int)((double)(pal[test*3+2])*dist));
		}
	}

	int i = 0;
	do
	{
		ss.str("");
		ss << Options::getMasterUserFolder() << "fpslook" << std::setfill('0') << std::setw(3) << i << ".png";
		i++;
	}
	while (CrossPlatform::fileExists(ss.str()));


	unsigned error = lodepng::encode(ss.str(), image, 512, 512, LCT_RGB);
	if (error)
	{
		Log(LOG_ERROR) << "Saving to PNG failed: " << lodepng_error_text(error);
	}

	return;
}

/**
 * Saves each layer of voxels on the bettlescape as a png.
 */
void BattlescapeState::saveVoxelMap()
{
	std::ostringstream ss;
	std::vector<unsigned char> image;
	static const unsigned char pal[30]=
	{255,255,255, 224,224,224,  128,160,255,  255,160,128, 128,255,128, 192,0,255,  255,255,255, 255,255,255,  224,192,0,  255,64,128 };

	Tile *tile;

	for (int z = 0; z < _save->getMapSizeZ()*12; ++z)
	{
		image.clear();

		for (int y = 0; y < _save->getMapSizeY()*16; ++y)
		{
			for (int x = 0; x < _save->getMapSizeX()*16; ++x)
			{
				int test = _save->getTileEngine()->voxelCheck(Position(x,y,z*2),0,0) +1;
				float dist=1;
				if (x%16==15)
				{
					dist*=0.9f;
				}
				if (y%16==15)
				{
					dist*=0.9f;
				}

				if (test == V_OUTOFBOUNDS)
				{
					tile = _save->getTile(Position(x/16, y/16, z/12));
					if (tile->getUnit())
					{
						if (tile->getUnit()->getFaction()==FACTION_NEUTRAL) test=9;
						else
						if (tile->getUnit()->getFaction()==FACTION_PLAYER) test=8;
					}
					else
					{
						tile = _save->getTile(Position(x/16, y/16, z/12-1));
						if (tile && tile->getUnit())
						{
							if (tile->getUnit()->getFaction()==FACTION_NEUTRAL) test=9;
							else
							if (tile->getUnit()->getFaction()==FACTION_PLAYER) test=8;
						}
					}
				}

				image.push_back((int)((float)pal[test*3+0]*dist));
				image.push_back((int)((float)pal[test*3+1]*dist));
				image.push_back((int)((float)pal[test*3+2]*dist));
			}
		}

		ss.str("");
		ss << Options::getMasterUserFolder() << "voxel" << std::setfill('0') << std::setw(2) << z << ".png";

		unsigned error = lodepng::encode(ss.str(), image, _save->getMapSizeX()*16, _save->getMapSizeY()*16, LCT_RGB);
		if (error)
		{
			Log(LOG_ERROR) << "Saving to PNG failed: " << lodepng_error_text(error);
		}
	}
	return;
}

/**
 * Adds a new popup window to the queue
 * (this prevents popups from overlapping).
 * @param state Pointer to popup state.
 */
void BattlescapeState::popup(State *state)
{
	_popups.push_back(state);
}

/**
 * Finishes up the current battle, shuts down the battlescape
 * and presents the debriefing screen for the mission.
 * @param abort Was the mission aborted?
 * @param inExitArea Number of soldiers in the exit area OR number of survivors when battle finished due to either all aliens or objective being destroyed.
 */
void BattlescapeState::finishBattle(bool abort, int inExitArea)
{
	while (!_game->isState(this))
	{
		_game->popState();
	}
	_game->getCursor()->setVisible(true);
	if (_save->getAmbientSound() != -1)
	{
		_game->getMod()->getSoundByDepth(0, _save->getAmbientSound())->stopLoop();
	}
	AlienDeployment *ruleDeploy = _game->getMod()->getDeployment(_save->getMissionType());
	if (!ruleDeploy)
	{
		for (std::vector<Ufo*>::iterator ufo =_game->getSavedGame()->getUfos()->begin(); ufo != _game->getSavedGame()->getUfos()->end(); ++ufo)
		{
			if ((*ufo)->isInBattlescape())
			{
				ruleDeploy = _game->getMod()->getDeployment((*ufo)->getRules()->getType());
				break;
			}
		}
	}
	std::string nextStage;
	if (ruleDeploy)
	{
		nextStage = ruleDeploy->getNextStage();
	}

	if (!nextStage.empty() && inExitArea)
	{
		// if there is a next mission stage + we have people in exit area OR we killed all aliens, load the next stage
		_popups.clear();
		_save->setMissionType(nextStage);
		BattlescapeGenerator bgen = BattlescapeGenerator(_game);
		bgen.nextStage();
		_game->popState();
		_game->pushState(new BriefingState(0, 0));
	}
	else
	{
		_popups.clear();
		_animTimer->stop();
		_gameTimer->stop();
		_game->popState();
		_game->pushState(new DebriefingState);
		std::string cutscene;
		if (ruleDeploy)
		{
			if (abort)
			{
				cutscene = ruleDeploy->getAbortCutscene();
			}
			else if (inExitArea == 0)
			{
				cutscene = ruleDeploy->getLoseCutscene();
			}
			else
			{
				cutscene = ruleDeploy->getWinCutscene();
			}
		}
		if (!cutscene.empty())
		{
			// if cutscene is "wingame" or "losegame", then the DebriefingState
			// pushed above will get popped without being shown.  otherwise
			// it will get shown after the cutscene.
			_game->pushState(new CutsceneState(cutscene));

			if (cutscene == CutsceneState::WIN_GAME)
			{
				_game->getSavedGame()->setEnding(END_WIN);
			}
			else if (cutscene == CutsceneState::LOSE_GAME)
			{
				_game->getSavedGame()->setEnding(END_LOSE);
			}
			// Autosave if game is over
			if (_game->getSavedGame()->getEnding() != END_NONE && _game->getSavedGame()->isIronman())
			{
				_game->pushState(new SaveGameState(OPT_BATTLESCAPE, SAVE_IRONMAN, _palette));
			}
		}
	}
}

/**
 * Shows the launch button.
 * @param show Show launch button?
 */
void BattlescapeState::showLaunchButton(bool show)
{
	_btnLaunch->setVisible(show);
}

/**
 * Shows the PSI button.
 * @param show Show PSI button?
 */
void BattlescapeState::showPsiButton(bool show)
{
	_btnPsi->setVisible(show);
}

/**
 * Clears mouse-scrolling state (isMouseScrolling).
 */
void BattlescapeState::clearMouseScrollingState()
{
	_isMouseScrolling = false;
}

/**
 * Returns a pointer to the battlegame, in case we need its functions.
 */
BattlescapeGame *BattlescapeState::getBattleGame()
{
	return _battleGame;
}

/**
 * Handler for the mouse moving over the icons, disabling the tile selection cube.
 * @param action Pointer to an action.
 */
void BattlescapeState::mouseInIcons(Action *)
{
	_mouseOverIcons = true;
}

/**
 * Handler for the mouse going out of the icons, enabling the tile selection cube.
 * @param action Pointer to an action.
 */
void BattlescapeState::mouseOutIcons(Action *)
{
	_mouseOverIcons = false;
}

/**
 * Checks if the mouse is over the icons.
 * @return True, if the mouse is over the icons.
 */
bool BattlescapeState::getMouseOverIcons() const
{
	return _mouseOverIcons;
}

#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
// SPDX-License-Identifier: GPL-3.0-or-later; selection presentation guard, 2026-10-12.
bool BattlescapeState::selectionControlVisible() const
{
    // Both ordinary cycling hotkeys belong to this displayed button. No target
    // lookup or eligibility prediction is exposed by the control boundary.
    return _btnNextSoldier->stanceContextVisible() && _btnNextSoldier->getX() >= 0 &&
        _btnNextSoldier->getY() >= 144 && _btnNextSoldier->getX() + _btnNextSoldier->getWidth() <= 320 &&
        _btnNextSoldier->getY() + _btnNextSoldier->getHeight() <= 200;
}
bool BattlescapeState::stanceContext(Observer::StanceContext &c, bool covered) const
{
    if ((!covered && !_game->isState(const_cast<BattlescapeState *>(this))) ||
        !_game->getScreen()->stanceAllowed() || _firstInit || !_gameTimer->isRunning() || !_popups.empty() ||
        _save->getSide() != FACTION_PLAYER || _save->getDebugMode() || !_battleGame->getPanicHandled() ||
        _battleGame->isBusy() || _map->getProjectile() || !_map->getExplosions()->empty() ||
        _save->getUnitsFalling() || !_save->getFallingUnits()->empty() || !allowButtons()) return false;
    BattleUnit *actor = _save->getSelectedUnit();
    if (!actor || actor->getFaction() != FACTION_PLAYER || actor->getOriginalFaction() != FACTION_PLAYER ||
        actor->getType() != "SOLDIER" || actor->isOut() || actor->isFloating() || actor->getStatus() != STATUS_STANDING) return false;
    Surface *widgets[] = {_icons, _txtName, _numTimeUnits, _numEnergy, _numHealth, _numMorale,
        _btnKneel, _btnReserveNone, _btnReserveSnap, _btnReserveAimed, _btnReserveAuto, _btnReserveKneel};
    for (Surface *widget : widgets)
        if (!widget->stanceContextVisible() || widget->getX() < 0 || widget->getY() < 144 ||
            widget->getX() + widget->getWidth() > 320 || widget->getY() + widget->getHeight() > 200) return false;
    if (_stanceShownActor != actor || _stanceShownKneeled != actor->isKneeled() ||
        _btnReserveKneel->stanceToggled() != _save->getKneelReserved() ||
        _reserve != (_save->getTUReserved() == BA_SNAPSHOT ? _btnReserveSnap :
            _save->getTUReserved() == BA_AIMEDSHOT ? _btnReserveAimed :
            _save->getTUReserved() == BA_AUTOSHOT ? _btnReserveAuto : _btnReserveNone) ||
        _icons->getWidth() != 320 || _icons->getHeight() != 56 || _warning->getVisible() ||
        _txtName->getText().empty() || _txtName->getText() != actor->getName(_game->getLanguage(), false) ||
        _numTimeUnits->getValue() != static_cast<unsigned>(actor->getTimeUnits()) ||
        _numEnergy->getValue() != static_cast<unsigned>(actor->getEnergy()) ||
        _numHealth->getValue() != static_cast<unsigned>(actor->getHealth()) ||
        _numMorale->getValue() != static_cast<unsigned>(actor->getMorale())) return false;
    c.state = const_cast<BattlescapeState *>(this); c.save = _save; c.actor = actor;
    c.selectionRevision = _save->stanceControlRevision(); c.actorRevision = actor->stanceControlRevision();
    c.uiRevision = _stanceUiRevision; c.kneeled = actor->isKneeled(); c.kneelReserved = _save->getKneelReserved();
    c.tu = actor->getTimeUnits(); c.energy = actor->getEnergy(); c.health = actor->getHealth();
    c.morale = actor->getMorale(); c.stun = actor->getStunlevel(); c.reserved = _save->getTUReserved();
    c.name = _txtName->getText(); return true;
}
#endif

#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
// Only the actual displayed control is checked. The ordinary handler owns
// inventory eligibility, action cancellation, construction and ground layout.
bool BattlescapeState::inventoryControlVisible() const
{
    return _btnInventory->stanceContextVisible() && _btnInventory->getX() >= 0 &&
        _btnInventory->getY() >= 144 && _btnInventory->getX() + _btnInventory->getWidth() <= 320 &&
        _btnInventory->getY() + _btnInventory->getHeight() <= 200;
}
#endif

#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_TURN
// SPDX-License-Identifier: GPL-3.0-or-later; normal right-click pilot, 2026-10-12.
bool BattlescapeState::turnObservationWaiting(const BattleUnit *expectedActor) const
{
    // Only the natural busy interval following our normal right-click may wait
    // for a fresh view. No queue contents or busy reasons cross the boundary.
    BattleUnit *actor = _save->getSelectedUnit();
    return _game->isState(const_cast<BattlescapeState *>(this)) && _game->getScreen()->stanceAllowed() &&
        !_firstInit && _gameTimer->isRunning() && _popups.empty() && _save->getSide() == FACTION_PLAYER &&
        !_save->getDebugMode() && _battleGame->getPanicHandled() && _battleGame->isBusy() &&
        !_map->getProjectile() && _map->getExplosions()->empty() && !_save->getUnitsFalling() &&
        _save->getFallingUnits()->empty() && actor && actor == expectedActor && actor->getFaction() == FACTION_PLAYER &&
        actor->getOriginalFaction() == FACTION_PLAYER && actor->getType() == "SOLDIER" &&
        !actor->isOut() && !actor->isFloating() &&
        (actor->getStatus() == STATUS_TURNING || actor->getStatus() == STATUS_STANDING) &&
        SDL_GetModState() == KMOD_NONE && SDL_GetMouseState(0, 0) == 0 &&
        !_isMouseScrolling && !_isMouseScrolled && !_mouseOverIcons && _map->getCamera()->turnControlIdle() &&
        _map->getCursorType() == CT_NORMAL && _battleGame->getCurrentAction()->type == BA_NONE &&
        !_battleGame->getCurrentAction()->targeting && _battleGame->getCurrentAction()->waypoints.empty() &&
        _map->getWaypoints()->empty() && !_save->getPathfinding()->isPathPreviewed();
}
bool BattlescapeState::turnContext(Observer::TurnContext &c, bool covered) const
{
    Observer::StanceContext hud;
    if (!stanceContext(hud, covered) || SDL_GetModState() != KMOD_NONE ||
        (SDL_GetMouseState(0, 0) & ~(covered ? SDL_BUTTON_LMASK : 0)) != 0 || _isMouseScrolling || _isMouseScrolled || _mouseOverIcons ||
        _map->getCursorType() != CT_NORMAL || !_map->getCamera()->turnControlIdle() ||
        _battleGame->getCurrentAction()->type != BA_NONE || _battleGame->getCurrentAction()->targeting ||
        !_battleGame->getCurrentAction()->waypoints.empty() || !_map->getWaypoints()->empty() ||
        _save->getPathfinding()->isPathPreviewed() ||
        !_map->turnContextVisible() || _map->getX() != 0 || _map->getY() != 0 ||
        _map->getWidth() != 320 || _map->getHeight() != 200) return false;
    const Camera *camera = _map->getCamera();
    c.position = hud.actor->getPosition(); c.direction = hud.actor->getDirection();
    c.turretDirection = hud.actor->getTurretDirection(); c.poseRevision = hud.actor->turnControlPoseRevision();
    c.cameraOffset = camera->getMapOffset(); c.cameraRevision = camera->turnControlRevision();
    c.allLayers = camera->getShowAllLayers(); c.geometry = camera->turnControlGeometry();
    c.geometry.push_back(_map->getX()); c.geometry.push_back(_map->getY());
    c.geometry.push_back(_map->getWidth()); c.geometry.push_back(_map->getHeight());
    return true;
}
void BattlescapeState::turnControlClick(int x, int y)
{
    // The ordinary Map mouse-over path establishes its cached selector, including
    // the sprite-height offset. The ordinary right-click then owns all gameplay.
    SDL_Event event = {};
    event.type = SDL_MOUSEBUTTONUP;
    event.button.button = SDL_BUTTON_RIGHT; event.button.state = SDL_RELEASED;
    event.button.x = x; event.button.y = y;
    Action action(&event, 1.0, 1.0, 0, 0);
    action.setMouseAction(x, y, _map->getX(), _map->getY()); action.setSender(_map);
    _map->mouseOver(&action, this);
    mapClick(&action);
}
#endif

#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK
// This option is checked for L only. Observing V and ordinary R remain usable
// with the user's existing preview setting; the pilot never changes it.
bool BattlescapeState::mapLeftClickControlAllowed() const
{
    return Options::battleNewPreviewPath == PATH_NONE;
}
bool BattlescapeState::mapLeftClickObservationContinuing(const BattleUnit *expectedActor, bool busy) const
{
    // A narrowly bounded continuation of the live ordinary walking action.
    // There is no path/target inspection and no state advancement or abort.
    // Normal movement and selection mark the HUD dirty before its next blit.
    // This continuity check permits that redraw; granting a view still uses
    // the fully painted, exact stanceContext/turnContext checks after blit.
    BattleUnit *actor = _save->getSelectedUnit();
    if (!_game->isState(const_cast<BattlescapeState *>(this)) || !_game->getScreen()->stanceAllowed() ||
        _firstInit || !_gameTimer->isRunning() || !_popups.empty() || _warning->getVisible() ||
        _save->getSide() != FACTION_PLAYER || _save->getDebugMode() || !_battleGame->getPanicHandled() ||
        _battleGame->isBusy() != busy || _map->getProjectile() || !_map->getExplosions()->empty() ||
        _save->getUnitsFalling() || !_save->getFallingUnits()->empty() ||
        !actor || (busy && actor != expectedActor) || actor->getFaction() != FACTION_PLAYER ||
        actor->getOriginalFaction() != FACTION_PLAYER || actor->getType() != "SOLDIER" ||
        actor->isOut() || actor->isFloating() ||
        (actor->getStatus() != STATUS_WALKING && actor->getStatus() != STATUS_TURNING && actor->getStatus() != STATUS_STANDING) ||
        (!busy && actor->getStatus() != STATUS_STANDING) ||
        SDL_GetModState() != KMOD_NONE || SDL_GetMouseState(0, 0) != 0 ||
        _isMouseScrolling || _isMouseScrolled || _mouseOverIcons || !_map->getCamera()->turnControlIdle() ||
        _map->getCursorType() != (busy ? CT_NONE : CT_NORMAL) || _battleGame->getCurrentAction()->type != BA_NONE ||
        _battleGame->getCurrentAction()->targeting || !_battleGame->getCurrentAction()->waypoints.empty() ||
        !_map->getWaypoints()->empty() || _save->getPathfinding()->isPathPreviewed() ||
        !_map->turnContextVisible() || _map->getX() != 0 || _map->getY() != 0 ||
        _map->getWidth() != 320 || _map->getHeight() != 200) return false;
    Surface *widgets[] = {_icons, _txtName, _numTimeUnits, _numEnergy, _numHealth, _numMorale,
        _btnKneel, _btnReserveNone, _btnReserveSnap, _btnReserveAimed, _btnReserveAuto, _btnReserveKneel};
    for (Surface *widget : widgets)
        if (!widget->turnContextVisible() || widget->getX() < 0 || widget->getY() < 144 ||
            widget->getX() + widget->getWidth() > 320 || widget->getY() + widget->getHeight() > 200) return false;
    return _icons->getWidth() == 320 && _icons->getHeight() == 56;
}
bool BattlescapeState::mapLeftClickObservationReady() const
{
    // An ordinary click may select another visible friendly. Only a newly
    // selected supported soldier can subsequently produce a fresh grant.
    return mapLeftClickObservationContinuing(0, false);
}
bool BattlescapeState::mapLeftClickObservationWaiting(const BattleUnit *expectedActor) const
{
    return mapLeftClickObservationContinuing(expectedActor, true);
}
void BattlescapeState::mapLeftClickControlClick(int x, int y)
{
    // Identical selector setup to the right-click pilot, followed by exactly
    // one normal map left-click. primaryAction owns movement, selection, costs,
    // reserves, contact interruption, reaction fire and every possible no-op.
    SDL_Event event = {};
    event.type = SDL_MOUSEBUTTONUP;
    event.button.button = SDL_BUTTON_LEFT; event.button.state = SDL_RELEASED;
    event.button.x = x; event.button.y = y;
    Action action(&event, 1.0, 1.0, 0, 0);
    action.setMouseAction(x, y, _map->getX(), _map->getY()); action.setSender(_map);
    _map->mouseOver(&action, this);
    mapClick(&action);
}
#endif

/**
 * Determines whether the player is allowed to press buttons.
 * Buttons are disabled in the middle of a shot, during the alien turn,
 * and while a player's units are panicking.
 * The save button is an exception as we want to still be able to save if something
 * goes wrong during the alien turn, and submit the save file for dissection.
 * @param allowSaving True, if the help button was clicked.
 * @return True if the player can still press buttons.
 */
bool BattlescapeState::allowButtons(bool allowSaving) const
{
	return ((allowSaving || _save->getSide() == FACTION_PLAYER || _save->getDebugMode())
		&& (_battleGame->getPanicHandled() || _firstInit )
		&& (allowSaving || !_battleGame->isBusy() || _firstInit)
		&& (_map->getProjectile() == 0));
}

/**
 * Reserves time units for kneeling.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnReserveKneelClick(Action *action)
{
	if (allowButtons())
	{
		SDL_Event ev;
		ev.type = SDL_MOUSEBUTTONDOWN;
		ev.button.button = SDL_BUTTON_LEFT;
		Action a = Action(&ev, 0.0, 0.0, 0, 0);
		action->getSender()->mousePress(&a, this);
		_battleGame->setKneelReserved(!_battleGame->getKneelReserved());

		_btnReserveKneel->toggle(_battleGame->getKneelReserved());

		// update any path preview
		if (_battleGame->getPathfinding()->isPathPreviewed())
		{
			_battleGame->getPathfinding()->removePreview();
			_battleGame->getPathfinding()->previewPath();
		}
	}
}

/**
 * Removes all time units.
 * @param action Pointer to an action.
 */
void BattlescapeState::btnZeroTUsClick(Action *action)
{
	if (allowButtons())
	{
		SDL_Event ev;
		ev.type = SDL_MOUSEBUTTONDOWN;
		ev.button.button = SDL_BUTTON_LEFT;
		Action a = Action(&ev, 0.0, 0.0, 0, 0);
		action->getSender()->mousePress(&a, this);
		if (_battleGame->getSave()->getSelectedUnit())
		{
			_battleGame->getSave()->getSelectedUnit()->setTimeUnits(0);
			updateSoldierInfo();
		}
	}
}

/**
 * Shows a tooltip for the appropriate button.
 * @param action Pointer to an action.
 */
void BattlescapeState::txtTooltipIn(Action *action)
{
	if (allowButtons() && Options::battleTooltips)
	{
		_currentTooltip = action->getSender()->getTooltip();
		_txtTooltip->setText(tr(_currentTooltip));
	}
}

/**
 * Clears the tooltip text.
 * @param action Pointer to an action.
 */
void BattlescapeState::txtTooltipOut(Action *action)
{
	if (allowButtons() && Options::battleTooltips)
	{
		if (_currentTooltip == action->getSender()->getTooltip())
		{
			_txtTooltip->setText("");
		}
	}
}

/**
 * Updates the scale.
 * @param dX delta of X;
 * @param dY delta of Y;
 */
void BattlescapeState::resize(int &dX, int &dY)
{
	dX = Options::baseXResolution;
	dY = Options::baseYResolution;
	int divisor = 1;
	double pixelRatioY = 1.0;

	if (Options::nonSquarePixelRatio)
	{
		pixelRatioY = 1.2;
	}
	switch (Options::battlescapeScale)
	{
	case SCALE_SCREEN_DIV_3:
		divisor = 3;
		break;
	case SCALE_SCREEN_DIV_2:
		divisor = 2;
		break;
	case SCALE_SCREEN:
		break;
	default:
		dX = 0;
		dY = 0;
		return;
	}

	Options::baseXResolution = std::max(Screen::ORIGINAL_WIDTH, Options::displayWidth / divisor);
	Options::baseYResolution = std::max(Screen::ORIGINAL_HEIGHT, (int)(Options::displayHeight / pixelRatioY / divisor));

	dX = Options::baseXResolution - dX;
	dY = Options::baseYResolution - dY;
	_map->setWidth(Options::baseXResolution);
	_map->setHeight(Options::baseYResolution);
	_map->getCamera()->resize();
	_map->getCamera()->jumpXY(dX/2, dY/2);

	for (std::vector<Surface*>::const_iterator i = _surfaces.begin(); i != _surfaces.end(); ++i)
	{
		if (*i != _map && (*i) != _btnPsi && *i != _btnLaunch && *i != _txtDebug)
		{
			(*i)->setX((*i)->getX() + dX / 2);
			(*i)->setY((*i)->getY() + dY);
		}
		else if (*i != _map && *i != _txtDebug)
		{
			(*i)->setX((*i)->getX() + dX);
		}
	}

}

/**
 * Move the mouse back to where it started after we finish drag scrolling.
 * @param action Pointer to an action.
 */
void BattlescapeState::stopScrolling(Action *action)
{
	if (Options::battleDragScrollInvert)
	{
		SDL_WarpMouse(_xBeforeMouseScrolling, _yBeforeMouseScrolling);
		action->setMouseAction(_xBeforeMouseScrolling, _yBeforeMouseScrolling, _map->getX(), _map->getY());
		_battleGame->setupCursor();
		if (_battleGame->getCurrentAction()->actor == 0 && (_save->getSide() == FACTION_PLAYER || _save->getDebugMode()))
		{
			getMap()->setCursorType(CT_NORMAL);
		}
	}
	else
	{
		SDL_WarpMouse(_cursorPosition.x, _cursorPosition.y);
		action->setMouseAction(_cursorPosition.x, _cursorPosition.y, _map->getX(), _map->getY());
		_map->setSelectorPosition(action->getAbsoluteXMouse(), action->getAbsoluteYMouse());
	}
	// reset our "mouse position stored" flag
	_cursorPosition.z = 0;
}

/**
 * Autosave the game the next time the battlescape is displayed.
 */
void BattlescapeState::autosave()
{
	_autosave = true;
}

}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
namespace OpenXcom {
namespace {
// Camera geometry and a separately bounded semantic engine context. Neither
// terrain presence nor an activation target promises passability or action success.
std::string agentTacticalJson(const Map *map, const Game *game, const std::vector<Surface *> &paintOrder, Observer::AgentUiState &ui, std::string &focus, std::string &mapContext)
{
    AgentMapSnapshot view; focus = "null";
    const std::string emptyContext = "{\"scope\":\"viewport\",\"selected_position\":null,\"cells\":[],\"truncated\":false}";
    mapContext = emptyContext;
    const std::string unavailable = "{\"mode\":\"unavailable\",\"viewport\":null,\"message\":null,\"view_level\":null,\"grid\":[],\"fragment_status\":\"unavailable\",\"fragments\":[],\"map_context\":" + emptyContext + ",\"truncated\":false}";
    const auto reject = [&]() { ui.points.clear(); mapContext = emptyContext; return unavailable; };
    if (!map || !map->agentMapSnapshot(view) || view.x < 0 || view.x > 4095 || view.y < 0 || view.y > 4095 ||
        view.width <= 0 || view.width > 640 || view.height <= 0 || view.height > 400) return reject();
    const auto clear = [&](int x, int y, int width, int height) -> bool
    {
        Observer::AgentUiState footprint;
        footprint.regions.push_back({view.x + x, view.y + y, width, height});
        return Observer::agentRegionsUncovered(footprint.regions, map, paintOrder) && game->agentOverlayClear(footprint);
    };
    std::string label = "null";
    if (view.hiddenMovement && !view.hiddenMovementLabel.empty() &&
        clear(view.messageRegion.x, view.messageRegion.y, view.messageRegion.width, view.messageRegion.height))
        label = Observer::agentJsonString(view.hiddenMovementLabel);
    const auto positionJson = [](const AgentMapPosition &p) {
        return std::string("{\"x\":") + std::to_string(p.x) + ",\"y\":" + std::to_string(p.y) + ",\"z\":" + std::to_string(p.z) + "}";
    };
    std::string grid = "[", cells = "[";
    unsigned count = 0;
    bool truncated = view.gridTruncated;
    if (!view.hiddenMovement)
        for (const AgentCameraGridTarget &cell : view.grid)
        {
            const int left = cell.polygon[3].x, top = cell.polygon[0].y;
            const int width = cell.polygon[1].x - left + 1, height = cell.polygon[2].y - top + 1;
            if (!clear(left, top, width, height)) continue;
            std::string item = "{\"handle\":\"g" + std::to_string(count) + "\",\"polygon\":[";
            for (unsigned i = 0; i < 4; ++i)
            {
                if (i) item += ",";
                item += "{\"x\":" + std::to_string(cell.polygon[i].x) + ",\"y\":" + std::to_string(cell.polygon[i].y) + "}";
            }
            item += "],\"target\":{\"x\":" + std::to_string(cell.target.x) + ",\"y\":" + std::to_string(cell.target.y) + "}}";
            const std::size_t sourceIndex = &cell - &view.grid.front();
            if (sourceIndex >= view.cells.size()) return reject();
            const AgentMapCell &fact = view.cells[sourceIndex];
            if (fact.position.x < -1 || fact.position.y < -1 || fact.position.z < 0) return reject();
            std::string semantic = "{\"handle\":\"g" + std::to_string(count) + "\",\"position\":" + positionJson(fact.position) + ",\"parts\":{";
            const char *names[] = {"floor", "west", "north", "object"};
            for (unsigned part = 0; part < 4; ++part)
            {
                if (part) semantic += ",";
                semantic += Observer::agentJsonString(names[part]) + ":" + Observer::agentJsonString(fact.parts[part]);
            }
            semantic += "},\"unit\":" + Observer::agentJsonString(fact.unit) + "}";
            // Exact compact serialized array lengths, including closing brackets/commas.
            if (count == 128 || grid.size() + cells.size() + item.size() + semantic.size() + 2 + (count ? 2 : 0) > 23500)
            { truncated = true; break; }
            if (cell.target == view.focus) focus = Observer::agentJsonString("g" + std::to_string(count));
            ui.points.push_back(std::make_pair(view.x + cell.target.x, view.y + cell.target.y));
            ui.mapTargetPositions.push_back({{fact.position.x,fact.position.y,fact.position.z}});
            if (count++) { grid += ","; cells += ","; }
            grid += item; cells += semantic;
        }
    grid += "]"; cells += "]";
    if (!view.hiddenMovement)
    {
        if (view.selectedPositionKnown && (view.selectedPosition.x < 0 || view.selectedPosition.y < 0 || view.selectedPosition.z < 0)) return reject();
        mapContext = "{\"scope\":\"viewport\",\"selected_position\":" +
            (view.selectedPositionKnown ? positionJson(view.selectedPosition) : std::string("null")) +
            ",\"cells\":" + cells + ",\"truncated\":" + (truncated ? "true" : "false") + "}";
    }
    // A private draw branch cannot survive complete public occlusion. A
    // missing/clipped/covered message must not disclose Hidden Movement.
    if (!agentMapModeAdmitted(view, label != "null", count)) { mapContext = emptyContext; return reject(); }
    if (!view.hiddenMovement)
    {
        const auto admitted = [&](int x, int y) { return clear(x, y, 1, 1); };
        ui.projectileFact = map->agentProjectileFact(game->getScreen(), admitted);
        ui.impactFact = map->agentProjectileFact(game->getScreen(), admitted, Observer::AgentRendererRole::Impact);
        ui.collapseFact = map->agentProjectileFact(game->getScreen(), admitted, Observer::AgentRendererRole::Collapse);
        ui.thrownItemFact = map->agentProjectileFact(game->getScreen(), admitted, Observer::AgentRendererRole::ThrownItem);
        ui.largeExplosionFact = map->agentProjectileFact(game->getScreen(), admitted, Observer::AgentRendererRole::LargeExplosion);
    }
    if (!view.hiddenMovement)
    {
        ui.publicCameraKnown = true;
        ui.publicCamera = {view.x, view.y, view.width, view.height};
        ui.publicCameraRevision = map->agentPaintedViewRevision();
    }
    return std::string("{\"mode\":\"") + (view.hiddenMovement ? "hidden_movement" : "map") +
        "\",\"viewport\":{\"x\":" + std::to_string(view.x) + ",\"y\":" + std::to_string(view.y) +
        ",\"width\":" + std::to_string(view.width) + ",\"height\":" + std::to_string(view.height) +
        "},\"message\":" + label + ",\"view_level\":null,\"grid\":" + grid +
        ",\"fragment_status\":\"unavailable\",\"fragments\":[],\"map_context\":" + mapContext + ",\"truncated\":" + (truncated ? "true" : "false") + "}";
}
}
std::string BattlescapeState::agentViewportControls(Observer::AgentUiState &out) const
{
    // Only the painted HUD digit is public. Camera z/show-all state is not a
    // substitute when that digit is hidden, dirty, clipped or covered.
    SDL_Rect layerInk;
    std::vector<Observer::AgentUiRect> layerRegion;
    if (_numLayers && _numLayers->agentPaintedBounds(layerInk))
        layerRegion.push_back({layerInk.x, layerInk.y, layerInk.w, layerInk.h});
    const bool layersShown = !layerRegion.empty() &&
        (_numLayers->getValue() == 1 || _numLayers->getValue() == 2) &&
        Observer::agentRegionsUncovered(layerRegion, _numLayers, _surfaces);
    bool layersIncluded = false;
    const Surface *buttons[] = {_btnMapUp, _btnMapDown, _btnShowMap, _btnUnitUp, _btnUnitDown};
    const char *actions[] = {"view_level_up", "view_level_down", "open_minimap", "unit_up", "unit_down"};
    const char *labels[] = {"View level above", "View level below", "Minimap", "Unit level above", "Unit level below"};
    for (unsigned i = 0; i < 5; ++i)
    {
        if (!Observer::agentWidgetUncovered(buttons[i], _surfaces)) continue;
        out.actions.push_back(Observer::AgentUiAction(actions[i], labels[i], "visible_control_semantics"));
        Observer::agentIncludeWidget(out, buttons[i]);
    }
    // The ordinary layer digit is part of this button's visible composition.
    // Ignore only that proved, contained digit when checking later occluders.
    std::vector<Surface *> layerOrder = _surfaces;
    const bool contained = layersShown && _btnShowLayers &&
        layerInk.x >= _btnShowLayers->getX() && layerInk.y >= _btnShowLayers->getY() &&
        layerInk.x + layerInk.w <= _btnShowLayers->getX() + _btnShowLayers->getWidth() &&
        layerInk.y + layerInk.h <= _btnShowLayers->getY() + _btnShowLayers->getHeight();
    if (contained)
        layerOrder.erase(std::remove(layerOrder.begin(), layerOrder.end(), _numLayers), layerOrder.end());
    if (Observer::agentWidgetUncovered(_btnShowLayers, layerOrder))
    {
        out.actions.push_back(Observer::AgentUiAction("toggle_layers", "Toggle map layers", "visible_control_semantics"));
        Observer::agentIncludeWidget(out, _btnShowLayers);
        layersIncluded = contained;
    }
    if (layersShown && !layersIncluded)
        out.regions.insert(out.regions.end(), layerRegion.begin(), layerRegion.end());
    // Grid regions already prove an uncovered painted map viewport. No unit,
    // terrain, destination or hidden bounds query grants camera authority.
    if (!out.points.empty() && !_isMouseScrolling && !_isMouseScrolled && _map->getCamera()->agentScrollReady())
        out.actions.push_back(Observer::AgentUiAction("camera_pan", "Pan camera one step", "visible_control_semantics", "camera_direction"));
    return layersShown ? std::to_string(_numLayers->getValue()) : "null";
}
bool BattlescapeState::agentViewportDispatch(const Observer::AgentRequest &request, const Observer::AgentUiState &shown)
{
    if (!shown.offers(request.action)) return false;
    if (request.action == "view_level_up") btnMapUpClick(nullptr);
    else if (request.action == "view_level_down") btnMapDownClick(nullptr);
    else if (request.action == "toggle_layers") btnShowLayersClick(nullptr);
    else if (request.action == "open_minimap") btnShowMapClick(nullptr);
    else if (request.action == "unit_up") btnUnitUpClick(nullptr);
    else if (request.action == "unit_down") btnUnitDownClick(nullptr);
    else if (request.action == "camera_pan")
        return !_isMouseScrolling && !_isMouseScrolled && _map->getCamera()->agentScrollStep(request.choice);
    else return false;
    return true;
}
// Public meaning of ordinary map input, not a promise of movement or hit.
// Only the selected player's current control is disclosed; no target/weapon data.
std::string BattlescapeState::agentInputContext(bool mapAvailable) const
{
    const std::string unavailable = "{\"mode\":\"unavailable\",\"selected_action\":null,\"cursor\":null}";
    if (!mapAvailable || _save->getDebugMode() || _save->getSide() != FACTION_PLAYER)
        return unavailable;
    BattleUnit *selected = _save->getSelectedUnit();
    const BattleAction *current = _battleGame->getCurrentAction();
    if (!selected || selected->getFaction() != FACTION_PLAYER || current->actor != selected)
        return unavailable;
    if (_battleGame->isBusy())
        return "{\"mode\":\"busy\",\"selected_action\":null,\"cursor\":null}";
    const char *action = "none";
    if (current->targeting)
    {
        switch (current->type)
        {
        case BA_SNAPSHOT: action = "snap"; break;
        case BA_AIMEDSHOT: action = "aimed"; break;
        case BA_AUTOSHOT: action = "auto"; break;
        case BA_THROW: action = "throw"; break;
        case BA_LAUNCH: action = "launch"; break;
        case BA_PANIC: action = "psi_panic"; break;
        case BA_MINDCONTROL: action = "psi_control"; break;
        case BA_USE: action = "use"; break;
        default: action = "other"; break;
        }
    }
    const char *cursor = "other";
    switch (_map->getCursorType())
    {
    case CT_NORMAL: cursor = "normal"; break;
    case CT_AIM: cursor = "aim"; break;
    case CT_THROW: cursor = "throw"; break;
    case CT_WAYPOINT: cursor = "waypoint"; break;
    case CT_PSI: cursor = "psi"; break;
    case CT_NONE: cursor = "none"; break;
    default: break;
    }
    return std::string("{\"mode\":\"") + (current->targeting ? "targeting" : "normal") +
        "\",\"selected_action\":\"" + action + "\",\"cursor\":\"" + cursor + "\"}";
}
bool BattlescapeState::agentUiState(Observer::AgentUiState &out) const
{
    if (_warning->getVisible())
    {
        if (_modal || !_game->isState(const_cast<BattlescapeState *>(this)) ||
            !Observer::agentWidgetUncovered(_warning, _surfaces)) return false;
        out = Observer::AgentUiState(); out.screen = "battlescape_warning";
        out.displayedMessage = _warning->agentMessage();
        out.visible = "{\"message\":" + (out.displayedMessage.empty() ? std::string("null") : Observer::agentJsonString(out.displayedMessage)) + "}";
        Observer::agentIncludeWidget(out, _warning); return true;
    }
    if (_modal || !_game->isState(const_cast<BattlescapeState *>(this)) ||
        !Observer::agentWidgetVisible(_icons)) return false;
    out = Observer::AgentUiState(); out.screen = "battlescape";
    const bool selected = Observer::agentWidgetVisible(_numTimeUnits) && _numTimeUnits->agentValueFullyVisible() &&
        Observer::agentWidgetVisible(_numEnergy) && _numEnergy->agentValueFullyVisible() &&
        Observer::agentWidgetVisible(_numHealth) && _numHealth->agentValueFullyVisible() &&
        Observer::agentWidgetVisible(_numMorale) && _numMorale->agentValueFullyVisible();
    const bool stance = selected && Observer::agentWidgetVisible(_btnKneel);
    out.visible = "{\"selected_name\":" + Observer::agentTextJson(_txtName) +
        ",\"time_units\":" + (selected ? std::to_string(_numTimeUnits->getValue()) : "null") +
        ",\"energy\":" + (selected ? std::to_string(_numEnergy->getValue()) : "null") +
        ",\"health\":" + (selected ? std::to_string(_numHealth->getValue()) : "null") +
        ",\"morale\":" + (selected ? std::to_string(_numMorale->getValue()) : "null") +
        ",\"kneel_indicator\":" + (stance ? Observer::agentJsonString(_agentShownKneeled ? "kneeling" : "standing") : "null") + "}";
    // Read the drawn radio-button selection, never the saved TU reservation.
    BattlescapeButton *reserveButtons[] = {_btnReserveNone, _btnReserveSnap, _btnReserveAimed, _btnReserveAuto};
    const char *reserveNames[] = {"none", "snap", "aimed", "auto"};
    bool reserveShown = true;
    unsigned reserveCount = 0, reserveIndex = 0;
    for (unsigned i = 0; i < 4; ++i)
    {
        reserveShown = reserveShown && Observer::agentWidgetUncovered(reserveButtons[i], _surfaces) &&
            reserveButtons[i]->agentTogglePainted(_game->getScreen()->getSurface());
        if (reserveButtons[i]->agentShownToggled()) { ++reserveCount; reserveIndex = i; }
    }
    reserveShown = reserveShown && reserveCount == 1;
    out.visible.pop_back();
    out.visible += ",\"reserve_indicator\":" + (reserveShown ? Observer::agentJsonString(reserveNames[reserveIndex]) : "null") + "}";
    if (reserveShown)
    {
        for (BattlescapeButton *button : reserveButtons) Observer::agentIncludeWidget(out, button);
        // Native handler availability, not a prediction that an action can spend TU.
        if (!_firstInit && _save->getSide() == FACTION_PLAYER && allowButtons())
        {
            out.actions.push_back(Observer::AgentUiAction("reserve_none", "Reserve no time units", "visible_control_semantics"));
            out.actions.push_back(Observer::AgentUiAction("reserve_snap", "Reserve Snap Shot time units", "visible_control_semantics"));
            out.actions.push_back(Observer::AgentUiAction("reserve_aimed", "Reserve Aimed Shot time units", "visible_control_semantics"));
            out.actions.push_back(Observer::AgentUiAction("reserve_auto", "Reserve Auto Shot time units", "visible_control_semantics"));
        }
    }
    // Separate displayed toggle; do not infer it from the save or soldier pose.
    const bool kneelReserveShown = Observer::agentWidgetUncovered(_btnReserveKneel, _surfaces) &&
        _btnReserveKneel->agentStandaloneTogglePainted(_game->getScreen()->getSurface());
    out.visible.pop_back();
    out.visible += ",\"kneel_reserve_indicator\":" + (kneelReserveShown ?
        Observer::agentJsonString(_btnReserveKneel->agentShownToggled() ? "on" : "off") : "null") + "}";
    if (kneelReserveShown)
    {
        Observer::agentIncludeWidget(out, _btnReserveKneel);
        if (!_firstInit && _save->getSide() == FACTION_PLAYER && allowButtons())
            out.actions.push_back(Observer::AgentUiAction("toggle_kneel_reserve", "Toggle kneeling time-unit reserve", "visible_control_semantics"));
    }
    // Only admitted camera-grid points carry map-action authority.
    // Keep unrelated tactical animation outside the control binding.
    std::string focus, mapContext;
    const std::string tactical = agentTacticalJson(_map, _game, _surfaces, out, focus, mapContext);
    out.visible.pop_back();
    out.visible += ",\"layers_indicator\":" + agentViewportControls(out) + ",\"hands\":[";
    InteractiveSurface *hands[] = {_btnLeftHandItem, _btnRightHandItem};
    NumberText *ammo[] = {_numAmmoLeft, _numAmmoRight};
    unsigned handCount = 0;
    for (unsigned i = 0; i < 2; ++i)
    {
        if (!Observer::agentWidgetVisible(hands[i])) continue;
        const std::vector<Observer::AgentUiRect> region = {{hands[i]->getX(), hands[i]->getY(), hands[i]->getWidth(), hands[i]->getHeight()}};
        if (!Observer::agentRegionsUncovered(region, ammo[i], _surfaces)) continue;
        SDL_Rect ammoInk;
        const bool shownAmmo = ammo[i]->agentPaintedBounds(ammoInk) &&
            ammoInk.x >= region[0].x && ammoInk.y >= region[0].y &&
            ammoInk.x + ammoInk.w <= region[0].x + region[0].width &&
            ammoInk.y + ammoInk.h <= region[0].y + region[0].height;
        if (handCount++) out.visible += ",";
        out.visible += std::string("{\"hand\":\"") + (i ? "right" : "left") + "\",\"ammo\":" +
            (shownAmmo ? std::to_string(ammo[i]->getValue()) : "null") + "}";
        out.regions.insert(out.regions.end(), region.begin(), region.end());
    }
    out.visible += "],\"enemy_buttons\":[";
    for (unsigned i = 0; i < VISIBLE_MAX; ++i)
    {
        SDL_Rect numberInk;
        if (!_agentEnemyPainted[i] || !Observer::agentWidgetVisible(_btnVisibleUnit[i]) ||
            !_numVisibleUnit[i]->agentPaintedBounds(numberInk) ||
            !Observer::agentRegionVisible({numberInk.x, numberInk.y, numberInk.w, numberInk.h})) continue;
        const std::vector<Observer::AgentUiRect> region = {{_btnVisibleUnit[i]->getX(), _btnVisibleUnit[i]->getY(), _btnVisibleUnit[i]->getWidth(), _btnVisibleUnit[i]->getHeight()}};
        if (numberInk.x < region[0].x || numberInk.y < region[0].y ||
            numberInk.x + numberInk.w > region[0].x + region[0].width ||
            numberInk.y + numberInk.h > region[0].y + region[0].height ||
            !Observer::agentRegionsUncovered(region, _numVisibleUnit[i], _surfaces)) continue;
        if (!out.rows.empty()) out.visible += ",";
        out.visible += "{\"handle\":\"e" + std::to_string(out.rows.size()) + "\",\"number\":" + std::to_string(_numVisibleUnit[i]->getValue()) + "}";
        out.rows.push_back(i); out.widgets.push_back(std::make_pair(_visibleUnit[i], 0));
        out.regions.insert(out.regions.end(), region.begin(), region.end());
    }
    out.visible += "],\"focus_target\":" + focus + "}";
    out.visible.pop_back();
    out.visible += ",\"input_context\":" + agentInputContext(tactical.find("{\"mode\":\"map\",") == 0) + "}";
    out.controlVisible = out.visible;
    out.controlVisible.pop_back(); out.controlVisible += ",\"map_context\":" + mapContext + "}";
    out.visible.pop_back(); out.visible += ",\"tactical\":" + tactical + "}";
    // Geometry and ordinary input context bind targets; animation/audio do not.
    out.widgets.push_back(std::make_pair(_map->getCamera(), _map->getCamera()->agentViewRevision()));
    const BattleAction *current = _battleGame->getCurrentAction();
    out.widgets.push_back(std::make_pair(current->weapon, 0));
    out.context = {static_cast<std::uint64_t>(current->type), current->targeting ? 1ULL : 0ULL,
        static_cast<std::uint64_t>(_map->getCursorType()), _battleGame->isBusy() ? 1ULL : 0ULL};
    out.context.push_back(static_cast<std::uint64_t>(Options::battleScrollSpeed));
    if (handCount) out.actions.push_back(Observer::AgentUiAction("open_hand_action_menu", "Activate hand control", "visible_control_semantics", "hand"));
    if (!out.rows.empty()) out.actions.push_back(Observer::AgentUiAction("center_enemy", "Center on enemy button", "visible_control_semantics", "enemy_button"));
    if (Observer::agentWidgetUncovered(_btnCenter, _surfaces))
    { out.actions.push_back(Observer::AgentUiAction("center_selected_unit", "Center selected unit", "visible_control_semantics"));
      Observer::agentIncludeWidget(out, _btnCenter); }
    if (!out.points.empty() && !_battleGame->isBusy() && _map->getCursorType() != CT_NONE &&
        !_isMouseScrolling && !_isMouseScrolled)
    {
        out.actions.push_back(Observer::AgentUiAction("map_primary", "Primary map activation", "visible_control_semantics", "grid_cell"));
        out.actions.push_back(Observer::AgentUiAction("map_secondary", "Secondary map activation", "visible_control_semantics", "grid_cell"));
    }
    // Game clears the composite before painting this top full-screen state.
    // Require a post-overlay public map witness too; unavailable/hidden views
    // never prove that an earlier displayed report disappeared.
    out.displayedMessageAbsent = tactical.find("{\"mode\":\"map\",") == 0;
    if (Observer::agentWidgetUncovered(_btnEndTurn, _surfaces))
    {
        out.actions.push_back(Observer::AgentUiAction("end_turn", "End turn", "visible_control_semantics"));
        Observer::agentIncludeWidget(out, _btnEndTurn);
    }
    if (Observer::agentWidgetVisible(_btnNextSoldier))
    {
        out.actions.push_back(Observer::AgentUiAction("next_soldier", "Next soldier", "visible_control_semantics"));
        // Previous is the existing keyboard action on this same shown control.
        out.actions.push_back(Observer::AgentUiAction("previous_soldier", "Previous soldier", "visible_control_semantics"));
    }
    if (stance)
        out.actions.push_back(Observer::AgentUiAction(_agentShownKneeled ? "stand" : "kneel",
            _agentShownKneeled ? "Stand" : "Kneel", "visible_control_semantics"));
    if (Observer::agentWidgetUncovered(_btnInventory, _surfaces))
    {
        out.actions.push_back(Observer::AgentUiAction("open_inventory", "Inventory", "visible_control_semantics"));
        Observer::agentIncludeWidget(out, _btnInventory);
    }
    if (Observer::agentWidgetUncovered(_btnHelp, _surfaces))
    {
        out.actions.push_back(Observer::AgentUiAction("open_pause_menu", "Options", "visible_control_semantics"));
        Observer::agentIncludeWidget(out, _btnHelp);
    }
    // Capture private freshness generations at observation time. Normal init
    // may reapply reserve settings after it last painted the HUD; those writes
    // must not make every future fresh observation permanently stale.
    BattleUnit *currentActor = _save->getSelectedUnit();
    out.actor = currentActor; out.save = _save;
    out.actorRevision = currentActor ? currentActor->stanceControlRevision() : 0;
    out.context.push_back(currentActor ? currentActor->turnControlPoseRevision() : 0);
    out.selectionRevision = _save->stanceControlRevision();
    out.uiRevision = _agentUiRevision;
    const Surface *regions[] = {_txtName, _numTimeUnits, _numEnergy, _numHealth, _numMorale, _btnNextSoldier, _btnKneel};
    for (const Surface *widget : regions) Observer::agentIncludeWidget(out, widget);
    return true;
}
bool BattlescapeState::agentBindingCurrent(const Observer::AgentUiState &binding) const
{
    // Validation only. These private identities/revisions never reach the wire,
    // action list, status reason or success claims. Detect away-and-back changes.
    BattleUnit *actor = _save->getSelectedUnit();
    return binding.save == _save && binding.actor == actor && actor == _agentShownActor &&
        (!actor || actor->isKneeled() == _agentShownKneeled) &&
        binding.selectionRevision == _save->stanceControlRevision() &&
        binding.actorRevision == (actor ? actor->stanceControlRevision() : 0);
}
bool BattlescapeState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState shown;
    if (!agentUiState(shown) || !shown.offers(request.action)) return false;
    if (agentViewportDispatch(request, shown)) return true;
    if (request.action == "reserve_none" || request.action == "reserve_snap" ||
        request.action == "reserve_aimed" || request.action == "reserve_auto")
    {
        SDL_Event event = {};
        event.type = SDL_MOUSEBUTTONDOWN;
        event.button.button = SDL_BUTTON_LEFT;
        Action action(&event, 1.0, 1.0, 0, 0);
        action.setSender(request.action == "reserve_none" ? _btnReserveNone :
            request.action == "reserve_snap" ? _btnReserveSnap :
            request.action == "reserve_aimed" ? _btnReserveAimed : _btnReserveAuto);
        btnReserveClick(&action);
        return true;
    }
    if (request.action == "toggle_kneel_reserve")
    {
        SDL_Event event = {};
        event.type = SDL_MOUSEBUTTONDOWN;
        event.button.button = SDL_BUTTON_LEFT;
        Action action(&event, 1.0, 1.0, 0, 0);
        action.setSender(_btnReserveKneel);
        btnReserveKneelClick(&action);
        return true;
    }
    if (request.action == "map_primary" || request.action == "map_secondary")
    {
        const unsigned cell = static_cast<unsigned>(std::stoul(request.choice.substr(1)));
        if (cell >= shown.points.size()) return false;
        const bool tracedPosition = shown.mapTargetPositions.size() == shown.points.size();
        const std::array<int,3> expected = tracedPosition ? shown.mapTargetPositions[cell] : std::array<int,3>{{0,0,0}};
        Observer::AgentMapInputTrace trace(_agentMapInputTrace, cell, tracedPosition, expected[0], expected[1], expected[2],
            shown.points[cell].first, shown.points[cell].second, _map->getX(), _map->getY());
        // Shown points are canvas-absolute; native selector input is map-relative.
        _map->setSelectorPosition(shown.points[cell].first - _map->getX(), shown.points[cell].second - _map->getY());
        const Uint8 button = request.action == "map_primary" ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT;
        // The fresh admitted target already proves viewport routing. The parked
        // human pointer's HUD hover latch is not authority for this target.
        if (!cancelMapAction(button)) activateMapButton(button);
        else trace.emit("cancelled");
        return true;
    }
    if (request.action == "center_enemy")
    {
        const unsigned index = static_cast<unsigned>(std::stoul(request.choice.substr(1)));
        if (index >= shown.rows.size()) return false;
        centerVisibleUnit(shown.rows[index]); return true;
    }
    if (request.action == "center_selected_unit") { btnCenterClick(nullptr); return true; }
    if (request.action == "open_hand_action_menu")
    {
        const std::string field = std::string("\"hand\":\"") + request.choice + "\"";
        if (shown.visible.find(field) == std::string::npos) return false;
        if (request.choice == "left") btnLeftHandItemClick(nullptr); else btnRightHandItemClick(nullptr);
        return true;
    }
    return agentDispatch(request.action);
}
bool BattlescapeState::agentDispatch(const std::string &action)
{
    if (action == "next_soldier") btnNextSoldierClick(0);
    else if (action == "previous_soldier") btnPrevSoldierClick(0);
    else if ((action == "kneel" && !_agentShownKneeled) || (action == "stand" && _agentShownKneeled)) btnKneelClick(0);
    else if (action == "open_pause_menu") btnHelpClick(0);
    else if (action == "open_inventory") btnInventoryClick(0);
    else if (action == "end_turn") btnEndTurnClick(0);
    else return false;
    return true;
}
}
#endif
