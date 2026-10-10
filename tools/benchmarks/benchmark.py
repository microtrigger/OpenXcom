#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Created 2026-10-10. See the OpenXcom project's LICENSE.txt for full terms.
"""Offline transport microbenchmarks. No game, GUI, network or model calls."""
import argparse
import base64
import csv
import hashlib
import json
from pathlib import Path
import random
import resource
import statistics
import struct
import sys
import time
import zlib


def dump(obj):
    return json.dumps(obj, separators=(',', ':'), ensure_ascii=True).encode()


def fixture(pixels):
    return dict(schema='experimental-presented-cursor/0', session='a'*32,
                semantics='last_observation', freshness='unverified', public_revision=1,
                profile='software-single-buffer-8bit-integer-scale-no-bands',
                unavailable_or_unsupported=False,
                crop=dict(screen_width=960, screen_height=600, x=0, y=0,
                          width=128, height=128, format='rgb24-hex-row-major', pixels=pixels.hex()))


def decode_png(png):
    """Independent verification of adapter's exact RGB, filter-0 PNG subset."""
    assert png[:8] == b'\x89PNG\r\n\x1a\n'
    pos, data, size = 8, b'', None
    while pos < len(png):
        length = struct.unpack('>I', png[pos:pos+4])[0]
        tag, payload = png[pos+4:pos+8], png[pos+8:pos+8+length]
        crc = struct.unpack('>I', png[pos+8+length:pos+12+length])[0]
        assert zlib.crc32(tag + payload) == crc
        if tag == b'IHDR':
            w, h, depth, color, compression, filtering, interlace = struct.unpack('>IIBBBBB', payload)
            assert (depth, color, compression, filtering, interlace) == (8, 2, 0, 0, 0)
            size = (w, h)
        elif tag == b'IDAT':
            data += payload
        pos += length + 12
    w, h = size
    rows = zlib.decompress(data)
    assert len(rows) == h * (w*3+1)
    assert all(rows[y*(w*3+1)] == 0 for y in range(h))
    return b''.join(rows[y*(w*3+1)+1:(y+1)*(w*3+1)] for y in range(h))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--adapter-dir', type=Path, required=True)
    p.add_argument('--cursor', type=Path, action='append', default=[])
    p.add_argument('--repeats', type=int, default=50)
    p.add_argument('--seed', type=int, default=1729)
    p.add_argument('--out', type=Path, required=True)
    args = p.parse_args()
    if args.repeats < 2:
        p.error('repeats must be at least 2')
    sys.path.insert(0, str(args.adapter_dir.resolve()))
    import snapshots as s
    rng = random.Random(args.seed)
    cases = [('synthetic_black', dump(fixture(bytes(49152)))),
             ('synthetic_noise', dump(fixture(rng.randbytes(49152)))),
             ('synthetic_palette', dump(fixture(bytes((i//3 % 16)*16 for i in range(49152)))))]
    cases += [(f'cursor_{i}', path.read_bytes()) for i, path in enumerate(args.cursor, 1)]
    rows, inventory = [], []
    for name, raw in cases:
        obj = s.validate(raw, 'cursor')
        if obj['crop'] is None:
            raise ValueError(f'{name}: no crop; codec benchmark requires pixels')
        pixels = bytes.fromhex(obj['crop']['pixels'])
        png = s.encode_png(obj)
        assert decode_png(png) == pixels
        cache = s.CursorCache()
        assert cache.convert(raw) == png
        operations = {
            'validate_historical_json': lambda: s.validate(raw, 'cursor'),
            'encode_png_including_validation': lambda: s.encode_png(obj),
            'base64_png_only': lambda: base64.b64encode(png),
            'cache_hit_including_validation': lambda: cache.convert(raw),
            'image_block_json_including_encode': lambda: dump(s.image_content(obj)),
        }
        # Every block runs all operations; shuffle paired order to reduce time drift.
        # First observed invocation is recorded separately; OS pages are not flushed.
        for rep in range(args.repeats):
            order = list(operations)
            rng.shuffle(order)
            for ordinal, op in enumerate(order):
                cpu0, t0 = time.process_time_ns(), time.perf_counter_ns()
                value = operations[op]()
                wall, cpu = time.perf_counter_ns()-t0, time.process_time_ns()-cpu0
                rows.append(dict(fixture=name, repetition=rep, order=ordinal,
                                 phase='first_observed' if rep == 0 else 'warm', operation=op,
                                 wall_ns=wall, process_cpu_ns=cpu, success=True))
        image_json = dump(s.image_content(obj))
        inventory.append(dict(fixture=name, input_sha256=hashlib.sha256(raw).hexdigest(),
                              width=obj['crop']['width'], height=obj['crop']['height'],
                              rgb_bytes=len(pixels), hex_utf8_bytes=len(obj['crop']['pixels'].encode()),
                              snapshot_json_bytes=len(raw), png_bytes=len(png),
                              png_base64_bytes=len(base64.b64encode(png)),
                              image_block_json_bytes=len(image_json), lossless_verified=True))
    summary = []
    for item in inventory:
        for op in operations:
            subset = [r for r in rows if r['fixture']==item['fixture'] and r['operation']==op and r['phase']=='warm']
            times = sorted(r['wall_ns'] for r in subset)
            summary.append(dict(fixture=item['fixture'], operation=op, samples=len(times),
                                median_wall_ns=statistics.median(times),
                                p95_wall_ns=times[min(len(times)-1, int(.95*len(times)))],
                                median_process_cpu_ns=statistics.median(r['process_cpu_ns'] for r in subset)))
    result = dict(schema='openxcom-offline-codec-benchmark/1', scope='historical offline transport microbenchmark',
                  freshness='unverified', seed=args.seed, repeats=args.repeats,
                  codec_reference=dict(format='PNG RGB8 filter 0', zlib=zlib.ZLIB_RUNTIME_VERSION),
                  adapter_sha256={name:hashlib.sha256((args.adapter_dir/name).read_bytes()).hexdigest() for name in ('adapter.py','snapshots.py')},
                  inventory=inventory, summary=summary,
                  resource_scope='single harness process; peak RSS cumulative, includes Python/inputs/adapter; not incremental or game RSS',
                  harness_peak_rss_kib=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss if sys.platform.startswith('linux') else None,
                  actual_model_tokens=None, model_compute=None, bill_cost=None, game_cpu=None, game_rss=None,
                  limitations=['No model API, GUI acquisition, game action, live freshness, network, or stdio IPC measured.',
                               'No fair MCP-versus-GUI speedup or token/cost inference follows from these bytes.',
                               'First observed is not a cold process or cold page cache; setup/validation already ran.',
                               'Process CPU covers benchmark process, not whole-machine CPU; timing is not comparable across machines or uncontrolled loads.'])
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out/'results.json').write_text(json.dumps(result, indent=2)+'\n')
    with (args.out/'samples.csv').open('w') as f:
        writer=csv.DictWriter(f, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
    print(json.dumps({'fixtures':len(inventory),'samples':len(rows),'lossless_verified':True}))

if __name__ == '__main__':
    main()
