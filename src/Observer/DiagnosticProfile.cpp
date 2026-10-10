// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// Free software under GNU GPL version 3 or later; without warranty.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
#include "DiagnosticProfile.h"
#include "../Engine/Options.h"
#include "../Engine/Logger.h"
namespace OpenXcom { namespace Observer {
namespace {
bool initialized = false;
bool rejected = false;
}
void initializeDiagnosticProfile()
{
    if (initialized) return;
    initialized = true;
    diagnosticProfileQualified();
}
bool diagnosticProfileQualified()
{
    if (!initialized || rejected) return false;
    const SeverityLevel level = Logger::reportingLevel();
    if (Options::debug || Options::debugUi || Options::traceAI || Options::verboseLogging ||
        level < LOG_FATAL || level > LOG_INFO) rejected = true;
    return !rejected;
}
} }
