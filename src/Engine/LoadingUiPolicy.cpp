// Copyright 2026 OpenXcom contributors. GPL-3.0-or-later; see LICENSE.txt and LICENSE-NOTICES.md.
#include "LoadingUiPolicy.h"
#include <exception>
#include <stdexcept>
#include <utility>

namespace OpenXcom
{
namespace
{
[[noreturn]] void policyInvariant() noexcept { std::terminate(); }
int clampDimension(int value, int lower, int upper) noexcept
{
    return value < lower ? lower : (value > upper ? upper : value);
}
const unsigned focusMask = LoadingUiPolicy::InputFocus |
    LoadingUiPolicy::MouseFocus | LoadingUiPolicy::ApplicationActive;
}

class LoadingUiPolicy::Operation
{
public:
    explicit Operation(const LoadingUiPolicy &policy) noexcept : _policy(policy)
    {
        if (std::this_thread::get_id() != _policy._owner) policyInvariant();
        if (_policy._inside) policyInvariant();
        _policy._inside = true;
    }
    ~Operation() noexcept { _policy._inside = false; }
private:
    const LoadingUiPolicy &_policy;
};

LoadingUiPolicy::Config::Config() noexcept :
    allowResize(true), suppressStartupResize(true), minimumWidth(320),
    minimumHeight(200), maximumWidth(16384), maximumHeight(16384),
    initialFocus(InputFocus | MouseFocus | ApplicationActive), maximumEventsPerTick(64) {}

LoadingUiPolicy::LoadingUiPolicy(std::shared_ptr<LoaderSession> session, Config config) :
    _session(std::move(session)), _config(config), _owner(std::this_thread::get_id()),
    _inside(false), _phase(Phase::Idle), _failure(FixedFailure::None),
    _closeRequested(false), _renderAllowed(true), _repaint(false), _resizePending(false),
    _startupResizePending(config.suppressStartupResize), _hasResult(false),
    _width(0), _height(0), _focus(config.initialFocus & focusMask), _events(0)
{
    if (!_session || config.minimumWidth <= 0 || config.minimumHeight <= 0 ||
        config.maximumWidth < config.minimumWidth || config.maximumHeight < config.minimumHeight ||
        config.maximumEventsPerTick == 0 || config.maximumEventsPerTick > 256)
        throw std::invalid_argument("LoadingUiPolicy requires a session and bounded configuration");
    if (!_session->quiescent() || _session->admissionClosed() || _session->attempt() != 0)
        throw std::invalid_argument("LoadingUiPolicy requires an unused open session");
}

LoadingUiPolicy::~LoadingUiPolicy() noexcept
{
    Operation operation(*this);
    if (!_session->quiescent() || _phase == Phase::Quarantined) policyInvariant();
}

LoadingUiPolicy::Admission LoadingUiPolicy::reserveView(bool hasLiveSave) noexcept
{
    Operation operation(*this);
    if (_closeRequested || _session->admissionClosed()) return Admission::Closed;
    if (_phase != Phase::Idle) return Admission::Busy;
    if (hasLiveSave) return Admission::LiveSave;
    // Commit reservation before any external preparation. No backend call,
    // allocation, session reset or preparation callback is made here.
    _phase = Phase::Preparing;
    _failure = FixedFailure::None;
    _hasResult = false;
    _result.details.clear();
    _resizePending = false;
    _repaint = true;
    return Admission::Accepted;
}

LoaderSession::BeginResult LoadingUiPolicy::startPrepared() noexcept
{
    Operation operation(*this);
    if (_phase == Phase::Quarantined) return LoaderSession::BeginResult::Quarantined;
    if (_closeRequested) return LoaderSession::BeginResult::Closed;
    if (_phase != Phase::Preparing) return LoaderSession::BeginResult::Busy;
    // Reserve active UI ownership before a possibly early callback completes.
    _phase = Phase::Active;
    const LoaderSession::BeginResult begun = _session->begin();
    switch (begun)
    {
    case LoaderSession::BeginResult::Started:
    case LoaderSession::BeginResult::StartFailed:
        break; // StartFailed has a real NoThread result, consumed by tick.
    case LoaderSession::BeginResult::Quarantined:
        _phase = Phase::Quarantined;
        _closeRequested = true;
        break;
    case LoaderSession::BeginResult::Closed:
    case LoaderSession::BeginResult::Busy:
        // Unexpected external session use must not reopen UI admission. Close
        // retains any outstanding worker and leaves retirement to later ticks.
        close(FixedFailure::UnexpectedSession);
        break;
    }
    return begun;
}

void LoadingUiPolicy::close(FixedFailure reason) noexcept
{
    if (_failure == FixedFailure::None) _failure = reason;
    _closeRequested = true;
    _session->requestClose();
    if (_phase != Phase::Quarantined && _phase != Phase::Finished) _phase = Phase::Closing;
}
void LoadingUiPolicy::preparationFailed() noexcept
{
    Operation operation(*this);
    if (_phase != Phase::Preparing) policyInvariant();
    close(FixedFailure::Preparation);
}
void LoadingUiPolicy::transitionFailed() noexcept
{
    Operation operation(*this);
    if (_phase != Phase::SuccessReserved) policyInvariant();
    close(FixedFailure::Transition);
}
void LoadingUiPolicy::renderingFailed() noexcept
{
    Operation operation(*this);
    _renderAllowed = false;
    _repaint = false;
    close(FixedFailure::Rendering);
}

bool LoadingUiPolicy::recordValue(const EventValue &event) noexcept
{
    // A directly observed close is never discarded by event-budget exhaustion.
    if (event.kind == EventKind::Quit ||
        (event.kind == EventKind::KeyDown && _phase == Phase::Failure))
    {
        close(FixedFailure::None);
        if (_events < _config.maximumEventsPerTick) ++_events;
        return true;
    }
    if (_events == _config.maximumEventsPerTick) return false;
    ++_events;
    switch (event.kind)
    {
    case EventKind::StartupResize:
        if (_startupResizePending) { _startupResizePending = false; break; }
        // Only the first tagged synthetic resize is suppressed, across reloads.
        // Later tagged resizes are ordinary bounded resize intent.
        // fall through
    case EventKind::Resize:
        if (_config.allowResize)
        {
            _width = clampDimension(event.width, _config.minimumWidth, _config.maximumWidth);
            _height = clampDimension(event.height, _config.minimumHeight, _config.maximumHeight);
            _resizePending = true;
            if (_renderAllowed) _repaint = true;
        }
        break;
    case EventKind::Focus:
        _focus = event.focus & focusMask;
        if (_renderAllowed) _repaint = true;
        break;
    case EventKind::Minimize:
        _focus &= ~static_cast<unsigned>(ApplicationActive);
        break;
    case EventKind::Restore:
        _focus |= ApplicationActive;
        if (_renderAllowed) _repaint = true;
        break;
    case EventKind::Expose:
        if (_renderAllowed) _repaint = true;
        break;
    case EventKind::Quit:
    case EventKind::KeyDown:
    case EventKind::SuppressedInput:
        break;
    }
    return true;
}

bool LoadingUiPolicy::record(const EventValue &event) noexcept
{
    Operation operation(*this);
    return recordValue(event);
}
std::size_t LoadingUiPolicy::recordBatch(const EventValue *events, std::size_t count) noexcept
{
    Operation operation(*this);
    if (count && !events) policyInvariant();
    const std::size_t remaining = _config.maximumEventsPerTick - _events;
    const std::size_t limit = count < remaining ? count : remaining;
    for (std::size_t i = 0; i < limit; ++i) recordValue(events[i]);
    return limit;
}
std::size_t LoadingUiPolicy::remainingEventBudget() const noexcept
{
    Operation operation(*this);
    return _config.maximumEventsPerTick - _events;
}

LoadingUiPolicy::Decision LoadingUiPolicy::tick() noexcept
{
    Operation operation(*this);
    _events = 0;
    if (_phase == Phase::Quarantined) return Decision::RetainQuarantine;
    if (_phase == Phase::Finished) return Decision::FinishQuit;
    if (_phase == Phase::SuccessReserved) return Decision::PublishSuccess;
    if (_phase == Phase::Failure) return Decision::ShowFailure;
    // Exactly one public session poll on this path; none in quarantined or
    // already-decided paths. Never use terminalPublished as a retirement fact.
    const LoaderSession::PollResult polled = _session->poll();
    if (polled == LoaderSession::PollResult::Quarantined)
    {
        _phase = Phase::Quarantined;
        _closeRequested = true;
        return Decision::RetainQuarantine;
    }
    if (_closeRequested || polled == LoaderSession::PollResult::Closed)
    {
        // Closed may have no result (Idle/Preparing or consumed success).
        // Shutdown needs real quiescence, not an allocating diagnostic copy.
        close(FixedFailure::None);
        if (_session->quiescent())
        {
            _phase = Phase::Finished;
            return Decision::FinishQuit;
        }
        return Decision::Wait;
    }
    if (polled == LoaderSession::PollResult::Running) return Decision::Wait;
    if (polled == LoaderSession::PollResult::Idle)
    {
        if (_phase == Phase::Idle || _phase == Phase::Preparing) return Decision::Wait;
        if (!_session->quiescent()) policyInvariant();
        close(FixedFailure::MissingResult);
        _phase = Phase::Finished;
        return Decision::FinishQuit;
    }
    if (polled != LoaderSession::PollResult::Ready || !_session->quiescent()) policyInvariant();
    try
    {
        if (!_session->copyResult(_result))
        {
            close(FixedFailure::MissingResult);
            _phase = Phase::Finished;
            return Decision::FinishQuit;
        }
    }
    catch (...)
    {
        // No diagnostic allocation or repeated copy attempt after failure.
        // The real session still owns its unconsumed result; close is safe.
        close(FixedFailure::ResultCopy);
        _phase = Phase::Finished;
        return Decision::FinishQuit;
    }
    _hasResult = true;
    if (_result.terminal == LoaderSession::Terminal::Success)
    {
        _phase = Phase::SuccessReserved;
        return Decision::PublishSuccess;
    }
    _phase = Phase::Failure;
    _repaint = _renderAllowed;
    return Decision::ShowFailure;
}

bool LoadingUiPolicy::transitionCompleted() noexcept
{
    Operation operation(*this);
    if (_phase != Phase::SuccessReserved || _closeRequested) return false;
    if (!_session->quiescent() || _session->admissionClosed()) policyInvariant();
    _phase = Phase::Idle;
    return true;
}
LoadingUiPolicy::View LoadingUiPolicy::view() const noexcept
{
    Operation operation(*this);
    return View{_phase, _failure, _closeRequested, _renderAllowed, _repaint,
                _resizePending, _width, _height, _focus};
}
const LoaderSession::Result *LoadingUiPolicy::result() const noexcept
{
    Operation operation(*this);
    return _hasResult ? &_result : nullptr;
}
bool LoadingUiPolicy::takeRepaint() noexcept
{
    Operation operation(*this);
    const bool repaint = _repaint && _renderAllowed;
    _repaint = false;
    return repaint;
}
}
