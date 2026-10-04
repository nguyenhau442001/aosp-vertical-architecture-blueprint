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

#include "cantransport/CanFrame.h"

#include <cstdio>
#include <cstring>

namespace blueprint::can {

namespace {
constexpr uint32_t kStandardIdMask = 0x7FF;
constexpr uint32_t kFlagMask = 0xE0000000;  // EFF | RTR | ERR
}  // namespace

bool operator==(const CanFrame& a, const CanFrame& b) {
    return a.id == b.id && a.dlc == b.dlc &&
           std::memcmp(a.data.data(), b.data.data(), a.dlc) == 0;
}

void serializeFrame(const CanFrame& frame, uint8_t out[kWireFrameSize]) {
    std::memset(out, 0, kWireFrameSize);
    for (int i = 0; i < 4; ++i) out[i] = static_cast<uint8_t>(frame.id >> (8 * i));
    out[4] = frame.dlc;
    std::memcpy(out + 8, frame.data.data(), 8);
}

bool deserializeFrame(const uint8_t in[kWireFrameSize], CanFrame* frame) {
    uint32_t id = 0;
    for (int i = 0; i < 4; ++i) id |= static_cast<uint32_t>(in[i]) << (8 * i);
    if ((id & kFlagMask) != 0 || id > kStandardIdMask || in[4] > 8) return false;
    frame->id = id;
    frame->dlc = in[4];
    frame->data.fill(0);
    std::memcpy(frame->data.data(), in + 8, frame->dlc);
    return true;
}

std::string toString(const CanFrame& frame) {
    char buf[32];
    int n = std::snprintf(buf, sizeof(buf), "%03X#", frame.id);
    for (uint8_t i = 0; i < frame.dlc && i < 8; ++i) {
        n += std::snprintf(buf + n, sizeof(buf) - n, "%02X", frame.data[i]);
    }
    return std::string(buf, n);
}

}  // namespace blueprint::can
