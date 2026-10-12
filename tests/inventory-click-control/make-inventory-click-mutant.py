#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
"""Apply one bounded actual-source CLICK fault to a disposable source copy.

Never edit the ordinary checkout, test oracle, fixtures, or adapters. An applied
fault is setup only. A failed build, launch error, watchdog, or timeout is not a
behavioral fault detection; the native gate must fail a relevant assertion.
"""
from hashlib import sha256
from pathlib import Path
import sys

MARKER = 'Inventory CLICK sensitivity fault:'
CONTROLLER = 'src/Observer/StanceController.cpp'
INVENTORY = 'src/Battlescape/InventoryState.cpp'
MUTATIONS = {'cancel-init': ('src/Battlescape/InventoryState.cpp',
                 '    _inventoryCloseOverlay->dismiss();\n',
                 '    _inventoryCloseOverlay->dismiss();\n    init();\n'),
 'cancel-arrange': ('src/Battlescape/InventoryState.cpp',
                    '    _inventoryCloseOverlay->dismiss();\n',
                    '    _inventoryCloseOverlay->dismiss();\n    _inv->arrangeGround();\n'),
 'dismiss-clickthrough': ('src/Battlescape/InventoryState.cpp',
                          '    if (_inventoryCloseConsumeEvent || inventoryCloseOverlayOwned())\n',
                          '    if (inventoryCloseOverlayOwned())\n'),
 'id-conflict-bypass': ('src/Observer/StanceController.cpp',
                        '        if (prior->second.request == request) replyStatus(id);\n',
                        '        if (true) replyStatus(id);\n'),
 'capless-I-authority': ('src/Observer/StanceController.cpp',
                         '        const std::string packet = '
                         '"{\\"schema\\":\\"experimental-inventory-view/1\\",\\"challenge\\":\\"" '
                         '+ challenge +\n',
                         '        const std::string packet = '
                         '"{\\"schema\\":\\"experimental-inventory-view/1\\",\\"capability\\":\\"00000000000000000000000000000000\\",\\"challenge\\":\\"" '
                         '+ challenge +\n'),
 'lost-barrier-bypass': ('src/Observer/StanceController.cpp',
                         '    if (state == _inventoryDestination) _inventoryDestination = 0;\n',
                         '    if (state == _inventoryDestination) { _inventoryDestination = 0; '
                         '_awaitingInventoryView = false; }\n'),
 'initial-I-side-effect': ('src/Observer/StanceController.cpp',
                           '        if (!state || (_awaitingInventoryView && state != '
                           '_inventoryDestination)) { unavailable(); return; }\n',
                           '        if (!state || (_awaitingInventoryView && state != '
                           '_inventoryDestination)) { unavailable(); return; }\n'
                           '        if (!_awaitingInventoryView) state->btnGroundClick(0);\n'),
 'missing-click': ('src/Battlescape/InventoryState.cpp',
                   '    _inv->mouseClick(&action, this);\n',
                   '    // Ordinary click omitted.\n'),
 'duplicate-click': ('src/Battlescape/InventoryState.cpp',
                     '    _inv->mouseClick(&action, this);\n',
                     '    _inv->mouseClick(&action, this);\n'
                     '    _inv->mouseClick(&action, this);\n'),
 'missing-hover': ('src/Battlescape/InventoryState.cpp',
                   '    _inv->mouseOver(&action, this);\n',
                   '    // Ordinary hover omitted.\n'),
 'wrong-hover': ('src/Battlescape/InventoryState.cpp',
                 '    _inv->mouseOver(&action, this);\n',
                 '    action.setMouseAction(160, 80, _inv->getX(), _inv->getY());\n'
                 '    _inv->mouseOver(&action, this);\n'
                 '    action.setMouseAction(x, y, _inv->getX(), _inv->getY());\n'),
 'right-click-dispatch': ('src/Battlescape/InventoryState.cpp',
                          '    event.button.button = SDL_BUTTON_LEFT; event.button.state = '
                          'SDL_RELEASED;\n',
                          '    event.button.button = SDL_BUTTON_RIGHT; event.button.state = '
                          'SDL_RELEASED;\n'),
 'missing-callback-tail': ('src/Battlescape/Inventory.cpp',
                           '\tInteractiveSurface::mouseClick(action, state);\n',
                           '\t// Callback tail omitted.\n'),
 'click-barrier-bypass': ('src/Observer/StanceController.cpp',
                          '        _awaitingInventoryView = true; _inventoryDestination = state;\n'
                          '        state->inventoryControlClick(_targetX, _targetY);\n',
                          '        _awaitingInventoryView = false; _inventoryDestination = state;\n'
                          '        state->inventoryControlClick(_targetX, _targetY);\n'),
 'click-barrier-too-late': ('src/Observer/StanceController.cpp',
                            '        _awaitingInventoryView = true; _inventoryDestination = '
                            'state;\n'
                            '        state->inventoryControlClick(_targetX, _targetY);\n',
                            '        state->inventoryControlClick(_targetX, _targetY);\n'
                            '        _awaitingInventoryView = true; _inventoryDestination = '
                            'state;\n'),
 'failed-send-barrier-bypass': ('src/Observer/StanceController.cpp',
                                '        if (sendPacket(packet) && _awaitingInventoryView && '
                                '_frameInventory == _inventoryDestination)\n',
                                '        if ((sendPacket(packet), true) && _awaitingInventoryView '
                                '&& _frameInventory == _inventoryDestination)\n'),
 'held-item-refusal': ('src/Battlescape/InventoryState.cpp',
                       '    if (!inventoryControlContext(context, overlay, true) || '
                       '!_inv->inventoryClickPresentationAllowed() ||\n',
                       '    if (!inventoryControlContext(context, overlay, false) || '
                       '!_inv->inventoryClickPresentationAllowed() ||\n'),
 'warning-occlusion-bypass': ('src/Battlescape/Inventory.cpp',
                              '_selection->getWidth() != 32 || _selection->getHeight() != 48 || '
                              '_warning->inventoryClickOccluding()) return false;',
                              '_selection->getWidth() != 32 || _selection->getHeight() != 48) '
                              'return false;'),
 'stale-held': ('src/Observer/StanceController.cpp',
                '        && heldRevision == o.heldRevision && selectionX == o.selectionX && '
                'selectionY == o.selectionY\n',
                '        && selectionX == o.selectionX && selectionY == o.selectionY\n'),
 'stale-actor': ('src/Observer/StanceController.cpp',
                 '        selectionRevision == o.selectionRevision && actorRevision == '
                 'o.actorRevision && uiRevision == o.uiRevision\n',
                 '        actorRevision == o.actorRevision && uiRevision == o.uiRevision\n'),
 'stale-reinit': ('src/Observer/StanceController.cpp',
                  '        selectionRevision == o.selectionRevision && actorRevision == '
                  'o.actorRevision && uiRevision == o.uiRevision\n',
                  '        selectionRevision == o.selectionRevision && actorRevision == '
                  'o.actorRevision\n'),
 'click-runtime-bypass': ('src/Observer/StanceController.cpp',
                          '    _inventoryClickEnabled = _inventoryCloseEnabled && inventoryClick '
                          '&& std::string(inventoryClick) == "1";\n',
                          '    _inventoryClickEnabled = _inventoryCloseEnabled;\n'),
 'close-prerequisite-bypass': ('src/Observer/StanceController.cpp',
                               '    _inventoryClickEnabled = _inventoryCloseEnabled && '
                               'inventoryClick && std::string(inventoryClick) == "1";\n',
                               '    _inventoryClickEnabled = inventoryClick && '
                               'std::string(inventoryClick) == "1";\n'),
 'click-domain-bypass': ('src/Observer/StanceController.cpp',
                         '        (inventoryClick && !((x >= 0 && x < 32 && y >= 63 && y < 111) '
                         '||\n'
                         '            (x >= 128 && x < 160 && y >= 63 && y < 111) || (x >= 0 && x '
                         '< 320 && y >= 152 && y < 200))) ||\n',
                         '        false ||\n'),
 'click-kind-confusion': ('src/Observer/StanceController.cpp',
                          "            (_contextKind == 'J' && inventoryClose) || (_contextKind == "
                          "'K' && inventoryGround) || (_contextKind == 'H' && inventoryClick)) "
                          '||\n',
                          "            (_contextKind == 'J' && (inventoryClose || inventoryClick)) "
                          "|| (_contextKind == 'K' && inventoryGround) || (_contextKind == 'H' && "
                          '(inventoryClick || inventoryClose))) ||\n'),
 'wire-H-pixel': ('src/Observer/StanceController.cpp',
                  "        if (_contextKind == 'H') schema = "
                  '"experimental-inventory-click-context/1";\n',
                  "        if (_contextKind == 'H') { schema = "
                  '"experimental-inventory-click-context/1"; indicesHex[1] = indicesHex[1] == '
                  "'0' ? '1' : '0'; }\n"),
 'wrong-banner-band': ('src/Battlescape/Inventory.cpp',
                       '    return top < 144 && bottom > 112 ? 24 : 112;\n',
                       '    return 112;\n'),
 'wrong-confirm-hitbox': ('src/Observer/InventoryCloseOverlay.cpp',
                          '    if (x >= _yes->getX() && x < _yes->getX() + _yes->getWidth() &&\n'
                          '        y >= _yes->getY() && y < _yes->getY() + _yes->getHeight()) '
                          'return 1;\n',
                          '    if (x >= 242 && x < 318 && y >= 105 && y < 119) return 1;\n'),
 'opaque-pointer-center': ('src/Observer/InventoryCloseOverlay.cpp',
                           '            if (squared >= 25 && squared <= 36) _pointer->setPixel(dx '
                           '+ 8, dy + 8, squared <= 30 ? light : dark);\n',
                           '            if (squared <= 36) _pointer->setPixel(dx + 8, dy + 8, '
                           'squared <= 30 ? light : dark);\n'),
 'wrong-pointer-marker': ('src/Observer/InventoryCloseOverlay.cpp',
                          '_pointer->setX(controller->targetX() - 8); '
                          '_pointer->setY(controller->targetY() - 8);',
                          '_pointer->setX(controller->targetX() - 7); '
                          '_pointer->setY(controller->targetY() - 8);'),
 'missing-overlay-blit': ('src/Observer/InventoryCloseOverlay.cpp',
                          '    _frame->blit(surface); _text->blit(surface); _yes->blit(surface); '
                          '_no->blit(surface);\n',
                          '    // Confirmation text and buttons omitted.\n'),
 'unseen-overlay': ('src/Observer/StanceController.cpp',
                    '            _confirmEventAllowed = _dialogPresented && !_yDown && '
                    'event.key.keysym.mod == KMOD_NONE;\n',
                    '            _confirmEventAllowed = !_yDown && event.key.keysym.mod == '
                    'KMOD_NONE;\n'),
 'failed-I-barrier-bypass': ('src/Observer/StanceController.cpp',
                             '        invalidate(); if (!_challenge.empty()) { _challenge.clear(); '
                             'unavailable(); } return;\n',
                             '        _awaitingInventoryView = false;\n'
                             '        invalidate(); if (!_challenge.empty()) { _challenge.clear(); '
                             'unavailable(); } return;\n'),
 'invalid-child-clickthrough': ('src/Observer/StanceController.cpp',
                                '        const bool owned = '
                                'inventory->inventoryCloseOverlayOwned();\n',
                                '        const bool owned = '
                                'inventory->inventoryCloseOverlayActive();\n'),
 'paired-release-clickthrough': ('src/Observer/StanceController.cpp',
                                 "            if (owned && _action == 'e' && event.type == "
                                 'SDL_MOUSEBUTTONDOWN)\n'
                                 '                inventory->_inventoryClickConsumedButtons |= '
                                 'button;\n',
                                 '            // Consumed DOWN no longer protects its matching '
                                 'release.\n')}
