# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Optional official SDK AudioContent smoke with synthetic anonymous peers.
No game audio, assets, device, listeners, credentials, API or model calls.
"""
import json
import os
import secrets
import select
import socket
import struct
import sys
from pathlib import Path

import mixed_audio as audio
from snapshots import PROFILE as VISUAL_PROFILE

PCM = struct.pack('<hh', -12345, 23456) * audio.FRAMES
CROP = {'screen_width': 320, 'screen_height': 200, 'x': 0, 'y': 0,
        'width': 1, 'height': 1, 'format': 'rgb24-hex-row-major', 'pixels': '010203'}
HUD = dict(time_units=0, energy=999, health=33, morale=100)


def synthetic_producer():
    peers = {}
    version = int(os.environ.get('OPENXCOM_PRESENTATION_VERSION', '1'))
    for kind, name in (('audio', 'OPENXCOM_MIXED_AUDIO_FD'),
                       ('visual', 'OPENXCOM_PRESENTATION_FD')):
        if name not in os.environ:
            continue
        peer = socket.socket(fileno=int(os.environ[name]))
        epoch, admission = secrets.token_hex(16), secrets.token_hex(16)
        hello = audio.HELLO if kind == 'audio' else f'OPENXCOM-PRESENTATION/{version} '.encode()
        peer.send(hello + (epoch + admission).encode())
        peers[peer] = [kind, epoch, admission, 0]
    try:
        while peers:
            ready, _, _ = select.select(list(peers), [], [], 5)
            assert ready, 'synthetic peer deadline'
            for peer in ready:
                request = peer.recv(97)
                if not request:
                    del peers[peer]; peer.close(); continue
                kind, epoch, admission, count = peers[peer]
                request = request.decode('ascii')
                assert len(request) == 96 and request[:64] == epoch + admission
                admission = secrets.token_hex(16)
                common = {'epoch': epoch, 'challenge': request[64:], 'next_token': admission}
                if kind == 'audio':
                    pcm = PCM if count == 0 else bytes(audio.PCM_BYTES)
                    reply = dict(common, schema=audio.SCHEMA, semantics=audio.SEMANTICS,
                                 profile=audio.PROFILE, duration_ms=500, sample_rate=22050,
                                 channels=2, sample_format='s16le', frames=11025, pcm_hex=pcm.hex())
                else:
                    reply = dict(common, schema=f'experimental-presentation/{version}',
                                 semantics='presented_after_request', profile=VISUAL_PROFILE, crop=CROP)
                    if version == 2:
                        reply['hud'] = HUD
                peer.send(json.dumps(reply, separators=(',', ':')).encode())
                peers[peer] = [kind, epoch, admission, count + 1]
    finally:
        for peer in peers:
            peer.close()


async def smoke():
    import asyncio
    import base64
    from datetime import timedelta
    from importlib.metadata import version as package_version
    from mcp import ClientSession, StdioServerParameters
    from mcp.client.stdio import stdio_client
    from mcp.types import AudioContent

    for flags, visual_version in ((['--audio-only'], None), (['--mixed-audio'], 1),
                                  (['--mixed-audio', '--presented-hud'], 2)):
        command = [str(Path(__file__).with_name('launch_presentation.py')), *flags,
                   '--', sys.executable, str(Path(__file__)), '--synthetic-producer']
        params = StdioServerParameters(command=sys.executable, args=command)
        async with stdio_client(params) as (read, write):
            async with ClientSession(read, write, read_timeout_seconds=timedelta(seconds=5)) as client:
                initialized = await client.initialize()
                listed = {tool.name for tool in (await client.list_tools()).tools}
                expected = {'read_last_hud', 'read_last_cursor', 'read_mixed_audio'}
                if visual_version is not None: expected.add('read_presented_cursor')
                if visual_version == 2: expected.add('read_presented_hud')
                assert listed == expected
                clip = await client.call_tool('read_mixed_audio', {})
                assert not clip.isError and len(clip.content) == 2
                assert isinstance(clip.content[1], AudioContent)
                assert clip.content[1].mimeType == 'audio/wav'
                assert base64.b64decode(clip.content[1].data, validate=True) == audio.encode_wav(PCM)
                assert len(clip.content[1].data) == audio.BASE64_CHARACTERS
                meta = json.loads(clip.content[0].text)
                assert set(meta) == set(audio.KEYS.split()) - {'next_token', 'pcm_hex'}
                assert meta['schema'] == audio.PUBLIC_SCHEMA and meta['semantics'] == audio.SEMANTICS
                if visual_version is not None:
                    visual = await client.call_tool('read_presented_cursor', {})
                    assert not visual.isError and visual.content[1].type == 'image'
                    visual_meta = json.loads(visual.content[0].text)
                    assert visual_meta['epoch'] != meta['epoch']
                    assert visual_meta['schema'] == 'openxcom-mcp-presented-cursor/1'
                    assert set(visual_meta) == {'schema', 'epoch', 'challenge', 'semantics', 'profile', 'crop'}
                await asyncio.sleep(1.02)
                silent = await client.call_tool('read_mixed_audio', {})
                assert not silent.isError and isinstance(silent.content[1], AudioContent)
                assert base64.b64decode(silent.content[1].data, validate=True) == audio.encode_wav(bytes(audio.PCM_BYTES))
                assert json.loads(silent.content[0].text)['challenge'] != meta['challenge']
                # A too-fast next call closes only the independent audio stream.
                rejected = await client.call_tool('read_mixed_audio', {})
                assert rejected.isError and len(rejected.content) == 1
                assert json.loads(rejected.content[0].text) == {
                    'available': False, 'reason': 'observation_unavailable'}
                if visual_version is not None:
                    name = 'read_presented_hud' if visual_version == 2 else 'read_presented_cursor'
                    survivor = await client.call_tool(name, {})
                    assert not survivor.isError
                    if visual_version == 2:
                        assert json.loads(survivor.content[0].text)['hud'] == HUD
                historical = await client.call_tool('read_last_hud', {'require_current': True})
                assert historical.isError
                print(json.dumps({'sdk_version': package_version('mcp'),
                                  'protocol': initialized.protocolVersion,
                                  'visual_version': visual_version,
                                  'native_audio_content': True, 'exact_wav_pcm': True,
                                  'silence_preserved': True, 'binding_rotated': True,
                                  'rate_closed_audio_only': True,
                                  'historical_current_denied': True}))


if __name__ == '__main__':
    if sys.argv[1:] == ['--synthetic-producer']:
        synthetic_producer()
    else:
        import asyncio
        asyncio.run(smoke())
