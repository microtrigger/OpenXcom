# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-12.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Official SDK and real launcher with synthetic inventory RIGHT-click transport.

No game UI, native consent, ordinary held cancel/close, gameplay, model calls,
assets, token benchmark or fairness claim. Synthetic destinations are fixtures,
never public protocol fields and never observations of native behavior.
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
    if not value: raise RuntimeError('Synthetic inventory RIGHT smoke check failed')


def synthetic_producer(mode='held'):
    for fd in (1, 2): os.write(fd, b'PRIVATE-INVENTORY-RIGHT-DIAGNOSTIC\xff\n')
    if os.environ.get('OPENXCOM_INVENTORY_RIGHT_CLICK_ENABLED') is None: return
    for name in ('RIGHT_CLICK', 'CLOSE', ''):
        key = 'OPENXCOM_INVENTORY' + ('_' + name if name else '') + '_ENABLED'
        check(os.environ.get(key) == '1')
    check(os.environ.get('OPENXCOM_TURN_ENABLED') == '1')
    indices, palette, _ = fixture_pixels()
    destination = b'V' if mode in ('unheld', 'bad_post_v', 'wrong_i') else b'I'
    with socket.socket(fileno=int(os.environ['OPENXCOM_STANCE_FD'])) as peer:
        peer.settimeout(15)
        epoch, capability, grant, known, counter, required = '1' * 32, None, None, {}, 1, None
        peer.send(json.dumps(dict(schema=stance.HELLO_SCHEMA, control_epoch=epoch)).encode())
        while True:
            request = peer.recv(4096)
            if not request: return
            action = request[:1]
            if action in (b'O', b'V', b'I', b'J', b'K', b'H', b'U'):
                check(len(request) == 33)
                if required and action != required:
                    peer.send(b'{"error":"unavailable"}'); continue
                post_right = bool(required and required == destination)
                required = None
                if action == b'I':
                    capability = grant = None
                    reply = dict(schema=stance.INVENTORY_VIEW_SCHEMA, challenge=request[1:].decode(),
                                 semantics='presented_after_request')
                else:
                    counter += 1; capability, grant = f'{counter:032x}', action
                    schema = {b'O':stance.CONTEXT_SCHEMA, b'V':stance.TACTICAL_CONTEXT_SCHEMA,
                              b'J':stance.INVENTORY_CLOSE_CONTEXT_SCHEMA, b'K':stance.INVENTORY_GROUND_CONTEXT_SCHEMA,
                              b'H':stance.INVENTORY_CLICK_CONTEXT_SCHEMA, b'U':stance.INVENTORY_RIGHT_CLICK_CONTEXT_SCHEMA}[action]
                    reply = dict(schema=schema, challenge=request[1:].decode(), control_epoch=epoch,
                                 capability=capability, semantics='presented_after_request')
                if action == b'O': reply['panel'] = dict(width=320, height=56, rgb_hex='0180ff' * 320 * 56)
                else: reply['view'] = dict(width=320, height=200, indices_hex=indices.hex(), palette_hex=palette.hex())
                if action == b'U':
                    if mode == 'disconnect': return
                    if mode == 'delay': time.sleep(2.3)
                    if mode == 'malformed': reply = {}
                    if mode in ('held_field', 'outcome', 'next_view'): reply['held' if mode == 'held_field' else mode] = 'PRIVATE-SENTINEL'
                    if mode in ('tactical_kind', 'close_kind', 'ground_kind', 'left_kind'):
                        reply['schema'] = {'tactical_kind':stance.TACTICAL_CONTEXT_SCHEMA, 'close_kind':stance.INVENTORY_CLOSE_CONTEXT_SCHEMA,
                            'ground_kind':stance.INVENTORY_GROUND_CONTEXT_SCHEMA, 'left_kind':stance.INVENTORY_CLICK_CONTEXT_SCHEMA}[mode]
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
                if post_right and mode in ('bad_post_i', 'bad_post_v'): reply['view']['outcome'] = 'PRIVATE-SENTINEL'
            elif action in (b'B', b'C', b'R', b'L', b'D', b'N', b'G', b'E', b'X'):
                check(required is None)
                check(request[1:33].decode() == epoch and request[33:65].decode() == capability)
                check(grant == (b'U' if action == b'X' else b'H' if action == b'E' else b'K' if action == b'G' else b'J' if action == b'C'
                                else b'V' if action in (b'B', b'R', b'L') else b'O'))
                command_id = request[65:97].decode(); check(command_id not in known)
                payload = (b'' if action in (b'C', b'G') else b'016080' if action in (b'E', b'X') else b'o0' if action == b'B'
                           else b'016040' if action in (b'R', b'L') else b'sk' if action == b'D' else b'p0')
                check(len(request) == 97 + len(payload) and request[97:] == payload)
                known[command_id] = [action, False]; capability = grant = None
                reply = dict(schema=stance.STATUS_SCHEMA, command_id=command_id, status='pending_confirmation')
            elif action == b'S':
                check(len(request) == 33)
                command_id = request[1:].decode(); check(command_id in known)
                kind, terminal = known[command_id]
                status = 'unavailable' if mode == 'cancel' or kind in (b'D', b'N') else 'dispatched'
                if not terminal and status == 'dispatched':
                    required = destination if kind == b'X' else b'I' if kind in (b'B', b'G', b'E') else b'V'
                known[command_id][1] = True
                reply = dict(schema=stance.STATUS_SCHEMA, command_id=command_id, status=status)
            else: raise RuntimeError('Unexpected protocol request')
            try: peer.send(json.dumps(reply, separators=(',', ':')).encode())
            except BrokenPipeError: return