EXTRA_EDITS = {'unseen-overlay': (('    const bool valid = accept && _dialogPresented && _confirmEventAllowed && '
                     '!id.empty() && bindingCurrent(true);\n'
                     '    _confirmEventAllowed = false;\n'
                     '    // Retire only our InteractiveSurface.',
                     '    const bool valid = accept && _confirmEventAllowed && !id.empty() && '
                     'bindingCurrent(true);\n'
                     '    _confirmEventAllowed = false;\n'
                     '    // Retire only our InteractiveSurface.'),
                    ('            if (waiting != 0) invalidate();\n'
                     '        }\n'
                     '        return;\n'
                     '    }\n'
                     '#endif\n'
                     '    if (_dialog)',
                     '            if (false) invalidate(); // queued pre-flip consent incorrectly '
                     'accepted\n'
                     '        }\n'
                     '        return;\n'
                     '    }\n'
                     '#endif\n'
                     '    if (_dialog)'))}
FAIL = 'INVENTORY_CLICK_RUNTIME_MUTANT INCOMPLETE source-or-arguments'


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
        gate = root / 'tests/inventory-click-control/InventoryClickControlGate.cpp'
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
    print('INVENTORY_CLICK_RUNTIME_MUTANT APPLIED ' + kind + ' source-sha256=' + sha256(result.encode()).hexdigest())
    return 0


if __name__ == '__main__':
    sys.exit(main())
