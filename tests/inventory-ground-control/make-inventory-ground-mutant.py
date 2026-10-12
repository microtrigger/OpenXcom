#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
"""Apply one bounded actual-source GROUND fault to a disposable source copy.

Never edit the ordinary checkout, test oracle, fixtures, or adapters. An applied
fault is setup only. A failed build, launch error, watchdog, or timeout is not a
behavioral fault detection; the native gate must fail a relevant assertion.
"""
from hashlib import sha256
from pathlib import Path
import sys

MARKER = 'Inventory GROUND sensitivity fault:'
CONTROLLER = 'src/Observer/StanceController.cpp'
INVENTORY = 'src/Battlescape/InventoryState.cpp'
MUTATIONS = {'missing-overlay-blit': ('src/Observer/InventoryCloseOverlay.cpp',
        'void InventoryCloseOverlay::blit(Surface *surface)\n{\n    if (!active()) return;\n',
        'void InventoryCloseOverlay::blit(Surface *surface)\n{\n    return; // active proposal is never drawn to the real display\n'),
 'cancel-init': ('src/Battlescape/InventoryState.cpp',
                 '    _inventoryCloseOverlay->dismiss();\n',
                 '    _inventoryCloseOverlay->dismiss();\n    init();\n'),
 'cancel-arrange': ('src/Battlescape/InventoryState.cpp',
                    '    _inventoryCloseOverlay->dismiss();\n',
                    '    _inventoryCloseOverlay->dismiss();\n    _inv->arrangeGround();\n'),
 'dismiss-clickthrough': ('src/Battlescape/InventoryState.cpp',
                          '    if (_inventoryCloseConsumeEvent || inventoryCloseOverlayOwned())\n',
                          '    if (inventoryCloseOverlayOwned())\n'),
 'unseen-overlay': ('src/Observer/StanceController.cpp',
                    '            _confirmEventAllowed = _dialogPresented && !_yDown && event.key.keysym.mod == '
                    'KMOD_NONE;\n',
                    '            _confirmEventAllowed = !_yDown && event.key.keysym.mod == KMOD_NONE;\n'),
 'stale-actor': ('src/Observer/StanceController.cpp',
        '        selectionRevision == o.selectionRevision && actorRevision == o.actorRevision && uiRevision == o.uiRevision\n',
        '        actorRevision == o.actorRevision && uiRevision == o.uiRevision\n'),
 'stale-reinit': ('src/Observer/StanceController.cpp',
        '        selectionRevision == o.selectionRevision && actorRevision == o.actorRevision && uiRevision == o.uiRevision\n',
        '        selectionRevision == o.selectionRevision && actorRevision == o.actorRevision\n'),
 'id-conflict-bypass': ('src/Observer/StanceController.cpp',
                        '        if (prior->second.request == request) replyStatus(id);\n',
                        '        if (true) replyStatus(id);\n'),
 'capless-I-authority': ('src/Observer/StanceController.cpp',
                         '        const std::string packet = '
                         '"{\\"schema\\":\\"experimental-inventory-view/1\\",\\"challenge\\":\\"" + challenge +\n',
                         '        const std::string packet = '
                         '"{\\"schema\\":\\"experimental-inventory-view/1\\",\\"capability\\":\\"00000000000000000000000000000000\\",\\"challenge\\":\\"" '
                         '+ challenge +\n'),
 'held-item-grant': ('src/Battlescape/InventoryState.cpp',
        '        (!allowHeld && _inv->getSelectedItem())) return false;\n',
        '        false) return false;\n'),
 'missing-ground': ('src/Observer/StanceController.cpp',
                    '        state->btnGroundClick(0);\n',
                    '        // Ordinary Ground handler omitted.\n'),
 'duplicate-ground': ('src/Observer/StanceController.cpp',
                      '        state->btnGroundClick(0);\n',
                      '        state->btnGroundClick(0);\n        state->btnGroundClick(0);\n'),
 'wrong-handler': ('src/Observer/StanceController.cpp',
                   '        state->btnGroundClick(0);\n',
                   '        state->btnOkClick(0);\n'),
 'ground-barrier-bypass': ('src/Observer/StanceController.cpp',
        '        _awaitingInventoryView = true; _inventoryDestination = state;\n        state->btnGroundClick(0);\n',
        '        _awaitingInventoryView = false; _inventoryDestination = state;\n        state->btnGroundClick(0);\n'),
 'lost-barrier-bypass': ('src/Observer/StanceController.cpp',
                         '    if (state == _inventoryDestination) _inventoryDestination = 0;\n',
                         '    if (state == _inventoryDestination) { _inventoryDestination = 0; _awaitingInventoryView '
                         '= false; }\n'),
 'failed-I-barrier-bypass': ('src/Observer/StanceController.cpp',
                             '        invalidate(); if (!_challenge.empty()) { _challenge.clear(); unavailable(); } '
                             'return;\n',
                             '        _awaitingInventoryView = false;\n'
                             '        invalidate(); if (!_challenge.empty()) { _challenge.clear(); unavailable(); } '
                             'return;\n'),
 'ground-kind-confusion': ('src/Observer/StanceController.cpp',
        "            (_contextKind == 'J' && inventoryClose) || (_contextKind == 'K' && inventoryGround) || (_contextKind == 'H' && inventoryClick)) ||\n",
        "            (_contextKind == 'J' && (inventoryClose || inventoryGround)) || (_contextKind == 'K' && (inventoryGround || inventoryClose)) || (_contextKind == 'H' && inventoryClick)) ||\n"),
 'wire-K-pixel': ('src/Observer/StanceController.cpp',
                  '        if (_contextKind == \'K\') schema = "experimental-inventory-ground-context/1";\n',
                  '        if (_contextKind == \'K\') { schema = "experimental-inventory-ground-context/1"; '
                  "indicesHex[1] = indicesHex[1] == '0' ? '1' : '0'; }\n"),
 'ground-visibility-bypass': ('src/Battlescape/InventoryState.cpp',
                              '    return _btnGround->turnContextVisible() && _btnGround->stanceContextVisible() &&\n',
                              '    return true &&\n'),
 'ground-zero-TU-refusal': ('src/Battlescape/InventoryState.cpp',
                            '    return _btnGround->turnContextVisible() && _btnGround->stanceContextVisible() &&\n',
                            '    return _battleGame->getSelectedUnit()->getTimeUnits() > 0 && '
                            '_btnGround->turnContextVisible() && _btnGround->stanceContextVisible() &&\n'),
 'ground-TU-charge': ('src/Battlescape/InventoryState.cpp',
                      '\t_inv->arrangeGround();\n',
                      '\t_battleGame->getSelectedUnit()->spendTimeUnits(1);\n\t_inv->arrangeGround();\n'),
 'ground-no-page-change': ('src/Battlescape/InventoryState.cpp',
                           '\t_inv->arrangeGround();\n',
                           '\t_inv->arrangeGround(false);\n'),
 'exact-full-page-boundary': ('src/Battlescape/Inventory.cpp',
                              '\t\tif (xMax >= _groundOffset + slotsX)\n',
                              '\t\tif (xMax > _groundOffset + slotsX)\n'),
 'ground-stack-cache': ('src/Battlescape/Inventory.cpp',
                        '\t\t\t\t\t\t_stackLevel[x][y] += 1;\n\t\t\t\t\t}\n\t\t\t\t\txMax = std::max(xMax, x + (*i)->getRules()->getInventoryWidth());\n',
                        '\t\t\t\t\t\t_stackLevel[x][y] = 1;\n\t\t\t\t\t}\n\t\t\t\t\txMax = std::max(xMax, x + (*i)->getRules()->getInventoryWidth());\n'),
 'initial-I-side-effect': ('src/Observer/StanceController.cpp',
                           '        if (!state || (_awaitingInventoryView && state != _inventoryDestination)) { '
                           'unavailable(); return; }\n',
                           '        if (!state || (_awaitingInventoryView && state != _inventoryDestination)) { '
                           'unavailable(); return; }\n'
                           '        if (!_awaitingInventoryView) state->btnGroundClick(0);\n')}
