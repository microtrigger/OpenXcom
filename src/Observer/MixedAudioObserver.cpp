// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// Free software under GNU GPL version 3 or later; without warranty.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
#include "MixedAudioObserver.h"
#include "DiagnosticProfile.h"
#include <SDL_mixer.h>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <fcntl.h>
#include <new>
#include <sys/socket.h>
#include <unistd.h>

namespace OpenXcom { namespace Observer {
namespace {
bool randomToken(std::string &out)
{
    unsigned char bytes[16];
    int fd = open("/dev/urandom", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return false;
    const ssize_t count = read(fd, bytes, sizeof(bytes));
    ::close(fd);
    if (count != sizeof(bytes)) return false;
    const char *hex = "0123456789abcdef";
    out.resize(32);
    for (unsigned i = 0; i < 16; ++i)
    {
        out[i * 2] = hex[bytes[i] >> 4];
        out[i * 2 + 1] = hex[bytes[i] & 15];
    }
    return true;
}
bool socketEmpty(int fd)
{
    char extra;
    return recv(fd, &extra, 1, MSG_DONTWAIT | MSG_PEEK) < 0 &&
        (errno == EAGAIN || errno == EWOULDBLOCK);
}
}
MixedAudioObserver::MixedAudioObserver(const char *descriptor) :
    _fd(-1), _attached(false), _helloSent(false), _admitted(false), _catalog(0),
    _started(Clock::now())
{
#ifndef MSG_NOSIGNAL
    (void)descriptor;
    return;
#endif
    if (!descriptor || !*descriptor) return;
    char *end = 0;
    errno = 0;
    const long value = std::strtol(descriptor, &end, 10);
    if (errno || *end || value < 3 || value > INT_MAX) return;
    const int fd = static_cast<int>(value);
    int type = 0, listening = 0;
    socklen_t size = sizeof(type);
    sockaddr_storage peer, local;
    socklen_t peerSize = sizeof(peer), localSize = sizeof(local);
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &size) != 0 || type != SOCK_SEQPACKET ||
        getpeername(fd, reinterpret_cast<sockaddr *>(&peer), &peerSize) != 0 || peer.ss_family != AF_UNIX ||
        peerSize != sizeof(sa_family_t) ||
        getsockname(fd, reinterpret_cast<sockaddr *>(&local), &localSize) != 0 || local.ss_family != AF_UNIX ||
        localSize != sizeof(sa_family_t) ||
        getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &listening, &size) != 0 || listening) return;
    // Own only this valid connected descriptor, closing the inherited original.
    _fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
    ::close(fd);
    if (_fd < 0) return;
    if (!diagnosticProfileQualified()) { close(); return; }
    const int flags = fcntl(_fd, F_GETFL);
    if (flags < 0 || fcntl(_fd, F_SETFL, flags | O_NONBLOCK) != 0 ||
        !_slot.lockFree() || !_catalog.is_lock_free()) close();
    // BOOTSTRAP: no hook, tokens, hello or PCM before resources are ready.
}
MixedAudioObserver::~MixedAudioObserver() { close(); }
void MixedAudioObserver::publishCatalogSupported(bool supported)
{
    _catalog.store(supported ? 1 : 2, std::memory_order_release);
}
void MixedAudioObserver::beforeAudioReset(bool resourceReload)
{
    // A sent hello already promises a qualified epoch, so any reset is terminal.
    if (_helloSent) close();
    if (resourceReload) _catalog.store(0, std::memory_order_release);
}
void MixedAudioObserver::close()
{
    if (_attached)
    {
        // SDL_mixer 1.2.12 setter locks/unlocks the audio device internally.
        // Never surround this setter with SDL_LockAudio (non-recursive lock).
        Mix_SetPostMix(NULL, NULL);
        _attached = false;
    }
    closeDetachedState();
}
void MixedAudioObserver::closeDetachedState() noexcept
{
    _slot.closeAfterDetach();
    if (_fd >= 0) ::close(_fd);
    _fd = -1;
    _challenge.clear();
    _admission.clear();
}
#ifdef OPENXCOM_LOADING_AUDIO_HANDOFF
MixedAudioObserver::LoadingReset MixedAudioObserver::resetForExclusiveLoadingAfterPostmixDetach() noexcept
{
    // The friend completed the empty drain and global hook detach already.
    // Never enter public owner APIs, protocol, diagnostics or clock logic here.
    _attached = false;
    _catalog.store(0, std::memory_order_release);
    if (_helloSent)
    {
        closeDetachedState();
        return LoadingReset::EpochClosed;
    }
    // Preserve original bootstrap start/deadline, fd and slot; no revival.
    if (_fd < 0) return LoadingReset::DisabledRetained;
    return LoadingReset::BootstrapPreserved;
}
#ifdef OPENXCOM_RESPONSIVE_LOADING
void MixedAudioObserver::terminalCloseForExclusiveLoadingAfterPostmixDetach() noexcept
{
    // Same-context exclusive worker authority: no clock/protocol/owner API and
    // no mixer setter. Native callback exclusion and postmix detach came first.
    _attached = false;
    _catalog.store(0, std::memory_order_release);
    closeDetachedState();
}
#endif

