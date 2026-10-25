# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded retained actual mixed PCM, never sound-source or world-state labels."""
import base64
import json
import struct
from snapshots import exact,integer,require

PROFILE='s16le-stereo-22050'
MAX_WINDOWS=8
MAX_PAGES=8
PCM_BYTES=8820

def validate_arguments(args):
    from agent_control import token,MAX_COUNTER
    require(type(args) is dict)
    exact(args,'epoch after limit page_budget' if 'page_budget' in args else 'epoch after limit');token(args['epoch'])
    integer(args.get('page_budget',MAX_PAGES),1,MAX_PAGES)
    integer(args['after'],0,MAX_COUNTER);integer(args['limit'],1,MAX_WINDOWS)

def metadata_page(page):
    result=dict(page)
    result['events']=[{k:v for k,v in event.items() if k!='pcm_hex'} for event in page['events']]
    return result

def wav(pcm):
    return (b'RIFF'+struct.pack('<I',len(pcm)+36)+b'WAVEfmt '+struct.pack('<IHHIIHH',16,1,2,22050,88200,4,16)
            +b'data'+struct.pack('<I',len(pcm))+pcm)

def read_audio(client,epoch,after,limit,page_budget=MAX_PAGES):
    validate_arguments(dict(epoch=epoch,after=after,limit=limit,page_budget=page_budget))
    cursor=after;cutoff=None;latest=after;windows=0;gap=False;events=[];segments=[];blocks=[]
    pending=bytearray();first_pcm=last_pcm=first_event=last_event=None;previous_pcm=None
    status='unavailable';history='unavailable';pages=0
    def flush():
        nonlocal pending,first_pcm,last_pcm,first_event,last_event
        if not pending:return
        index=len(blocks)+1
        blocks.append(dict(type='audio',mimeType='audio/wav',data=base64.b64encode(wav(bytes(pending))).decode('ascii')))
        segments.append(dict(content_index=index,first_pcm_sequence=first_pcm,last_pcm_sequence=last_pcm,
            first_event_sequence=first_event,last_event_sequence=last_event,frames=len(pending)//4,
            duration_ms=len(pending)//PCM_BYTES*100,continuity_before='unknown_or_boundary'))
        pending=bytearray();first_pcm=last_pcm=first_event=last_event=None
    for _ in range(page_budget):
        page=client.read_events(epoch,cursor,16);pages+=1
        if cutoff is None:cutoff=page['newest']
        latest=page['newest'];status=page['audio_status'];history=page['audio_history']
        if page['gap']:gap=True;flush()
        for event in page['events']:
            if event['seq']>cutoff or windows>=limit:break
            cursor=event['seq']
            summary={'seq':cursor,'kind':event['kind']}
            if event['kind']=='audio_output':
                seq=event['pcm_sequence']
                require(previous_pcm is None or seq > previous_pcm)
                if previous_pcm is not None and seq != previous_pcm+1:gap=True
                previous_pcm=seq
                if pending and seq!=last_pcm+1:flush();gap=True
                if not pending:first_pcm=seq;first_event=cursor
                pending.extend(bytes.fromhex(event['pcm_hex']));last_pcm=seq;last_event=cursor;windows+=1
                summary.update(pcm_sequence=seq,nonzero=event['nonzero'],duration_ms=100)
            elif event['kind'] in ('audio_gap','audio_status'):
                flush()
                if event['kind']=='audio_gap':gap=True;summary['dropped_windows']=event['dropped_windows']
                else:summary.update(status=event['status'],partial_window_discarded=event['partial_window_discarded'])
            events.append(summary)
        if cursor>=cutoff or windows>=limit or not page['events']:break
    flush()
    obj=dict(schema='openxcom-public-audio/1',epoch=epoch,after=after,cutoff=cutoff,next_cursor=cursor,
        newest=latest,audio_history=history,audio_status=status,gap=gap,deferred=cursor<cutoff,
        profile=PROFILE,sample_rate=22050,channels=2,sample_format='s16le',windows=windows,pages=pages,
        segments=segments,events=events,semantics='actual_retained_postmix_no_source_attribution',
        temporal_alignment='sample_order_only_no_video_clock')
    result=dict(content=[dict(type='text',text=json.dumps(obj,separators=(',',':')))]+blocks,structuredContent=obj,isError=False)
    require(len(json.dumps(result,separators=(',',':')).encode())<=131072)
    return result
