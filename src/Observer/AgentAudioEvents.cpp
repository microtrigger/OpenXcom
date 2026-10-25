// SPDX-License-Identifier: GPL-3.0-or-later
#include "AgentAudioEvents.h"
#include "DiagnosticProfile.h"
#include <SDL_mixer.h>
#include <cstdlib>
#include <cstring>

namespace OpenXcom { namespace Observer {
AgentAudioEvents::AgentAudioEvents() : _catalog(0), _read(0), _write(0), _failed(0),
    _produced(0), _sequence(0), _partialBytes(0), _partialNonzero(false), _skipFirst(true),
    _attached(false), _terminal(false), _pending(false), _initialStatusPending(true),
    _activeStatusPending(false), _terminalStatusPending(false), _partialWindowDiscarded(false),
    _expected(1), _started(Clock::now()) {}
AgentAudioEvents::~AgentAudioEvents() { close(); }
bool AgentAudioEvents::lockFree() const
{
    return _catalog.is_lock_free() && _read.is_lock_free() && _write.is_lock_free() &&
        _failed.is_lock_free() && _produced.is_lock_free();
}
void AgentAudioEvents::publishCatalogSupported(bool supported)
{
    _catalog.store(supported ? 1 : 2, std::memory_order_release);
}
void AgentAudioEvents::poll(bool active)
{
    if (_terminal) return;
    const char *mode = std::getenv("OPENXCOM_AGENT_CONTROL");
    if (!active || !mode || std::strcmp(mode, "1") || !diagnosticProfileQualified())
    { close(); return; }
    if (_failed.load(std::memory_order_acquire)) { close(); return; }
    if (_attached) return;
    if (!lockFree() || Clock::now() - _started >= std::chrono::seconds(30))
    { close(); return; }
    const unsigned catalog = _catalog.load(std::memory_order_acquire);
    if (!catalog) return;
    int rate = 0, channels = 0;
    Uint16 format = 0;
    const SDL_version *version = Mix_Linked_Version();
    if (catalog != 1 || !version || version->major != 1 || version->minor != 2 || version->patch != 12 ||
        !Mix_QuerySpec(&rate, &format, &channels) || rate != 22050 || format != AUDIO_S16LSB || channels != 2)
    { close(); return; }
    // The build excludes the independent mixed-audio observer. This is the
    // sole postmix owner, while ordinary music hooks remain untouched.
    Mix_SetPostMix(postmix, this);
    _attached = true;
    _activeStatusPending = true;
}
void AgentAudioEvents::beforeAudioReset(bool resourceReload)
{
    if (_attached) close(); // A qualified capture epoch is never reused.
    if (resourceReload) _catalog.store(0, std::memory_order_release);
}
void AgentAudioEvents::close()
{
    if (_terminal) return;
    if (_attached)
    {
        // SDL_mixer 1.2.12 synchronizes internally; no outer SDL_LockAudio.
        Mix_SetPostMix(NULL, NULL);
        _attached = false;
    }
    // The setter synchronized with the callback before this private read.
    // Completed windows remain queued; only the unfinished tail is discarded.
    _partialWindowDiscarded = _partialBytes != 0;
    _terminalStatusPending = true;
    _terminal = true;
}
void SDLCALL AgentAudioEvents::postmix(void *context, Uint8 *stream, int len)
{
    static_cast<AgentAudioEvents *>(context)->observe(stream, len);
}
void AgentAudioEvents::observe(const Uint8 *stream, int len)
{
    if (_failed.load(std::memory_order_relaxed)) return;
    if (!stream || len <= 0 || len > MaxCallbackBytes || len % FrameBytes)
    { _failed.store(1, std::memory_order_release); return; }
    // Registration can meet an already-mixed block. The continuous epoch
    // begins with the following callback, with no pre-attachment samples.
    if (_skipFirst) { _skipFirst = false; return; }
    const unsigned windowBytes = FramesPerWindow * FrameBytes;
    unsigned offset = 0;
    while (offset < static_cast<unsigned>(len))
    {
        const unsigned remaining = windowBytes - _partialBytes;
        const unsigned available = static_cast<unsigned>(len) - offset;
        const unsigned count = remaining < available ? remaining : available;
        // S16LE digital zero has all zero bytes. No waveform classification,
        // source reconstruction, playback request, or game identity is read.
        for (unsigned i = 0; !_partialNonzero && i < count; ++i)
            if (stream[offset + i]) _partialNonzero = true;
        std::memcpy(_partialPcm + _partialBytes, stream + offset, count);
        _partialBytes += count;
        offset += count;
        if (_partialBytes != windowBytes) continue;
        if (_sequence >= 9007199254740991ULL)
        { _failed.store(1, std::memory_order_release); return; }
        ++_sequence;
        const unsigned write = _write.load(std::memory_order_relaxed);
        const unsigned next = (write + 1) % Slots;
        if (next != _read.load(std::memory_order_acquire))
        {
            _ring[write].sequence = _sequence;
            _ring[write].nonzero = _partialNonzero;
            std::memcpy(_ring[write].pcm, _partialPcm, WindowBytes);
            _write.store(next, std::memory_order_release);
        }
        // Publish after the optional ring write. When the ring is full this
        // advances only the completed-output watermark, exposing exact loss.
        _produced.store(_sequence, std::memory_order_release);
        _partialBytes = 0;
        _partialNonzero = false;
    }
}
bool AgentAudioEvents::take(Event &event)
{
    event = Event();
    if (_initialStatusPending)
    {
        _initialStatusPending = false;
        event.kind = Event::Status;
        event.state = Event::Pending;
        return true;
    }
    if (_activeStatusPending)
    {
        _activeStatusPending = false;
        event.kind = Event::Status;
        event.state = Event::Active;
        return true;
    }
    Summary summary;
    if (_pending)
    {
        summary = _pendingSummary;
        _pending = false;
    }
    else
    {
        const unsigned read = _read.load(std::memory_order_relaxed);
        if (read == _write.load(std::memory_order_acquire))
        {
            const std::uint64_t produced = _produced.load(std::memory_order_acquire);
            // Acquiring the watermark precedes this second ring check. A
            // published entry can never be mistaken for an overflow gap.
            if (read != _write.load(std::memory_order_acquire)) return false;
            if (produced < _expected)
            {
                if (!_terminalStatusPending) return false;
                _terminalStatusPending = false;
                event.kind = Event::Status;
                event.state = Event::Unavailable;
                event.partialWindowDiscarded = _partialWindowDiscarded;
                return true;
            }
            event.kind = Event::Gap;
            event.droppedWindows = produced - _expected + 1;
            _expected = produced + 1;
            return true;
        }
        summary = _ring[read];
        _read.store((read + 1) % Slots, std::memory_order_release);
    }
    if (summary.sequence > _expected)
    {
        _pendingSummary = summary;
        _pending = true;
        event.kind = Event::Gap;
        event.droppedWindows = summary.sequence - _expected;
        _expected = summary.sequence;
        return true;
    }
    if (summary.sequence != _expected)
    { _failed.store(1, std::memory_order_release); return false; }
    event.nonzero = summary.nonzero;
    event.pcmSequence = summary.sequence;
    std::memcpy(event.pcm, summary.pcm, WindowBytes);
    _expected = summary.sequence + 1;
    return true;
}
} }
