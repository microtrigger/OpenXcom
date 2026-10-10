// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// Free software under GNU GPL version 3 or later; without warranty.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
#include "../../src/Observer/DiagnosticProfile.h"
#include "../../src/Engine/Logger.h"
namespace OpenXcom { namespace Options {
bool debug = false, debugUi = false, traceAI = false, verboseLogging = false;
}
namespace CrossPlatform { std::string now() { return "synthetic"; } }
}
inline void qualifyNormalDiagnostics()
{
    OpenXcom::Logger::reportingLevel() = OpenXcom::LOG_INFO;
    OpenXcom::Observer::initializeDiagnosticProfile();
}
