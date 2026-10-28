// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace OpenXcom { class Game;
namespace AgentReportFixture {
// Explicit default-off bootstrap aliases only, including containment-stock-v1 and memorial-history-v1.
// Never an MCP mutation route and never applied to an existing campaign.
void prepare(int &argc, char **argv);
bool waiting();
bool start(Game *game);
} }
