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

#include <gtest/gtest.h>

#include <condition_variable>
#include <deque>
#include <iterator>

#include "canbridge/CanBridge.h"
#include "canbridge/SignalRouter.h"
#include "generated/BlueprintSignals.h"

namespace blueprint::can {
namespace {

using namespace std::chrono_literals;

constexpr int32_t kEnvOutsideTemperature = 0x11600703;
constexpr int32_t kHvacTemperatureCurrent = 0x15600502;
constexpr int64_t kMs = 1000000;

std::vector<SignalBinding> allBindings() {
    return {std::begin(generated::kBindings), std::end(generated::kBindings)};
}

CanFrame thermal(uint8_t b0, uint8_t b1, uint8_t left = 0, uint8_t right = 0) {
    CanFrame f;
    f.id = 0x3E8;
    f.dlc = 8;
    f.data = {b0, b1, left, right, 0, 0, 0, 0};
    return f;
}

TEST(SignalRouterTest, RoutesGeneratedBindings) {
    SignalRouter router(allBindings(), 1000 * kMs);
    // OutsideTemp raw 633 -> 23.3, cabin left raw 63 -> 21.5, right raw 68 -> 24.0
    auto updates = router.onFrame(thermal(0x79, 0x02, 63, 68), 1);
    ASSERT_EQ(updates.size(), 3u);
    EXPECT_EQ(updates[0].propId, kEnvOutsideTemperature);
    EXPECT_EQ(updates[0].areaId, 0);
    EXPECT_NEAR(updates[0].value, 23.3, 1e-9);
    EXPECT_EQ(updates[1].propId, kHvacTemperatureCurrent);
    EXPECT_EQ(updates[1].areaId, 49);
    EXPECT_NEAR(updates[1].value, 21.5, 1e-9);
    EXPECT_EQ(updates[2].areaId, 68);
    EXPECT_NEAR(updates[2].value, 24.0, 1e-9);
    EXPECT_TRUE(updates[0].available);
}

TEST(SignalRouterTest, EmitsOnlyOnChange) {
    SignalRouter router(allBindings(), 1000 * kMs);
    router.onFrame(thermal(0x79, 0x02, 63, 68), 1);
    EXPECT_TRUE(router.onFrame(thermal(0x79, 0x02, 63, 68), 2).empty());
    auto updates = router.onFrame(thermal(0x7A, 0x02, 63, 68), 3);
    ASSERT_EQ(updates.size(), 1u);
    EXPECT_NEAR(updates[0].value, 23.4, 1e-9);
}

TEST(SignalRouterTest, CountsUnknownAndShortFrames) {
    SignalRouter router(allBindings(), 1000 * kMs);
    CanFrame other;
    other.id = 0x123;
    other.dlc = 8;
    EXPECT_TRUE(router.onFrame(other, 1).empty());
    CanFrame shortFrame = thermal(0x79, 0x02);
    shortFrame.dlc = 2;
    EXPECT_TRUE(router.onFrame(shortFrame, 1).empty());
    EXPECT_EQ(router.stats().framesUnknown, 1u);
    EXPECT_EQ(router.stats().dlcErrors, 1u);
    EXPECT_EQ(router.stats().framesRouted, 0u);
}

TEST(SignalRouterTest, TimeoutMarksUnavailableOnceThenRecovers) {
    SignalRouter router(allBindings(), 100 * kMs);
    router.onFrame(thermal(0x79, 0x02, 63, 68), 0);
    EXPECT_TRUE(router.checkTimeouts(50 * kMs).empty());

    auto stale = router.checkTimeouts(150 * kMs);
    ASSERT_EQ(stale.size(), 3u);
    EXPECT_FALSE(stale[0].available);
    EXPECT_TRUE(router.checkTimeouts(300 * kMs).empty());

    // Same value comes back: must be re-announced as available.
    auto back = router.onFrame(thermal(0x79, 0x02, 63, 68), 400 * kMs);
    ASSERT_EQ(back.size(), 3u);
    EXPECT_TRUE(back[0].available);
}

// Scripted transport: hands out queued frames, then reports kClosed.
class FakeTransport : public ICanTransport {
  public:
    std::deque<CanFrame> frames;
    int failOpens = 0;
    bool closeWhenEmpty = false;  // simulate the link dropping after the queue drains
    std::atomic<int> opens{0};

