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

#include <gtest/gtest.h>

#include <array>

namespace blueprint::can {
namespace {

// Same vectors as tools/cansim/tests/test_dbc.py so Python and C++ agree.
constexpr SignalSpec kOutsideTemp{"OutsideTemp", 0, 16, ByteOrder::kIntel, false, 0.1, -40,
                                  -40, 125};
constexpr SignalSpec kEvaporatorTemp{"EvaporatorTemp", 39, 16, ByteOrder::kMotorola, true,
                                     0.01, 0, -50, 150};
constexpr SignalSpec kOddMotorola{"X", 11, 12, ByteOrder::kMotorola, false, 1, 0, 0, 0};
constexpr SignalSpec kCounter{"ThermalCounter", 56, 4, ByteOrder::kIntel, false, 1, 0, 0, 15};

TEST(SignalTest, IntelDecodeMatchesPrimerExample) {
    std::array<uint8_t, 8> data{0x79, 0x02};
    EXPECT_EQ(extractRaw(kOutsideTemp, data.data(), data.size()), 633);
    EXPECT_NEAR(*decodePhysical(kOutsideTemp, data.data(), data.size()), 23.3, 1e-9);
}

TEST(SignalTest, MotorolaSignedRoundTrip) {
    std::array<uint8_t, 8> data{};
    ASSERT_TRUE(insertRaw(kEvaporatorTemp, -1234, data.data(), data.size()));
    EXPECT_EQ(data[4], 0xFB);
    EXPECT_EQ(data[5], 0x2E);
    EXPECT_EQ(extractRaw(kEvaporatorTemp, data.data(), data.size()), -1234);
    EXPECT_NEAR(*decodePhysical(kEvaporatorTemp, data.data(), data.size()), -12.34, 1e-9);
}

TEST(SignalTest, MotorolaUnalignedCrossesByteBoundary) {
    std::array<uint8_t, 8> data{};
    ASSERT_TRUE(insertRaw(kOddMotorola, 0xABC, data.data(), data.size()));
    EXPECT_EQ(data[1], 0x0A);
    EXPECT_EQ(data[2], 0xBC);
    EXPECT_EQ(extractRaw(kOddMotorola, data.data(), data.size()), 0xABC);
}

TEST(SignalTest, InsertLeavesOtherBitsAlone) {
    std::array<uint8_t, 8> data;
    data.fill(0xFF);
    ASSERT_TRUE(insertRaw(kCounter, 0x5, data.data(), data.size()));
    EXPECT_EQ(data[7], 0xF5);
    EXPECT_EQ(data[6], 0xFF);
}

TEST(SignalTest, RejectsSignalBeyondPayload) {
    std::array<uint8_t, 2> shortData{0x79, 0x02};
    EXPECT_FALSE(extractRaw(kCounter, shortData.data(), shortData.size()).has_value());
    EXPECT_FALSE(insertRaw(kCounter, 1, shortData.data(), shortData.size()));
}

TEST(SignalTest, RejectsRawThatDoesNotFit) {
    std::array<uint8_t, 8> data{};
    EXPECT_FALSE(insertRaw(kCounter, 16, data.data(), data.size()));
    EXPECT_FALSE(insertRaw(kEvaporatorTemp, 40000, data.data(), data.size()));
}

TEST(SignalTest, PhysicalToRawHonoursRange) {
    EXPECT_EQ(physicalToRaw(kOutsideTemp, 23.3), 633);
    EXPECT_EQ(physicalToRaw(kOutsideTemp, -40), 0);
    EXPECT_FALSE(physicalToRaw(kOutsideTemp, 200).has_value());
    EXPECT_EQ(physicalToRaw(kOddMotorola, 1e6), 1000000);  // no range declared
}

}  // namespace
}  // namespace blueprint::can
