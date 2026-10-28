// SPDX-License-Identifier: GPL-3.0-or-later
// Agent control is intentionally a different ABI from supervised proposals.
#include "AgentController.h"
#include "DiagnosticProfile.h"
#include "AgentAudioEvents.h"
#include "AgentSoundPlayback.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_REPORT_FIXTURE
#include "../../tests/agent-report-fixture/AgentReportFixture.h"
#endif
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
#include <poll.h>

namespace OpenXcom { namespace Observer {
namespace {
std::uint64_t now()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
const char *audioStatusName(AgentAudioEvents::Event::State state)
{
    return state == AgentAudioEvents::Event::Active ? "active" :
        state == AgentAudioEvents::Event::Unavailable ? "unavailable" : "pending";
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
    if (!_publicEvents.start(_epoch)) { close(); return; }
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_REPORT_FIXTURE
    if (AgentReportFixture::waiting()) return;
    _fixtureHelloSent = true;
#endif
    sendPacket(agentEnvelope() + ",\"epoch\":" + agentJsonString(_epoch) +
        ",\"ledger_capacity\":256,\"audio_history\":\"" + std::string(AgentSoundPlayback::primaryMode() ? "unavailable" : "mixed_output") + "\"}");
}
AgentController::~AgentController() { close(); }
void AgentController::close()
{
    if (_fd >= 0) ::close(_fd);
    _fd = -1; _ready = false; _capability.clear(); _readChallenge.clear();
    _boundTop = _frameTop = 0;
    _queued.clear(); _pendingId.clear();
}
bool AgentController::sendPacket(const std::string &packet)
{
    if (_fd >= 0 && packet.size() <= 49152 && diagnosticProfileQualified() &&
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
    AgentSoundPlayback::instance().invalidateCamera();
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
    const bool provider = top->agentUiState(out);
    const bool disjoint = agentRegionsDisjoint(out);
    const bool overlayClear = _game->agentOverlayClear(out);
    if (!provider || !disjoint || !overlayClear)
    {
        _game->agentRecordAdmissionFailure(provider, disjoint, overlayClear);
        out = AgentUiState();
    }
    // Cursor-only UI motion is independent of semantic provider availability.
    // A rejected provider was cleared above; never revive its hidden semantics.
    _game->agentCursorState(out);
    return true;
}
void AgentController::unavailable()
{
    sendPacket(agentEnvelope() + ",\"type\":\"error\",\"status\":\"unavailable\"}");
}
void AgentController::readExpired(const std::string &challenge)
{
    sendPacket(agentEnvelope() + ",\"type\":\"read_error\",\"read_error_schema\":\"openxcom-agent-read-error/1\",\"epoch\":" +
        agentJsonString(_epoch) + ",\"challenge\":" + agentJsonString(challenge) + ",\"reason\":\"read_expired\"}");
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
void AgentController::eventPage(const AgentRequest &request)
{
    AgentPublicEvents::Page page;
    if (!_publicEvents.read(request.epoch, request.after, request.limit, page)) { unavailable(); return; }
    std::string packet = agentEnvelope() + ",\"type\":\"events\",\"challenge\":" +
        agentJsonString(request.challenge) + ",\"epoch\":" + agentJsonString(_epoch) +
        ",\"audio_history\":\"" + std::string(AgentSoundPlayback::primaryMode() ? "unavailable" : "mixed_output") + "\",\"audio_status\":" +
        agentJsonString(AgentSoundPlayback::primaryMode() ? "unavailable" : audioStatusName(_game->agentAudioEvents().status())) + ",\"after\":" + std::to_string(request.after) +
        ",\"oldest\":" + std::to_string(page.oldest) + ",\"newest\":" + std::to_string(page.newest) +
        ",\"next_cursor\":" + std::to_string(page.next) + ",\"gap\":" + (page.gap ? "true" : "false") +
        ",\"events\":[";
    for (std::size_t i = 0; i < page.events.size(); ++i)
    { if (i) packet += ","; packet += page.events[i].json; }
    sendPacket(packet + "],\"sound_playback\":" + AgentSoundPlayback::instance().currentJson() + "}");
}
void AgentController::drainAudioEvents()
{
    AgentSoundPlayback::Event sound;
    for (unsigned i=0;i<66 && AgentSoundPlayback::instance().take(sound);++i)
        if (!_publicEvents.append(AgentSoundPlayback::eventBody(sound))) { close(); return; }
    if (AgentSoundPlayback::primaryMode()) return;
    AgentAudioEvents::Event event;
    // Independent of R/V request frequency. Audio has no control authority.
    for (unsigned i = 0; i < 64 && _game->agentAudioEvents().take(event); ++i)
    {
        // Main-thread encoding only; callback retains fixed postmix bytes, no metadata.
        std::string pcmHex;
        if (event.kind == AgentAudioEvents::Event::Window)
        {
            static const char digits[] = "0123456789abcdef";
            pcmHex.reserve(AgentAudioEvents::WindowBytes * 2);
            for (unsigned i = 0; i < AgentAudioEvents::WindowBytes; ++i)
            { pcmHex += digits[event.pcm[i] >> 4]; pcmHex += digits[event.pcm[i] & 15]; }
        }
        const std::string body = event.kind == AgentAudioEvents::Event::Status ?
            "\"kind\":\"audio_status\",\"status\":" + agentJsonString(audioStatusName(event.state)) +
                ",\"partial_window_discarded\":" + (event.partialWindowDiscarded ? "true" : "false") :
            event.kind == AgentAudioEvents::Event::Gap ?
                "\"kind\":\"audio_gap\",\"dropped_windows\":" + std::to_string(event.droppedWindows) :
                std::string("\"kind\":\"audio_output\",\"duration_ms\":100,\"nonzero\":") +
                    (event.nonzero ? "true" : "false") + ",\"source\":null,\"sound\":null,\"position\":null" +
                    ",\"pcm_sequence\":" + std::to_string(event.pcmSequence) +
                    ",\"profile\":\"s16le-stereo-22050\",\"pcm_hex\":\"" + pcmHex + "\"";
        if (!_publicEvents.append(body)) { close(); return; }
    }
}
void AgentController::resetPlayerBases()
{
    _playerBases.reset(); _basesSave=nullptr; _basesRecords.clear();
    _basesSnapshot.clear(); _basesEpoch.clear(); _basesGeneration=0;
}
void AgentController::playerBasesPage(const AgentRequest &request)
{
    // Sample only on the owner main thread. No UI sampling, grant or ledger mutation.
    SavedGame *save=_game->getSavedGame();
    if (save!=_basesSave) { resetPlayerBases(); _basesSave=save; }
    if (_basesEpoch.empty() && !randomToken(_basesEpoch)) { close(); return; }
    std::string status="ok", reason="", records="[";
    std::size_t next=request.after;
    if (request.snapshot=="-")
    {
        std::vector<std::string> captured;
        if (!_playerBases.collect(save,_game->getLanguage(),captured,reason)) status="unavailable";
        else if (_basesSnapshot.empty() || captured!=_basesRecords)
        {
            if (++_basesGeneration>=9007199254740992ULL || !randomToken(_basesSnapshot)) { close(); return; }
            _basesRecords.swap(captured);
        }
    }
    else if (request.snapshot!=_basesSnapshot || _basesSnapshot.empty()) { status="unavailable"; reason="snapshot_expired"; }
    if (status=="ok" && request.after>_basesRecords.size()) { status="unavailable"; reason="invalid_offset"; }
    if (status=="ok")
    {
        while(next<_basesRecords.size() && next-request.after<request.limit)
        {
            const auto &record=_basesRecords[next];
            if (record.size()>16384) { status="unavailable"; reason="record_limit"; break; }
            if (records.size()+record.size()+2>32768) break;
            if (next!=request.after) records+=",";
            records+=record; ++next;
        }
    }
    records+="]";
    if(status!="ok") { records="[]"; next=request.after; }
    const bool ok=status=="ok";
    sendPacket("{\"schema\":\"openxcom-player-bases/5\",\"type\":\"player_bases\",\"epoch\":"+agentJsonString(_epoch)+
        ",\"challenge\":"+agentJsonString(request.challenge)+",\"campaign_epoch\":"+agentJsonString(_basesEpoch)+
        ",\"status\":"+agentJsonString(status)+",\"reason\":"+(ok?"null":agentJsonString(reason))+
        ",\"snapshot\":"+(ok?agentJsonString(_basesSnapshot):"null")+",\"generation\":"+std::to_string(_basesGeneration)+
        ",\"offset\":"+std::to_string(request.after)+",\"next_offset\":"+std::to_string(next)+
        ",\"total\":"+(ok?std::to_string(_basesRecords.size()):"null")+",\"complete\":"+(ok&&next==_basesRecords.size()?"true":"false")+
        ",\"records\":"+records+"}");
}
void AgentController::poll()
{
    if (_fd < 0) return;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_REPORT_FIXTURE
    if (AgentReportFixture::waiting()) return;
    if (!_fixtureHelloSent)
    {
        _fixtureHelloSent = true;
        if (!sendPacket(agentEnvelope() + ",\"epoch\":" + agentJsonString(_epoch) +
            ",\"ledger_capacity\":256,\"audio_history\":\"" + std::string(AgentSoundPlayback::primaryMode() ? "unavailable" : "mixed_output") + "\"}")) return;
    }
#endif
    drainAudioEvents();
    if (_fd < 0) return;
    if (!diagnosticProfileQualified()) { close(); return; }
    if (!focused()) invalidate(false);
    if (_queued.active() && (_queued.expired(now()) || !_queued.generationMatches(_generation))) rejectQueued();
    if (_fd < 0) return;
    if (!_readChallenge.empty() && now() - _readTime > 1000)
    {
        const std::string challenge = _readChallenge;
        _readChallenge.clear(); invalidate(); readExpired(challenge);
    }
    if (!_pendingId.empty() && now() - _dispatchTime > 8000)
    {
        const std::string id = _pendingId; _pendingId.clear(); invalidate();
        _ledger.find(id)->response = resultPacket(id, "unknown_after_dispatch");
        receipt(id);
    }
    // One packet per owner-loop iteration, never an unbounded event-loop drain.
    char bytes[512];
    iovec vector = {bytes, sizeof(bytes)};
    msghdr message = {};
    message.msg_iov = &vector; message.msg_iovlen = 1;
    // No ancillary receive buffer: the kernel discards and closes transmitted
    // descriptors. MSG_CTRUNC still detects and rejects such packets.
    const ssize_t count = recvmsg(_fd, &message, MSG_DONTWAIT | MSG_TRUNC);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
    if (count <= 0) { close(); return; }
    if (count > 512 || (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC))) { close(); return; }
    const std::string packet(bytes, static_cast<std::size_t>(count));
    AgentRequest request;
    if (!parseAgentRequest(packet, request)) { invalidate(); if (!_ready) close(); else unavailable(); return; }
    if (!_ready)
    {
        if (request.kind != 'H') { close(); return; }
        _ready = true;
        sendPacket(agentEnvelope() + ",\"status\":\"ready\"}"); return;
    }
    if (request.kind == 'B') { playerBasesPage(request); return; }
    if (request.kind == 'S') { receipt(request.id); return; }
    if (request.kind == 'V') { eventPage(request); return; }
    if (request.kind == 'R')
    {
        if (_queued.active() || !_pendingId.empty() || !_readChallenge.empty()) { invalidate(); unavailable(); return; }
        invalidate(); _readChallenge = request.challenge; _readTime = now(); return;
    }
    if (request.kind != 'E') { invalidate(); unavailable(); return; }
    const AgentLedger::Admission admission = _ledger.begin(request.id, packet);
    if (admission == AgentLedger::Duplicate) { receipt(request.id); return; }
    if (admission != AgentLedger::Fresh) { invalidate(); sendPacket(resultPacket(request.id, "unavailable")); return; }
    // Authenticate and consume the original grant now, without treating a
    // queued redraw between ordinary frames as a fresh visible projection.
    const std::uint64_t received = now(), grantTime = _contextTime, display = _boundDisplayRevision;
    const AgentUiState binding = _binding;
    State *target = _boundTop;
    const bool valid = !_queued.active() && _pendingId.empty() && _readChallenge.empty() &&
        request.epoch == _epoch && !_capability.empty() && request.capability == _capability &&
        request.revision == _revision && received >= grantTime && received - grantTime <= 120000 &&
        target && target == _game->agentTopState() && focused() &&
        display == _game->getScreen()->agentDisplayRevision() && binding.offers(request.action);
    invalidate(); // One-use consumption occurs before queueing, never on replay.
    if (!valid || !_queued.stage(request, binding, target, _generation, display, grantTime, received))
    {
        _ledger.find(request.id)->response = resultPacket(request.id, "unavailable"); receipt(request.id);
    }
}
void AgentController::rejectQueued()
{
    if (!_queued.active()) return;
    const std::string id = _queued.request().id;
    _queued.clear(); // Terminal before any response or reentrant callback.
    _ledger.find(id)->response = resultPacket(id, "unavailable"); receipt(id);
}
bool AgentController::transportAlive() const
{
    if (_fd < 0) return false;
    pollfd descriptor = {_fd, POLLIN, 0};
    if (::poll(&descriptor, 1, 0) < 0 || (descriptor.revents & (POLLHUP | POLLERR | POLLNVAL))) return false;
    char byte;
    const ssize_t result = recv(_fd, &byte, 1, MSG_PEEK | MSG_DONTWAIT);
    return result > 0 || (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
}
bool AgentController::queuedInputClear() const
{
    // Capture pending physical input without consuming it or synthesizing an
    // event. The ordinary event loop still owns its eventual handling.
    SDL_PumpEvents();
    SDL_Event event;
    const Uint32 mask = SDL_KEYDOWNMASK | SDL_KEYUPMASK | SDL_MOUSEBUTTONDOWNMASK |
        SDL_MOUSEBUTTONUPMASK | SDL_MOUSEMOTIONMASK | SDL_ACTIVEEVENTMASK |
        SDL_VIDEORESIZEMASK | SDL_VIDEOEXPOSEMASK | SDL_QUITMASK;
    return SDL_PeepEvents(&event, 1, SDL_PEEKEVENT, mask) == 0;
}
void AgentController::dispatchQueued(const AgentUiState &shown)
{
    if (!_queued.active()) return;
    if (!queuedInputClear() || !focused()) { invalidate(false); rejectQueued(); return; }
    if (!transportAlive()) { close(); return; }
    State *target = _game->agentTopState();
    const bool valid = diagnosticProfileQualified() && agentPaintHealthy() &&
        _game->getScreen()->agentPresentationReady() &&
        _queued.matchesFrame(now(), _generation, _game->getScreen()->agentDisplayRevision(), target, shown) &&
        (_queued.request().action == "move_cursor" || target->agentBindingCurrent(_queued.binding()));
    if (!valid) { rejectQueued(); return; }
    const AgentRequest request = _queued.request();
    _queued.clear(); // No replay or reentrant frame can dispatch this again.
    _pendingId = request.id; _dispatchTime = now();
    _dispatchPresentation = _presentationSerial; _dispatchInProgress = true;
    _ledger.find(request.id)->dispatched = true;
    try
    {
        const bool dispatched = request.action == "move_cursor"
            ? _game->agentMoveCursor(request.x, request.y, target)
            : target->agentDispatch(request);
        if (!dispatched)
        {
            _pendingId.clear();
            _ledger.find(request.id)->response = resultPacket(request.id, "unavailable"); receipt(request.id);
        }
        // Existing post-handler observation deadline; queue time is separate.
        _dispatchTime = now(); _dispatchInProgress = false;
    }
    catch (...)
    {
        _dispatchInProgress = false; _pendingId.clear();
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
    if (_fd < 0 || !_ready || _dispatchInProgress) return;
    if (!agentPaintHealthy()) { invalidate(); rejectQueued(); return; }
    AgentUiState ui;
    if (_frameTop != _game->agentTopState() || _frameGeneration != _generation || !sample(ui))
    { invalidate(); rejectQueued(); return; }
    if (++_presentationSerial == 0) { close(); return; }
    AgentSoundPlayback::instance().camera(ui.publicCameraKnown, ui.publicCamera.x, ui.publicCamera.y, ui.publicCamera.width, ui.publicCamera.height, _presentationSerial, ui.publicCameraRevision);
    const std::string publicState = ui.payload();
    if (publicState != _lastPublic)
    {
        _lastPublic = publicState;
        if (++_eventSequence >= 9007199254740992ULL) { close(); return; }
    }
    const std::string authority = ui.authorityPayload();
    if (authority != _lastAuthority) { _lastAuthority = authority; invalidate(); }
    else if (!_capability.empty() && (!_binding.sameBinding(ui) ||
        _boundDisplayRevision != _game->getScreen()->agentDisplayRevision())) invalidate(false);
    // Identical public presentations do not create repeated journal entries.
    // This runs after successful flip, independent of state/event polling.
    if (!_publicEvents.presentation(ui.screen, ui.visible)) { close(); return; }
    // Only the explicitly admitted map provider proves report absence.
    // Unsupported, covered, incomplete and other state types remain unknown.
    if (!_publicEvents.displayedMessage(ui.displayedMessage, ui.displayedMessageAbsent)) { close(); return; }
    if (!_publicEvents.projectile(ui.projectileFact) || !_publicEvents.impact(ui.impactFact) ||
        !_publicEvents.collapse(ui.collapseFact) || !_publicEvents.thrownItem(ui.thrownItemFact) ||
        !_publicEvents.largeExplosion(ui.largeExplosionFact)) { close(); return; }
    if (_queued.active())
    {
        dispatchQueued(ui);
        return; // This pre-action frame can never be an outcome or fresh grant.
    }
    if (!_pendingId.empty() && _presentationSerial > _dispatchPresentation)
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
