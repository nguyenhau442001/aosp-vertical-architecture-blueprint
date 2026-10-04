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

// bp-candump: print every frame a transport delivers. Use it to check the
// transport layer alone before adding decoding on top.
//
//   bp-candump tcp:127.0.0.1:29536
//   bp-candump socketcan:vcan0

#include <cstdio>

#include "cantransport/ICanTransport.h"

using namespace blueprint::can;

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
    if (!transport->open()) {
        std::fprintf(stderr, "cannot open %s\n", transport->describe().c_str());
        return 1;
    }
    std::fprintf(stderr, "listening on %s\n", transport->describe().c_str());

    CanFrame frame;
    while (true) {
        switch (transport->read(&frame, std::chrono::milliseconds(1000))) {
            case ReadResult::kFrame:
                std::printf("%s\n", toString(frame).c_str());
                std::fflush(stdout);
                break;
            case ReadResult::kTimeout:
                break;
            case ReadResult::kClosed:
                std::fprintf(stderr, "transport closed\n");
                return 0;
            case ReadResult::kError:
                std::fprintf(stderr, "transport error\n");
                return 1;
        }
    }
}
