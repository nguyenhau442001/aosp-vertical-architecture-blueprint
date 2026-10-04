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
#include <string>

#include "cantransport/ICanTransport.h"

namespace blueprint::can {

// TCP client that receives 16-byte can_frame records from `cansim serve`.
// On the emulator: `adb reverse tcp:29536 tcp:29536`, then connect to 127.0.0.1.
class TcpCanTransport : public ICanTransport {
  public:
    TcpCanTransport(std::string host, uint16_t port);
    ~TcpCanTransport() override;

    bool open() override;
    void close() override;
    ReadResult read(CanFrame* frame, std::chrono::milliseconds timeout) override;
    bool write(const CanFrame& frame) override;
    std::string describe() const override;

  private:
    std::string mHost;
    uint16_t mPort;
    int mFd = -1;
    uint8_t mBuffer[kWireFrameSize] = {};
    size_t mFilled = 0;  // partial frame survives across read() timeouts
};

}  // namespace blueprint::can