async def smoke():
    from datetime import timedelta
    from importlib.metadata import version as package_version
    from itertools import product
    from jsonschema import Draft202012Validator
    from mcp import ClientSession, StdioServerParameters
    from mcp.client.stdio import stdio_client
    from mcp.shared.exceptions import McpError

    _, _, rgb = fixture_pixels()
    scenarios = [(False, left, ground, 'held') for left, ground in product((False, True), repeat=2)]
    scenarios += [(True, left, ground, mode) for left, ground, mode in product((False, True), (False, True), ('held', 'unheld'))]
    scenarios += [(True, True, True, mode) for mode in ('cancel', 'bad_post_i', 'bad_post_v', 'wrong_i', 'wrong_v')]
    for enabled, left, ground, mode in scenarios:
        python_flags = ['-O'] if sys.flags.optimize else []
        command = [*python_flags, str(Path(__file__).with_name('launch_presentation.py')),
                   '--supervised-stance', '--supervised-turn', '--supervised-selection',
                   '--supervised-map-left-click', '--supervised-inventory', '--supervised-inventory-close']
        if enabled: command.append('--supervised-inventory-right-click')
        if left: command.append('--supervised-inventory-click')
        if ground: command.append('--supervised-inventory-ground')
        command.extend(['--', sys.executable, *python_flags, str(Path(__file__)), '--synthetic-producer', mode])
        async with stdio_client(StdioServerParameters(command=sys.executable, args=command,
                env=dict(os.environ, OPENXCOM_INVENTORY_RIGHT_CLICK_ENABLED='1'))) as (read, write):
            async with ClientSession(read, write, read_timeout_seconds=timedelta(seconds=15)) as client:
                initialized = await client.initialize(); tools = (await client.list_tools()).tools
                names = {tool.name for tool in tools}
                check(('request_inventory_right_click' in names) == enabled)
                check(('read_inventory_right_click_context' in names) == enabled)
                check(('request_inventory_click' in names) == left)
                check(('request_inventory_ground' in names) == ground)
                if not enabled:
                    for name in ('request_inventory_right_click', 'read_inventory_right_click_context'):
                        try: await client.call_tool(name, {})
                        except McpError: pass
                        else: raise RuntimeError('Disabled RIGHT tool was accepted')
                    continue
                tool = next(tool for tool in tools if tool.name == 'request_inventory_right_click')
                check(set(tool.inputSchema['properties']) == {'capability', 'control_epoch', 'command_id', 'x', 'y'})
                check(not tool.annotations.readOnlyHint and tool.annotations.destructiveHint)
                check('Never probe one view and fall back' in tool.description)
                async def observe(name):
                    result = await client.call_tool(name, {}); check(not result.isError)
                    check(result.structuredContent == json.loads(result.content[0].text))
                    validate_png(result.content[1].data, rgb)
                    return result.structuredContent
                def args(meta):
                    return dict(capability=meta['capability'], control_epoch=meta['control_epoch'], command_id=secrets.token_hex(16))
                meta = await observe('read_inventory_right_click_context')
                check(set(meta) == {'schema', 'challenge', 'control_epoch', 'capability', 'semantics', 'view'})
                check(meta['schema'] == stance.PUBLIC_INVENTORY_RIGHT_CLICK_CONTEXT_SCHEMA)
                request = dict(args(meta), x=16, y=80)
                schema = Draft202012Validator(tool.inputSchema)
                for x, y in product((-1, 0, 31, 32, 127, 128, 159, 160, 319, 320), (0, 62, 63, 110, 111, 151, 152, 199, 200)):
                    allowed = ((0 <= x < 32 or 128 <= x < 160) and 63 <= y < 111) or (0 <= x < 320 and 152 <= y < 200)
                    check(schema.is_valid(dict(request, x=x, y=y)) == allowed)
                for invalid in (dict(request, approved=True), dict(request, held=True), dict(request, next_view='I'),
                                dict(request, outcome='cancel'), dict(request, x=32), dict(request, x=True), dict(request, y=80.0),
                                dict(request, button='right'), dict(request, modifiers=[]), dict(request, item_id='private')):
                    try: await client.call_tool('request_inventory_right_click', invalid)
                    except McpError: pass
                    else: raise RuntimeError('Invalid RIGHT arguments were accepted')
                other_tools = [('request_close_inventory', args(meta))]
                if left: other_tools.append(('request_inventory_click', dict(args(meta), x=16, y=80)))
                if ground: other_tools.append(('request_inventory_ground', args(meta)))
                for name, other in other_tools: check((await client.call_tool(name, other)).isError)
                pending = await client.call_tool('request_inventory_right_click', request)
                check(pending.structuredContent['status'] == 'pending_confirmation')
                observations = ['read_stance_context', 'read_tactical_context', 'read_inventory_view',
                                'read_inventory_close_context', 'read_inventory_right_click_context']
                if left: observations.append('read_inventory_click_context')
                if ground: observations.append('read_inventory_ground_context')
                for name in observations: check((await client.call_tool(name, {})).isError)
                for changed in (dict(request, x=17), dict(request, y=81)):
                    check((await client.call_tool('request_inventory_right_click', changed)).isError)
                terminal = await client.call_tool('request_inventory_right_click', request)
                expected = 'unavailable' if mode == 'cancel' else 'dispatched'
                check(terminal.structuredContent == dict(schema=stance.PUBLIC_STATUS_SCHEMA, command_id=request['command_id'],
                    status=expected, semantics='dispatch_status_not_success'))
                if mode == 'cancel':
                    await observe('read_inventory_right_click_context'); continue
                for name in observations:
                    if name not in ('read_inventory_view', 'read_tactical_context'): check((await client.call_tool(name, {})).isError)
                check((await client.call_tool('read_stance_status', {'command_id':request['command_id']})).structuredContent == terminal.structuredContent)
                chosen = 'read_tactical_context' if mode in ('unheld', 'bad_post_v', 'wrong_v') else 'read_inventory_view'
                verified = await client.call_tool(chosen, {})
                if mode in ('bad_post_i', 'bad_post_v', 'wrong_i', 'wrong_v'):
                    check(verified.isError and verified.structuredContent == {'error':'unavailable'})
                    for name in observations: check((await client.call_tool(name, {})).isError)
                    check((await client.call_tool('request_inventory_right_click', request)).isError)
                    continue
                check(not verified.isError); validate_png(verified.content[1].data, rgb)
                check(('capability' in verified.structuredContent) == (chosen == 'read_tactical_context'))
                check((await client.call_tool('request_inventory_right_click', request)).structuredContent == terminal.structuredContent)
                # Fresh independent authority and separate synthetic receipt.
                second = dict(args(await observe('read_inventory_right_click_context')), x=16, y=80)
                check((await client.call_tool('request_inventory_right_click', request)).structuredContent == terminal.structuredContent)
                check((await client.call_tool('request_inventory_right_click', second)).structuredContent['status'] == 'pending_confirmation')
                check((await client.call_tool('read_stance_status', {'command_id':second['command_id']})).structuredContent['status'] == 'dispatched')
                await observe(chosen)
                # Prior LEFT/Ground/CLOSE remain independently typed and verified.
                for name, context, extras, verification in (
                    ('request_inventory_click', 'read_inventory_click_context', {'x':16, 'y':80}, 'read_inventory_view'),
                    ('request_inventory_ground', 'read_inventory_ground_context', {}, 'read_inventory_view'),
                    ('request_close_inventory', 'read_inventory_close_context', {}, 'read_tactical_context')):
                    if name not in names: continue
                    current = dict(args(await observe(context)), **extras)
                    check((await client.call_tool('request_inventory_right_click', dict(current, x=16, y=80))).isError)
                    check((await client.call_tool(name, current)).structuredContent['status'] == 'pending_confirmation')
                    check((await client.call_tool('read_stance_status', {'command_id':current['command_id']})).structuredContent['status'] == 'dispatched')
                    check((await client.call_tool('request_inventory_right_click', request)).structuredContent == terminal.structuredContent)
                    check((await client.call_tool('read_inventory_right_click_context', {})).isError)
                    await observe(verification)
    print(json.dumps(dict(sdk_version=package_version('mcp'), protocol=initialized.protocolVersion,
        scenarios=len(scenarios), default_disabled=True, independent_click_ground_flags=True,
        ambient_activation_scrubbed=True, exact_u33_x103=True, strict_five_field_arguments=True,
        right_context_exact_envelope=True, right_png_exact_rgb=True, receipt_has_no_branch=True,
        mandatory_explicit_i_or_v=True, wrong_choice_terminal_no_fallback=True,
        malformed_verification_terminal=True, duplicate_status_only_no_rearm=True,
        prior_click_ground_close_interop=True, optimized=bool(sys.flags.optimize),
        native_ui_verified=False, native_held_cancel_or_close_verified=False, model_token_benchmark=False)))


if __name__ == '__main__':
    if len(sys.argv) >= 2 and sys.argv[1] == '--synthetic-producer':
        synthetic_producer(sys.argv[2] if len(sys.argv) == 3 else 'held')
    else:
        import asyncio
        asyncio.run(smoke())
