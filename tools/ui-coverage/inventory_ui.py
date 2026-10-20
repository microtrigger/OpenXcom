#!/usr/bin/env python3
"""Read source UI bindings; emit reproducible candidates, never claim state exhaustiveness."""
import argparse, hashlib, json, re
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CALL = re.compile(r'(?P<receiver>(?:[A-Za-z_][A-Za-z_0-9]*(?:\[[^\]\n]+\])?(?:\.at\([^()\n]*\))?|\(\*[A-Za-z_][A-Za-z_0-9]*\)))\s*(?:->|\.)\s*(?P<event>on[A-Z][A-Za-z_0-9]*)\s*\(')
DIRECT = re.compile(r'\b([A-Za-z_][A-Za-z_0-9]*)::(handle|keyboardPress|keyboardRelease|mouseClick|mousePress|mouseRelease|mouseOver|mouseIn|mouseOut)\s*\(')
VARIANTS = ['success', 'ordinary_refusal', 'cancel_or_back', 'boundary', 'stale', 'duplicate', 'unknown_outcome', 'visible_enemy_event', 'audible_enemy_event']

def masked(source, mask_strings=False):
    # Keep source length/line numbers. Remove comments only, preserve quoted text.
    pattern = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
    return pattern.sub(lambda m: ''.join('\n' if c=='\n' else ' ' for c in m.group()) if mask_strings or m.group().startswith(('/',)) else m.group(), source)

def balanced_call(text, start):
    depth = 1; quote = None; escape = False
    for i in range(start, len(text)):
        char = text[i]
        if quote:
            if escape: escape = False
            elif char == '\\': escape = True
            elif char == quote: quote = None
            continue
        if char in ('"', "'"): quote = char
        elif char == '(': depth += 1
        elif char == ')':
            depth -= 1
            if depth == 0: return text[start:i]
    raise ValueError('Unclosed binding argument list')

def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--output', type=Path, required=True)
    args=parser.parse_args(); args.output.mkdir(parents=True, exist_ok=True)
    bindings=[]; directs=[]; unresolved=[]; sources={}; seen={}
    for path in sorted((ROOT/'src').rglob('*.cpp')):
        rel=str(path.relative_to(ROOT)); raw=path.read_bytes(); src=raw.decode(); clean=masked(src); code=masked(src,mask_strings=True)
        sources[rel]=hashlib.sha256(raw).hexdigest()
        for match in CALL.finditer(code):
            event=match['event']
            if event in ('onTimer','onOwner'): continue
            params=' '.join(balanced_call(clean,match.end()).split())
            callback=re.findall(r'&\s*([A-Za-z_][A-Za-z_0-9]*::[A-Za-z_][A-Za-z_0-9]*)',params)
            signature='|'.join((rel,match['receiver'],event,params)); occurrence=seen.get(signature,0)+1;seen[signature]=occurrence
            row={'binding_id':'ui-'+hashlib.sha256((signature+'|'+str(occurrence)).encode()).hexdigest()[:20],
                 'path':rel,'line':src.count('\n',0,match.start())+1,'receiver':match['receiver'],
                 'event':event,'arguments':params,'callbacks':callback,
                 'binding_kind':'registered' if callback else 'cleared_or_dynamic',
                 'subsystem':path.parent.name,'manual_state_variants_review':'pending'}
            bindings.append(row)
        matched_events={m.start('event') for m in CALL.finditer(code)}
        for m in re.finditer(r'(?:->|\.)\s*(on[A-Z][A-Za-z_0-9]*)\s*\(',code):
            if m[1] not in ('onTimer','onOwner') and m.start(1) not in matched_events:
                unresolved.append({'path':rel,'line':src.count('\n',0,m.start())+1,'event':m[1],'review':'unparsed_receiver'})
        for match in DIRECT.finditer(code):
            directs.append({'handler_id':match[1]+'::'+match[2], 'path':rel,'line':src.count('\n',0,match.start())+1,
                            'review':'pending_direct_input_branches'})
    groups={}
    for b in bindings:
        for callback in b['callbacks']:
            row=groups.setdefault(callback,{'ui_handler_id':callback,'subsystem':b['subsystem'],'binding_ids':[],
                'semantic_action_ids':[], 'games':['unclassified'], 'status':'not_mapped',
                'variants':{v:{'status':'not_run','evidence':[]} for v in VARIANTS}})
            row['binding_ids'].append(b['binding_id'])
    limitations=['Static registrations are candidates, not proof of every runtime state or every possible parameter combination.',
      'Conditional widgets, inherited controls, list rows, hotkeys, event polling and dynamically reassigned callbacks need manual review.',
      'Direct input handlers are included separately; no mapping to MCP is implied.',
      'NOT_APPLICABLE requires an explicit reason; absent evidence remains NOT_RUN/BLOCKED, never PASS.',
      'Visible/audio projections and enemy events require dedicated fairness evidence, beyond UI callback coverage.']
    inventory={'schema':'openxcom-ui-universe/1','source':'current isolated strategy-v3 worktree; no retail assets read',
      'observed_utc':datetime.now(timezone.utc).isoformat(),'source_sha256':sources,'bindings':bindings,'direct_input_handlers':directs,'unresolved_bindings':unresolved,'limitations':limitations,
      'counts':{'source_cpp_files':len(sources),'bindings':len(bindings),'distinct_callback_handlers':len(groups),'direct_input_implementations':len(directs),'unresolved_receiver_sites':len(unresolved)}}
    matrix={'schema':'openxcom-ui-coverage/1','universe':'ui-universe.json','rows':sorted(groups.values(),key=lambda x:x['ui_handler_id']),
      'status_values':['not_mapped','blocked_missing_mcp','not_run','pass','fail','outcome_unknown','not_applicable'], 'limitations':limitations}
    for name,doc in [('ui-universe.json',inventory),('coverage-matrix.json',matrix)]:
        (args.output/name).write_text(json.dumps(doc,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(inventory['counts'],sort_keys=True))

if __name__=='__main__': main()