    bool open() override {
        ++opens;
        if (closeWhenEmpty && opens > 1) return false;
        return failOpens-- <= 0;
    }
    void close() override {}
    ReadResult read(CanFrame* frame, std::chrono::milliseconds timeout) override {
        if (frames.empty()) {
            if (closeWhenEmpty) return ReadResult::kClosed;
            std::this_thread::sleep_for(timeout);
            return ReadResult::kTimeout;
        }
        *frame = frames.front();
        frames.pop_front();
        return ReadResult::kFrame;
    }
    bool write(const CanFrame&) override { return true; }
    std::string describe() const override { return "fake"; }
};

class Collector {
  public:
    void operator()(const std::vector<PropertyUpdate>& updates) {
        std::lock_guard<std::mutex> lock(mLock);
        mUpdates.insert(mUpdates.end(), updates.begin(), updates.end());
        mCv.notify_all();
    }
    bool waitFor(size_t count, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mLock);
        return mCv.wait_for(lock, timeout, [&] { return mUpdates.size() >= count; });
    }
    std::vector<PropertyUpdate> updates() {
        std::lock_guard<std::mutex> lock(mLock);
        return mUpdates;
    }

  private:
    std::mutex mLock;
    std::condition_variable mCv;
    std::vector<PropertyUpdate> mUpdates;
};

TEST(CanBridgeTest, RetriesOpenThenDeliversUpdates) {
    auto transport = std::make_unique<FakeTransport>();
    transport->failOpens = 2;
    transport->frames.push_back(thermal(0x79, 0x02, 63, 68));
    FakeTransport* raw = transport.get();

    Collector collector;
    CanBridge::Options options;
    options.reconnectMin = 10ms;
    options.readTimeout = 10ms;
    CanBridge bridge(std::move(transport), allBindings(),
                     [&](const auto& u) { collector(u); }, options);
    bridge.start();
    ASSERT_TRUE(collector.waitFor(3, 2s));
    bridge.stop();

    EXPECT_EQ(raw->opens.load(), 3);
    EXPECT_NEAR(collector.updates()[0].value, 23.3, 1e-9);
    EXPECT_EQ(bridge.stats().framesRouted, 1u);
}

TEST(CanBridgeTest, ReportsUnavailableWhenEcuGoesSilent) {
    auto transport = std::make_unique<FakeTransport>();
    transport->frames.push_back(thermal(0x79, 0x02, 63, 68));

    Collector collector;
    CanBridge::Options options;
    options.readTimeout = 10ms;
    options.signalTimeout = 50ms;
    CanBridge bridge(std::move(transport), allBindings(),
                     [&](const auto& u) { collector(u); }, options);
    bridge.start();
    ASSERT_TRUE(collector.waitFor(6, 2s));
    bridge.stop();

    auto updates = collector.updates();
    EXPECT_TRUE(updates[0].available);
    EXPECT_FALSE(updates[3].available);
}

TEST(CanBridgeTest, ReportsUnavailableWhileDisconnected) {
    auto transport = std::make_unique<FakeTransport>();
    transport->frames.push_back(thermal(0x79, 0x02, 63, 68));
    transport->closeWhenEmpty = true;

    Collector collector;
    CanBridge::Options options;
    options.readTimeout = 10ms;
    options.signalTimeout = 50ms;
    options.reconnectMin = 10ms;
    CanBridge bridge(std::move(transport), allBindings(),
                     [&](const auto& u) { collector(u); }, options);
    bridge.start();
    ASSERT_TRUE(collector.waitFor(6, 2s));
    bridge.stop();

    auto updates = collector.updates();
    EXPECT_FALSE(updates[3].available);
}

}  // namespace
}  // namespace blueprint::can
