#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
"""Apply one real inventory OPEN / capless-I fault to a disposable source copy.

Setup is not a detected runtime fault: rebuild copies and run the native gate.
No gate, assertion, packet fixture, mock, adapter, or normal checkout is edited.
Use a separate source copy for each fault; ambiguous/reapplied anchors fail closed.
"""
from hashlib import sha256
from pathlib import Path
import sys

MARKER = 'Inventory OPEN sensitivity fault:'
CONTROLLER = 'src/Observer/StanceController.cpp'
MUTATIONS = {
    'duplicate-open': (CONTROLLER,
        '        _context.state->btnInventoryClick(0);\n',
        '        _context.state->btnInventoryClick(0);\n        _context.state->btnInventoryClick(0);\n'),
    'wrong-handler': (CONTROLLER,
        '        _context.state->btnInventoryClick(0);\n',
        '        _context.state->btnKneelClick(0);\n'),
    'spend-TU': ('src/Battlescape/BattlescapeState.cpp',
        '\t\t_battleGame->cancelAllActions();\n\t\t_game->pushState(new InventoryState(!_save->getDebugMode(), this));\n',
        '\t\t_battleGame->cancelAllActions();\n\t\t_save->getSelectedUnit()->spendTimeUnits(1);\n\t\t_game->pushState(new InventoryState(!_save->getDebugMode(), this));\n'),
    'wire-pixel': (CONTROLLER,
        '    if (inventoryView)\n    {\n        std::string indicesHex, paletteHex;\n        if (!canonicalView(rgb, indicesHex, paletteHex)) { invalidate(); unavailable(); return; }\n',
        "    if (inventoryView)\n    {\n        std::string indicesHex, paletteHex;\n        if (!canonicalView(rgb, indicesHex, paletteHex)) { invalidate(); unavailable(); return; }\n        indicesHex[1] = indicesHex[1] == '0' ? '1' : '0';\n"),
    'inventory-capability': (CONTROLLER,
        '        const std::string packet = "{\\"schema\\":\\"experimental-inventory-view/1\\",\\"challenge\\":\\"" + challenge +\n',
        '        const std::string packet = "{\\"schema\\":\\"experimental-inventory-view/1\\",\\"capability\\":\\"00000000000000000000000000000000\\",\\"challenge\\":\\"" + challenge +\n'),
    'B-barrier-bypass': (CONTROLLER,
        '        _awaitingInventoryView = true; _inventoryDestination = 0;\n',
        '        _awaitingInventoryView = false; _inventoryDestination = 0;\n'),
    'I-clears-post-map-barrier': (CONTROLLER,
        '        _capability.clear();\n        if (!_challenge.empty() || _dialog || _awaitingView) { unavailable(); return; }\n',
        '        _capability.clear(); _awaitingView = false;\n        if (!_challenge.empty() || _dialog || _awaitingView) { unavailable(); return; }\n'),
    'pending-I-input-cancel': (CONTROLLER,
        '    invalidate(true, false, sameInventoryRead);\n',
        '    invalidate(true, false, false);\n'),
    'forced-init': (CONTROLLER,
        '        // Admission may precede normal initialization in the next engine loop.\n',
        '        state->init();\n        // Admission may precede normal initialization in the next engine loop.\n'),
    'stale-camera': ('src/Battlescape/Camera.cpp',
        '        if (offset != oldOffset || width != oldWidth || height != oldHeight || visible != oldVisible || layers != oldLayers)\n            ++revision;\n',
        '        if (false) ++revision;\n'),
    'stale-pose': ('src/Savegame/BattleUnit.cpp',
        '        if (position != oldPosition || direction != oldDirection || turret != oldTurret) ++revision;\n',
        '        if (false) ++revision;\n'),
    'unseen-modal': (CONTROLLER,
        '            _confirmEventAllowed = _dialogPresented && !_yDown && event.key.keysym.mod == KMOD_NONE;\n',
        '            _confirmEventAllowed = !_yDown && event.key.keysym.mod == KMOD_NONE;\n'),
    'deadline-extension': (CONTROLLER,
        "        invalidate(); _contextKind = 'I';\n        _challengeTime = now(); _challengeDeadline = _challengeTime + 1500;\n",
        "        invalidate(); _contextKind = 'I';\n        _challengeTime = now(); _challengeDeadline = _challengeTime + 120000;\n"),
    'id-conflict-bypass': (CONTROLLER,
        '        if (prior->second.request == request) replyStatus(id);\n',
        '        if (true) replyStatus(id);\n'),
    'missing-first-flip-palette-wait': ('src/Engine/Screen.cpp',
        '        (_observerPaletteExact || (_firstColor == 0 && _numColors == 256)) &&\n',
        '        _observerPaletteExact &&\n'),
}
EXTRA_EDITS = {
    'unseen-modal': (
        ('    const bool valid = accept && _dialogPresented && _confirmEventAllowed && !id.empty() && bindingCurrent(true);\n',
         '    const bool valid = accept && _confirmEventAllowed && !id.empty() && bindingCurrent(true);\n'),
    ),
}
FAIL = 'INVENTORY_OPEN_RUNTIME_MUTANT INCOMPLETE source-or-arguments'


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
        gate = root / 'tests/inventory-open-control/InventoryOpenControlGate.cpp'
        if not gate.is_file() or not gate.resolve().is_relative_to(root):
            raise ValueError('checkout')
        # Separate copies prevent combining faults, including ones in other files.
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
    print('INVENTORY_OPEN_RUNTIME_MUTANT APPLIED ' + kind + ' source-sha256=' + sha256(result.encode()).hexdigest())
    return 0


if __name__ == '__main__':
    sys.exit(main())
