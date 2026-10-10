// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// Free software under GNU GPL version 3 or later; without warranty.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
namespace OpenXcom { namespace Observer {
// Main-thread-only. Call once after effective configuration and logging setup.
// Rejection is terminal for this process, including after options change back.
void initializeDiagnosticProfile();
bool diagnosticProfileQualified();
} }
