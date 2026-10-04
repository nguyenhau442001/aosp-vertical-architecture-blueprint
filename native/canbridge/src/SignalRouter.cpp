/*
 * Copyright (C) 2026 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "canbridge/SignalRouter.h"

namespace blueprint::can {

SignalRouter::SignalRouter(std::vector<SignalBinding> bindings, int64_t timeoutNs)
    : mBindings(std::move(bindings)), mTimeoutNs(timeoutNs) {}

std::vector<PropertyUpdate> SignalRouter::onFrame(const CanFrame& frame, int64_t nowNs) {
    std::vector<PropertyUpdate> updates;
    bool matched = false;
    for (const auto& b : mBindings) {
        if (b.canId != frame.id) continue;
        if (!matched) {
            matched = true;
            if (frame.dlc < b.dlc) {
                ++mStats.dlcErrors;
                return updates;
            }
            ++mStats.framesRouted;
        }
        auto value = decodePhysical(*b.signal, frame.data.data(), frame.dlc);
        if (!value) continue;

        State& st = mState[{b.propId, b.areaId}];
        st.lastSeenNs = nowNs;
        if (st.available && st.value == *value) continue;
        st.value = *value;
        st.available = true;
        updates.push_back({b.propId, b.areaId, *value, true, nowNs, b.propName});
    }
    if (!matched) ++mStats.framesUnknown;
    return updates;
}

std::vector<PropertyUpdate> SignalRouter::checkTimeouts(int64_t nowNs) {
    std::vector<PropertyUpdate> updates;
    for (const auto& b : mBindings) {
        auto it = mState.find({b.propId, b.areaId});
        if (it == mState.end() || !it->second.available) continue;
        if (nowNs - it->second.lastSeenNs <= mTimeoutNs) continue;
        it->second.available = false;
        updates.push_back({b.propId, b.areaId, it->second.value, false, nowNs, b.propName});
    }
    return updates;
}

}  // namespace blueprint::can
