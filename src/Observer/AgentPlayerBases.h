// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <vector>
namespace OpenXcom {
class SavedGame;
class Language;
class Mod;
namespace Observer {
// Explicitly authorized, read-only own-base model access. No UI authority and
// no mutating operation is supplied by this collector. Caller owns wire epochs.
class AgentPlayerBases {
    struct Identity { std::string id; std::weak_ptr<const void> lifetime; bool tracked = false; };
    typedef std::pair<std::string, const void *> Key;
    std::map<Key, Identity> _identities;
    std::set<Key> _seen;
    std::map<std::tuple<std::string,std::string,std::string>,std::string> _logicalIdentities;
    std::string logicalIdentity(const std::string &kind,const std::string &scope,const std::string &type);
    SavedGame *_save = nullptr;
    std::uint64_t _next = 0;
    std::thread::id _thread;
    std::string identify(const std::string &kind, const void *pointer,
        const std::weak_ptr<const void> &lifetime = std::weak_ptr<const void>(), bool tracked = false);
public:
    // Main game thread only. Complete sorted typed records, never truncation.
    // Includes own monthly totals and researched MonthlyCosts rule ledgers;
    // at most 256 unique types per ledger list, within the record byte bound.
    // false clears records and supplies a bounded reason. No campaign is valid
    // failure, not a request to create/load a campaign.
    bool collect(SavedGame *save, Language *language, std::vector<std::string> &records, std::string &reason);
    // The active ruleset is needed for the native research-gated psi evaluation.
    // A missing ruleset fails closed for that evaluation (trained stats remain visible).
    bool collect(SavedGame *save, Language *language, std::vector<std::string> &records, std::string &reason, const Mod *mod);
    // Mandatory at every save/load/new-campaign boundary, including address ABA.
    void reset();
};
} }
#endif
