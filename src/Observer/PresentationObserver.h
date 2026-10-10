// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
#include "PresentedCursor.h"
namespace OpenXcom { namespace Observer {
// Transient, connected, read-only request channel. No discovery or listener.
class PresentationObserver
{
    int _fd;
    bool _hudEnabled;
    std::string _epoch, _admission, _challenge;
    bool sendPacket(const std::string &bytes);
public:
    explicit PresentationObserver(const char *descriptor);
    ~PresentationObserver();
    bool enabled() const { return _fd >= 0; }
    bool pending() const { return _fd >= 0 && !_challenge.empty(); }
    void resetPresentation();
    void beginFrame();
    bool hudEnabled() const { return _hudEnabled; }
    void presented(const PixelCrop &crop, const std::string &hud = "null");
    void close();
private:
    PresentationObserver(const PresentationObserver &);
    PresentationObserver &operator=(const PresentationObserver &);
};
} }
