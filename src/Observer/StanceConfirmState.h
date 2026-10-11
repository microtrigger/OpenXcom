// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution, 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
#include "../Engine/State.h"
namespace OpenXcom { class Action; namespace Observer {
class StanceController;
// Pilot-only local approval. No remote protocol method can invoke this UI action.
class StanceConfirmState : public State {
    StanceController *_controller;
public:
    explicit StanceConfirmState(StanceController *controller);
    void approve(Action *action);
    void cancel(Action *action);
    void handle(Action *action);
};
} }
