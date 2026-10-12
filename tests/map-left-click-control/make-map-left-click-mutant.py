#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
"""Apply an actual ordinary-left-click fault to a disposable source copy.

Never edits the normal checkout. Setup success is not a detected runtime fault.
Rebuild each copy and require each master wrapper to reject the native run.
No test assertions, fixture packets, adapter returns, or mocks are altered.
"""
from hashlib import sha256
from pathlib import Path
import sys

MUTATIONS = {
    'stale-selector': ('src/Battlescape/BattlescapeState.cpp',
        'void BattlescapeState::mapLeftClickControlClick(int x, int y)\n{\n    // Identical selector setup to the right-click pilot, followed by exactly\n    // one normal map left-click. primaryAction owns movement, selection, costs,\n    // reserves, contact interruption, reaction fire and every possible no-op.\n    SDL_Event event = {};\n    event.type = SDL_MOUSEBUTTONUP;\n    event.button.button = SDL_BUTTON_LEFT; event.button.state = SDL_RELEASED;\n    event.button.x = x; event.button.y = y;\n    Action action(&event, 1.0, 1.0, 0, 0);\n    action.setMouseAction(x, y, _map->getX(), _map->getY()); action.setSender(_map);\n    _map->mouseOver(&action, this);\n    mapClick(&action);\n}',
        'void BattlescapeState::mapLeftClickControlClick(int x, int y)\n{\n    // Identical selector setup to the right-click pilot, followed by exactly\n    // one normal map left-click. primaryAction owns movement, selection, costs,\n    // reserves, contact interruption, reaction fire and every possible no-op.\n    SDL_Event event = {};\n    event.type = SDL_MOUSEBUTTONUP;\n    event.button.button = SDL_BUTTON_LEFT; event.button.state = SDL_RELEASED;\n    event.button.x = x; event.button.y = y;\n    Action action(&event, 1.0, 1.0, 0, 0);\n    action.setMouseAction(x, y, _map->getX(), _map->getY()); action.setSender(_map);\n    mapClick(&action);\n}'),
    'wrong-pixel': ('src/Battlescape/BattlescapeState.cpp',
        'void BattlescapeState::mapLeftClickControlClick(int x, int y)\n{\n    // Identical selector setup to the right-click pilot, followed by exactly\n    // one normal map left-click. primaryAction owns movement, selection, costs,\n    // reserves, contact interruption, reaction fire and every possible no-op.\n    SDL_Event event = {};\n    event.type = SDL_MOUSEBUTTONUP;\n    event.button.button = SDL_BUTTON_LEFT; event.button.state = SDL_RELEASED;\n    event.button.x = x; event.button.y = y;\n    Action action(&event, 1.0, 1.0, 0, 0);\n    action.setMouseAction(x, y, _map->getX(), _map->getY()); action.setSender(_map);\n    _map->mouseOver(&action, this);\n    mapClick(&action);\n}',
        'void BattlescapeState::mapLeftClickControlClick(int x, int y)\n{\n    // Identical selector setup to the right-click pilot, followed by exactly\n    // one normal map left-click. primaryAction owns movement, selection, costs,\n    // reserves, contact interruption, reaction fire and every possible no-op.\n    SDL_Event event = {};\n    event.type = SDL_MOUSEBUTTONUP;\n    event.button.button = SDL_BUTTON_LEFT; event.button.state = SDL_RELEASED;\n    x += 32;\n    event.button.x = x; event.button.y = y;\n    Action action(&event, 1.0, 1.0, 0, 0);\n    action.setMouseAction(x, y, _map->getX(), _map->getY()); action.setSender(_map);\n    _map->mouseOver(&action, this);\n    mapClick(&action);\n}'),
    'duplicate-click': ('src/Battlescape/BattlescapeState.cpp',
        'void BattlescapeState::mapLeftClickControlClick(int x, int y)\n{\n    // Identical selector setup to the right-click pilot, followed by exactly\n    // one normal map left-click. primaryAction owns movement, selection, costs,\n    // reserves, contact interruption, reaction fire and every possible no-op.\n    SDL_Event event = {};\n    event.type = SDL_MOUSEBUTTONUP;\n    event.button.button = SDL_BUTTON_LEFT; event.button.state = SDL_RELEASED;\n    event.button.x = x; event.button.y = y;\n    Action action(&event, 1.0, 1.0, 0, 0);\n    action.setMouseAction(x, y, _map->getX(), _map->getY()); action.setSender(_map);\n    _map->mouseOver(&action, this);\n    mapClick(&action);\n}',
        'void BattlescapeState::mapLeftClickControlClick(int x, int y)\n{\n    // Identical selector setup to the right-click pilot, followed by exactly\n    // one normal map left-click. primaryAction owns movement, selection, costs,\n    // reserves, contact interruption, reaction fire and every possible no-op.\n    SDL_Event event = {};\n    event.type = SDL_MOUSEBUTTONUP;\n    event.button.button = SDL_BUTTON_LEFT; event.button.state = SDL_RELEASED;\n    event.button.x = x; event.button.y = y;\n    Action action(&event, 1.0, 1.0, 0, 0);\n    action.setMouseAction(x, y, _map->getX(), _map->getY()); action.setSender(_map);\n    _map->mouseOver(&action, this);\n    mapClick(&action);\n    mapClick(&action);\n}'),
    'wrong-button': ('src/Battlescape/BattlescapeState.cpp',
        'void BattlescapeState::mapLeftClickControlClick(int x, int y)\n{\n    // Identical selector setup to the right-click pilot, followed by exactly\n    // one normal map left-click. primaryAction owns movement, selection, costs,\n    // reserves, contact interruption, reaction fire and every possible no-op.\n    SDL_Event event = {};\n    event.type = SDL_MOUSEBUTTONUP;\n    event.button.button = SDL_BUTTON_LEFT; event.button.state = SDL_RELEASED;\n    event.button.x = x; event.button.y = y;\n    Action action(&event, 1.0, 1.0, 0, 0);\n    action.setMouseAction(x, y, _map->getX(), _map->getY()); action.setSender(_map);\n    _map->mouseOver(&action, this);\n    mapClick(&action);\n}',
        'void BattlescapeState::mapLeftClickControlClick(int x, int y)\n{\n    // Identical selector setup to the right-click pilot, followed by exactly\n    // one normal map left-click. primaryAction owns movement, selection, costs,\n    // reserves, contact interruption, reaction fire and every possible no-op.\n    SDL_Event event = {};\n    event.type = SDL_MOUSEBUTTONUP;\n    event.button.button = SDL_BUTTON_RIGHT; event.button.state = SDL_RELEASED;\n    event.button.x = x; event.button.y = y;\n    Action action(&event, 1.0, 1.0, 0, 0);\n    action.setMouseAction(x, y, _map->getX(), _map->getY()); action.setSender(_map);\n    _map->mouseOver(&action, this);\n    mapClick(&action);\n}'),
    'free-walk': ('src/Battlescape/UnitWalkBState.cpp',
        '\t\t\tif (_unit->spendTimeUnits(tu))\n',
        '\t\t\tif (_unit->spendTimeUnits(0))\n'),
    'free-energy': ('src/Battlescape/UnitWalkBState.cpp',
        '\t\t\t\tif (_unit->spendEnergy(energy))\n',
        '\t\t\t\tif (_unit->spendEnergy(0))\n'),
    'reserve-bypass': ('src/Battlescape/UnitWalkBState.cpp',
        '\t\t\tif (_parent->getPanicHandled() && _parent->checkReservedTU(_unit, tu) == false)\n',
        '\t\t\tif (false)\n'),
    'contact-bypass': ('src/Battlescape/UnitWalkBState.cpp',
        '\t\tif (unitSpotted && !_action.desperate && !_unit->getCharging() && !_falling)\n',
        '\t\tif (false)\n'),
    'reaction-bypass': ('src/Battlescape/UnitWalkBState.cpp',
        '\t\t\t\tif (_terrain->checkReactionFire(_unit))\n',
        '\t\t\t\tif (false)\n'),
    'door-right-cost': ('src/Battlescape/UnitWalkBState.cpp',
        '\t\t\t\tint door = _terrain->unitOpensDoor(_unit, false, dir);\n',
        '\t\t\t\tint door = _terrain->unitOpensDoor(_unit, true, dir);\n'),
    'preview-bypass': ('src/Battlescape/BattlescapeState.cpp',
        'bool BattlescapeState::mapLeftClickControlAllowed() const\n{\n    return Options::battleNewPreviewPath == PATH_NONE;\n}',
        'bool BattlescapeState::mapLeftClickControlAllowed() const\n{\n    return true;\n}'),
    'camera-generation': ('src/Battlescape/Camera.cpp',
        '        if (offset != oldOffset || width != oldWidth || height != oldHeight || visible != oldVisible || layers != oldLayers)\n            ++revision;\n',
        '        if (false) ++revision;\n'),
    'pose-generation': ('src/Savegame/BattleUnit.cpp',
        '        if (position != oldPosition || direction != oldDirection || turret != oldTurret) ++revision;\n',
        '        if (false) ++revision;\n'),
    'wire-pixel': ('src/Observer/StanceController.cpp',
        '        if (!canonicalView(rgb, indicesHex, paletteHex)) { invalidate(); unavailable(); return; }\n',
        "        if (!canonicalView(rgb, indicesHex, paletteHex)) { invalidate(); unavailable(); return; }\n        indicesHex[1] = indicesHex[1] == '0' ? '1' : '0';\n"),
    'unseen-modal': ('src/Observer/StanceController.cpp',
        '            _confirmEventAllowed = _dialogPresented && !_yDown && event.key.keysym.mod == KMOD_NONE;\n',
        '            _confirmEventAllowed = !_yDown && event.key.keysym.mod == KMOD_NONE;\n'),
    'short-busy-wait': ('src/Observer/StanceController.cpp',
        '                _challengeDeadline = _mapLeftClickDispatchTime + 8000;\n',
        '                _challengeDeadline = _mapLeftClickDispatchTime + 1500;\n'),
    'timeout-disabled': ('src/Observer/StanceController.cpp',
        '        else if (now() - _mapLeftClickDispatchTime >= 8000)\n',
        '        else if (false)\n'),
    'id-conflict-bypass': ('src/Observer/StanceController.cpp',
        '        if (prior->second.request == request) replyStatus(id);\n',
        '        if (true) replyStatus(id);\n'),
}