#endif
void SDLCALL MixedAudioObserver::postmix(void *context, Uint8 *stream, int len)
{
    static_cast<MixedAudioSlot *>(context)->callback(stream, len);
}
bool MixedAudioObserver::sendPacket(const std::string &packet)
{
    if (!diagnosticProfileQualified()) { close(); return false; }
#ifdef MSG_NOSIGNAL
    if (_fd >= 0 && packet.size() < 102400 &&
        send(_fd, packet.data(), packet.size(), MSG_DONTWAIT | MSG_NOSIGNAL) ==
            static_cast<ssize_t>(packet.size())) return true;
#else
    (void)packet;
#endif
    close();
    return false;
}
void MixedAudioObserver::poll()
{
    if (_fd < 0) return;
    if (!diagnosticProfileQualified()) { close(); return; }
    try { pollImpl(); }
    catch (const std::bad_alloc &) { close(); }
}
void MixedAudioObserver::pollImpl()
{
    const Clock::time_point now = Clock::now();
    if (!_helloSent)
    {
        if (now - _started >= std::chrono::seconds(30) || !socketEmpty(_fd))
        { close(); return; }
        const unsigned catalog = _catalog.load(std::memory_order_acquire);
        if (!catalog) return;
        int rate = 0, channels = 0;
        Uint16 format = 0;
        const SDL_version *version = Mix_Linked_Version();
        if (catalog != 1 || !version || version->major != 1 || version->minor != 2 || version->patch != 12 ||
            !Mix_QuerySpec(&rate, &format, &channels) || rate != 22050 || format != AUDIO_S16LSB || channels != 2)
        { close(); return; }
        if (!randomToken(_epoch) || !randomToken(_admission)) { close(); return; }
        Mix_SetPostMix(postmix, &_slot);
        _attached = true;
        if (!sendPacket("OPENXCOM-MIXED-AUDIO/1 " + _epoch + _admission)) return;
        _helloSent = true;
        return;
    }
    if (!_challenge.empty())
    {
        if (now - _lastAdmission >= std::chrono::seconds(2) || !socketEmpty(_fd))
        { close(); return; }
        const MixedAudioSlot::State state = _slot.state();
        if (state == MixedAudioSlot::FAILED) { close(); return; }
        if (state != MixedAudioSlot::READY) return;
        const unsigned char *pcm = _slot.readyData();
        std::string packet = "{\"schema\":\"experimental-mixed-audio/1\",\"epoch\":\"" + _epoch +
            "\",\"challenge\":\"" + _challenge + "\",\"next_token\":\"" + _admission +
            "\",\"semantics\":\"post_admission_mixed_output\","
            "\"profile\":\"sdl-mixer-postmix-s16le-stereo-22050\","
            "\"duration_ms\":500,\"sample_rate\":22050,\"channels\":2,\"sample_format\":\"s16le\","
            "\"frames\":11025,\"pcm_hex\":\"";
        packet.reserve(packet.size() + MixedAudioSlot::Bytes * 2 + 2);
        const char *hex = "0123456789abcdef";
        for (unsigned i = 0; i < MixedAudioSlot::Bytes; ++i)
        {
            packet += hex[pcm[i] >> 4];
            packet += hex[pcm[i] & 15];
        }
        packet += "\"}";
        if (Clock::now() - _lastAdmission >= std::chrono::seconds(2)) { close(); return; }
        if (!sendPacket(packet)) return;
        _challenge.clear();
        _slot.sent(); // Only after successful send. Callback never touched READY.
        return;
    }
    char bytes[97];
    const ssize_t count = recv(_fd, bytes, sizeof(bytes), MSG_DONTWAIT);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
    if (count != 96 || (_admitted && now - _lastAdmission < std::chrono::seconds(1)))
    { close(); return; }
    const std::string request(bytes, 96);
    if (request.find_first_not_of("0123456789abcdef") != std::string::npos ||
        request.compare(0, 32, _epoch) || request.compare(32, 32, _admission) || !socketEmpty(_fd))
    { close(); return; }
    _challenge = request.substr(64, 32);
    if (!randomToken(_admission)) { close(); return; }
    _lastAdmission = now;
    _admitted = true;
    if (!_slot.arm()) close();
}
} }
