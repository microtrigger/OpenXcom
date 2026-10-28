#!/usr/bin/env python3
"""Run trusted native test recipes with immutable input receipts; not a sandbox."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def exact(value, keys):
    if type(value) is not dict or set(value) != set(keys):
        raise ValueError('Unexpected recipe fields')


def text(value):
    if type(value) is not str or not value or '\0' in value:
        raise ValueError('Expected nonempty text without NUL')
    return value


def load_recipe(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError('Duplicate JSON key')
            result[key] = value
        return result
    with Path(path).open(encoding='utf-8') as stream:
        return json.load(stream, object_pairs_hook=unique,
                         parse_constant=lambda _: (_ for _ in ()).throw(ValueError('Nonfinite JSON number')))


def prepare(recipe):
    exact(recipe, ('version', 'source', 'output', 'inputs', 'steps'))
    if type(recipe['version']) is not int or recipe['version'] != 1:
        raise ValueError('Unsupported recipe version')
    source = Path(text(recipe['source'])).resolve(strict=True)
    output = Path(text(recipe['output'])).resolve()
    if not source.is_dir() or output == source or source in output.parents:
        raise ValueError('Output must be outside the source tree')
    if output.exists():
        raise ValueError('Output already exists')
    if type(recipe['inputs']) is not list or not recipe['inputs']:
        raise ValueError('Explicit nonempty inputs required')
    inputs = []
    for raw in recipe['inputs']:
        path = Path(text(raw)).resolve(strict=True)
        if not path.is_file() or path in inputs or output == path or output in path.parents:
            raise ValueError('Input must be a unique existing file outside output')
        inputs.append(path)
    if type(recipe['steps']) is not list or not recipe['steps']:
        raise ValueError('Explicit nonempty steps required')
    steps = []
    names = set()
    for step in recipe['steps']:
        exact(step, ('name', 'argv', 'cwd', 'env'))
        name = text(step['name'])
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}', name) or name in names:
            raise ValueError('Step names must be unique safe identifiers')
        names.add(name)
        if type(step['argv']) is not list or not step['argv']:
            raise ValueError('argv must be a nonempty array')
        argv = [text(arg) for arg in step['argv']]
        cwd = Path(text(step['cwd'])).resolve()
        if cwd != output and not cwd.is_dir():
            raise ValueError('cwd must exist or equal the output directory')
        if type(step['env']) is not dict:
            raise ValueError('env must be an explicit overlay object')
        env = {}
        for key, value in step['env'].items():
            text(key)
            if '=' in key or type(value) is not str or '\0' in value:
                raise ValueError('Invalid environment overlay')
            env[key] = value
        steps.append((name, argv, cwd, env))
    return source, output, inputs, steps


def run_recipe(recipe):
    """Return receipt. Paths are relative to caller cwd; no interpolation or shell.

    argv may reference the explicit output path. Source is a protected location
    only for runner output placement; trusted subprocesses are NOT sandboxed.
    """
    source, output, inputs, steps = prepare(recipe)
    before = {str(path): digest(path) for path in inputs}
    output.mkdir(parents=True, exist_ok=False)
    receipt = {'version': 1, 'all_passed': False, 'inputs_unchanged': False,
               'input_sha256_before': before, 'input_sha256_after': {}, 'steps': []}
    def save():
        temporary = output / 'RESULT.json.tmp'
        temporary.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
        temporary.replace(output / 'RESULT.json')
    save()
    try:
        for name, argv, cwd, overlay in steps:
            result = {'name': name, 'log': name + '.log', 'exit_code': None,
                      'launch_error': None}
            receipt['steps'].append(result)
            env = os.environ.copy()
            env.update(overlay)
            try:
                with (output / result['log']).open('wb') as log:
                    completed = subprocess.run(argv, cwd=cwd, env=env, shell=False,
                                               stdin=subprocess.DEVNULL, stdout=log,
                                               stderr=subprocess.STDOUT, check=False)
                result['exit_code'] = completed.returncode
            except OSError as error:
                result['launch_error'] = type(error).__name__
            save()
            if result['exit_code'] != 0:
                break
    finally:
        after = {}
        for path in inputs:
            try:
                after[str(path)] = digest(path)
            except OSError:
                after[str(path)] = None
        receipt['input_sha256_after'] = after
        receipt['inputs_unchanged'] = before == after
        receipt['all_passed'] = (receipt['inputs_unchanged'] and
                                 len(receipt['steps']) == len(steps) and
                                 all(step['exit_code'] == 0 for step in receipt['steps']))
        save()
    return receipt


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('recipe', type=Path)
    args = parser.parse_args(argv)
    try:
        result = run_recipe(load_recipe(args.recipe))
    except (ValueError, OSError) as error:
        print('Runner refused recipe: ' + type(error).__name__, file=sys.stderr)
        return 2
    return 0 if result['all_passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
