// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// Free software under GNU GPL version 3 or later; without warranty.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
#include "../diagnostic-isolation/fixture_options.h"
#include "../../src/Observer/MixedAudioObserver.h"
#include "../../src/Observer/MixedAudioMusicProfile.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <vector>
using namespace OpenXcom::Observer;
namespace {
bool failAllocation = false;
void (*callback)(void *, Uint8 *, int) = 0;
void *callbackContext = 0;
int rate = 22050, channels = 2;
Uint16 format = AUDIO_S16LSB;
SDL_version version = {1, 2, 12};
unsigned detaches = 0;
}
void *operator new(std::size_t size)
{
    if (failAllocation) throw std::bad_alloc();
    if (void *p = std::malloc(size)) return p;
    throw std::bad_alloc();
}
// Prevent GCC's interprocedural mismatched-new/delete false positive for this
// deliberate malloc-backed replacement allocation pair in optimized tests.
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete(void *p) noexcept { std::free(p); }
extern "C" {
const SDL_version *Mix_Linked_Version() { return &version; }
int Mix_QuerySpec(int *r, Uint16 *f, int *c) { *r = rate; *f = format; *c = channels; return 1; }
void Mix_SetPostMix(void (*cb)(void *, Uint8 *, int), void *context)
{ callback = cb; callbackContext = context; if (!cb) ++detaches; }
}
namespace {
std::string receive(int fd)
{
    char buf[102400];
    const ssize_t n = recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
    return n <= 0 ? "" : std::string(buf, n);
}
void sendRequest(int fd, const std::string &s)
{ assert(send(fd, s.data(), s.size(), MSG_NOSIGNAL) == static_cast<ssize_t>(s.size())); }
std::string field(const std::string &s, const std::string &key)
{
    const std::string tag = "\"" + key + "\":\"";
    const std::size_t start = s.find(tag);
    assert(start != std::string::npos);
    const std::size_t value = start + tag.size();
    return s.substr(value, s.find('"', value) - value);
}
struct Fixture
{
    int peer;
    MixedAudioObserver *observer;
    std::string hello, token, request;
    Fixture(bool ready = true, bool smallBuffer = false) : observer(0)
    {
        int pair[2]; assert(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair) == 0);
        peer = pair[0];
        if (smallBuffer) { int bytes = 4096; assert(setsockopt(pair[1], SOL_SOCKET, SO_SNDBUF, &bytes, sizeof(bytes)) == 0); }
        observer = new MixedAudioObserver(std::to_string(pair[1]).c_str());
        assert(observer->enabled());
        observer->poll(); assert(receive(peer).empty()); // BOOTSTRAP has no hook/hello.
        assert(!callback);
        if (ready) qualify();
    }
    void qualify()
    {
        observer->publishCatalogSupported(true); observer->poll();
        hello = receive(peer);
        assert(hello.size() == std::strlen("OPENXCOM-MIXED-AUDIO/1 ") + 64);
        const std::string prefix = "OPENXCOM-MIXED-AUDIO/1 ";
        assert(hello.compare(0, prefix.size(), prefix) == 0);
        token = hello.substr(prefix.size());
        request = token + std::string(32, 'c');
        assert(callback);
    }
    void arm() { sendRequest(peer, request); observer->poll(); assert(observer->enabled()); }
    void capture(unsigned char byte = 0x5a)
    {
        std::vector<Uint8> block(1024, byte);
        callback(callbackContext, block.data(), block.size()); // Admission skipped.
        for (unsigned i = 0; i < 44; ++i) callback(callbackContext, block.data(), block.size());
    }
    ~Fixture() { delete observer; if (peer >= 0) ::close(peer); }
};
}
int main(int argc, char **argv)
{
    qualifyNormalDiagnostics();
    (void)argv;
    assert(!softwareMixedMusicType(MUS_NONE));
    assert(!softwareMixedMusicType(MUS_MID));
    assert(!softwareMixedMusicType(MUS_CMD));
    assert(!softwareMixedMusicType(static_cast<Mix_MusicType>(99)));
    for (auto type : {MUS_WAV, MUS_MOD, MUS_OGG, MUS_MP3, MUS_MP3_MAD, MUS_FLAC, MUS_MODPLUG})
        assert(softwareMixedMusicType(type));
    std::string normalized;
    for (unsigned label = 0; label < 2; ++label)
    {
        Fixture f(false);
        f.observer->publishCatalogSupported(true);
        f.observer->beforeAudioReset(true); f.observer->poll();
        assert(f.observer->enabled() && !callback);
        f.qualify(); f.arm(); f.capture(); f.observer->poll();
        std::string result = receive(f.peer);
        const std::string pcm = field(result, "pcm_hex");
        assert(result.size() < 102400 && pcm.size() == 88200);
        for (unsigned i = 0; i < pcm.size(); i += 2) assert(pcm.substr(i, 2) == "5a");
        assert(field(result, "schema") == "experimental-mixed-audio/1");
        assert(field(result, "epoch") == f.token.substr(0, 32));
        assert(field(result, "challenge") == std::string(32, 'c'));
        assert(field(result, "next_token") != f.token.substr(32));
        assert(result.find("\"frames\":11025") != std::string::npos);
        assert(result.find("timestamp") == std::string::npos);
        // Different private source labels are absent from the native capture API.
        // Normalize only independent transport tokens before comparing payloads.
        for (auto key : {"epoch", "challenge", "next_token"})
        {
            const std::string value = field(result, key);
            result.replace(result.find(value), value.size(), std::string(32, '0'));
        }
        if (label) assert(result == normalized); else normalized = result;
        // Returning READY to IDLE does not replay the old clip spontaneously.
        f.observer->poll(); assert(receive(f.peer).empty());
    }
    for (unsigned variant = 0; variant < 6; ++variant)
    {
        Fixture f;
        std::string bad = f.request;
        if (variant == 0) bad.resize(95);
        if (variant == 1) bad += 'a';
        if (variant == 2) bad.assign(120000, 'a');
        if (variant == 3) bad[0] = bad[0] == 'a' ? 'b' : 'a';
        if (variant == 4) bad[32] = bad[32] == 'a' ? 'b' : 'a';
        if (variant == 5) bad[95] = 'G';
        sendRequest(f.peer, bad); f.observer->poll();
        assert(!f.observer->enabled() && !callback);
    }
    {
        Fixture f; sendRequest(f.peer, f.request); sendRequest(f.peer, f.request);
        f.observer->poll(); assert(!f.observer->enabled());
    }
    {
        Fixture f; f.arm(); sendRequest(f.peer, f.request); f.observer->poll();
        assert(!f.observer->enabled()); // One in flight, including queued duplicates.
    }
    {
        Fixture f; f.arm(); f.capture(); f.observer->poll();
        const std::string reply = receive(f.peer);
        f.request = f.token.substr(0, 32) + field(reply, "next_token") + std::string(32, 'd');
        sendRequest(f.peer, f.request); f.observer->poll();
        assert(!f.observer->enabled()); // Even valid fresh token is rate limited.
    }
    {
        Fixture f; f.arm(); f.capture(); f.observer->poll(); receive(f.peer);
        std::this_thread::sleep_for(std::chrono::milliseconds(1010));
        sendRequest(f.peer, f.request); f.observer->poll();
        assert(!f.observer->enabled()); // Consumed admission token cannot replay.
    }
    {
        Fixture f; f.arm(); f.capture(); f.observer->poll();
        const std::string reply = receive(f.peer);
        f.request = f.token.substr(0, 32) + field(reply, "next_token") + std::string(32, 'd');
        std::this_thread::sleep_for(std::chrono::milliseconds(1010));
        f.arm(); f.capture(0); f.observer->poll();
        assert(field(receive(f.peer), "pcm_hex") == std::string(88200, '0'));
    }
    {
        Fixture f; f.arm(); f.capture();
        std::this_thread::sleep_for(std::chrono::milliseconds(2010));
        f.observer->poll(); assert(!f.observer->enabled() && receive(f.peer).empty());
    }
    for (bool admitted : {false, true})
    {
        Fixture f; if (admitted) f.arm();
        const unsigned before = detaches;
        f.observer->beforeAudioReset();
        assert(!f.observer->enabled() && !callback && detaches == before + 1);
    }
    {
        Fixture f; f.arm(); ::close(f.peer); f.peer = -1; f.observer->poll();
        assert(!f.observer->enabled() && !callback);
    }
    {
        Fixture f(false); sendRequest(f.peer, std::string(96, 'a')); f.observer->poll();
        assert(!f.observer->enabled()); // No requests before qualified hello.
    }
    {
        Fixture f(false); f.observer->publishCatalogSupported(false); f.observer->poll();
        assert(!f.observer->enabled() && !callback);
    }
    for (unsigned variant = 0; variant < 4; ++variant)
    {
        Fixture f(false);
        if (variant == 0) rate = 44100;
        if (variant == 1) channels = 1;
        if (variant == 2) format = AUDIO_U8;
        if (variant == 3) version.patch = 11;
        f.observer->publishCatalogSupported(true); f.observer->poll();
        assert(!f.observer->enabled() && !callback);
        rate = 22050; channels = 2; format = AUDIO_S16LSB; version.patch = 12;
    }
    {
        Fixture f(false); f.observer->publishCatalogSupported(true);
        failAllocation = true; f.observer->poll(); failAllocation = false;
        assert(!f.observer->enabled() && !callback);
    }
    {
        Fixture f; f.arm(); f.capture();
        failAllocation = true; f.observer->poll(); failAllocation = false;
        assert(!f.observer->enabled() && !callback);
    }
    {
        Fixture f; f.arm();
        Uint8 bytes[44100] = {};
        failAllocation = true;
        callback(callbackContext, bytes, sizeof(bytes));
        callback(callbackContext, bytes, sizeof(bytes));
        failAllocation = false;
        f.observer->poll(); assert(field(receive(f.peer), "pcm_hex") == std::string(88200, '0'));
    }
    {
        Fixture f(true, true); f.arm(); f.capture(); f.observer->poll();
        assert(!f.observer->enabled() && !callback && receive(f.peer).empty());
    }
    {
        Fixture f; f.arm(); Uint8 bytes[4] = {};
        callback(callbackContext, bytes, 3); f.observer->poll();
        assert(!f.observer->enabled() && !callback);
    }
    {
        Fixture f; f.arm();
        std::this_thread::sleep_for(std::chrono::milliseconds(2010));
        f.observer->poll(); assert(!f.observer->enabled() && !callback);
    }
    for (int type : {SOCK_STREAM, SOCK_DGRAM})
    {
        int pair[2]; assert(socketpair(AF_UNIX, type, 0, pair) == 0);
        MixedAudioObserver rejected(std::to_string(pair[1]).c_str());
        assert(!rejected.enabled()); ::close(pair[0]); ::close(pair[1]);
    }
    if (argc > 1)
    {
        Fixture f(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(15000));
        f.observer->beforeAudioReset(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(15010));
        f.observer->poll(); assert(!f.observer->enabled() && !callback);
    }
    std::cout << "PASS protocol: bootstrap, profile/type guard, exact bytes, paired labels, malformed/duplicate/replay/rate/deadline/disconnect/reset, allocation failures" << (argc > 1 ? ", 30s startup deadline" : "") << "\n";
}
