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

#include "canbridge/CanBridge.h"

#include <algorithm>
#include <ctime>

namespace blueprint::can {

int64_t elapsedRealtimeNs() {
    // VehiclePropValue.timestamp must use elapsedRealtimeNano(), i.e. CLOCK_BOOTTIME.
    timespec ts{};
#ifdef CLOCK_BOOTTIME
    clock_gettime(CLOCK_BOOTTIME, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return static_cast<int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

CanBridge::CanBridge(std::unique_ptr<ICanTransport> transport,
                     std::vector<SignalBinding> bindings, Listener listener, Options options)
    : mTransport(std::move(transport)),
      mListener(std::move(listener)),
      mOptions(options),
      mRouter(std::move(bindings),
              std::chrono::duration_cast<std::chrono::nanoseconds>(options.signalTimeout)
                      .count()) {}

CanBridge::~CanBridge() {
    stop();
}

void CanBridge::start() {
    if (mRunning.exchange(true)) return;
    mThread = std::thread(&CanBridge::run, this);
}

void CanBridge::stop() {
    if (!mRunning.exchange(false)) return;
    if (mThread.joinable()) mThread.join();
    mTransport->close();
    mConnected = false;
}

SignalRouter::Stats CanBridge::stats() const {
    std::lock_guard<std::mutex> lock(mRouterLock);
    return mRouter.stats();
}

bool CanBridge::sleepFor(std::chrono::milliseconds duration) {
    auto end = std::chrono::steady_clock::now() + duration;
    while (mRunning && std::chrono::steady_clock::now() < end) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return mRunning;
}

void CanBridge::run() {
    auto backoff = mOptions.reconnectMin;
    auto nextAttempt = std::chrono::steady_clock::now();
    while (mRunning) {
        ReadResult result = ReadResult::kClosed;
        CanFrame frame;
        if (!mConnected && std::chrono::steady_clock::now() >= nextAttempt) {
            if (mTransport->open()) {
                mConnected = true;
                backoff = mOptions.reconnectMin;
            } else {
                nextAttempt = std::chrono::steady_clock::now() + backoff;
                backoff = std::min(backoff * 2, mOptions.reconnectMax);
            }
        }
        if (mConnected) {
            result = mTransport->read(&frame, mOptions.readTimeout);
        }

        // Timeouts are checked even while disconnected: a dead link means the
        // ECU values are stale and VHAL must stop reporting them as AVAILABLE.
        std::vector<PropertyUpdate> updates;
        {
            std::lock_guard<std::mutex> lock(mRouterLock);
            int64_t now = elapsedRealtimeNs();
            if (result == ReadResult::kFrame) {
                updates = mRouter.onFrame(frame, now);
            }
            auto stale = mRouter.checkTimeouts(now);
            updates.insert(updates.end(), stale.begin(), stale.end());
        }
        if (!updates.empty()) mListener(updates);

        if (mConnected && (result == ReadResult::kClosed || result == ReadResult::kError)) {
            mTransport->close();
            mConnected = false;
            nextAttempt = std::chrono::steady_clock::now();  // first retry right away
        } else if (!mConnected && !sleepFor(mOptions.readTimeout)) {
            break;
        }
    }
}

}  // namespace blueprint::can
