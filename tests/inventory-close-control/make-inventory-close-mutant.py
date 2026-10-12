#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
"""Apply one bounded actual-source CLOSE fault to a disposable source copy.

Never edit the ordinary checkout, test oracle, fixtures, or adapters. An applied
fault is setup only. A failed build, launch error, watchdog, or timeout is not a
behavioral fault detection; the native gate must fail a relevant assertion.
"""
from hashlib import sha256
from pathlib import Path
import sys

MARKER = 'Inventory CLOSE sensitivity fault:'
CONTROLLER = 'src/Observer/StanceController.cpp'
INVENTORY = 'src/Battlescape/InventoryState.cpp'
MUTATIONS = {
    'missing-overlay-blit': ('src/Observer/InventoryCloseOverlay.cpp',
        'void InventoryCloseOverlay::blit(Surface *surface)\n{\n    if (!active()) return;\n',
        'void InventoryCloseOverlay::blit(Surface *surface)\n{\n    return; // active proposal is never drawn to the real display\n'),
    'duplicate-close': (CONTROLLER,
        '    state->btnOkClick(0);\n',
        '    state->btnOkClick(0);\n    state->btnOkClick(0);\n'),
    'direct-pop': (CONTROLLER,
        '    state->btnOkClick(0);\n',
        '    _game->popState();\n'),
    'cancel-init': (INVENTORY,
        '    _inventoryCloseOverlay->dismiss();\n',
        '    _inventoryCloseOverlay->dismiss();\n    init();\n'),
    'cancel-arrange': (INVENTORY,
        '    _inventoryCloseOverlay->dismiss();\n',
        '    _inventoryCloseOverlay->dismiss();\n    _inv->arrangeGround();\n'),
    'dismiss-clickthrough': (INVENTORY,
        '    if (_inventoryCloseConsumeEvent || inventoryCloseOverlayOwned())\n',
        '    if (inventoryCloseOverlayOwned())\n'),
    'unseen-overlay': (CONTROLLER,
        '            _confirmEventAllowed = _dialogPresented && !_yDown && event.key.keysym.mod == KMOD_NONE;\n',
        '            _confirmEventAllowed = !_yDown && event.key.keysym.mod == KMOD_NONE;\n'),
    'stale-actor': ('src/Observer/StanceController.cpp',
        '        selectionRevision == o.selectionRevision && actorRevision == o.actorRevision && uiRevision == o.uiRevision\n',
        '        actorRevision == o.actorRevision && uiRevision == o.uiRevision\n'),
    'stale-reinit': ('src/Observer/StanceController.cpp',
        '        selectionRevision == o.selectionRevision && actorRevision == o.actorRevision && uiRevision == o.uiRevision\n',
        '        selectionRevision == o.selectionRevision && actorRevision == o.actorRevision\n'),
    'skip-gravity': (INVENTORY,
        '\t\t_battleGame->getTileEngine()->applyGravity(inventoryTile);\n',
        '\t\t(void)inventoryTile; // ordinary close gravity omitted\n'),
    'skip-lighting': (INVENTORY,
        '\t\t_battleGame->getTileEngine()->calculateTerrainLighting(); // dropping/picking up flares\n',
        '\t\t// ordinary close terrain-lighting update omitted\n'),
    'skip-FOV': (INVENTORY,
        '\t\t_battleGame->getTileEngine()->recalculateFOV();\n',
        '\t\t// ordinary close FOV update omitted\n'),
    'premature-V': (CONTROLLER,
        '        else if (!_game->inventoryCloseRestored(_closeParent) || !_game->getScreen()->stanceAllowed()) return;\n',
        '        else if (!_game->getScreen()->stanceAllowed()) return;\n'),
    'close-barrier-bypass': ('src/Observer/StanceController.cpp',
        '    _awaitingView = _awaitingCloseView = _closeRestoring = true;\n    _closeParent = _closeContext.parent;',
        '    _awaitingView = _awaitingCloseView = _closeRestoring = false;\n    _closeParent = _closeContext.parent;'),
    'id-conflict-bypass': (CONTROLLER,
        '        if (prior->second.request == request) replyStatus(id);\n',
        '        if (true) replyStatus(id);\n'),
    'capless-I-authority': (CONTROLLER,
        '        const std::string packet = "{\\"schema\\":\\"experimental-inventory-view/1\\",\\"challenge\\":\\"" + challenge +\n',
        '        const std::string packet = "{\\"schema\\":\\"experimental-inventory-view/1\\",\\"capability\\":\\"00000000000000000000000000000000\\",\\"challenge\\":\\"" + challenge +\n'),
    'held-item-grant': ('src/Battlescape/InventoryState.cpp',
        '        (!allowHeld && _inv->getSelectedItem())) return false;\n',
        '        false) return false;\n'),
    'wire-J-pixel': (CONTROLLER,
        '        std::string schema = closeView ? "experimental-inventory-close-context/1" : "experimental-tactical-context/1";\n',
        '        if (_contextKind == \'J\') indicesHex[1] = indicesHex[1] == \'0\' ? \'1\' : \'0\';\n        std::string schema = closeView ? "experimental-inventory-close-context/1" : "experimental-tactical-context/1";\n'),
}
EXTRA_EDITS = {
    'unseen-overlay': (
        ('    const bool valid = accept && _dialogPresented && _confirmEventAllowed && !id.empty() && bindingCurrent(true);\n    _confirmEventAllowed = false;\n    // Retire only our InteractiveSurface.',
         '    const bool valid = accept && _confirmEventAllowed && !id.empty() && bindingCurrent(true);\n    _confirmEventAllowed = false;\n    // Retire only our InteractiveSurface.'),
        ('            if (waiting != 0) invalidate();\n        }\n        return;\n    }\n#endif\n    if (_dialog)',
         '            if (false) invalidate(); // queued pre-flip consent incorrectly accepted\n        }\n        return;\n    }\n#endif\n    if (_dialog)'),
    ),
}
FAIL = 'INVENTORY_CLOSE_RUNTIME_MUTANT INCOMPLETE source-or-arguments'


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
        gate = root / 'tests/inventory-close-control/InventoryCloseControlGate.cpp'
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
    print('INVENTORY_CLOSE_RUNTIME_MUTANT APPLIED ' + kind + ' source-sha256=' + sha256(result.encode()).hexdigest())
    return 0


if __name__ == '__main__':
    sys.exit(main())
