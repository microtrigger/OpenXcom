// SPDX-License-Identifier: GPL-3.0-or-later
// Off-screen stock-font widgets. The focused runner substitutes only State's
// Game-dependent constructor with a zero-cursor fixture; no Game/window exists.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Engine/Unicode.h"
#include "../../src/Interface/ComboBox.h"
#include "../../src/Interface/TextList.h"
#include "../../src/Observer/AgentUi.h"
#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <algorithm>
#include <iostream>
using namespace OpenXcom;
namespace {
unsigned checks = 0;
void check(bool condition, const char *why)
{
    ++checks;
    if (!condition) { std::cerr << "FAIL " << why << '\n'; std::exit(1); }
}
class FixtureState : public State
{
public:
    int changes = 0;
    void change(Action *) { ++changes; }
    InteractiveSurface *modal() const { return _modal; }
};
void hover(TextList &list, int localY, State *state)
{
    SDL_Event event = {}; event.type = SDL_MOUSEMOTION;
    Action action(&event, 1, 1, 0, 0);
    action.setMouseAction(list.getX() + 3, list.getY() + localY, list.getX(), list.getY());
    list.mouseOver(&action, state);
}
}
int main()
{
    check(SDL_Init(0) == 0 && !SDL_GetVideoSurface(), "no-window initialization");
    Options::baseXResolution = 320; Options::baseYResolution = 200;
    Font big, small;
    for (const auto &font : YAML::LoadFile("Language/Font.dat")["fonts"])
    {
        if (font["id"].as<std::string>() == "FONT_BIG") big.load(font);
        if (font["id"].as<std::string>() == "FONT_SMALL") small.load(font);
    }
    Language language; Surface canvas(320, 200), selectorBackground(320, 200); FixtureState state;
    SDL_Color palette[256] = {};
    for (int i = 0; i < 256; ++i) palette[i].r = palette[i].g = palette[i].b = static_cast<Uint8>(i);
    canvas.setPalette(palette); selectorBackground.setPalette(palette);
    selectorBackground.drawRect(0, 0, 320, 200, 40);
    std::vector<AgentListRow> rows;
    const Surface *up = nullptr, *down = nullptr;
    TextList list(120, 32, 10, 10);
    list.initText(&big, &small, &language); list.setPalette(palette); list.setColumns(2, 60, 60);
    list.setSelectable(true); list.setBackground(&selectorBackground);
    list.addRow(2, "ONE", "1"); list.addRow(2, "TWO", "2");
    list.addRow(2, "THREE", "3"); list.addRow(2, "SECRET OFFSCREEN", "4"); list.addRow(2, "FIVE", "5");
    check(!list.agentRows(rows), "unpainted list unavailable");
    list.blit(&canvas);
    check(list.agentRows(rows) && rows.size() == 3 && rows[0].cells[0] == "ONE" && rows[2].cells[1] == "3", "complete painted rows only");
    check(rows[0].logicalIndex == 0 && !rows[0].selected, "initial logical binding private; no unpainted selector");
    list.agentScrollControls(up, down);
    check(up && down, "both actual arrows exposed at boundary");
    const auto start = list.agentListRevision();
    list.scrollUp(false, false); list.blit(&canvas);
    check(list.agentListRevision() == start, "boundary no-op retains semantic revision");
    list.invalidate(); list.blit(&canvas);
    check(list.agentListRevision() == start, "redraw does not advance semantic revision");
    list.scrollDown(false, false);
    check(!list.agentRows(rows), "scroll waits for blit");
    list.blit(&canvas);
    check(list.agentRows(rows) && !rows.empty() && rows.front().logicalIndex == 1, "single native scroll advances one physical row");
    list.scrollUp(false, false); list.blit(&canvas);
    check(list.agentListRevision() == start + 2, "away/back scroll binding changes");
    const auto beforeContent = list.agentListRevision();
    list.setCellText(0, 0, "OTHER"); list.setCellText(0, 0, "ONE"); list.blit(&canvas);
    check(list.agentListRevision() == beforeContent + 2, "away/back content binding changes");
    hover(list, 2, &state);
    check(!list.agentRows(rows), "unpainted hover selector unavailable");
    selectorBackground.blit(&canvas); list.blit(&canvas);
    check(list.agentRows(rows) && rows[0].selected, "painted selector selects visible row");
    const auto selected = list.agentListRevision();
    hover(list, 2, &state); selectorBackground.blit(&canvas); list.blit(&canvas);
    check(list.agentRows(rows) && rows[0].selected, "normal background repaint preserves visible selector highlight");
    check(list.agentListRevision() == selected, "same hover does not invalidate grants every frame");
    hover(list, 10, &state); hover(list, 2, &state); list.blit(&canvas);
    check(list.agentListRevision() == selected + 2, "away/back hover invalidates private binding");
    list.mouseOut(nullptr, &state); list.blit(&canvas);
    check(list.agentRows(rows) && !rows[0].selected && list.agentListRevision() > selected, "selector disappearance is captured");
    list.setVisible(false);
    check(!list.agentRows(rows), "hidden list unavailable");
    list.setVisible(true); list.blit(&canvas);
    list.setCellText(0, 0, "A\xF0\x9F\x8C\x8C"); list.blit(&canvas); list.agentRows(rows);
    check(rows.size() == 2 && rows.front().logicalIndex == 1, "missing glyph row omitted");
    list.setCellText(0, 0, std::string(80, 'W')); list.blit(&canvas); list.agentRows(rows);
    check(rows.size() == 2, "overflow original text omitted");
    Surface transparentBackground(320, 200); transparentBackground.setPalette(palette);
    TextList transparentSelector(100, 16, 160, 170); transparentSelector.initText(&big, &small, &language);
    transparentSelector.setPalette(palette); transparentSelector.setColumns(1, 100); transparentSelector.setSelectable(true); transparentSelector.setBackground(&transparentBackground);
    transparentSelector.addRow(1, "VISIBLE TEXT"); transparentSelector.blit(&canvas);
    hover(transparentSelector, 2, &state); transparentSelector.blit(&canvas);
    check(transparentSelector.agentRows(rows) && rows.size() == 1 && !rows[0].selected, "transparent selector cannot advertise a selected highlight");
    Surface underTextBackground(320, 200); underTextBackground.setPalette(palette); bool foundPaintedGlyph = false;
    for (int y = 0; y < 8 && !foundPaintedGlyph; ++y)
        for (int x = 0; x < transparentSelector.getWidth(); ++x)
            if (transparentSelector.getPixel(x, y) != 0)
            {
                underTextBackground.drawRect(transparentSelector.getX() + x, transparentSelector.getY() + y, 1, 1, 40);
                foundPaintedGlyph = true; break;
            }
    check(foundPaintedGlyph, "selector survival fixture has actual later text ink");
    transparentSelector.setBackground(&underTextBackground); hover(transparentSelector, 2, &state); transparentSelector.blit(&canvas);
    check(transparentSelector.agentRows(rows) && rows.size() == 1 && !rows[0].selected, "selector entirely covered by later text cannot advertise highlight");

    SDL_Color equalPalette[256]; std::copy(palette, palette + 256, equalPalette);
    equalPalette[32] = equalPalette[40]; // Different indices, identical actual RGB.
    Surface equalCanvas(320, 200), equalBackground(320, 200); equalCanvas.setPalette(equalPalette); equalBackground.setPalette(equalPalette);
    equalBackground.drawRect(0, 0, 320, 200, 40); equalBackground.blit(&equalCanvas);
    TextList equalSelector(100, 16, 10, 10); equalSelector.initText(&big, &small, &language); equalSelector.setPalette(equalPalette);
    equalSelector.setColumns(1, 100); equalSelector.setSelectable(true); equalSelector.setBackground(&equalBackground);
    equalSelector.addRow(1, "VISIBLE TEXT"); equalSelector.blit(&equalCanvas); hover(equalSelector, 2, &state);
    equalBackground.blit(&equalCanvas); equalSelector.blit(&equalCanvas);
    check(equalSelector.agentRows(rows) && rows.size() == 1 && !rows[0].selected, "different selector index with identical RGB is not a visible highlight");
    TextList partial(120, 20, 10, 40); partial.initText(&big, &small, &language); partial.setColumns(1, 120);
    partial.addRow(1, "A"); partial.addRow(1, "B"); partial.addRow(1, "C"); partial.blit(&canvas);
    check(partial.agentRows(rows) && rows.size() == 2, "partial bottom row omitted");
    TextList wrapped(25, 24, 10, 70); wrapped.initText(&big, &small, &language); wrapped.setColumns(1, 25); wrapped.setWordWrap(true);
    wrapped.addRow(1, "AAAA BBBB CCCC"); wrapped.addRow(1, "D"); wrapped.addRow(1, "E"); wrapped.blit(&canvas);
    check(wrapped.agentRows(rows) && rows.empty(), "wrapped top row omitted without leaking its remaining lines");
    wrapped.scrollDown(false, false); wrapped.blit(&canvas); wrapped.agentRows(rows);
    for (const auto &row : rows) check(row.logicalIndex != 0, "partial wrapped row remains omitted after scroll");
    TextList funding(260, 24, 32, 120); funding.initText(&big, &small, &language); funding.setColumns(3, 108, 100, 52); funding.setDot(true);
    funding.addRow(3, "USA", "\x01$100\x01", "$0"); funding.addRow(2, "TOTAL", "$100"); funding.blit(&canvas);
    // Decorative leaders are excluded only through TextList's tagged original
    // range, proven glyph-by-glyph during the ordinary Text::draw traversal.
    check(funding.agentRows(rows) && !rows.empty(), "complete main text survives clipped decorative leaders");
    for (const auto &row : rows)
    {
        std::cerr << "FUNDING_TWO_ROW logical=" << row.logicalIndex;
        for (const auto &cell : row.cells) std::cerr << " [" << cell << ']';
        std::cerr << '\n';
    }
    TextList dottedSingle(120, 16, 10, 70); dottedSingle.initText(&big, &small, &language); dottedSingle.setPalette(palette);
    dottedSingle.setColumns(2, 80, 40); dottedSingle.setDot(true); dottedSingle.addRow(2, "USA.", "100"); dottedSingle.blit(&canvas);
    check(dottedSingle.agentRows(rows) && rows.size() == 1 && rows[0].cells[0] == "USA." && rows[0].cells[1] == "100", "literal original trailing dot retained while introduced leaders excluded");
    TextList dottedClipped(30, 16, 10, 70); dottedClipped.initText(&big, &small, &language); dottedClipped.setColumns(2, 10, 20); dottedClipped.setDot(true);
    dottedClipped.addRow(2, "TOO WIDE", "1"); dottedClipped.blit(&canvas);
    check(dottedClipped.agentRows(rows) && rows.empty(), "tagged main text genuinely clipped still refused");
    TextList dottedMissing(120, 16, 10, 70); dottedMissing.initText(&big, &small, &language); dottedMissing.setColumns(2, 80, 40); dottedMissing.setDot(true);
    dottedMissing.addRow(2, "A\xF0\x9F\x8C\x8C", "1"); dottedMissing.blit(&canvas);
    check(dottedMissing.agentRows(rows) && rows.empty(), "tagged missing main glyph still refused");
    TextList dottedWrapped(80, 32, 10, 70); dottedWrapped.initText(&big, &small, &language); dottedWrapped.setColumns(2, 25, 55); dottedWrapped.setDot(true); dottedWrapped.setWordWrap(true);
    dottedWrapped.addRow(2, "AAAA BBBB CCCC", "1"); dottedWrapped.blit(&canvas);
    check(dottedWrapped.agentRows(rows) && rows.empty(), "tagged wrapped main text still refused");
    TextList dottedPrepend(120, 16, 10, 70); dottedPrepend.initText(&big, &small, &language); dottedPrepend.setColumns(2, 80, 40); dottedPrepend.setAlign(ALIGN_RIGHT, 0); dottedPrepend.setDot(true);
    dottedPrepend.addRow(2, "USA.", "100"); dottedPrepend.blit(&canvas);
    check(dottedPrepend.agentRows(rows) && rows.size() == 1 && rows[0].cells[0] == "USA.", "prepended generated leaders excluded using exact original range");
    dottedSingle.setCellText(0, 0, std::string(80, '.')); dottedSingle.blit(&canvas);
    check(dottedSingle.agentRows(rows) && rows.empty(), "explicit cell replacement clears old leader provenance and refuses clipped literal text");

    const int fundingPitch = small.getHeight() + small.getSpacing();
    Text firstCurrency(100, small.getHeight(), 0, 0), nextCurrency(100, small.getHeight(), 0, fundingPitch);
    firstCurrency.initText(&big, &small, &language); nextCurrency.initText(&big, &small, &language);
    firstCurrency.setPalette(palette); nextCurrency.setPalette(palette); firstCurrency.setText("$100"); nextCurrency.setText("$100");
    firstCurrency.blit(&canvas); nextCurrency.blit(&canvas); SDL_Rect currencyInk = {}, dollarInk = {};
    check(firstCurrency.agentPaintedBounds(currencyInk) && small.agentGlyphInk('$', dollarInk), "funding overlap diagnostic uses actual native glyph ink");
    unsigned sharedPixels = 0, changedPixels = 0;
    for (int y = fundingPitch; y < firstCurrency.getHeight(); ++y)
        for (int x = 0; x < firstCurrency.getWidth(); ++x)
        {
            const Uint8 earlier = firstCurrency.getPixel(x, y), later = nextCurrency.getPixel(x, y - fundingPitch);
            if (earlier && later) { ++sharedPixels; if (earlier != later) ++changedPixels; }
        }
    std::cerr << "FUNDING_CURRENCY_OVERLAP row_pitch=" << fundingPitch << " font_height=" << small.getHeight()
        << " dollar_ink=" << dollarInk.x << ',' << dollarInk.y << ',' << dollarInk.w << ',' << dollarInk.h
        << " amount_ink=" << currencyInk.x << ',' << currencyInk.y << ',' << currencyInk.w << ',' << currencyInk.h
        << " actual_shared_nontransparent_pixels=" << sharedPixels << " actual_changed_palette_pixels=" << changedPixels << '\n';
    TextList funding17(260, 136, 32, 40); funding17.initText(&big, &small, &language); funding17.setColumns(3, 108, 100, 52); funding17.setDot(true);
    const char *countries[] = {"USA", "RUSSIA", "UK", "FRANCE", "GERMANY", "ITALY", "SPAIN", "CHINA", "JAPAN", "INDIA", "BRAZIL", "AUSTRALIA", "NIGERIA", "SOUTH AFRICA", "EGYPT", "CANADA"};
    for (const char *country : countries) funding17.addRow(3, country, "\x01$100\x01", "$0");
    funding17.addRow(2, "TOTAL", "$1600"); funding17.blit(&canvas);
    check(funding17.agentRows(rows), "realistic seventeen-row funding shape projection is available");
    std::cerr << "FUNDING_17_DRAW_ROWS count=" << rows.size() << '\n';
    for (const auto &row : rows)
    {
        std::cerr << "FUNDING_17 logical=" << row.logicalIndex;
        for (const auto &cell : row.cells) std::cerr << " [" << cell << ']';
        for (const auto &ink : row.regions) std::cerr << " ink=" << ink.x << ',' << ink.y << ',' << ink.w << ',' << ink.h;
        std::cerr << '\n';
    }
    Observer::AgentUiState fundingUi; Observer::AgentListProjection fundingProjection;
    check(Observer::agentProjectList(&funding17, &funding17, Observer::agentFlattenPaintOrder({&funding17}), 3, false, fundingUi, fundingProjection), "funding-shaped main rows pass actual provider projection");
    std::cerr << "FUNDING_17_PUBLIC rows=" << fundingProjection.rows << '\n';
    TextList completeColumns(260, 24, 32, 120); completeColumns.initText(&big, &small, &language);
    completeColumns.setColumns(3, 108, 100, 52); completeColumns.setPalette(palette);
    completeColumns.addRow(3, "USA", "\x01" "100" "\x01", "0"); completeColumns.addRow(2, "TOTAL", "100"); completeColumns.blit(&canvas);
    check(completeColumns.agentRows(rows) && rows.size() == 2, "complete three-column rows project");
    check(rows[0].cells.size() == 3 && rows[0].cells[1] == "100", "nonpainted color token stripped from complete cell");
    check(rows[1].cells.size() == 3 && rows[1].cells[2].empty(), "missing TOTAL column is visible blank");

    TextList projected(100, 32, 10, 100); projected.initText(&big, &small, &language); projected.setColumns(1, 100);
    for (unsigned i = 0; i != 6; ++i) projected.addRow(1, "SAME");
    projected.blit(&canvas); projected.agentRows(rows);
    check(rows.size() == 4 && !rows[0].regions.empty(), "projection fixture has four complete ink rows");
    const SDL_Rect covered = rows[0].regions.front();
    Surface overlay(covered.w, covered.h, covered.x, covered.y); overlay.blit(&canvas);
    std::vector<Surface *> order = Observer::agentFlattenPaintOrder({&projected, &overlay});
    Observer::AgentUiState ui; Observer::AgentListProjection projection;
    check(Observer::agentProjectList(&projected, &projected, order, 1, true, ui, projection), "actual owner-order projection works");
    check(ui.rows.size() == 3 && ui.rows.front() == 1 && projection.rows.find("\"handle\":\"r0\"") != std::string::npos, "covered first row omitted with fresh r0 logical binding");
    check(projection.rows.find("logical") == std::string::npos && projection.rows.find("total") == std::string::npos, "private identities and totals never serialized");
    overlay.setVisible(false); ui = Observer::AgentUiState();
    check(Observer::agentProjectList(&projected, &projected, order, 1, true, ui, projection) && ui.rows.front() == 0 && projection.up && projection.down, "uncovered boundary arrows and first row restored");
    const Observer::AgentUiState previousUi = ui;
    projected.scrollDown(false, false); projected.blit(&canvas); ui = Observer::AgentUiState();
    check(Observer::agentProjectList(&projected, &projected, order, 1, true, ui, projection) && ui.rows.front() == 1 && !ui.sameBinding(previousUi), "identical labels on another logical page invalidate private bindings");
    projected.agentScrollControls(up, down); check(up && down, "projection arrows available before hide fixture");
    const_cast<Surface *>(up)->setVisible(false); ui = Observer::AgentUiState();
    check(Observer::agentProjectList(&projected, &projected, order, 1, true, ui, projection) && !projection.up && projection.down, "hidden arrow withheld");
    projected.setX(-1); projected.blit(&canvas); ui = Observer::AgentUiState();
    check(!Observer::agentProjectList(&projected, &projected, order, 1, true, ui, projection), "viewport-clipped list refused");
    Options::baseYResolution = 400; Surface tallCanvas(320, 400);
    TextList tall(100, 280, 10, 10); tall.initText(&big, &small, &language); tall.setColumns(1, 100);
    for (unsigned i = 0; i != 40; ++i) tall.addRow(1, "ROW");
    tall.blit(&tallCanvas); order = Observer::agentFlattenPaintOrder({&tall}); ui = Observer::AgentUiState();
    check(Observer::agentProjectList(&tall, &tall, order, 1, true, ui, projection) && ui.rows.size() == 32 && projection.rows.find("r32") == std::string::npos, "public row slots bounded to 32");
    Options::baseYResolution = 200;

    ComboBox combo(&state, 100, 16, 160, 10); combo.initText(&big, &small, &language); combo.setPalette(palette); combo.setBackground(&canvas);
    combo.setOptions({"ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN", "TWELVE"});
    combo.onChange((ActionHandler)&FixtureState::change); combo.blit(&canvas);
    check(!combo.agentPopupOpen() && combo.agentLabel() == "ONE", "closed combo only paints selected label");
    std::vector<Surface *> toggleOrder = Observer::agentFlattenPaintOrder({&combo});
    check(combo.agentToggleUncovered(toggleOrder), "known button-arrow composite toggle is fully uncovered");
    Surface toggleOverlay(2, 2, combo.getX() + 1, combo.getY() + 1); toggleOverlay.blit(&canvas);
    toggleOrder = Observer::agentFlattenPaintOrder({&combo, &toggleOverlay});
    check(!combo.agentToggleUncovered(toggleOrder), "later sibling covers main combo toggle");
    toggleOrder = Observer::agentFlattenPaintOrder({&toggleOverlay, &combo});
    check(combo.agentToggleUncovered(toggleOrder), "earlier sibling does not cover combo toggle");
    combo.setWidth(80); combo.blit(&canvas); toggleOrder = Observer::agentFlattenPaintOrder({&combo});
    check(!combo.agentToggleUncovered(toggleOrder), "resized root cannot hide larger internal button geometry");
    combo.setWidth(100); combo.blit(&canvas);

    check(combo.agentOpen() && combo.agentPopupOpen() && state.modal() == &combo && state.changes == 0, "shared open makes actual popup modal without callback");
    combo.blit(&canvas);
    const int ordinaryListY = combo.agentList()->getY();
    combo.agentList()->setY(combo.getY()); combo.blit(&canvas);
    check(!combo.agentToggleUncovered(Observer::agentFlattenPaintOrder({&combo})), "actual own later popup list occludes toggle when overlapping");
    combo.agentList()->setY(ordinaryListY); combo.blit(&canvas);
    const auto popupRev = combo.agentUiRevision();
    combo.blit(&canvas);
    check(combo.agentUiRevision() == popupRev, "combo per-frame list invalidation does not stale grants");
    check(combo.agentList()->agentRows(rows) && rows.size() == 10, "dropdown returns only ten actual visible options");
    check(!combo.agentChooseVisibleRow(11, nullptr, &state) && state.changes == 0 && combo.agentPopupOpen(), "offscreen logical index is not a usable row handle");
    check(combo.agentChooseVisibleRow(1, nullptr, &state) && combo.getSelected() == 1 && !combo.agentPopupOpen() && !state.modal() && state.changes == 1, "visible semantic choice uses shared selection/activation and flushes once");
    combo.blit(&canvas); check(combo.agentLabel() == "TWO", "chosen closed label is actual rendered label");
    SDL_Event neutral = {}; neutral.type = SDL_USEREVENT; Action action(&neutral, 1, 1, 0, 0);
    combo.handle(&action, &state);
    check(state.changes == 1, "next ordinary event cannot replay deferred callback");
    const auto beforeOpenClose = combo.agentComboRevision();
    check(combo.agentOpen(), "reopen for close-without-choice"); combo.blit(&canvas);
    check(combo.agentClose(nullptr, &state) && state.changes == 2 && combo.getSelected() == 1, "close without choice preserves normal exactly-once onChange");
    combo.handle(&action, &state); check(state.changes == 2, "close callback is not deferred twice");
    check(combo.agentComboRevision() == beforeOpenClose + 2, "open/close away-back changes private combo revision");
    combo.blit(&canvas); const auto beforeChoice = combo.agentComboRevision();
    combo.setSelected(0); combo.setSelected(1); combo.blit(&canvas);
    check(combo.agentComboRevision() == beforeChoice + 2, "selection away/back changes private combo revision");
    combo.toggle(); combo.blit(&canvas); hover(*combo.agentList(), 2, &state);
    SDL_Event click = {}; click.type = SDL_MOUSEBUTTONUP; click.button.button = SDL_BUTTON_LEFT;
    Action ordinaryClick(&click, 1, 1, 0, 0); combo.agentList()->mouseClick(&ordinaryClick, &state);
    check(combo.getSelected() == combo.agentList()->getScroll() && !combo.agentPopupOpen() && state.changes == 2, "ordinary mouseClick shares activation and defers callback");
    combo.handle(&action, &state); combo.handle(&action, &state);
    check(state.changes == 3, "ordinary deferred callback runs once");
    ComboBox wrappedCombo(&state, 50, 16, 160, 120); wrappedCombo.initText(&big, &small, &language); wrappedCombo.setBackground(&canvas);
    wrappedCombo.agentList()->setWordWrap(true);
    wrappedCombo.setOptions({"AAAA BBBB CCCC", "A", "B", "C", "D"}); wrappedCombo.setSelected(1); wrappedCombo.blit(&canvas);
    check(wrappedCombo.agentOpen(), "short selected label opens prior-wrapped mapping fixture"); wrappedCombo.blit(&canvas);
    check(wrappedCombo.agentList()->agentRows(rows) && rows.empty() && !wrappedCombo.agentChooseVisibleRow(1, nullptr, &state), "unwrapped targets after wrapped option are omitted when ordinary physical index differs");
    check(Observer::agentPaintHealthy(), "all real offscreen paints succeeded");
    check(!SDL_GetVideoSurface() && !SDL_WasInit(SDL_INIT_VIDEO), "no GUI initialized");
    std::cout << "AgentListWidgetTest PASS " << checks << " checks\n";
}
