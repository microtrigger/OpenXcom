// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution, 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md.
#include "StanceController.h"
#include "StanceConfirmState.h"
#include "DiagnosticProfile.h"
#include "PresentedCursorSurface.h"
#include "../Battlescape/BattlescapeState.h"
#include "../Engine/Game.h"
#include "../Engine/Screen.h"
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
namespace OpenXcom { namespace Observer {
namespace {
bool token(const std::string &s) { return s.size() == 32 && s.find_first_not_of("0123456789abcdef") == std::string::npos; }
bool randomToken(std::string &out)
{
    unsigned char bytes[16];
    int fd = open("/dev/urandom", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return false;
    const ssize_t n = read(fd, bytes, sizeof(bytes)); ::close(fd);
    if (n != sizeof(bytes)) return false;
    const char *hex = "0123456789abcdef"; out.clear();
    for (unsigned char b : bytes) { out += hex[b >> 4]; out += hex[b & 15]; }
    return true;
}
std::uint64_t now()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool focused()
{
    const Uint8 required = SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS;
    return (SDL_GetAppState() & required) == required;
}
}
bool StanceContext::operator==(const StanceContext &o) const
{
    return state == o.state && save == o.save && actor == o.actor && selectionRevision == o.selectionRevision &&
        actorRevision == o.actorRevision && uiRevision == o.uiRevision && kneeled == o.kneeled &&
        kneelReserved == o.kneelReserved && tu == o.tu && energy == o.energy && health == o.health &&
        morale == o.morale && stun == o.stun && reserved == o.reserved && name == o.name;
}
StanceController::StanceController(Game *game, const char *descriptor) : _game(game), _fd(-1),
    _dialog(0), _frameDialog(0), _expectedPush(0), _expectedPop(0), _armed(false), _desired(false),
    _dialogPresented(false), _confirmEventAllowed(false), _yDown(false), _mouseDown(false), _mouseArmed(false), _contextTime(0)
{
    std::memset(_palette, 0, sizeof(_palette));
#ifndef MSG_NOSIGNAL
    (void)descriptor; return;
#endif
    if (!descriptor || !*descriptor) return;
    char *end = 0; errno = 0;
    const long number = std::strtol(descriptor, &end, 10);
    if (errno || *end || number < 3 || number > INT_MAX) return;
    const int fd = static_cast<int>(number);
    int type = 0, listening = 0; socklen_t size = sizeof(type);
    sockaddr_storage peer; socklen_t peerSize = sizeof(peer);
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &size) || type != SOCK_SEQPACKET ||
        getpeername(fd, reinterpret_cast<sockaddr *>(&peer), &peerSize) || peer.ss_family != AF_UNIX ||
        getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &listening, &size) || listening) return;
    _fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
    if (_fd < 0) return;
    ::close(fd);
    const int flags = fcntl(_fd, F_GETFL);
    if (flags < 0 || fcntl(_fd, F_SETFL, flags | O_NONBLOCK) || !randomToken(_epoch)) { close(); return; }
    sendPacket("{\"schema\":\"experimental-stance/1\",\"control_epoch\":\"" + _epoch + "\"}");
}
StanceController::~StanceController() { close(); }
void StanceController::close()
{
    if (_fd >= 0) ::close(_fd);
    _fd = -1;
    invalidate();
    _challenge.clear();
}
bool StanceController::connected() const
{
    if (_fd < 0) return false;
    char byte;
    const ssize_t n = recv(_fd, &byte, 1, MSG_PEEK | MSG_DONTWAIT);
    return n > 0 || (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
}
bool StanceController::sendPacket(const std::string &packet)
{
#ifdef MSG_NOSIGNAL
    if (_fd >= 0 && diagnosticProfileQualified() &&
        send(_fd, packet.data(), packet.size(), MSG_DONTWAIT | MSG_NOSIGNAL) == static_cast<ssize_t>(packet.size())) return true;
#else
    (void)packet;
#endif
    close(); return false;
}
void StanceController::invalidate(bool newEpoch)
{
    _capability.clear(); _confirmEventAllowed = false;
    if (!_pendingId.empty())
    {
        _commands[_pendingId].status = "unavailable";
        _pendingId.clear();
    }
    if (newEpoch && _fd >= 0 && !randomToken(_epoch)) close();
}
void StanceController::beforePush(State *state)
{
    if (state == _expectedPush && state == _dialog && !_pendingId.empty()) { _expectedPush = 0; return; }
    invalidate(true);
}
void StanceController::beforePop(State *state)
{
    if (state == _expectedPop && state == _dialog) { _expectedPop = 0; return; }
    invalidate(true);
    if (state == _dialog) _dialog = 0;
}
bool StanceController::ownsDialog(State *state) const { return _dialog && state == _dialog; }
void StanceController::beforeEvent(const SDL_Event &event, State *top)
{
    _confirmEventAllowed = false;
    if (_fd < 0) return;
    if (ownsDialog(top) && _game->isState(_dialog))
    {
        if (event.type == SDL_MOUSEMOTION) return;
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
        {
            _mouseArmed = _dialogPresented && !_mouseDown;
            if (!_mouseArmed) invalidate();
            _mouseDown = true;
            return;
        }
        if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
        {
            _confirmEventAllowed = _dialogPresented && _mouseArmed;
            if (!_confirmEventAllowed) invalidate();
            _mouseArmed = false; _mouseDown = false;
            return;
        }
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_y)
        {
            _confirmEventAllowed = _dialogPresented && !_yDown && event.key.keysym.mod == KMOD_NONE;
            if (!_confirmEventAllowed) invalidate();
            _yDown = true;
            return;
        }
        if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_y) { _yDown = false; return; }
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) return;
        if (event.type == SDL_KEYUP) return;
    }
    // Track ordinary events as well as SDL's physical state. This also covers
    // queued transitions and makes held-input rejection independent of repeats.
    if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && event.key.keysym.sym == SDLK_y)
        _yDown = event.type == SDL_KEYDOWN;
    if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_LEFT)
        _mouseDown = event.type == SDL_MOUSEBUTTONDOWN;
    // Input/focus/resize/quit cannot leave a reusable grant behind. The original
    // SDL event still follows its ordinary route; manual takeover is never blocked.
    invalidate(true);
}

