// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AgentProtocol.h"
#include "AgentDisplayedMessages.h"
#include <algorithm>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>
namespace OpenXcom { namespace Observer {
// Public records only. This ring has no game objects, clock, RNG, callback IDs,
// simulation counters or command authority. Reads are non-consuming.
class AgentPublicEvents
{
public:
    enum { Capacity = 256, MaxEventBytes = 32768, PageBytes = 46000 };
    struct Record { std::uint64_t seq; std::string json; };
    struct Page {
        std::uint64_t oldest = 1, newest = 0, next = 0;
        bool gap = false;
        std::vector<Record> events;
    };
private:
    std::string _epoch, _lastPresentation;
    AgentDisplayedMessages _messages;
    std::uint64_t _newest = 0;
    std::deque<Record> _records;
    // Conservative canonical-ASCII upper bound without interpreting strings.
    static std::size_t asciiBound(const std::string &s)
    {
        std::size_t n = 0;
        for (unsigned char c : s) n += c < 128 ? 1 : 6;
        return n;
    }
public:
    bool start(const std::string &epoch)
    {
        if (!_epoch.empty() || !agentToken(epoch)) return false;
        _epoch = epoch; return true;
    }
    bool append(const std::string &publicBody)
    {
        if (_epoch.empty() || _newest >= 9007199254740991ULL ||
            asciiBound(publicBody) + 40 > MaxEventBytes) return false;
        Record record;
        record.seq = _newest + 1;
        record.json = "{\"seq\":" + std::to_string(record.seq) + "," + publicBody + "}";
        _records.push_back(record); _newest = record.seq;
        if (_records.size() > Capacity) _records.pop_front();
        return true;
    }
    bool presentation(const std::string &state, const std::string &visible)
    {
        const std::string body = "\"kind\":\"presentation\",\"state\":" + agentJsonString(state) +
            ",\"visible\":" + visible;
        if (body == _lastPresentation) return true;
        if (!append(body)) return false;
        _lastPresentation = body; return true;
    }
    bool displayedMessage(const std::string &text, bool knownAbsent)
    {
        const std::string body = _messages.observe(text, knownAbsent);
        return body.empty() || append(body);
    }
    bool read(const std::string &epoch, std::uint64_t after, unsigned limit, Page &page) const
    {
        if (_epoch.empty() || epoch != _epoch || after > _newest || !limit || limit > 16) return false;
        page = Page(); page.newest = _newest; page.next = after;
        page.oldest = _records.empty() ? 1 : _records.front().seq;
        page.gap = after + 1 < page.oldest;
        std::size_t bytes = 0;
        for (const Record &record : _records)
        {
            if (record.seq <= after) continue;
            const std::size_t size = asciiBound(record.json) + 1;
            if (page.events.size() == limit || bytes + size > PageBytes) break;
            page.events.push_back(record); bytes += size; page.next = record.seq;
        }
        return after == _newest || !page.events.empty();
    }
    std::uint64_t newest() const { return _newest; }
    std::size_t size() const { return _records.size(); }
};
} }
