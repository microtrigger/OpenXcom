# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-12.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Official MCP SDK and real-launcher smoke using only synthetic tactical pixels.

No game, assets, network, genuine approval, native UI or gameplay is tested here.
Synthetic pending/dispatched receipts verify transport and adapter semantics only.
"""
import base64
import json
import os
from pathlib import Path
import random
import secrets
import socket
import struct
import sys
import zlib

import supervised_stance as stance


def fixture_pixels():
    rng = random.Random(17)
    colors = [rng.randbytes(3) for _ in range(256)]
    assert len(set(colors)) == 256
    indices = bytes(range(256)) + rng.randbytes(64000 - 256)
    return indices, b''.join(colors), b''.join(colors[index] for index in indices)


def validate_png(data, rgb):
    """Check the exact PNG subset independently from the adapter encoder."""
    png = base64.b64decode(data, validate=True)
    assert png[:8] == b'\x89PNG\r\n\x1a\n'
    offset, chunks = 8, []
    while offset < len(png):
        size, = struct.unpack('>I', png[offset:offset + 4])
        kind, payload = png[offset + 4:offset + 8], png[offset + 8:offset + 8 + size]
        crc, = struct.unpack('>I', png[offset + 8 + size:offset + 12 + size])
        assert crc == zlib.crc32(kind + payload) & 0xffffffff
        chunks.append((kind, payload)); offset += size + 12
    assert offset == len(png)
    assert [kind for kind, _ in chunks] == [b'IHDR', b'PLTE', b'IDAT', b'IEND']
    assert struct.unpack('>IIBBBBB', chunks[0][1]) == (320, 200, 8, 3, 0, 0, 0)
    palette, rows = chunks[1][1], zlib.decompress(chunks[2][1])
    assert len(palette) == 768 and len(rows) == 64200 and chunks[3][1] == b''
    assert all(rows[y * 321] == 0 for y in range(200))
    indices = b''.join(rows[y * 321 + 1:(y + 1) * 321] for y in range(200))
    assert b''.join(palette[i * 3:i * 3 + 3] for i in indices) == rgb
    return len(png)


def synthetic_producer():
    for fd in (1, 2): os.write(fd, b'PRIVATE-TURN-DIAGNOSTIC\xff\n')
    if 'OPENXCOM_STANCE_FD' not in os.environ: return
    indices, palette, _ = fixture_pixels()
    with socket.socket(fileno=int(os.environ['OPENXCOM_STANCE_FD'])) as peer:
        peer.settimeout(10)
        epoch = secrets.token_hex(16)
        peer.send(json.dumps({'schema': stance.HELLO_SCHEMA, 'control_epoch': epoch}).encode())
        observations = commands = polls = 0
        active, known, capability = None, {}, None
        while True:
            request = peer.recv(4096)
            if not request: return
            action = request[:1]
            if action in (b'V', b'O'):
                assert len(request) == 33
                if action == b'V': observations += 1
                capability = secrets.token_hex(16)
                reply = {'schema': stance.TACTICAL_CONTEXT_SCHEMA if action == b'V' else stance.CONTEXT_SCHEMA,
                    'challenge': request[1:].decode(), 'control_epoch': epoch, 'capability': capability,
                    'semantics': 'presented_after_request'}
                if action == b'V':
                    assert os.environ['OPENXCOM_TURN_ENABLED'] == '1'
                    reply['view'] = {'width': 320, 'height': 200,
                                     'indices_hex': indices.hex(), 'palette_hex': palette.hex()}
                    if observations == 3:
                        # Same RGB under duplicate indices is noncanonical and must close the adapter.
                        reply['view']['indices_hex'] = '0001' * 32000
                        reply['view']['palette_hex'] = '0180ff0180ff' + '00' * 762
                else:
                    reply['panel'] = {'width': 320, 'height': 56, 'rgb_hex': '0180ff' * 320 * 56}
            elif action in (b'R', b'D'):
                assert request[1:33].decode() == epoch and request[33:65].decode() == capability
                command_id = request[65:97].decode()
                assert command_id not in known
                if action == b'R': assert len(request) == 103 and request[97:] == b'016040'
                else: assert len(request) == 99 and request[97:] == b'sk'
                commands += 1; active = command_id; known[command_id] = 'pending_confirmation'
                reply = {'schema': stance.STATUS_SCHEMA, 'command_id': command_id, 'status': known[command_id]}
            elif action == b'S':
                if len(request) != 65: raise RuntimeError('Invalid status query')
                nonce = request[33:65].decode('ascii'); stance.token(nonce)
                if nonce == '0' * 32: raise RuntimeError('Invalid status query')
                command_id = request[1:33].decode(); assert command_id in known
                polls += 1
                if known[command_id] == 'pending_confirmation' and polls > 1:
                    known[command_id] = 'dispatched' if commands == 1 else 'unavailable'
                reply = {'schema': stance.STATUS_SCHEMA, 'command_id': command_id, 'status': known[command_id]}
            else: raise AssertionError('Unexpected protocol request')
            if reply.get('schema') == stance.STATUS_SCHEMA:
                reply['query_nonce'] = nonce if request[:1] == b'S' else None
            peer.send(json.dumps(reply, separators=(',', ':')).encode())


async def smoke():
    from datetime import timedelta
    from importlib.metadata import version as package_version
    from mcp import ClientSession, StdioServerParameters
    from mcp.client.stdio import stdio_client
    from mcp.shared.exceptions import McpError

    _, _, rgb = fixture_pixels()
    for enabled in (False, True):
        command = [str(Path(__file__).with_name('launch_presentation.py')), '--supervised-stance']
        if enabled: command.append('--supervised-turn')
        command.extend(['--', sys.executable, str(Path(__file__)), '--synthetic-producer'])
        async with stdio_client(StdioServerParameters(command=sys.executable, args=command)) as (read, write):
            async with ClientSession(read, write, read_timeout_seconds=timedelta(seconds=5)) as client:
                initialized = await client.initialize()
                names = {tool.name for tool in (await client.list_tools()).tools}
                assert ({'read_tactical_context', 'request_turn'} <= names) if enabled else not (
                    {'read_tactical_context', 'request_turn'} & names)
                if not enabled: continue
                observed = await client.call_tool('read_tactical_context', {})
                assert not observed.isError and len(observed.content) == 2
                meta = json.loads(observed.content[0].text)
                assert observed.structuredContent == meta and meta['schema'] == stance.PUBLIC_TACTICAL_CONTEXT_SCHEMA
                assert set(meta) == {'schema', 'challenge', 'control_epoch', 'capability', 'semantics', 'view'}
                assert meta['view'] == {'width': 320, 'height': 200, 'format': 'image/png'}
                png_bytes = validate_png(observed.content[1].data, rgb)
                args = {name: meta[name] for name in ('control_epoch', 'capability')}
                args.update(command_id=secrets.token_hex(16), x=16, y=40)
                for invalid in (dict(args, approved=True), dict(args, x=16.0), dict(args, y=True)):
                    try: await client.call_tool('request_turn', invalid)
                    except McpError: pass
                    else: raise AssertionError('Invalid turn arguments were accepted')
                wrong_kind = {name: args[name] for name in ('control_epoch', 'capability', 'command_id')}
                wrong_kind.update(observed='stand', desired='kneel')
                assert (await client.call_tool('request_stance', wrong_kind)).isError
                pending = await client.call_tool('request_turn', args)
                assert not pending.isError and pending.structuredContent['status'] == 'pending_confirmation'
                assert (await client.call_tool('read_tactical_context', {})).isError
                duplicate = await client.call_tool('request_turn', args)
                assert duplicate.structuredContent['status'] == 'pending_confirmation'
                dispatched = await client.call_tool('read_stance_status', {'command_id': args['command_id']})
                assert dispatched.structuredContent['status'] == 'dispatched'
                assert dispatched.structuredContent['semantics'] == 'dispatch_status_not_success'
                assert (await client.call_tool('read_stance_context', {})).isError
                assert (await client.call_tool('request_turn', dict(args, x=17))).isError
                fresh = await client.call_tool('read_tactical_context', {})
                assert not fresh.isError and fresh.structuredContent['capability'] != meta['capability']
                validate_png(fresh.content[1].data, rgb)
                assert (await client.call_tool('request_turn', args)).structuredContent['status'] == 'dispatched'
                second = {name: fresh.structuredContent[name] for name in ('control_epoch', 'capability')}
                second.update(command_id=secrets.token_hex(16), x=16, y=40)
                assert (await client.call_tool('request_turn', second)).structuredContent['status'] == 'pending_confirmation'
                assert (await client.call_tool('read_stance_status', {'command_id': second['command_id']})).structuredContent['status'] == 'unavailable'
                panel = await client.call_tool('read_stance_context', {})
                assert not panel.isError and panel.structuredContent['schema'] == stance.PUBLIC_CONTEXT_SCHEMA
                stance_args = {name: panel.structuredContent[name] for name in ('control_epoch', 'capability')}
                stance_args.update(command_id=secrets.token_hex(16), observed='stand', desired='kneel')
                assert (await client.call_tool('request_stance', stance_args)).structuredContent['status'] == 'pending_confirmation'
                assert (await client.call_tool('read_stance_status', {'command_id': stance_args['command_id']})).structuredContent['status'] == 'unavailable'
                failed = await client.call_tool('read_tactical_context', {})
                assert failed.isError and failed.structuredContent == {'error': 'unavailable'}
                assert (await client.call_tool('request_turn', second)).isError
                print(json.dumps({'sdk_version': package_version('mcp'), 'protocol': initialized.protocolVersion,
                    'default_disabled': True, 'explicit_tools': True, 'exact_tactical_png': True,
                    'png_bytes': png_bytes, 'strict_arguments': True, 'type_bound_capabilities': True,
                    'pending_not_dispatch': True, 'duplicate_status_only': True, 'conflicting_id_rejected': True,
                    'post_dispatch_tactical_required': True, 'stance_schema_unchanged': True,
                    'cancellation_terminal': True, 'noncanonical_observation_closes': True,
                    'native_ui_verified': False}))


if __name__ == '__main__':
    if sys.argv[1:] == ['--synthetic-producer']: synthetic_producer()
    else:
        import asyncio
        asyncio.run(smoke())
