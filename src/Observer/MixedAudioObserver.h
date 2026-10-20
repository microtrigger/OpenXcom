// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// Free software under GNU GPL version 3 or later; without warranty.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
#include "MixedAudioSlot.h"
#include <chrono>
#include <string>
#include <SDL.h>

#ifdef OPENXCOM_LOADING_AUDIO_HANDOFF
namespace OpenXcom { class LoadingAudioHandoff; }
#endif

namespace OpenXcom { namespace Observer {
// Independent, inherited connected socket. Never opens a listener or file log.
// Public methods are main-thread-only except publishCatalogSupported().
// A selected helper may exclusively reset retained same-context state after drain/detach.
class MixedAudioObserver
{
    typedef std::chrono::steady_clock Clock;
    int _fd;
    bool _attached, _helloSent, _admitted;
    std::atomic<unsigned> _catalog; // 0=not loaded, 1=supported, 2=unsupported.
    MixedAudioSlot _slot;
    std::string _epoch, _admission, _challenge;
    Clock::time_point _started, _lastAdmission;
    bool sendPacket(const std::string &packet);
    void pollImpl();
    void closeDetachedState() noexcept;
#ifdef OPENXCOM_LOADING_AUDIO_HANDOFF
    friend class OpenXcom::LoadingAudioHandoff;
    enum class LoadingReset { BootstrapPreserved, EpochClosed, DisabledRetained };
    // Private exclusive worker transfer; never an owner API or cleanup fallback.
    LoadingReset resetForExclusiveLoadingAfterPostmixDetach() noexcept;
#ifdef OPENXCOM_RESPONSIVE_LOADING
    void terminalCloseForExclusiveLoadingAfterPostmixDetach() noexcept;
#endif
#endif
    static void SDLCALL postmix(void *context, Uint8 *stream, int len);
public:
    explicit MixedAudioObserver(const char *descriptor);
    ~MixedAudioObserver();
    bool enabled() const { return _fd >= 0; }
    void poll();
    // Called on the resource-loading thread after inspecting the entire catalog.
    // No main-thread resource, Music, Mod or active-track reads are necessary.
    void publishCatalogSupported(bool supported);
    // Call before any audio reopen; resource reload also invalidates catalog.
    // Pre-hello startup reopens do not extend the absolute startup deadline.
    void beforeAudioReset(bool resourceReload = false);
    void close();
private:
    MixedAudioObserver(const MixedAudioObserver &);
    MixedAudioObserver &operator=(const MixedAudioObserver &);
};
} }
