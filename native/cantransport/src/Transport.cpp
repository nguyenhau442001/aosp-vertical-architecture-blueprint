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

#include "cantransport/ICanTransport.h"

#include <cstdlib>

#include "cantransport/SocketCanTransport.h"
#include "cantransport/TcpCanTransport.h"

namespace blueprint::can {

std::unique_ptr<ICanTransport> createTransport(const std::string& spec) {
    constexpr char kSocketCan[] = "socketcan:";
    constexpr char kTcp[] = "tcp:";

    if (spec.rfind(kSocketCan, 0) == 0) {
        std::string ifname = spec.substr(sizeof(kSocketCan) - 1);
        if (ifname.empty()) return nullptr;
        return std::make_unique<SocketCanTransport>(ifname);
    }
    if (spec.rfind(kTcp, 0) == 0) {
        std::string rest = spec.substr(sizeof(kTcp) - 1);
        size_t colon = rest.rfind(':');
        if (colon == std::string::npos || colon == 0) return nullptr;
        char* end = nullptr;
        long port = std::strtol(rest.c_str() + colon + 1, &end, 10);
        if (*end != '\0' || port <= 0 || port > 65535) return nullptr;
        return std::make_unique<TcpCanTransport>(rest.substr(0, colon),
                                                 static_cast<uint16_t>(port));
    }
    return nullptr;
}

}  // namespace blueprint::can
