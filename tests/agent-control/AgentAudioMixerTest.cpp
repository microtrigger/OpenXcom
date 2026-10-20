// SPDX-License-Identifier: GPL-3.0-or-later
// Real SDL dummy mixer with an independent final-buffer oracle; no game/GUI.
#include "../../src/Observer/AgentAudioEvents.h"
#include <SDL_mixer.h>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
using OpenXcom::Observer::AgentAudioEvents;
namespace {
void (*production)(void *, Uint8 *, int) = nullptr;
void *productionContext = nullptr;
std::atomic<unsigned> produced(0), errors(0);
bool expected[64], skip = true, nonzero = false;
unsigned frames = 0, checks = 0;
Uint8 before[AgentAudioEvents::MaxCallbackBytes];
void check(bool value, const char *message)
{
    ++checks;
    if (!value) { std::cerr << "FAIL " << message << ": " << SDL_GetError() << '\n'; std::exit(1); }
}
void oracle(void *, Uint8 *stream, int len)
{
    if (!stream || len <= 0 || static_cast<unsigned>(len) > sizeof(before) || len % 4)
    { errors.store(1); return; }
    std::memcpy(before, stream, len);
    if (skip) skip = false;
    else
    {
        for (int i = 0; i < len; i += 4)
        {
            const unsigned left = unsigned(stream[i]) | unsigned(stream[i + 1]) << 8;
            const unsigned right = unsigned(stream[i + 2]) | unsigned(stream[i + 3]) << 8;
            nonzero = nonzero || left || right;
            if (++frames != 2205) continue;
            const unsigned index = produced.load(std::memory_order_relaxed);
            if (index >= 64) { errors.store(2); return; }
            expected[index] = nonzero;
            produced.store(index + 1, std::memory_order_release);
            frames = 0; nonzero = false;
        }
    }
    production(productionContext, stream, len);
    if (std::memcmp(before, stream, len)) errors.store(3);
}
void zeroEffect(int, void *stream, int len, void *) { std::memset(stream, 0, len); }
void paused()
{
    SDL_PauseAudio(1); SDL_LockAudio(); SDL_UnlockAudio();
}
void scenario(Mix_Chunk *chunk, unsigned kind)
{
    paused(); Mix_HaltChannel(-1); Mix_UnregisterAllEffects(MIX_CHANNEL_POST);
    Mix_AllocateChannels(kind == 0 ? 0 : 4);
    Mix_Volume(-1, kind == 2 ? 0 : MIX_MAX_VOLUME);
    produced.store(0); errors.store(0); skip = true; nonzero = false; frames = 0;
    if (kind == 0)
        check(Mix_PlayChannel(-1, chunk, -1) == -1, "failed play attempt supplies no evidence");
    else check(Mix_PlayChannel(0, chunk, -1) == 0, "synthetic chunk accepted");
    if (kind == 3) check(Mix_RegisterEffect(MIX_CHANNEL_POST, zeroEffect, nullptr, nullptr) != 0,
        "final post-effect installed");
    AgentAudioEvents audio; audio.publishCatalogSupported(true); audio.poll(true);
    check(production != nullptr, "production postmix owner attached");
    AgentAudioEvents::Event status;
    check(audio.take(status) && status.kind == AgentAudioEvents::Event::Status &&
        status.state == AgentAudioEvents::Event::Pending, "pending lifecycle status");
    check(audio.take(status) && status.kind == AgentAudioEvents::Event::Status &&
        status.state == AgentAudioEvents::Event::Active, "active lifecycle status");
    SDL_PauseAudio(0);
    const Uint32 start = SDL_GetTicks();
    unsigned consumed = 0;
    while (consumed < 4 && SDL_GetTicks() - start < 3000)
    {
        AgentAudioEvents::Event event;
        while (audio.take(event))
        {
            check(event.kind == AgentAudioEvents::Event::Window, "no unexpected summary loss");
            check(consumed < produced.load(std::memory_order_acquire), "oracle observed same completed window");
            check(event.nonzero == expected[consumed++], "production summary equals independent final-output oracle");
            check(event.nonzero == (kind == 1), "post-volume/post-effect output fact");
        }
        SDL_Delay(1);
    }
    paused();
    check(consumed >= 4, "four continuous 100 ms output windows");
    check(errors.load() == 0, "production callback leaves entire final output buffer unchanged");
    audio.close();
    check(production == nullptr, "production hook detached");
    bool terminal = false;
    while (audio.take(status))
    {
        if (status.kind != AgentAudioEvents::Event::Status) continue;
        check(status.state == AgentAudioEvents::Event::Unavailable, "terminal lifecycle status");
        terminal = true;
    }
    check(terminal, "detachment does not silently stop history");
}
}
extern "C" void __real_Mix_SetPostMix(void (*)(void *, Uint8 *, int), void *);
extern "C" void __wrap_Mix_SetPostMix(void (*fn)(void *, Uint8 *, int), void *data)
{
    // Every test registration/detachment runs after pause and empty drain.
    production = fn; productionContext = data;
    __real_Mix_SetPostMix(fn ? oracle : nullptr, nullptr);
}
namespace OpenXcom { namespace Observer {
bool diagnosticProfileQualified() { return true; }
} }
int main()
{
    setenv("SDL_AUDIODRIVER", "dummy", 1); setenv("OPENXCOM_AGENT_CONTROL", "1", 1);
    check(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) == 0, "dummy audio initialization");
    check(Mix_OpenAudio(22050, AUDIO_S16LSB, 2, 512) == 0, "qualified mixer initialization");
    Mix_AllocateChannels(4); paused();
    Uint8 pcm[22050 * 4] = {};
    for (unsigned i = 0; i < sizeof(pcm); i += 2) pcm[i + 1] = 1;
    Mix_Chunk *chunk = Mix_QuickLoad_RAW(pcm, sizeof(pcm)); check(chunk != nullptr, "synthetic PCM chunk");
    for (unsigned kind = 0; kind != 4; ++kind) scenario(chunk, kind);
    Mix_HaltChannel(-1); Mix_UnregisterAllEffects(MIX_CHANNEL_POST); Mix_FreeChunk(chunk);
    Mix_CloseAudio(); SDL_Quit();
    std::cout << "AgentAudioMixerTest PASS " << checks << " checks; 4 output profiles, no physical audibility claim\n";
}
