# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-12.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Official SDK through the real launcher with synthetic inventory Unload pixels.

Transport correctness only. No game UI, native consent, ordinary inventory
unload effect, gameplay, model calls, token benchmark or fairness claim.
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
    if not value: raise RuntimeError('Synthetic inventory Unload smoke check failed')


def synthetic_producer(mode='normal'):
    for fd in (1, 2): os.write(fd, b'PRIVATE-INVENTORY-UNLOAD-DIAGNOSTIC\xff\n')
    if os.environ.get('OPENXCOM_INVENTORY_UNLOAD_ENABLED') is None:
        # Disabled sessions list/call absent tools only; never observe.
        return
    check(os.environ['OPENXCOM_INVENTORY_UNLOAD_ENABLED'] == '1')
    check(os.environ.get('OPENXCOM_INVENTORY_CLOSE_ENABLED') == '1')
    check(os.environ.get('OPENXCOM_INVENTORY_ENABLED') == '1')
    check(os.environ.get('OPENXCOM_TURN_ENABLED') == '1')
    indices, palette, _ = fixture_pixels()
    with socket.socket(fileno=int(os.environ['OPENXCOM_STANCE_FD'])) as peer:
        peer.settimeout(15)
        epoch, capability, grant, known, counter, unloads = '1' * 32, None, None, {}, 1, 0
        peer.send(json.dumps(dict(schema=stance.HELLO_SCHEMA, control_epoch=epoch)).encode())
        while True:
            request = peer.recv(4096)
            if not request: return
            action = request[:1]
            if action in (b'O', b'V', b'I', b'J', b'K', b'H', b'U', b'Q'):
                check(len(request) == 33)
                if action == b'I':
                    capability = grant = None
                    reply = dict(schema=stance.INVENTORY_VIEW_SCHEMA, challenge=request[1:].decode(),
                                 semantics='presented_after_request')
                else:
                    counter += 1; capability, grant = f'{counter:032x}', action
                    schema = {b'O':stance.CONTEXT_SCHEMA, b'V':stance.TACTICAL_CONTEXT_SCHEMA,
                              b'J':stance.INVENTORY_CLOSE_CONTEXT_SCHEMA, b'K':stance.INVENTORY_GROUND_CONTEXT_SCHEMA,
                              b'H':stance.INVENTORY_CLICK_CONTEXT_SCHEMA, b'U':stance.INVENTORY_RIGHT_CLICK_CONTEXT_SCHEMA,
                              b'Q':stance.INVENTORY_UNLOAD_CONTEXT_SCHEMA}[action]
                    reply = dict(schema=schema, challenge=request[1:].decode(), control_epoch=epoch,
                                 capability=capability, semantics='presented_after_request')
                if action == b'O':
                    reply['panel'] = dict(width=320, height=56, rgb_hex='0180ff' * 320 * 56)
                else:
                    reply['view'] = dict(width=320, height=200, indices_hex=indices.hex(), palette_hex=palette.hex())
                if action == b'Q':
                    if mode == 'disconnect': return
                    if mode == 'delay': time.sleep(2.3)
                    if mode == 'malformed': reply = {}
                    if mode == 'extra': reply['items'] = []
                    if mode == 'wrong_kind': reply['schema'] = stance.TACTICAL_CONTEXT_SCHEMA
                    if mode == 'close_kind': reply['schema'] = stance.INVENTORY_CLOSE_CONTEXT_SCHEMA
                    if mode == 'capless':
                        reply['schema'] = stance.INVENTORY_VIEW_SCHEMA
                        del reply['capability']; del reply['control_epoch']
                    if mode == 'replay': reply['challenge'] = 'f' * 32
                    if mode == 'alias':
                        reply['view']['indices_hex'] = '0001' * 32000
                        reply['view']['palette_hex'] = '0180ff0180ff' + '00' * 762
                    if mode == 'unused_palette':
                        reply['view']['indices_hex'] = '00' * 64000
                        reply['view']['palette_hex'] = '0180ffff0001' + '00' * 762
                if action == b'I' and unloads and mode == 'bad_post_i': reply['items'] = []
            elif action in (b'B', b'C', b'R', b'L', b'D', b'N', b'G', b'E', b'X', b'F'):
                check(request[1:33].decode() == epoch and request[33:65].decode() == capability)
                check(grant == (b'Q' if action == b'F' else b'K' if action == b'G' else b'H' if action == b'E' else b'U' if action == b'X' else b'J' if action == b'C' else b'V' if action in (b'B', b'R', b'L') else b'O'))
                command_id = request[65:97].decode(); check(command_id not in known)
                payload = (b'' if action in (b'C', b'G', b'F') else b'o0' if action == b'B' else b'016040' if action in (b'R', b'L')
                           else b'016080' if action in (b'E', b'X') else b'sk' if action == b'D' else b'p0')
                check(len(request) == 97 + len(payload) and request[97:] == payload)
                known[command_id] = action; capability = grant = None
                if action == b'F': unloads += 1
                reply = dict(schema=stance.STATUS_SCHEMA, command_id=command_id, status='pending_confirmation')
            elif action == b'S':
                check(len(request) == 65)
                nonce = request[33:65].decode('ascii'); stance.token(nonce)
                check(nonce != '0' * 32)
                command_id = request[1:33].decode(); check(command_id in known)
                reply = dict(schema=stance.STATUS_SCHEMA, command_id=command_id,
                    status='dispatched' if known[command_id] in (b'B', b'C', b'F') and mode != 'cancel' else 'unavailable')
            else: raise RuntimeError('Unexpected protocol request')
            if reply.get('schema') == stance.STATUS_SCHEMA:
                reply['query_nonce'] = nonce if request[:1] == b'S' else None
            try: peer.send(json.dumps(reply, separators=(',', ':')).encode())
            except BrokenPipeError: return


