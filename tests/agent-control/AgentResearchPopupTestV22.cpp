// SPDX-License-Identifier: GPL-3.0-or-later
#define main retainedOutfittingFixtureMain
#include "AgentOutfittingProviderTestV21.cpp"
#undef main
namespace ResearchPopup {
using State=Outfitting::State;using Base=Outfitting::Base;
struct RuleResearch {std::string name,lookup;const std::string& getName()const{return name;}const std::string&getLookup()const{return lookup;}};
struct RuleManufacture {};struct RuleItem {};struct MissionSite {};struct GeoscapeState {};
struct ResearchState:State{ResearchState(Base*b):State(b){kind="research";}};
struct ManufactureState:State{ManufactureState(Base*b):State(b){kind="manufacture";}};
struct Ufopaedia {static std::vector<std::string> opened;static void openArticle(Outfitting::Game*g,const std::string&name){opened.push_back(name);++g->commands;}};
std::vector<std::string> Ufopaedia::opened;
struct ResearchCompleteState : public State
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentDestinationPaintFull = false;
    bool agentDestinationFull() const;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    std::vector<std::uint64_t> agentPaintRevisions() const;
#endif

	Window *_window;
	Text *_txtTitle, *_txtResearch;
	TextButton *_btnReport, *_btnOk;
	const RuleResearch * _research, * _bonus;
public:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    void blit() override;
    bool agentUiState(Observer::AgentUiState &out) const override;
    bool agentDispatch(const Observer::AgentRequest &request) override;
#endif
	/// Creates the EndResearch state.
ResearchCompleteState():State() {}
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	/// Handler for clicking the Report button.
	void btnReportClick(Action *action);
};
#include "ResearchCompleteStateMethods.generated.inc"
struct NewPossibleResearchState : public State
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentDestinationPaintFull = false;
    bool agentDestinationFull() const;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    std::vector<std::uint64_t> agentPaintRevisions() const;
#endif

public:
	Window *_window;
	Text *_txtTitle;
	TextList * _lstPossibilities;
	TextButton *_btnResearch, *_btnOk;
	Base * _base;
public:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    void blit() override;
    bool agentUiState(Observer::AgentUiState &out) const override;
    bool agentDispatch(const Observer::AgentRequest &request) override;
#endif
	/// Creates the NewPossibleResearch state.
NewPossibleResearchState():State() {}
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	/// Handler for clicking the Allocate Research button.
	void btnResearchClick(Action *action);
};
#include "NewPossibleResearchStateMethods.generated.inc"
struct NewPossibleManufactureState : public State
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentDestinationPaintFull = false;
    bool agentDestinationFull() const;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    std::vector<std::uint64_t> agentPaintRevisions() const;
#endif

public:
	Window *_window;
	Text *_txtTitle;
	TextList * _lstPossibilities;
	TextButton *_btnManufacture, *_btnOk;
	Base * _base;
public:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    void blit() override;
    bool agentUiState(Observer::AgentUiState &out) const override;
    bool agentDispatch(const Observer::AgentRequest &request) override;
#endif
	/// Creates the NewPossibleManufacture state.
NewPossibleManufactureState():State() {}
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	/// Handler for clicking the Allocate Manufacture button.
	void btnManufactureClick(Action *action);
};
#include "NewPossibleManufactureStateMethods.generated.inc"
struct ResearchRequiredState : public State
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentDestinationPaintFull = false;
    bool agentDestinationFull() const;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    std::vector<std::uint64_t> agentPaintRevisions() const;
#endif

	Window *_window;
	Text *_txtTitle;
	TextButton *_btnOk;
public:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    void blit() override;
    bool agentUiState(Observer::AgentUiState &out) const override;
    bool agentDispatch(const Observer::AgentRequest &request) override;
#endif
	/// Creates the ResearchRequired state.
ResearchRequiredState():State() {}
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
};
#include "ResearchRequiredStateMethods.generated.inc"
struct MissionDetectedState : public State
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentDestinationPaintFull = false;
    bool agentDestinationFull() const;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    std::vector<std::uint64_t> agentPaintRevisions() const;
#endif

