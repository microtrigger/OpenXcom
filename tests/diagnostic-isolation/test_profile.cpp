// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// Free software under GNU GPL version 3 or later; without warranty.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
#include "fixture_options.h"
#include "../../src/Observer/PresentationObserver.h"
#include "../../src/Observer/MixedAudioObserver.h"
#include "../../src/Observer/CursorFileObserver.h"
#include "../../src/Observer/HudFileObserver.h"
#include <SDL_mixer.h>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
using namespace OpenXcom;
using namespace OpenXcom::Observer;
namespace {
void (*callback)(void *, Uint8 *, int) = 0;
void *context = 0;
SDL_version mixerVersion = {1, 2, 12};
const char sentinel[] = "PRIVATE-DIAGNOSTIC-SENTINEL";
std::string receive(int fd)
{
    char bytes[102401];
    const ssize_t size = recv(fd, bytes, sizeof(bytes), MSG_DONTWAIT);
    return size > 0 ? std::string(bytes, size) : "";
}
void request(int fd, const std::string &hello)
{
    assert(hello.size() >= 64);
    const std::string bytes = hello.substr(hello.size() - 64) + std::string(32, 'a');
    assert(send(fd, bytes.data(), bytes.size(), MSG_NOSIGNAL) == 96);
}
void disqualify(unsigned variant)
{
    if (variant == 0) Options::debug = true;
    if (variant == 1) Options::debugUi = true;
    if (variant == 2) Options::traceAI = true;
    if (variant == 3) Options::verboseLogging = true;
    if (variant == 4) Logger::reportingLevel() = LOG_DEBUG;
    if (variant == 5) Logger::reportingLevel() = LOG_VERBOSE;
}
PixelCrop crop()
{
    PixelCrop c; c.screenWidth = 320; c.screenHeight = 200;
    c.x = c.y = 0; c.width = c.height = 1; c.rgb = {1, 2, 3}; return c;
}
std::string contents(const std::string &path)
{
    std::ifstream file(path); std::ostringstream out; out << file.rdbuf(); return out.str();
}
void lockReleased(const std::string &path)
{
    const int fd = open(path.c_str(), O_RDWR); assert(fd >= 0);
    assert(flock(fd, LOCK_EX | LOCK_NB) == 0); ::close(fd);
}
void diagnostics()
{
    assert(std::fwrite(sentinel, 1, sizeof(sentinel), stdout) == sizeof(sentinel));
    assert(std::fwrite(sentinel, 1, sizeof(sentinel), stderr) == sizeof(sentinel));
    Logger::logFile() = "/dev/null/observer-log"; // ENOTDIR forces the real fallback.
    Log(LOG_INFO) << sentinel;
    unsigned char bytes[8192]; std::fill(bytes, bytes + sizeof(bytes), 0xff);
    for (unsigned i = 0; i < 256; ++i)
    {
        assert(std::fwrite(bytes, 1, sizeof(bytes), stdout) == sizeof(bytes));
        assert(std::fwrite(bytes, 1, sizeof(bytes), stderr) == sizeof(bytes));
    }
    std::fflush(stdout); std::fflush(stderr);
}
int loggerChild()
{
    qualifyNormalDiagnostics();
    diagnostics();
    PresentationObserver producer(std::getenv("OPENXCOM_PRESENTATION_FD"));
    if (!producer.enabled()) return 1;
    for (unsigned i = 0; i < 2000 && !producer.pending(); ++i)
    { producer.beginFrame(); usleep(1000); }
    if (!producer.pending()) return 1;
    diagnostics(); producer.presented(crop()); diagnostics(); return 0;
}
}
extern "C" {
const SDL_version *Mix_Linked_Version() { return &mixerVersion; }
int Mix_QuerySpec(int *rate, Uint16 *format, int *channels)
{ *rate = 22050; *format = AUDIO_S16LSB; *channels = 2; return 1; }
void Mix_SetPostMix(void (*cb)(void *, Uint8 *, int), void *ctx) { callback = cb; context = ctx; }
}
int main(int argc, char **argv)
{
    if (argc == 2 && std::string(argv[1]) == "--logger-child") return loggerChild();
    assert(argc == 3);
    const std::string stage(argv[1]); const unsigned variant = std::strtoul(argv[2], 0, 10);
    assert(variant < 6);
    assert(!diagnosticProfileQualified()); // No observation before configuration.
    Logger::reportingLevel() = LOG_INFO;
    if (stage == "startup") disqualify(variant);
    if (stage != "uninitialized") initializeDiagnosticProfile();
    const bool admitted = stage == "admission" || stage == "publication";
    assert(diagnosticProfileQualified() == admitted);
    int visual[2], audio[2];
    assert(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, visual) == 0);
    assert(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, audio) == 0);
    PresentationObserver pixels(std::to_string(visual[1]).c_str());
    MixedAudioObserver sound(std::to_string(audio[1]).c_str());
    sound.publishCatalogSupported(true); sound.poll();
    std::string pixelHello = receive(visual[0]), audioHello = receive(audio[0]);
    char directory[] = "/tmp/openxcom-diagnostic-test-XXXXXX";
    assert(mkdtemp(directory)); const std::string root(directory);
    {
        CursorFileObserver cursor(directory); HudFileObserver hud(directory);
        if (!admitted)
        {
            assert(!pixels.enabled() && !sound.enabled() && !callback);
            assert(pixelHello.empty() && audioHello.empty());
            assert(!cursor.enabled() && !hud.enabled());
        }
        else
        {
            assert(pixelHello.size() == 88 && audioHello.size() == 87);
            assert(pixels.enabled() && sound.enabled() && callback);
            assert(cursor.enabled() && hud.enabled());
            cursor.observe(crop()); PublicHud values; values.selected = true; values.health = 20;
            hud.observe(values);
            const std::string beforeCursor = contents(root + "/cursor.json");
            const std::string beforeHud = contents(root + "/snapshot.json");
            assert(!beforeCursor.empty() && !beforeHud.empty());
            request(visual[0], pixelHello); request(audio[0], audioHello);
            if (stage == "publication")
            {
                pixels.beginFrame(); sound.poll();
                assert(pixels.pending());
                std::vector<Uint8> block(1024, 0x5a);
                for (unsigned i = 0; i < 45; ++i) callback(context, block.data(), block.size());
            }
            disqualify(variant);
            if (stage == "admission") pixels.beginFrame(); else pixels.presented(crop());
            sound.poll(); cursor.observe(crop()); values.health = 99; hud.observe(values);
            assert(!pixels.enabled() && !sound.enabled() && !callback);
            assert(receive(visual[0]).empty() && receive(audio[0]).empty());
            assert(!cursor.enabled() && !hud.enabled());
            assert(contents(root + "/cursor.json") == beforeCursor);
            assert(contents(root + "/snapshot.json") == beforeHud);
            lockReleased(root + "/cursor.lock"); lockReleased(root + "/observer.lock");
        }
    }
    if (stage != "uninitialized")
    {
        Options::debug = Options::debugUi = Options::traceAI = Options::verboseLogging = false;
        qualifyNormalDiagnostics();
        assert(!diagnosticProfileQualified()); // Cannot silently regain qualification.
        assert(!pixels.enabled() && !sound.enabled());
    }
    ::close(visual[0]); ::close(audio[0]);
    for (const char *file : {"cursor.json", "cursor.lock", "snapshot.json", "observer.lock"})
        unlink((root + "/" + file).c_str());
    assert(rmdir(directory) == 0);
}
