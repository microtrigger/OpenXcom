#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# This file is free software: you may redistribute and/or modify it under
# GNU GPL version 3 or, at your option, any later version.
# Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
"""Conservative source-change tripwire; not runtime presentation/noninterference proof."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
observer = root / 'src/Observer'
names = ('PresentedCursor', 'PresentedCursorSurface', 'CursorFileObserver')
allowed_headers = {
    'PresentedCursor.h', 'PresentedCursorSurface.h', 'CursorFileObserver.h',
    'SDL.h', 'cstdint', 'string', 'vector', 'memory', 'algorithm', 'locale',
    'sstream', 'stdexcept', 'cerrno', 'cstdio', 'fcntl.h', 'sys/file.h',
    'sys/stat.h', 'unistd.h',
}
forbidden = (
    'BattleUnit', 'BattleItem', 'BattleState', 'BattleSave', 'SavedBattleGame',
    'SavedGame', 'TileEngine', 'MapData', 'Battlescape', 'Geoscape', 'Position',
    'getUnits', 'getTiles', 'getTile', 'getId', 'getHealth', 'getTimeUnits',
    'getEnergy', 'getMorale', 'getVisibleUnits', 'getFaction', 'getName',
    'getTurn', 'getSelected', 'getSave', 'getBattle', 'getMap', 'calculateFOV',
    'RNG::', 'SDL_GetTicks', 'SDL_GetMouseState', 'SDL_PollEvent', 'SDL_PushEvent',
    'gettimeofday', 'clock_gettime', 'system(', 'popen(', 'socket(',
)

def audit(text, label):
    for token in forbidden:
        assert token not in text, f'{label}: forbidden simulation/input/time source: {token}'

for name in names:
    for suffix in ('.h', '.cpp'):
        path = observer / (name + suffix)
        text = path.read_text()
        includes = re.findall(r'^\s*#include\s*[<"]([^>"]+)[>"]', text, re.M)
        assert set(includes) <= allowed_headers, f'{path.name}: unreviewed dependency {set(includes) - allowed_headers}'
        audit(text, path.name)

surface = (observer / 'PresentedCursorSurface.cpp').read_text()
for gate in ('!allowed', '!screen', '!screen->format', '!screen->pixels',
             'SDL_OPENGL', 'SDL_HWSURFACE', 'SDL_DOUBLEBUF', 'SDL_ASYNCBLIT',
             'BitsPerPixel != 8', 'BytesPerPixel != 1', 'ncolors != 256',
             'screen->pitch < screen->w', 'screen->w / bw != screen->h / bh',
             'screen->w / bw > 6', 'SDL_LockSurface(screen)', 'SDL_UnlockSurface(screen)'):
    assert gate in surface, f'Adapter missing profile guard: {gate}'
assert surface.index('crop.rgb.resize(') < surface.index('SDL_LockSurface(screen)'), 'Allocate before acquiring SDL surface lock'
assert 'screen->format->palette->colors[row[x]]' in surface
assert all(f'crop.rgb[at++] = color.{channel}' in surface for channel in 'rgb')

screen = (root / 'src/Engine/Screen.cpp').read_text()
start = screen.index('void Screen::flip(int cursorX, int cursorY)')
end = screen.index('void Screen::clear()', start)
flip = screen[start:end]
audit(flip, 'Screen::flip')
assert flip.index('SDL_BlitSurface(') < flip.index('SDL_Flip(_screen)')
assert flip.index('SDL_SetColors(') < flip.index('SDL_Flip(_screen)')
assert re.search(r'if\s*\(SDL_Flip\(_screen\) == -1\)\s*\{\s*throw Exception\(SDL_GetError\(\)\);\s*\}', flip)
assert flip.index('SDL_Flip(_screen)') < flip.index('capturePresentedCursor(')
hook = flip[flip.index('if (_cursorObserver && _cursorObserver->enabled())'):]
audit(hook, 'post-flip boundary')
for gate in ('const bool allowed = focused &&', 'SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS',
             '(SDL_GetAppState() & requiredFocus) == requiredFocus', '!useOpenGL()', '!use32bitScaler()', '!Options::debug', '_observerPaletteExact',
             '_topBlackBand == 0', '_bottomBlackBand == 0', '_leftBlackBand == 0', '_rightBlackBand == 0'):
    assert gate in hook, f'Native hook missing fail-closed gate: {gate}'
assert re.search(r'capturePresentedCursor\(_screen, _baseWidth, _baseHeight,\s*cursorX, cursorY, allowed\)', hook)
assert screen.count('capturePresentedCursor(') == 1
assert 'std::getenv("OPENXCOM_EXPERIMENTAL_CURSOR_DIR")' in screen
header = (root / 'src/Engine/Screen.h').read_text()
assert re.search(r'void flip\(int cursorX = -1, int cursorY = -1\)', header), 'Other flip callers must default unavailable'

game = (root / 'src/Engine/Game.cpp').read_text()
assert re.search(r'_cursor->blit\(_screen->getSurface\(\)\);\s*#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER\s*const bool cursorPresented = _cursor->getVisible\(\);\s*_screen->flip\(cursorPresented \? _cursor->getX\(\) : -1, cursorPresented \? _cursor->getY\(\) : -1\);', game)
cmake = (root / 'CMakeLists.txt').read_text()
assert re.search(r'option\s*\(\s*EXPERIMENTAL_CURSOR_OBSERVER\s+"[^"]*"\s+OFF\s*\)', cmake)
print('PASS: cursor source dependencies, final-pixel hook order and conservative gates (structural guard only)')
