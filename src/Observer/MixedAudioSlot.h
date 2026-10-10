// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// Free software under GNU GPL version 3 or later; without warranty.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
#include <atomic>
#include <cstddef>
#include <cstring>

namespace OpenXcom { namespace Observer {
// One producer (SDL audio callback), one consumer (game main thread).
// No history, cancellation, wraparound generation or concurrent PCM readers.
class MixedAudioSlot
{
public:
    enum { Frames = 11025, FrameBytes = 4, Bytes = Frames * FrameBytes,
           MaxCallbackBytes = 262144 };
    enum State { IDLE, ARMED, CAPTURING, READY, FAILED, CLOSED };
private:
    std::atomic<unsigned> _state;
    unsigned char _pcm[Bytes];
    std::size_t _offset; // Callback-private until READY; main never reads it.
public:
    MixedAudioSlot() : _state(IDLE), _offset(0) {}
    bool lockFree() const { return _state.is_lock_free(); }
    State state() const { return static_cast<State>(_state.load(std::memory_order_acquire)); }
    bool arm()
    {
        if (state() != IDLE) return false;
        _state.store(ARMED, std::memory_order_release);
        return true;
    }
    void callback(const unsigned char *stream, int len)
    {
        const State ownership = state();
        if (ownership != ARMED && ownership != CAPTURING) return;
        if (!stream || len <= 0 || len > MaxCallbackBytes || len % FrameBytes)
        {
            _state.store(FAILED, std::memory_order_release);
            return;
        }
        if (ownership == ARMED)
        {
            // This block may have started mixing before admission. Discard all
            // of it; only later callback invocations contribute PCM.
            _offset = 0;
            _state.store(CAPTURING, std::memory_order_release);
            return;
        }
        const std::size_t remaining = Bytes - _offset;
        const std::size_t take = static_cast<std::size_t>(len) < remaining ? len : remaining;
        std::memcpy(_pcm + _offset, stream, take);
        _offset += take;
        if (_offset == Bytes) _state.store(READY, std::memory_order_release);
    }
    // Caller must observe READY with acquire first and retain ownership until
    // serialization AND successful send finish. Callback never touches READY.
    const unsigned char *readyData() const { return state() == READY ? _pcm : 0; }
    bool sent()
    {
        if (state() != READY) return false;
        _state.store(IDLE, std::memory_order_release);
        return true;
    }
    // Only after Mix_SetPostMix(NULL, NULL) has synchronized with the callback.
    // Terminal: this slot must never be armed again.
    void closeAfterDetach() { _state.store(CLOSED, std::memory_order_release); }
private:
    MixedAudioSlot(const MixedAudioSlot &);
    MixedAudioSlot &operator=(const MixedAudioSlot &);
};
} }
