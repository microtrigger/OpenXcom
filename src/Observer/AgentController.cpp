// SPDX-License-Identifier: GPL-3.0-or-later
// Agent control is intentionally a different ABI from supervised proposals.
#include "AgentController.h"
#include "DiagnosticProfile.h"
#include "../Engine/Game.h"
#include "../Engine/Screen.h"
#include "../Engine/State.h"
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstddef>
#include <unistd.h>

namespace OpenXcom { namespace Observer {
namespace {
std::uint64_t now()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool randomToken(std::string &out)
{
    unsigned char bytes[16];
    const int fd = open("/dev/urandom", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return false;
    const ssize_t count = read(fd, bytes, sizeof(bytes)); ::close(fd);
    if (count != sizeof(bytes)) return false;
    const char *hex = "0123456789abcdef"; out.clear();
    for (unsigned char c : bytes) { out += hex[c >> 4]; out += hex[c & 15]; }
    return true;
}
}
AgentController::AgentController(Game *game) : _game(game)
{
    const char *mode = std::getenv("OPENXCOM_AGENT_CONTROL");
    const char *descriptor = std::getenv("OPENXCOM_AGENT_CONTROL_FD");
    // Ambient supervised transport cannot silently select this execution mode.
    if (!mode || std::string(mode) != "1" || !descriptor || !*descriptor ||
        std::getenv("OPENXCOM_STANCE_FD")) return;
    char *end = 0; errno = 0;
    const long number = std::strtol(descriptor, &end, 10);
    if (errno || *end || number < 3 || number > INT_MAX) return;
    const int fd = static_cast<int>(number);
    int type = 0, listening = 0; socklen_t size = sizeof(type);
    sockaddr_storage peer, local; socklen_t peerSize = sizeof(peer), localSize = sizeof(local);
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &size) || type != SOCK_SEQPACKET ||
        getpeername(fd, reinterpret_cast<sockaddr *>(&peer), &peerSize) || peer.ss_family != AF_UNIX ||
        getsockname(fd, reinterpret_cast<sockaddr *>(&local), &localSize) || local.ss_family != AF_UNIX ||
        peerSize != offsetof(sockaddr_un, sun_path) || localSize != offsetof(sockaddr_un, sun_path) ||
        getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &listening, &size) || listening) return;
    _fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
    if (_fd < 0) return;
    ::close(fd);
    const int flags = fcntl(_fd, F_GETFL);
    if (flags < 0 || fcntl(_fd, F_SETFL, flags | O_NONBLOCK) || !randomToken(_epoch)) { close(); return; }
    sendPacket(agentEnvelope() + ",\"epoch\":" + agentJsonString(_epoch) + ",\"ledger_capacity\":64}");
}
AgentController::~AgentController() { close(); }
void AgentController::close()
{
    if (_fd >= 0) ::close(_fd);
    _fd = -1; _ready = false; _capability.clear(); _readChallenge.clear();
    _boundTop = _frameTop = 0;
}
bool AgentController::sendPacket(const std::string &packet)
{
    if (_fd >= 0 && diagnosticProfileQualified() &&
        send(_fd, packet.data(), packet.size(), MSG_DONTWAIT | MSG_NOSIGNAL) == static_cast<ssize_t>(packet.size())) return true;
    close(); return false;
}
bool AgentController::focused() const
{
    const Uint8 required = SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS;
    return (SDL_GetAppState() & required) == required && SDL_GetModState() == KMOD_NONE &&
        SDL_GetMouseState(0, 0) == 0;
}
void AgentController::invalidate(bool publicChange)
{
    _capability.clear(); _boundTop = 0;
    if (++_generation == 0) close();
    if (publicChange && ++_revision >= 9007199254740992ULL) close();
}
void AgentController::beforeEvent(const SDL_Event &event)
{
    switch (event.type)
    {
        case SDL_KEYDOWN: case SDL_KEYUP: case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEMOTION: case SDL_ACTIVEEVENT: case SDL_VIDEORESIZE: case SDL_VIDEOEXPOSE: case SDL_QUIT:
            invalidate(); break;
        default: break;
    }
}
bool AgentController::sample(AgentUiState &out) const
{
    State *top = _game->agentTopState();
    if (!top || !focused() || !diagnosticProfileQualified() || !_game->getScreen()->agentPresentationReady()) return false;
    out = AgentUiState();
    // Only the top painted state supplies semantics. No stack/object inventory.
    if (!top->agentUiState(out) || !agentRegionsDisjoint(out) || !_game->agentOverlayClear(out)) out = AgentUiState();
    return true;
}
void AgentController::unavailable()
{
    sendPacket(agentEnvelope() + ",\"type\":\"error\",\"status\":\"unavailable\"}");
}
std::string AgentController::statePacket(const AgentUiState &ui, const std::string &challenge, bool grant)
{
    _capability.clear(); _boundTop = 0;
    if (grant && !ui.actions.empty())
    {
        if (!randomToken(_capability)) { close(); return std::string(); }
        _binding = ui; _boundTop = _game->agentTopState(); _contextTime = now();
        _boundDisplayRevision = _game->getScreen()->agentDisplayRevision();
    }
    return agentEnvelope() + ",\"type\":\"state\",\"challenge\":" +
        (challenge.empty() ? "null" : agentJsonString(challenge)) + ",\"epoch\":" + agentJsonString(_epoch) +
        ",\"revision\":" + std::to_string(_revision) + ",\"capability\":" +
        (_capability.empty() ? "null" : agentJsonString(_capability)) + "," + ui.payload() +
        ",\"event_sequence\":" + std::to_string(_eventSequence) + ",\"semantics\":\"presented_after_request\"}";
}
std::string AgentController::resultPacket(const std::string &id, const std::string &status, const std::string &observation) const
{
    return agentEnvelope() + ",\"type\":\"result\",\"command_id\":" + agentJsonString(id) +
        ",\"status\":" + agentJsonString(status) + ",\"event_sequence\":" + std::to_string(_eventSequence) +
        ",\"observation\":" + observation + ",\"semantics\":\"dispatch_not_success\"}";
}
void AgentController::receipt(const std::string &id)
{
    AgentLedger::Entry *entry = _ledger.find(id);
    sendPacket(entry && !entry->response.empty() ? entry->response :
        resultPacket(id, entry ? "pending" : "unknown"));
}
void AgentController::poll()
{
    if (_fd < 0) return;
    if (!diagnosticProfileQualified() || !agentPaintHealthy()) { close(); return; }
    if (!focused()) invalidate(false);
    if (!_readChallenge.empty() && now() - _readTime > 1000)
    { _readChallenge.clear(); invalidate(); unavailable(); }
    if (!_pendingId.empty() && now() - _dispatchTime > 8000)
    {
        const std::string id = _pendingId; _pendingId.clear(); invalidate();
        _ledger.find(id)->response = resultPacket(id, "unknown_after_dispatch");
        receipt(id);
    }
    // One packet per owner-loop iteration, never an unbounded event-loop drain.
    char bytes[256];
    iovec vector = {bytes, sizeof(bytes)};
    msghdr message = {};
    message.msg_iov = &vector; message.msg_iovlen = 1;
    // No ancillary receive buffer: the kernel discards and closes transmitted
    // descriptors. MSG_CTRUNC still detects and rejects such packets.
    const ssize_t count = recvmsg(_fd, &message, MSG_DONTWAIT | MSG_TRUNC);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
    if (count <= 0) { close(); return; }
    if (count > 256 || (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC))) { close(); return; }
    const std::string packet(bytes, static_cast<std::size_t>(count));
    AgentRequest request;
    if (!parseAgentRequest(packet, request)) { invalidate(); if (!_ready) close(); else unavailable(); return; }
    if (!_ready)
    {
        if (request.kind != 'H') { close(); return; }
        _ready = true;
        sendPacket(agentEnvelope() + ",\"status\":\"ready\"}"); return;
    }
    if (request.kind == 'S') { receipt(request.id); return; }
    if (request.kind == 'R')
    {
        if (!_pendingId.empty() || !_readChallenge.empty()) { invalidate(); unavailable(); return; }
        invalidate(); _readChallenge = request.challenge; _readTime = now(); return;
    }
    if (request.kind != 'E') { invalidate(); unavailable(); return; }
    const AgentLedger::Admission admission = _ledger.begin(request.id, packet);
    if (admission == AgentLedger::Duplicate) { receipt(request.id); return; }
    if (admission != AgentLedger::Fresh) { invalidate(); sendPacket(resultPacket(request.id, "unavailable")); return; }
    AgentUiState current;
    const bool valid = _pendingId.empty() && _readChallenge.empty() && request.epoch == _epoch &&
        !_capability.empty() && request.capability == _capability && request.revision == _revision &&
        now() - _contextTime <= 120000 && _boundTop == _game->agentTopState() && sample(current) &&
        _boundDisplayRevision == _game->getScreen()->agentDisplayRevision() &&
        _binding.sameBinding(current) && _boundTop->agentBindingCurrent(_binding) && current.offers(request.action);
    State *target = _boundTop;
    invalidate(); // Consume before invoking any normal handler or callback.
    if (!valid)
    {
        _ledger.find(request.id)->response = resultPacket(request.id, "unavailable"); receipt(request.id); return;
    }
    _pendingId = request.id; _dispatchTime = now();
    _ledger.find(request.id)->dispatched = true;
    try
    {
        if (!target->agentDispatch(request.action))
        {
            _pendingId.clear();
            _ledger.find(request.id)->response = resultPacket(request.id, "unavailable"); receipt(request.id);
        }
        // A normal synchronous map generator can take time. The observation
        // deadline starts after that handler returns; the adapter separately
        // bounds the entire exchange and never retries an uncertain command.
        _dispatchTime = now();
    }
    catch (...)
    {
        _pendingId.clear();
        _ledger.find(request.id)->response = resultPacket(request.id, "unknown_after_dispatch"); receipt(request.id);
        throw;
    }
}
void AgentController::beginFrame()
{
    _frameTop = _game->agentTopState(); _frameGeneration = _generation;
}
void AgentController::presented()
{
    if (_fd < 0 || !_ready) return;
    if (!agentPaintHealthy()) { close(); return; }
    AgentUiState ui;
    if (_frameTop != _game->agentTopState() || _frameGeneration != _generation || !sample(ui))
    { invalidate(); return; }
    const std::string publicState = ui.payload();
    if (publicState != _lastPublic)
    {
        _lastPublic = publicState; invalidate();
        if (++_eventSequence >= 9007199254740992ULL) { close(); return; }
    }
    else if (!_capability.empty() && (!_binding.sameBinding(ui) ||
        _boundDisplayRevision != _game->getScreen()->agentDisplayRevision())) invalidate(false);
    if (!_pendingId.empty())
    {
        const std::string id = _pendingId; _pendingId.clear();
        if (++_eventSequence >= 9007199254740992ULL) { close(); return; }
        const std::string observation = statePacket(ui, std::string(), false);
        _ledger.find(id)->response = resultPacket(id, "dispatched", observation);
        receipt(id);
    }
    else if (!_readChallenge.empty())
    {
        const std::string challenge = _readChallenge; _readChallenge.clear();
        sendPacket(statePacket(ui, challenge, true));
    }
}
} }
