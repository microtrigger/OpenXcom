// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
// Digit patterns adapted from src/Interface/NumberText.cpp;
// Copyright 2010-2016 OpenXcom Developers; GPL-3.0-or-later.
#include "../../src/Observer/PresentedHud.h"
#include <cassert>
#include <iostream>
using namespace OpenXcom::Observer;
static const char *glyphs[10] = {
 "111101101101111", "010110010010111", "111001111100111", "111001111001111",
 "101101111001001", "111100111001111", "111100111101111", "111001001001001",
 "111101111101111", "111101111001111"
};
std::vector<unsigned char> render(const std::string &number, int width)
{
 std::vector<unsigned char> result(width * 5, 0);
 for (std::size_t n = 0; n < number.size(); ++n)
  for (int y = 0; y < 5; ++y)
   for (int x = 0; x < 3 && static_cast<int>(n * 4) + x < width; ++x)
    result[y * width + n * 4 + x] = glyphs[number[n] - '0'][y * 3 + x] == '1';
 return result;
}
int main()
{
 unsigned int value = 0;
 for (int i = 0; i < 10; ++i) {
  assert(decodeHudDigits(render(std::to_string(i), 15), 15, value)); assert(value == static_cast<unsigned>(i));
 }
 for (const char *s : {"0", "58", "100", "999"}) {
  assert(decodeHudDigits(render(s, 15), 15, value)); assert(value == std::stoul(s));
 }
 assert(!decodeHudDigits(render("9999", 19), 19, value));
 for (const char *s : {"9999", "99990", "10000", "4294967295", "00", "01", ""})
  assert(!decodeHudDigits(render(s, 15), 15, value));
 assert(render("9999",15) == render("99990",15));
 auto mask = render("58",15); mask[14] = 1; assert(!decodeHudDigits(mask,15,value));
 mask = render("58",15); mask[3] = 1; assert(!decodeHudDigits(mask,15,value));
 mask = render("58",15); mask[0] = 0; assert(!decodeHudDigits(mask,15,value));
 for (int scale = 1; scale <= 6; ++scale) {
  SDL_Surface *s = SDL_CreateRGBSurface(SDL_SWSURFACE,80*scale,40*scale,8,0,0,0,0); assert(s);
  SDL_Color colors[256] = {}; colors[1].r = 240; colors[1].g = 200; colors[1].b = 80;
  SDL_SetColors(s, colors,0,256);
  HudFrame frame;
  for (int i = 0; i < 4; ++i) {
   frame[i].x = 2+i*18; frame[i].y = 2; frame[i].width = 15; frame[i].height = 5; frame[i].foreground = 1;
   auto m = render(i == 0 ? "58" : "100",15);
   for(int y=0;y<5*scale;++y) for(int x=0;x<15*scale;++x)
    static_cast<unsigned char *>(s->pixels)[(2*scale+y)*s->pitch+(frame[i].x*scale+x)] = m[(y/scale)*15+x/scale];
  }
  const std::string expected="{\"time_units\":58,\"energy\":100,\"health\":100,\"morale\":100}";
  assert(provePresentedHud(s,80,40,frame,true)==expected);
  // Different palette indices with identical RGB must yield identical output.
  colors[2]=colors[1];SDL_SetColors(s,colors,0,256);
  for(int y=0;y<s->h;++y) for(int x=0;x<s->w;++x) {
   auto &pixel=static_cast<unsigned char *>(s->pixels)[y*s->pitch+x]; if(pixel==1) pixel=2;
  }
  assert(provePresentedHud(s,80,40,frame,true)==expected);
  // No raw source numeral is present in the candidate; only final glyphs decide.
  auto replacement=render("59",15);
  for(int y=0;y<5*scale;++y) for(int x=0;x<15*scale;++x)
   static_cast<unsigned char *>(s->pixels)[(2*scale+y)*s->pitch+2*scale+x]=replacement[(y/scale)*15+x/scale];
  assert(provePresentedHud(s,80,40,frame,true)=="{\"time_units\":59,\"energy\":100,\"health\":100,\"morale\":100}");
  replacement=render("58",15);
  for(int y=0;y<5*scale;++y) for(int x=0;x<15*scale;++x)
   static_cast<unsigned char *>(s->pixels)[(2*scale+y)*s->pitch+2*scale+x]=replacement[(y/scale)*15+x/scale];
  assert(provePresentedHud(s,80,40,frame,false)=="null");
  HudFrame overlap=frame;overlap[1]=overlap[0];assert(provePresentedHud(s,80,40,overlap,true)=="null");
  HudFrame missing=frame;missing[2]=HudDigits();assert(provePresentedHud(s,80,40,missing,true)=="null");
  HudFrame clipped=frame;clipped[0].x=-1;assert(provePresentedHud(s,80,40,clipped,true)=="null");
  clipped=frame;clipped[0].height=7;assert(provePresentedHud(s,80,40,clipped,true)=="null");
  // Aliased background and foreground erase contrast without changing indices.
  colors[0]=colors[1];SDL_SetColors(s,colors,0,256);assert(provePresentedHud(s,80,40,frame,true)=="null");
  colors[0]={};SDL_SetColors(s,colors,0,256);
  // An appended partial digit or one changed subpixel fails the whole proof.
  auto *pixels=static_cast<unsigned char *>(s->pixels);
  pixels[2*scale*s->pitch+16*scale]=1;assert(provePresentedHud(s,80,40,frame,true)=="null");
  pixels[2*scale*s->pitch+16*scale]=0;
  pixels[2*scale*s->pitch+2*scale]=0;assert(provePresentedHud(s,80,40,frame,true)=="null");
  SDL_FreeSurface(s);
 }
 std::cout << "PASS: final RGB HUD decoding, clipping equivalence, masks, contrast and scales1..6\n";
}
