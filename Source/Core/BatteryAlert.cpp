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

#include "BatteryAlert.h"

#include <algorithm>

#include "../Logger.h"

namespace Core::BatteryAlert {

namespace {

// How far the battery must move away from a level that already fired before it can fire again.
// One step would let a reading that flickers between two values raise the alert repeatedly.
constexpr uint32_t kRearmDistance = kLevelStep * 2;

void TakeLowest(std::optional<uint32_t> &lowest, uint32_t value)
{
    lowest = lowest.has_value() ? std::min(lowest.value(), value) : value;
}

} // namespace

bool Tracker::Update(
    std::optional<uint32_t> draining, std::optional<uint32_t> charging, LevelMask lowMask,
    LevelMask chargedMask)
{
    bool fired = false;

    for (uint32_t level = kMinLevel; level <= kMaxChargedLevel; level += kLevelStep) {
        const auto bit = LevelBit(level);

        if (level <= kMaxLowLevel) {
            const bool atOrBelow = draining.has_value() && draining.value() <= level;

            if ((_lowFired & bit) != 0) {
                const bool recovered =
                    !atOrBelow &&
                    ((draining.has_value() && draining.value() >= level + kRearmDistance) ||
                     (charging.has_value() && charging.value() > level));
                if (recovered) {
                    _lowFired &= ~bit;
                }
            }
            else if (atOrBelow && HasLevel(lowMask, level)) {
                LOG(Info, "BatteryAlert: Battery dropped to {}%, level {}%.", draining.value(),
                    level);
                _lowFired |= bit;
                fired = true;
            }
        }

        if (charging.has_value()) {
            const auto value = charging.value();
            const bool wasFired = (_chargedFired & bit) != 0;

            if (value < level && (!wasFired || value + kRearmDistance <= level)) {
                _chargedArmed |= bit;
                _chargedFired &= ~bit;
            }
            else if (value >= level && (_chargedArmed & bit) != 0 && HasLevel(chargedMask, level)) {
                LOG(Info, "BatteryAlert: Battery charged to {}%, level {}%.", value, level);
                _chargedArmed &= ~bit;
                _chargedFired |= bit;
                fired = true;
            }
        }
    }

    return fired;
}

bool Monitor::OnStateUpdated(const AirPods::State &state, const Levels &levels)
{
    std::optional<uint32_t> podsDraining, podsCharging;
    for (const auto &pod : {state.pods.left, state.pods.right}) {
        if (pod.battery.Available()) {
            TakeLowest(pod.isCharging ? podsCharging : podsDraining, pod.battery.Value());
        }
    }

    std::optional<uint32_t> caseDraining, caseCharging;
    if (state.caseBox.battery.Available()) {
        (state.caseBox.isCharging ? caseCharging : caseDraining) = state.caseBox.battery.Value();
    }

    // Both trackers must run on every update so neither misses a reading.
    const bool podsFired =
        _pods.Update(podsDraining, podsCharging, levels.podsLow, levels.podsCharged);
    const bool caseFired =
        _caseBox.Update(caseDraining, caseCharging, levels.caseLow, levels.caseCharged);

    return podsFired || caseFired;
}

} // namespace Core::BatteryAlert