EXTRA_EDITS = {
    'unseen-overlay': (
        ('    const bool valid = accept && _dialogPresented && _confirmEventAllowed && !id.empty() && bindingCurrent(true);\n    _confirmEventAllowed = false;\n    // Retire only our InteractiveSurface.',
         '    const bool valid = accept && _confirmEventAllowed && !id.empty() && bindingCurrent(true);\n    _confirmEventAllowed = false;\n    // Retire only our InteractiveSurface.'),
        ('            if (waiting != 0) invalidate();\n        }\n        return;\n    }\n#endif\n    if (_dialog)',
         '            if (false) invalidate(); // queued pre-flip consent incorrectly accepted\n        }\n        return;\n    }\n#endif\n    if (_dialog)'),
    ),
}
FAIL = 'INVENTORY_GROUND_RUNTIME_MUTANT INCOMPLETE source-or-arguments'


def mutate(kind, source):
    _, anchor, replacement = MUTATIONS[kind]
    edits = ((anchor, replacement),) + EXTRA_EDITS.get(kind, ())
    if MARKER in source or any(source.count(before) != 1 for before, _ in edits):
        raise ValueError('source-anchor')
    result = source
    for index, (before, after) in enumerate(edits):
        prefix = '    // ' + MARKER + ' ' + kind + '\n' if index == 0 else ''
        result = result.replace(before, prefix + after, 1)
    return result


def _safe_source(root, relative, normal):
    path = root / relative
    resolved = path.resolve()
    if not resolved.is_relative_to(root) or resolved.is_relative_to(normal):
        raise ValueError('source-alias')
    original = normal / relative
    if path.exists() and original.exists() and path.samefile(original):
        raise ValueError('source-hardlink')
    return path


def main(args=None):
    args = sys.argv[1:] if args is None else args
    try:
        if len(args) != 2 or args[0] not in MUTATIONS:
            raise ValueError('arguments')
        kind, root = args[0], Path(args[1])
        normal = Path(__file__).resolve().parents[2]
        if not root.is_absolute():
            raise ValueError('checkout')
        root = root.resolve()
        if root == normal or root.is_relative_to(normal):
            raise ValueError('normal-checkout')
        gate = root / 'tests/inventory-ground-control/InventoryGroundControlGate.cpp'
        if not gate.is_file() or not gate.resolve().is_relative_to(root):
            raise ValueError('checkout')
        for relative in {entry[0] for entry in MUTATIONS.values()}:
            candidate = _safe_source(root, relative, normal)
            if candidate.is_file() and MARKER in candidate.read_text(encoding='utf-8'):
                raise ValueError('already-mutated')
        path = _safe_source(root, MUTATIONS[kind][0], normal)
        result = mutate(kind, path.read_text(encoding='utf-8'))
        path.write_text(result, encoding='utf-8')
    except (OSError, ValueError, KeyError, RuntimeError):
        print(FAIL, file=sys.stderr)
        return 1
    print('INVENTORY_GROUND_RUNTIME_MUTANT APPLIED ' + kind + ' source-sha256=' + sha256(result.encode()).hexdigest())
    return 0


if __name__ == '__main__':
    sys.exit(main())
