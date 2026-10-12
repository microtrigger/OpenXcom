#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Predeclared actual-production-source faults; never edit gate/oracle/adapter.

Application/build is setup only. Runtime classification requires the exact
profile and semantic endpoint in fault-map.json. Unrelated assertion, crash,
timeout, launch/build failure is inconclusive, never a detection.
"""
from hashlib import sha256
from pathlib import Path
import sys

MARKER = 'Inventory RIGHT sensitivity fault:'
FAIL = 'INVENTORY_RIGHT_CLICK_RUNTIME_MUTANT INCOMPLETE source-or-arguments'
MUTATIONS = {'right-runtime-bypass': ('src/Observer/StanceController.cpp',
                          (('    _inventoryRightClickEnabled = _inventoryCloseEnabled && inventoryRightClick '
                            '&& std::string(inventoryRightClick) == "1";\n',
                            '    _inventoryRightClickEnabled = _inventoryCloseEnabled;\n'),)),
 'close-prerequisite-bypass': ('src/Observer/StanceController.cpp',
                               (('    _inventoryRightClickEnabled = _inventoryCloseEnabled && '
                                 'inventoryRightClick && std::string(inventoryRightClick) == "1";\n',
                                 '    _inventoryRightClickEnabled = inventoryRightClick && '
                                 'std::string(inventoryRightClick) == "1";\n'),)),
 'missing-right-click': ('src/Battlescape/InventoryState.cpp',
                         (('    _inv->mouseClick(&rightAction, this);\n',
                           '    // Ordinary RIGHT callback omitted.\n'),)),
 'duplicate-right-click': ('src/Battlescape/InventoryState.cpp',
                           (('    _inv->mouseClick(&rightAction, this);\n',
                             '    _inv->mouseClick(&rightAction, this);\n'
                             '    _inv->mouseClick(&rightAction, this);\n'),)),
 'missing-right-hover': ('src/Battlescape/InventoryState.cpp',
                         (('    _inv->mouseOver(&rightAction, this);\n',
                           '    // Ordinary hover omitted.\n'),)),
 'wrong-right-button': ('src/Battlescape/InventoryState.cpp',
                        (('    event.button.button = SDL_BUTTON_RIGHT; event.button.state = SDL_RELEASED;\n',
                          '    event.button.button = SDL_BUTTON_LEFT; event.button.state = '
                          'SDL_RELEASED;\n'),)),
 'missing-prehandler-barrier': ('src/Observer/StanceController.cpp',
                                (('        _awaitingInventoryView = true; _inventoryDestination = 0;\n'
                                  '        _inventoryRightOrigin = state; _inventoryRightDispatching = '
                                  'true;\n',
                                  '        _awaitingInventoryView = false; _inventoryDestination = 0;\n'
                                  '        _inventoryRightOrigin = state; _inventoryRightDispatching = '
                                  'true;\n'),)),
 'late-prehandler-barrier': ('src/Observer/StanceController.cpp',
                             (('        _awaitingInventoryView = true; _inventoryDestination = 0;\n'
                               '        _inventoryRightOrigin = state; _inventoryRightDispatching = true;\n'
                               '        _inventoryRightPopped = _inventoryRightUnexpected = false;\n'
                               '        state->inventoryControlRightClick(_targetX, _targetY);\n',
                               '        _inventoryRightOrigin = state; _inventoryRightDispatching = true;\n'
                               '        _inventoryRightPopped = _inventoryRightUnexpected = false;\n'
                               '        state->inventoryControlRightClick(_targetX, _targetY);\n'
                               '        _awaitingInventoryView = true; _inventoryDestination = 0;\n'),)),
 'missing-callback-tail': ('src/Battlescape/Inventory.cpp',
                           (('\tInteractiveSurface::mouseClick(action, state);\n',
                             '\t// Ordinary invClick tail omitted.\n'),)),
 'unexpected-transition-ignored': ('src/Observer/StanceController.cpp',
                                   (('        if (_inventoryRightUnexpected) return;\n',
                                     '        // Intervening transitions incorrectly accepted.\n'),)),
 'surviving-destination-omitted': ('src/Observer/StanceController.cpp',
                                   (('        if (!_inventoryRightPopped && _game->isState(state) && '
                                     '_game->inventoryParentState(origin.parent))\n'
                                     '            _inventoryDestination = state;\n',
                                     '        if (!_inventoryRightPopped && _game->isState(state) && '
                                     '_game->inventoryParentState(origin.parent))\n'
                                     '            _inventoryDestination = 0;\n'),)),
 'lost-I-barrier-cleared': ('src/Observer/StanceController.cpp',
                            (('    if (state == _inventoryDestination) _inventoryDestination = 0;\n',
                              '    if (state == _inventoryDestination) { _inventoryDestination = 0; '
                              '_awaitingInventoryView = false; }\n'),)),
 'failed-I-send-clears': ('src/Observer/StanceController.cpp',
                          (('        if (sendPacket(packet) && _awaitingInventoryView && _frameInventory == '
                            '_inventoryDestination)\n',
                            '        if ((sendPacket(packet), true) && _awaitingInventoryView && '
                            '_frameInventory == _inventoryDestination)\n'),)),
 'failed-V-send-clears': ('src/Observer/StanceController.cpp',
                          (('        if (sendPacket(packet))\n'
                            '        {\n'
                            '#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE\n',
                            '        if ((sendPacket(packet), true))\n'
                            '        {\n'
                            '#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE\n'),)),
 'right-domain-bypass': ('src/Observer/StanceController.cpp',
                         (('        (inventoryRightClick && !((x >= 0 && x < 32 && y >= 63 && y < 111) ||\n'
                           '            (x >= 128 && x < 160 && y >= 63 && y < 111) || (x >= 0 && x < 320 && '
                           'y >= 152 && y < 200))) ||\n',
                           '        false ||\n'),)),
 'right-kind-confusion': ('src/Observer/StanceController.cpp',
                          (("!((_contextKind == 'U' && inventoryRightClick) || (_contextKind == 'O'",
                            "!(((_contextKind == 'U' || _contextKind == 'H') && inventoryRightClick) || "
                            "(_contextKind == 'O'"),)),
 'wire-U-pixel': ('src/Observer/StanceController.cpp',
                  (("        if (_contextKind == 'U') schema = "
                    '"experimental-inventory-right-click-context/1";\n',
                    "        if (_contextKind == 'U') { schema = "
                    '"experimental-inventory-right-click-context/1"; indicesHex[1] = indicesHex[1] == \'0\' '
                    "? '1' : '0'; }\n"),)),
 'right-overlay-predicts-only-cancel': ('src/Observer/InventoryCloseOverlay.cpp',
                                        (('"): held = cancel.\\nOtherwise closes inventory.\\nNormal '
                                          'gravity, lighting/FOV.");',
                                          '"): held = cancel.\\nInventory will remain open.\\nNo other '
                                          'outcome.");'),)),
 'invalid-child-clickthrough': ('src/Observer/StanceController.cpp',
                                (('        const bool owned = inventory->inventoryCloseOverlayOwned();\n',
                                  '        const bool owned = '
                                  'inventory->inventoryCloseOverlayActive();\n'),)),
 'orphan-Y-new-pair-swallowed': ('src/Observer/StanceController.cpp',
                                 (('    if (!rightOwned && event.type == SDL_KEYDOWN && event.key.keysym.sym '
                                   '== SDLK_y)\n'
                                   '        _inventoryOwnedYRelease = false;\n',
                                   '    // New Y gesture incorrectly inherits old release ownership.\n'),)),
 'Y-tail-after-close': ('src/Observer/StanceController.cpp',
                        (('        _inventoryOwnedYRelease = false; _yDown = false;\n'
                          '        if (inventory) inventory->_inventoryCloseConsumeEvent = true;\n'
                          '        return true;\n',
                          '        _inventoryOwnedYRelease = false; _yDown = false;\n'
                          '        if (inventory) inventory->_inventoryCloseConsumeEvent = true;\n'
                          '        return false;\n'),)),
 'mouse-tail-after-close': ('src/Observer/StanceController.cpp',
                            (('                if (event.button.button == SDL_BUTTON_LEFT) _mouseDown = '
                              '_mouseArmed = false;\n'
                              '                return true;\n',
                              '                if (event.button.button == SDL_BUTTON_LEFT) _mouseDown = '
                              '_mouseArmed = false;\n'
                              '                return false;\n'),)),
 'orphan-mouse-new-pair-swallowed': ('src/Observer/StanceController.cpp',
                                     (('    if (inventory && !pointerOwned && event.type == '
                                       'SDL_MOUSEBUTTONDOWN &&\n'
                                       '        event.button.button >= 1 && event.button.button <= 8)\n'
                                       '        inventory->_inventoryClickConsumedButtons &= '
                                       '~std::uint8_t(1u << (event.button.button - 1));\n',
                                       '    // New ordinary pair incorrectly retains per-inventory '
                                       'ownership.\n'),
                                      ('    if (!rightOwned && event.type == SDL_MOUSEBUTTONDOWN && '
                                       'event.button.button >= 1 && event.button.button <= 8)\n',
                                       '    if (false && !rightOwned && event.type == SDL_MOUSEBUTTONDOWN && '
                                       'event.button.button >= 1 && event.button.button <= 8)\n'))),
 'paired-release-clickthrough': ('src/Observer/StanceController.cpp',
                                 (("            if (owned && _action == 'x' && event.type == "
                                   'SDL_MOUSEBUTTONDOWN)\n',
                                   "            if (false && owned && _action == 'x' && event.type == "
                                   'SDL_MOUSEBUTTONDOWN)\n'),)),
 'synchronous-save-notice-ignored': ('src/Observer/StanceController.cpp',
                                     (('void StanceController::beforeSaveChange()\n'
                                       '{\n'
                                       '#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK\n'
                                       '    if (_inventoryRightDispatching) _inventoryRightUnexpected = '
                                       'true;\n',
                                       'void StanceController::beforeSaveChange()\n'
                                       '{\n'
                                       '#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK\n'
                                       '    // Synchronous save notification incorrectly ignored.\n'),))}

def mutate(kind, source):
    _, edits = MUTATIONS[kind]
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
    if path.is_file() and original.is_file() and path.samefile(original):
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
        gate = root / 'tests/inventory-right-click-control/InventoryRightClickControlGate.cpp'
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
    print('INVENTORY_RIGHT_CLICK_RUNTIME_MUTANT APPLIED ' + kind + ' source-sha256=' + sha256(result.encode()).hexdigest())
    return 0


if __name__ == '__main__':
    sys.exit(main())
