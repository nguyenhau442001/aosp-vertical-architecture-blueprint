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

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "cantransport/CanFrame.h"
#include "cantransport/ICanTransport.h"
#include "cantransport/TcpCanTransport.h"

namespace blueprint::can {
namespace {

using std::chrono::milliseconds;

CanFrame thermalFrame() {
    CanFrame f;
    f.id = 0x3E8;
    f.dlc = 8;
    f.data = {0x79, 0x02, 0, 0, 0, 0, 0, 0};
    return f;
}

TEST(CanFrameTest, SerializeMatchesLinuxLayout) {
    uint8_t buf[kWireFrameSize];
    serializeFrame(thermalFrame(), buf);
    const uint8_t expected[kWireFrameSize] = {0xE8, 0x03, 0, 0, 8, 0, 0, 0,
                                              0x79, 0x02, 0, 0, 0, 0, 0, 0};
    EXPECT_EQ(0, memcmp(buf, expected, kWireFrameSize));

    CanFrame back;
    ASSERT_TRUE(deserializeFrame(buf, &back));
    EXPECT_EQ(back, thermalFrame());
}

TEST(CanFrameTest, DeserializeRejectsExtendedAndLongFrames) {
    uint8_t buf[kWireFrameSize] = {};
    CanFrame f;
    buf[3] = 0x80;  // CAN_EFF_FLAG
    EXPECT_FALSE(deserializeFrame(buf, &f));
    buf[3] = 0;
    buf[4] = 9;
    EXPECT_FALSE(deserializeFrame(buf, &f));
}

TEST(CanFrameTest, ToStringIsCandumpStyle) {
    EXPECT_EQ(toString(thermalFrame()), "3E8#7902000000000000");
    CanFrame empty;
    empty.id = 0x7;
    EXPECT_EQ(toString(empty), "007#");
}

TEST(CanFrameTest, ParseFrameRoundTripsToString) {
    auto frame = parseFrame("3E8#7902000000000000");
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(*frame, thermalFrame());
    EXPECT_EQ(parseFrame("7#")->dlc, 0);
    EXPECT_FALSE(parseFrame("800#00").has_value());
    EXPECT_FALSE(parseFrame("3E8#7").has_value());
    EXPECT_FALSE(parseFrame("3E8#0011223344556677AA").has_value());
    EXPECT_FALSE(parseFrame("3G8#00").has_value());
    EXPECT_FALSE(parseFrame("3E8").has_value());
}

TEST(CreateTransportTest, ParsesSpecs) {
    auto tcp = createTransport("tcp:127.0.0.1:29536");
    ASSERT_NE(tcp, nullptr);
    EXPECT_EQ(tcp->describe(), "tcp:127.0.0.1:29536");
    auto sc = createTransport("socketcan:vcan0");
    ASSERT_NE(sc, nullptr);
    EXPECT_EQ(sc->describe(), "socketcan:vcan0");

    EXPECT_EQ(createTransport("tcp:host"), nullptr);
    EXPECT_EQ(createTransport("tcp:host:99999"), nullptr);
    EXPECT_EQ(createTransport("socketcan:"), nullptr);
    EXPECT_EQ(createTransport("serial:/dev/ttyS0"), nullptr);
}

// Minimal TCP server standing in for `cansim serve`.
class TcpTransportTest : public ::testing::Test {
  protected:
    void SetUp() override {
        mListenFd = ::socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(mListenFd, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ASSERT_EQ(::bind(mListenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
        ASSERT_EQ(::listen(mListenFd, 1), 0);
        socklen_t len = sizeof(addr);
        ASSERT_EQ(::getsockname(mListenFd, reinterpret_cast<sockaddr*>(&addr), &len), 0);
        mPort = ntohs(addr.sin_port);
    }

    void TearDown() override {
        if (mPeerFd >= 0) ::close(mPeerFd);
        ::close(mListenFd);
    }

    void accept() {
        mPeerFd = ::accept(mListenFd, nullptr, nullptr);
        ASSERT_GE(mPeerFd, 0);
    }

    int mListenFd = -1;
    int mPeerFd = -1;
    uint16_t mPort = 0;
};

TEST_F(TcpTransportTest, ReassemblesFrameSplitAcrossReads) {
    TcpCanTransport transport("127.0.0.1", mPort);
    ASSERT_TRUE(transport.open());
    accept();

    uint8_t buf[kWireFrameSize];
    serializeFrame(thermalFrame(), buf);
    ASSERT_EQ(::send(mPeerFd, buf, 5, 0), 5);

    CanFrame got;
    EXPECT_EQ(transport.read(&got, milliseconds(50)), ReadResult::kTimeout);

    ASSERT_EQ(::send(mPeerFd, buf + 5, kWireFrameSize - 5, 0),
              static_cast<ssize_t>(kWireFrameSize - 5));
    ASSERT_EQ(transport.read(&got, milliseconds(500)), ReadResult::kFrame);
    EXPECT_EQ(got, thermalFrame());
}

TEST_F(TcpTransportTest, WriteSendsWireFormat) {
    TcpCanTransport transport("127.0.0.1", mPort);
    ASSERT_TRUE(transport.open());
    accept();

    ASSERT_TRUE(transport.write(thermalFrame()));
    uint8_t buf[kWireFrameSize];
    ASSERT_EQ(::recv(mPeerFd, buf, kWireFrameSize, MSG_WAITALL),
              static_cast<ssize_t>(kWireFrameSize));
    CanFrame got;
    ASSERT_TRUE(deserializeFrame(buf, &got));
    EXPECT_EQ(got, thermalFrame());
}

TEST_F(TcpTransportTest, ReportsClosedWhenPeerDisconnects) {
    TcpCanTransport transport("127.0.0.1", mPort);
    ASSERT_TRUE(transport.open());
    accept();
    ::close(mPeerFd);
    mPeerFd = -1;

    CanFrame got;
    EXPECT_EQ(transport.read(&got, milliseconds(500)), ReadResult::kClosed);
}

}  // namespace
}  // namespace blueprint::can