public:
	MissionSite *_mission;
	GeoscapeState *_state;

	TextButton *_btnIntercept, *_btnCenter, *_btnCancel;
	Window *_window;
	Text *_txtTitle, *_txtCity;
public:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    void blit() override;
    bool agentUiState(Observer::AgentUiState &out) const override;
    bool agentDispatch(const Observer::AgentRequest &request) override;
#endif
	/// Creates the Mission Detected state.
MissionDetectedState():State() {}
	/// Cleans up the Mission Detected state.
	~MissionDetectedState(){}
	/// Handler for clicking the Intercept button.
	void btnInterceptClick(Action *action);
	/// Handler for clicking the Center on Site button.
	void btnCenterClick(Action *action);
	/// Handler for clicking the Cancel button.
	void btnCancelClick(Action *action);
};
#include "MissionDetectedStateMethods.generated.inc"
}
namespace {

void popupSetup(ResearchPopup::ResearchCompleteState&s,Paint&p){
 s._btnOk=p.button(80,16,64,146,"STR_OK");s._btnReport=p.button(80,16,176,146,"STR_VIEW_REPORTS");s._txtTitle=p.text(230,17,45,70,p.lang.getString("STR_RESEARCH_COMPLETED"),true);s._txtTitle->setAlign(ALIGN_CENTER);s._txtResearch=p.text(230,32,45,96,p.lang.getString(p.master=="xcom1"?"STR_LASER_WEAPONS":"STR_GAUSS_TECH"),true,"text2");s._txtResearch->setWordWrap(true);s._txtResearch->setAlign(ALIGN_CENTER);s._research=s._bonus=nullptr;finishSetup(s,p);
}
template<class T>void noticeSetup(T&s,Paint&p,bool manufacture,TextButton*&allocate){
 s._btnOk=p.button(160,14,80,149,"STR_OK");allocate=p.button(160,14,80,165,manufacture?"STR_ALLOCATE_MANUFACTURE":"STR_ALLOCATE_RESEARCH");s._txtTitle=p.text(288,40,16,20,p.lang.getString(manufacture?"STR_WE_CAN_NOW_PRODUCE":"STR_WE_CAN_NOW_RESEARCH"),true);s._txtTitle->setAlign(ALIGN_CENTER);s._lstPossibilities=p.list(250,manufacture?80:96,35,50,"text2");s._lstPossibilities->setColumns(1,250);s._lstPossibilities->setBig();s._lstPossibilities->setAlign(ALIGN_CENTER);s._lstPossibilities->setScrolling(true,0);s._lstPossibilities->addRow(1,p.lang.getString(p.master=="xcom1"?"STR_LASER_PISTOL":"STR_GAUSS_PISTOL").c_str());s._base=nullptr;finishSetup(s,p);
}
void popupSetup(ResearchPopup::NewPossibleResearchState&s,Paint&p){noticeSetup(s,p,false,s._btnResearch);}
void popupSetup(ResearchPopup::NewPossibleManufactureState&s,Paint&p){noticeSetup(s,p,true,s._btnManufacture);}
void popupSetup(ResearchPopup::ResearchRequiredState&s,Paint&p){
 s._btnOk=p.button(160,18,80,150,"STR_OK");const std::string weapon=p.master=="xcom1"?"STR_PLASMA_PISTOL":"STR_SONIC_PISTOL",ammo=p.master=="xcom1"?"STR_PLASMA_PISTOL_CLIP":"STR_PISTOL_POWER_CLIP";s._txtTitle=p.text(288,80,16,50,p.lang.getString("STR_YOU_NEED_TO_RESEARCH_ITEM_TO_PRODUCE_ITEM").arg(p.lang.getString(ammo)).arg(p.lang.getString(weapon)),true);s._txtTitle->setAlign(ALIGN_CENTER);s._txtTitle->setVerticalAlign(ALIGN_MIDDLE);finishSetup(s,p);
}
void popupSetup(ResearchPopup::MissionDetectedState&s,Paint&p){
 s._btnIntercept=p.button(200,16,28,130,"STR_INTERCEPT");s._btnCenter=p.button(200,16,28,150,"STR_CENTER_ON_SITE_TIME_5_SECONDS");s._btnCancel=p.button(200,16,28,170,"STR_CANCEL_UC");s._txtTitle=p.text(246,32,5,48,p.lang.getString(p.master=="xcom1"?"STR_ALIENS_TERRORISE":"STR_ALIENS_LAUNCH_PORT_TERROR"),true,"text");s._txtTitle->setAlign(ALIGN_CENTER);s._txtTitle->setWordWrap(true);s._txtCity=p.text(246,17,5,80,p.lang.getString("STR_LONDON"),true,"text");s._txtCity->setAlign(ALIGN_CENTER);finishSetup(s,p);
}
template<class T>void noticeProof(T&s,Paint&p,const std::string&screen){
 TableChannel c(p.master+"-"+screen+"-rows");connect(c,s);auto initial=record(c,p.master,screen,"notice_stock_row");
 check(initial["visible"]["rows"].size()>0,"stock one-column notice has shown text");auto*list=s._lstPossibilities;auto first=list->getCellText(0,0);
 list->setCellText(0,0,std::string(1025,'W'));p.draw();auto partial=record(c,p.master,screen,"notice_incomplete_row");check(partial["visible"]["rows"].size()==0,"incomplete notice row omitted");check(partial["actions"].size()>=2,"notice buttons independent of list text");list->setCellText(0,0,first);p.draw();
 for(unsigned n=0;n<20;++n)list->addRow(1,first.c_str());p.draw();record(c,p.master,screen,"notice_scrolling");execute(c,s,p,"scroll_visible_list","down");record(c,p.master,screen,"notice_after_down");execute(c,s,p,"scroll_visible_list","up");record(c,p.master,screen,"notice_after_up");
 c.read();c.queue("scroll_visible_list","down");list->scrollDown(false,false);list->scrollUp(false,false);p.draw();c.frame();check(YAML::Load(c.receive())["status"].as<std::string>()=="unavailable","notice scroll ABA stale");
 list->clearList();p.draw();auto empty=record(c,p.master,screen,"notice_empty");check(empty["visible"]["rows"].size()==0&&empty["actions"].size()>=2,"empty notice retains independently shown buttons");
}
bool actualNoticeBound(AgentUiState &out) {
#include "NoticeBound.generated.inc"
return true;
}
void noticeTransport(){
 AgentUiState ui;ui.screen="new_possible_research";ui.regions.push_back({0,0,1,1});
 const auto visible=[](unsigned id,unsigned chars,bool unicode){std::string field;for(unsigned i=0;i<chars;++i)field+=unicode?"é":"W";std::string rows="[";for(unsigned r=0;r<32;++r){if(r)rows+=",";rows+="{\"handle\":\"r"+std::to_string(r)+"\",\"cells\":["+agentJsonString(field)+"],\"selected\":false}";}return "{\"title\":"+agentJsonString(std::to_string(id))+",\"rows\":"+rows+"],\"scroll\":{\"up\":false,\"down\":false},\"buttons\":[]}";};
 for(bool unicode:{false,true}){unsigned size=1;for(;size<=512;++size){ui.visible=visible(0,size,unicode);if(!actualNoticeBound(ui))break;}check(size>1&&size<=512,"actual extracted notice byte cap has bounded maximum");const auto max=size-1;ui.visible=visible(0,max,unicode);check(actualNoticeBound(ui),"actual producer maximum accepted");ui.visible=visible(0,size,unicode);check(!actualNoticeBound(ui),"next producer candidate rejected");TableChannel c(std::string("synthetic-research-transport-")+(unicode?"unicode":"ascii"));c.state.project=[&](AgentUiState&out){out=ui;return true;};for(unsigned id=0;id<12;++id){ui.visible=visible(id,max-1,unicode);check(actualNoticeBound(ui),"every synthetic presentation fits actual producer cap");c.read();}
 std::string first;std::uint64_t cursor=0;unsigned found=0;for(;;){c.send("V "+token(c.challenge++)+" "+c.epoch+" "+std::to_string(cursor)+" 16");c.controller->poll();auto bytes=c.receive();auto page=YAML::Load(bytes);if(first.empty())first=bytes;check(bytes.size()<=49152,"actual large retained page byte bounded");auto count=page["events"].size();if(!count)break;const auto next=page["next_cursor"].as<std::uint64_t>();check(next>cursor,"large partial page cursor advances");found+=count;cursor=next;}check(found==12,"all12 retained large presentations without loss");auto saved=YAML::Load(first);c.send("V "+saved["challenge"].as<std::string>()+" "+c.epoch+" 0 16");c.controller->poll();check(c.receive()==first,"first page replay is byte-identical");}
}
void popupTests(Font&big,Font&small,Language&lang,const std::string&source,const std::string&game){
 Outfitting::Game g;g.lang=&lang;Outfitting::active=&g;
 {Paint p(big,small,lang,source,game,"geoResearchComplete");ResearchPopup::ResearchCompleteState s;popupSetup(s,p);common(s,p,s._txtTitle,s._btnOk,nullptr,"research_complete",{{"dismiss_research_complete",""},{"view_research_reports",""}});paintedFailure(s,p);
 ResearchPopup::RuleResearch main,bonus;main.name="MAIN";main.lookup="MAIN_LOOKUP";bonus.name="BONUS";bonus.lookup="BONUS_LOOKUP";s._research=&main;s._bonus=&bonus;ResearchPopup::Ufopaedia::opened.clear();s.btnReportClick(nullptr);check(ResearchPopup::Ufopaedia::opened==std::vector<std::string>({"BONUS_LOOKUP","MAIN_LOOKUP"}),"unchanged report callback bonus/main lookup ordering");main.lookup.clear();bonus.lookup.clear();ResearchPopup::Ufopaedia::opened.clear();s.btnReportClick(nullptr);check(ResearchPopup::Ufopaedia::opened==std::vector<std::string>({"BONUS","MAIN"}),"unchanged report name fallback ordering");}
 {Paint p(big,small,lang,source,game,"geoResearch");ResearchPopup::NewPossibleResearchState s;popupSetup(s,p);common(s,p,s._txtTitle,s._btnOk,nullptr,"new_possible_research",{{"dismiss_new_research",""},{"open_research_allocation",""}});noticeProof(s,p,"new_possible_research");paintedFailure(s,p);}
 {Paint p(big,small,lang,source,game,"geoManufacture");ResearchPopup::NewPossibleManufactureState s;popupSetup(s,p);common(s,p,s._txtTitle,s._btnOk,nullptr,"new_possible_manufacture",{{"dismiss_new_manufacture",""},{"open_manufacture_allocation",""}});noticeProof(s,p,"new_possible_manufacture");paintedFailure(s,p);}
 {Paint p(big,small,lang,source,game,"geoResearchRequired");ResearchPopup::ResearchRequiredState s;popupSetup(s,p);common(s,p,s._txtTitle,s._btnOk,nullptr,"research_required",{{"dismiss_research_required",""}});paintedFailure(s,p);}
 {Paint p(big,small,lang,source,game,"terrorSite");ResearchPopup::MissionDetectedState s;popupSetup(s,p);common(s,p,s._txtTitle,s._btnCancel,nullptr,"mission_alert",{{"dismiss_mission_alert",""}});AgentUiState ui;check(s.agentUiState(ui)&&ui.actions.size()==1,"mission intercept and center display only");paintedFailure(s,p);}
}
}
int main(int argc,char**argv){check(argc==2,"source root");check(!SDL_Init(0)&&!SDL_GetVideoSurface(),"offscreen only");Options::baseXResolution=320;Options::baseYResolution=200;noticeTransport();Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}for(const std::string game:{"xcom1","xcom2"}){Language lang;lang.loadFile(std::string(argv[1])+"/bin/standard/"+game+"/Language/en-US.yml");popupTests(big,small,lang,argv[1],game);}std::string json="[";for(auto&r:records){if(json.size()>1)json+=",";json+=r;}const char*d=getenv("OPENXCOM_AGENT_TEST_PACKET_DIR");std::ofstream out(std::string(d)+"/case-records.json");out<<json<<"]";check(bool(out),"case records saved");check(agentPaintHealthy()&&!SDL_GetVideoSurface(),"no GUI or paint failure");std::cout<<"PASS "<<checks<<" checks\n";}
