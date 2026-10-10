// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// Free software under GNU GPL version 3 or later; without warranty.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
#include "../diagnostic-isolation/fixture_options.h"
#include "../../src/Observer/MixedAudioObserver.h"
#include "../../src/Engine/Sound.h"
#include "../../src/Engine/Options.h"
#include <SDL_mixer.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
using namespace OpenXcom;
using namespace OpenXcom::Observer;
// Link the production Sound implementation; these fixture settings replace the
// unrelated application option parser and timestamp logger, not Sound::play.
namespace OpenXcom { namespace Options { bool mute = false; bool StereoSound = true; } }
namespace {
void (*productionCallback)(void *, Uint8 *, int) = 0;
void *productionContext = 0;
std::atomic<unsigned> phase(0);
unsigned char reference[MixedAudioSlot::Bytes];
unsigned char skipped[4096];
unsigned skippedLen = 0;
std::size_t offset = 0;
std::atomic<bool> modified(false);
void oracle(void *, Uint8 *stream, int len)
{
    assert(len > 0 && len <= 4096 && len % 4 == 0);
    unsigned char original[4096];
    std::memcpy(original, stream, len);
    unsigned state = phase.load(std::memory_order_acquire);
    if (state == 1)
    {
        std::memcpy(skipped, stream, len); skippedLen = len;
        offset = 0; phase.store(2, std::memory_order_release);
    }
    else if (state == 2)
    {
        const std::size_t take = std::min<std::size_t>(len, sizeof(reference) - offset);
        std::memcpy(reference + offset, stream, take); offset += take;
        if (offset == sizeof(reference)) phase.store(3, std::memory_order_release);
    }
    productionCallback(productionContext, stream, len);
    if (std::memcmp(original, stream, len)) modified.store(true, std::memory_order_release);
}
void le16(unsigned char *p, int value)
{ p[0] = value & 255; p[1] = (value >> 8) & 255; }
void le32(unsigned char *p, unsigned value)
{ for (unsigned i = 0; i < 4; ++i) p[i] = (value >> (i * 8)) & 255; }
std::vector<unsigned char> wave(const std::vector<unsigned char> &raw)
{
    std::vector<unsigned char> wav(44 + raw.size());
    std::memcpy(wav.data(), "RIFF", 4); le32(wav.data() + 4, wav.size() - 8);
    std::memcpy(wav.data() + 8, "WAVEfmt ", 8); le32(wav.data() + 16, 16);
    le16(wav.data() + 20, 1); le16(wav.data() + 22, 2); le32(wav.data() + 24, 22050);
    le32(wav.data() + 28, 88200); le16(wav.data() + 32, 4); le16(wav.data() + 34, 16);
    std::memcpy(wav.data() + 36, "data", 4); le32(wav.data() + 40, raw.size());
    std::copy(raw.begin(), raw.end(), wav.begin() + 44); return wav;
}
std::string receive(int fd)
{
    char buf[102400]; ssize_t n = recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
    return n <= 0 ? "" : std::string(buf, n);
}
std::vector<unsigned char> capture()
{
    int pair[2]; assert(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair) == 0);
    MixedAudioObserver observer(std::to_string(pair[1]).c_str());
    observer.publishCatalogSupported(true); observer.poll();
    std::string hello = receive(pair[0]);
    const std::string prefix = "OPENXCOM-MIXED-AUDIO/1 ";
    assert(hello.size() == prefix.size() + 64);
    const std::string request = hello.substr(prefix.size()) + std::string(32, 'a');
    assert(send(pair[0], request.data(), request.size(), MSG_NOSIGNAL) == 96);
    // Test-only admission barrier. The production admission does not lock SDL.
    // This makes the oracle's discarded callback exactly the production one.
    SDL_LockAudio();
    phase.store(1, std::memory_order_release); modified.store(false, std::memory_order_release);
    observer.poll();
    SDL_UnlockAudio();
    std::string reply;
    for (unsigned i = 0; i < 1800 && reply.empty(); ++i)
    { observer.poll(); reply = receive(pair[0]); SDL_Delay(1); }
    assert(!reply.empty());
    assert(phase.load(std::memory_order_acquire) == 3);
    assert(!modified.load(std::memory_order_acquire));
    const std::string tag = "\"pcm_hex\":\"";
    const std::size_t start = reply.find(tag) + tag.size();
    const std::string hex = reply.substr(start, reply.find('"', start) - start);
    assert(hex.size() == MixedAudioSlot::Bytes * 2);
    std::vector<unsigned char> result(MixedAudioSlot::Bytes);
    auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    for (unsigned i = 0; i < result.size(); ++i) result[i] = digit(hex[i * 2]) * 16 + digit(hex[i * 2 + 1]);
    assert(std::equal(result.begin(), result.end(), reference));
    // Positive sensitivity controls: comparator rejects changed byte/stereo order.
    std::vector<unsigned char> corrupted(result); corrupted[501] ^= 1;
    assert(!std::equal(corrupted.begin(), corrupted.end(), reference));
    observer.close(); ::close(pair[0]);
    phase.store(0, std::memory_order_release);
    return result;
}
bool silent(const std::vector<unsigned char> &pcm)
{ return std::all_of(pcm.begin(), pcm.end(), [](unsigned char b) { return b == 0; }); }
void effect(int, void *stream, int len, void *)
{
    unsigned char *p = static_cast<unsigned char *>(stream);
    for (int i = 0; i < len; i += 2)
    {
        const Sint16 sample = static_cast<Sint16>(p[i] | (p[i + 1] << 8));
        le16(p + i, sample / 2 + 3);
    }
}
unsigned musicFrame = 0;
void music(void *, Uint8 *stream, int len)
{
    // Non-periodic within each 500ms window so admission-block shifts are visible.
    for (int i = 0; i < len; i += 4, ++musicFrame)
    { le16(stream + i, musicFrame % 10000 - 5000); le16(stream + i + 2, 2000 - musicFrame % 4000); }
}
}
extern "C" {
void __real_Mix_SetPostMix(void (*)(void *, Uint8 *, int), void *);
void __wrap_Mix_SetPostMix(void (*callback)(void *, Uint8 *, int), void *context)
{
    if (callback)
    {
        assert(!productionCallback);
        productionCallback = callback; productionContext = context;
        __real_Mix_SetPostMix(oracle, NULL);
    }
    else
    {
        __real_Mix_SetPostMix(NULL, NULL); // Synchronized before fixture mutation.
        productionCallback = 0; productionContext = 0;
    }
}
}
int main()
{
    qualifyNormalDiagnostics();
    assert(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) == 0);
    assert(Mix_OpenAudio(22050, AUDIO_S16LSB, 2, 512) == 0);
    Mix_AllocateChannels(8);
    assert(silent(capture()));
    std::vector<unsigned char> raw(22050 * 4);
    for (unsigned i = 0; i < raw.size(); i += 4) { le16(raw.data() + i, 4000); le16(raw.data() + i + 2, -2000); }
    Mix_Chunk *chunk = Mix_QuickLoad_RAW(raw.data(), raw.size()); assert(chunk);
    Mix_AllocateChannels(0);
    assert(Mix_PlayChannel(-1, chunk, 0) == -1);
    assert(silent(capture())); // Failed allocation has no fabricated waveform.
    Mix_AllocateChannels(8);
    assert(Mix_PlayChannel(0, chunk, -1) == 0);
    const std::vector<unsigned char> firstSource = capture();
    assert(!silent(firstSource));
    Mix_HaltChannel(0);
    Mix_Chunk *otherSource = Mix_QuickLoad_RAW(raw.data(), raw.size()); assert(otherSource);
    assert(Mix_PlayChannel(2, otherSource, -1) == 2);
    assert(capture() == firstSource); // Different source object/channel, identical final PCM.
    Mix_HaltChannel(2); Mix_FreeChunk(otherSource);
    assert(Mix_PlayChannel(0, chunk, -1) == 0);
    Mix_Volume(-1, 0); assert(silent(capture())); Mix_Volume(-1, MIX_MAX_VOLUME);
    assert(Mix_SetPanning(0, 255, 0));
    const std::vector<unsigned char> leftOnly = capture();
    assert(Mix_SetPanning(0, 0, 255));
    const std::vector<unsigned char> rightOnly = capture();
    assert(leftOnly != rightOnly && !silent(leftOnly) && !silent(rightOnly));
    assert(Mix_PlayChannel(1, chunk, -1) == 1);
    assert(Mix_SetPanning(0, 255, 0)); assert(Mix_SetPanning(1, 0, 160));
    Mix_HookMusic(music, NULL);
    assert(Mix_RegisterEffect(MIX_CHANNEL_POST, effect, NULL, NULL));
    const std::vector<unsigned char> mixed = capture();
    assert(!silent(mixed));
    assert(!std::equal(mixed.begin(), mixed.end(), raw.begin())); // Pre-effects/source control.
    std::vector<unsigned char> swapped(mixed);
    for (unsigned i = 0; i < swapped.size(); i += 4)
    { std::swap(swapped[i], swapped[i + 2]); std::swap(swapped[i + 1], swapped[i + 3]); }
    assert(swapped != mixed); // Stereo swap positive control.
    std::vector<unsigned char> early(skipped, skipped + skippedLen);
    early.insert(early.end(), mixed.begin(), mixed.end() - skippedLen);
    assert(early != mixed); // Including the discarded callback is detectable.
    Mix_UnregisterAllEffects(MIX_CHANNEL_POST); Mix_HookMusic(NULL, NULL);
    Mix_HaltChannel(-1); Mix_FreeChunk(chunk);
    {
        Sound sound;
        const std::vector<unsigned char> wav = wave(raw);
        sound.load(wav.data(), wav.size());
        Options::mute = true; sound.play(); assert(silent(capture()));
        Options::mute = false; sound.loop(); assert(Mix_Playing(3));
        Options::mute = true; Sound::stop(); sound.play();
        assert(Mix_Playing(3) && !silent(capture())); // Actual mute only gates later playback.
        Options::mute = false; Sound::stop(); assert(silent(capture()));
    }
    Mix_CloseAudio(); SDL_Quit();
    std::cout << "PASS SDL dummy mixer: exact final-PCM oracle, true silence, volume zero, failed channel allocation, pan-only change, overlapping/panned sources + custom music + posteffect, real Sound mute with/without playing channel; corruption/early-admission/stereo/pre-effect controls\n";
}
