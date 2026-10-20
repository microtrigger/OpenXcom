// SPDX-License-Identifier: GPL-3.0-or-later
// Off-screen real-widget projection regression. No Game, window or GUI is created.
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/NumberText.h"
#include "../../src/Observer/AgentUi.h"
#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <iostream>
using namespace OpenXcom;
namespace {
unsigned checks = 0;
void check(bool value, const char *message)
{
    ++checks;
    if (!value) { std::cerr << "FAIL " << message << '\n'; std::exit(1); }
}
}
int main()
{
    check(SDL_Init(0) == 0, "SDL eventless initialization (no video/audio)");
    check(SDL_GetVideoSurface() == 0, "no window or display surface");
    Options::baseXResolution = 320; Options::baseYResolution = 200;
    const YAML::Node data = YAML::LoadFile("Language/Font.dat");
    Font big, small;
    for (const auto &font : data["fonts"])
    {
        if (font["id"].as<std::string>() == "FONT_BIG") big.load(font);
        if (font["id"].as<std::string>() == "FONT_SMALL") small.load(font);
    }
    check(small.getSpacing() == -1, "real stock negative-spacing font");
    Language language;
    Surface canvas(320, 200);
    TextButton button(92, 20, 164, 90);
    button.initText(&big, &small, &language); button.setText("NEW BATTLE"); button.blit(&canvas);
    check(button.agentLabel() == "NEW BATTLE", "stock main-menu label projects");
    check(Observer::agentButtonLabel(&button) == "NEW BATTLE", "public helper preserves real label");
    button.setVisible(false);
    check(button.agentLabel().empty(), "hidden button exposes no action label");
    button.setVisible(true); button.setText(std::string(200, 'W')); button.blit(&canvas);
    check(button.agentLabel().empty(), "clipped button rejects underlying long text");
    button.setText("A\xF0\x9F\x8C\x8C"); button.blit(&canvas);
    check(button.agentLabel().empty(), "missing emoji glyph does not leak original label");
    button.setText("NEW BATTLE"); button.blit(&canvas);
    check(button.agentLabel() == "NEW BATTLE", "new complete render restores ordinary label");
    Text name(136, 10, 135, 176);
    name.initText(&big, &small, &language); name.setText("Ada"); name.blit(&canvas);
    check(Observer::agentTextJson(&name) == "\"Ada\"", "visible text projects");
    name.setText("Ada\xF0\x9F\x8C\x8C"); name.blit(&canvas);
    check(Observer::agentTextJson(&name) == "null", "missing glyph never exports original soldier-name text");
    name.setText(std::string(200, 'W')); name.blit(&canvas);
    check(Observer::agentTextJson(&name) == "null", "clipped name omitted");
    NumberText number(15, 5, 136, 186);
    number.setValue(9999); number.blit(&canvas);
    check(number.agentValueFullyVisible(), "four digits fit 15px HUD");
    number.setValue(10000); number.blit(&canvas);
    check(!number.agentValueFullyVisible(), "five digits not exported from 15px HUD");
    TextButton cancel(80, 16, 20, 20);
    cancel.initText(&big, &small, &language); cancel.setText("CANCEL"); cancel.blit(&canvas);
    Surface background(320, 200), overlay(10, 10, 25, 22);
    background.blit(&canvas); overlay.blit(&canvas);
    std::vector<Surface *> order = {&background, &cancel, &overlay};
    check(!Observer::agentWidgetUncovered(&cancel, order), "later visible surface blocks control");
    overlay.setVisible(false);
    check(Observer::agentWidgetUncovered(&cancel, order), "invisible later surface does not block control");
    overlay.setVisible(true); overlay.setX(101);
    check(Observer::agentWidgetUncovered(&cancel, order), "nonoverlapping later surface permitted");
    overlay.setX(25); order = {&background, &overlay, &cancel};
    check(Observer::agentWidgetUncovered(&cancel, order), "earlier background surface does not cover later control");
    order = {&background, &overlay};
    check(!Observer::agentWidgetUncovered(&cancel, order), "control missing from actual paint order rejected");
    order.push_back(&cancel); cancel.setVisible(false);
    check(!Observer::agentWidgetUncovered(&cancel, order), "hidden control rejected even if in paint order");
    cancel.setVisible(true); cancel.blit(&canvas);
    Text laterText(180, 40, 20, 0);
    laterText.initText(&big, &small, &language); laterText.setText("TITLE"); laterText.blit(&canvas);
    order = {&background, &cancel, &laterText};
    check(Observer::agentWidgetUncovered(&cancel, order), "text allocation overlap alone does not hide control");
    laterText.setY(22); laterText.blit(&canvas);
    check(!Observer::agentWidgetUncovered(&cancel, order), "later actual glyph overlap hides control");
    Text errorMessage(246, 80, 37, 50);
    errorMessage.initText(&big, &small, &language); errorMessage.setBig();
    errorMessage.setAlign(ALIGN_CENTER); errorMessage.setVerticalAlign(ALIGN_MIDDLE);
    errorMessage.setWordWrap(true); errorMessage.setText("A normal error message."); errorMessage.blit(&canvas);
    check(Observer::agentTextJson(&errorMessage) == "\"A normal error message.\"", "ordinary error text projection");
    TextButton dismiss(120, 18, 100, 154);
    dismiss.initText(&big, &small, &language); dismiss.setText("OK"); dismiss.blit(&canvas);
    order = {&background, &dismiss, &errorMessage};
    check(Observer::agentWidgetUncovered(&dismiss, order) && dismiss.agentLabel() == "OK", "error OK control projects without message overlap");
    check(Observer::agentPaintHealthy(), "all off-screen constituent blits succeeded");
    check(SDL_GetVideoSurface() == 0 && !(SDL_WasInit(SDL_INIT_VIDEO)), "test never initialized GUI");
    std::cout << "AgentWidgetProjectionTest PASS " << checks << " checks\n";
}
