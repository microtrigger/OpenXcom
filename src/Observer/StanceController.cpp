// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution, 2026-10-11; selection/turn/map-left-click extensions, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
#include "StanceController.h"
#include "StanceConfirmState.h"
#include "DiagnosticProfile.h"
#include "PresentedCursorSurface.h"
#include "../Battlescape/BattlescapeState.h"
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
#include "../Battlescape/InventoryState.h"
#endif
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
std::string hexBytes(const std::vector<unsigned char> &bytes)
{
    const char *hex = "0123456789abcdef";
    std::string result; result.reserve(bytes.size() * 2);
    for (unsigned char b : bytes) { result += hex[b >> 4]; result += hex[b & 15]; }
    return result;
}
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_TURN
bool canonicalView(const std::vector<unsigned char> &rgb, std::string &indicesHex, std::string &paletteHex)
{
    if (rgb.size() != 320 * 200 * 3) return false;
    std::map<unsigned, unsigned char> colors;
    std::vector<unsigned char> indices, palette(256 * 3, 0);
    indices.reserve(320 * 200);
    for (std::size_t i = 0; i < rgb.size(); i += 3)
    {
        const unsigned color = (unsigned(rgb[i]) << 16) | (unsigned(rgb[i + 1]) << 8) | rgb[i + 2];
        auto entry = colors.find(color);
        if (entry == colors.end())
        {
            if (colors.size() == 256) return false;
            const unsigned char index = static_cast<unsigned char>(colors.size());
            entry = colors.insert(std::make_pair(color, index)).first;
            palette[index * 3] = rgb[i]; palette[index * 3 + 1] = rgb[i + 1]; palette[index * 3 + 2] = rgb[i + 2];
        }
        indices.push_back(entry->second);
    }
    indicesHex = hexBytes(indices); paletteHex = hexBytes(palette); return true;
}
#endif
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
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_TURN
bool TurnContext::operator==(const TurnContext &o) const
{
    return position == o.position && cameraOffset == o.cameraOffset && poseRevision == o.poseRevision &&
        cameraRevision == o.cameraRevision && direction == o.direction && turretDirection == o.turretDirection &&
        allLayers == o.allLayers && geometry == o.geometry;
}
#endif
StanceController::StanceController(Game *game, const char *descriptor) : _game(game), _fd(-1),
    _dialog(0), _frameDialog(0), _expectedPush(0), _expectedPop(0), _challengeState(0),
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    _inventoryEnabled(false), _awaitingInventoryView(false), _inventoryDestination(0), _frameInventory(0),
#endif
    _action('s'), _selectionEnabled(false), _turnEnabled(false), _mapLeftClickEnabled(false), _awaitingView(false), _waitingTurnFrame(false), _contextKind('O'), _targetX(0), _targetY(0), _armed(false), _desired(false),
    _dialogPresented(false), _confirmEventAllowed(false), _yDown(false), _mouseDown(false), _mouseArmed(false), _contextTime(0), _challengeTime(0), _challengeDeadline(0)
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK
    , _mapLeftClickObservationActive(false), _confirmEventFromKey(false), _mapLeftClickConfirmKeyRelease(false), _mapLeftClickDispatchTime(0)
