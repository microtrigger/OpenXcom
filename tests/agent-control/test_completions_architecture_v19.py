"""Narrow /19 source parity, local paint lifetime and isolated fixture contracts."""
from pathlib import Path
import re, unittest
S=Path(__file__).resolve().parents[2]
B=S.parent.parent/'agent-control-price-v18b-20261019/source'
def without_agent(text):
    out=[];depth=0
    for line in text.splitlines():
        stripped=line.strip()
        if stripped=='#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL':
            if depth: depth+=1
            else: depth=1
            continue
        if depth:
            if stripped.startswith(('#if ', '#ifdef ', '#ifndef ')):depth+=1
            elif stripped=='#endif':depth-=1
            continue
        out.append(line)
    if depth:raise RuntimeError('unterminated agent block')
    return '\n'.join(line for line in out if line.strip())
class Architecture(unittest.TestCase):
    def test_off_build_preserves_all_ordinary_widget_and_popup_source(self):
        paths=['Interface/Text','Interface/TextButton','Menu/ErrorMessageState','Geoscape/ItemsArrivingState','Geoscape/ProductionCompleteState','Geoscape/UfoDetectedState']
        for path in paths:
            for suffix in ('.cpp','.h'):
                p='src/'+path+suffix
                with self.subTest(path=p):self.assertEqual(without_agent((S/p).read_text()),without_agent((B/p).read_text()))
    def test_shared_bridge_and_simulation_unchanged(self):
        paths=['Engine/Game','Engine/State','Engine/Surface','Observer/AgentController','Observer/AgentUi','Savegame/Production','Savegame/Transfer','Savegame/SavedGame','Savegame/ProductionQuantityCommands','Savegame/PurchaseQuantityCommands']
        for path in paths:
            for suffix in ('.cpp','.h'):
                p='src/'+path+suffix
                if (B/p).exists():
                    with self.subTest(path=p):self.assertEqual((S/p).read_bytes(),(B/p).read_bytes())
    def test_private_revision_only_at_changed_typed_text(self):
        text=(S/'src/Interface/Text.cpp').read_text()
        self.assertIn('if (_text != text) ++_agentTextRevision;',text)
        self.assertEqual(text.count('++_agentTextRevision'),1)
        self.assertIn('agentTextRevision()', (S/'src/Interface/TextButton.cpp').read_text())
        self.assertNotIn('agentTextRevision', (S/'src/Observer/AgentProtocol.cpp').read_text())
    def test_capture_is_opt_in_bounded_typed_range(self):
        text=(S/'src/Interface/Text.cpp').read_text()
        for literal in ('_agentErrorHeading && !_scroll','headingEnd <= 1024','agentCaptureErrorHeading();','> 65536','== 8192','pixel.value'):self.assertIn(literal,text)
        self.assertEqual(sum('agentEnableErrorHeading' in p.read_text() for p in (S/'src').rglob('*.cpp')),1)
    def test_local_last_paint_and_current_clip_guards(self):
        for name,folder in [('ErrorMessageState','Menu'),('ItemsArrivingState','Geoscape'),('ProductionCompleteState','Geoscape'),('UfoDetectedState','Geoscape')]:
            text=(S/f'src/{folder}/{name}.cpp').read_text()
            with self.subTest(name=name):
                for value in ('_agentDestinationPaintFull = false','State::blit();','agentDestinationFull()','clip_rect','agentPaintHealthy()'):self.assertIn(value,text)
                self.assertIn('!_agentDestinationPaintFull',text)
    def test_separate_fixture_restores_exact_prior_with_five_narrow_changes(self):
        p='tests/agent-report-fixture/AgentReportFixture.cpp';text=(S/p).read_text()
        text=text.replace(', completionCampaign = false;', ';',1)
        text=text.replace(' || std::string(argv[i+1])=="completion-medikit-v1"','',1)
        text=text.replace('        completionCampaign=std::string(argv[i+1])=="completion-medikit-v1";\n','',1)
        text=text.replace('manufactureCampaign=completionCampaign || ','manufactureCampaign=',1)
        text=text.replace('base->setName(completionCampaign ? "MCP19" : "MCP Manufacture");','base->setName("MCP Manufacture");',1)
        self.assertEqual(text,(B/p).read_text())
    def test_new_protocol_closed_none_ids(self):
        text=(S/'src/Observer/AgentProtocol.cpp').read_text()
        self.assertIn('openxcom-agent-control/22',text)
        for action in ('dismiss_items_arriving','open_arrival_base','dismiss_production_complete','open_production_destination','dismiss_ufo_contact'):self.assertIn('"'+action+'"',text)
        self.assertNotIn('intercept_ufo',text)
if __name__=='__main__':unittest.main(verbosity=2)
