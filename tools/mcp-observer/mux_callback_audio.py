"""Piecewise correction of actual PCM sample clock, never silence/gap filling.
No production interface changes. All fit anchors originate in the postmix callback.
"""
import hashlib,json,math,pathlib,statistics,subprocess,sys
import numpy as np
RATE=22050
FRAME=2205
# Preserve the earlier recorder's 150ms alignment and 20ms wall-clock gates.
ALIGNMENT_BUDGET=.150
WALL_JUMP_BUDGET=.020
VIDEO_FRAME=1/15
PACKET_QUANTIZATION=.001 # Matroska capture PTS uses millisecond timebase.

def least_squares(points):
    assert len(points)>=3
    xm=statistics.mean(x for x,y in points);ym=statistics.mean(y for x,y in points)
    denominator=sum((x-xm)**2 for x,y in points);assert denominator>0
    slope=sum((x-xm)*(y-ym)for x,y in points)/denominator
    assert math.isfinite(slope) and slope>0
    return ym-slope*xm,slope

def model(rows):
    assert len(rows)>=6
    anchors={}
    for seq,r in enumerate(rows,1):
        assert r['pcm_sequence']==seq and r['frames']==FRAME,'missing PCM window'
        t=r['timing'];assert t['clock']=='posix_realtime_monotonic_us'
        assert 0<=t['first_frame_offset']<t['first_callback_frames'] and 1<=t['last_frame_end_offset']<=t['last_callback_frames']
        for frame,prefix in (((seq-1)*FRAME-t['first_frame_offset'],'first'),
                             (seq*FRAME-t['last_frame_end_offset'],'last')):
            m=t[prefix+'_monotonic_us'];w=t[prefix+'_realtime_us'];count=t[prefix+'_callback_frames']
            assert isinstance(m,int) and not isinstance(m,bool) and m>0 and w>0 and 1<=count<=65536
            anchor=(m,w,count)
            assert frame>=0 and (frame not in anchors or anchors[frame]==anchor),'contradictory callback anchor'
            anchors[frame]=anchor
    xs=sorted(anchors);assert len(xs)>=6
    assert all(anchors[b][0]>anchors[a][0] and anchors[b][1]>anchors[a][1]for a,b in zip(xs,xs[1:])), 'backward callback clock'
    offsets=[(anchors[x][1]-anchors[x][0])/1e6 for x in xs]
    jump=max(offsets)-min(offsets);assert jump<WALL_JUMP_BUDGET,'wall-clock discontinuity'
    origin=anchors[xs[0]][0]
    points=[(x,(anchors[x][0]-origin)/1e6)for x in xs]
    xx=np.array([x for x,y in points],dtype=float);tt=np.array([y for x,y in points])
    slopes=np.diff(tt)/np.diff(xx);assert np.all(slopes>0)
    # Alternating-anchor holdout tests local interpolation without the tested anchors.
    holdout=0.
    for parity in (0,1):
        keep=np.arange(parity,len(xx),2)
        test=np.arange(1-parity,len(xx),2)
        test=test[(test>keep[0])&(test<keep[-1])]
        holdout=max(holdout,float(np.max(np.abs(np.interp(xx[test],xx[keep],tt[keep])-tt[test]))))
    callback=max(anchors[x][2] for x in xs)*float(np.max(slopes))
    uncertainty=holdout+callback+VIDEO_FRAME+PACKET_QUANTIZATION+jump+1/RATE
    assert uncertainty<ALIGNMENT_BUDGET,'piecewise fit exceeds original150ms alignment budget'
    start=origin/1e6+statistics.median(offsets)
    return dict(method='bounded_piecewise_callback_sample_clock',anchors=len(points),
        frame_anchors=xx.tolist(),time_anchors=tt.tolist(),
        audio_start_realtime=start,audio_end_realtime=start+float(tt[-1]),
        alternate_anchor_holdout_max_error_seconds=holdout,
        max_callback_duration_seconds=callback,rate_rounding_error_seconds=1/RATE,
        video_frame_seconds=VIDEO_FRAME,packet_timestamp_quantization_seconds=PACKET_QUANTIZATION,
        max_wall_clock_offset_change_seconds=jump,alignment_uncertainty_seconds=uncertainty,
        alignment_budget_seconds=ALIGNMENT_BUDGET,source_clock_not_speaker_time=True)

