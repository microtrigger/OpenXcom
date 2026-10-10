# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Bounded synthetic encoding/validation evidence, not game or model savings."""
import json
import time
import tracemalloc

import adapter
import mixed_audio as audio


def measure():
    pcm = bytes(audio.PCM_BYTES)
    native = {'schema': audio.SCHEMA, 'epoch': '1' * 32, 'challenge': '3' * 32,
              'next_token': '4' * 32, 'semantics': audio.SEMANTICS, 'profile': audio.PROFILE,
              'duration_ms': 500, 'sample_rate': audio.SAMPLE_RATE, 'channels': audio.CHANNELS,
              'sample_format': 's16le', 'frames': audio.FRAMES, 'pcm_hex': pcm.hex()}
    packet = json.dumps(native, separators=(',', ':')).encode()
    class SyntheticClient:
        def observe(self):
            return audio.validate(packet, '1' * 32, '3' * 32, '2' * 32)
    server = adapter.Server(mixed_audio=SyntheticClient())
    server.state = 'ready'
    request = {'jsonrpc': '2.0', 'id': 1, 'method': 'tools/call',
               'params': {'name': 'read_mixed_audio', 'arguments': {}}}
    def pipeline():
        return json.dumps(server.handle(request), separators=(',', ':')).encode() + b'\n'
    for _ in range(10): pipeline()
    iterations = 1000
    started_wall, started_cpu = time.perf_counter(), time.process_time()
    for _ in range(iterations): result = pipeline()
    wall, cpu = time.perf_counter() - started_wall, time.process_time() - started_cpu
    tracemalloc.start()
    pipeline()
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return {'license': 'GPL-3.0-or-later; see repository LICENSE.txt and LICENSE-NOTICES.md',
            'date': '2026-10-10', 'fixture': 'synthetic all-zero interleaved PCM',
            'duration_ms': 500, 'sample_rate': 22050, 'channels': 2, 'frames': 11025,
            'pcm_bytes': len(pcm), 'native_hex_characters': len(native['pcm_hex']),
            'native_json_packet_bytes': len(packet), 'native_packet_bound_bytes': audio.MAX_PACKET,
            'wav_header_bytes': 44, 'wav_bytes': len(audio.encode_wav(pcm)),
            'base64_characters': len(audio.audio_content(pcm)['data']),
            'mcp_jsonrpc_id_1_line_bytes': len(result), 'mcp_line_bound_bytes': adapter.MAX_RESPONSE,
            'iterations': iterations, 'wall_seconds': wall, 'cpu_seconds': cpu,
            'wall_microseconds_per_iteration': wall * 1e6 / iterations,
            'python_traced_peak_bytes_one_iteration': peak,
            'scope': 'in-memory native JSON validation, WAV/base64 and MCP JSON serialization only',
            'not_measured': ['game capture latency', 'socket I/O', 'device output', 'process RSS',
                             'model tokens', 'model cost', 'end-to-end savings', 'timing parity']}


if __name__ == '__main__':
    print(json.dumps(measure(), indent=2))