def mutate(kind, source):
    _, anchor, replacement = MUTATIONS[kind]
    if 'Map left-click sensitivity fault:' in source or source.count(anchor) != 1:
        raise ValueError('source-anchor')
    result = source.replace(anchor, '    // Map left-click sensitivity fault: ' + kind + '\n' + replacement)
    if kind == 'unseen-modal':
        guard = '    const bool valid = accept && _dialogPresented && _confirmEventAllowed && !id.empty() && bindingCurrent(true);\n'
        if result.count(guard) != 1:
            raise ValueError('guard-anchor')
        result = result.replace(guard, guard.replace(' && _dialogPresented', ''))
    if kind == 'timeout-disabled':
        deadline = '                _challengeDeadline = _mapLeftClickDispatchTime + 8000;\n'
        if result.count(deadline) != 1:
            raise ValueError('deadline-anchor')
        result = result.replace(deadline, deadline.replace('+ 8000', '+ 800000'))
    return result


def main(args=None):
    args = sys.argv[1:] if args is None else args
    try:
        if len(args) != 2 or args[0] not in MUTATIONS:
            raise ValueError('arguments')
        kind, root = args[0], Path(args[1])
        if not root.is_absolute() or not (root / 'tests/map-left-click-control/MapLeftClickControlGate.cpp').is_file():
            raise ValueError('checkout')
        if root.resolve() == Path(__file__).resolve().parents[2]:
            raise ValueError('normal-checkout')
        path = root / MUTATIONS[kind][0]
        result = mutate(kind, path.read_text())
        path.write_text(result)
    except (OSError, ValueError, KeyError):
        print('MAP_LEFT_CLICK_RUNTIME_MUTANT INCOMPLETE source-or-arguments', file=sys.stderr)
        return 1
    print('MAP_LEFT_CLICK_RUNTIME_MUTANT APPLIED ' + kind + ' source-sha256=' + sha256(result.encode()).hexdigest())
    return 0


if __name__ == '__main__':
    sys.exit(main())
