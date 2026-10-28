// SPDX-License-Identifier: GPL-3.0-or-later
// Continuous, bounded summaries of actual application postmix PCM only.
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <SDL.h>

namespace OpenXcom { namespace Observer {
class AgentAudioEvents
{
public:
    enum { FramesPerWindow = 2205, FrameBytes = 4, WindowBytes = FramesPerWindow * FrameBytes, RingCapacity = 32,
           MaxCallbackBytes = 262144 };
    // Actual callback-entry clocks and frame offsets, NOT speaker timestamps.
    struct Timing {
        std::uint64_t firstRealtimeUs = 0, firstMonotonicUs = 0;
        unsigned firstFrameOffset = 0, firstCallbackFrames = 0;
        std::uint64_t lastRealtimeUs = 0, lastMonotonicUs = 0;
        unsigned lastFrameEndOffset = 0, lastCallbackFrames = 0;
    };
    struct Event {
        enum Kind { Window, Gap, Status } kind = Window;
        enum State { Pending, Active, Unavailable } state = Pending;
        bool nonzero = false;
        Timing timing;
        std::uint64_t pcmSequence = 0;
        Uint8 pcm[WindowBytes] = {};
        std::uint64_t droppedWindows = 0;
        bool partialWindowDiscarded = false;
    };
    AgentAudioEvents();
    ~AgentAudioEvents();
    // Only this aggregate is published by the resource-loading thread.
    void publishCatalogSupported(bool supported);
    // Main thread; active must come from the validated agent controller.
    void poll(bool active);
    // Main-thread consumer, independent of whether an MCP request arrived.
    bool take(Event &event);
    // Main-thread current status survives lifecycle-event history eviction.
    Event::State status() const { return _terminal ? Event::Unavailable : _attached ? Event::Active : Event::Pending; }
    void beforeAudioReset(bool resourceReload = false);
    void close();
private:
    typedef std::chrono::steady_clock Clock;
    struct Summary { std::uint64_t sequence; bool nonzero; Timing timing; Uint8 pcm[WindowBytes]; };
    enum { Slots = RingCapacity + 1 };
    std::atomic<unsigned> _catalog, _read, _write, _failed;
    std::atomic<std::uint64_t> _produced;
    Summary _ring[Slots];
    // Audio-callback-only fields until synchronized hook detachment.
    std::uint64_t _sequence;
    Uint8 _partialPcm[WindowBytes];
    unsigned _partialBytes;
    Timing _partialTiming;
    std::uint64_t _lastRealtimeUs, _lastMonotonicUs;
    bool _partialNonzero, _skipFirst;
    // Main-thread-only fields; no callback accesses controller/game state.
    bool _attached, _terminal, _pending;
    bool _initialStatusPending, _activeStatusPending, _terminalStatusPending, _partialWindowDiscarded;
    Summary _pendingSummary;
    std::uint64_t _expected;
    Clock::time_point _started;
    bool lockFree() const;
    static void SDLCALL postmix(void *context, Uint8 *stream, int len);
    void observe(const Uint8 *stream, int len);
    AgentAudioEvents(const AgentAudioEvents &) = delete;
    AgentAudioEvents &operator=(const AgentAudioEvents &) = delete;
};
} }
