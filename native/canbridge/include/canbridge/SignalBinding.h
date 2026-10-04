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

#include "cansignal/Signal.h"

namespace blueprint::can {

// Values match aidl VehiclePropertyChangeMode.
enum class ChangeMode : int32_t { kOnChange = 1, kContinuous = 2 };

// One row of the routing table: "this bit field of this CAN message is that
// VHAL property/area". Generated from the DBC into generated/BlueprintSignals.h.
struct SignalBinding {
    uint32_t canId;
    uint8_t dlc;  // expected payload length
    const SignalSpec* signal;
    int32_t propId;
    int32_t areaId;  // 0 for VehicleArea::GLOBAL properties
    const char* propName;
    // Used only when VHAL has no config for propId (typical for VENDOR
    // properties): CanVehicleHardware then builds a READ config from these.
    ChangeMode changeMode;
    float minSampleRateHz;  // CONTINUOUS only
    float maxSampleRateHz;  // CONTINUOUS only
};

// What the bridge hands to the VHAL layer.
struct PropertyUpdate {
    int32_t propId;
    int32_t areaId;
    double value;
    bool available;  // false once the source ECU stops sending (signal timeout)
    int64_t timestampNs;
    const char* propName;
};

}  // namespace blueprint::can
