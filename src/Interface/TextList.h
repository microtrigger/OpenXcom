#pragma once
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
#include <vector>
#include <map>
#include "../Engine/InteractiveSurface.h"
#include "Text.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include <cstdint>
#endif

namespace OpenXcom
{

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
// A painted row. logicalIndex and geometry are private grant bindings,
// never protocol fields; the owning state allocates its own grant-local rN slots.
struct AgentPurchaseCell
{
	// Private expected RGB at the opaque pixels of this already drawn Text.
	// Used only while the ordinary list draw is in progress, never serialized.
	struct Pixel { Sint16 x, y; Uint8 r, g, b; };
	bool known = false;
	std::string text;
	std::vector<SDL_Rect> regions;
	std::vector<Pixel> ink;
};
struct AgentPurchaseRow
{
	size_t logicalIndex;
	AgentPurchaseCell cells[4];
	SDL_Rect bounds;
	bool selected = false;
	const Surface *increase = nullptr, *decrease = nullptr;
};
// Explicit arrivals-only readout; no row activation or hidden identity export.
struct AgentArrivalRow
{
	size_t logicalIndex;
	AgentPurchaseCell cells[3];
	SDL_Rect bounds;
};
// Explicit personnel/equipment opt-in. No model identity is serialized.
struct AgentOutfittingRow
{
	size_t logicalIndex;
	std::vector<AgentPurchaseCell> cells;
	std::vector<SDL_Rect> controlInk;
	SDL_Rect bounds, clickBand;
	bool selected = false;
	const Surface *increase = nullptr, *decrease = nullptr;
};
struct AgentListRow
{
	size_t logicalIndex;
	std::vector<std::string> cells;
	bool selected;
	bool completeText = true;
	std::vector<SDL_Rect> labelRegions;
	// Private opaque ink runs; dotted-content bounds remain conservative.
	std::vector<SDL_Rect> regions;
	SDL_Rect bounds;
};
#endif

enum ArrowOrientation { ARROW_VERTICAL, ARROW_HORIZONTAL };

class ArrowButton;
class ComboBox;
class ScrollBar;

/**
 * List of Text's split into columns.
 * Contains a set of Text's that are automatically lined up by
 * rows and columns, like a big table, making it easy to manage
 * them together.
 */
class TextList : public InteractiveSurface
{
private:
	std::vector< std::vector<Text*> > _texts;
	std::vector<size_t> _columns, _rows;
	Font *_big, *_small, *_font;
	Language *_lang;
	size_t _scroll, _visibleRows, _selRow;
	Uint8 _color, _color2;
	std::map<int, TextHAlign> _align;
	bool _dot, _selectable, _condensed, _contrast, _wrap, _flooding, _fullRowsOnly;
	Surface *_bg, *_selector;
	ArrowButton *_up, *_down;
	ScrollBar *_scrollbar;
	int _margin;
	bool _scrolling;
	std::vector<ArrowButton*> _arrowLeft, _arrowRight;
	int _arrowPos, _scrollPos;
	ArrowOrientation _arrowType;
	ActionHandler _leftClick, _leftPress, _leftRelease, _rightClick, _rightPress, _rightRelease;
	int _arrowsLeftEdge, _arrowsRightEdge;
	ComboBox *_comboBox;
	/// Shared ordinary hover/activation operations; no synthetic input events.
	void selectPhysicalRow(size_t row);
	void activateRow(Action *action, State *state, Uint8 button);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	std::uint64_t _agentListRevision = 0, _agentPaintRevision = UINT64_MAX;
	std::vector<AgentListRow> _agentDrawRows, _agentPaintRows, _agentDrawControls, _agentPaintControls;
	std::vector<Surface *> _agentPaintOrder;
	std::vector<AgentPurchaseRow> _agentDrawPurchaseRows, _agentPaintPurchaseRows;
	std::vector<AgentArrivalRow> _agentDrawArrivalRows, _agentPaintArrivalRows;
	bool _agentArrivalCells = false;
	// ItemsArrivingState-only logical current viewport; no row action authority.
	struct AgentArrivalLogicalCapture
	{
		AgentArrivalRow row;
		const Text *cells[3];
		std::uint64_t textRevisions[3], presentationRevisions[3];
		SDL_Rect geometry[3];
	};
	bool _agentArrivalLogicalCells = false, _agentArrivalLogicalCaptureHealthy = true;
	bool _agentArrivalLogicalEpochExhausted = false;
	std::uint64_t _agentArrivalLogicalDrawEpoch = 0, _agentArrivalLogicalPaintEpoch = 0;
	std::vector<AgentArrivalLogicalCapture> _agentDrawArrivalLogicalRows, _agentPaintArrivalLogicalRows;
	SDL_Rect _agentArrivalLogicalClip = {};
	const Surface *_agentArrivalLogicalDestination = nullptr;
	void agentCaptureArrivalLogicalRow(size_t logicalIndex, int y);

