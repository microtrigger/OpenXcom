// SPDX-License-Identifier: GPL-3.0-or-later
// Current real widgets; inherited owning Game/Screen seams are unused here.
// Synthetic font assets are generated in this test's output, never game assets.
#define main inheritedBaseTableMain
#include "AgentBaseTableProviderTest.cpp"
#undef main
#include "../../src/Engine/FileMap.h"
#include <sys/stat.h>

namespace {
bool referenceDisjoint(const AgentUiState &ui)
{
    for (size_t i=0;i<ui.regions.size();++i) for (size_t j=i+1;j<ui.regions.size();++j)
    {
        const AgentUiRect &a=ui.regions[i], &b=ui.regions[j];
        if(a.x<b.x+b.width && a.x+a.width>b.x && a.y<b.y+b.height && a.y+a.height>b.y) return false;
    }
    return true;
}
void sweepTests()
{
    AgentUiState ui;
    for(int ax=-2;ax<=2;++ax) for(int ay=-2;ay<=2;++ay)
    for(int bx=-2;bx<=2;++bx) for(int by=-2;by<=2;++by)
    for(int aw=1;aw<=3;++aw) for(int ah=1;ah<=3;++ah)
    for(int bw=1;bw<=3;++bw) for(int bh=1;bh<=3;++bh)
    {
        ui.regions={{ax,ay,aw,ah},{bx,by,bw,bh}};
        check(agentRegionsDisjoint(ui)==referenceDisjoint(ui),"sweep equals original strict rectangle predicate exhaustively");
    }
    unsigned seed=0x71b;
    for(unsigned sample=0;sample<2048;++sample)
    {
        ui.regions.clear();
        for(unsigned i=0;i<sample%31;++i)
        {
            seed=1664525U*seed+1013904223U; int x=int(seed%200)-100;
            seed=1664525U*seed+1013904223U; int y=int(seed%200)-100;
            seed=1664525U*seed+1013904223U; int w=1+seed%12;
            seed=1664525U*seed+1013904223U; int h=1+seed%12;
            ui.regions.push_back({x,y,w,h});
        }
        check(agentRegionsDisjoint(ui)==referenceDisjoint(ui),"deterministic arbitrary-order rectangles equal old predicate");
        std::reverse(ui.regions.begin(),ui.regions.end());
        check(agentRegionsDisjoint(ui)==referenceDisjoint(ui),"input reversal preserves exact overlap result");
    }
    ui.regions.clear(); for(int i=0;i<16384;++i) ui.regions.push_back({0,i,1,1});
    check(agentRegionsDisjoint(ui),"maximum private region count remains bounded and admitted when disjoint");
    ui.regions.push_back({0,16384,1,1});
    check(!agentRegionsDisjoint(ui),"private region count exhaustion fails closed");
    ui.regions.clear(); for(int i=0;i<2100;++i) ui.regions.push_back({i*2,0,1,1});
    check(referenceDisjoint(ui) && !agentRegionsDisjoint(ui),"comparison work exhaustion is explicitly conservative");
}
void paint(TextList &list, Surface &canvas) { list.invalidate(); canvas.clear(); list.blit(&canvas); }
void setup(TextList &list, Font &font, Language &lang, Surface &background, SDL_Color *palette)
{
    list.setPalette(palette); list.initText(&font,&font,&lang); list.setColor(80);
    list.setColumns(1,list.getWidth()); list.setSelectable(true); list.setBackground(&background);
}
void glyphTests(const char *output)
{
    const std::string root=std::string(output)+"/synthetic-font";
    mkdir(root.c_str(),0700); mkdir((root+"/Language").c_str(),0700);
    check(std::ifstream(root+"/Language/ink.png").good(),"runner supplied exact indexed synthetic opacity atlas");
    FileMap::load("test-ink-only",root,true);
    Font font; font.load(YAML::Load("width: 3\nheight: 9\nspacing: -1\nmonospace: true\nimages:\n - file: ink.png\n   chars: AB\n"));
    SDL_Rect exactB={};check(font.agentGlyphInk('B',exactB)&&exactB.x==0&&exactB.y==0&&exactB.w==1&&exactB.h==1,"indexed synthetic B glyph has exactly one opaque pixel");
    Language language;
    SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};
    Options::baseXResolution=640;Options::baseYResolution=400;
    Surface canvas(640,400),background(640,400); canvas.setPalette(palette);background.setPalette(palette);
    background.drawRect(0,0,640,400,40);
    Text source(20,9,30,30);source.setPalette(palette);source.initText(&font,&font,&language);source.setColor(80);source.setText("B");
    canvas.clear();source.blit(&canvas);check(source.agentContextVisible(),"ordinary Text first blit is clean");
    SDL_Rect ink={};check(source.agentPaintedBounds(ink)&&ink.w&&ink.h,"synthetic Text ink proof exists");
    std::vector<Uint8> original;
    for(int y=0;y<9;++y)for(int x=0;x<20;++x)original.push_back(canvas.getPixel(30+x,30+y));
    SDL_Rect sourceClip={10,5,1,1};SDL_SetClipRect(source.getSurface(),&sourceClip);
    check(source.getSurface()->clip_rect.x==10&&source.getSurface()->clip_rect.y==5,"nonstandard source clip actually installed");
    canvas.clear();source.blit(&canvas);bool identical=true;unsigned index=0;
    for(int y=0;y<9;++y)for(int x=0;x<20;++x)identical=identical&&(original[index++]==canvas.getPixel(30+x,30+y));
    std::cout<<"source-clip diagnostic identical="<<identical<<" bounds="<<ink.x<<","<<ink.y<<","<<ink.w<<","<<ink.h<<" target="<<int(canvas.getPixel(ink.x,ink.y))<<" originalNonzero="<<std::count_if(original.begin(),original.end(),[](Uint8 x){return x!=0;})<<"\n";
    check(identical&&canvas.getPixel(ink.x,ink.y)!=0,"real SDL ignores source clip_rect when ordinary Text source crop is null");
    check(source.agentContextVisible()&&source.agentTextComplete(),"source clip does not invalidate actual complete ordinary Text contribution");
    SDL_Rect targetClip={40,35,1,1};SDL_SetClipRect(canvas.getSurface(),&targetClip);
    canvas.setPixel(ink.x,ink.y,0);source.blit(&canvas);
    check(canvas.getPixel(ink.x,ink.y)==0,"same actual SDL respects destination clipping");
    SDL_SetClipRect(canvas.getSurface(),nullptr);
    source.getCrop()->x=10;source.getCrop()->y=5;source.getCrop()->w=1;source.getCrop()->h=1;
    canvas.clear();source.blit(&canvas);
    check(canvas.getPixel(ink.x,ink.y)==0&&!source.agentContextVisible()&&agentTextJson(&source)=="null",
        "ordinary explicit source crop does restrict paint and existing semantic guard rejects it");
    TextList list(40,24,10,10);setup(list,font,language,background,palette);
    list.addRow(1,"A"); paint(list,canvas);
    std::vector<AgentListRow> labels,controls;
    check(list.agentRows(labels)&&labels.size()==1,"complete isolated first synthetic label admitted");
    list.addRow(1,"B");paint(list,canvas);
    check(list.agentRows(labels)&&labels.size()==1&&labels[0].logicalIndex==1,"one same-color opaque pixel overwrite retires full earlier text");
    check(list.agentControlRows(controls)&&controls.size()==2&&!controls[0].completeText&&controls[1].completeText,
        "surviving first-row ink retains unknown control separately from overwritten text");
    check(controls[0].bounds.y<controls[1].bounds.y,"controls ordered by actually shown click-band y");
    AgentUiState projected; AgentListProjection projection; std::vector<Surface*> order;list.agentPaintOrder(order);
    check(agentProjectList(&list,&list,order,1,true,projected,projection,{},false,true),"unknown control opt-in projection admitted");
    YAML::Node rows=YAML::Load(projection.rows);
    check(rows.size()==2&&rows[0]["cells"][0].IsNull()&&rows[1]["cells"][0].as<std::string>()=="B",
        "unknown is JSON null and carries no original hidden string");
    check(list.agentSelectProjectRow(0,true),"explicit control route selects the ordinary shown first band");
    paint(list,canvas);check(list.agentControlRows(controls)&&controls.size()==2,"ordinary selection repaints fresh controls");
    list.clearList();list.addRow(1,"");list.addRow(1,"B");paint(list,canvas);
    check(list.agentControlRows(controls)&&controls.size()==1&&controls[0].logicalIndex==1,"blank first row cannot reveal control existence");
    SDL_Rect clip={0,0,20,24};SDL_SetClipRect(list.getSurface(),&clip);paint(list,canvas);
    check(!list.agentRows(labels)&&!list.agentControlRows(controls),"partial destination clip invalidates text and controls");
    SDL_SetClipRect(list.getSurface(),nullptr);paint(list,canvas);
    check(list.agentControlRows(controls)&&controls.size()==1,"complete destination clip restores fresh proof");
    SDL_SetColorKey(list.getSurface(),0,0);paint(list,canvas);
    check(!list.agentRows(labels)&&!list.agentControlRows(controls),"opaque/malformed list layer is unsupported");
    SDL_SetColorKey(list.getSurface(),SDL_SRCCOLORKEY,0);paint(list,canvas);
    check(list.agentControlRows(controls)&&controls.size()==1,"transparent-zero list layer restores proof");
    Surface overlay(40,8,10,18);overlay.setPalette(palette);overlay.drawRect(0,0,40,8,20);overlay.blit(&canvas);
    order.clear();list.agentPaintOrder(order);order.push_back(&overlay);projected=AgentUiState();
    check(agentProjectList(&list,&list,order,1,true,projected,projection,{},false,true)&&projected.rows.empty(),
        "whole click-band overlay removes all control authority");
    list.setY(-1);paint(list,canvas);
    check(!list.agentControlRows(controls),"partly offscreen list loses all control authority");
    list.setY(10);paint(list,canvas);
    check(list.agentControlRows(controls)&&controls.size()==1,"restored list requires new paint and fresh binding");
    const auto revision=list.agentListRevision();list.setCellText(1,0,"A");list.setCellText(1,0,"B");
    check(list.agentListRevision()!=revision&&!list.agentControlRows(controls),"away-back text mutation remains dirty and changes generation");
    paint(list,canvas);check(list.agentControlRows(controls),"fresh repaint after ABA is admitted under new generation");
}
void budgetTests()
{
    Font big,small;for(const auto &font:YAML::LoadFile("Language/Font.dat")["fonts"])
    {if(font["id"].as<std::string>()=="FONT_BIG")big.load(font);if(font["id"].as<std::string>()=="FONT_SMALL")small.load(font);}
    Language language;SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};
    Surface canvas(640,400),background(640,400);canvas.setPalette(palette);background.setPalette(palette);
    TextList list(640,400);list.setPalette(palette);list.initText(&big,&small,&language);list.setColor(80);
    list.setColumns(16,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40);list.setFlooding(true);
    for(unsigned i=0;i<50;++i)list.addRow(16,"","","","","","","","","","","","","","","","");
    paint(list,canvas);std::vector<AgentListRow> rows;
    check(!list.agentRows(rows),"2Mi scan work budget fails closed on many flooding cells");
    TextList dense(640,400);setup(dense,small,language,background,palette);
    std::string text;for(unsigned i=0;i<100;++i)text+='W';
    for(unsigned i=0;i<50;++i)dense.addRow(1,text.c_str());
    paint(dense,canvas);check(!dense.agentRows(rows),"opaque run budget fails closed on dense visible list");
    check(agentPaintHealthy(),"budget exhaustion never marks ordinary SDL paint as failed");
}
}
int main(int argc,char**argv)
{
    check(argc==2,"source path provided");(void)argv;
    check(SDL_GetVideoSurface()==nullptr&&!SDL_WasInit(SDL_INIT_VIDEO),"offscreen without GUI");
    sweepTests();budgetTests();glyphTests(std::getenv("OPENXCOM_AGENT_TEST_PACKET_DIR"));
    check(agentPaintHealthy(),"all ordinary raster calls healthy");
    std::cout<<"AgentInkCoverageTest PASS "<<checks<<" checks\n";
}
