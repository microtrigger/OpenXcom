#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# This file is free software: you may redistribute and/or modify it under
# GNU GPL version 3 or, at your option, any later version.
# Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
"""Record build inputs without opening game assets, saves or credentials."""
import hashlib
import json
import pathlib
import subprocess
import sys

source, build = (pathlib.Path(x).resolve() for x in sys.argv[1:3])
mode = sys.argv[3]
cache_text = (build / 'CMakeCache.txt').read_text()
effective_mode = next((line.split('=', 1)[1] for line in cache_text.splitlines()
                       if line.startswith('EXPERIMENTAL_HUD_OBSERVER:BOOL=')), 'not_defined')
cursor_mode = next((line.split('=', 1)[1] for line in cache_text.splitlines()
                    if line.startswith('EXPERIMENTAL_CURSOR_OBSERVER:BOOL=')), 'not_defined')
def git(*args):
    return subprocess.check_output(['git', '-C', str(source), *args], text=True).strip()
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
inputs = [source / 'CMakeLists.txt']
for folder in ('src', 'cmake'):
    inputs.extend(p for p in (source / folder).rglob('*') if p.is_file())
manifest = ''.join(f'{sha(p)}  {p.relative_to(source)}\n' for p in sorted(inputs))
(build / 'source-inputs.sha256').write_text(manifest)
result = {
    'git_head': git('rev-parse', 'HEAD'),
    'git_tracked_diff_sha256': hashlib.sha256(subprocess.check_output(
        ['git', '-C', str(source), 'diff', 'HEAD', '--', 'src', 'cmake', 'CMakeLists.txt'])).hexdigest(),
    'source_inputs_sha256': hashlib.sha256(manifest.encode()).hexdigest(),
    'requested_observer_mode': mode,
    'configured_observer_mode': effective_mode,
    'configured_cursor_observer_mode': cursor_mode,
    'binary_sha256': sha(build / 'bin/openxcom'),
    'cmake_cache_sha256': sha(build / 'CMakeCache.txt'),
    'note': 'Source manifest includes untracked source files. No assets or save data are read. '
            'Record compiler, dependency package versions and configure arguments separately.'
}
(build / 'observer-build-provenance.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
