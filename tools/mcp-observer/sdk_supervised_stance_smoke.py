# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-11.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Official MCP SDK smoke over a synthetic stance peer and real launcher.

No game, assets, network or genuine human approval are involved. Synthetic
status transitions test protocol handling only, not the native modal UI.
"""
import json
import os
from pathlib import Path
import secrets
import socket
import sys

import supervised_stance as stance

RGB = bytes.fromhex('0180ff') * (stance.WIDTH * stance.HEIGHT)


def synthetic_producer():
    if 'OPENXCOM_STANCE_FD' not in os.environ:
        return
    # Verify that child diagnostics never become MCP observation data.
    for fd in (1, 2):
        os.write(fd, b'PRIVATE-STANCE-DIAGNOSTIC\xff\n')
    with socket.socket(fileno=int(os.environ['OPENXCOM_STANCE_FD'])) as peer:
        peer.settimeout(10)
        epoch = secrets.token_hex(16)
        peer.send(json.dumps({'schema': stance.HELLO_SCHEMA, 'control_epoch': epoch}).encode())
        observations = commands = polls = 0
        while True:
            request = peer.recv(4096)
            if not request:
                return
            if request[:1] == b'O':
                assert len(request) == 33
                observations += 1
                if observations == 3:
                    reply = {'error': 'unavailable'}
                else:
                    reply = {'schema': stance.CONTEXT_SCHEMA, 'challenge': request[1:].decode(),
                             'control_epoch': epoch, 'capability': secrets.token_hex(16),
                             'semantics': 'presented_after_request',
                             'panel': {'width': stance.WIDTH, 'height': stance.HEIGHT,
                                       'rgb_hex': RGB.hex()}}
            elif request[:1] == b'D':
                assert len(request) == 99 and request[1:33].decode() == epoch
                commands += 1
                assert commands <= 2
                reply = {'schema': stance.STATUS_SCHEMA, 'command_id': request[65:97].decode(),
                         'status': 'pending_confirmation'}
            elif request[:1] == b'S':
                assert len(request) == 33
                polls += 1
                value = 'pending_confirmation' if polls == 1 else (
                    'dispatched' if commands == 1 else 'unavailable')
                reply = {'schema': stance.STATUS_SCHEMA, 'command_id': request[1:].decode(), 'status': value}
            else:
                raise AssertionError('Unexpected protocol packet')
            peer.send(json.dumps(reply, separators=(',', ':')).encode())


async def smoke():
    from datetime import timedelta
    from importlib.metadata import version as package_version
    from mcp import ClientSession, StdioServerParameters
    from mcp.client.stdio import stdio_client
    from mcp.shared.exceptions import McpError

    for enabled in (False, True):
        command = [str(Path(__file__).with_name('launch_presentation.py'))]
        if enabled:
            command.append('--supervised-stance')
        command.extend(['--', sys.executable, str(Path(__file__)), '--synthetic-producer'])
        params = StdioServerParameters(command=sys.executable, args=command)
        async with stdio_client(params) as (read, write):
            async with ClientSession(read, write, read_timeout_seconds=timedelta(seconds=5)) as client:
                initialized = await client.initialize()
                listed = {tool.name for tool in (await client.list_tools()).tools}
                names = {'read_stance_context', 'request_stance', 'read_stance_status'}
                assert (names <= listed) if enabled else not (names & listed)
                if not enabled:
                    continue
                observed = await client.call_tool('read_stance_context', {})
                assert not observed.isError and len(observed.content) == 2
                meta = json.loads(observed.content[0].text)
                assert observed.structuredContent == meta
                assert meta['schema'] == stance.PUBLIC_CONTEXT_SCHEMA
                assert meta['semantics'] == 'presented_after_request'
                assert observed.content[1].type == 'image' and observed.content[1].mimeType == 'image/png'
                assert observed.content[1].data == stance.image_content(RGB)['data']
                arguments = {name: meta[name] for name in ('control_epoch', 'capability')}
                arguments.update(command_id=secrets.token_hex(16), observed='stand', desired='kneel')
                try:
                    await client.call_tool('request_stance', dict(arguments, approved=True))
                except McpError:
                    pass
                else:
                    raise AssertionError('Model approval field was not rejected')
                pending = await client.call_tool('request_stance', arguments)
                assert not pending.isError and pending.structuredContent['status'] == 'pending_confirmation'
                pending_status = await client.call_tool('read_stance_status', {'command_id': arguments['command_id']})
                assert pending_status.structuredContent['status'] == 'pending_confirmation'
                blocked = await client.call_tool('read_stance_context', {})
                assert blocked.isError and blocked.structuredContent == {'error': 'unavailable'}
                # Explicit duplicate asks S and does not replay D.
                dispatched = await client.call_tool('request_stance', arguments)
                assert not dispatched.isError and dispatched.structuredContent['status'] == 'dispatched'
                assert dispatched.structuredContent['semantics'] == 'dispatch_status_not_success'
                conflict = await client.call_tool('request_stance', dict(arguments, desired='stand'))
                assert conflict.isError
                premature = await client.call_tool('request_stance', dict(arguments, command_id=secrets.token_hex(16)))
                assert premature.isError
                fresh = await client.call_tool('read_stance_context', {})
                assert not fresh.isError and fresh.structuredContent['capability'] != meta['capability']
                second = {name: fresh.structuredContent[name] for name in ('control_epoch', 'capability')}
                second.update(command_id=secrets.token_hex(16), observed='kneel', desired='stand')
                assert (await client.call_tool('request_stance', second)).structuredContent['status'] == 'pending_confirmation'
                cancelled = await client.call_tool('read_stance_status', {'command_id': second['command_id']})
                assert cancelled.structuredContent['status'] == 'unavailable'
                failed = await client.call_tool('read_stance_context', {})
                assert failed.isError and failed.structuredContent == {'error': 'unavailable'}
                closed = await client.call_tool('request_stance', second)
                assert closed.isError
                print(json.dumps({'sdk_version': package_version('mcp'),
                                  'protocol': initialized.protocolVersion,
                                  'default_disabled': True, 'explicit_tools': True,
                                  'exact_panel_png': True, 'structured_metadata': True,
                                  'model_approval_rejected': True, 'pending_not_dispatch': True,
                                  'duplicate_queries_status_only': True, 'conflicting_id_rejected': True,
                                  'post_dispatch_context_required': True, 'cancellation_terminal': True,
                                  'failed_observation_closes': True, 'native_ui_verified': False}))


if __name__ == '__main__':
    if sys.argv[1:] == ['--synthetic-producer']:
        synthetic_producer()
    else:
        import asyncio
        asyncio.run(smoke())
