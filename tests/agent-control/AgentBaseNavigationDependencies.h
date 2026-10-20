// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Explicit offline model dependencies for literal ordinary MiniBase draw.
#include <vector>
#include <string>
namespace OpenXcom {
struct RuleBaseFacility { int getSize() const { return 1; } };
struct BaseFacility { RuleBaseFacility rule; int getBuildTime() const{return 0;} int getX() const{return 1;} int getY() const{return 1;} RuleBaseFacility*getRules(){return &rule;} };
struct Base { std::vector<BaseFacility*> facilities; std::string name; unsigned reads=0; explicit Base(const std::string &n):name(n){} std::vector<BaseFacility*>*getFacilities(){++reads;return &facilities;} };
}
