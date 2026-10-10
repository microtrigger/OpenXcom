// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// This file is free software: you may redistribute and/or modify it under
// GNU GPL version 3 or, at your option, any later version.
// Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
#include "../../src/Observer/PresentedCursor.h"
#include "../../src/Observer/PresentedCursorSurface.h"
#include "../../src/Observer/CursorFileObserver.h"
#include <cassert>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
using namespace OpenXcom::Observer;
static std::string readFile(const std::string &p)
{ std::ifstream in(p.c_str()); std::ostringstream out; out << in.rdbuf(); return out.str(); }
static void writeFile(const std::string &p, const std::string &s)
{ std::ofstream out(p.c_str()); out << s; out.close(); assert(chmod(p.c_str(), 0600) == 0); }
static std::string temporaryDirectory()
{ char p[] = "/tmp/openxcom-cursor-test-XXXXXX"; char *d = mkdtemp(p); assert(d); return d; }
static void cleanup(const std::string &d)
{
 for (const char *n : {"cursor.json", "cursor.pending", "cursor.lock", "victim"}) unlink((d + "/" + n).c_str());
 assert(rmdir(d.c_str()) == 0);
}
static PixelCrop sample()
{ PixelCrop c; assert(cursorCropBounds(320,200,160,100,c)); c.rgb.assign(c.width*c.height*3, 7); assert(validCrop(c)); return c; }
static void boundsTests()
{
 PixelCrop c;
 assert(cursorCropBounds(320,200,160,100,c)); assert(c.x==96 && c.y==36 && c.width==128 && c.height==128);
 assert(cursorCropBounds(320,200,0,0,c)); assert(c.x==0 && c.y==0 && c.width==64 && c.height==64);
 assert(cursorCropBounds(320,200,319,199,c)); assert(c.x==255 && c.y==135 && c.width==65 && c.height==65);
 assert(cursorCropBounds(1,1,0,0,c)); assert(c.width==1 && c.height==1);
 for (int bad : {INT_MIN, -1, 0, 4097, INT_MAX}) {
  assert(!cursorCropBounds(bad,200,0,0,c)); assert(!validCrop(c));
  assert(!cursorCropBounds(320,bad,0,0,c));
 }
 for (int bad : {INT_MIN, -1, 320, INT_MAX}) assert(!cursorCropBounds(320,200,bad,0,c));
 for (int bad : {INT_MIN, -1, 200, INT_MAX}) assert(!cursorCropBounds(320,200,0,bad,c));
 assert(cursorCropBounds(4096,4096,4095,4095,c));
 c = sample(); c.rgb.pop_back(); assert(!validCrop(c));
 c = sample(); c.width=INT_MAX; assert(!validCrop(c));
 c = sample(); c.x=INT_MAX; assert(!validCrop(c));
 c = sample(); c.y=INT_MIN; assert(!validCrop(c));
 c = sample(); c.height=0; assert(!validCrop(c));
 c = sample(); c.screenWidth=INT_MIN; assert(!validCrop(c));
}
static void cacheTests()
{
 const std::string session(32,'a'); PresentedCursorCache a(session), b(session);
 assert(a.revision()==0 && a.snapshot()==b.snapshot()); assert(!a.close());
 assert(a.snapshot().find("\"freshness\":\"unverified\"")!=std::string::npos);
 assert(a.snapshot().find("\"crop\":null")!=std::string::npos);
 PixelCrop c=sample(); assert(a.observe(c) && b.observe(c)); assert(a.snapshot()==b.snapshot());
 const std::string original=a.snapshot();
 for (int i=0;i<1000;++i) assert(!a.observe(c) && a.snapshot()==original && a.revision()==1);
 ++c.rgb.back(); assert(a.observe(c) && a.revision()==2 && a.snapshot()!=original);
 assert(a.close() && a.revision()==3); assert(!a.close() && a.revision()==3);
 assert(!a.observe(PixelCrop()) && a.revision()==3);
 assert(a.observe(c)); c.rgb.clear(); assert(a.observe(c) && a.revision()==5);
 for (const std::string &s : {std::string(), std::string(32,'z'), std::string(31,'a'), std::string(33,'a')}) {
  bool rejected=false; try { PresentedCursorCache bad(s); } catch(const std::invalid_argument &) { rejected=true; } assert(rejected);
 }
}
static SDL_Surface *surface(int w,int h,int depth=8)
{
 SDL_Surface *s=SDL_CreateRGBSurface(SDL_SWSURFACE,w,h,depth,0,0,0,0); assert(s);
 if (depth==8) {
  SDL_Color colors[256]={}; colors[3].r=colors[4].r=12; colors[3].g=colors[4].g=34; colors[3].b=colors[4].b=56;
  assert(SDL_SetColors(s,colors,0,256)); assert(SDL_FillRect(s,0,3)==0);
 }
 return s;
}
static void surfaceTests()
{
 assert(!validCrop(capturePresentedCursor(0,320,200,0,0,true)));
 for(int scale=1;scale<=6;++scale) {
  SDL_Surface *s=surface(320*scale,200*scale);
  PixelCrop c=capturePresentedCursor(s,320,200,160,100,true); assert(validCrop(c));
  assert(c.x==160*scale-64 && c.y==100*scale-64 && c.width==128 && c.height==128);
  for(std::size_t i=0;i<c.rgb.size();i+=3) assert(c.rgb[i]==12 && c.rgb[i+1]==34 && c.rgb[i+2]==56);
  PresentedCursorCache cache(std::string(32,'a')); assert(cache.observe(c));
  assert(SDL_FillRect(s,0,4)==0); PixelCrop same=capturePresentedCursor(s,320,200,160,100,true);
  assert(c.rgb==same.rgb && !cache.observe(same) && cache.revision()==1);
  // Palette indices are not public identity. Only resulting RGB bytes count.
  s->format->palette->colors[4].r=13;
  assert(cache.observe(capturePresentedCursor(s,320,200,160,100,true)) && cache.revision()==2);
  assert(!validCrop(capturePresentedCursor(s,320,200,160,100,false)));
  for(Uint32 flag : {Uint32(SDL_OPENGL),Uint32(SDL_HWSURFACE),Uint32(SDL_DOUBLEBUF),Uint32(SDL_ASYNCBLIT)}) {
   Uint32 saved=s->flags; s->flags|=flag; assert(!validCrop(capturePresentedCursor(s,320,200,1,1,true))); s->flags=saved;
  }
  for(int bad : {INT_MIN, -1, 0, INT_MAX}) assert(!validCrop(capturePresentedCursor(s,bad,200,1,1,true)));
  for(int bad : {INT_MIN, -1, 320, INT_MAX}) assert(!validCrop(capturePresentedCursor(s,320,200,bad,1,true)));
  int nc=s->format->palette->ncolors; s->format->palette->ncolors=255; assert(!validCrop(capturePresentedCursor(s,320,200,1,1,true))); s->format->palette->ncolors=nc;
  SDL_FreeSurface(s);
 }
 for(int depth : {16,24,32}) { SDL_Surface *s=surface(320,200,depth); assert(!validCrop(capturePresentedCursor(s,320,200,1,1,true))); SDL_FreeSurface(s); }
 for(int width : {319,321,640,2240}) { SDL_Surface *s=surface(width,200); assert(!validCrop(capturePresentedCursor(s,320,200,1,1,true))); SDL_FreeSurface(s); }
 SDL_Surface *s=surface(2240,1400); assert(!validCrop(capturePresentedCursor(s,320,200,1,1,true))); SDL_FreeSurface(s);
}
static void malformedAndPaddedSurfaceTests()
{
 SDL_Surface *s=surface(321,201); // SDL pads indexed rows; never treat pitch as width.
 PixelCrop original=capturePresentedCursor(s,321,201,160,100,true); assert(validCrop(original));
 unsigned char *bytes=static_cast<unsigned char *>(s->pixels);
 bytes[0]=0; // A changed pixel outside the crop must have no public effect.
 assert(capturePresentedCursor(s,321,201,160,100,true).rgb==original.rgb);
 bytes[original.y*s->pitch+original.x]=0;
 PixelCrop changed=capturePresentedCursor(s,321,201,160,100,true);
 assert(changed.rgb[0]==0 && changed.rgb[1]==0 && changed.rgb[2]==0);
 assert(changed.rgb[3]==12 && changed.rgb[4]==34 && changed.rgb[5]==56);
 const auto rejected=[&]() { assert(!validCrop(capturePresentedCursor(s,321,201,160,100,true))); };
 int width=s->w; for(int bad : {INT_MIN,0,4097,INT_MAX}) { s->w=bad; rejected(); } s->w=width;
 int height=s->h; for(int bad : {INT_MIN,0,4097,INT_MAX}) { s->h=bad; rejected(); } s->h=height;
 Uint16 pitch=s->pitch; s->pitch=1; rejected(); s->pitch=pitch;
 void *pixels=s->pixels; s->pixels=0; rejected(); s->pixels=pixels;
 SDL_PixelFormat *format=s->format; s->format=0; rejected(); s->format=format;
 SDL_Palette *palette=format->palette; format->palette=0; rejected(); format->palette=palette;
 SDL_Color *colors=palette->colors; palette->colors=0; rejected(); palette->colors=colors;
 Uint8 bpp=format->BytesPerPixel; format->BytesPerPixel=2; rejected(); format->BytesPerPixel=bpp;
 SDL_FreeSurface(s);
}
static void fileTests()
{
 PixelCrop c=sample(); CursorFileObserver unset(0), empty(""); assert(!unset.enabled() && !empty.enabled());
 std::string d=temporaryDirectory(), observed;
 {
  CursorFileObserver sink(d.c_str()); assert(sink.enabled()); CursorFileObserver other(d.c_str()); assert(!other.enabled());
  sink.observe(c); observed=readFile(d+"/cursor.json"); assert(observed.find("\"public_revision\":1")!=std::string::npos);
  struct stat before,after; assert(stat((d+"/cursor.json").c_str(),&before)==0); assert((before.st_mode&0777)==0600);
  assert(stat((d+"/cursor.lock").c_str(),&after)==0 && (after.st_mode&0777)==0600);
  sink.observe(c); assert(stat((d+"/cursor.json").c_str(),&after)==0); assert(before.st_ino==after.st_ino && observed==readFile(d+"/cursor.json"));
 }
 assert(readFile(d+"/cursor.json").find("\"crop\":null")!=std::string::npos);
 { CursorFileObserver restarted(d.c_str()); assert(restarted.enabled()); restarted.observe(c); assert(readFile(d+"/cursor.json")!=observed); }
 assert(chmod(d.c_str(),0755)==0); { CursorFileObserver bad(d.c_str()); assert(!bad.enabled()); } assert(chmod(d.c_str(),0700)==0);
 std::string alias=d+"-link"; assert(symlink(d.c_str(),alias.c_str())==0); { CursorFileObserver bad(alias.c_str()); assert(!bad.enabled()); } unlink(alias.c_str()); cleanup(d);
 // Existing pending files, symlinks and hardlinks are never overwritten.
 for(int kind=0;kind<3;++kind) {
  d=temporaryDirectory(); writeFile(d+"/victim","untouched");
  if(kind==0) writeFile(d+"/cursor.pending","stale");
  if(kind==1) assert(symlink("victim",(d+"/cursor.pending").c_str())==0);
  if(kind==2) assert(link((d+"/victim").c_str(),(d+"/cursor.pending").c_str())==0);
  { CursorFileObserver bad(d.c_str()); assert(!bad.enabled()); }
  assert(readFile(d+"/victim")=="untouched"); assert(readFile(d+"/cursor.pending")== (kind==0?"stale":"untouched")); cleanup(d);
 }
 // Lock symlinks, hardlinks, public regular files and nonregular entries fail closed.
 for(int kind=0;kind<4;++kind) {
  d=temporaryDirectory(); writeFile(d+"/victim","untouched");
  if(kind==0) assert(symlink("victim",(d+"/cursor.lock").c_str())==0);
  if(kind==1) assert(link((d+"/victim").c_str(),(d+"/cursor.lock").c_str())==0);
  if(kind==2) { writeFile(d+"/cursor.lock","lock"); assert(chmod((d+"/cursor.lock").c_str(),0644)==0); }
  if(kind==3) assert(mkfifo((d+"/cursor.lock").c_str(),0600)==0);
  { CursorFileObserver bad(d.c_str()); assert(!bad.enabled()); } assert(readFile(d+"/victim")=="untouched"); cleanup(d);
 }
 // Destination links are replaced atomically, never followed into their targets.
 for(int kind=0;kind<2;++kind) {
  d=temporaryDirectory(); writeFile(d+"/victim","untouched");
  if(kind==0) assert(symlink("victim",(d+"/cursor.json").c_str())==0);
  else assert(link((d+"/victim").c_str(),(d+"/cursor.json").c_str())==0);
  { CursorFileObserver sink(d.c_str()); assert(sink.enabled()); sink.observe(c); }
  assert(readFile(d+"/victim")=="untouched"); struct stat st; assert(lstat((d+"/cursor.json").c_str(),&st)==0 && S_ISREG(st.st_mode) && st.st_nlink==1); cleanup(d);
 }
 d=temporaryDirectory(); {
  CursorFileObserver sink(d.c_str()); assert(sink.enabled()); sink.observe(c); const std::string old=readFile(d+"/cursor.json");
  writeFile(d+"/cursor.pending","stale"); ++c.rgb[0]; sink.observe(c); assert(!sink.enabled());
  assert(readFile(d+"/cursor.json")==old && readFile(d+"/cursor.pending")=="stale");
 } cleanup(d);
}
int main()
{
 boundsTests(); cacheTests(); surfaceTests(); malformedAndPaddedSurfaceTests(); fileTests();
 std::cout << "PASS: synthetic cursor pixels, bounded crops, public revisions, conservative profiles, private fail-closed files\n";
}
