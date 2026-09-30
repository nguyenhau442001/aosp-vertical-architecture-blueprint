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
import android.hardware.smartcabin.ISmartCabinCallback;

/**
 * Hardware Abstraction Layer (HAL) interface for vehicle Smart Cabin control.
 *
 * This AIDL HAL exposes control and telemetry for:
 * - Seat heating and comfort zones
 * - Multi-zone cabin temperature
 * - Interior ambient lighting
 * - Event callbacks to framework SystemServer
 */
@VintfStability
interface ISmartCabin {
    /**
     * Set seat heating level for a specific cabin zone.
     *
     * @param zone The target seat zone.
     * @param level Heating level (OFF, LOW, MEDIUM, HIGH).
     */
    void setSeatHeating(in CabinZone zone, in HeatingLevel level);

    /**
     * Get current seat heating level for a specific cabin zone.
     *
     * @param zone The target seat zone.
     * @return Current HeatingLevel.
     */
    HeatingLevel getSeatHeating(in CabinZone zone);

    /**
     * Set target temperature for a specific cabin zone.
     *
     * @param zone The target zone (or CabinZone.ALL for entire vehicle).
     * @param temperatureCelsius Target temperature in degrees Celsius (16.0 - 30.0).
     */
    void setTargetTemperature(in CabinZone zone, in float temperatureCelsius);

    /**
     * Get current temperature reading for a specific cabin zone.
     *
     * @param zone The target zone.
     * @return Current temperature in degrees Celsius.
     */
    float getCabinTemperature(in CabinZone zone);

    /**
     * Set ambient lighting color and brightness.
     *
     * @param rgbColor RGB color format (0x00RRGGBB).
     * @param brightness Brightness percentage from 0 to 100.
     */
    void setAmbientLight(in int rgbColor, in int brightness);

    /**
     * Get current ambient lighting color.
     *
     * @return Current RGB color format (0x00RRGGBB).
     */
    int getAmbientLightColor();

    /**
     * Get current ambient lighting brightness.
     *
     * @return Current brightness level (0 - 100).
     */
    int getAmbientLightBrightness();

    /**
     * Register a callback listener for asynchronous cabin status events.
     *
     * @param callback The callback interface instance.
     */
    void registerCallback(in ISmartCabinCallback callback);

    /**
     * Unregister a previously registered callback listener.
     *
     * @param callback The callback interface instance to remove.
     */
    void unregisterCallback(in ISmartCabinCallback callback);
}
