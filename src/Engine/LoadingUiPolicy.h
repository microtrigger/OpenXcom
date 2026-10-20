#pragma once
// Copyright 2026 OpenXcom contributors. GPL-3.0-or-later; see LICENSE.txt and LICENSE-NOTICES.md.
// Owner-only, SDL-neutral UI decisions. Not activated by Game in this slice.
#include "LoaderSession.h"
#include <cstddef>
#include <memory>
#include <thread>

namespace OpenXcom
{
/** Retains a real session; never implements thread completion or retirement.
 * Transfer sole session-owner operations to this policy while it exists. The
 * initial session must be unused, open and quiescent. Worker/backend bindings
 * must not call policy operations. All public operations check immutable owner
 * identity before owner state, and fail-stop on reentry. Invalid construction
 * throws before any begin. Preparation happens outside this class, after
 * reserveView, with no worker; only startPrepared may start it.
 *
 * Integration must finish the initiating callback, admitted commits, terminal
 * preparation and old-state retirement before startPrepared. On any failure it
 * must call preparationFailed, never accidentally fall through to begin. This
 * class provides the reservation, not proof of those Game callsite conditions.
 *
 * Drain at most remainingEventBudget events, then tick once per owner loop.
 * Do not dispatch ignored input or replay it later. recordBatch returns the
 * consumed prefix; remaining events belong to a later iteration. A directly
 * observed Quit is accepted even if this iteration exhausted its budget.
 * Events precede tick so observed Quit wins over same-iteration success.
 * Session polling never depends on focus/minimization or rendering success.
 *
 * PublishSuccess is a reserved navigation intent, not a repeated navigation
 * callback. The caller must transition once, then call transitionCompleted;
 * failure calls transitionFailed. View/result references contain owned values
 * only. They do not grant permission to read partial Game resources.
 */
class LoadingUiPolicy
{
public:
    enum FocusBits { InputFocus = 1, MouseFocus = 2, ApplicationActive = 4 };
    struct Config
    {
        bool allowResize;
        bool suppressStartupResize;
        int minimumWidth, minimumHeight, maximumWidth, maximumHeight;
        unsigned initialFocus;
        std::size_t maximumEventsPerTick;
        Config() noexcept;
    };
    enum class Phase { Idle, Preparing, Active, SuccessReserved, Failure,
                       Closing, Quarantined, Finished };
    enum class Admission { Accepted, Busy, Closed, LiveSave };
    enum class Decision { Wait, PublishSuccess, ShowFailure, FinishQuit, RetainQuarantine };
    enum class FixedFailure { None, Preparation, ResultCopy, MissingResult,
                              UnexpectedSession, Transition, Rendering };
    // All ordinary keys, fullscreen/grab/screenshot/debug/FPS/gameplay/mouse
    // events translate to KeyDown or SuppressedInput, never owner callbacks.
    enum class EventKind { Quit, Resize, StartupResize, Focus, Minimize,
                           Restore, Expose, KeyDown, SuppressedInput };
    struct EventValue
    {
        EventKind kind;
        int width, height;
        unsigned focus;
        explicit EventValue(EventKind event, int w = 0, int h = 0,
                            unsigned bits = 0) noexcept :
            kind(event), width(w), height(h), focus(bits) {}
    };
    struct View
    {
        Phase phase;
        FixedFailure fixedFailure;
        bool closeRequested, renderAllowed, repaintRequested, resizePending;
        int width, height;
        unsigned focus;
    };

    LoadingUiPolicy(std::shared_ptr<LoaderSession> session, Config config);
    ~LoadingUiPolicy() noexcept;
    Admission reserveView(bool hasLiveSave) noexcept;
    LoaderSession::BeginResult startPrepared() noexcept;
    void preparationFailed() noexcept;
    void transitionFailed() noexcept;
    void renderingFailed() noexcept;
    // False means no event was consumed; the caller must retain that event.
    bool record(const EventValue &event) noexcept;
    std::size_t recordBatch(const EventValue *events, std::size_t count) noexcept;
    std::size_t remainingEventBudget() const noexcept;
    Decision tick() noexcept;
    // Returns false unless the successful intent is still reserved and open.
    bool transitionCompleted() noexcept;
    View view() const noexcept;
    // Available only after checked quiescence and successful copy. The pointer
    // stays valid until the next accepted reservation or policy destruction.
    const LoaderSession::Result *result() const noexcept;
    bool takeRepaint() noexcept;
    LoadingUiPolicy(const LoadingUiPolicy &) = delete;
    LoadingUiPolicy &operator=(const LoadingUiPolicy &) = delete;
private:
    class Operation;
    bool recordValue(const EventValue &event) noexcept;
    void close(FixedFailure reason) noexcept;
    const std::shared_ptr<LoaderSession> _session;
    const Config _config;
    const std::thread::id _owner;
    mutable bool _inside;
    Phase _phase;
    FixedFailure _failure;
    bool _closeRequested, _renderAllowed, _repaint, _resizePending;
    bool _startupResizePending, _hasResult;
    int _width, _height;
    unsigned _focus;
    std::size_t _events;
    LoaderSession::Result _result;
};
}
