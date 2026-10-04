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

#include "CanVehicleHardware.h"

#include <VehicleUtils.h>
#include <android-base/stringprintf.h>
#include <utils/Log.h>

#include <algorithm>
#include <cmath>
#include <set>

namespace android::hardware::automotive::vehicle::canhw {

using ::android::base::StringAppendF;
using ::blueprint::can::CanBridge;
using ::blueprint::can::elapsedRealtimeNs;
using ::blueprint::can::ICanTransport;
using ::blueprint::can::parseFrame;
using ::blueprint::can::PropertyUpdate;
using ::blueprint::can::SignalBinding;

using aidlvhal::GetValueRequest;
using aidlvhal::GetValueResult;
using aidlvhal::SetValueRequest;
using aidlvhal::StatusCode;
using aidlvhal::SubscribeOptions;
using aidlvhal::VehiclePropConfig;
using aidlvhal::VehiclePropertyStatus;
using aidlvhal::VehiclePropertyType;
using aidlvhal::VehiclePropValue;

CanVehicleHardware::CanVehicleHardware(std::unique_ptr<IVehicleHardware> inner,
                                       std::unique_ptr<ICanTransport> transport,
                                       std::vector<SignalBinding> bindings)
    : mInner(std::move(inner)), mBindings(std::move(bindings)) {
    // A binding is useless if VHAL has no config for it: clients cannot see it.
    std::set<int32_t> configured;
    for (const auto& config : mInner->getAllPropertyConfigs()) configured.insert(config.prop);
    for (const auto& b : mBindings) {
        if (configured.count(b.propId) == 0) {
            ALOGW("%s (0x%08x) is bound to CAN signal %s but has no VehiclePropConfig; "
                  "add it to the VHAL config JSON",
                  b.propName, b.propId, b.signal->name);
        }
    }

    std::string name = transport->describe();
    mBridge = std::make_unique<CanBridge>(
            std::move(transport), mBindings,
            [this](const std::vector<PropertyUpdate>& updates) { onCanUpdates(updates); });
    mRefreshThread = std::thread(&CanVehicleHardware::refreshLoop, this);
    mBridge->start();
    ALOGI("CAN bridge started on %s with %zu bindings", name.c_str(), mBindings.size());
}

CanVehicleHardware::~CanVehicleHardware() {
    mBridge->stop();  // no more onCanUpdates() after this
    mRunning = false;
    mRefreshCv.notify_all();
    if (mRefreshThread.joinable()) mRefreshThread.join();
}

bool CanVehicleHardware::isCanOwned(int32_t propId, int32_t areaId) const {
    return std::any_of(mBindings.begin(), mBindings.end(), [&](const SignalBinding& b) {
        return b.propId == propId && b.areaId == areaId;
    });
}

bool CanVehicleHardware::isCanOwnedProp(int32_t propId) const {
    return std::any_of(mBindings.begin(), mBindings.end(),
                       [&](const SignalBinding& b) { return b.propId == propId; });
}

VehiclePropValue CanVehicleHardware::toPropValue(const PropertyUpdate& update) {
    VehiclePropValue value;
    value.timestamp = update.timestampNs;
    value.prop = update.propId;
    value.areaId = update.areaId;
    value.status = update.available ? VehiclePropertyStatus::AVAILABLE
                                    : VehiclePropertyStatus::UNAVAILABLE;
    switch (getPropType(update.propId)) {
        case VehiclePropertyType::FLOAT:
            value.value.floatValues = {static_cast<float>(update.value)};
            break;
        case VehiclePropertyType::INT32:
            value.value.int32Values = {static_cast<int32_t>(std::llround(update.value))};
            break;
        case VehiclePropertyType::BOOLEAN:
            value.value.int32Values = {update.value != 0.0 ? 1 : 0};
            break;
        default:
            // codegen only emits the three types above.
            ALOGE("unsupported property type for %s", update.propName);
            break;
    }
    return value;
}

void CanVehicleHardware::onCanUpdates(const std::vector<PropertyUpdate>& updates) {
    std::vector<VehiclePropValue> values;
    values.reserve(updates.size());
    {
        std::lock_guard<std::mutex> lock(mLock);
        for (const auto& u : updates) {
            VehiclePropValue value = toPropValue(u);
            mCache[{u.propId, u.areaId}] = value;
            values.push_back(std::move(value));
        }
    }
    emit(std::move(values));
}

void CanVehicleHardware::emit(std::vector<VehiclePropValue> values) {
    if (values.empty()) return;
    std::shared_ptr<const PropertyChangeCallback> callback;
    {
        std::lock_guard<std::mutex> lock(mLock);
        callback = mOnChange;
    }
    if (callback) (*callback)(std::move(values));
}

void CanVehicleHardware::refreshLoop() {
    // CONTINUOUS properties must be reported at the subscribed sample rate even
    // if the value does not change (unless the client enabled variable update rate).
    std::unique_lock<std::mutex> lock(mLock);
    while (mRunning) {
        auto now = std::chrono::steady_clock::now();
        auto wake = now + std::chrono::milliseconds(100);
        std::vector<VehiclePropValue> due;
        for (auto& [key, sub] : mContinuousSubs) {
            if (sub.interval.count() == 0) continue;
            if (sub.next <= now) {
                auto it = mCache.find(key);
                if (it != mCache.end() && it->second.status == VehiclePropertyStatus::AVAILABLE) {
                    VehiclePropValue value = it->second;
                    value.timestamp = elapsedRealtimeNs();
                    due.push_back(std::move(value));
                }
                sub.next = now + sub.interval;
            }
            wake = std::min(wake, sub.next);
        }
        if (!due.empty()) {
            lock.unlock();
            emit(std::move(due));
            lock.lock();
            continue;
        }
        mRefreshCv.wait_until(lock, wake);
    }
}

std::vector<VehiclePropConfig> CanVehicleHardware::getAllPropertyConfigs() const {
    return mInner->getAllPropertyConfigs();
}

StatusCode CanVehicleHardware::setValues(std::shared_ptr<const SetValuesCallback> callback,
                                         const std::vector<SetValueRequest>& requests) {
    // Bound properties are sensors (READ access), so DefaultVehicleHal rejects
    // writes before they get here. Writable CAN properties (TX path) would be
    // encoded and sent through the transport at this point.
    return mInner->setValues(std::move(callback), requests);
}

StatusCode CanVehicleHardware::getValues(std::shared_ptr<const GetValuesCallback> callback,
                                         const std::vector<GetValueRequest>& requests) const {
    std::vector<GetValueResult> canResults;
    std::vector<GetValueRequest> innerRequests;
    {
        std::lock_guard<std::mutex> lock(mLock);
        for (const auto& request : requests) {
            if (!isCanOwned(request.prop.prop, request.prop.areaId)) {
                innerRequests.push_back(request);
                continue;
            }
            GetValueResult result;
            result.requestId = request.requestId;
            auto it = mCache.find({request.prop.prop, request.prop.areaId});
            if (it == mCache.end() || it->second.status != VehiclePropertyStatus::AVAILABLE) {
                result.status = StatusCode::NOT_AVAILABLE;  // no frame yet, or ECU silent
            } else {
                result.status = StatusCode::OK;
                result.prop = it->second;
            }
            canResults.push_back(std::move(result));
        }
    }
    if (!canResults.empty()) (*callback)(std::move(canResults));
    if (innerRequests.empty()) return StatusCode::OK;
    return mInner->getValues(std::move(callback), innerRequests);
}

DumpResult CanVehicleHardware::dump(const std::vector<std::string>& options) {
    if (!options.empty() && options[0] == "--can-inject") {
        DumpResult result;
        result.callerShouldDumpState = false;
        if (options.size() != 2) {
            result.buffer = "usage: --can-inject 3E8#7902000000000000\n";
            return result;
        }
        auto frame = parseFrame(options[1]);
        if (!frame) {
            result.buffer = "invalid frame: " + options[1] + "\n";
            return result;
        }
        mBridge->injectFrame(*frame);
        result.buffer = "injected " + options[1] + "\n";
        return result;
    }

    bool canOnly = !options.empty() && options[0] == "--can-status";
    DumpResult result;
    if (canOnly) {
        result.callerShouldDumpState = false;
    } else {
        result = mInner->dump(options);
        if (!options.empty()) return result;  // inner handled a specific command
    }

    auto stats = mBridge->stats();
    std::string& out = result.buffer;
    StringAppendF(&out, "\n--- Blueprint CAN bridge ---\n");
    StringAppendF(&out, "connected: %s\n", mBridge->connected() ? "yes" : "no");
    StringAppendF(&out, "frames routed=%llu unknown=%llu dlcErrors=%llu\n",
                  static_cast<unsigned long long>(stats.framesRouted),
                  static_cast<unsigned long long>(stats.framesUnknown),
                  static_cast<unsigned long long>(stats.dlcErrors));
    std::lock_guard<std::mutex> lock(mLock);
    for (const auto& b : mBindings) {
        StringAppendF(&out, "  0x%03X %-20s -> %s (0x%08x) area %d: ", b.canId, b.signal->name,
                      b.propName, b.propId, b.areaId);
        auto it = mCache.find({b.propId, b.areaId});
        if (it == mCache.end()) {
            StringAppendF(&out, "no data\n");
        } else {
            StringAppendF(&out, "%s\n", it->second.toString().c_str());
        }
    }
    if (canOnly) {
        StringAppendF(&out, "inject: dumpsys <vhal> --can-inject 3E8#7902000000000000\n");
    }
    return result;
}

StatusCode CanVehicleHardware::checkHealth() {
    return mInner->checkHealth();
}

void CanVehicleHardware::registerOnPropertyChangeEvent(
        std::unique_ptr<const PropertyChangeCallback> callback) {
    {
        std::lock_guard<std::mutex> lock(mLock);
        mOnChange = std::move(callback);
    }
    // Forward the inner hardware's events, minus the ones CAN owns: the fake
    // hardware still has its own stale value for those and must not leak it.
    mInner->registerOnPropertyChangeEvent(std::make_unique<const PropertyChangeCallback>(
            [this](std::vector<VehiclePropValue> values) {
                values.erase(std::remove_if(values.begin(), values.end(),
                                            [this](const VehiclePropValue& v) {
                                                return isCanOwned(v.prop, v.areaId);
                                            }),
                             values.end());
                emit(std::move(values));
            }));
}

void CanVehicleHardware::registerOnPropertySetErrorEvent(
        std::unique_ptr<const PropertySetErrorCallback> callback) {
    mInner->registerOnPropertySetErrorEvent(std::move(callback));
}

std::chrono::nanoseconds CanVehicleHardware::getPropertyOnChangeEventBatchingWindow() {
    return mInner->getPropertyOnChangeEventBatchingWindow();
}

StatusCode CanVehicleHardware::subscribe(SubscribeOptions options) {
    if (!isCanOwnedProp(options.propId)) return mInner->subscribe(std::move(options));

    std::vector<int32_t> areas = options.areaIds;
    if (areas.empty()) {
        for (const auto& b : mBindings) {
            if (b.propId == options.propId) areas.push_back(b.areaId);
        }
    }
    std::vector<int32_t> innerAreas;
    {
        std::lock_guard<std::mutex> lock(mLock);
        for (int32_t area : areas) {
            if (!isCanOwned(options.propId, area)) {
                innerAreas.push_back(area);
                continue;
            }
            Subscription sub{std::chrono::nanoseconds(0), std::chrono::steady_clock::now()};
            if (options.sampleRate > 0.0f && !options.enableVariableUpdateRate) {
                sub.interval = std::chrono::nanoseconds(
                        static_cast<int64_t>(1e9 / options.sampleRate));
            }
            mContinuousSubs[{options.propId, area}] = sub;
        }
    }
    mRefreshCv.notify_all();
    if (!innerAreas.empty()) {
        options.areaIds = innerAreas;
        return mInner->subscribe(std::move(options));
    }
    return StatusCode::OK;
}

StatusCode CanVehicleHardware::unsubscribe(int32_t propId, int32_t areaId) {
    if (!isCanOwned(propId, areaId)) return mInner->unsubscribe(propId, areaId);
    std::lock_guard<std::mutex> lock(mLock);
    mContinuousSubs.erase({propId, areaId});
    return StatusCode::OK;
}

StatusCode CanVehicleHardware::updateSampleRate(int32_t propId, int32_t areaId,
                                                float sampleRate) {
    if (!isCanOwned(propId, areaId)) return mInner->updateSampleRate(propId, areaId, sampleRate);
    {
        std::lock_guard<std::mutex> lock(mLock);
        auto& sub = mContinuousSubs[{propId, areaId}];
        sub.interval = sampleRate > 0.0f
                               ? std::chrono::nanoseconds(static_cast<int64_t>(1e9 / sampleRate))
                               : std::chrono::nanoseconds(0);
        sub.next = std::chrono::steady_clock::now();
    }
    mRefreshCv.notify_all();
    return StatusCode::OK;
}

}  // namespace android::hardware::automotive::vehicle::canhw
