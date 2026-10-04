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

std::optional<CanFrame> parseFrame(const std::string& text) {
    size_t hash = text.find('#');
    if (hash == std::string::npos || hash == 0 || hash > 3) return std::nullopt;
    std::string payload = text.substr(hash + 1);
    if (payload.size() % 2 != 0 || payload.size() > 16) return std::nullopt;

    auto hexValue = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    CanFrame frame;
    for (size_t i = 0; i < hash; ++i) {
        int v = hexValue(text[i]);
        if (v < 0) return std::nullopt;
        frame.id = (frame.id << 4) | static_cast<uint32_t>(v);
    }
    if (frame.id > kStandardIdMask) return std::nullopt;
    frame.dlc = static_cast<uint8_t>(payload.size() / 2);
    for (uint8_t i = 0; i < frame.dlc; ++i) {
        int hi = hexValue(payload[2 * i]);
        int lo = hexValue(payload[2 * i + 1]);
        if (hi < 0 || lo < 0) return std::nullopt;
        frame.data[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return frame;
}

}  // namespace blueprint::can
