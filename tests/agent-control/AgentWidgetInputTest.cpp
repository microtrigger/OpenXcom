// SPDX-License-Identifier: GPL-3.0-or-later
// Real off-screen widget input/projection checks. No Game, window, or SDL event
// queue is created. Local Action fixtures exercise ordinary handlers directly.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/ToggleTextButton.h"
#include "../../src/Interface/TextEdit.h"
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
void key(TextEdit &edit, SDLKey symbol, Uint16 unicode)
{
    SDL_Event event = {};
    event.type = SDL_KEYDOWN; event.key.keysym.sym = symbol; event.key.keysym.unicode = unicode;
    Action action(&event, 1.0, 1.0, 0, 0);
    edit.keyboardPress(&action, nullptr);
}
}
int main()
{
    check(SDL_Init(0) == 0, "SDL eventless initialization");
    check(SDL_GetVideoSurface() == nullptr, "no display surface");
    Options::baseXResolution = 320; Options::baseYResolution = 200;
    Options::keyboardMode = KEYBOARD_ON;
    const YAML::Node data = YAML::LoadFile("Language/Font.dat");
    Font big, small;
    for (const auto &font : data["fonts"])
    {
        if (font["id"].as<std::string>() == "FONT_BIG") big.load(font);
        if (font["id"].as<std::string>() == "FONT_SMALL") small.load(font);
    }
    Language language;
    Surface canvas(320, 200);
    TextButton first(160, 18, 80, 32), second(160, 18, 80, 52);
    first.initText(&big, &small, &language); second.initText(&big, &small, &language);
    first.setText("1. BEGINNER"); second.setText("2. EXPERIENCED");
    TextButton *group = &first; first.setGroup(&group); second.setGroup(&group);
    first.blit(&canvas); second.blit(&canvas);
    bool pressed = false;
    check(first.agentPressed(pressed) && pressed, "initial selected appearance");
    check(second.agentPressed(pressed) && !pressed, "initial unselected appearance");
    const auto oldSecondRevision = second.agentSelectionRevision();
    check(second.selectRadio() && group == &second, "native radio operation selects group");
    check(first.agentPressed(pressed) && !pressed, "prior radio redraws unselected");
    check(second.agentPressed(pressed) && pressed, "chosen radio redraws selected");
    check(second.agentSelectionRevision() > oldSecondRevision, "radio mutation binds ABA");
    group = &first; // Private model-only fixture change must not change the reported draw.
    check(second.agentPressed(pressed) && pressed, "selection comes from draw, not group pointer");
    second.draw(); first.draw();
    check(second.agentPressed(pressed) && !pressed, "next draw updates captured selection");
    SDL_Event pressEvent = {}; pressEvent.type = SDL_MOUSEBUTTONDOWN;
    pressEvent.button.button = SDL_BUTTON_LEFT;
    Action pressAction(&pressEvent, 1.0, 1.0, 0, 0);
    second.mousePress(&pressAction, nullptr);
    check(group == &second && second.agentPressed(pressed) && pressed, "ordinary mouse shares native radio operation");
    first.setVisible(false);
    check(!first.agentPressed(pressed), "hidden radio has no selected projection");

    ToggleTextButton toggle(78, 18, 80, 138);
    toggle.initText(&big, &small, &language); toggle.setColor(10); toggle.setText("IRONMAN"); toggle.blit(&canvas);
    check(toggle.agentTogglePressed(pressed) && !pressed, "initial toggle false");
    toggle.setPressed(true);
    check(!toggle.agentTogglePressed(pressed), "dirty toggle does not expose desired value");
    toggle.blit(&canvas);
    check(toggle.agentTogglePressed(pressed) && pressed, "desired true is captured after render");
    const auto trueRevision = toggle.agentToggleRevision();
    toggle.setPressed(true); toggle.blit(&canvas);
    check(toggle.agentToggleRevision() == trueRevision && toggle.getPressed(), "desired true is idempotent");
    toggle.setPressed(false); toggle.setPressed(true); toggle.blit(&canvas);
    check(toggle.agentToggleRevision() == trueRevision + 2, "toggle away/back invalidates binding");
    toggle.mousePress(&pressAction, nullptr);
    check(!toggle.getPressed() && toggle.agentTogglePressed(pressed) && !pressed, "ordinary mouse shares desired-state mutation");

    // No owner is required for non-modal input with no change callback.
    TextEdit edit(nullptr, 127, 16, 59, 94);
    edit.initText(&big, &small, &language); edit.setBig(); edit.setFocus(true, false); edit.blit(&canvas);
    std::string rendered;
    check(edit.agentEditable() && edit.agentRenderedText(rendered) && rendered.empty(), "empty focused editor is visible/editable");
    check(edit.agentReplaceText("Alpha") && edit.getText() == "Alpha", "bounded replacement uses native input");
    check(edit.agentRenderedText(rendered) && rendered == "Alpha", "replacement preserves complete rendered string");
    const auto revision = edit.agentInputRevision();
    check(!edit.agentReplaceText(std::string(128, 'W')) && edit.getText() == "Alpha", "over-width edit rejects atomically");
    check(!edit.agentReplaceText("A\xF0\x9F\x8C\x8C") && edit.getText() == "Alpha", "missing glyph edit rejects atomically");
    check(!edit.agentReplaceText("\xC0\xAF") && edit.getText() == "Alpha", "overlong UTF-8 rejected before insertion");
    check(!edit.agentReplaceText("A\nB") && edit.getText() == "Alpha", "control characters rejected");
    check(edit.agentInputRevision() == revision && edit.agentRenderedText(rendered) && rendered == "Alpha", "rejection preserves input revision and rendered value");
    check(edit.agentReplaceText("") && edit.getText().empty(), "empty replacement is permitted");
    edit.setConstraint(TEC_NUMERIC_POSITIVE);
    key(edit, SDLK_a, 'a'); edit.blit(&canvas);
    check(edit.getText().empty(), "ordinary keyboard obeys numeric constraint");
    check(!edit.agentReplaceText("a") && edit.getText().empty(), "typed replacement shares numeric constraint");
    key(edit, SDLK_1, '1'); edit.blit(&canvas);
    check(edit.getText() == "1", "ordinary keyboard accepts numeric input");
    check(edit.agentReplaceText("12") && edit.getText() == "12", "typed replacement accepts same numeric input");
    edit.setConstraint(TEC_NONE);
    const auto beforeAba = edit.agentInputRevision();
    check(edit.agentReplaceText("Beta") && edit.agentReplaceText("12"), "editor away/back changes succeed");
    check(edit.agentInputRevision() > beforeAba, "editor ABA retains distinct binding");
    edit.blink(); edit.blit(&canvas);
    check(edit.agentEditable(), "normal caret off phase does not flicker capability");
    edit.setFocus(false, false); edit.blit(&canvas);
    check(!edit.agentEditable() && !edit.agentReplaceText("Beta"), "unfocused editor cannot be changed");
    edit.setFocus(true, false); edit.blit(&canvas); edit.setVisible(false);
    check(!edit.agentRenderedText(rendered) && !edit.agentReplaceText("Beta"), "hidden editor exports no name and rejects input");
    edit.setVisible(true); edit.blit(&canvas); Options::keyboardMode = KEYBOARD_OFF;
    check(!edit.agentEditable() && !edit.agentReplaceText("Beta"), "keyboard-off mode is not bypassed");
    Options::keyboardMode = KEYBOARD_ON;

    TextEdit narrow(nullptr, big.getCharSize('A').w, 16, 0, 0);
    narrow.initText(&big, &small, &language); narrow.setBig(); narrow.setFocus(true, false); narrow.blit(&canvas);
    check(narrow.agentEditable(), "narrow empty editor fits caret");
    check(!narrow.agentReplaceText("A") && narrow.getText().empty(), "terminal caret clipping rejects with rollback");
    check(narrow.agentEditable(), "rollback restores complete caret");
    check(Observer::agentPaintHealthy(), "all off-screen constituent blits succeeded");
    check(SDL_GetVideoSurface() == nullptr && !(SDL_WasInit(SDL_INIT_VIDEO)), "no GUI was initialized");
    std::cout << "AgentWidgetInputTest PASS " << checks << " checks\n";
}
