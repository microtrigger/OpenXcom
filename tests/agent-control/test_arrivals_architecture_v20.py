"""Narrow /20 opt-in, ordinary rendering and bridge invariants."""
from pathlib import Path
import re, unittest
S=Path(__file__).resolve().parents[2]
B=S.parent.parent/'agent-control-completions-v19-20261019/source'
def without_agent(text):
    out=[];depth=0
    for line in text.splitlines():
        stripped=line.strip()
        if stripped=='#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL':depth+=1;continue
        if depth:
            if stripped.startswith(('#if ', '#ifdef ', '#ifndef ')):depth+=1
            elif stripped=='#endif':depth-=1
            continue
        out.append(line)
    if depth:raise RuntimeError('unterminated agent block')
    return '\n'.join(line for line in out if line.strip())
def body(text,signature):
    start=text.index(signature);i=text.index('{',start)+1;depth=1
    while depth:
        if text.startswith('//',i):i=text.index('\n',i)+1;continue
        if text.startswith('/*',i):i=text.index('*/',i)+2;continue
        if text[i] in ('"',"'"):
            q=text[i];i+=1
            while text[i]!=q:i+=2 if text[i]=='\\' else 1
        elif text[i]=='{':depth+=1
        elif text[i]=='}':depth-=1
        i+=1
    return text[start:i]
class Architecture(unittest.TestCase):
    def test_off_build_source_and_ordinary_callbacks(self):
        for p in ('src/Interface/TextList.cpp','src/Interface/TextList.h','src/Geoscape/ItemsArrivingState.cpp'):
            with self.subTest(path=p):self.assertEqual(without_agent((S/p).read_text()),without_agent((B/p).read_text()))
    def test_controller_game_surface_and_models_unchanged(self):
        for name in ('Engine/Game','Engine/State','Engine/Surface','Observer/AgentController','Interface/Text','Interface/TextButton','Savegame/Transfer','Savegame/Production','Savegame/SavedGame','Savegame/ProductionQuantityCommands','Savegame/PurchaseQuantityCommands'):
            for suffix in ('.cpp','.h'):
                p='src/'+name+suffix
                if (B/p).exists():
                    with self.subTest(path=p):self.assertEqual((S/p).read_bytes(),(B/p).read_bytes())
    def test_unrelated_table_providers_and_fixture_unchanged(self):
        for p in ('src/Basescape/StoresState.cpp','src/Basescape/TransfersState.cpp','src/Basescape/PurchaseState.cpp','src/Basescape/ResearchState.cpp','src/Basescape/NewResearchListState.cpp','tests/agent-report-fixture/AgentReportFixture.cpp'):
            self.assertEqual((S/p).read_bytes(),(B/p).read_bytes())
    def test_old_projection_bodies_exact(self):
        for p,signatures in [('src/Observer/AgentUi.cpp',('bool agentProjectList(', 'bool agentRegionsDisjoint(', 'bool agentRegionsUncovered(')),('src/Interface/TextList.cpp',('bool TextList::agentRows(', 'bool TextList::agentControlRows(', 'bool TextList::agentPurchaseRows('))]:
            for signature in signatures:
                with self.subTest(signature=signature):self.assertEqual(body((S/p).read_text(),signature),body((B/p).read_text(),signature))
    def test_explicit_only_arrivals_opt_in(self):
        files=[p.relative_to(S).as_posix() for p in (S/'src').rglob('*.cpp') if '->agentEnableArrivalCells()' in p.read_text()]
        self.assertEqual(files,['src/Geoscape/ItemsArrivingState.cpp'])
        text=(S/'src/Interface/TextList.h').read_text();self.assertIn('_agentArrivalCells = false',text)
        capture=body((S/'src/Interface/TextList.cpp').read_text(),'void TextList::agentCaptureRow(')
        self.assertIn('const bool independentCells = purchaseCells || arrivalCells;',capture)
        self.assertIn('_columns.size() == 3',capture)
    def test_initial_draw_does_not_initialize_trusted_coverage(self):
        ctor=body((S/'src/Interface/TextList.cpp').read_text(),'TextList::TextList(')
        self.assertIn('_redraw = true;',ctor);self.assertNotIn('_agentCoverageHealthy = true',ctor)
        self.assertIn('_agentCoverageHealthy = false',(S/'src/Interface/TextList.h').read_text())
        self.assertIn('_agentPaintRevision != _agentListRevision',body((S/'src/Interface/TextList.cpp').read_text(),'bool TextList::agentRows('))
    def test_bounds_and_full_mask_before_union(self):
        text=body((S/'src/Observer/AgentUi.cpp').read_text(),'bool agentProjectArrivalList(')
        for v in ('count == 32','2 * 1024 * 1024','> 8192','cell.text.size() <= 1024','!cell.text.empty() && !regions.empty()','row.bounds.y <= lastY'):self.assertIn(v,text)
        self.assertLess(text.index('!agentRegionsUncovered(regions, owner, paintOrder)'),text.index('std::vector<AgentUiRect> additions'))
        self.assertLess(text.index('!regionAllowed(part)'),text.index('std::vector<AgentUiRect> additions'))
        self.assertIn('if (!healthy) return false;',text)
    def test_private_draw_projection_no_model_reads(self):
        parts=[body((S/'src/Interface/TextList.cpp').read_text(),'bool TextList::agentArrivalRows('),body((S/'src/Observer/AgentUi.cpp').read_text(),'bool agentProjectArrivalList('),body((S/'src/Geoscape/ItemsArrivingState.cpp').read_text(),'bool ItemsArrivingState::agentUiState(')]
        for text in parts:
            for forbidden in ('getTransfers(', 'getSavedGame(', 'getBases(', '_base->', 'getName(', 'getItems('):self.assertNotIn(forbidden,text)
    def test_protocol_only_version_changes(self):
        p='src/Observer/AgentProtocol.cpp';self.assertEqual((S/p).read_text(),(B/p).read_text().replace('openxcom-agent-control/22','openxcom-agent-control/23'))
if __name__=='__main__':unittest.main(verbosity=2)
