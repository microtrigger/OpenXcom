// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AgentProtocol.h"
namespace OpenXcom { namespace Observer {
// Only complete presented text enters this reducer. No simulation references,
// message translation keys, private identities, clocks or input callbacks.
class AgentDisplayedMessages
{
    std::string _last;
    static bool plainPrefix(const std::string &text)
    {
        if (text.empty()) return false;
        for (std::size_t i = 0; i < text.size();)
        {
            unsigned char first = text[i++]; unsigned code = 0, remaining = 0, minimum = 0;
            if (first < 0x80) code = first;
            else if (first >= 0xc2 && first <= 0xdf) { code = first & 31; remaining = 1; minimum = 0x80; }
            else if (first >= 0xe0 && first <= 0xef) { code = first & 15; remaining = 2; minimum = 0x800; }
            else if (first >= 0xf0 && first <= 0xf4) { code = first & 7; remaining = 3; minimum = 0x10000; }
            else return false;
            if (remaining > text.size() - i) return false;
            while (remaining--)
            {
                unsigned char next = text[i++];
                if ((next & 0xc0) != 0x80) return false;
                code = (code << 6) | (next & 63);
            }
            if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff) ||
                code < 32 || (code >= 127 && code <= 159)) return false;
        }
        return true;
    }
public:
    static const char *classify(const std::string &text)
    {
        const std::size_t newline = text.find('\n');
        if (text.size() > 1024 || newline == std::string::npos || !plainPrefix(text.substr(0, newline))) return nullptr;
        const std::string suffix = text.substr(newline + 1);
        if (suffix == "is under alien control" || suffix == "is under Molecular Control") return "alien_control";
        if (suffix == "has Panicked") return "panic";
        if (suffix == "has gone Berserk") return "berserk";
        return nullptr;
    }
    std::string observe(const std::string &text, bool knownAbsent)
    {
        if (text.empty()) { if (knownAbsent) _last.clear(); return std::string(); }
        if (text.size() > 1024 || text == _last) return std::string();
        _last = text;
        const char *report = classify(text);
        return "\"kind\":\"displayed_message\",\"text\":" + agentJsonString(text) +
            ",\"report\":" + (report ? agentJsonString(report) : "null") +
            ",\"source\":null,\"position\":null";
    }
};
} }
