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

#include <cstddef>
#include <cstdint>
#include <optional>

namespace blueprint::can {

// DBC "@1" = Intel (little-endian), "@0" = Motorola (big-endian).
enum class ByteOrder : uint8_t { kIntel, kMotorola };

// One DBC SG_ line. startBit uses DBC numbering: LSB for Intel, MSB for Motorola.
struct SignalSpec {
    const char* name;
    uint16_t startBit;
    uint8_t length;  // 1..64
    ByteOrder order;
    bool isSigned;
    double factor;
    double offset;
    double minimum;  // minimum == maximum == 0 means "no range check"
    double maximum;
};

// Raw integer (sign-extended if isSigned). nullopt if the signal does not fit in len bytes.
std::optional<int64_t> extractRaw(const SignalSpec& spec, const uint8_t* data, size_t len);

// raw * factor + offset.
std::optional<double> decodePhysical(const SignalSpec& spec, const uint8_t* data, size_t len);

// Writes raw into its bit positions, leaving other bits untouched.
// False if raw does not fit the signal width or the signal does not fit in len bytes.
bool insertRaw(const SignalSpec& spec, int64_t raw, uint8_t* data, size_t len);

// round((physical - offset) / factor). nullopt if physical is outside [minimum, maximum].
std::optional<int64_t> physicalToRaw(const SignalSpec& spec, double physical);

}  // namespace blueprint::can
