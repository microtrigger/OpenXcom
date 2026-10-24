// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <memory>
namespace OpenXcom { namespace Observer {
// Observational identity only. Does not own Target or change native fields.
// Weak grants keep the control-block identity, not the target, alive.
class AgentTargetLifetime {
    std::shared_ptr<const void> _token;
    static std::shared_ptr<const void> fresh() { return std::make_shared<const char>(0); }
public:
    AgentTargetLifetime() : _token(fresh()) {}
    AgentTargetLifetime(const AgentTargetLifetime &) : _token(fresh()) {}
    AgentTargetLifetime &operator=(const AgentTargetLifetime &)
    { _token = fresh(); return *this; }
    std::weak_ptr<const void> token() const { return _token; }
    void invalidate() { _token.reset(); }
};
} }