#endif
{
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SELECTION
    const char *selection = std::getenv("OPENXCOM_SELECTION_ENABLED");
    _selectionEnabled = selection && std::string(selection) == "1";
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_TURN
    const char *turn = std::getenv("OPENXCOM_TURN_ENABLED");
    _turnEnabled = turn && std::string(turn) == "1";
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK
    const char *leftClick = std::getenv("OPENXCOM_MAP_LEFT_CLICK_ENABLED");
    _mapLeftClickEnabled = _turnEnabled && leftClick && std::string(leftClick) == "1";
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    const char *inventory = std::getenv("OPENXCOM_INVENTORY_ENABLED");
    _inventoryEnabled = _turnEnabled && inventory && std::string(inventory) == "1";
#endif
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
void StanceController::invalidate(bool newEpoch, bool preserveMapWait, bool preserveInventoryRead)
{
    _capability.clear(); _confirmEventAllowed = false;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK
    _confirmEventFromKey = false;
    if (newEpoch || !preserveMapWait)
        _mapLeftClickObservationActive = _mapLeftClickConfirmKeyRelease = false;
#else
    (void)preserveMapWait;
#endif
#ifndef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    (void)preserveInventoryRead;
#endif
    if (newEpoch && (_contextKind == 'V'
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
        || (_contextKind == 'I' && !preserveInventoryRead)
#endif
        ) && !_challenge.empty())
    { _challenge.clear(); unavailable(); }
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
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    // Never retain a destination pointer after it has left the live stack.
    // Losing it does not discharge the mandatory post-B observation barrier.
    if (state == _inventoryDestination) _inventoryDestination = 0;
    if (state == _frameInventory) _frameInventory = 0;
#endif
    if (state == _expectedPop && state == _dialog) { _expectedPop = 0; return; }
    invalidate(true);
    if (state == _dialog) _dialog = 0;
}
bool StanceController::ownsDialog(State *state) const { return _dialog && state == _dialog; }
void StanceController::beforeEvent(const SDL_Event &event, State *top)
{
    _confirmEventAllowed = false;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK
    _confirmEventFromKey = false;
#endif
    if (_fd < 0) return;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK
    if (_mapLeftClickObservationActive && _mapLeftClickConfirmKeyRelease && !_dialog &&
        _awaitingView && _action == 'l' && _game->isState(_context.state) &&
        event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_y &&
        event.key.keysym.mod == KMOD_NONE && SDL_GetModState() == KMOD_NONE)
    {
        // Exactly the release completing the accepted KEYDOWN confirmation.
        // The engine still receives this event normally. Every other input
        // follows the ordinary invalidation path and revokes this one-use tail.
        _yDown = false; _mapLeftClickConfirmKeyRelease = false; return;
    }
#endif
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
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK
            _confirmEventFromKey = _confirmEventAllowed;
#endif
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
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    const bool ordinaryInput = event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN ||
        event.type == SDL_MOUSEBUTTONUP || event.type == SDL_KEYDOWN || event.type == SDL_KEYUP;
    // I is read-only. Natural init warps, hover, paging, actor changes and other
    // ordinary same-state input remain live while waiting for actual pixels.
    // Every event still revokes action grants/epochs; focus and state changes
    // never receive this narrow challenge-only preservation.
    const bool sameInventoryRead = ordinaryInput && _contextKind == 'I' && !_challenge.empty() &&
        top == _challengeState && _game->isState(top) && focused();
    invalidate(true, false, sameInventoryRead);
#else
    invalidate(true);
#endif
}

bool StanceController::bindingCurrent(bool modal) const
{
    if (!connected() || !diagnosticProfileQualified() || !focused() || !_game->getScreen()->stanceAllowed() || !_context.state ||
        now() - _contextTime > 120000 ||
        (modal ? !_dialog || !_game->isState(_dialog) : !_game->isState(_context.state))) return false;
    StanceContext current;
    if (!_context.state->stanceContext(current, modal) || !(current == _context)) return false;
    if (_contextKind == 'V')
    {
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_TURN
        TurnContext turn;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK
        if (_dialog && mapLeftClickRequest() && !_context.state->mapLeftClickControlAllowed()) return false;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
        if (_dialog && inventoryOpenRequest() &&
            (!_inventoryEnabled || !_context.state->inventoryControlVisible())) return false;
#endif
        return _turnEnabled && _context.state->turnContext(turn, modal) && turn == _turnContext;
#else
        return false;
#endif
    }
    return true;
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
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK
    if (_mapLeftClickObservationActive)
    {
        // Observe continuity even before a replacement V arrives. Unsupported
        // transients cannot disappear and restore permission to wait. This
        // changes only control availability, never the natural walking state.
        const bool sameState = _context.state && _game->isState(_context.state);
        const bool idle = sameState && _context.state->mapLeftClickObservationReady();
        if (!focused() || !_game->getScreen()->stanceAllowed() || !sameState ||
            (!idle && !_context.state->mapLeftClickObservationWaiting(_context.actor)))
        {
            _mapLeftClickObservationActive = _mapLeftClickConfirmKeyRelease = false;
            if (!_challenge.empty()) { _challenge.clear(); unavailable(); }
        }
        else if (now() - _mapLeftClickDispatchTime >= 8000)
        {
            // Expire the continuation even if the actor is already idle. An
            // idle-admitted V keeps its own frozen presentation deadline;
            // a busy-admitted V expires below at dispatch + eight seconds.
            _mapLeftClickObservationActive = _mapLeftClickConfirmKeyRelease = false;
        }
    }
#endif
    if ((_contextKind == 'V'
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
        || _contextKind == 'I'
#endif
        ) && !_challenge.empty() &&
        (now() >= _challengeDeadline || !focused() ||
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
        (_contextKind == 'I' ? !_game->getScreen()->inventoryWaitingAllowed() : !_game->getScreen()->stanceAllowed()) ||
#else
        !_game->getScreen()->stanceAllowed() ||
#endif
        (!_challengeState || !_game->isState(_challengeState))))
    { invalidate(); _challenge.clear(); unavailable(); }
    char bytes[104]; iovec part = {bytes, sizeof(bytes)}; msghdr msg = {};
    msg.msg_iov = &part; msg.msg_iovlen = 1;
    const ssize_t n = recvmsg(_fd, &msg, MSG_DONTWAIT);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
    if (n <= 0 || (msg.msg_flags & MSG_TRUNC)) { close(); return; }
    const std::string request(bytes, n);
    if (n == 33 && (request[0] == 'O' || (_turnEnabled && request[0] == 'V')) && token(request.substr(1)))
    {
        if (!_challenge.empty() || _dialog || (_awaitingView && request[0] != 'V')
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
            || _awaitingInventoryView
#endif
            ) { unavailable(); return; }
        // Replacing the consumed grant with V does not itself revoke the
        // surviving post-L wait. Every external invalidation still does.
        invalidate(false, request[0] == 'V'); _contextKind = request[0];
        _challengeTime = now(); _challengeDeadline = _challengeTime + 1500;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_TURN
        if (_contextKind == 'V')
        {
            _challengeState = dynamic_cast<BattlescapeState *>(_game->turnControlState());
            if (!_challengeState) { unavailable(); return; }
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK
            if (_mapLeftClickObservationActive && _challengeState == _context.state &&
                _context.state->mapLeftClickObservationWaiting(_context.actor))
                _challengeDeadline = _mapLeftClickDispatchTime + 8000;
#endif
        }
#endif
        _challenge = request.substr(1); return;
    }
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    if (_inventoryEnabled && n == 33 && request[0] == 'I' && token(request.substr(1)))
    {
        // This observation never authorizes actions and cannot satisfy a map
        // action's outstanding V. A lost B destination remains fail-closed.
        _capability.clear();
        if (!_challenge.empty() || _dialog || _awaitingView) { unavailable(); return; }
        invalidate(); _contextKind = 'I';
        _challengeTime = now(); _challengeDeadline = _challengeTime + 1500;
        InventoryState *state = dynamic_cast<InventoryState *>(_game->turnControlState());
        if (!state || (_awaitingInventoryView && state != _inventoryDestination)) { unavailable(); return; }
        // Admission may precede normal initialization in the next engine loop.
        // Only initialized ordinary blit + successful flip can answer it.
        _challengeState = state; _challenge = request.substr(1); return;
    }
#endif
    if (n == 33 && request[0] == 'S' && token(request.substr(1))) { replyStatus(request.substr(1)); return; }
    const bool stance = n == 99 && request[0] == 'D' &&
        (request[97] == 'k' || request[97] == 's') && (request[98] == 'k' || request[98] == 's');
    const bool selection = _selectionEnabled && n == 99 && request[0] == 'N' &&
        (request[97] == 'n' || request[97] == 'p') && request[98] == '0';
    const bool turn = _turnEnabled && n == 103 && request[0] == 'R' &&
        request.substr(97).find_first_not_of("0123456789") == std::string::npos;
    const bool leftClick = _mapLeftClickEnabled && n == 103 && request[0] == 'L' &&
        request.substr(97).find_first_not_of("0123456789") == std::string::npos;
    const bool inventoryOpen =
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
        _inventoryEnabled && n == 99 && request[0] == 'B' && request[97] == 'o' && request[98] == '0';
#else
        false;
#endif
    const bool mapClick = turn || leftClick;
    const int x = mapClick ? std::atoi(request.substr(97, 3).c_str()) : 0;
    const int y = mapClick ? std::atoi(request.substr(100, 3).c_str()) : 0;
    if ((!stance && !selection && !mapClick && !inventoryOpen) || (mapClick && (x < 16 || x > 303 || y < 40 || y > 127)) || !token(request.substr(1, 32)) || !token(request.substr(33, 32)) ||
        !token(request.substr(65, 32))) { unavailable(); close(); return; }
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
    if (_awaitingView ||
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
        _awaitingInventoryView ||
#endif
        _dialog || !_challenge.empty() || _capability.empty() || request.substr(1, 32) != _epoch ||
        request.substr(33, 32) != _capability ||
        !((_contextKind == 'O' && (stance || selection)) || (_contextKind == 'V' && (mapClick || inventoryOpen))) ||
        (stance && ((request[97] == 'k') != _context.kneeled || request[97] == request[98])) ||
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK
        (leftClick && !_context.state->mapLeftClickControlAllowed()) ||
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
        (inventoryOpen && !_context.state->inventoryControlVisible()) ||
#endif
        !bindingCurrent() || (selection && !_context.state->selectionControlVisible())) { replyStatus(id); return; }
    // Reserving this exact capability is already one-use. Local confirmation
    // cannot create a new capability, change the actor, or change the command.
    _capability.clear();
    _action = inventoryOpen ? 'o' : leftClick ? 'l' : turn ? 'r' : selection ? request[97] : request[98];
    _targetX = x; _targetY = y;
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
    _armed = false; _waitingTurnFrame = false;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    _frameInventory = 0;
#endif
    _frameDialog = _dialog && _game->isState(_dialog) ? _dialog : 0;
}
void StanceController::arm(BattlescapeState *state)
{
    if (_fd < 0 || _dialog || _contextKind == 'I' || !_game->isState(state)) return;
    _armed = state->stanceContext(_frameContext, false);
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_TURN
    if (_contextKind == 'V')
    {
        if (!_challenge.empty() && state != _challengeState) { _armed = false; return; }
        if (_armed) _armed = _turnEnabled && state->turnContext(_frameTurnContext, false);
        _waitingTurnFrame = !_armed && _awaitingView && !_challenge.empty() && state == _context.state &&
            (
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK
                (_action == 'l' && _mapLeftClickObservationActive && state->mapLeftClickObservationWaiting(_context.actor)) ||
#endif
                (_action == 'r' && state->turnObservationWaiting(_context.actor)));
    }
#endif
}
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
void StanceController::armInventory(InventoryState *state)
{
    if (_fd < 0 || !_inventoryEnabled || _dialog || _contextKind != 'I' || _challenge.empty() ||
        state != _challengeState || !_game->isState(state) ||
        (_awaitingInventoryView && state != _inventoryDestination)) return;
    _armed = state->inventoryViewAllowed();
    if (_armed) _frameInventory = state;
}
#endif
void StanceController::presented(SDL_Surface *surface, int bw, int bh, bool profileAllowed, bool panelClear)
{
    // Existing HUD/action/modal protections are unchanged. A capless inventory
    // image includes the actual normal cursor/FPS overlay even over the ground.
    const bool inventoryView =
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
        _contextKind == 'I' && !_dialog;
#else
        false;
#endif
    const bool allowed = profileAllowed && (inventoryView || panelClear);
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
    if ((_contextKind == 'V' || inventoryView) && !_challenge.empty() && now() >= _challengeDeadline)
    { invalidate(); _challenge.clear(); unavailable(); return; }
    if (bw != 320 || bh != 200 || !supportedPresentedSurface(surface, bw, bh, allowed) || ((_contextKind == 'V' || inventoryView) && !focused()))
    { invalidate(); if (!_challenge.empty()) { _challenge.clear(); unavailable(); } return; }
    if (!_armed)
    {
        if (_waitingTurnFrame && now() < _challengeDeadline) return;
        invalidate(); if (!_challenge.empty()) { _challenge.clear(); unavailable(); } return;
    }
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    if (inventoryView && (!_frameInventory || _frameInventory != _challengeState ||
        !_game->isState(_frameInventory) || !_frameInventory->inventoryViewAllowed() ||
        (_awaitingInventoryView && _frameInventory != _inventoryDestination)))
    { invalidate(); _challenge.clear(); unavailable(); return; }
#endif
    const int scale = surface->w / bw;
    // The stable bottom HUD guards both kinds of grant. A new tactical request
    // also captures the complete final frame, but later animation is not compared.
    const bool fullView = (_contextKind == 'V' || inventoryView) && !_challenge.empty();
    std::vector<unsigned char> rgb, indices;
    rgb.reserve(320 * (fullView ? 200 : 56) * 3); indices.reserve(320 * (fullView ? 200 : 56));
    if (SDL_LockSurface(surface) != 0) { close(); return; }
    bool exact = true;
    for (int y = fullView ? 0 : 144; y < 200 && exact; ++y) for (int x = 0; x < 320 && exact; ++x)
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
    if (_challenge.empty())
    {
        bool stable = _frameContext == _context && rgb == _rgb;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_TURN
        if (_contextKind == 'V') stable = stable && _frameTurnContext == _turnContext;
#endif
        if (!stable) invalidate();
        return;
    }
    const std::string challenge = _challenge; _challenge.clear();
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    if (inventoryView)
    {
        std::string indicesHex, paletteHex;
        if (!canonicalView(rgb, indicesHex, paletteHex)) { invalidate(); unavailable(); return; }
        const std::string packet = "{\"schema\":\"experimental-inventory-view/1\",\"challenge\":\"" + challenge +
            "\",\"semantics\":\"presented_after_request\",\"view\":{\"width\":320,\"height\":200,\"indices_hex\":\"" + indicesHex +
            "\",\"palette_hex\":\"" + paletteHex + "\"}}";
        if (packet.size() >= 128 * 1024) { close(); return; }
        if (now() >= _challengeDeadline) { invalidate(); unavailable(); return; }
        if (sendPacket(packet) && _awaitingInventoryView && _frameInventory == _inventoryDestination)
        { _awaitingInventoryView = false; _inventoryDestination = 0; }
        return;
    }
#endif
    if (!randomToken(_capability)) { close(); return; }
    _context = _frameContext; _indices.swap(indices); _contextTime = now();
    std::memcpy(_palette, surface->format->palette->colors, sizeof(_palette));
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_TURN
    if (fullView)
    {
        std::string indicesHex, paletteHex;
        // Canonical first-seen RGB colors, never original SDL indices/palette.
        // Equal final pixels yield equal wire images even with palette aliases.
        if (!canonicalView(rgb, indicesHex, paletteHex)) { invalidate(); unavailable(); return; }
        _rgb.assign(rgb.begin() + 320 * 144 * 3, rgb.end());
        _turnContext = _frameTurnContext;
        const std::string packet = "{\"schema\":\"experimental-tactical-context/1\",\"challenge\":\"" + challenge +
            "\",\"control_epoch\":\"" + _epoch + "\",\"capability\":\"" + _capability +
            "\",\"semantics\":\"presented_after_request\",\"view\":{\"width\":320,\"height\":200,\"indices_hex\":\"" + indicesHex +
            "\",\"palette_hex\":\"" + paletteHex + "\"}}";
        if (packet.size() >= 128 * 1024) { close(); return; }
        if (sendPacket(packet))
        {
            _awaitingView = false;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK
            _mapLeftClickObservationActive = _mapLeftClickConfirmKeyRelease = false;
#endif
        }
        return;
    }
#endif
    _rgb.swap(rgb);
    sendPacket("{\"schema\":\"experimental-stance-context/1\",\"challenge\":\"" + challenge +
        "\",\"control_epoch\":\"" + _epoch + "\",\"capability\":\"" + _capability +
        "\",\"semantics\":\"presented_after_request\",\"panel\":{\"width\":320,\"height\":56,\"rgb_hex\":\"" + hexBytes(_rgb) + "\"}}");
}
void StanceController::confirm(StanceConfirmState *dialog, bool accept)
{
    if (dialog != _dialog || !_game->isState(dialog)) return;
    const std::string id = _pendingId;
    // All validation, one-use consumption, receipt transition and normal handler
    // dispatch occur in this single main-thread event. There is no queued input.
    const bool valid = accept && _dialogPresented && _confirmEventAllowed && !id.empty() && bindingCurrent(true);
    const bool selectionValid = valid && (!selectionRequest() ||
        (_selectionEnabled && _context.state->selectionControlVisible()));
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK
    const bool leftClickValid = valid && (!mapLeftClickRequest() ||
        (_mapLeftClickEnabled && _context.state->mapLeftClickControlAllowed()));
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    const bool inventoryValid = valid && (!inventoryOpenRequest() ||
        (_inventoryEnabled && _context.state->inventoryControlVisible()));
#endif
    _confirmEventAllowed = false;
    _expectedPop = dialog;
    _game->popState();
    _dialog = 0;
    if (!valid || !selectionValid || !bindingCurrent(false) ||
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK
        !leftClickValid || (mapLeftClickRequest() && !_context.state->mapLeftClickControlAllowed()) ||
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
        !inventoryValid || (inventoryOpenRequest() && !_context.state->inventoryControlVisible()) ||
#endif
        (selectionRequest() && !_context.state->selectionControlVisible())) { invalidate(); return; }
    _pendingId.clear(); _capability.clear();
    _commands[id].status = "dispatched";
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    if (_action == 'o')
    {
        _awaitingInventoryView = true; _inventoryDestination = 0;
        // Receipt is final before the ordinary handler. Constructor/reset/push
        // retain all normal epoch invalidations and gameplay behavior.
        _context.state->btnInventoryClick(0);
        _inventoryDestination = dynamic_cast<InventoryState *>(_game->turnControlState());
        return;
    }
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_TURN
    if (_action == 'r') { _awaitingView = true; _context.state->turnControlClick(_targetX, _targetY); return; }
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK
    if (_action == 'l')
    {
        _awaitingView = true;
        _mapLeftClickObservationActive = true; _mapLeftClickDispatchTime = now();
        _mapLeftClickConfirmKeyRelease = _confirmEventFromKey;
        _context.state->mapLeftClickControlClick(_targetX, _targetY); return;
    }
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SELECTION
    if (_action == 'n') { _context.state->btnNextSoldierClick(0); return; }
    if (_action == 'p') { _context.state->btnPrevSoldierClick(0); return; }
#endif
    _context.state->btnKneelClick(0);
    // Receipt says only that the normal UI handler ran. TU/reservation refusal
    // and reaction fire are learned through the next actual displayed image.
}
} }
