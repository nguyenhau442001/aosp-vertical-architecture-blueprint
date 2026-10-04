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

#include "cansignal/Signal.h"

#include <cmath>

namespace blueprint::can {

namespace {

// Absolute bit position (byte * 8 + bit) of the i-th bit counted from the signal MSB.
// Motorola walks down inside a byte, then jumps to bit 7 of the next byte.
uint32_t bitPosition(const SignalSpec& spec, uint32_t i) {
    if (spec.order == ByteOrder::kIntel) {
        return spec.startBit + (spec.length - 1 - i);
    }
    uint32_t pos = spec.startBit;
    for (uint32_t k = 0; k < i; ++k) {
        pos = (pos % 8 == 0) ? pos + 15 : pos - 1;
    }
    return pos;
}

bool fits(const SignalSpec& spec, size_t len) {
    if (spec.length == 0 || spec.length > 64) return false;
    for (uint32_t i = 0; i < spec.length; ++i) {
        if (bitPosition(spec, i) / 8 >= len) return false;
    }
    return true;
}

bool hasRange(const SignalSpec& spec) {
    return !(spec.minimum == 0.0 && spec.maximum == 0.0);
}

}  // namespace

std::optional<int64_t> extractRaw(const SignalSpec& spec, const uint8_t* data, size_t len) {
    if (data == nullptr || !fits(spec, len)) return std::nullopt;
    uint64_t raw = 0;
    for (uint32_t i = 0; i < spec.length; ++i) {
        uint32_t pos = bitPosition(spec, i);
        raw = (raw << 1) | ((data[pos / 8] >> (pos % 8)) & 1u);
    }
    if (spec.isSigned && spec.length < 64 && (raw & (uint64_t{1} << (spec.length - 1)))) {
        raw |= ~uint64_t{0} << spec.length;  // sign-extend
    }
    return static_cast<int64_t>(raw);
}

std::optional<double> decodePhysical(const SignalSpec& spec, const uint8_t* data, size_t len) {
    auto raw = extractRaw(spec, data, len);
    if (!raw) return std::nullopt;
    return static_cast<double>(*raw) * spec.factor + spec.offset;
}

bool insertRaw(const SignalSpec& spec, int64_t raw, uint8_t* data, size_t len) {
    if (data == nullptr || !fits(spec, len)) return false;
    if (spec.length < 64) {
        int64_t lo = spec.isSigned ? -(int64_t{1} << (spec.length - 1)) : 0;
        int64_t hi = spec.isSigned ? (int64_t{1} << (spec.length - 1)) - 1
                                   : (int64_t{1} << spec.length) - 1;
        if (raw < lo || raw > hi) return false;
    }
    uint64_t bits = static_cast<uint64_t>(raw);
    for (uint32_t i = 0; i < spec.length; ++i) {
        uint32_t pos = bitPosition(spec, i);
        uint32_t valueBit = spec.length - 1 - i;
        uint8_t mask = static_cast<uint8_t>(1u << (pos % 8));
        if ((bits >> valueBit) & 1u) {
            data[pos / 8] |= mask;
        } else {
            data[pos / 8] &= static_cast<uint8_t>(~mask);
        }
    }
    return true;
}

std::optional<int64_t> physicalToRaw(const SignalSpec& spec, double physical) {
    if (spec.factor == 0.0 || std::isnan(physical)) return std::nullopt;
    if (hasRange(spec) && (physical < spec.minimum || physical > spec.maximum)) {
        return std::nullopt;
    }
    return static_cast<int64_t>(std::llround((physical - spec.offset) / spec.factor));
}

}  // namespace blueprint::can
