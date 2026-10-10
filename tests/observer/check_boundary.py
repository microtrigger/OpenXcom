#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# This file is free software: you may redistribute and/or modify it under
# GNU GPL version 3 or, at your option, any later version.
# Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
"""Structural guard, not an engine-level noninterference proof."""
from pathlib import Path
source = (Path(__file__).resolve().parents[2] / 'src/Battlescape/BattlescapeState.cpp').read_text()
start = source.index('void BattlescapeState::samplePublicHud()')
end = source.index('\n#endif', start)
boundary = source[start:end]
for forbidden in ('getUnits()', 'getTiles()', 'getId()', 'getHealth()', 'getTimeUnits()',
                  'getEnergy()', 'getMorale()', 'getVisibleUnits()', 'getVisible()',
                  'calculateFOV', 'RNG::', 'getOriginalFaction()', 'getName(', 'getTurn()'):
    # getVisible is allowed ONLY for the four display widgets, checked below.
    if forbidden == 'getVisible()':
        continue
    assert forbidden not in boundary, f'Forbidden source in public boundary: {forbidden}'
allowed_reads = {'timeUnits': '_numTimeUnits', 'energy': '_numEnergy',
                 'health': '_numHealth', 'morale': '_numMorale'}
for field, widget in allowed_reads.items():
    assert f'hud.{field} = {widget}->getValue();' in boundary
    assert f'{widget}->getVisible()' in boundary
for gate in ('_game->isState(this)', '_gameTimer->isRunning()', '_firstInit', '_popups.empty()',
             '_save->getSide() != FACTION_PLAYER', '_save->getDebugMode()', '_battleGame->isBusy()',
             '_battleGame->getPanicHandled()', '_save->getUnitsFalling()',
             '_save->getFallingUnits()->empty()', '_map->getProjectile()',
             '_map->getExplosions()->empty()', 'selected->getFaction() == FACTION_PLAYER',
             '!selected->isOut()'):
    assert gate in boundary, f'Missing gate: {gate}'
print('PASS: structural allowlist and stable-capture guard (not runtime proof)')
