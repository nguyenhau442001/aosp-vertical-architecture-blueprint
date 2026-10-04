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

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace blueprint::can {

// Portable mirror of Linux `struct can_frame` (classic CAN, 11-bit id only).
struct CanFrame {
    uint32_t id = 0;
    uint8_t dlc = 0;
    std::array<uint8_t, 8> data{};
};

bool operator==(const CanFrame& a, const CanFrame& b);

// Linux struct can_frame is 16 bytes: u32 can_id, u8 len, 3 pad, u8 data[8].
// The TCP transport streams exactly these 16 bytes (can_id little-endian), so
// a frame captured with candump and one sent by cansim look the same.
constexpr size_t kWireFrameSize = 16;

void serializeFrame(const CanFrame& frame, uint8_t out[kWireFrameSize]);
bool deserializeFrame(const uint8_t in[kWireFrameSize], CanFrame* frame);

// candump-style text: "3E8#7902000000000000".
std::string toString(const CanFrame& frame);

}  // namespace blueprint::can
