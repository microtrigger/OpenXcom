// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Engine/FileMap.h"
#define main retainedOutfittingFixtureMain
#include "MissionHarnessBase.generated.inc"
#undef main
namespace Slides {
using State=Outfitting::State;
struct Game:Outfitting::Game {Surface cursor{1,1};Surface*getCursor(){return &cursor;}};
struct Timer {unsigned starts=0;int interval=0;void start(){++starts;}void setInterval(int i){interval=i;}};
struct SlideshowHeader {int transitionSeconds=3;};struct SlideshowSlide {int transitionSeconds=0;};
struct CutsceneState {static unsigned resets;static void resetDisplay(bool){++resets;}};unsigned CutsceneState::resets=0;
struct SlideshowState:State {
 Game*_game;bool _agentDestinationPaintFull=false;std::uint64_t _agentSlideRevision=0;std::vector<std::uint64_t>_agentStoredPaintRevisions;
 SlideshowHeader _slideshowHeader;std::vector<SlideshowSlide>*_slideshowSlides;bool _wasLetterboxed=false;std::vector<InteractiveSurface*>_slides;std::vector<Text*>_captions;int _curScreen=-1;Timer*_transitionTimer;
 explicit SlideshowState(Game*g):_game(g){}
 void setPalette(SDL_Color*){} // Explicit display palette seam; real widget blits remain.
 bool agentDestinationFull()const;std::vector<std::uint64_t>agentPaintRevisions()const;
 void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void screenClick(Action*);void screenSkip(Action*);void screenTimer();
};
#include "SlideshowMethods.generated.inc"
}
namespace {
void slideTests(Font&big,Font&small,Language&lang,const std::string&source,const std::string&master){
 Slides::Game game;game.lang=&lang;Outfitting::active=&game;Paint paint(big,small,lang,source,master,"terrorSite");Slides::SlideshowState state(&game);Slides::Timer timer;state._transitionTimer=&timer;std::vector<Slides::SlideshowSlide> rules(3);state._slideshowSlides=&rules;
 for(int i=0;i<3;++i){auto*slide=paint.own(new InteractiveSurface(320,200));slide->agentTrackPresentation();slide->clear(30+i);slide->setVisible(false);state._slides.push_back(slide);auto*caption=paint.text(200,20,40,160,i==0?"CURRENT CAPTION":("FUTURE PRIVATE "+std::to_string(i)));caption->agentTrackPresentation();caption->setVisible(false);state._captions.push_back(caption);}
 finishSetup(state,paint);state.screenClick(nullptr);AgentUiState ui;check(!state.agentUiState(ui),"new slide unpainted denied");paint.draw();check(state.agentUiState(ui)&&ui.offers("advance_slideshow"),"painted active slide admitted despite own caption overlap");check(ui.visible.find("CURRENT CAPTION")!=std::string::npos&&ui.visible.find("FUTURE")==std::string::npos,"current caption only no queued text");check(ui.actions.size()==1,"only one-step action no skip-all");
 AgentRequest request;request.action="advance_slideshow";auto first=ui;state._captions[0]->setText("CHANGED");check(!state.agentUiState(ui),"caption mutation invalidates paint");paint.draw();check(state.agentUiState(ui)&&!ui.sameBinding(first),"caption revision binding changed");
 Surface overlay(20,20,0,0);game._cursor=&overlay;check(state.agentUiState(ui),"ordinary cursor ignored");game._fpsCounter=&overlay;check(!state.agentUiState(ui)&&!state.agentDispatch(request),"real overlay blocks state and action");game._fpsCounter=nullptr;game._cursor=nullptr;
 state._surfaces.push_back(&overlay);paint.draw();check(!state.agentUiState(ui),"later noncaption overlay blocks slide");state._surfaces.pop_back();paint.draw();
 check(state.agentUiState(ui),"recovered paint admitted");first=ui;state.screenTimer();check(state._curScreen==1&&game.pops==0,"native timer advances exactly one and not skip");check(!state.agentUiState(ui),"timer transition invalidates until paint");paint.draw();check(state.agentUiState(ui)&&!ui.sameBinding(first),"new slide private binding changes");
 check(state.agentDispatch(request)&&state._curScreen==2&&game.pops==0,"dispatch advances exactly one slide");check(!state.agentDispatch(request)&&state._curScreen==2,"immediate duplicate without new paint does not advance");paint.draw();check(state.agentUiState(ui),"last slide still admitted");check(state.agentDispatch(request)&&game.pops==1&&Slides::CutsceneState::resets==1,"last advance invokes native reset and pop once");check(!state.agentUiState(ui),"completed slideshow not admitted");check(timer.starts==3,"ordinary native timer restart once per slide");
}
}
int main(int argc,char**argv){check(argc==3,"source and master");check(!SDL_Init(0)&&!SDL_GetVideoSurface(),"offscreen only");Options::baseXResolution=320;Options::baseYResolution=200;FileMap::load("slideshow-test","./",true);Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}Language lang;lang.loadFile(std::string(argv[1])+"/bin/standard/"+argv[2]+"/Language/en-US.yml");slideTests(big,small,lang,argv[1],argv[2]);check(agentPaintHealthy()&&!SDL_GetVideoSurface(),"paint healthy no GUI");std::cout<<"PASS "<<checks<<" checks\n";}