def run(p):
    status=json.loads((p/'audio-status.json').read_text())
    rows=[json.loads(x)for x in(p/'audio-windows.jsonl').read_text().splitlines()]
    assert status['error'] is None and status['windows']==len(rows) and status['nonzero_windows']>0
    pcm=(p/'actual-postmix.s16le').read_bytes();assert len(pcm)==len(rows)*FRAME*4
    for n,r in enumerate(rows):
        chunk=pcm[n*FRAME*4:(n+1)*FRAME*4]
        assert hashlib.sha256(chunk).hexdigest()==r['sha256'],'PCM bytes differ from captured receipt'
    fit=model(rows)
    waveform=np.frombuffer(pcm,dtype='<i2').reshape(-1,2).astype(np.float64)
    times=np.arange(math.floor((fit['audio_end_realtime']-fit['audio_start_realtime'])*RATE))/RATE
    positions=np.interp(times,fit['time_anchors'],fit['frame_anchors'])
    assert positions[0]>=0 and positions[-1]<len(waveform)
    source_positions=np.arange(len(waveform))
    corrected=np.column_stack([np.interp(positions,source_positions,waveform[:,channel]) for channel in range(2)])
    corrected=np.clip(np.rint(corrected),-32768,32767).astype('<i2')
    corrected_path=p/'clock-corrected53.s16le'
    assert not corrected_path.exists(), 'do not overwrite uncertain resampling'
    corrected_path.write_bytes(corrected.tobytes())
    raw=subprocess.run(['ffprobe','-v','error','-show_packets','-select_streams','v:0','-of','json',str(p/'passive-video.mkv')],capture_output=True,text=True,check=True)
    packets=json.loads(raw.stdout)['packets'];pts=[float(x['pts_time'])for x in packets]
    assert len(pts)>2 and all(b>=a for a,b in zip(pts,pts[1:]))
    observed_gap=max(b-a for a,b in zip(pts,pts[1:]))
    fit['observed_max_video_packet_gap_seconds']=observed_gap
    fit['alignment_uncertainty_seconds']+=max(0,observed_gap-VIDEO_FRAME)
    assert fit['alignment_uncertainty_seconds']<ALIGNMENT_BUDGET,'observed video cadence exceeds alignment budget'
    assert abs(pts[0]-fit['audio_start_realtime'])<120,'no common capture clock'
    start=max(pts[0],fit['audio_start_realtime']);end=min(pts[-1]+VIDEO_FRAME,fit['audio_end_realtime'])
    assert end-start>3
    # Trim only captured edges; resample the real retained waveform, never apad/async.
    first=math.ceil((start-fit['audio_start_realtime'])*RATE)
    last=math.floor((end-fit['audio_start_realtime'])*RATE)
    assert 0<=first<last<=len(corrected)
    target=p/'passive-av-piecewise53.mp4'
    filters=(f'[0:v]setpts=PTS-STARTPTS,trim=start={start-pts[0]:.9f}:duration={end-start:.9f},setpts=PTS-STARTPTS[v];'
             f'[1:a]atrim=start_sample={first}:end_sample={last},asetpts=PTS-STARTPTS,'
             f'anull[a]')
    cmd=['ffmpeg','-nostdin','-v','warning','-n','-i',str(p/'passive-video.mkv'),'-f','s16le','-ar',str(RATE),'-ac','2','-i',str(corrected_path),'-filter_complex',filters,'-map','[v]','-map','[a]','-c:v','libx264','-preset','fast','-crf','20','-c:a','aac','-b:a','128k','-movflags','+faststart',str(target)]
    result=subprocess.run(cmd,capture_output=True,text=True);(p/'mux-piecewise53.log').write_text(result.stdout+result.stderr)
    assert result.returncode==0,'affine mux failed; do not repeat uncertain output'
    probe=subprocess.run(['ffprobe','-v','error','-show_streams','-show_format','-of','json',str(target)],capture_output=True,text=True,check=True)
    metadata=json.loads(probe.stdout);streams=metadata['streams'];assert {s['codec_type']for s in streams}=={'audio','video'}
    durations={s['codec_type']:float(s['duration'])for s in streams}
    assert abs(durations['audio']-durations['video'])<ALIGNMENT_BUDGET,'mux duration disagreement'
    report=dict(status='qualified_bounded_piecewise_callback_timeline',fit={k:v for k,v in fit.items() if k not in ('frame_anchors','time_anchors')},source='actual_retained_postmix',
        output=str(target),video_start_realtime=pts[0],overlap_seconds=end-start,
        first_retained_sample=first,end_retained_sample_exclusive=last,
        fabricated_silence=False,missing_windows_filled=False,pcm_samples_resampled=True,
        processing='Declared piecewise linear interpolation of actual PCM against measured callback anchors of actual PCM and AAC encoding; no async/apad',
        synchronization_claim='Digital callback timeline only; conservative measured bound includes callback buffer, frame cadence, fit/holdout residual, clock drift and quantization. No speaker latency measurement.',
        command=cmd,probe=metadata)
    (p/'mux-piecewise53-result.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

if __name__=='__main__':run(pathlib.Path(sys.argv[1]))
