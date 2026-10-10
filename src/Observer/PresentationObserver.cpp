// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
#include "PresentationObserver.h"
#include <cerrno>
#include <cstdlib>
#include <climits>
#include <fcntl.h>
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
    out.clear();
    for (unsigned char byte : bytes) { out += hex[byte >> 4]; out += hex[byte & 15]; }
    return true;
}
}
PresentationObserver::PresentationObserver(const char *descriptor) : _fd(-1), _hudEnabled(false)
{
#ifndef MSG_NOSIGNAL
    // Without a per-send SIGPIPE suppression flag this prototype stays disabled.
    (void)descriptor;
    return;
#endif
    if (!descriptor || !*descriptor) return;
    char *end = 0;
    errno = 0;
    long value = std::strtol(descriptor, &end, 10);
    if (errno || *end || value < 3 || value > INT_MAX) return;
    int fd = static_cast<int>(value), type = 0, listening = 0;
    socklen_t size = sizeof(type);
    sockaddr_storage peer; socklen_t peerSize = sizeof(peer);
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &size) != 0 || type != SOCK_SEQPACKET ||
        getpeername(fd, reinterpret_cast<sockaddr *>(&peer), &peerSize) != 0 || peer.ss_family != AF_UNIX ||
        getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &listening, &size) != 0 || listening) return;
    // Duplicate then own only this transport descriptor; no other inherited fds.
    _fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
    if (_fd < 0) return;
    ::close(fd);
    int flags = fcntl(_fd, F_GETFL);
    if (flags < 0 || fcntl(_fd, F_SETFL, flags | O_NONBLOCK) != 0 ||
        !randomToken(_epoch) || !randomToken(_admission)) { close(); return; }
    const char *version = std::getenv("OPENXCOM_PRESENTATION_VERSION");
    _hudEnabled = version && std::string(version) == "2";
    sendPacket(std::string(_hudEnabled ? "OPENXCOM-PRESENTATION/2 " : "OPENXCOM-PRESENTATION/1 ") + _epoch + _admission);
}
PresentationObserver::~PresentationObserver() { close(); }
void PresentationObserver::close()
{
    if (_fd >= 0) ::close(_fd);
    _fd = -1; _challenge.clear(); _admission.clear();
}
bool PresentationObserver::sendPacket(const std::string &bytes)
{
    if (_fd < 0) return false;
#ifdef MSG_NOSIGNAL
    if (send(_fd, bytes.data(), bytes.size(), MSG_DONTWAIT | MSG_NOSIGNAL) != static_cast<ssize_t>(bytes.size()))
    { close(); return false; }
    return true;
#else
    (void)bytes;
    close();
    return false;
#endif
}
void PresentationObserver::resetPresentation()
{
    // An idle display reset does not change this Screen-lifetime pixel stream.
    // A consumed request must never cross a reset into another presentation.
    if (pending()) close();
}
void PresentationObserver::beginFrame()
{
    if (_fd < 0) return;
    // A previous frame token must never survive an interrupted composition.
    if (!_challenge.empty()) { close(); return; }
    char bytes[97];
    ssize_t count = recv(_fd, bytes, sizeof(bytes), MSG_DONTWAIT);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
    if (count != 96) { close(); return; }
    const std::string request(bytes, 96);
    if (request.find_first_not_of("0123456789abcdef") != std::string::npos ||
        request.substr(0, 32) != _epoch || request.substr(32, 32) != _admission)
    { close(); return; }
    // Multiple queued requests are never merged or carried to future frames.
    char extra;
    if (recv(_fd, &extra, 1, MSG_DONTWAIT | MSG_PEEK) >= 0 ||
        (errno != EAGAIN && errno != EWOULDBLOCK)) { close(); return; }
    _challenge = request.substr(64, 32);
    if (!randomToken(_admission)) close();
}
void PresentationObserver::presented(const PixelCrop &crop, const std::string &hud)
{
    if (_fd < 0 || _challenge.empty()) return;
    if (!validCrop(crop)) { close(); return; }
    const std::string challenge = _challenge;
    _challenge.clear();
    sendPacket(std::string("{\"schema\":\"experimental-presentation/") + (_hudEnabled ? "2" : "1") + "\",\"epoch\":\"" + _epoch +
        "\",\"challenge\":\"" + challenge + "\",\"next_token\":\"" + _admission +
        "\",\"semantics\":\"presented_after_request\",\"profile\":\"software-single-buffer-8bit-integer-scale-no-bands\",\"crop\":" + serializeCrop(crop) + (_hudEnabled ? ",\"hud\":" + hud : "") + "}");
}
} }