async def smoke():
    from datetime import timedelta
    from importlib.metadata import version as package_version
    from mcp import ClientSession, StdioServerParameters
    from mcp.client.stdio import stdio_client
    from mcp.shared.exceptions import McpError

    _, _, rgb = fixture_pixels()
    interpreter_flags = ['-O'] if sys.flags.optimize else []
    for enabled, mode in ((False, 'normal'), (True, 'normal'), (True, 'cancel'), (True, 'bad_post_i')):
        command = [*interpreter_flags, str(Path(__file__).with_name('launch_presentation.py')),
                   '--supervised-stance', '--supervised-turn', '--supervised-selection',
                   '--supervised-map-left-click', '--supervised-inventory', '--supervised-inventory-close',
                   '--supervised-inventory-ground', '--supervised-inventory-click', '--supervised-inventory-right-click']
        if enabled: command.append('--supervised-inventory-unload')
        command.extend(['--', sys.executable, *interpreter_flags, str(Path(__file__)), '--synthetic-producer', mode])
        async with stdio_client(StdioServerParameters(command=sys.executable, args=command,
                env=dict(os.environ, OPENXCOM_INVENTORY_UNLOAD_ENABLED='1'))) as (read, write):
            async with ClientSession(read, write, read_timeout_seconds=timedelta(seconds=15)) as client:
                initialized = await client.initialize()
                tools = (await client.list_tools()).tools; names = {tool.name for tool in tools}
                check(('request_inventory_unload' in names) == enabled)
                check(('read_inventory_unload_context' in names) == enabled)
                check({'read_inventory_view', 'request_open_inventory', 'request_close_inventory', 'read_inventory_close_context'} <= names)
                if not enabled:
                    for name in ('request_inventory_unload', 'read_inventory_unload_context'):
                        try: await client.call_tool(name, {})
                        except McpError: pass
                        else: raise RuntimeError('Disabled Unload tool was accepted')
                    continue
                tool = next(tool for tool in tools if tool.name == 'request_inventory_unload')
                check(set(tool.inputSchema['properties']) == {'capability', 'control_epoch', 'command_id'})
                check(not tool.annotations.readOnlyHint and tool.annotations.destructiveHint)
                check('ordinary Unload button attempt' in tool.description and 'every dispatched attempt' in tool.description)
                async def observe(name):
                    result = await client.call_tool(name, {}); check(not result.isError)
                    meta = result.structuredContent
                    check(meta == json.loads(result.content[0].text))
                    if name != 'read_stance_context': validate_png(result.content[1].data, rgb)
                    return meta
                def arguments(meta):
                    return dict(capability=meta['capability'], control_epoch=meta['control_epoch'],
                                command_id=secrets.token_hex(16))
                if mode == 'normal':
                    tactical = await observe('read_tactical_context'); opened = arguments(tactical)
                    pending = await client.call_tool('request_open_inventory', opened)
                    check(pending.structuredContent['status'] == 'pending_confirmation')
                    check((await client.call_tool('read_stance_status', {'command_id':opened['command_id']})).structuredContent['status'] == 'dispatched')
                    for name in ('read_inventory_unload_context', 'read_inventory_close_context'):
                        check((await client.call_tool(name, {})).isError)
                    capless = await observe('read_inventory_view')
                    check(set(capless) == {'schema', 'challenge', 'semantics', 'view'})
                    check(capless['schema'] == stance.PUBLIC_INVENTORY_VIEW_SCHEMA)
                    check((await client.call_tool('request_inventory_unload', arguments(tactical))).isError)
                    # J still authorizes only close; neither J nor I can authorize Unload.
                    closed = arguments(await observe('read_inventory_close_context'))
                    check((await client.call_tool('request_inventory_unload', closed)).isError)
                meta = await observe('read_inventory_unload_context')
                check(set(meta) == {'schema', 'challenge', 'control_epoch', 'capability', 'semantics', 'view'})
                check(meta['schema'] == stance.PUBLIC_INVENTORY_UNLOAD_CONTEXT_SCHEMA)
                check(meta['semantics'] == 'presented_after_request')
                check(meta['view'] == {'width':320, 'height':200, 'format':'image/png'})
                args = arguments(meta)
                for invalid in (dict(args, approved=True), dict(args, item_id='private'), dict(args, x=16),
                                dict(args, action='unload'), dict(args, ammo=1), dict(args, held=True), dict(args, eligibility=True),
                                dict(args, button='left'), dict(args, modifiers=[]), dict(args, outcome='unloaded'), dict(args, preflight=True),
                                dict(args, control_epoch=True), dict(args, command_id='F' * 32)):
                    try: await client.call_tool('request_inventory_unload', invalid)
                    except McpError: pass
                    else: raise RuntimeError('Invalid Unload arguments were accepted')
                for name, extras in (('request_open_inventory', {}), ('request_close_inventory', {}),
                                     ('request_turn', {'x':16, 'y':40}), ('request_map_left_click', {'x':16, 'y':40}),
                                     ('request_stance', {'observed':'stand', 'desired':'kneel'}),
                                     ('request_selection', {'direction':'previous'}), ('request_inventory_ground', {}),
                                     ('request_inventory_click', {'x':16, 'y':80}),
                                     ('request_inventory_right_click', {'x':16, 'y':80})):
                    check((await client.call_tool(name, dict(args, **extras))).isError)
                pending = await client.call_tool('request_inventory_unload', args)
                check(not pending.isError and pending.structuredContent['status'] == 'pending_confirmation')
                for name in ('read_inventory_unload_context', 'read_inventory_close_context', 'read_inventory_view',
                             'read_tactical_context', 'read_stance_context'):
                    check((await client.call_tool(name, {})).isError)
                terminal = await client.call_tool('request_inventory_unload', args)
                expected = 'unavailable' if mode == 'cancel' else 'dispatched'
                check(terminal.structuredContent['status'] == expected)
                check(terminal.structuredContent['semantics'] == 'dispatch_status_not_success')
                check((await client.call_tool('read_stance_status', {'command_id':args['command_id']})).structuredContent['status'] == expected)
                if mode == 'cancel':
                    check((await client.call_tool('request_inventory_unload', dict(args, command_id=secrets.token_hex(16)))).isError)
                    fresh = await observe('read_inventory_unload_context'); retry = arguments(fresh)
                    check((await client.call_tool('request_inventory_unload', retry)).structuredContent['status'] == 'pending_confirmation')
                    check((await client.call_tool('read_stance_status', {'command_id':retry['command_id']})).structuredContent['status'] == 'unavailable')
                    continue
                for name in ('read_inventory_unload_context', 'read_inventory_close_context', 'read_tactical_context', 'read_stance_context'):
                    check((await client.call_tool(name, {})).isError)
                verified = await client.call_tool('read_inventory_view', {})
                if mode == 'bad_post_i':
                    check(verified.isError and verified.structuredContent == {'error':'unavailable'})
                    for name, params in (('request_inventory_unload', args), ('read_tactical_context', {}),
                                         ('read_inventory_view', {}), ('read_inventory_unload_context', {}),
                                         ('read_inventory_close_context', {})):
                        check((await client.call_tool(name, params)).isError)
                    continue
                check(not verified.isError); validate_png(verified.content[1].data, rgb)
                check(set(verified.structuredContent) == {'schema', 'challenge', 'semantics', 'view'})
                check((await client.call_tool('request_inventory_unload', args)).structuredContent['status'] == 'dispatched')
                # Duplicate F must not rearm I. A fresh J can now grant C, which still requires V.
                current = arguments(await observe('read_inventory_close_context'))
                check((await client.call_tool('request_inventory_unload', current)).isError)
                check((await client.call_tool('request_close_inventory', current)).structuredContent['status'] == 'pending_confirmation')
                check((await client.call_tool('read_stance_status', {'command_id':current['command_id']})).structuredContent['status'] == 'dispatched')
                for name in ('read_inventory_unload_context', 'read_inventory_close_context', 'read_inventory_view'):
                    check((await client.call_tool(name, {})).isError)
                await observe('read_tactical_context')
                # Synthetic transport progression only, never evidence of native gameplay.
                for name, view, extras in (('request_stance', 'read_stance_context', {'observed':'stand', 'desired':'kneel'}),
                                          ('request_selection', 'read_stance_context', {'direction':'previous'}),
                                          ('request_turn', 'read_tactical_context', {'x':16, 'y':40}),
                                          ('request_map_left_click', 'read_tactical_context', {'x':16, 'y':40}),
                                          ('request_inventory_ground', 'read_inventory_ground_context', {}),
                                          ('request_inventory_click', 'read_inventory_click_context', {'x':16, 'y':80}),
                                          ('request_inventory_right_click', 'read_inventory_right_click_context', {'x':16, 'y':80})):
                    current = arguments(await observe(view))
                    check((await client.call_tool('request_inventory_unload', current)).isError)
                    check((await client.call_tool(name, dict(current, **extras))).structuredContent['status'] == 'pending_confirmation')
                    check((await client.call_tool('read_stance_status', {'command_id':current['command_id']})).structuredContent['status'] == 'unavailable')
    print(json.dumps(dict(sdk_version=package_version('mcp'), protocol=initialized.protocolVersion,
        optimized_launcher=bool(sys.flags.optimize),
        default_disabled=True, existing_tools_preserved=True, ambient_activation_scrubbed=True,
        exact_q33_f97=True, strict_three_token_arguments=True, unload_context_exact_envelope=True,
        unload_png_exact_rgb=True, capless_i_unchanged=True, shared_ten_action_ledger=True,
        cross_context_matrix=True, duplicate_status_only=True, mandatory_post_b_i=True,
        mandatory_post_c_v=True, mandatory_post_f_i=True, cancelled_unload_requires_fresh_context=True,
        failed_post_f_i_terminal=True, d_n_r_l_b_c_g_e_x_interop=True,
        native_ui_verified=False, native_unload_verified=False, model_token_benchmark=False)))


if __name__ == '__main__':
    if len(sys.argv) >= 2 and sys.argv[1] == '--synthetic-producer':
        synthetic_producer(sys.argv[2] if len(sys.argv) == 3 else 'normal')
    else:
        import asyncio
        asyncio.run(smoke())
