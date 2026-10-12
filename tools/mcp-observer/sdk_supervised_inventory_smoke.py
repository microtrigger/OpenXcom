# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-12.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Official SDK through the real launcher with synthetic inventory pixels.

Transport correctness only. No game UI, native human confirmation, gameplay,
model calls, token benchmark or fairness claim. Server uses no SDK dependency.
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
    if not value: raise RuntimeError('Synthetic inventory smoke check failed')


def synthetic_producer(mode='normal'):
    for fd in (1, 2): os.write(fd, b'PRIVATE-INVENTORY-DIAGNOSTIC\xff\n')
    if os.environ.get('OPENXCOM_INVENTORY_ENABLED') is None:
        # The disabled smoke only lists/calls unavailable tools, never observes.
        return
    check(os.environ['OPENXCOM_INVENTORY_ENABLED'] == '1')
    check(os.environ.get('OPENXCOM_TURN_ENABLED') == '1')
    indices, palette, _ = fixture_pixels()
    with socket.socket(fileno=int(os.environ['OPENXCOM_STANCE_FD'])) as peer:
        peer.settimeout(15)
        epoch, capability, grant, known, counter, views = '1' * 32, None, None, {}, 1, 0
        peer.send(json.dumps(dict(schema=stance.HELLO_SCHEMA, control_epoch=epoch)).encode())
        while True:
            request = peer.recv(4096)
            if not request: return
            action = request[:1]
            if action in (b'O', b'V', b'I'):
                check(len(request) == 33)
                if action == b'I':
                    views += 1; capability = grant = None
                    reply = dict(schema=stance.INVENTORY_VIEW_SCHEMA, challenge=request[1:].decode(),
                                 semantics='presented_after_request')
                else:
                    counter += 1; capability, grant = f'{counter:032x}', action
                    reply = dict(schema=stance.TACTICAL_CONTEXT_SCHEMA if action == b'V' else stance.CONTEXT_SCHEMA,
                        challenge=request[1:].decode(), control_epoch=epoch, capability=capability,
                        semantics='presented_after_request')
                if action == b'O':
                    reply['panel'] = dict(width=320, height=56, rgb_hex='0180ff' * 320 * 56)
                else:
                    reply['view'] = dict(width=320, height=200, indices_hex=indices.hex(), palette_hex=palette.hex())
                if action == b'I':
                    if mode == 'disconnect': return
                    if mode == 'delay': time.sleep(2.3)
                    if mode == 'malformed': reply = {}
                    if mode == 'extra' or (mode == 'smoke' and views == 3): reply['capability'] = 'f' * 32
                    if mode == 'wrong_kind': reply['schema'] = stance.TACTICAL_CONTEXT_SCHEMA
                    if mode == 'replay': reply['challenge'] = 'f' * 32
                    if mode == 'alias':
                        reply['view']['indices_hex'] = '0001' * 32000
                        reply['view']['palette_hex'] = '0180ff0180ff' + '00' * 762
                    if mode == 'unused_palette':
                        reply['view']['indices_hex'] = '00' * 64000
                        reply['view']['palette_hex'] = '0180ffff0001' + '00' * 762
            elif action in (b'B', b'R', b'L', b'D', b'N'):
                check(request[1:33].decode() == epoch and request[33:65].decode() == capability)
                check(grant == (b'V' if action in (b'B', b'R', b'L') else b'O'))
                command_id = request[65:97].decode(); check(command_id not in known)
                payload = (b'o0' if action == b'B' else b'016040' if action in (b'R', b'L')
                           else b'sk' if action == b'D' else b'p0')
                check(len(request) == 97 + len(payload) and request[97:] == payload)
                known[command_id] = action; capability = grant = None
                reply = dict(schema=stance.STATUS_SCHEMA, command_id=command_id, status='pending_confirmation')
            elif action == b'S':
                check(len(request) == 33)
                command_id = request[1:].decode(); check(command_id in known)
                reply = dict(schema=stance.STATUS_SCHEMA, command_id=command_id,
                             status='dispatched' if known[command_id] == b'B' else 'unavailable')
            else: raise RuntimeError('Unexpected protocol request')
            try: peer.send(json.dumps(reply, separators=(',', ':')).encode())
            except BrokenPipeError: return


