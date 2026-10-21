// SPDX-License-Identifier: GPL-3.0-or-later
// Real stock glyph raster/blit proof, with inherited explicit owning state seams.
#define main retainedOutfittingFixtureMain
#include "AgentOutfittingProviderTestV21.cpp"
#undef main
// Frozen v21 ordinary wheel rule, independent of current scrollLine implementation.
int oldTextScroll(bool scroll,int height,int textHeight,int step,int offset,bool down)
{ const int area=height-textHeight; return scroll && area<0 ? std::max(area,std::min(0,offset+(down?-step:step))) : offset; }
namespace {
void repaint(Text &text,Surface &canvas) {canvas.clear(40);text.blit(&canvas);}
std::vector<Text::AgentVisibleLine> shown(Text &text,Surface &canvas)
{std::vector<Text::AgentVisibleLine> v;check(text.agentVisibleLines(&canvas,v),"qualified actual line paint");return v;}
void lineTests(Font&big,Font&small,Language&lang,const std::string&source,const std::string&game)
{
 Paint p(big,small,lang,source,game,"storesInfo");p.canvas.setHeight(400);
 // Actual Screen disables destination color key; source text remains keyed.
 check(SDL_SetColorKey(p.canvas.getSurface(),0,0)==0,"opaque final Screen canvas");
 Text text(200,80,10,30);text.initText(&big,&small,&lang);text.setPalette(p.palette);text.setColor(2);text.setWordWrap(true);text.setScrollable(true);text.agentEnableVisibleLines();
 text.setText("Visible line\nSecond line");repaint(text,p.canvas);auto lines=shown(text,p.canvas);

 check(lines.size()==2&&lines[0].complete&&lines[0].text=="Visible line"&&lines[1].text=="Second line","two actual glyph lines complete");
 // Unsupported format flags must not borrow authority from a previous good blit.
 for(int mode=0;mode<5;++mode)
 {
  const pid_t child=fork();check(child>=0,"format guard child");
  if(!child){
   SDL_Surface *surface=mode<3?text.getSurface():p.canvas.getSurface();
   if(mode==0)SDL_SetColorKey(surface,0,0);
   else if(mode==2 || mode==4)SDL_SetAlpha(surface,SDL_SRCALPHA,128);
   else SDL_SetColorKey(surface,SDL_SRCCOLORKEY,1);
   text.blit(&p.canvas);std::vector<Text::AgentVisibleLine> rejected;
   if(text.agentVisibleLines(&p.canvas,rejected))_exit(2);
   if(mode<3) {SDL_SetAlpha(surface,0,255);SDL_SetColorKey(surface,SDL_SRCCOLORKEY,0);}
   else {SDL_SetAlpha(surface,0,255);SDL_SetColorKey(surface,0,0);}
   if(text.agentVisibleLines(&p.canvas,rejected))_exit(3);
   repaint(text,p.canvas);
   const bool available=text.agentVisibleLines(&p.canvas,rejected);
   if(available!=agentPaintHealthy())_exit(4);
   _exit(0);
  }
  int status=0;check(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0,"unsupported flags and restore without repaint refuse");
 }
 SDL_SetColorKey(p.canvas.getSurface(),SDL_SRCCOLORKEY,0);repaint(text,p.canvas);
 check(text.agentVisibleLines(&p.canvas,lines)&&lines.size()==2&&lines[0].complete,"legacy zero-key canvas still supported");
 SDL_SetColorKey(p.canvas.getSurface(),0,0);repaint(text,p.canvas);
 check(text.agentVisibleLineControl(&p.canvas),"separate shown scroll control");const auto initial=text.agentPresentationRevision();repaint(text,p.canvas);check(text.agentPresentationRevision()==initial,"unchanged repaint preserves mutation binding");
 text.setText(text.getText());check(text.agentPresentationRevision()!=initial,"same text setter invalidates binding");std::vector<Text::AgentVisibleLine> none;check(!text.agentVisibleLines(&p.canvas,none),"same text requires fresh paint");repaint(text,p.canvas);
 const auto before=text.agentPresentationRevision();text.setColor(3);text.setColor(2);check(text.agentPresentationRevision()!=before&&!text.agentVisibleLines(&p.canvas,none),"color ABA invalidates");repaint(text,p.canvas);
 const auto moved=text.agentPresentationRevision();text.setX(11);text.setX(10);check(text.agentPresentationRevision()!=moved&&!text.agentVisibleLines(&p.canvas,none),"position ABA invalidates");repaint(text,p.canvas);
 Surface *base=&text;const auto hidden=text.agentPresentationRevision();base->setHidden(true);base->setHidden(false);check(text.agentPresentationRevision()!=hidden&&!text.agentVisibleLines(&p.canvas,none),"base-pointer hidden ABA invalidates");repaint(text,p.canvas);
 const auto visibility=text.agentPresentationRevision();base->setVisible(false);base->setVisible(true);check(text.agentPresentationRevision()!=visibility&&!text.agentVisibleLines(&p.canvas,none),"base-pointer visible ABA invalidates");repaint(text,p.canvas);
 const auto resized=text.agentPresentationRevision();base->setWidth(201);base->setWidth(200);base->setHeight(81);base->setHeight(80);check(text.agentPresentationRevision()!=resized&&!text.agentVisibleLines(&p.canvas,none),"base-pointer resize ABA needs repaint");repaint(text,p.canvas);
 SDL_Color alternate[256];std::copy(p.palette,p.palette+256,alternate);alternate[2].r^=1;const auto recolored=text.agentPresentationRevision();base->setPalette(alternate);base->setPalette(p.palette);check(text.agentPresentationRevision()!=recolored&&!text.agentVisibleLines(&p.canvas,none),"base-pointer palette ABA needs repaint");repaint(text,p.canvas);
 SDL_Rect clip={0,0,200,35};SDL_SetClipRect(p.canvas.getSurface(),&clip);text.blit(&p.canvas);SDL_SetClipRect(p.canvas.getSurface(),nullptr);check(!text.agentVisibleLines(&p.canvas,none),"destination clip paint restore cannot revive");repaint(text,p.canvas);check(text.agentVisibleLines(&p.canvas,none),"full repaint restores");
 *text.getCrop()={0,0,50,50};text.blit(&p.canvas);text.resetCrop();check(!text.agentVisibleLines(&p.canvas,none),"source crop paint restore cannot revive");repaint(text,p.canvas);
 auto v=shown(text,p.canvas);check(!v.empty()&&!v[0].pixels.empty(),"known glyph pixel exists");auto pixel=v[0].pixels[0];p.canvas.setPixel(text.getX()+pixel.x,text.getY()+pixel.y,40);v=shown(text,p.canvas);check(!v.empty()&&!v[0].complete,"one overwritten required glyph pixel withholds complete line");
 text.setText("Alpha  \t Beta\nGamma");repaint(text,p.canvas);v=shown(text,p.canvas);check(v.size()==2&&v[0].complete&&v[0].text=="Alpha Beta","witnessed internal whitespace normalized");
 text.setText(std::string("Alpha")+char(1)+" Beta");repaint(text,p.canvas);v=shown(text,p.canvas);check(v.size()==1&&v[0].complete&&v[0].text=="Alpha Beta","renderer color marker omitted");
 text.setText(std::string("Alpha ")+"\xf4\x8f\xbf\xbf");repaint(text,p.canvas);v=shown(text,p.canvas);check(v.size()==1&&!v[0].complete,"fallback glyph never reconstructs missing original");
 text.setHeight(7);text.setText("Partial line");repaint(text,p.canvas);v=shown(text,p.canvas);check(v.size()==1&&!v[0].complete,"partial bottom advance band yields unknown line");
 text.setHeight(80);text.setWidth(20);text.setWordWrap(false);text.setText("long_word_outside_width");repaint(text,p.canvas);v=shown(text,p.canvas);check(v.size()==1&&!v[0].complete,"horizontal clipping withholds full line");
 text.setWidth(200);text.setHeight(400);text.setY(0);std::string many;for(int i=0;i<40;++i)many+="Line\n";text.setText(many);repaint(text,p.canvas);check(!text.agentVisibleLines(&p.canvas,none),"more than32 viewport candidates fails closed");
 text.setHeight(80);text.setY(30);text.setText("Visible line");repaint(text,p.canvas);auto palette=p.palette;SDL_Color changed[256];std::copy(palette,palette+256,changed);changed[2].r^=1;p.canvas.setPalette(changed);check(!text.agentVisibleLines(&p.canvas,none),"palette mismatch invalidates");p.canvas.setPalette(p.palette);repaint(text,p.canvas);
 // A glyph wholly lost after compositing cannot be inferred from its neighbors.
 text.setText("Visible line");repaint(text,p.canvas);v=shown(text,p.canvas);check(v.size()==1&&v[0].complete,"composed stock kerning keeps ordinary line");
 const auto firstGlyph=v[0].pixels.front().glyph;for(const auto &px:v[0].pixels)if(px.glyph==firstGlyph)p.canvas.setPixel(text.getX()+px.x,text.getY()+px.y,40);v=shown(text,p.canvas);check(v.size()==1&&!v[0].complete,"fully destroyed glyph withholds whole line");
 text.setText("Before");repaint(text,p.canvas);text.setText("Different");text.setText("Before");check(!text.agentVisibleLines(&p.canvas,none),"content ABA cannot revive without paint");repaint(text,p.canvas);check(text.agentVisibleLines(&p.canvas,none),"content ABA fresh paint recovers new authority");
 SDL_Rect sourceClip={0,0,10,5};SDL_SetClipRect(text.getSurface(),&sourceClip);text.blit(&p.canvas);SDL_SetClipRect(text.getSurface(),nullptr);check(!text.agentVisibleLines(&p.canvas,none),"source clip paint restore needs new blit");repaint(text,p.canvas);check(text.agentVisibleLines(&p.canvas,none),"full source blit recovers");
 bool crossLine=false;for(char first:std::string("gjpqy"))for(char second:std::string("AMW")){text.setText(std::string(6,first)+"\n"+std::string(6,second));repaint(text,p.canvas);v=shown(text,p.canvas);if(v.size()==2&&!v[0].complete&&!v[1].complete)crossLine=true;}check(crossLine,"later actual line overlapping prior descenders invalidates both");
 text.setText("A");repaint(text,p.canvas);text.agentBeginVisibleLines();text.agentStartVisibleLine(0,9);text._agentLinePixelCount=65536;text.agentCaptureVisibleGlyph('A',&small,small.getChar('A'),0,0,2,1,0);text.agentFinishVisibleLine();text.agentFinishVisibleLines();check(!text.agentVisibleLineSource(),"explicit captured-pixel work cap fails closed");text.invalidate();repaint(text,p.canvas);
 text.agentBeginVisibleLines();text.agentStartVisibleLine(0,9);text._agentLineWork=2097152;text.agentCaptureVisibleGlyph('A',&small,small.getChar('A'),0,0,2,1,0);text.agentFinishVisibleLine();text.agentFinishVisibleLines();check(!text.agentVisibleLineSource(),"explicit operation work cap fails closed");text.invalidate();repaint(text,p.canvas);
 const pid_t child=fork();check(child>=0,"paint failure child");if(!child){agentObservePaint(-1);if(text.agentVisibleLines(&p.canvas,none))_exit(2);repaint(text,p.canvas);if(text.agentVisibleLines(&p.canvas,none))_exit(3);_exit(0);}int status=0;check(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0,"failed paint never recovers by ordinary repaint");
 // Embedded v21 ordinary clamp oracle, independent of the current helper.
 for(const std::string content:{std::string(),std::string("Short"),many})for(bool scroll:{false,true})
 {
  Text helper(200,30),wheel(200,30);for(auto*t:{&helper,&wheel}){t->initText(&big,&small,&lang);t->setPalette(p.palette);t->setScrollable(scroll);t->setText(content);}
  int expected=0;for(int step=0;step<100;++step){const bool down=step<50;expected=oldTextScroll(scroll,helper.getHeight(),helper.getTextHeight(),small.getHeight()+small.getSpacing(),expected,down);helper.scrollLine(down);SDL_Event e={};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=down?SDL_BUTTON_WHEELDOWN:SDL_BUTTON_WHEELUP;Action a(&e,1,1,0,0);wheel.mousePress(&a,nullptr);helper.draw();wheel.draw();
   check(helper.getSurface()->pitch==wheel.getSurface()->pitch&&std::memcmp(helper.getSurface()->pixels,wheel.getSurface()->pixels,helper.getSurface()->pitch*helper.getHeight())==0,"shared helper and ordinary wheel pixel parity");
   check(agentTextOffset(helper)==expected&&agentTextOffset(wheel)==expected,"shared helper and GUI match embedded v21 clamp oracle");
  }
 }
}
}
void stockArticleOverlap(Font &big,Font &small,Language &lang,const std::string &game)
{
 // Reproduce actual stock pistol Text geometry, palette and static background,
 // not an ArticleState constructor or a live research-completion route.
 const bool underwater=game=="xcom2";
 const std::string root=std::string("../")+(underwater?"TFTD":"UFO");
 Palette palette;palette.loadDat(root+"/GEODATA/PALETTES.DAT",256,Palette::palOffset(underwater?1:4));
 Surface canvas(320,200),background(320,200);
 canvas.setPalette(palette.getColors());background.setPalette(palette.getColors());
 background.loadScr(root+"/GEOGRAPH/BACK08.SCR");
 if(underwater)
 {
  Surface picture(320,200);picture.setPalette(palette.getColors());
  picture.loadBdy(root+"/GEOGRAPH/UP031.BDY");picture.blit(&background);
 }
 check(SDL_SetColorKey(canvas.getSurface(),0,0)==0,"stock article opaque destination");
 Text text(underwater?157:300,underwater?136:56,underwater?163:8,underwater?34:138);
 text.initText(&big,&small,&lang);text.setPalette(palette.getColors());
 text.setColor(underwater?2:239);text.setWordWrap(true);text.setScrollable(true);text.agentEnableVisibleLines();
 text.setText(lang.getString(underwater?"STR_GAUSS_PISTOL_UFOPEDIA":"STR_LASER_PISTOL_UFOPEDIA"));
 background.blit(&canvas);text.blit(&canvas);
 auto lines=shown(text,canvas);
 check(lines.size()==(underwater?8u:3u),"stock pistol expected visual line count");
 unsigned partial=0,complete=0,collisions=0;
 for(size_t i=0;i<lines.size();++i)
 {
  const auto &line=lines[i];
  check(!line.text.empty()&&!line.pixels.empty(),"stock article typed line has raster witnesses");
  if(line.complete)++complete;else ++partial;
  // Check captured-source provenance before destination filtering as well:
  // cross-line overlap must withhold both typed lines even for equal colors.
  const auto &sourceLine=text._agentVisibleLines[i];
  check(line.complete==sourceLine.complete,"stock partial status originates at glyph composition");
  std::vector<bool> glyphs(line.glyphs,false);
  for(const auto &pixel:line.pixels)if(pixel.contributed)glyphs[pixel.glyph]=true;
  check(std::find(glyphs.begin(),glyphs.end(),false)==glyphs.end(),"partial stock line still has contribution from every glyph");
  for(size_t j=i+1;j<text._agentVisibleLines.size();++j)
   for(const auto &left:sourceLine.pixels)
    for(const auto &right:text._agentVisibleLines[j].pixels)
     if(left.x==right.x&&left.y==right.y)
     {
      ++collisions;
      check(!sourceLine.complete&&!text._agentVisibleLines[j].complete,"cross-line collision retains conservative invalidation");
     }
 }
 check(collisions>0&&partial==(underwater?7u:2u)&&complete==1&&lines.back().complete,
       "stock article preserves mixed partial lines and complete final line");
 // Opt-in UI fix changes actual row positions; the same conservative observer
 // must now prove every line without weakening the negative control above.
 const auto legacyHeight=text.getTextHeight();
 const auto oldRevision=text.agentPresentationRevision();
 text.setExtraLineSpacing(1);
 check(text.agentPresentationRevision()!=oldRevision,"spacing mutation invalidates old paint authority");
 check(text.getTextHeight()==legacyHeight+int(lines.size()),"row metrics include extra spacing");
 background.blit(&canvas);text.blit(&canvas);lines=shown(text,canvas);
 check(lines.size()==(underwater?8u:3u),"readable spacing preserves words and wrap width");
 for(const auto &line:lines)check(line.complete,"separated stock article rows fully witnessed");
 text.setHeight(18);text.setText("gggggg\nMMMMMM\nqqqqqq");
 background.blit(&canvas);text.blit(&canvas);lines=shown(text,canvas);
 check(agentTextOffset(text)==0,"spacing layout starts at top");
 text.scrollLine(true);check(agentTextOffset(text)==-9,"opt-in wheel advances one readable row");
 text.scrollLine(true);check(agentTextOffset(text)==-9,"opt-in wheel clamps at bottom");
 text.scrollLine(false);check(agentTextOffset(text)==0,"opt-in wheel returns to top");
 text.setExtraLineSpacing(-1);check(text.getTextHeight()==24,"negative extra spacing clamps to legacy zero");
}

