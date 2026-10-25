# Accepted playback observation (opt-in)

`OPENXCOM_AGENT_SOUND_MODE=playback` selects a separate observer for accepted
`Sound::play` and newly started `Sound::loop` requests. Existing `read_public_events`
returns the events; action/state/tool catalogs and gameplay commands do not change.
The default mixed-PCM mode is unchanged. Playback mode does not install the PCM
postmix observer: `audio_history`/`audio_status` report unavailable honestly, while
required `sound_playback: {playback_epoch, status}` independently reports current
playback observer status (`inactive`, `active`, `unavailable`). Epoch zero means
inactive. An empty page or PCM status is not evidence of silence or active playback.

`sound_play_issued` contains an opaque random 128-bit `sound_id`, playback epoch,
ordered sound sequence, native monotonic milliseconds, positive channel/chunk gain
snapshots, loop-start flag, optional successful mixer position parameters and an
optional last-painted public viewport snapshot. Equality means identical decoded
content and format observed within this process, not a known weapon, unit, race,
filename or outcome. Identity persists across same-process load, while pending
observations and playback sequence reset with the playback epoch. Restart starts
a new unrelated identity space. A bounded consumer may report familiarity unknown
when its seen-ID capacity is exhausted; it must not claim unseen in that case.

Position angle/distance are mixer effect arguments, not source coordinates,
measured loudness, measured left/right gain or distance to an actor. Camera is an
issuance-time copy of the last successful public painted viewport, with frame,
revision, age and explicit staleness; it is not a full camera pose or a source
location. Native monotonic time is not UTC/video synchronization. Positive-gain
accepted playback is not proof of physical audibility or lack of masking. Music,
FLC and direct mixer calls are outside this narrow observer.

Fixed `sound_playback_status` and `sound_playback_gap` records report unavailable
observation, discarded pending history and overflow. Current page status remains
authoritative if older status events have been evicted. Missing events do not
prove no sound. No world-source or semantic classification is exported. Any
meaning assigned to repeated content is an operator's contextual hypothesis.

The private player uses these public records directly, without template loading,
waveform classification or claims of direct model listening. Whole public turn
tracking and bounded journals/views retain already delivered observations with
pending/gap/omission disclosure. A sound during an AI or neutral turn is not thereby
an enemy or civilian sound. Acknowledging a displayed view records declared review,
not proof of intelligent use. Same-channel read recovery does not replace the
owned process or permit action replay; old pending views are invalidated.

The private view may attach a separately marked `operator_declared_hypothesis`
to a seen ID. This is not a native semantic label. At most two annotations per
exact displayed-view acknowledgment are accepted, each at most160 characters,
with `tentative` or `uncertain` uncertainty and the exact evidence view ID. The ID
must occur in that delivered view and the current process's seen set. Up to128
associations are retained in memory across same-process load, not process restart;
capacity refusal is explicit. All annotations validate atomically before updates.
