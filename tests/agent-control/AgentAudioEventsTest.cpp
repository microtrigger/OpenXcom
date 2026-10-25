// SPDX-License-Identifier: GPL-3.0-or-later
// Executes the production callback via stubbed mixer registration. This is
// bounded ring/lifecycle evidence, not real mixer/device or gameplay evidence.
#include "../../src/Observer/AgentAudioEvents.h"
#include <SDL_mixer.h>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
using OpenXcom::Observer::AgentAudioEvents;
namespace {
void (*callback)(void *, Uint8 *, int) = nullptr;
void *context = nullptr;
SDL_version version = {1, 2, 12};
int rate = 22050, channels = 2, opens = 1;
Uint16 format = AUDIO_S16LSB;
bool qualified = true;
unsigned checks = 0, attaches = 0, detaches = 0;
void check(bool value, const char *message)
{
    ++checks;
    if (!value) { std::cerr << "FAIL " << message << '\n'; std::exit(1); }
}
void attach(AgentAudioEvents &audio)
{
    audio.publishCatalogSupported(true); audio.poll(true);
    check(callback != nullptr, "qualified postmix registration");
    check(audio.status() == AgentAudioEvents::Event::Active, "current active status");
    AgentAudioEvents::Event event;
    check(audio.take(event) && event.kind == AgentAudioEvents::Event::Status &&
        event.state == AgentAudioEvents::Event::Pending, "initial pending status");
    check(audio.take(event) && event.kind == AgentAudioEvents::Event::Status &&
        event.state == AgentAudioEvents::Event::Active, "qualified active status");
}
void feed(Uint8 *bytes, int count) { callback(context, bytes, count); }
void feedWindows(unsigned count, bool nonzero = false)
{
    Uint8 bytes[AgentAudioEvents::FramesPerWindow * AgentAudioEvents::FrameBytes] = {};
    if (nonzero) bytes[sizeof(bytes) - 1] = 0x80;
    for (unsigned i = 0; i < count; ++i) feed(bytes, sizeof(bytes));
}
}
extern "C" const SDL_version * SDLCALL Mix_Linked_Version() { return &version; }
extern "C" int SDLCALL Mix_QuerySpec(int *r, Uint16 *f, int *c)
{ *r = rate; *f = format; *c = channels; return opens; }
extern "C" void SDLCALL Mix_SetPostMix(void (*fn)(void *, Uint8 *, int), void *data)
{
    if (fn) ++attaches; else ++detaches;
    callback = fn; context = data;
}
namespace OpenXcom { namespace Observer {
bool diagnosticProfileQualified() { return qualified; }
} }
int main()
{
    setenv("OPENXCOM_AGENT_CONTROL", "1", 1);
    {
        AgentAudioEvents audio; AgentAudioEvents::Event event;
        audio.poll(true); check(!callback, "catalog bootstrap has no hook");
        attach(audio); feedWindows(1, true);
        check(!audio.take(event), "first already-mixed block discarded");
        const unsigned window = AgentAudioEvents::FramesPerWindow * AgentAudioEvents::FrameBytes;
        std::vector<Uint8> bytes(window * 3 + 4, 0);
        bytes[window - 1] = 1; bytes[window * 2] = 0x80;
        const std::vector<Uint8> original = bytes;
        unsigned offset = 0;
        const unsigned sizes[] = {4, 2048, 128, 4096, 12};
        for (unsigned n = 0; offset < bytes.size(); ++n)
        {
            const unsigned left = bytes.size() - offset;
            const unsigned count = sizes[n % 5] < left ? sizes[n % 5] : left;
            feed(bytes.data() + offset, count); offset += count;
        }
        check(bytes == original, "postmix input unchanged");
        check(audio.take(event) && event.kind == AgentAudioEvents::Event::Window && event.nonzero, "first digital-nonzero window");
        check(event.pcmSequence == 1 && std::memcmp(event.pcm, original.data() + window * 0, window) == 0, "exact immutable PCM and sequence");
        check(audio.take(event) && event.kind == AgentAudioEvents::Event::Window && !event.nonzero, "second exact digital-zero window");
        check(event.pcmSequence == 2 && std::memcmp(event.pcm, original.data() + window * 1, window) == 0, "exact immutable PCM and sequence");
        check(audio.take(event) && event.kind == AgentAudioEvents::Event::Window && event.nonzero, "third digital-nonzero window");
        check(event.pcmSequence == 3 && std::memcmp(event.pcm, original.data() + window * 2, window) == 0, "exact immutable PCM and sequence");
        check(!audio.take(event), "partial window not emitted");
        const unsigned before = detaches;
        audio.beforeAudioReset(true);
        check(!callback && detaches == before + 1, "reset synchronously detaches own hook");
        audio.publishCatalogSupported(true); audio.poll(true);
        check(!callback, "reset does not revive epoch");
    }
    {
        AgentAudioEvents audio; AgentAudioEvents::Event event;
        attach(audio); feedWindows(1); feedWindows(2, true);
        Uint8 partial[4] = {}; feed(partial, sizeof(partial));
        audio.close(); audio.close();
        for (unsigned n = 0; n < 2; ++n)
            check(audio.take(event) && event.kind == AgentAudioEvents::Event::Window && event.nonzero,
                "completed windows survive terminal detach");
        check(audio.take(event) && event.kind == AgentAudioEvents::Event::Status &&
            event.state == AgentAudioEvents::Event::Unavailable && event.partialWindowDiscarded,
            "terminal status follows windows and discloses partial tail loss");
        check(audio.status() == AgentAudioEvents::Event::Unavailable, "current unavailable status survives drain");
        check(!audio.take(event), "repeated close does not duplicate terminal status");
    }
    {
        AgentAudioEvents audio; AgentAudioEvents::Event event;
        audio.publishCatalogSupported(false); audio.poll(true);
        check(audio.take(event) && event.kind == AgentAudioEvents::Event::Status &&
            event.state == AgentAudioEvents::Event::Pending, "unsupported profile initially pending");
        check(audio.take(event) && event.kind == AgentAudioEvents::Event::Status &&
            event.state == AgentAudioEvents::Event::Unavailable && !event.partialWindowDiscarded,
            "unsupported profile terminal unavailable without invented PCM");
        check(!audio.take(event), "unsupported status emitted once");
    }
    {
        AgentAudioEvents audio; AgentAudioEvents::Event event;
        check(audio.status() == AgentAudioEvents::Event::Pending, "current initial pending status");
        attach(audio); feedWindows(1); feedWindows(AgentAudioEvents::RingCapacity + 2);
        audio.close();
        for (unsigned n = 0; n < AgentAudioEvents::RingCapacity; ++n)
            check(audio.take(event) && event.kind == AgentAudioEvents::Event::Window,
                "completed retained windows precede terminal overflow gap");
        check(audio.take(event) && event.kind == AgentAudioEvents::Event::Gap && event.droppedWindows == 2,
            "terminal overflow gap retains exact loss count");
        check(audio.take(event) && event.kind == AgentAudioEvents::Event::Status &&
            event.state == AgentAudioEvents::Event::Unavailable && !event.partialWindowDiscarded,
            "terminal status follows exact complete-window gap");
        check(!audio.take(event), "terminal empty ring has no invented gap");
    }
    {
        AgentAudioEvents audio; AgentAudioEvents::Event event;
        attach(audio); feedWindows(1); feedWindows(AgentAudioEvents::RingCapacity + 7);
        unsigned seen = 0;
        while (seen < AgentAudioEvents::RingCapacity)
        { check(audio.take(event) && event.kind == AgentAudioEvents::Event::Window, "retained window before gap"); ++seen; }
        check(audio.take(event) && event.kind == AgentAudioEvents::Event::Gap && event.droppedWindows == 7,
            "overflow reported without requiring a later callback");
        check(!audio.take(event), "overflow gap emitted once");
        feedWindows(1, true);
        check(audio.take(event) && event.kind == AgentAudioEvents::Event::Window && event.nonzero, "capture continues after gap");
        check(event.pcmSequence == AgentAudioEvents::RingCapacity + 8 && event.pcm[AgentAudioEvents::WindowBytes-1] == 0x80, "sequence spans dropped windows and PCM retained");
        check(!audio.take(event), "no false gap after recovery");
    }
    {
        AgentAudioEvents audio; AgentAudioEvents::Event event;
        attach(audio); feedWindows(1); feedWindows(AgentAudioEvents::RingCapacity + 3);
        check(audio.take(event), "free one ring entry");
        feedWindows(1, true);
        for (unsigned n = 1; n < AgentAudioEvents::RingCapacity; ++n)
            check(audio.take(event) && event.kind == AgentAudioEvents::Event::Window, "old ring windows preserve order");
        check(audio.take(event) && event.kind == AgentAudioEvents::Event::Gap && event.droppedWindows == 3,
            "interior overflow gap precedes recovered window");
        check(audio.take(event) && event.kind == AgentAudioEvents::Event::Window && event.nonzero, "pending recovered window retained");
        check(!audio.take(event), "no repeated interior gap");
    }
    {
        AgentAudioEvents audio; AgentAudioEvents::Event event;
        attach(audio); feedWindows(1);
        std::atomic<bool> done(false);
        std::thread producer([&] { feedWindows(5000, true); done.store(true, std::memory_order_release); });
        std::uint64_t windows = 0, dropped = 0;
        while (!done.load(std::memory_order_acquire))
        {
            if (!audio.take(event)) { std::this_thread::yield(); continue; }
            if (event.kind == AgentAudioEvents::Event::Gap) dropped += event.droppedWindows;
            else { check(event.nonzero, "concurrent summary is initialized"); ++windows; }
        }
        producer.join();
        while (audio.take(event))
        {
            if (event.kind == AgentAudioEvents::Event::Gap) dropped += event.droppedWindows;
            else { check(event.nonzero, "tail summary is initialized"); ++windows; }
        }
        check(windows + dropped == 5000, "threaded windows plus exact losses conserve completed PCM");
    }
    for (int bad : {0, 1, 3, -4, AgentAudioEvents::MaxCallbackBytes + 4})
    {
        AgentAudioEvents audio; attach(audio); Uint8 bytes[4] = {};
        feed(bytes, bad); audio.poll(true);
        check(!callback, "malformed callback closes on main poll");
    }
    {
        AgentAudioEvents audio; attach(audio); feed(nullptr, 4); audio.poll(true);
        check(!callback, "null callback input fails closed");
    }
    {
        AgentAudioEvents audio; audio.publishCatalogSupported(false); audio.poll(true);
        check(!callback, "unsupported catalog unavailable");
    }
    for (unsigned mismatch = 0; mismatch != 4; ++mismatch)
    {
        AgentAudioEvents audio;
        if (mismatch == 0) rate = 44100;
        if (mismatch == 1) format = AUDIO_S8;
        if (mismatch == 2) channels = 1;
        if (mismatch == 3) version.patch = 13;
        audio.publishCatalogSupported(true); audio.poll(true);
        check(!callback, "unqualified mixer profile unavailable");
        rate = 22050; format = AUDIO_S16LSB; channels = 2; version.patch = 12;
    }
    {
        AgentAudioEvents audio; attach(audio); qualified = false; audio.poll(true);
        check(!callback, "diagnostic rejection detaches"); qualified = true;
    }
    {
        AgentAudioEvents audio; audio.publishCatalogSupported(true); audio.poll(false);
        check(!callback, "inactive controller never attaches");
    }
    {
        unsetenv("OPENXCOM_AGENT_CONTROL"); AgentAudioEvents audio;
        audio.publishCatalogSupported(true); audio.poll(true);
        check(!callback, "runtime opt-in required");
    }
    check(attaches == detaches, "only owned hooks detached exactly once");
    std::cout << "AgentAudioEventsTest PASS " << checks << " checks\n";
}