// Stock HUD rows are seven pixels apart while their glyph ink can be eight high.
// Preserve refusal even when a later glyph overwrites with the same palette index.
void stockCalendarOverlap(Font &big,Font &small,Language &lang,const std::string &source,const std::string &game)
{
 Paint p(big,small,lang,source,game,"geoscape");
 Palette base;base.loadDat(std::string("../")+(game=="xcom1"?"UFO":"TFTD")+"/GEODATA/PALETTES.DAT",256,Palette::palOffset(0));
 for(unsigned i=0;i<256;++i)if(i<Palette::backPos||i>=Palette::backPos+16)p.palette[i]=base.getColors()[i];
 p.canvas.setPalette(p.palette);check(SDL_SetColorKey(p.canvas.getSurface(),0,0)==0,"calendar opaque canvas");
 Text weekday(59,8,259,87),day(29,8,259,94),month(29,8,288,94),year(59,8,259,101);
 std::vector<Surface*> order={&weekday,&day,&month,&year};
 for(auto *surface:order){Text *text=static_cast<Text*>(surface);text->initText(&big,&small,&lang);text->setPalette(p.palette);p.colors(text,"text");text->setAlign(ALIGN_CENTER);}
 weekday.setText(lang.getString("STR_THURSDAY"));day.setText("1st");month.setText(lang.getString("STR_JAN"));year.setText(game=="xcom1"?"1999":"2040");
 p.canvas.clear(40);for(auto *surface:order)surface->blit(&p.canvas);
 for(unsigned i=0;i<3;++i)
 {
  Text *text=static_cast<Text*>(order[i]);unsigned collisions=0,changed=0;
  for(int y=0;y<text->getHeight();++y)for(int x=0;x<text->getWidth();++x)
  {
   const Uint8 value=text->getPixel(x,y);if(!value)continue;
   const int xx=x+text->getX(),yy=y+text->getY();
   for(unsigned j=i+1;j<order.size();++j){Surface *later=order[j];const int lx=xx-later->getX(),ly=yy-later->getY();if(lx>=0&&ly>=0&&lx<later->getWidth()&&ly<later->getHeight()&&later->getPixel(lx,ly)){++collisions;break;}}
   if(p.canvas.getPixel(xx,yy)!=value)++changed;
  }
  check(text->agentTextComplete()&&Observer::agentTextJson(text)!="null","calendar source is complete before later paint");
  check(collisions>0&&changed>0,"calendar has real later-owned and overwritten ink, not only overlapping boxes");
  check(!Observer::agentWidgetUncovered(text,order)&&Observer::agentCompositeTextJson(text,320,200,order)=="null","overwritten calendar field remains unavailable");
 }
 check(Observer::agentWidgetUncovered(&year,order)&&Observer::agentTextJson(&year)!="null","last calendar row stays available");
 month.setText(lang.getString("STR_APR"));month.blit(&p.canvas);
 check(!month.agentTextComplete()&&Observer::agentTextJson(&month)=="null","stock descender month also clips in its own eight-pixel widget");
}

int main(int argc,char**argv)
{
 check(argc==2,"source root");check(!SDL_Init(0)&&!SDL_GetVideoSurface(),"offscreen only");Options::baseXResolution=320;Options::baseYResolution=400;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 for(const std::string game:{"xcom1","xcom2"}){Language lang;lang.loadFile(std::string(argv[1])+"/bin/standard/"+game+"/Language/en-US.yml");lineTests(big,small,lang,argv[1],game);stockArticleOverlap(big,small,lang,game);stockCalendarOverlap(big,small,lang,argv[1],game);}
 check(agentPaintHealthy()&&!SDL_GetVideoSurface(),"no paint failure or GUI");std::cout<<"PASS "<<checks<<" checks\n";
}
