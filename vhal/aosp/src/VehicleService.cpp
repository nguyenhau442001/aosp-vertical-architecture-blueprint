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

#define LOG_TAG "BlueprintCanVhal"

// Same shape as hardware/interfaces/automotive/vehicle/aidl/impl/vhal/src/VehicleService.cpp,
// with FakeVehicleHardware wrapped by CanVehicleHardware.

#include <DefaultVehicleHal.h>
#include <FakeVehicleHardware.h>
#include <android-base/properties.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <utils/Log.h>

#include <iterator>

#include "CanVehicleHardware.h"
#include "generated/BlueprintSignals.h"

using ::android::hardware::automotive::vehicle::DefaultVehicleHal;
using ::android::hardware::automotive::vehicle::canhw::CanVehicleHardware;
using ::android::hardware::automotive::vehicle::fake::FakeVehicleHardware;

namespace {
// Override at runtime: adb shell setprop persist.vendor.blueprint.can.transport socketcan:can0
constexpr char kTransportProp[] = "persist.vendor.blueprint.can.transport";
constexpr char kDefaultTransport[] = "tcp:127.0.0.1:29536";
constexpr char kInstance[] = "android.hardware.automotive.vehicle.IVehicle/default";
}  // namespace

int main(int /* argc */, char* /* argv */[]) {
    if (!ABinderProcess_setThreadPoolMaxThreadCount(4)) {
        ALOGE("failed to set thread pool max thread count");
        return 1;
    }
    ABinderProcess_startThreadPool();

    std::string spec = android::base::GetProperty(kTransportProp, kDefaultTransport);
    auto transport = ::blueprint::can::createTransport(spec);
    if (!transport) {
        ALOGE("invalid %s='%s'", kTransportProp, spec.c_str());
        return 1;
    }

    std::vector<::blueprint::can::SignalBinding> bindings(
            std::begin(::blueprint::can::generated::kBindings),
            std::end(::blueprint::can::generated::kBindings));
    auto hardware = std::make_unique<CanVehicleHardware>(std::make_unique<FakeVehicleHardware>(),
                                                         std::move(transport),
                                                         std::move(bindings));
    std::shared_ptr<DefaultVehicleHal> vhal =
            ::ndk::SharedRefBase::make<DefaultVehicleHal>(std::move(hardware));

    ALOGI("registering %s", kInstance);
    binder_exception_t err = AServiceManager_addService(vhal->asBinder().get(), kInstance);
    if (err != EX_NONE) {
        ALOGE("failed to register %s, exception %d", kInstance, err);
        return 1;
    }

    ABinderProcess_joinThreadPool();
    return 0;  // joinThreadPool never returns
}
