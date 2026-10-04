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

#pragma once

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "canbridge/SignalBinding.h"
#include "cantransport/CanFrame.h"

namespace blueprint::can {

// Pure logic, no threads, no I/O: frame in, property updates out.
//
// - Emits an update only when a value changes (VHAL ON_CHANGE semantics).
// - If a bound message is not seen for `timeoutNs`, emits available=false
//   once, so the VHAL can report the property as UNAVAILABLE.
class SignalRouter {
  public:
    struct Stats {
        uint64_t framesRouted = 0;
        uint64_t framesUnknown = 0;
        uint64_t dlcErrors = 0;
    };

    SignalRouter(std::vector<SignalBinding> bindings, int64_t timeoutNs);

    std::vector<PropertyUpdate> onFrame(const CanFrame& frame, int64_t nowNs);
    std::vector<PropertyUpdate> checkTimeouts(int64_t nowNs);

    const std::vector<SignalBinding>& bindings() const { return mBindings; }
    const Stats& stats() const { return mStats; }

  private:
    using Key = std::pair<int32_t, int32_t>;  // propId, areaId
    struct State {
        double value = 0;
        bool available = false;
        int64_t lastSeenNs = 0;
    };

    std::vector<SignalBinding> mBindings;
    int64_t mTimeoutNs;
    std::map<Key, State> mState;
    Stats mStats;
};

}  // namespace blueprint::can
