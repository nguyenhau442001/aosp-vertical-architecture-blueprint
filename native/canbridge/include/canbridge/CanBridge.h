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

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "canbridge/SignalRouter.h"
#include "cantransport/ICanTransport.h"

namespace blueprint::can {

// Owns the reader thread: transport -> SignalRouter -> listener.
// Reconnects with backoff when the transport drops (simulator restarted,
// cable unplugged, CAN controller reset).
class CanBridge {
  public:
    using Listener = std::function<void(const std::vector<PropertyUpdate>&)>;

    struct Options {
        std::chrono::milliseconds readTimeout{100};
        std::chrono::milliseconds signalTimeout{1000};
        std::chrono::milliseconds reconnectMin{200};
        std::chrono::milliseconds reconnectMax{2000};
    };

    CanBridge(std::unique_ptr<ICanTransport> transport, std::vector<SignalBinding> bindings,
              Listener listener, Options options);
    CanBridge(std::unique_ptr<ICanTransport> transport, std::vector<SignalBinding> bindings,
              Listener listener)
        : CanBridge(std::move(transport), std::move(bindings), std::move(listener), Options{}) {}
    ~CanBridge();

    void start();
    void stop();

    bool connected() const { return mConnected; }
    SignalRouter::Stats stats() const;

  private:
    void run();
    bool sleepFor(std::chrono::milliseconds duration);  // false if stopped meanwhile

    std::unique_ptr<ICanTransport> mTransport;
    Listener mListener;
    Options mOptions;

    mutable std::mutex mRouterLock;
    SignalRouter mRouter;

    std::atomic<bool> mRunning{false};
    std::atomic<bool> mConnected{false};
    std::thread mThread;
};

int64_t elapsedRealtimeNs();

}  // namespace blueprint::can
