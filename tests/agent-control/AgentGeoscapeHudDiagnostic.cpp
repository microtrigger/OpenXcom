// Off-screen diagnostic only: ordinary Geoscape text dimensions and paint order.
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Interface/Text.h"
#include "../../src/Observer/AgentUi.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <vector>
using namespace OpenXcom;
int main()
{
    if (SDL_Init(0) != 0 || SDL_GetVideoSurface()) return 1;
    Options::baseXResolution=320; Options::baseYResolution=200;
    Font big,small;
    for (const auto &font : YAML::LoadFile("Language/Font.dat")["fonts"])
    {
        if(font["id"].as<std::string>()=="FONT_BIG") big.load(font);
        if(font["id"].as<std::string>()=="FONT_SMALL") small.load(font);
    }
    Language language;
    std::cout << "fonts big="<<big.getWidth()<<'x'<<big.getHeight()<<" spacing="<<big.getSpacing()
      <<" small="<<small.getWidth()<<'x'<<small.getHeight()<<" spacing="<<small.getSpacing()<<'\n';
    for(Font *font : {&big,&small}) for(UCode ch : {'0','1',':','F','r','i','d','a','y'})
    {
        Surface *glyph=font->getChar(ch);const SDL_Rect crop=*glyph->getCrop();
        int left=crop.w,top=crop.h,right=-1,bottom=-1;
        for(int y=0;y<crop.h;++y)for(int x=0;x<crop.w;++x)
            if(glyph->getPixel(crop.x+x,crop.y+y)){left=std::min(left,x);top=std::min(top,y);right=std::max(right,x);bottom=std::max(bottom,y);}
        std::cout<<"glyph "<<(font==&big?"big":"small")<<' '<<char(ch)<<" crop="<<crop.w<<','<<crop.h
          <<" actualInk="<<left<<','<<top<<','<<(right-left+1)<<','<<(bottom-top+1)<<'\n';
    }
    for (bool showFunds : {false,true})
    {
        Surface canvas(320,200), background(320,200);
        Text funds(59,8,259,73), hour(20,16,259,74), hourSep(4,16,279,74), minute(20,16,283,74), minSep(4,16,303,74),
          second(11,8,307,80), weekday(59,8,259,87), day(29,8,259,94), month(29,8,288,94), year(59,8,259,101), debug(200,32,0,0);
        std::vector<Text*> fields={&funds,&hour,&hourSep,&minute,&minSep,&second,&weekday,&day,&month,&year,&debug};
        const char *names[]={"funds","hour","hourSep","minute","minSep","second","weekday","day","month","year","debug"};
        const char *values[]={"$6,000,000","12",":","34",":","05","Friday","1st","Jan","1999",""};
        std::vector<Surface*> order={&background};
        for(unsigned i=0;i<fields.size();++i){ fields[i]->initText(&big,&small,&language); fields[i]->setText(values[i]); order.push_back(fields[i]); }
        funds.setAlign(ALIGN_CENTER); funds.setVisible(showFunds);
        hour.setBig();hour.setAlign(ALIGN_RIGHT);hourSep.setBig();minute.setBig();minSep.setBig();
        weekday.setAlign(ALIGN_CENTER);day.setAlign(ALIGN_CENTER);month.setAlign(ALIGN_CENTER);year.setAlign(ALIGN_CENTER);
        if(showFunds){hour.setY(hour.getY()+6);hour.setSmall();hourSep.setY(hourSep.getY()+6);hourSep.setSmall();minute.setY(minute.getY()+6);minute.setSmall();minSep.setX(minSep.getX()-10);minSep.setY(minSep.getY()+6);minSep.setSmall();second.setX(second.getX()-10);}
        for(auto *widget:order) widget->blit(&canvas);
        std::cout << "showFunds="<<showFunds<<'\n';
        for(unsigned i=0;i<fields.size();++i){
            const Text *text=fields[i];SDL_Rect ink={};bool hasInk=text->agentPaintedBounds(ink);
            std::cout<<names[i]<<" context="<<text->agentContextVisible()<<" complete="<<text->agentTextComplete()
             <<" allocation="<<text->getX()<<','<<text->getY()<<','<<text->getWidth()<<','<<text->getHeight()
             <<" textSize="<<text->getTextWidth()<<','<<text->getTextHeight()<<" ink="<<hasInk<<':'<<ink.x<<','<<ink.y<<','<<ink.w<<','<<ink.h
             <<" json="<<Observer::agentTextJson(text)<<" uncovered="<<Observer::agentWidgetUncovered(text,order);
            int left=text->getWidth(),top=text->getHeight(),right=-1,bottom=-1;
            for(int y=0;y<text->getHeight();++y)for(int x=0;x<text->getWidth();++x)
                if(text->getPixel(x,y)){left=std::min(left,x);top=std::min(top,y);right=std::max(right,x);bottom=std::max(bottom,y);}
            std::cout<<" actualPixelInk="<<left<<','<<top<<','<<(right-left+1)<<','<<(bottom-top+1)<<'\n';
        }
    }
    return SDL_GetVideoSurface() || SDL_WasInit(SDL_INIT_VIDEO) ? 1:0;
}
