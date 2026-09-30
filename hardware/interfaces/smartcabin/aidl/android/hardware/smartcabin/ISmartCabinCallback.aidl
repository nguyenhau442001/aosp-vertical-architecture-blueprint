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

package android.hardware.smartcabin;

import android.hardware.smartcabin.CabinZone;
import android.hardware.smartcabin.HeatingLevel;

/**
 * Asynchronous callback interface for receiving Smart Cabin events from the HAL service.
 */
@VintfStability
oneway interface ISmartCabinCallback {
    /**
     * Triggered when the cabin temperature is updated by physical sensor or simulation.
     *
     * @param zone The cabin zone where the temperature change was detected.
     * @param temperatureCelsius Current temperature reading in Celsius.
     */
    void onCabinTemperatureChanged(in CabinZone zone, in float temperatureCelsius);

    /**
     * Triggered when seat heating level changes.
     *
     * @param zone The seat zone.
     * @param level Current heating level.
     */
    void onSeatHeatingChanged(in CabinZone zone, in HeatingLevel level);

    /**
     * Triggered when ambient light color or brightness changes.
     *
     * @param rgbColor ARGB / RGB hex color representation (e.g. 0xFF00FF).
     * @param brightness Percentage brightness from 0 to 100.
     */
    void onAmbientLightChanged(in int rgbColor, in int brightness);
}
