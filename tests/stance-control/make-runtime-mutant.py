#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md.
"""Apply one actual native-path sensitivity fault to a disposable source copy.

Never call this on the normal checkout. This does not alter test assertions,
fixtures, labels, packets, or a mock; the rebuilt native game must fail its gate.
Use a separate source copy and build directory for every sensitivity control.
"""
from hashlib import sha256
from pathlib import Path
import sys

DISPATCH = '    _context.state->btnKneelClick(0);\n'
MUTATIONS = {
    'wrong-actor': ('src/Observer/StanceController.cpp', DISPATCH,
                    '    // Sensitivity fault: forward valid dispatch to the wrong real unit.\n'
                    '    _context.save->setSelectedUnit(_context.save->getUnits()->at(1));\n' + DISPATCH),
    'duplicate': ('src/Observer/StanceController.cpp', DISPATCH,
                  '    // Sensitivity fault: repeat the real normal UI dispatch.\n' + DISPATCH + DISPATCH),
    'wrong-tu': ('src/Battlescape/BattlescapeGame.cpp', '\tint tu = bu->isKneeled()?8:4;\n',
                 '\t// Sensitivity fault: wrong actual kneel/stand TU cost.\n\tint tu = bu->isKneeled()?7:3;\n'),
    'unseen-modal': ('src/Observer/StanceController.cpp',
                     '            _confirmEventAllowed = _dialogPresented && !_yDown && event.key.keysym.mod == KMOD_NONE;\n',
                     '            // Sensitivity fault: allow Y before this modal was displayed.\n'
                     '            _confirmEventAllowed = !_yDown && event.key.keysym.mod == KMOD_NONE;\n'),
    'reaction-bypass': ('src/Battlescape/BattlescapeGame.cpp',
                        '\t\t\tgetTileEngine()->checkReactionFire(bu);\n',
                        '\t\t\t// Sensitivity fault: bypass the actual native reaction queue.\n'),
}


def mutate(kind, source):
    _, anchor, replacement = MUTATIONS[kind]
    if 'Sensitivity fault:' in source or source.count(anchor) != 1:
        raise ValueError('source-anchor')
    result = source.replace(anchor, replacement)
    if kind == 'wrong-actor':
        include = '#include "../Battlescape/BattlescapeState.h"\n'
        if result.count(include) != 1:
            raise ValueError('include-anchor')
        result = result.replace(include, include + '#include "../Savegame/SavedBattleGame.h"\n')
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
        if not root.is_absolute() or not (root / 'tests/stance-control/StanceControlGate.cpp').is_file():
            raise ValueError('checkout')
        here = Path(__file__).resolve().parents[2]
        if root.resolve() == here:
            raise ValueError('normal-checkout')
        path = root / MUTATIONS[kind][0]
        result = mutate(kind, path.read_text())
        path.write_text(result)
    except (OSError, ValueError, KeyError):
        print('STANCE_RUNTIME_MUTANT INCOMPLETE source-or-arguments', file=sys.stderr)
        return 1
    print('STANCE_RUNTIME_MUTANT APPLIED ' + kind + ' source-sha256=' + sha256(result.encode()).hexdigest())
    return 0


if __name__ == '__main__':
    sys.exit(main())