	// AllocatePsiTrainingState-only logical UI readout; independent of ink witnesses.
	struct AgentPsiCapture
	{
		AgentOutfittingRow row;
		const Text *cells[4];
		std::uint64_t textRevisions[4], presentationRevisions[4];
		SDL_Rect geometry[4];
	};
	// CommendationState-only logical viewport, with no selection/action authority.
	struct AgentCommendationCapture
	{
		AgentOutfittingRow row;
		const Text *cells[2];
		std::uint64_t textRevisions[2], presentationRevisions[2];
		SDL_Rect geometry[2];
	};
	bool _agentCommendationCells = false, _agentCommendationCaptureHealthy = true;
	std::vector<AgentCommendationCapture> _agentDrawCommendationRows, _agentPaintCommendationRows;
	SDL_Rect _agentCommendationClip = {};
	const Surface *_agentCommendationDestination = nullptr;
	void agentCaptureCommendationRow(size_t logicalIndex, int y);
	// PromotionsState-only logical viewport, with no selection/action authority.
	struct AgentPromotionCapture
	{
		AgentOutfittingRow row;
		const Text *cells[3];
		std::uint64_t textRevisions[3], presentationRevisions[3];
		SDL_Rect geometry[3];
	};
	bool _agentPromotionCells = false, _agentPromotionCaptureHealthy = true;
	std::vector<AgentPromotionCapture> _agentDrawPromotionRows, _agentPaintPromotionRows;
	SDL_Rect _agentPromotionClip = {};
	const Surface *_agentPromotionDestination = nullptr;
	void agentCapturePromotionRow(size_t logicalIndex, int y);
	// ReportsState-only logical viewport, with no selection/action authority.
	struct AgentReportCapture
	{
		AgentOutfittingRow row;
		const Text *cells[5];
		std::uint64_t textRevisions[5], presentationRevisions[5];
		SDL_Rect geometry[5];
	};
    // ManageAlienContainmentState-only logical four-cell arrow viewport.
    bool _agentContainmentCells = false;
    std::vector<AgentReportCapture> _agentDrawContainmentRows, _agentPaintContainmentRows;
    const Surface *_agentContainmentDestination = nullptr;
    SDL_Rect _agentContainmentClip = {};
    std::uint64_t _agentContainmentDrawEpoch = 0, _agentContainmentEpoch = 0;
    void agentCaptureContainmentRow(size_t logicalIndex, int y);
	unsigned _agentReportColumns = 0;
	bool _agentReportCells = false, _agentReportCaptureHealthy = true, _agentReportEpochExhausted = false;
	std::uint64_t _agentReportDrawEpoch = 0, _agentReportPaintEpoch = 0;
	std::vector<AgentReportCapture> _agentDrawReportRows, _agentPaintReportRows;
	SDL_Rect _agentReportClip = {};
	const Surface *_agentReportDestination = nullptr;
	void agentCaptureReportRow(size_t logicalIndex, int y);
	// Save-list-only logical viewport, with no selection/action authority.
	struct AgentSaveListCapture
	{
		AgentOutfittingRow row;
		const Text *cells[3];
		std::uint64_t textRevisions[3], presentationRevisions[3];
		SDL_Rect geometry[3];
	};
	bool _agentSaveListNewSlot = false;
	bool _agentSaveListCells = false, _agentSaveListCaptureHealthy = true;
	std::vector<AgentSaveListCapture> _agentDrawSaveListRows, _agentPaintSaveListRows;
	SDL_Rect _agentSaveListClip = {};
	const Surface *_agentSaveListDestination = nullptr;
	void agentCaptureSaveListRow(size_t logicalIndex, int y);
	bool _agentPsiTrainingCells = false;
	std::vector<AgentPsiCapture> _agentDrawPsiRows, _agentPaintPsiRows;
	SDL_Rect _agentPsiClip = {};
	const Surface *_agentPsiSelector = nullptr;
	std::uint64_t _agentPsiSelectorRevision = 0;
	void agentCapturePsiRow(size_t logicalIndex, int y);
	unsigned _agentOutfittingColumns = 0;
	bool _agentOutfittingEquipment = false, _agentOutfittingCrew = false;
	std::vector<AgentOutfittingRow> _agentDrawOutfittingRows, _agentPaintOutfittingRows;
	SDL_Rect _agentOutfittingClip = {};
	SDL_Rect _agentListClip = {};
	SDL_Rect _agentArrivalClip = {};
	SDL_Rect _agentPurchaseClip = {};
	bool _agentCoverageHealthy = false;
	size_t _agentCoverageWork = 0, _agentCoverageRuns = 0;
	void agentCaptureRow(size_t logicalIndex, int y);
#endif

