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

/**
 * Cabin zones representing seat and climate control areas inside the vehicle.
 */
@VintfStability
@Backing(type="int")
enum CabinZone {
    ROW_1_LEFT = 0,     // Driver seat
    ROW_1_RIGHT = 1,    // Front passenger seat
    ROW_2_LEFT = 2,     // Rear left passenger seat
    ROW_2_RIGHT = 3,    // Rear right passenger seat
    ROW_2_CENTER = 4,   // Rear center seat
    ALL = 100,          // Cabin-wide control
}
