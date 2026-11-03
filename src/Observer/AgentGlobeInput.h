// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
namespace OpenXcom { class Game; class State; class Globe;
namespace Observer {
enum class AgentGlobeCancelReason { StateChange, FocusLost, Deadline, ManualInput, Init, Destroyed, Modal, ViewTick, ConnectionClosed, Quit };
bool agentGlobeInputHeld(Game *game);
// Operational ownership only: diagnostic trace capacity never gates input.
bool agentGlobeInputIdle(Game *game);
void agentCancelGlobeInput(Game *game, AgentGlobeCancelReason reason=AgentGlobeCancelReason::StateChange);
void agentCancelGlobeInputOwner(State *owner, AgentGlobeCancelReason reason=AgentGlobeCancelReason::StateChange);
void agentPollGlobeInput(Game *game);
void agentOwnedGlobeRotationTick(Globe *globe);
void agentCloseGlobeInput(Game *game);
void agentForgetGlobeInput(Game *game);
// Schema2 immutable lifetime prefix; loss/saturation is explicit, never motion proof.
// Fixture calls only after independently verified transport EOF. Throws if still open.
std::string agentGlobeInputAuditAfterEOF(Game *game);
} }
