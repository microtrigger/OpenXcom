#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md.
"""Apply the one reviewed real-source delay to a separate local source checkout.

Never apply this to the normal build's checkout. No sample data or GUI is needed.
The hash and exact anchors reject a different/already-mutated source rather than
quietly constructing a different sensitivity control.
"""
from hashlib import sha256
from pathlib import Path
import sys

SOURCE_SHA256 = '0dfb11b19a3a4253e9b7b775df1f126617d4b5bed433fcbe72241f1ece155461'
INCLUDE = '#include "PresentationObserver.h"\n'
SEND = '    if (send(_fd, bytes.data(), bytes.size(), MSG_DONTWAIT | MSG_NOSIGNAL) != static_cast<ssize_t>(bytes.size()))\n'
ADDITION = '''    // Timing-control mutant, GPL-3.0-or-later, 2026-10-11.
    // This dereferences the actual current-hover Tile, never a trial label.
#ifdef OPENXCOM_EXPERIMENTAL_TIMING_CALIBRATION
    if (RenderPairGate::timingInvisibleHoverOccupant()) SDL_Delay(5);
#endif
'''


def mutate(source):
    if sha256(source.encode()).hexdigest() != SOURCE_SHA256:
        raise ValueError('source-mismatch')
    if source.count(INCLUDE) != 1 or source.count(SEND) != 1:
        raise ValueError('anchor-mismatch')
    return source.replace(INCLUDE, INCLUDE + '''#ifdef OPENXCOM_EXPERIMENTAL_TIMING_CALIBRATION
#include "../../tests/battle-observer/RenderPairGate.h"
#endif
''').replace(SEND, ADDITION + SEND)


def main():
    try:
        if len(sys.argv) != 2:
            raise ValueError('arguments')
        root = Path(sys.argv[1])
        if not root.is_absolute() or not (root / 'tests/battle-observer/TimingCalibrationGate.cpp').is_file():
            raise ValueError('source-checkout')
        path = root / 'src/Observer/PresentationObserver.cpp'
        result = mutate(path.read_text())
        path.write_text(result)
    except (OSError, ValueError):
        print('TIMING_MUTANT INCOMPLETE source-or-arguments', file=sys.stderr)
        return 1
    print('TIMING_MUTANT APPLIED ' + sha256(result.encode()).hexdigest())
    return 0


if __name__ == '__main__':
    sys.exit(main())
