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

#include <chrono>
#include <memory>
#include <string>

#include "cantransport/CanFrame.h"

namespace blueprint::can {

enum class ReadResult { kFrame, kTimeout, kClosed, kError };

// Where CAN frames come from. The decoder above never knows which one it is.
class ICanTransport {
  public:
    virtual ~ICanTransport() = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual ReadResult read(CanFrame* frame, std::chrono::milliseconds timeout) = 0;
    virtual bool write(const CanFrame& frame) = 0;
    virtual std::string describe() const = 0;
};

// "socketcan:vcan0" or "tcp:127.0.0.1:29536". nullptr if the spec is invalid
// or the backend is not available on this platform.
std::unique_ptr<ICanTransport> createTransport(const std::string& spec);

}  // namespace blueprint::can
