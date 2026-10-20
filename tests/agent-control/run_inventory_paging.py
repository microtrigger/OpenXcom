#!/usr/bin/env python3
"""Bounded native /12 paging and body-routing extraction tests.

Compile literal production methods against explicitly scoped data/UI shims. No
Game executable, full build, SDL, GUI, socket, live assets or acceptance gates.
Public xcom1/xcom2 inventory YAML supplies geometry, slots and TU costs.
"""
from pathlib import Path
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
import shlex
import subprocess
import time
import yaml
from test_inventory_routing_architecture import function

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
INVENTORY_METHODS = ('mouseOver', 'mouseClick', 'movePointer', 'activateButton',
                     'returnHeldItem', 'unload', 'setSelectedItem',
                     'setMouseOverItem', 'getSlotInPosition', 'moveItem',
                     'overlapItems', 'canBeStacked', 'fitItem', 'arrangeGround')
RULE_METHODS = ('checkSlotInPosition', 'fitItemInSlot', 'getCost')


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT.parent/'native-inventory-paging')
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    out = args.out.resolve()
    if ROOT.parent.resolve() not in out.parents:
        parser.error('Output must be below the isolated /12 root')
    out.mkdir(parents=True, exist_ok=True)
    inputs = {}
    cached = {}
    def read(path):
        key = str(path)
        if key not in cached:
            raw = path.read_bytes()
            inputs[key] = sha(raw)
            cached[key] = raw.decode()
        return cached[key]
    read(Path(__file__).resolve())
    read(HERE/'AgentInventoryPagingTest.cpp')
    read(HERE/'test_inventory_paging_architecture.py')
    read(HERE/'test_inventory_routing_architecture.py')
    read(ROOT/'src/Observer/AgentUi.h')
    read(ROOT/'src/Observer/AgentProtocol.h')
    read(ROOT/'src/Observer/AgentProjectileWitness.h')
    for file in ('Inventory.cpp', 'InventoryState.cpp'):
        read(ROOT.parent.parent/'agent-control-inventory-v11b-20261018/source/src/Battlescape'/file)
    read(ROOT/'src/Battlescape/InventoryState.cpp')
    extractions = []
    actual = []
    def extract(relative, qualified, substitutions=None, signature=None, namespace='OpenXcom'):
        path = ROOT/'src'/relative
        original = function(read(path), qualified, signature)
        transformed = original
        for old, new in (substitutions or {}).items():
            transformed = transformed.replace(old, new)
        actual.append('namespace '+namespace+' {\n'+transformed+'\n}')
        extractions.append({'input': str(path), 'method': qualified,
                            'signature_selector': signature,
                            'original_sha256': sha(original.encode()),
                            'substitutions': substitutions or {},
                            'compiled_sha256': sha(transformed.encode())})
    for name in INVENTORY_METHODS:
        extract('Battlescape/Inventory.cpp', 'Inventory::'+name)
    for name in RULE_METHODS:
        extract('Mod/RuleInventory.cpp', 'RuleInventory::'+name)
    extract('Savegame/BattleItem.cpp', 'BattleItem::occupiesSlot',
            {'_inventorySlot': 'slot', '_inventoryX': 'this->x',
             '_inventoryY': 'this->y', '_rules': 'rules'})
    extract('Savegame/BattleItem.cpp', 'BattleItem::needsAmmo', {'_ammoItem': 'ammo'})
    extract('Savegame/BattleItem.cpp', 'BattleItem::setAmmoItem',
            {'_ammoItem': 'ammo', '_rules': 'rules'})
    extract('Savegame/BattleUnit.cpp', 'BattleUnit::getItem',
            {'_inventory': 'items'}, 'RuleInventory *slot')
    for name in ('payload', 'authorityPayload', 'sameBinding'):
        extract('Observer/AgentUi.cpp', 'AgentUiState::'+name,
                namespace='OpenXcom { namespace Observer')
        actual[-1] += '\n}'
    extract('Observer/AgentProtocol.cpp', 'agentJsonString',
            namespace='OpenXcom { namespace Observer')
    actual[-1] += '\n}'
    generated = out/'InventoryPagingActual.inc'
    generated.write_text('// Generated from recorded source bytes.\n'+'\n\n'.join(actual)+'\n')
    layouts = ['// Generated directly from public inventory YAML; no retail assets.',
               'static void loadStockLayout(const std::string &name, OpenXcom::Mod &mod,',
               ' std::vector<std::unique_ptr<OpenXcom::RuleInventory>> &owned) {']
    layout_summary = {}
    for game in ('xcom1', 'xcom2'):
        data = yaml.safe_load(read(ROOT/'bin/standard'/game/'inventories.rul'))['invs']
        layout_summary[game] = data
        layouts.append('if (name == '+json.dumps(game)+') {')
        for slot in data:
            sid = json.dumps(slot['id'])
            layouts.append('owned.emplace_back(new OpenXcom::RuleInventory('+sid+
                           ',static_cast<OpenXcom::InventoryType>('+str(slot.get('type',0))+'),'+
                           str(slot.get('x',0))+','+str(slot.get('y',0))+'));')
            layouts.append('mod.slots['+sid+']=owned.back().get();')
            for x, y in slot.get('slots', []):
                layouts.append('owned.back()->_slots.push_back({'+str(x)+','+str(y)+'});')
            for dest, cost in slot.get('costs', {}).items():
                layouts.append('owned.back()->_costs['+json.dumps(dest)+']='+str(cost)+';')
        layouts.append('return; }')
    layouts.extend(['throw std::runtime_error("Unknown stock layout");', '}'])
    layout_file = out/'InventoryStockLayouts.inc'
    layout_file.write_text('\n'.join(layouts)+'\n')
    result = {'scope': __doc__, 'started_utc': datetime.now(timezone.utc).isoformat(),
              'game_executed': False, 'full_build_run': False, 'sdl_initialized': False,
              'gui_executed': False, 'retail_assets_read': False, 'runtime_acceptance': False,
              'supervised_gates_executed': False, 'inputs_sha256': inputs,
              'extractions': extractions, 'stock_layouts': layout_summary,
              'generated_sha256': {generated.name: sha(generated.read_bytes()),
                                   layout_file.name: sha(layout_file.read_bytes())},
              'shims': [
                  'In-memory RuleItem getters, dimensions, compatible ammunition and charges; no YAML RuleItem load.',
                  'In-memory BattleItem owner/tile membership, ordinary getters, fuse, unit position and isAmmo flag.',
                  'BattleUnit TU recorder implements simple decrement or insufficient-TU refusal; no actor stats or save.',
                  'Tile membership is a vector; Screen constants are stock 320x200.',
                  'Drawing counts calls; selection records position/sprite/clear. No rendering or frame admission.',
                  'Game/Mod/Language/Warning/Sound/Action/InteractiveSurface are dependency recorders, not linked game objects.',
                  'AgentUiState uses actual header and literal payload/binding/JSON functions. Fixed visible JSON is a test input, not a renderer projection.',
              ], 'steps': [], 'all_passed': False}
    target = out/'RESULT.json'
    def save():
        target.write_text(json.dumps(result, indent=2)+'\n')
    command = ['python3', str(HERE/'test_inventory_paging_architecture.py')]
    started = time.monotonic()
    architecture = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=30)
    result['steps'].append({'configuration': 'source-architecture', 'stage': 'execute',
                            'command': shlex.join(command), 'exit_code': architecture.returncode,
                            'stdout': architecture.stdout, 'stderr': architecture.stderr,
                            'elapsed_seconds': round(time.monotonic()-started,3)})
    save()
    print('source-architecture',architecture.returncode,architecture.stdout,architecture.stderr,flush=True)
    if architecture.returncode:
        return architecture.returncode
    configurations = [('agent-on-O0', ['-O0','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL']),
                      ('agent-on-O2', ['-O2','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL']),
                      ('agent-off-O2', ['-O2'])]
    if args.sanitized:
        configurations.append(('agent-on-sanitized', ['-O1','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL',
                               '-fsanitize=address,undefined','-fno-omit-frame-pointer']))
    for name, flags in configurations:
        binary = out/name
        command = ['c++','-std=c++11','-Wall','-Wextra','-Werror',*flags,
                   '-I',str(out),str(HERE/'AgentInventoryPagingTest.cpp'),'-o',str(binary)]
        for stage, cmd in [('compile',command),('execute',[str(binary)])]:
            started = time.monotonic()
            process = subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,timeout=90,
                                     env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',
                                              UBSAN_OPTIONS='halt_on_error=1'))
            result['steps'].append({'configuration': name, 'stage': stage,
                                    'command': shlex.join(cmd), 'exit_code': process.returncode,
                                    'stdout': process.stdout, 'stderr': process.stderr,
                                    'elapsed_seconds': round(time.monotonic()-started,3)})
            save()
            print(name,stage,process.returncode,process.stdout,process.stderr,flush=True)
            if process.returncode:
                return process.returncode
    result['inputs_unchanged_after_run'] = all(Path(p).is_file() and sha(Path(p).read_bytes())==h
                                             for p,h in inputs.items())
    result['all_passed'] = result['inputs_unchanged_after_run']
    save()
    return 0 if result['all_passed'] else 2


if __name__ == '__main__':
    raise SystemExit(main())
