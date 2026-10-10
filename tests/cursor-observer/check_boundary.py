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
names = ('PresentedCursor', 'PresentedCursorSurface', 'CursorFileObserver', 'PresentationObserver', 'PresentedHud')
allowed_headers = {
    'DiagnosticProfile.h', 'PresentedHud.h', 'array', 'PresentedCursor.h', 'PresentedCursorSurface.h', 'CursorFileObserver.h', 'PresentationObserver.h',
    'SDL.h', 'cstdint', 'string', 'vector', 'memory', 'algorithm', 'locale',
    'sstream', 'stdexcept', 'cerrno', 'cstdio', 'fcntl.h', 'sys/file.h',
    'sys/stat.h', 'unistd.h', 'sys/socket.h', 'cstdlib', 'climits',
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
assert 'if (_presentationObserver) _presentationObserver->close();' in flip
assert flip.index('_presentationObserver->close();') < flip.index('throw Exception(SDL_GetError());')
assert flip.index('SDL_Flip(_screen)') < flip.index('capturePresentedCursor(')
hook = flip[flip.index('if ((_cursorObserver && _cursorObserver->enabled())'):]
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
assert re.search(r'_cursor->blit\(_screen->getSurface\(\)\);\s*#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER[\s\S]*?const bool cursorPresented = _cursor->getVisible\(\);\s*_screen->flip\(cursorPresented \? _cursor->getX\(\) : -1, cursorPresented \? _cursor->getY\(\) : -1\);', game)
cmake = (root / 'CMakeLists.txt').read_text()
assert re.search(r'option\s*\(\s*EXPERIMENTAL_CURSOR_OBSERVER\s+"[^"]*"\s+OFF\s*\)', cmake)
print('PASS: cursor source dependencies, final-pixel hook order and conservative gates (structural guard only)')

# The fresh request is consumed before composition, never discovered after flip.
assert game.index('_screen->beginObserverFrame();') < game.index('_screen->clear();')
assert game.count('_screen->beginObserverFrame();') == 1
assert 'beginFrame(' not in flip and 'recv(' not in flip
assert flip.index('SDL_Flip(_screen)') < flip.index('capturePresentedCursor(') < flip.index('_presentationObserver->presented(crop,')
producer = (observer / 'PresentationObserver.cpp').read_text()
post = producer[producer.index('void PresentationObserver::presented('):]
assert 'recv(' not in post and 'beginFrame(' not in post
assert '_challenge.clear();' in post and 'if (!validCrop(crop)) { close(); return; }' in post
assert 'MSG_DONTWAIT | MSG_NOSIGNAL' in producer
assert 'char bytes[97]' in producer and 'count != 96' in producer
assert 'randomToken(_admission)' in producer and 'SOCK_SEQPACKET' in producer
assert 'public_revision' not in producer and 'SDL_GetTicks' not in producer
assert screen.index('if (_presentationObserver) _presentationObserver->resetPresentation();', screen.index('void Screen::resetDisplay(')) > 0
assert 'if (pending()) close();' in producer
print('PASS: precomposition admission, post-presentation acknowledgement and bounded transport gates')

# HUD proof uses final pixels and UI provenance only, never hidden numeric values.
hud = (observer / 'PresentedHud.cpp').read_text()
number = (root / 'src/Interface/NumberText.cpp').read_text()
blit = number[number.index('void NumberText::blit('):number.index('void NumberText::setBordered(')]
assert '_value' not in blit and 'getValue' not in blit
for gate in ('!_visible', '_hidden', '_redraw', '_bordered', '_crop.w', '_crop.h'):
    assert gate in blit
assert blit.index('Surface::blit(surface)') < blit.index('if (!_presentedDigits)')
assert 'value' not in (observer / 'PresentedHud.h').read_text().split('struct HudDigits')[1].split('};')[0]
assert flip.index('SDL_Flip(_screen)') < flip.index('provePresentedHud(')
assert '_presentedHud = Observer::HudFrame();' in screen
battle = (root / 'src/Battlescape/BattlescapeState.cpp').read_text()
proof = battle[battle.index('void BattlescapeState::blit()'):battle.index('/** Copy only')]
for token in forbidden:
    if token != 'Battlescape': assert token not in proof, token
assert '!_game->isState(this)' in proof and 'screen->wantsPresentedHud()' in proof
assert '{ _numTimeUnits, _numEnergy, _numHealth, _numMorale }' in proof
assert proof.index('armPresentedDigits(&frame[i])') < proof.rindex('State::blit();') < proof.rindex('armPresentedDigits(0)')
# Keep the decoder alphabet tied to actual upstream NumberText pixels.
glyph_literals = re.findall(r'"([01]{15})"', hud)
assert len(glyph_literals) == 10
for n, expected in enumerate(glyph_literals):
    glyph = ['0'] * 15
    for x, y in re.findall(r'_chars\[' + str(n) + r'\]->setPixel\((\d), (\d), 1\)', number):
        glyph[int(y) * 3 + int(x)] = '1'
    assert ''.join(glyph) == expected
assert game.count('excludePresentedHud(') == 2
print('PASS: HUD UI-only source boundary, actual blits, final pixels and exact glyph alphabet')
