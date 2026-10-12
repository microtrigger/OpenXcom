#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
"""Apply an actual native turn fault to a disposable copy, never this checkout.

Setup success is not a detected runtime fault. Rebuild each separate copy and
require both master gates to fail. No gate assertions or packets are changed.
"""
from hashlib import sha256
from pathlib import Path
import sys

MUTATIONS = {
    'stale-selector': ('src/Battlescape/BattlescapeState.cpp',
        'void BattlescapeState::turnControlClick(int x, int y)\n{\n    // The ordinary Map mouse-over path establishes its cached selector, including\n    // the sprite-height offset. The ordinary right-click then owns all gameplay.\n    SDL_Event event = {};\n    event.type = SDL_MOUSEBUTTONUP;\n    event.button.button = SDL_BUTTON_RIGHT; event.button.state = SDL_RELEASED;\n    event.button.x = x; event.button.y = y;\n    Action action(&event, 1.0, 1.0, 0, 0);\n    action.setMouseAction(x, y, _map->getX(), _map->getY()); action.setSender(_map);\n    _map->mouseOver(&action, this);\n    mapClick(&action);\n}',
        'void BattlescapeState::turnControlClick(int x, int y)\n{\n    // The ordinary Map mouse-over path establishes its cached selector, including\n    // the sprite-height offset. The ordinary right-click then owns all gameplay.\n    SDL_Event event = {};\n    event.type = SDL_MOUSEBUTTONUP;\n    event.button.button = SDL_BUTTON_RIGHT; event.button.state = SDL_RELEASED;\n    event.button.x = x; event.button.y = y;\n    Action action(&event, 1.0, 1.0, 0, 0);\n    action.setMouseAction(x, y, _map->getX(), _map->getY()); action.setSender(_map);\n    mapClick(&action);\n}'),
    'wrong-pixel': ('src/Battlescape/BattlescapeState.cpp',
        'void BattlescapeState::turnControlClick(int x, int y)\n{\n    // The ordinary Map mouse-over path establishes its cached selector, including\n    // the sprite-height offset. The ordinary right-click then owns all gameplay.\n    SDL_Event event = {};\n    event.type = SDL_MOUSEBUTTONUP;\n    event.button.button = SDL_BUTTON_RIGHT; event.button.state = SDL_RELEASED;\n    event.button.x = x; event.button.y = y;\n    Action action(&event, 1.0, 1.0, 0, 0);\n    action.setMouseAction(x, y, _map->getX(), _map->getY()); action.setSender(_map);\n    _map->mouseOver(&action, this);\n    mapClick(&action);\n}',
        'void BattlescapeState::turnControlClick(int x, int y)\n{\n    // The ordinary Map mouse-over path establishes its cached selector, including\n    // the sprite-height offset. The ordinary right-click then owns all gameplay.\n    SDL_Event event = {};\n    event.type = SDL_MOUSEBUTTONUP;\n    event.button.button = SDL_BUTTON_RIGHT; event.button.state = SDL_RELEASED;\n    x += 32;\n    event.button.x = x; event.button.y = y;\n    Action action(&event, 1.0, 1.0, 0, 0);\n    action.setMouseAction(x, y, _map->getX(), _map->getY()); action.setSender(_map);\n    _map->mouseOver(&action, this);\n    mapClick(&action);\n}'),
    'duplicate-turn': ('src/Battlescape/BattlescapeState.cpp',
                       '\t\t\t_battleGame->secondaryAction(pos);\n',
                       '\t\t\t_battleGame->secondaryAction(pos);\n\t\t\t_battleGame->secondaryAction(pos);\n'),
    'free-turn': ('src/Battlescape/UnitTurnBState.cpp',
                  '\tconst int tu = _chargeTUs ? 1 : 0;\n',
                  '\tconst int tu = 0;\n'),
    'reserve-bypass': ('src/Battlescape/UnitTurnBState.cpp',
                       '&& !_parent->checkReservedTU(_unit, tu))\n',
                       '&& false)\n'),
    'camera-generation': ('src/Battlescape/Camera.cpp',
                          '        if (offset != oldOffset || width != oldWidth || height != oldHeight || visible != oldVisible || layers != oldLayers)\n            ++revision;\n',
                          '        if (false) ++revision;\n'),
    'pose-generation': ('src/Savegame/BattleUnit.cpp',
                        '        if (position != oldPosition || direction != oldDirection || turret != oldTurret) ++revision;\n',
                        '        if (false) ++revision;\n'),
    'wire-pixel': ('src/Observer/StanceController.cpp',
        '        if (!canonicalView(rgb, indicesHex, paletteHex)) { invalidate(); unavailable(); return; }\n        _rgb.assign(rgb.begin() + 320 * 144 * 3, rgb.end());\n',
        "        if (!canonicalView(rgb, indicesHex, paletteHex)) { invalidate(); unavailable(); return; }\n        indicesHex[1] = indicesHex[1] == '0' ? '1' : '0';\n        _rgb.assign(rgb.begin() + 320 * 144 * 3, rgb.end());\n"),
    'unseen-modal': ('src/Observer/StanceController.cpp',
                     '            _confirmEventAllowed = _dialogPresented && !_yDown && event.key.keysym.mod == KMOD_NONE;\n',
                     '            _confirmEventAllowed = !_yDown && event.key.keysym.mod == KMOD_NONE;\n'),
}


def mutate(kind, source):
    _, anchor, replacement = MUTATIONS[kind]
    if 'Turn sensitivity fault:' in source or source.count(anchor) != 1:
        raise ValueError('source-anchor')
    result = source.replace(anchor, '    // Turn sensitivity fault: ' + kind + '\n' + replacement)
    if kind == 'unseen-modal':
        guard = '    const bool valid = accept && _dialogPresented && _confirmEventAllowed && !id.empty() && bindingCurrent(true);\n    const bool selectionValid = valid && (!selectionRequest() ||\n'
        if result.count(guard) != 1:
            raise ValueError('guard-anchor')
        result = result.replace(guard, guard.replace(' && _dialogPresented', ''))
    return result


def main(args=None):
    args = sys.argv[1:] if args is None else args
    try:
        if len(args) != 2 or args[0] not in MUTATIONS:
            raise ValueError('arguments')
        kind, root = args[0], Path(args[1])
        if not root.is_absolute() or not (root / 'tests/turn-control/TurnControlGate.cpp').is_file():
            raise ValueError('checkout')
        if root.resolve() == Path(__file__).resolve().parents[2]:
            raise ValueError('normal-checkout')
        path = root / MUTATIONS[kind][0]
        result = mutate(kind, path.read_text())
        path.write_text(result)
    except (OSError, ValueError, KeyError):
        print('TURN_RUNTIME_MUTANT INCOMPLETE source-or-arguments', file=sys.stderr)
        return 1
    print('TURN_RUNTIME_MUTANT APPLIED ' + kind + ' source-sha256=' + sha256(result.encode()).hexdigest())
    return 0


if __name__ == '__main__':
    sys.exit(main())
