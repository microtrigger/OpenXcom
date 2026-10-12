#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
"""Inject one real selection fault into a disposable source copy, never this one.

These mutations change real dispatch/gameplay, not fixtures or gate assertions.
A successful script is setup only; the rebuilt selection runtime must fail.
"""
from hashlib import sha256
from pathlib import Path
import sys

NEXT = "    if (_action == 'n') { _context.state->btnNextSoldierClick(0); return; }\n"
PREVIOUS = "    if (_action == 'p') { _context.state->btnPrevSoldierClick(0); return; }\n"
MUTATIONS = {
    'next-as-previous': ('src/Observer/StanceController.cpp', NEXT,
                         NEXT.replace('btnNextSoldierClick', 'btnPrevSoldierClick')),
    'previous-as-next': ('src/Observer/StanceController.cpp', PREVIOUS,
                         PREVIOUS.replace('btnPrevSoldierClick', 'btnNextSoldierClick')),
    'duplicate-next': ('src/Observer/StanceController.cpp', NEXT,
                       NEXT.replace(' return;', ' _context.state->btnNextSoldierClick(0); return;')),
    'selection-charges-tu': ('src/Battlescape/BattlescapeState.cpp',
                             '\t\tselectNextPlayerUnit(true, false);\n',
                             '\t\t_save->getSelectedUnit()->spendTimeUnits(1);\n'
                             '\t\tselectNextPlayerUnit(true, false);\n'),
}


def mutate(kind, source):
    _, anchor, replacement = MUTATIONS[kind]
    if 'Selection sensitivity fault:' in source or source.count(anchor) != 1:
        raise ValueError('source-anchor')
    return source.replace(anchor, '    // Selection sensitivity fault: ' + kind + '\n' + replacement)


def main(args=None):
    args = sys.argv[1:] if args is None else args
    try:
        if len(args) != 2 or args[0] not in MUTATIONS:
            raise ValueError('arguments')
        kind, root = args[0], Path(args[1])
        if not root.is_absolute() or not (root / 'tests/stance-control/SelectionControlGate.cpp').is_file():
            raise ValueError('checkout')
        if root.resolve() == Path(__file__).resolve().parents[2]:
            raise ValueError('normal-checkout')
        path = root / MUTATIONS[kind][0]
        result = mutate(kind, path.read_text())
        path.write_text(result)
    except (OSError, ValueError, KeyError):
        print('SELECTION_RUNTIME_MUTANT INCOMPLETE source-or-arguments', file=sys.stderr)
        return 1
    print('SELECTION_RUNTIME_MUTANT APPLIED ' + kind + ' source-sha256=' + sha256(result.encode()).hexdigest())
    return 0


if __name__ == '__main__':
    sys.exit(main())
