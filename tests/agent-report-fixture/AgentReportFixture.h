// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace OpenXcom { class Game;
namespace AgentReportFixture {
void prepare(int &argc, char **argv);
bool waiting();
bool start(Game *game);
} }
