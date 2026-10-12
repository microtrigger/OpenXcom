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

MARKER = 'Inventory Unload sensitivity fault:'
FAIL = 'INVENTORY_UNLOAD_RUNTIME_MUTANT INCOMPLETE source-or-arguments'
MUTATIONS = {'unload-runtime-bypass': ('src/Observer/StanceController.cpp',
                           (('    _inventoryUnloadEnabled = _inventoryCloseEnabled && inventoryUnload && '
                             'std::string(inventoryUnload) == "1";\n',
                             '    _inventoryUnloadEnabled = _inventoryCloseEnabled;\n'),)),
 'close-prerequisite-bypass': ('src/Observer/StanceController.cpp',
                               (('    _inventoryUnloadEnabled = _inventoryCloseEnabled && inventoryUnload '
                                 '&& std::string(inventoryUnload) == "1";\n',
                                 '    _inventoryUnloadEnabled = inventoryUnload && '
                                 'std::string(inventoryUnload) == "1";\n'),)),
 'missing-down': ('src/Observer/StanceController.cpp',
                  (('        state->inventoryControlUnloadEvent(SDL_MOUSEBUTTONDOWN);\n',
                    '        // Ordinary DOWN omitted.\n'),)),
 'duplicate-down': ('src/Observer/StanceController.cpp',
                    (('        state->inventoryControlUnloadEvent(SDL_MOUSEBUTTONDOWN);\n',
                      '        state->inventoryControlUnloadEvent(SDL_MOUSEBUTTONDOWN);\n'
                      '        state->inventoryControlUnloadEvent(SDL_MOUSEBUTTONDOWN);\n'),)),
 'missing-up': ('src/Observer/StanceController.cpp',
                (('        if (survives()) state->inventoryControlUnloadEvent(SDL_MOUSEBUTTONUP);\n',
                  '        // Ordinary UP omitted.\n'),)),
 'duplicate-up': ('src/Observer/StanceController.cpp',
                  (('        if (survives()) state->inventoryControlUnloadEvent(SDL_MOUSEBUTTONUP);\n',
                    '        if (survives()) { state->inventoryControlUnloadEvent(SDL_MOUSEBUTTONUP); '
                    'state->inventoryControlUnloadEvent(SDL_MOUSEBUTTONUP); }\n'),)),
 'direct-button-shortcut': ('src/Battlescape/InventoryState.cpp',
                            (('    State::handle(&action);\n',
                              '    if (type == SDL_MOUSEBUTTONUP) btnUnloadClick(&action);\n'),)),
 'wrong-fixed-coordinate': ('src/Battlescape/InventoryState.cpp',
                            (('    event.button.x = 304; event.button.y = 44;\n',
                              '    event.button.x = 303; event.button.y = 44;\n'),)),
 'missing-prehandler-barrier': ('src/Observer/StanceController.cpp',
                                (('        _awaitingInventoryView = true; _inventoryDestination = 0;\n'
                                  '        _inventoryUnloadDispatching = true; _inventoryUnloadUnexpected '
                                  '= false;\n',
                                  '        _awaitingInventoryView = false; _inventoryDestination = 0;\n'
                                  '        _inventoryUnloadDispatching = true; _inventoryUnloadUnexpected '
                                  '= false;\n'),)),
 'late-prehandler-barrier': ('src/Observer/StanceController.cpp',
                             (('        _awaitingInventoryView = true; _inventoryDestination = 0;\n'
                               '        _inventoryUnloadDispatching = true; _inventoryUnloadUnexpected = '
                               'false;\n'
                               '        state->inventoryControlUnloadEvent(SDL_MOUSEBUTTONDOWN);\n',
                               '        _inventoryUnloadDispatching = true; _inventoryUnloadUnexpected = '
                               'false;\n'
                               '        state->inventoryControlUnloadEvent(SDL_MOUSEBUTTONDOWN);\n'
                               '        _awaitingInventoryView = true; _inventoryDestination = 0;\n'),)),
 'missing-invclick-tail': ('src/Battlescape/Inventory.cpp',
                           (('\tInteractiveSurface::mouseClick(action, state);\n',
                             '\t// Ordinary invClick callback omitted.\n'),)),
 'duplicate-invclick-tail': ('src/Battlescape/Inventory.cpp',
                             (('\tInteractiveSurface::mouseClick(action, state);\n',
                               '\tInteractiveSurface::mouseClick(action, state);\n'
                               '\tInteractiveSurface::mouseClick(action, state);\n'),)),
 'ignored-between-edge-transition': ('src/Observer/StanceController.cpp',
                                     (('            return !_inventoryUnloadUnexpected && '
                                       '_game->isState(state) &&\n',
                                       '            return _game->isState(state) &&\n'),)),
 'ignored-synchronous-save-notice': ('src/Observer/StanceController.cpp',
                                     (('void StanceController::beforeSaveChange()\n'
                                       '{\n'
                                       '#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD\n'
                                       '    if (_inventoryUnloadDispatching) _inventoryUnloadUnexpected = '
                                       'true;\n',
                                       'void StanceController::beforeSaveChange()\n'
                                       '{\n'
                                       '#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD\n'
                                       '    // Dispatch-time save notice ignored.\n'),)),
 'surviving-destination-omitted': ('src/Observer/StanceController.cpp',
                                   (('        if (survived) _inventoryDestination = state;\n',
                                     '        if (survived) _inventoryDestination = 0;\n'),)),
 'lost-I-barrier-cleared': ('src/Observer/StanceController.cpp',
                            (('    if (state == _inventoryDestination) _inventoryDestination = 0;\n',
                              '    if (state == _inventoryDestination) { _inventoryDestination = 0; '
                              '_awaitingInventoryView = false; }\n'),)),
 'failed-I-send-clears': ('src/Observer/StanceController.cpp',
                          (('        if (sendPacket(packet) && _awaitingInventoryView && _frameInventory '
                            '== _inventoryDestination)\n',
                            '        if ((sendPacket(packet), true) && _awaitingInventoryView && '
                            '_frameInventory == _inventoryDestination)\n'),)),
 'unload-kind-confusion': ('src/Observer/StanceController.cpp',
                           (("(_contextKind == 'Q' && inventoryUnload)",
                             "((_contextKind == 'Q' || _contextKind == 'H') && inventoryUnload)"),)),
 'wire-Q-pixel': ('src/Observer/StanceController.cpp',
                  (("        if (_contextKind == 'Q') schema = "
                    '"experimental-inventory-unload-context/1";\n',
                    "        if (_contextKind == 'Q') { schema = "
                    '"experimental-inventory-unload-context/1"; indicesHex[1] = indicesHex[1] == \'0\' ? '
                    "'1' : '0'; }\n"),)),
 'unload-overlay-promises-success': ('src/Observer/InventoryCloseOverlay.cpp',
                                     (('"Ordinary inventory Unload.\\nMay spend 8 TU; normal '
                                       'refusal.\\nConfirm the soldier shown above."',
                                       '"Ordinary inventory Unload.\\nAlways unloads successfully.\\nNo '
                                       'normal refusal is possible."'),)),
 'hidden-button-admitted': ('src/Battlescape/InventoryState.cpp',
                            (('        !_btnUnload->turnContextVisible() || '
                              '!_btnUnload->stanceContextVisible() || _btnUnload->stanceToggled() ||\n',
                              '        _btnUnload->stanceToggled() ||\n'),)),
 'held-position-not-bound': ('src/Observer/StanceController.cpp',
                             (('        && heldRevision == o.heldRevision && selectionX == o.selectionX && '
                               'selectionY == o.selectionY\n',
                               '        && heldRevision == o.heldRevision\n'),)),
 'held-revision-not-bound': ('src/Observer/StanceController.cpp',
                             (('        && heldRevision == o.heldRevision && selectionX == o.selectionX && '
                               'selectionY == o.selectionY\n',
                               '        && selectionX == o.selectionX && selectionY == o.selectionY\n'),)),
 'invalid-child-clickthrough': ('src/Observer/StanceController.cpp',
                                (('        const bool owned = inventory->inventoryCloseOverlayOwned();\n',
                                  '        const bool owned = '
                                  'inventory->inventoryCloseOverlayActive();\n'),)),
 'orphan-Y-new-pair-swallowed': ('src/Observer/StanceController.cpp',
                                 (('    if (!rightOwned && event.type == SDL_KEYDOWN && '
                                   'event.key.keysym.sym == SDLK_y)\n'
                                   '        _inventoryOwnedYRelease = false;\n',
                                   '    // New ordinary Y incorrectly inherits old owned release.\n'),)),
 'orphan-mouse-new-pair-swallowed': ('src/Observer/StanceController.cpp',
                                     (('    if (inventory && !pointerOwned && event.type == '
                                       'SDL_MOUSEBUTTONDOWN &&\n'
                                       '        event.button.button >= 1 && event.button.button <= 8)\n'
                                       '        inventory->_inventoryClickConsumedButtons &= '
                                       '~std::uint8_t(1u << (event.button.button - 1));\n',
                                       '    // New ordinary mouse pair retains local ownership.\n'),
                                      ('    if (!rightOwned && event.type == SDL_MOUSEBUTTONDOWN && '
                                       'event.button.button >= 1 && event.button.button <= 8)\n',
                                       '    if (false && !rightOwned && event.type == SDL_MOUSEBUTTONDOWN '
                                       '&& event.button.button >= 1 && event.button.button <= 8)\n'))),
 'ordinary-seven-TU': ('src/Battlescape/Inventory.cpp',
                       (('\tif (!_tu || _selUnit->spendTimeUnits(8))\n',
                         '\tif (!_tu || _selUnit->spendTimeUnits(7))\n'),)),
 'ordinary-zero-ammo-refused': ('src/Battlescape/Inventory.cpp',
                                (('\tif (_selItem->getAmmoItem() == 0 || !_selItem->needsAmmo())\n',
                                  '\tif (_selItem->getAmmoItem() == 0 || !_selItem->needsAmmo() || '
                                  '_selItem->getAmmoItem()->getAmmoQuantity() == 0)\n'),)),
 'ordinary-hands-check-omitted': ('src/Battlescape/Inventory.cpp',
                                  (('\t\tif ((*i)->getSlot()->getType() == INV_HAND && (*i) != _selItem)\n',
                                    '\t\tif (false && (*i)->getSlot()->getType() == INV_HAND && (*i) != '
                                    '_selItem)\n'),)),
 'ordinary-no-ammo-warning-omitted': ('src/Battlescape/Inventory.cpp',
                                      (('\t\t'
                                        '_warning->showMessage(_game->getLanguage()->getString("STR_NO_AMMUNITION_LOADED"));\n',
                                        '\t\t// No-ammunition warning omitted.\n'),)),
 'ordinary-hands-warning-wrong': ('src/Battlescape/Inventory.cpp',
                                  (('\t\t\t'
                                    '_warning->showMessage(_game->getLanguage()->getString("STR_BOTH_HANDS_MUST_BE_EMPTY"));\n',
                                    '\t\t\t'
                                    '_warning->showMessage(_game->getLanguage()->getString("STR_NOT_ENOUGH_TIME_UNITS"));\n'),)),
 'ordinary-detach-omitted': ('src/Battlescape/Inventory.cpp',
                             (('\t\t_selItem->setAmmoItem(0);\n',
                               '\t\t// Ammunition detachment omitted.\n'),)),
 'ordinary-selection-clear-omitted': ('src/Battlescape/Inventory.cpp',
                                      (('\t\t_selItem->setAmmoItem(0);\n\t\tsetSelectedItem(0);\n',
                                        '\t\t_selItem->setAmmoItem(0);\n'
                                        '\t\t// Held selection incorrectly retained.\n'),)),
 'ordinary-sound-lookup-omitted': ('src/Battlescape/InventoryState.cpp',
                                   (('\t\t_game->getMod()->getSoundByDepth(0, Mod::ITEM_DROP)->play();\n',
                                     '\t\t// Normal success sound lookup and play omitted.\n'),)),
 'ordinary-sound-depth-wrong': ('src/Battlescape/InventoryState.cpp',
                                (('\t\t_game->getMod()->getSoundByDepth(0, Mod::ITEM_DROP)->play();\n',
                                  '\t\t_game->getMod()->getSoundByDepth(1, Mod::ITEM_DROP)->play();\n'),)),
 'ordinary-preview-clear-omitted': ('src/Battlescape/InventoryState.cpp',
                                    (('\t\t_selAmmo->clear();\n\t\tupdateStats();\n',
                                      '\t\t// Ammo preview incorrectly retained.\n\t\tupdateStats();\n'),))}

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
        gate = root / 'tests/inventory-unload-control/InventoryUnloadControlGate.cpp'
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
    print('INVENTORY_UNLOAD_RUNTIME_MUTANT APPLIED ' + kind + ' source-sha256=' + sha256(result.encode()).hexdigest())
    return 0


if __name__ == '__main__':
    sys.exit(main())
