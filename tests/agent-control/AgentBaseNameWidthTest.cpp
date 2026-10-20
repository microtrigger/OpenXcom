// SPDX-License-Identifier: GPL-3.0-or-later
// Final-build off-screen BaseName editor width and atomic rejection boundary.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Engine/Unicode.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/TextEdit.h"
#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <cctype>
#include <iostream>
using namespace OpenXcom;
namespace {
unsigned checks=0;
void check(bool value,const char *why){++checks;if(!value){std::cerr<<"FAIL "<<why<<'\n';std::exit(1);}}
void key(TextEdit &edit,SDLKey sym,Uint16 unicode=0){SDL_Event event={};event.type=SDL_KEYDOWN;event.key.keysym.sym=sym;event.key.keysym.unicode=unicode;Action action(&event,1,1,0,0);edit.keyboardPress(&action,nullptr);}
}
int main(){
 check(SDL_Init(0)==0 && !SDL_GetVideoSurface(),"no-window SDL");
 Options::baseXResolution=320;Options::baseYResolution=200;Options::keyboardMode=KEYBOARD_ON;
 Font big,small;for(const auto &font:YAML::LoadFile("Language/Font.dat")["fonts"]){if(font["id"].as<std::string>()=="FONT_BIG")big.load(font);if(font["id"].as<std::string>()=="FONT_SMALL")small.load(font);}
 Language language;Surface canvas(320,200);
 SDL_Rect caretInk={};check(big.agentGlyphInk('|',caretInk),"stock caret has glyph ink");
 std::cout<<"caret advance="<<big.getCharSize('|').w<<" ink="<<caretInk.x<<','<<caretInk.y<<','<<caretInk.w<<','<<caretInk.h<<" TFTD endCaretRight="<<(126+caretInk.x+caretInk.w)<<" fieldRight=127\n";
 for(const std::string candidate:{"MCP Draft","MCP UFO Test","MCP TFTD Test","MCP Test"}){
  TextEdit edit(nullptr,127,16,59,94);edit.initText(&big,&small,&language);edit.setBig();edit.setFocus(true,false);edit.blit(&canvas);
  check(edit.agentReplaceText("MCP Draft"),"prepare previous name");
  key(edit,SDLK_LEFT);key(edit,SDLK_LEFT);key(edit,SDLK_LEFT);edit.blink();edit.blit(&canvas);
  std::string previousRendered;check(edit.agentRenderedText(previousRendered),"baseline complete rendered name");
  const std::string previous=edit.getText();const auto revision=edit.agentInputRevision();
  int width=0;for(UCode ch:Unicode::convUtf8ToUtf32(candidate))width+=big.getCharSize(ch).w;
  Text probe(320,20);probe.initText(&big,&small,&language);probe.setBig();probe.setText(candidate);probe.blit(&canvas);SDL_Rect glyph={};const bool hasGlyph=probe.agentPaintedBounds(glyph);
  const bool accepted=edit.agentReplaceText(candidate);std::string rendered;const bool full=edit.agentRenderedText(rendered);
  std::cout<<"candidate="<<candidate<<" width="<<width<<" textWidth="<<probe.getTextWidth()<<" ink="<<hasGlyph<<':'<<glyph.x<<','<<glyph.y<<','<<glyph.w<<','<<glyph.h<<" field=127x16 accepted="<<accepted<<" name="<<edit.getText()<<" rendered="<<(full?rendered:"<unavailable>")<<" revisionBefore="<<revision<<" revisionAfter="<<edit.agentInputRevision();
  check(accepted,"all four stock-font candidates fit native text boundary");
  if(accepted){check(edit.getText()==candidate && full && rendered==candidate,"accepted full name");std::cout<<"\n";}
  else{
   check(edit.getText()==previous && full && rendered==previousRendered,"rejection preserves full previous model/rendered text");
   check(edit.agentInputRevision()==revision,"rejection preserves private input revision");
   key(edit,SDLK_EXCLAIM,'!');edit.blit(&canvas);
   const std::string expected=previous.substr(0,previous.size()-3)+"!"+previous.substr(previous.size()-3);
   check(edit.getText()==expected,"rejection preserves original middle caret insertion position");
   std::cout<<" preservedText=true preservedRevision=true caretProbe="<<edit.getText()<<" preservedCaret=true\n";
  }
 }
 // Actual rejection remains atomic at a middle insertion point. Paint the
 // caret's ordinary off phase so the text is not occluded before replacement.
 TextEdit rollback(nullptr,127,16,59,94);rollback.initText(&big,&small,&language);rollback.setBig();rollback.setFocus(true,false);rollback.blit(&canvas);
 check(rollback.agentReplaceText("MCP Draft"),"prepare middle-caret rollback fixture");
 key(rollback,SDLK_LEFT);key(rollback,SDLK_LEFT);key(rollback,SDLK_LEFT);rollback.blink();rollback.blit(&canvas);
 const auto rollbackRevision=rollback.agentInputRevision();std::string rollbackRendered;
 check(rollback.agentRenderedText(rollbackRendered) && rollbackRendered=="MCP Draft","ordinary blink-off leaves midpoint text complete");
 check(!rollback.agentReplaceText(std::string(128,'W')) && rollback.getText()=="MCP Draft","true text overflow rejects with complete previous name");
 check(!rollback.agentReplaceText("A\xF0\x9F\x8C\x8C") && rollback.getText()=="MCP Draft","missing glyph rejects with complete previous name");
 check(rollback.agentInputRevision()==rollbackRevision && rollback.agentRenderedText(rollbackRendered) && rollbackRendered=="MCP Draft","rejections preserve private revision and previous painted text");
 key(rollback,SDLK_EXCLAIM,'!');rollback.blit(&canvas);
 check(rollback.getText()=="MCP Dr!aft","rejections preserve original middle insertion position");
 // Ordinary keyboard input has only the shared character/text-width
 // constraints; semantic replacement now accepts the same complete text.
 TextEdit ordinary(nullptr,127,16,59,94);ordinary.initText(&big,&small,&language);ordinary.setBig();ordinary.setFocus(true,false);ordinary.blit(&canvas);
 const std::string ordinaryCandidate="MCP TFTD Test";
 for(unsigned char ch:ordinaryCandidate)key(ordinary,static_cast<SDLKey>(std::tolower(ch)),ch);
 ordinary.blit(&canvas);
 check(ordinary.getText()==ordinaryCandidate,"ordinary keyboard accepts the complete 126px name");
 std::string ordinaryRendered;const bool ordinaryFull=ordinary.agentRenderedText(ordinaryRendered);
 check(ordinaryFull && ordinaryRendered == ordinaryCandidate && ordinary.agentEditable(),"semantic editor accepts ordinary native full-text state despite clipped caret");
 SDL_Rect clippedCaret={}; if(ordinary.agentCaretBounds(clippedCaret))check(clippedCaret.x>=59 && clippedCaret.x+clippedCaret.w<=186,"caret occlusion stays inside editor footprint");
 check(126+caretInk.x+caretInk.w>127,"ordinary accepted end caret extends beyond the editor");
 std::cout<<"ordinaryKeyboard candidate="<<ordinaryCandidate<<" name="<<ordinary.getText()<<" width=126 nativeAccepted=true endCaretRight="<<(126+caretInk.x+caretInk.w)<<" fieldRight=127 caretClipped=true agentRenderedText="<<ordinaryFull<<" agentEditable="<<ordinary.agentEditable()<<" classification=native_and_semantic_input_equivalence\n";
 check(!SDL_GetVideoSurface() && !SDL_WasInit(SDL_INIT_VIDEO),"no GUI initialized");
 std::cout<<"AgentBaseNameWidthTest PASS "<<checks<<" checks\n";
}
