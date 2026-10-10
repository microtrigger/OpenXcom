#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Structural tripwires supplement native and subprocess tests, not a safety proof."""
from pathlib import Path
import re

if not __debug__:
    raise SystemExit('FAIL: diagnostic checks require assertions; run without optimization')
root = Path(__file__).resolve().parents[2]
profile = (root / 'src/Observer/DiagnosticProfile.cpp').read_text()
for option in ('debug', 'debugUi', 'traceAI', 'verboseLogging'):
    assert 'Options::' + option in profile
    assert not re.search(r'Options::' + option + r'\s*=(?!=)', profile)
assert 'level < LOG_FATAL || level > LOG_INFO' in profile
assert 'if (!initialized || rejected) return false;' in profile
assert 'if (initialized) return;' in profile
assert profile.count('rejected = false') == 1
assert profile.count('rejected = true') == 1
main = (root / 'src/main.cpp').read_text()
assert main.index('Options::init(argc, argv)') < main.index('Logger::reportingLevel() = LOG_VERBOSE')
assert main.index('Logger::reportingLevel() = LOG_VERBOSE') < main.index('Observer::initializeDiagnosticProfile()') < main.index('game = new Game(')
for filename, count in (('PresentationObserver.cpp', 4), ('MixedAudioObserver.cpp', 3),
                        ('CursorFileObserver.cpp', 3), ('HudFileObserver.cpp', 3)):
    text = (root / 'src/Observer' / filename).read_text()
    assert text.count('diagnosticProfileQualified()') == count, filename
    assert 'DiagnosticProfile.h' in text
    if 'FileObserver' in filename:
        assert 'if (!enabled()) return;' in text
        assert 'if (!diagnosticProfileQualified()) disable();' in text
        assert 'unlink(' not in text # Previously lawful historical snapshot is retained.
launcher = (root / 'tools/mcp-observer/launch_presentation.py').read_text()
assert 'stdout=subprocess.DEVNULL' in launcher and 'stderr=subprocess.DEVNULL' in launcher
for forbidden in ('subprocess.PIPE', 'stdout=sys.stderr', 'str(error)', 'traceback', '.terminate(', '.kill('):
    assert forbidden not in launcher
assert 'class QuietArgumentParser' in launcher
print('PASS: diagnostic qualification order, sticky rejection and bounded launcher isolation')