	/// Updates the arrow buttons.
	void updateArrows();
	/// Updates the visible rows.
	void updateVisible();
public:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool agentCursorMotionSafe() const override;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	bool agentRows(std::vector<AgentListRow> &rows) const;
	void agentEnableOutfitting(unsigned columns, bool equipment = false, bool crew = false);
    // BuildFacilitiesState-only single-cell paint proof. Legacy lists unchanged.
    void agentEnableFacilityCells();
    bool agentFacilityRows(const Surface *destination, std::vector<AgentOutfittingRow> &rows) const;
    bool agentSelectFacilityRow(const Surface *destination, size_t logicalIndex);
	// Narrow opt-in: current four UI cells, not glyph completeness or model data.
	void agentEnableCommendationCells();
	bool agentCommendationRows(std::vector<AgentOutfittingRow> &rows) const;
	void agentEnableSaveListCells(bool newSlot);
	bool agentSaveListRows(std::vector<AgentOutfittingRow> &rows) const;
	bool agentSelectSaveListRow(size_t logicalIndex);
    void agentEnableContainmentCells();
    bool agentContainmentRows(const Surface *destination, std::vector<AgentOutfittingRow> &rows, std::uint64_t &epoch) const;
    bool agentContainmentArrowClick(const Surface *destination, size_t logicalIndex, bool increase, State *state);
	void agentEnableReportCells(unsigned columns);
	bool agentReportRows(std::vector<AgentOutfittingRow> &rows, std::uint64_t &captureEpoch) const;
	void agentEnablePromotionCells();
	bool agentPromotionRows(std::vector<AgentOutfittingRow> &rows) const;
	void agentEnablePsiTrainingCells();
	bool agentPsiTrainingRows(std::vector<AgentOutfittingRow> &rows) const;
	bool agentSelectPsiTrainingRow(size_t logicalIndex);
	bool agentOutfittingRows(std::vector<AgentOutfittingRow> &rows) const;
	bool agentSelectOutfittingRow(size_t logicalIndex);
	// Explicitly enabled only by ItemsArrivingState before ordinary painting.
	void agentEnableArrivalLogicalCells();
	bool agentArrivalLogicalRows(std::vector<AgentArrivalRow> &rows, std::uint64_t &captureEpoch) const;
	void agentEnableArrivalCells();
	bool agentArrivalRows(std::vector<AgentArrivalRow> &rows) const;
	// Purchase-only, independent text witnesses and exact painted row arrows.
	bool agentPurchaseRows(std::vector<AgentPurchaseRow> &rows) const;
	ScrollBar *agentPurchaseScrollbar() const;
	// Explicit opt-in for new-research controls; text proof remains separate.
	bool agentControlRows(std::vector<AgentListRow> &rows) const;
	std::uint64_t agentListRevision() const { return _agentListRevision; }
	void agentScrollControls(const Surface *&up, const Surface *&down) const;
	bool agentChooseVisibleRow(size_t logicalIndex, Action *action, State *state);
	// Selection only for explicitly audited project lists; never fires callbacks.
	bool agentSelectProjectRow(size_t logicalIndex, bool allowUnknownText = false);
	void agentPaintOrder(std::vector<Surface *> &out) const;
#endif
	/// Creates a text list with the specified size and position.
	TextList(int width, int height, int x = 0, int y = 0);
	/// Cleans up the text list.
	~TextList();
	/// Sets the X position of the surface.
	void setX(int x);
	/// Sets the Y position of the surface.
	void setY(int y);
	/// Gets the arrowsLeftEdge.
	int getArrowsLeftEdge();
	/// Gets the arrowsRightEdge.
	int getArrowsRightEdge();
	/// Unpresses the surface.
	void unpress(State *state);
	/// Sets the text color of a certain cell.
	void setCellColor(size_t row, size_t column, Uint8 color);
	/// Sets the text color of a certain row.
	void setRowColor(size_t row, Uint8 color);
	/// Gets the text of a certain cell.
	std::string getCellText(size_t row, size_t column) const;
	/// Sets the text of a certain cell.
	void setCellText(size_t row, size_t column, const std::string &text);
	/// Gets the X position of a certain column.
	int getColumnX(size_t column) const;
	/// Gets the Y position of a certain row.
	int getRowY(size_t row) const;
	/// Gets the height of the row text in pixels
	int getTextHeight(size_t row) const;
	/// Gets the number of lines in the wrapped text for the specified row
	int getNumTextLines(size_t row) const;
	/// Gets the amount of text in the list.
	size_t getTexts() const;
	/// Gets the amount of rows in the list.
	size_t getRows() const;
	/// Gets the amount of visible rows in the list.
	size_t getVisibleRows() const;
	/// Adds a new row to the text list.
	void addRow(int cols, ...);
	/// Sets the columns in the text list.
	void setColumns(int cols, ...);
	/// Sets the palette of the text list.
	void setPalette(SDL_Color *colors, int firstcolor = 0, int ncolors = 256);
	/// Initializes the resources for the text list.
	void initText(Font *big, Font *small, Language *lang);
	/// Sets the height of the surface.
	void setHeight(int height);
	/// Count only fully fitting font rows when calculating the scroll boundary.
	void setFullRowsOnly(bool fullRowsOnly);
	/// Sets the text color of the text list.
	void setColor(Uint8 color);
	/// Gets the text color of the text list.
	Uint8 getColor() const;
	/// Sets the secondary color of the text list.
	void setSecondaryColor(Uint8 color);
	/// Gets the secondary color of the text list.
	Uint8 getSecondaryColor() const;
	/// Sets the text list's wordwrap setting.
	void setWordWrap(bool wrap);
	/// Sets the text list's high contrast color setting.
	void setHighContrast(bool contrast);
	/// Sets the text horizontal alignment of the text list.
	void setAlign(TextHAlign align, int col = -1);
	/// Sets whether to separate columns with dots.
	void setDot(bool dot);
	/// Sets whether the list is selectable.
	void setSelectable(bool selectable);
	/// Sets the text size to big.
	void setBig();
	/// Sets the text size to small.
	void setSmall();
	/// Sets whether to condense columns instead of a table like layout.
	void setCondensed(bool condensed);
	/// Sets the background for the selector.
	void setBackground(Surface *bg);
	/// Gets the selected row in the list.
	unsigned int getSelectedRow() const;
	/// Sets the margin of the text list.
	void setMargin(int margin);
	/// Gets the margin of the text list.
	int getMargin() const;
	/// Sets the arrow color of the text list.
	void setArrowColor(Uint8 color);
	/// Sets the arrow column of the text list.
	void setArrowColumn(int pos, ArrowOrientation type);
	/// Hooks an action handler to a mouse click on the left arrows.
	void onLeftArrowClick(ActionHandler handler);
	/// Hooks an action handler to a mouse press over the left arrows.
	void onLeftArrowPress(ActionHandler handler);
	/// Hooks an action handler to a mouse release over the left arrows.
	void onLeftArrowRelease(ActionHandler handler);
	/// Hooks an action handler to a mouse click on the right arrows.
	void onRightArrowClick(ActionHandler handler);
	/// Hooks an action handler to a mouse press over the right arrows.
	void onRightArrowPress(ActionHandler handler);
	/// Hooks an action handler to a mouse release over the right arrows.
	void onRightArrowRelease(ActionHandler handler);
	/// Clears the list.
	void clearList();
	/// Scrolls the list up.
	void scrollUp(bool toMax, bool scrollByWheel = false);
	/// Scrolls the list down.
	void scrollDown(bool toMax, bool scrollByWheel = false);
	/// Sets the list scrolling.
	void setScrolling(bool scrolling, int scrollPos = 4);
	/// Draws the text onto the text list.
	void draw();
	/// Blits the text list onto another surface.
	void blit(Surface *surface);
	/// Thinks arrow buttons.
	void think();
	/// Handles arrow buttons.
	void handle(Action *action, State *state);
	/// Special handling for mouse presses.
	void mousePress(Action *action, State *state);
	/// Special handling for mouse releases.
	void mouseRelease(Action *action, State *state);
	/// Special handling for mouse clicks.
	void mouseClick(Action *action, State *state);
	/// Special handling for mouse hovering.
	void mouseOver(Action *action, State *state);
	/// Special handling for mouse hovering out.
	void mouseOut(Action *action, State *state);
	/// get the scroll depth
	size_t getScroll();
	/// set the scroll depth
	void scrollTo(size_t scroll);
	/// Attaches this button to a combobox.
	void setComboBox(ComboBox *comboBox);
	/// Check for a combobox.
	ComboBox *getComboBox() const;
	void setBorderColor(Uint8 color);
	int getScrollbarColor();
	/// Allows the cell to flood into other columns.
	void setFlooding(bool flooding);
};

}
