#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# This file is free software: you may redistribute and/or modify it under
# GNU GPL version 3 or, at your option, any later version.
# Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
"""Local stdio protocol scaffold. Current producer cannot attest freshness."""
import json
import sys

PROTOCOL = '2025-11-25'
MAX_MESSAGE = 16384
MAX_RESPONSE = 131072
UNAVAILABLE = {'content': [{'type': 'text', 'text':
    '{"available":false,"freshness":"unverified","reason":"unsupported_freshness"}'}],
    'isError': True}
NAMES = ('read_last_hud', 'read_last_cursor')


def strict_json(raw):
    def pairs(items):
        result = {}
        for k, v in items:
            if k in result:
                raise ValueError('duplicate key')
            result[k] = v
        return result
    def reject(_):
        raise ValueError('nonfinite number')
    return json.loads(raw, object_pairs_hook=pairs, parse_constant=reject)


def error(ident, code, message):
    return {'jsonrpc': '2.0', 'id': ident, 'error': {'code': code, 'message': message}}


def definitions():
    return [{'name': name,
             'description': 'Read-only historical public observation, never current-state authority. HUD and cursor sessions are independent. require_current always fails closed.',
             'inputSchema': {'type': 'object', 'properties': {'require_current': {'type': 'boolean', 'default': False}}, 'additionalProperties': False},
             'annotations': {'readOnlyHint': True, 'destructiveHint': False,
                             'idempotentHint': True, 'openWorldHint': False}}
            for name in NAMES]


class Server:
    def __init__(self, hud_dir=None, cursor_dir=None):
        self.state = 'new'
        self.paths = {'hud': hud_dir, 'cursor': cursor_dir}

    def handle(self, msg):
        if not isinstance(msg, dict):
            return error(None, -32600, 'Invalid Request')
        ident = msg.get('id')
        request = 'id' in msg
        if (msg.get('jsonrpc') != '2.0' or not isinstance(msg.get('method'), str)
                or set(msg) - {'jsonrpc', 'id', 'method', 'params'}
                or (request and (type(ident) not in (int, str) or
                    (type(ident) is int and abs(ident) > 9007199254740991)))
                or ('params' in msg and not isinstance(msg['params'], dict))):
            return error(None, -32600, 'Invalid Request') if request or not isinstance(msg.get('method'), str) else None
        method, params = msg['method'], msg.get('params', {})
        if not request:
            if method == 'notifications/initialized' and self.state == 'initializing' and not params:
                self.state = 'ready'
            return None
        if method == 'ping':
            result = {}
        elif method == 'initialize':
            ci = params.get('clientInfo')
            if (self.state != 'new' or not isinstance(params.get('protocolVersion'), str)
                    or not isinstance(params.get('capabilities'), dict)
                    or not isinstance(ci, dict) or not isinstance(ci.get('name'), str)
                    or not isinstance(ci.get('version'), str)):
                return error(ident, -32602, 'Invalid initialization')
            self.state = 'initializing'
            result = {'protocolVersion': PROTOCOL, 'capabilities': {'tools': {}},
                      'serverInfo': {'name': 'openxcom-readonly-scaffold', 'version': '0.1.0'}}
        elif self.state != 'ready':
            return error(ident, -32000, 'Not initialized')
        elif method == 'tools/list':
            if params:
                return error(ident, -32602, 'Invalid params')
            result = {'tools': definitions()}
        elif method == 'tools/call':
            if (set(params) - {'name', 'arguments', '_meta'} or params.get('name') not in NAMES
                    or type(params.get('arguments', {})) is not dict
                    or set(params.get('arguments', {})) - {'require_current'}
                    or type(params.get('arguments', {}).get('require_current', False)) is not bool):
                return error(ident, -32602, 'Invalid params')
            if params.get('arguments', {}).get('require_current', False):
                result = UNAVAILABLE
            else:
                result = self.historical('hud' if params['name'] == NAMES[0] else 'cursor')
        else:
            return error(ident, -32601, 'Method not found')
        return {'jsonrpc': '2.0', 'id': ident, 'result': result}


    def historical(self, kind):
        from snapshots import Unavailable, read_candidate, image_content
        # Fixed filenames only; paths are trusted launch configuration, not MCP input.
        try:
            obj = read_candidate(self.paths[kind], kind)
            if kind == 'cursor' and obj['crop'] is not None:
                image = image_content(obj)
                obj['crop'] = {k: v for k, v in obj['crop'].items() if k not in ('pixels', 'format')}
                obj['crop']['format'] = 'image/png'
                obj['source_schema'] = obj['schema']
                obj['schema'] = 'openxcom-mcp-last-cursor/0'
                content = [{'type': 'text', 'text': json.dumps(obj, separators=(',', ':'))}, image]
            else:
                if kind == 'cursor':
                    obj['source_schema'] = obj['schema']
                    obj['schema'] = 'openxcom-mcp-last-cursor/0'
                content = [{'type': 'text', 'text': json.dumps(obj, separators=(',', ':'))}]
            return {'content': content, 'isError': False}
        except Unavailable:
            return {'content': [{'type': 'text', 'text':
                '{"available":false,"freshness":"unverified","reason":"observation_unavailable"}'}], 'isError': True}


def serve(source, sink, hud_dir=None, cursor_dir=None):
    server = Server(hud_dir, cursor_dir)
    while True:
        raw = source.readline(MAX_MESSAGE + 1)
        if not raw:
            return
        if len(raw) > MAX_MESSAGE:
            # Close instead of buffering/draining an attacker-controlled oversized line.
            reply = error(None, -32600, 'Message too large')
            sink.write(json.dumps(reply, separators=(',', ':')).encode() + b'\n')
            sink.flush()
            return
        try:
            msg = strict_json(raw.decode('utf-8'))
            reply = server.handle(msg)
        except (ValueError, UnicodeError, RecursionError):
            reply = error(None, -32700, 'Parse error')
        if reply is not None:
            out = json.dumps(reply, separators=(',', ':'), ensure_ascii=True).encode() + b'\n'
            if len(out) > MAX_RESPONSE:
                raise RuntimeError('response bound exceeded')
            sink.write(out)
            sink.flush()


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hud-dir')
    parser.add_argument('--cursor-dir')
    config = parser.parse_args()
    try:
        serve(sys.stdin.buffer, sys.stdout.buffer, config.hud_dir, config.cursor_dir)
    except BrokenPipeError:
        pass
