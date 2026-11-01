"""Compile the actual Battlescape stats-footprint block with geometry-only seams."""
import pathlib, subprocess, tempfile, unittest
class StatsFootprintTests(unittest.TestCase):
 def test_actual_union(self):
  root=pathlib.Path(__file__).resolve().parents[2]
  s=(root/'src/Battlescape/BattlescapeState.cpp').read_text()
  start=s.index('    if (out.offers("open_unit_stats"))')
  end=s.index('\n    return true;',start)
  block=s[start:end]
  code='''#include <algorithm>
#include <vector>
#include <cassert>
namespace Observer {struct AgentUiRect {int x,y,width,height;};}
struct Button {int getX(){return 107;}int getY(){return 177;}int getWidth(){return 164;}int getHeight(){return 23;}};
struct UI {std::vector<Observer::AgentUiRect> regions; bool offers(const char*) {return true;}};
void apply(UI &out) {Button b;auto _btnStats=&b;
'''+block+'''}
bool inside(const Observer::AgentUiRect&r,int x,int y){return x>=r.x&&y>=r.y&&x<r.x+r.width&&y<r.y+r.height;}
int main(){
for(int dy: {0,20}) {
// Actual HUD name y+32 straddles stats y+33; numbers are inside.
UI out;out.regions={{135,176+dy,84,8},{136,186+dy,15,5},{154,186+dy,15,5},{0,0,20,20}};
// Button seam uses y177; dy20 additionally exercises partial/outside geometry.
auto before=out.regions;apply(out);
for(int y=0;y<240;++y)for(int x=0;x<320;++x){int count=0;for(auto&r:out.regions)count+=inside(r,x,y);bool expected=inside({107,177,164,23},x,y);for(auto&r:before)expected|=inside(r,x,y);assert(count==(expected?1:0));}
}
}
'''
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d);(p/'test.cpp').write_text(code)
   subprocess.run(['g++','-std=c++11',str(p/'test.cpp'),'-o',str(p/'test')],check=True,capture_output=True)
   subprocess.run([str(p/'test')],check=True,capture_output=True)
