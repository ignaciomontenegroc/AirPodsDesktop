//
// AirPodsDesktop - AirPods Desktop User Experience Enhancement Program.
// Copyright (C) 2021-2022 SpriteOvO
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

#include <cstdint>
#include <optional>

#include "AirPods.h"

namespace Core::BatteryAlert {

// AirPods report battery in 10% steps, so alert levels are multiples of 10.
// Bit `n` of a mask selects the level `n * 10`%.
//
using LevelMask = uint32_t;

constexpr inline uint32_t kLevelStep = 10;
constexpr inline uint32_t kMinLevel = 10;
constexpr inline uint32_t kMaxLowLevel = 90;
constexpr inline uint32_t kMaxChargedLevel = 100;

constexpr LevelMask LevelBit(uint32_t percent)
{
    return LevelMask{1} << (percent / kLevelStep);
}

constexpr bool HasLevel(LevelMask mask, uint32_t percent)
{
    return (mask & LevelBit(percent)) != 0;
}

struct Levels {
    LevelMask podsLow{0}, podsCharged{0};
    LevelMask caseLow{0}, caseCharged{0};
};

// Tracks the alert levels of one battery source (the earbuds or the case).
//
// A low level fires when the draining battery is at or below it, including the first reading, and
// re-arms once the battery has clearly recovered. A charged level fires only when charging crosses
// it, and re-arms once the battery has dropped well below it again.
//
class Tracker
{
public:
    // `draining` is the lowest battery that is not charging, `charging` the lowest one that is.
    // Returns true if any enabled level fired.
    bool Update(
        std::optional<uint32_t> draining, std::optional<uint32_t> charging, LevelMask lowMask,
        LevelMask chargedMask);

private:
    LevelMask _lowFired{0};
    LevelMask _chargedArmed{0};
    LevelMask _chargedFired{0};
};

class Monitor
{
public:
    // Returns true if the AirPods window should be shown for an alert.
    bool OnStateUpdated(const AirPods::State &state, const Levels &levels);

private:
    Tracker _pods, _caseBox;
};

} // namespace Core::BatteryAlert
