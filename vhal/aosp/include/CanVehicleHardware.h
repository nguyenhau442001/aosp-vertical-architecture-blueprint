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

#include <IVehicleHardware.h>
#include <VehicleHalTypes.h>

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "canbridge/CanBridge.h"

namespace android::hardware::automotive::vehicle::canhw {

namespace aidlvhal = ::aidl::android::hardware::automotive::vehicle;

// IVehicleHardware decorator: properties bound to CAN signals (see
// vehicle/dbc/blueprint.dbc) are served from the CAN bridge, everything else
// is delegated to the wrapped hardware (FakeVehicleHardware by default), so
// the emulator keeps every other property it already had.
//
//   DefaultVehicleHal (AIDL IVehicle, Binder)
//          |
//   CanVehicleHardware  --- CAN-owned props --->  CanBridge -> ICanTransport
//          |
//   FakeVehicleHardware (all other props)
class CanVehicleHardware final : public IVehicleHardware {
  public:
    CanVehicleHardware(std::unique_ptr<IVehicleHardware> inner,
                       std::unique_ptr<::blueprint::can::ICanTransport> transport,
                       std::vector<::blueprint::can::SignalBinding> bindings);
    ~CanVehicleHardware() override;

    std::vector<aidlvhal::VehiclePropConfig> getAllPropertyConfigs() const override;
    aidlvhal::StatusCode setValues(std::shared_ptr<const SetValuesCallback> callback,
                                   const std::vector<aidlvhal::SetValueRequest>& requests) override;
    aidlvhal::StatusCode getValues(
            std::shared_ptr<const GetValuesCallback> callback,
            const std::vector<aidlvhal::GetValueRequest>& requests) const override;
    DumpResult dump(const std::vector<std::string>& options) override;
    aidlvhal::StatusCode checkHealth() override;
    void registerOnPropertyChangeEvent(
            std::unique_ptr<const PropertyChangeCallback> callback) override;
    void registerOnPropertySetErrorEvent(
            std::unique_ptr<const PropertySetErrorCallback> callback) override;
    std::chrono::nanoseconds getPropertyOnChangeEventBatchingWindow() override;
    aidlvhal::StatusCode subscribe(aidlvhal::SubscribeOptions options) override;
    aidlvhal::StatusCode unsubscribe(int32_t propId, int32_t areaId) override;
    aidlvhal::StatusCode updateSampleRate(int32_t propId, int32_t areaId,
                                          float sampleRate) override;

  private:
    using Key = std::pair<int32_t, int32_t>;  // propId, areaId

    struct Subscription {
        std::chrono::nanoseconds interval;  // zero = on-change only
        std::chrono::steady_clock::time_point next;
    };

    bool isCanOwned(int32_t propId, int32_t areaId) const;
    bool isCanOwnedProp(int32_t propId) const;
    void onCanUpdates(const std::vector<::blueprint::can::PropertyUpdate>& updates);
    void emit(std::vector<aidlvhal::VehiclePropValue> values);
    void refreshLoop();

    static aidlvhal::VehiclePropValue toPropValue(
            const ::blueprint::can::PropertyUpdate& update);

    std::unique_ptr<IVehicleHardware> mInner;
    std::vector<::blueprint::can::SignalBinding> mBindings;

    mutable std::mutex mLock;
    std::map<Key, aidlvhal::VehiclePropValue> mCache;  // last value from CAN
    std::map<Key, Subscription> mContinuousSubs;       // CAN-owned continuous props
    std::shared_ptr<const PropertyChangeCallback> mOnChange;

    std::unique_ptr<::blueprint::can::CanBridge> mBridge;

    std::atomic<bool> mRunning{true};
    std::condition_variable mRefreshCv;
    std::thread mRefreshThread;
};

}  // namespace android::hardware::automotive::vehicle::canhw
