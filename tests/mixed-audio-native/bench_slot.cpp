// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// Free software under GNU GPL version 3 or later; without warranty.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
#include "../../src/Observer/MixedAudioSlot.h"
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
using OpenXcom::Observer::MixedAudioSlot;
int main()
{
    typedef std::chrono::steady_clock Clock;
    MixedAudioSlot slot;
    unsigned char block[2048]; std::memset(block, 0x5a, sizeof(block));
    const unsigned idleCalls = 2000000, captures = 20000;
    auto start = Clock::now();
    for (unsigned i = 0; i < idleCalls; ++i) slot.callback(block, sizeof(block));
    double idleNs = std::chrono::duration<double, std::nano>(Clock::now() - start).count();
    volatile unsigned observed = 0;
    start = Clock::now();
    for (unsigned i = 0; i < captures; ++i)
    {
        assert(slot.arm());
        slot.callback(block, sizeof(block)); // Discard admission block.
        for (unsigned j = 0; j < 22; ++j) slot.callback(block, sizeof(block));
        observed += slot.readyData()[0];
        assert(slot.sent());
    }
    double activeNs = std::chrono::duration<double, std::nano>(Clock::now() - start).count();
    std::cout << "Synthetic cached-slot microbenchmark only: idle=" << idleNs / idleCalls
              << " ns/call (" << idleCalls << " calls, 0 bytes copied); active="
              << activeNs / (captures * 23) << " ns/call including arm/sent/inspection driver, "
              << captures << " captures * 44100 bytes=" << captures * 44100ULL
              << " total bytes copied, 23 calls/capture (1 skipped +22 copied), block=2048, tail=1092; "
              << "checksum=" << observed << "\n";
}
