# native/

C++ layers between the CAN driver and VHAL. Each directory builds on host
(CMake, for fast tests) and on device (Android.bp, same sources).

| Directory | Library | Role |
|-----------|---------|------|
| `cansignal/` | `libblueprint_cansignal` | Bit field -> physical value (DBC rules) |
| `cantransport/` | `libblueprint_cantransport` | `struct can_frame` from SocketCAN or TCP |
| `canbridge/` | `libblueprint_canbridge` | Frame -> `PropertyUpdate` (propId, areaId, value), change detection, signal timeout, reconnect |

`canbridge/generated/BlueprintSignals.h` is generated from
[vehicle/dbc/blueprint.dbc](../vehicle/dbc/blueprint.dbc) by `make codegen`.
Never edit it by hand; a Python test fails if it is out of date.

## Host workflow

```bash
make test-native                    # build + gtest

# terminal 1: fake ECU
cd tools/cansim && python3 -m cansim serve THERMAL_STATUS \
    CabinTempRow1Left=21.5 CabinTempRow1Right=24 --ramp OutsideTemp:-10:40:20

# terminal 2: what VHAL would receive
native/build/bp-canbridge tcp:127.0.0.1:29536
# ENV_OUTSIDE_TEMPERATURE    area=0   value=26.4
# HVAC_TEMPERATURE_CURRENT   area=49  value=21.5
# ...stop terminal 1 and after 1 s:
# ENV_OUTSIDE_TEMPERATURE    area=0   UNAVAILABLE (signal timeout)
```

Debug a layer at a time: if `bp-canbridge` prints nothing, run
`bp-candump` on the same transport. Frames there but no properties means the
DBC binding (id, DLC, `VhalProperty` attribute) is wrong.

## Run on the emulator without an AOSP tree (NDK)

Soong cannot run on macOS, but these libraries have no AOSP dependency, so the
Android NDK cross-compiles them directly. Binaries go to `/data/local/tmp`,
which needs no root and no remount.

```bash
make android-build      # NDK clang -> native/build-android/ (arm64-v8a, API 35, static libc++)
make android-test       # push + run all gtests on the emulator

# terminal 1 (Mac): fake ECUs
cd tools/cansim && python3 -m cansim serve THERMAL_STATUS,BATTERY_THERMAL \
    CellTempMax=47 --ramp OutsideTemp:-10:40:20

# terminal 2: adb reverse + bp-canbridge running inside the emulator
make android-bridge
```

`x86_64` emulator image: `./scripts/build_android.sh x86_64`.

With `adb reverse`, the device-side connect succeeds even while nothing
listens on the Mac, then drops at once. Seeing `connected` / `lost` flip
while the simulator is stopped is expected.

What this does not cover: the VHAL service in `vhal/aosp/` links
`DefaultVehicleHal`, `FakeVehicleHardware`, `libbase`, `libutils`, which are
not in the NDK. That module still needs an AOSP build on a Linux x86_64 host.