bool StanceController::bindingCurrent(bool modal) const
{
    if (!connected() || !diagnosticProfileQualified() || !focused() || !_game->getScreen()->stanceAllowed() || !_context.state ||
        now() - _contextTime > 120000 ||
        (modal ? !_dialog || !_game->isState(_dialog) : !_game->isState(_context.state))) return false;
    StanceContext current;
    return _context.state->stanceContext(current, modal) && current == _context;
}
void StanceController::replyStatus(const std::string &id)
{
    const auto i = _commands.find(id);
    if (i == _commands.end()) { unavailable(); return; }
    sendPacket("{\"schema\":\"experimental-stance-status/1\",\"command_id\":\"" + id + "\",\"status\":\"" + i->second.status + "\"}");
}
void StanceController::unavailable() { sendPacket("{\"error\":\"unavailable\"}"); }
void StanceController::poll()
{
    if (_fd < 0) return;
    if (!diagnosticProfileQualified() || !connected()) { close(); return; }
    if ((!_capability.empty() || !_pendingId.empty()) && !bindingCurrent(_dialog != 0)) invalidate();
    char bytes[100]; iovec part = {bytes, sizeof(bytes)}; msghdr msg = {};
    msg.msg_iov = &part; msg.msg_iovlen = 1;
    const ssize_t n = recvmsg(_fd, &msg, MSG_DONTWAIT);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
    if (n <= 0 || (msg.msg_flags & MSG_TRUNC)) { close(); return; }
    const std::string request(bytes, n);
    if (n == 33 && request[0] == 'O' && token(request.substr(1)))
    {
        if (!_challenge.empty() || _dialog) { unavailable(); return; }
        invalidate(); _challenge = request.substr(1); return;
    }
    if (n == 33 && request[0] == 'S' && token(request.substr(1))) { replyStatus(request.substr(1)); return; }
    if (n != 99 || request[0] != 'D' || !token(request.substr(1, 32)) || !token(request.substr(33, 32)) ||
        !token(request.substr(65, 32)) || (request[97] != 'k' && request[97] != 's') ||
        (request[98] != 'k' && request[98] != 's')) { unavailable(); close(); return; }
    const std::string id = request.substr(65, 32);
    const auto prior = _commands.find(id);
    if (prior != _commands.end())
    {
        if (prior->second.request == request) replyStatus(id);
        else sendPacket("{\"schema\":\"experimental-stance-status/1\",\"command_id\":\"" + id + "\",\"status\":\"unavailable\"}");
        return;
    }
    // Never evict entries: eviction would turn command IDs into replay tokens.
    if (_commands.size() >= 64) { unavailable(); close(); return; }
    _commands[id] = {request, "unavailable"};
    if (_dialog || !_challenge.empty() || _capability.empty() || request.substr(1, 32) != _epoch ||
        request.substr(33, 32) != _capability || (request[97] == 'k') != _context.kneeled ||
        request[97] == request[98] || !bindingCurrent()) { replyStatus(id); return; }
    // Reserving this exact capability is already one-use. Local confirmation
    // cannot create a new capability, change the actor, or change the command.
    _capability.clear();
    _desired = request[98] == 'k'; _pendingId = id;
    _dialogPresented = _confirmEventAllowed = _mouseArmed = false;
    _yDown = _yDown || SDL_GetKeyState(0)[SDLK_y] != 0;
    _mouseDown = _mouseDown || (SDL_GetMouseState(0, 0) & SDL_BUTTON_LMASK) != 0;
    _commands[id].status = "pending_confirmation";
    _dialog = new StanceConfirmState(this); _expectedPush = _dialog;
    _game->pushState(_dialog);
    replyStatus(id);
}
void StanceController::beginFrame()
{
    poll();
    _armed = false;
    _frameDialog = _dialog && _game->isState(_dialog) ? _dialog : 0;
}
void StanceController::arm(BattlescapeState *state)
{
    if (_fd < 0 || _dialog || !_game->isState(state)) return;
    _armed = state->stanceContext(_frameContext, false);
}
void StanceController::presented(SDL_Surface *surface, int bw, int bh, bool allowed)
{
    if (_fd < 0) return;
    if (_dialog)
    {
        if (_frameDialog != _dialog || !_game->isState(_dialog) ||
            !supportedPresentedSurface(surface, bw, bh, allowed)) { invalidate(); return; }
        if (!_dialogPresented)
        {
            // A proposal cannot be approved before its exact modal has really
            // completed presentation. Held keys/buttons cannot become consent.
            _dialogPresented = true;
            _yDown = _yDown || SDL_GetKeyState(0)[SDLK_y] != 0;
            _mouseDown = _mouseDown || (SDL_GetMouseState(0, 0) & SDL_BUTTON_LMASK) != 0;
            SDL_Event queued;
            const int waiting = SDL_PeepEvents(&queued, 1, SDL_PEEKEVENT,
                SDL_KEYDOWNMASK | SDL_MOUSEBUTTONDOWNMASK | SDL_MOUSEBUTTONUPMASK);
            if (waiting != 0) invalidate();
        }
        return;
    }
    if (_challenge.empty() && _capability.empty()) return;
    if (!_armed || bw != 320 || bh != 200 || !supportedPresentedSurface(surface, bw, bh, allowed))
    { invalidate(); if (!_challenge.empty()) { _challenge.clear(); unavailable(); } return; }
    const int scale = surface->w / bw;
    std::vector<unsigned char> rgb, indices;
    rgb.reserve(320 * 56 * 3); indices.reserve(320 * 56);
    if (SDL_LockSurface(surface) != 0) { close(); return; }
    bool exact = true;
    for (int y = 144; y < 200 && exact; ++y) for (int x = 0; x < 320 && exact; ++x)
    {
        const unsigned char index = *(static_cast<unsigned char *>(surface->pixels) + y * scale * surface->pitch + x * scale);
        const SDL_Color color = surface->format->palette->colors[index];
        for (int sy = 0; sy < scale && exact; ++sy) for (int sx = 0; sx < scale; ++sx)
        {
            const unsigned char actual = *(static_cast<unsigned char *>(surface->pixels) + (y * scale + sy) * surface->pitch + x * scale + sx);
            const SDL_Color c = surface->format->palette->colors[actual];
            if (c.r != color.r || c.g != color.g || c.b != color.b) { exact = false; break; }
        }
        indices.push_back(index); rgb.push_back(color.r); rgb.push_back(color.g); rgb.push_back(color.b);
    }
    SDL_UnlockSurface(surface);
    if (!exact) { invalidate(); if (!_challenge.empty()) { _challenge.clear(); unavailable(); } return; }
    if (_challenge.empty()) { if (!(_frameContext == _context) || rgb != _rgb) invalidate(); return; }
    const std::string challenge = _challenge; _challenge.clear();
    if (!randomToken(_capability)) { close(); return; }
    _context = _frameContext; _rgb.swap(rgb); _indices.swap(indices); _contextTime = now();
    std::memcpy(_palette, surface->format->palette->colors, sizeof(_palette));
    const char *hex = "0123456789abcdef";
    std::string pixels; pixels.reserve(_rgb.size() * 2);
    for (unsigned char b : _rgb) { pixels += hex[b >> 4]; pixels += hex[b & 15]; }
    sendPacket("{\"schema\":\"experimental-stance-context/1\",\"challenge\":\"" + challenge +
        "\",\"control_epoch\":\"" + _epoch + "\",\"capability\":\"" + _capability +
        "\",\"semantics\":\"presented_after_request\",\"panel\":{\"width\":320,\"height\":56,\"rgb_hex\":\"" + pixels + "\"}}");
}
void StanceController::confirm(StanceConfirmState *dialog, bool accept)
{
    if (dialog != _dialog || !_game->isState(dialog)) return;
    const std::string id = _pendingId;
    // All validation, one-use consumption, receipt transition and normal handler
    // dispatch occur in this single main-thread event. There is no queued input.
    const bool valid = accept && _dialogPresented && _confirmEventAllowed && !id.empty() && bindingCurrent(true);
    _confirmEventAllowed = false;
    _expectedPop = dialog;
    _game->popState();
    _dialog = 0;
    if (!valid || !bindingCurrent(false)) { invalidate(); return; }
    _pendingId.clear(); _capability.clear();
    _commands[id].status = "dispatched";
    _context.state->btnKneelClick(0);
    // Receipt says only that the normal UI handler ran. TU/reservation refusal
    // and reaction fire are learned through the next actual displayed image.
}
} }