async def smoke():
    from datetime import timedelta
    from importlib.metadata import version as package_version
    from mcp import ClientSession, StdioServerParameters
    from mcp.client.stdio import stdio_client
    from mcp.shared.exceptions import McpError

    _, _, rgb = fixture_pixels()
    for enabled in (False, True):
        command = [str(Path(__file__).with_name('launch_presentation.py')),
                   '--supervised-stance', '--supervised-turn', '--supervised-selection', '--supervised-map-left-click']
        if enabled: command.append('--supervised-inventory')
        command.extend(['--', sys.executable, str(Path(__file__)), '--synthetic-producer', 'smoke'])
        async with stdio_client(StdioServerParameters(command=sys.executable, args=command,
                env=dict(os.environ, OPENXCOM_INVENTORY_ENABLED='1'))) as (read, write):
            async with ClientSession(read, write, read_timeout_seconds=timedelta(seconds=15)) as client:
                initialized = await client.initialize()
                tools = (await client.list_tools()).tools
                names = {tool.name for tool in tools}
                check(('request_open_inventory' in names) == enabled)
                check(('read_inventory_view' in names) == enabled)
                check(not any('close_inventory' in name for name in names))
                if not enabled:
                    for name in ('request_open_inventory', 'read_inventory_view'):
                        try: await client.call_tool(name, {})
                        except McpError: pass
                        else: raise RuntimeError('Disabled inventory tool was accepted')
                    continue
                tool = next(tool for tool in tools if tool.name == 'request_open_inventory')
                check(set(tool.inputSchema['properties']) == {'capability', 'control_epoch', 'command_id'})
                manual = await client.call_tool('read_inventory_view', {})
                check(not manual.isError)
                validate_png(manual.content[1].data, rgb)
                check(set(manual.structuredContent) == {'schema', 'challenge', 'semantics', 'view'})
                observed = await client.call_tool('read_tactical_context', {})
                check(not observed.isError)
                meta = observed.structuredContent
                args = {name: meta[name] for name in ('control_epoch', 'capability')}
                args['command_id'] = secrets.token_hex(16)
                for invalid in (dict(args, approved=True), dict(args, item_id='private'), dict(args, x=16),
                                dict(args, action='close'), dict(args, control_epoch=True), dict(args, command_id='G' * 32)):
                    try: await client.call_tool('request_open_inventory', invalid)
                    except McpError: pass
                    else: raise RuntimeError('Invalid inventory arguments were accepted')
                pending = await client.call_tool('request_open_inventory', args)
                check(not pending.isError and pending.structuredContent['status'] == 'pending_confirmation')
                for name in ('read_inventory_view', 'read_tactical_context', 'read_stance_context'):
                    check((await client.call_tool(name, {})).isError)
                for name, extras in (('request_turn', {'x':16, 'y':40}),
                                     ('request_map_left_click', {'x':16, 'y':40}),
                                     ('request_stance', {'observed':'stand', 'desired':'kneel'}),
                                     ('request_selection', {'direction':'previous'})):
                    check((await client.call_tool(name, dict(args, **extras))).isError)
                dispatched = await client.call_tool('request_open_inventory', args)
                check(dispatched.structuredContent['status'] == 'dispatched')
                check(dispatched.structuredContent['semantics'] == 'dispatch_status_not_success')
                for name in ('read_tactical_context', 'read_stance_context'):
                    check((await client.call_tool(name, {})).isError)
                check((await client.call_tool('read_stance_status', {'command_id':args['command_id']})).structuredContent['status'] == 'dispatched')
                image = await client.call_tool('read_inventory_view', {})
                check(not image.isError and len(image.content) == 2)
                inventory = image.structuredContent
                check(inventory == json.loads(image.content[0].text))
                check(set(inventory) == {'schema', 'challenge', 'semantics', 'view'})
                check(inventory['schema'] == stance.PUBLIC_INVENTORY_VIEW_SCHEMA)
                check(inventory['semantics'] == 'presented_after_request')
                check(inventory['view'] == {'width':320, 'height':200, 'format':'image/png'})
                png_bytes = validate_png(image.content[1].data, rgb)
                check((await client.call_tool('request_open_inventory', dict(args, command_id=secrets.token_hex(16)))).isError)
                check((await client.call_tool('request_open_inventory', args)).structuredContent['status'] == 'dispatched')
                # Ordinary contexts here are synthetic. This does not automate native inventory close.
                for name, view, extras in (('request_stance', 'read_stance_context', {'observed':'stand', 'desired':'kneel'}),
                                          ('request_selection', 'read_stance_context', {'direction':'previous'}),
                                          ('request_turn', 'read_tactical_context', {'x':16, 'y':40}),
                                          ('request_map_left_click', 'read_tactical_context', {'x':16, 'y':40})):
                    fresh = await client.call_tool(view, {})
                    check(not fresh.isError)
                    current = {key:fresh.structuredContent[key] for key in ('control_epoch', 'capability')}
                    current['command_id'] = secrets.token_hex(16)
                    if view == 'read_stance_context':
                        check((await client.call_tool('request_open_inventory', current)).isError)
                    check((await client.call_tool(name, dict(current, **extras))).structuredContent['status'] == 'pending_confirmation')
                    check((await client.call_tool('read_stance_status', {'command_id':current['command_id']})).structuredContent['status'] == 'unavailable')
                failed = await client.call_tool('read_inventory_view', {})
                check(failed.isError and failed.structuredContent == {'error':'unavailable'})
                check((await client.call_tool('request_open_inventory', args)).isError)
                check((await client.call_tool('read_tactical_context', {})).isError)
                print(json.dumps(dict(sdk_version=package_version('mcp'), protocol=initialized.protocolVersion,
                    default_disabled=True, ambient_activation_scrubbed=True, exact_b99_i33=True,
                    strict_three_token_arguments=True, capless_inventory_envelope=True,
                    inventory_png_exact_rgb=True, png_bytes=png_bytes, manual_inventory_read=True,
                    shared_five_action_ledger=True, duplicate_status_only=True, mandatory_post_b_i=True,
                    fresh_context_required_after_i=True, d_n_r_l_interop=True,
                    extra_inventory_field_closes=True, no_close_tool=True, native_ui_verified=False,
                    model_token_benchmark=False)))


if __name__ == '__main__':
    if len(sys.argv) >= 2 and sys.argv[1] == '--synthetic-producer':
        synthetic_producer(sys.argv[2] if len(sys.argv) == 3 else 'normal')
    else:
        import asyncio
        asyncio.run(smoke())
