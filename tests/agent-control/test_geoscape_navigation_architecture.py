from pathlib import Path
import unittest
S=Path(__file__).resolve().parents[2]
B=S.parents[1]/'agent-control-viewport-v15-20261019/source'
def method(text,signature):
 a=text.index(signature);i=text.index('{',a)+1;depth=1
 while depth:
  if text[i]=='{':depth+=1
  elif text[i]=='}':depth-=1
  i+=1
 return text[a:i]
class NavigationArchitecture(unittest.TestCase):
 def test_provider_no_model_traversal(self):
  for cls in ['BasescapeState','BaseInfoState','StoresState','TransfersState']:
   text=(S/f'src/Basescape/{cls}.cpp').read_text();body=method(text,'bool '+cls+'::agentUiState')
   for getter in ['getSavedGame(', 'getBases(', 'getFacilities(', 'getTransfers(', 'getStorageItems(', 'getAvailable', 'getTotal']:
    self.assertNotIn(getter,body)
 def test_ordinary_callbacks_unchanged(self):
  for cls,names in [('BasescapeState',['miniClick','btnBaseInfoClick','btnGeoscapeClick']),('BaseInfoState',['miniClick','btnOkClick','btnStoresClick','btnTransfersClick']),('StoresState',['btnOkClick']),('TransfersState',['btnOkClick'])]:
   a=(S/f'src/Basescape/{cls}.cpp').read_text();b=(B/f'src/Basescape/{cls}.cpp').read_text()
   for name in names:self.assertEqual(method(a,'void '+cls+'::'+name),method(b,'void '+cls+'::'+name))
 def test_minibase_provider_private_snapshot(self):
  text=(S/'src/Basescape/MiniBaseView.cpp').read_text();body=method(text,'std::vector<AgentMiniBaseSlot> MiniBaseView::agentSlots')
  self.assertNotIn('_bases',body);self.assertNotIn('_base',body);self.assertNotIn('_texture',body)
  self.assertIn('output.matches(',body);self.assertIn('getPixel(slot*16+x,y)!=pixel',body)
 def test_shared_hover(self):
  text=(S/'src/Basescape/MiniBaseView.cpp').read_text();self.assertIn('hoverAt((int)floor(action->getRelativeXMouse() / action->getXScale()))',text)
  for cls in ['BasescapeState','BaseInfoState']:
   self.assertIn('_mini->hoverAt(int(fresh.rows[index])*16+8); miniClick(nullptr);',(S/f'src/Basescape/{cls}.cpp').read_text())
 def test_unknown_existence_slots(self):
  for cls in ['BasescapeState','BaseInfoState']:
   body=method((S/f'src/Basescape/{cls}.cpp').read_text(),'bool '+cls+'::agentUiState')
   self.assertIn('out.rows.push_back(slot.slot)',body);self.assertNotIn('exists',body)
 def test_final_and_regional_gates(self):
  for cls in ['BasescapeState','BaseInfoState']:
   text=(S/f'src/Basescape/{cls}.cpp').read_text()
   for guard in ['_game->isState(', '_game->agentOverlayClear(part)', 'agentCaptureOutput(_mini,output)', 'agentRegionsUncovered(part.regions,_mini,painted)', 'out.visible.size()<=32768']:
    self.assertIn(guard,text)
 def test_no_fixture_delta(self):
  for p in (B/'tests/agent-report-fixture').glob('*'):
   if p.is_file():self.assertEqual(p.read_bytes(),(S/'tests/agent-report-fixture'/p.name).read_bytes())
 def test_wire17(self):
  self.assertIn('openxcom-agent-control/17',(S/'src/Observer/AgentProtocol.cpp').read_text())
if __name__=='__main__':unittest.main()
