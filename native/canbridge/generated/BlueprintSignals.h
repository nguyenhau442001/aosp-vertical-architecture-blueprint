// AUTO-GENERATED from vehicle/dbc/blueprint.dbc by `python3 -m cansim codegen`. DO NOT EDIT.
// Edit the DBC, then run: make codegen

#pragma once

#include "canbridge/SignalBinding.h"

namespace blueprint::can::generated {

// THERMAL_STATUS: id 0x3E8, 8 bytes, sender ClimateECU
inline constexpr SignalSpec kTHERMAL_STATUS_OutsideTemp{"OutsideTemp", 0, 16, ByteOrder::kIntel, false, 0.1, -40.0, -40.0, 125.0};
inline constexpr SignalSpec kTHERMAL_STATUS_CabinTempRow1Left{"CabinTempRow1Left", 16, 8, ByteOrder::kIntel, false, 0.5, -10.0, -10.0, 117.5};
inline constexpr SignalSpec kTHERMAL_STATUS_CabinTempRow1Right{"CabinTempRow1Right", 24, 8, ByteOrder::kIntel, false, 0.5, -10.0, -10.0, 117.5};
inline constexpr SignalSpec kTHERMAL_STATUS_EvaporatorTemp{"EvaporatorTemp", 39, 16, ByteOrder::kMotorola, true, 0.01, 0.0, -50.0, 150.0};
inline constexpr SignalSpec kTHERMAL_STATUS_ThermalCounter{"ThermalCounter", 56, 4, ByteOrder::kIntel, false, 1.0, 0.0, 0.0, 15.0};

inline constexpr SignalBinding kBindings[] = {
        // 0x11600703 = SYSTEM | GLOBAL | FLOAT | 0x0703
        {0x3E8, 8, &kTHERMAL_STATUS_OutsideTemp, 0x11600703, 0, "ENV_OUTSIDE_TEMPERATURE", ChangeMode::kContinuous, 1.0f, 2.0f},
        // 0x15600502 = SYSTEM | SEAT | FLOAT | 0x0502
        {0x3E8, 8, &kTHERMAL_STATUS_CabinTempRow1Left, 0x15600502, 49, "HVAC_TEMPERATURE_CURRENT", ChangeMode::kOnChange, 0.0f, 0.0f},
        // 0x15600502 = SYSTEM | SEAT | FLOAT | 0x0502
        {0x3E8, 8, &kTHERMAL_STATUS_CabinTempRow1Right, 0x15600502, 68, "HVAC_TEMPERATURE_CURRENT", ChangeMode::kOnChange, 0.0f, 0.0f},
};

}  // namespace blueprint::can::generated
