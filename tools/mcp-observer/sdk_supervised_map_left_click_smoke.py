# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-12.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Official SDK through the real launcher, with synthetic pixels and receipts.

This verifies transport, not native UI approval, game assets, movement or fairness.
No SDK dependency is required by the server itself.
"""
import json
import os
from pathlib import Path
import secrets
import socket
import sys
import time

import supervised_stance as stance
from sdk_supervised_turn_smoke import fixture_pixels, validate_png


def check(value):
    if not value: raise RuntimeError('Synthetic map-left-click smoke check failed')


def synthetic_producer():
    for fd in (1, 2): os.write(fd, b'PRIVATE-LEFT-CLICK-DIAGNOSTIC\xff\n')
    indices, palette, _ = fixture_pixels()
    check(os.environ.get('OPENXCOM_MAP_LEFT_CLICK_ENABLED') == '1')
    check(os.environ.get('OPENXCOM_TURN_ENABLED') == '1')
    with socket.socket(fileno=int(os.environ['OPENXCOM_STANCE_FD'])) as peer:
        peer.settimeout(15)
        epoch, capability, known, views = secrets.token_hex(16), None, {}, 0
        peer.send(json.dumps({'schema': stance.HELLO_SCHEMA, 'control_epoch': epoch}).encode())
        while True:
            request = peer.recv(4096)
            if not request: return
            action = request[:1]
            if action in (b'V', b'O'):
                check(len(request) == 33)
                capability = secrets.token_hex(16)
                reply = {'schema': stance.TACTICAL_CONTEXT_SCHEMA if action == b'V' else stance.CONTEXT_SCHEMA,
                    'challenge': request[1:].decode(), 'control_epoch': epoch, 'capability': capability,
                    'semantics': 'presented_after_request'}
                if action == b'V':
                    views += 1
                    if views == 2: time.sleep(2.3)
                    reply['view'] = {'width': 320, 'height': 200,
                                     'indices_hex': indices.hex(), 'palette_hex': palette.hex()}
                    if views == 3: reply['view']['busy'] = False
                else:
                    reply['panel'] = {'width': 320, 'height': 56, 'rgb_hex': '0180ff' * 320 * 56}
            elif action in (b'L', b'R', b'D', b'N'):
                check(request[1:33].decode() == epoch and request[33:65].decode() == capability)
                command_id = request[65:97].decode(); check(command_id not in known)
                payload = b'016040' if action in (b'L', b'R') else (b'sk' if action == b'D' else b'p0')
                check(len(request) == 97 + len(payload) and request[97:] == payload)
                known[command_id] = action
                reply = {'schema': stance.STATUS_SCHEMA, 'command_id': command_id, 'status': 'pending_confirmation'}
            elif action == b'S':
                check(len(request) == 65)
                nonce = request[33:65].decode('ascii'); stance.token(nonce)
                check(nonce != '0' * 32)
                command_id = request[1:33].decode(); check(command_id in known)
                reply = {'schema': stance.STATUS_SCHEMA, 'command_id': command_id,
                         'status': 'dispatched' if known[command_id] == b'L' else 'unavailable'}
            else: raise RuntimeError('Unexpected protocol request')
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
        command = [str(Path(__file__).with_name('launch_presentation.py')),
                   '--supervised-stance', '--supervised-turn', '--supervised-selection']
        if enabled: command.append('--supervised-map-left-click')
        command.extend(['--', sys.executable, str(Path(__file__)), '--synthetic-producer'])
        async with stdio_client(StdioServerParameters(command=sys.executable, args=command,
                env=dict(os.environ, OPENXCOM_MAP_LEFT_CLICK_ENABLED='1'))) as (read, write):
            async with ClientSession(read, write, read_timeout_seconds=timedelta(seconds=15)) as client:
                initialized = await client.initialize()
                names = {tool.name for tool in (await client.list_tools()).tools}
                check(('request_map_left_click' in names) == enabled)
                if not enabled: continue
                observed = await client.call_tool('read_tactical_context', {})
                check(not observed.isError and len(observed.content) == 2)
                meta = observed.structuredContent
                check(meta == json.loads(observed.content[0].text))
                check(set(meta) == {'schema', 'challenge', 'control_epoch', 'capability', 'semantics', 'view'})
                check(meta['schema'] == stance.PUBLIC_TACTICAL_CONTEXT_SCHEMA)
                png_bytes = validate_png(observed.content[1].data, rgb)
                args = {name: meta[name] for name in ('control_epoch', 'capability')}
                args.update(command_id=secrets.token_hex(16), x=16, y=40)
                for invalid in (dict(args, approved=True), dict(args, x=16.0), dict(args, y=True),
                                dict(args, targets=[]), dict(args, preflight=True)):
                    try: await client.call_tool('request_map_left_click', invalid)
                    except McpError: pass
                    else: raise RuntimeError('Invalid left-click arguments were accepted')
                pending = await client.call_tool('request_map_left_click', args)
                check(not pending.isError and pending.structuredContent['status'] == 'pending_confirmation')
                check((await client.call_tool('read_tactical_context', {})).isError)
                check((await client.call_tool('request_turn', args)).isError)
                check((await client.call_tool('request_map_left_click', dict(args, x=17))).isError)
                dispatched = await client.call_tool('request_map_left_click', args)
                check(dispatched.structuredContent['status'] == 'dispatched')
                check(dispatched.structuredContent['semantics'] == 'dispatch_status_not_success')
                check((await client.call_tool('read_stance_context', {})).isError)
                fresh = await client.call_tool('read_tactical_context', {})
                check(not fresh.isError and fresh.structuredContent['capability'] != meta['capability'])
                validate_png(fresh.content[1].data, rgb)
                check((await client.call_tool('request_map_left_click', args)).structuredContent['status'] == 'dispatched')
                right = {name: fresh.structuredContent[name] for name in ('control_epoch', 'capability')}
                right.update(command_id=secrets.token_hex(16), x=16, y=40)
                check((await client.call_tool('request_turn', right)).structuredContent['status'] == 'pending_confirmation')
                check((await client.call_tool('read_stance_status', {'command_id': right['command_id']})).structuredContent['status'] == 'unavailable')
                for tool, extras in (('request_selection', {'direction': 'previous'}),
                                     ('request_stance', {'observed': 'stand', 'desired': 'kneel'})):
                    panel = await client.call_tool('read_stance_context', {})
                    check(not panel.isError and panel.structuredContent['schema'] == stance.PUBLIC_CONTEXT_SCHEMA)
                    panel_args = {name: panel.structuredContent[name] for name in ('control_epoch', 'capability')}
                    panel_args.update(command_id=secrets.token_hex(16))
                    check((await client.call_tool('request_map_left_click', dict(panel_args, x=16, y=40))).isError)
                    check((await client.call_tool(tool, dict(panel_args, **extras))).structuredContent['status'] == 'pending_confirmation')
                    check((await client.call_tool('read_stance_status', {'command_id': panel_args['command_id']})).structuredContent['status'] == 'unavailable')
                failed = await client.call_tool('read_tactical_context', {})
                check(failed.isError and failed.structuredContent == {'error': 'unavailable'})
                check((await client.call_tool('request_map_left_click', args)).isError)
                print(json.dumps({'sdk_version': package_version('mcp'), 'protocol': initialized.protocolVersion,
                    'default_disabled': True, 'ambient_activation_scrubbed': True, 'explicit_tool': True,
                    'exact_tactical_png': True, 'png_bytes': png_bytes, 'strict_arguments': True,
                    'pending_not_dispatch': True, 'duplicate_status_only': True, 'cross_kind_conflict_rejected': True,
                    'post_l_tactical_required': True, 'post_l_frame_over_two_seconds_accepted': True,
                    'l_r_o_n_d_interop': True, 'public_schemas_unchanged': True,
                    'extra_busy_field_closes': True, 'native_ui_verified': False}))


if __name__ == '__main__':
    if sys.argv[1:] == ['--synthetic-producer']: synthetic_producer()
    else:
        import asyncio
        asyncio.run(smoke())
