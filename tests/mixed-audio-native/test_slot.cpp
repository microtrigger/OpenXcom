// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// Free software under GNU GPL version 3 or later; without warranty.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
#include "../../src/Observer/MixedAudioSlot.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
using OpenXcom::Observer::MixedAudioSlot;
int main()
{
    typedef MixedAudioSlot Slot;
    Slot slot;
    assert(slot.lockFree());
    std::vector<unsigned char> block(Slot::MaxCallbackBytes, 0x7b);
    slot.callback(block.data(), 1024); // No pre-request history.
    assert(slot.state() == Slot::IDLE && !slot.readyData());
    for (unsigned round = 0; round < 200; ++round)
    {
        assert(slot.arm());
        assert(!slot.arm());
        slot.callback(block.data(), 1024); // Deliberately nonzero admission block.
        assert(slot.state() == Slot::CAPTURING && !slot.readyData());
        std::vector<unsigned char> expected;
        unsigned emitted = 0;
        while (expected.size() < Slot::Bytes)
        {
            const unsigned frames = 1 + ((emitted * 13 + round * 71) % 3101);
            for (unsigned i = 0; i < frames * 4; ++i) block[i] = (emitted + i + round) % 251;
            const std::vector<unsigned char> before(block);
            const unsigned take = std::min<unsigned>(frames * 4, Slot::Bytes - expected.size());
            expected.insert(expected.end(), block.begin(), block.begin() + take);
            slot.callback(block.data(), frames * 4);
            assert(block == before); // Never modifies SDL output.
            emitted += frames * 4;
        }
        assert(slot.state() == Slot::READY);
        assert(std::equal(expected.begin(), expected.end(), slot.readyData()));
        std::fill(block.begin(), block.end(), 0xab);
        slot.callback(block.data(), block.size()); // Neither tail nor next block retained.
        assert(std::equal(expected.begin(), expected.end(), slot.readyData()));
        assert(slot.sent());
    }
    for (int len : {0, -4, 3, Slot::MaxCallbackBytes + 4})
    {
        Slot invalid;
        assert(invalid.arm());
        invalid.callback(block.data(), len);
        assert(invalid.state() == Slot::FAILED);
    }
    Slot nullStream;
    assert(nullStream.arm());
    nullStream.callback(0, 4);
    assert(nullStream.state() == Slot::FAILED);
    Slot silent;
    std::fill(block.begin(), block.end(), 0);
    assert(silent.arm());
    silent.callback(block.data(), 4);
    silent.callback(block.data(), Slot::MaxCallbackBytes);
    assert(silent.state() == Slot::READY);
    for (unsigned i = 0; i < Slot::Bytes; ++i) assert(silent.readyData()[i] == 0);
    silent.closeAfterDetach();
    assert(!silent.arm());
    // Real producer/consumer ownership handoffs, not a non-atomic seqlock.
    Slot stress;
    std::atomic<bool> stop(false);
    std::thread producer([&]() {
        unsigned char pcm[4096];
        std::fill(pcm, pcm + sizeof(pcm), 0x51);
        while (!stop.load(std::memory_order_relaxed)) stress.callback(pcm, sizeof(pcm));
    });
    for (unsigned round = 0; round < 3000; ++round)
    {
        assert(stress.arm());
        while (stress.state() != Slot::READY) std::this_thread::yield();
        const unsigned char *pcm = stress.readyData();
        for (unsigned i = 0; i < Slot::Bytes; ++i) assert(pcm[i] == 0x51);
        assert(stress.sent());
    }
    stop.store(true, std::memory_order_relaxed);
    producer.join(); // Equivalent ownership precondition to synchronized detach.
    stress.closeAfterDetach();
    std::cout << "PASS slot: 200 varied captures, silence, malformed callbacks, no tail/history, unchanged source; 3000 threaded handoffs\n";
}
