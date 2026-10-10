# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Optional official SDK integration for synthetic /1 and /2 presentation peers.
No game, assets, network, durable endpoint or current-world claim is involved.
"""
import json
import os
import secrets
import socket
import sys
from pathlib import Path

from snapshots import PROFILE, encode_png

CROP = {'screen_width': 320, 'screen_height': 200, 'x': 0, 'y': 0,
        'width': 1, 'height': 1, 'format': 'rgb24-hex-row-major', 'pixels': '010203'}
HUD = {'time_units': 0, 'energy': 999, 'health': 33, 'morale': 100}


def synthetic_producer():
    version = int(os.environ.get('OPENXCOM_PRESENTATION_VERSION', '1'))
    assert version in (1, 2)
    epoch, admission = secrets.token_hex(16), secrets.token_hex(16)
    with socket.socket(fileno=int(os.environ['OPENXCOM_PRESENTATION_FD'])) as peer:
        peer.settimeout(5)
        peer.send((f'OPENXCOM-PRESENTATION/{version} ' + epoch + admission).encode())
        for index in range(4 if version == 2 else 1):
            request = peer.recv(97).decode('ascii')
            assert len(request) == 96 and request[:64] == epoch + admission
            admission = secrets.token_hex(16)
            reply = {'schema': f'experimental-presentation/{version}', 'epoch': epoch,
                     'challenge': request[64:], 'next_token': admission,
                     'semantics': 'presented_after_request', 'profile': PROFILE, 'crop': CROP}
            if version == 2:
                reply['hud'] = None if index == 1 else dict(HUD)
                if index == 3:
                    reply['hud']['health'] = 1000
            peer.send(json.dumps(reply, separators=(',', ':')).encode())


async def smoke():
    import asyncio
    import base64
    from datetime import timedelta
    from importlib.metadata import version as package_version
    from mcp import ClientSession, StdioServerParameters
    from mcp.client.stdio import stdio_client

    for version in (1, 2):
        command = [str(Path(__file__).with_name('launch_presentation.py'))]
        if version == 2:
            command.append('--presented-hud')
        command.extend(['--', sys.executable, str(Path(__file__)), '--synthetic-producer'])
        params = StdioServerParameters(command=sys.executable, args=command)
        async with stdio_client(params) as (read, write):
            async with ClientSession(read, write, read_timeout_seconds=timedelta(seconds=5)) as client:
                initialized = await client.initialize()
                listed = {tool.name for tool in (await client.list_tools()).tools}
                assert listed == {'read_last_hud', 'read_last_cursor', 'read_presented_cursor'} | (
                    {'read_presented_hud'} if version == 2 else set())
                denied = await client.call_tool('read_last_hud', {'require_current': True})
                assert denied.isError
                if version == 2:
                    displayed = await client.call_tool('read_presented_hud', {})
                    assert not displayed.isError and len(displayed.content) == 1
                    obj = json.loads(displayed.content[0].text)
                    assert obj['schema'] == 'openxcom-mcp-presented-hud/1' and obj['hud'] == HUD
                    assert obj['semantics'] == 'presented_after_request'
                    assert set(obj) == {'schema', 'epoch', 'challenge', 'semantics', 'profile', 'hud'}
                    await asyncio.sleep(0.26)
                    absent = await client.call_tool('read_presented_hud', {})
                    assert not absent.isError
                    absent_obj = json.loads(absent.content[0].text)
                    assert absent_obj['hud'] is None and 'reason' not in absent_obj
                    assert obj['challenge'] != absent_obj['challenge']
                    await asyncio.sleep(0.26)
                pixels = await client.call_tool('read_presented_cursor', {})
                assert not pixels.isError and len(pixels.content) == 2
                meta = json.loads(pixels.content[0].text)
                assert meta['schema'] == 'openxcom-mcp-presented-cursor/1'
                assert set(meta) == {'schema', 'epoch', 'challenge', 'semantics', 'profile', 'crop'}
                candidate = {'schema': 'experimental-presented-cursor/0', 'session': meta['epoch'],
                             'semantics': 'last_observation', 'freshness': 'unverified',
                             'profile': PROFILE, 'unavailable_or_unsupported': False,
                             'public_revision': 0, 'crop': CROP}
                assert base64.b64decode(pixels.content[1].data) == encode_png(candidate)
                if version == 2:
                    await asyncio.sleep(0.26)
                    malformed = await client.call_tool('read_presented_hud', {})
                    assert malformed.isError and len(malformed.content) == 1
                    assert json.loads(malformed.content[0].text) == {
                        'available': False, 'reason': 'observation_unavailable'}
                print(json.dumps({'sdk_version': package_version('mcp'),
                                  'protocol': initialized.protocolVersion,
                                  'presentation_version': version, 'initialize': True,
                                  'list_tools': True, 'exact_cursor_png': True,
                                  'historical_current_denied': True,
                                  **({'displayed_hud': True, 'null_hud': True,
                                      'overflow_rejected': True} if version == 2 else {})}))


if __name__ == '__main__':
    if sys.argv[1:] == ['--synthetic-producer']:
        synthetic_producer()
    else:
        import asyncio
        asyncio.run(smoke())
