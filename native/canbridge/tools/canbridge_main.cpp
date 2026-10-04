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

// bp-canbridge: run the full CAN -> property pipeline without VHAL and print
// what VHAL would receive. Same code path as the VHAL service, minus Binder.
//
//   bp-canbridge tcp:127.0.0.1:29536
//   bp-canbridge socketcan:vcan0

#include <csignal>
#include <cstdio>
#include <iterator>
#include <thread>

#include "canbridge/CanBridge.h"
#include "generated/BlueprintSignals.h"

using namespace blueprint::can;

namespace {
volatile std::sig_atomic_t gStop = 0;
void onSignal(int) {
    gStop = 1;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s tcp:HOST:PORT | socketcan:IFACE\n", argv[0]);
        return 2;
    }
    auto transport = createTransport(argv[1]);
    if (!transport) {
        std::fprintf(stderr, "invalid transport spec: %s\n", argv[1]);
        return 2;
    }
    std::string name = transport->describe();

    std::vector<SignalBinding> bindings(std::begin(generated::kBindings),
                                        std::end(generated::kBindings));
    std::fprintf(stderr, "bp-canbridge: %zu bindings, transport %s\n", bindings.size(),
                 name.c_str());
    for (const auto& b : bindings) {
        std::fprintf(stderr, "  0x%03X %-20s -> %s (0x%08X) area %d\n", b.canId, b.signal->name,
                     b.propName, b.propId, b.areaId);
    }

    CanBridge bridge(std::move(transport), bindings, [](const auto& updates) {
        for (const auto& u : updates) {
            if (u.available) {
                std::printf("%-26s area=%-3d value=%g\n", u.propName, u.areaId, u.value);
            } else {
                std::printf("%-26s area=%-3d UNAVAILABLE (signal timeout)\n", u.propName,
                            u.areaId);
            }
        }
        std::fflush(stdout);
    });

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    bridge.start();
    bool wasConnected = false;
    while (!gStop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (bridge.connected() != wasConnected) {
            wasConnected = bridge.connected();
            std::fprintf(stderr, "%s %s\n", wasConnected ? "connected to" : "lost", name.c_str());
        }
    }
    bridge.stop();
    auto stats = bridge.stats();
    std::fprintf(stderr, "routed=%llu unknown=%llu dlcErrors=%llu\n",
                 static_cast<unsigned long long>(stats.framesRouted),
                 static_cast<unsigned long long>(stats.framesUnknown),
                 static_cast<unsigned long long>(stats.dlcErrors));
    return 0;
}
