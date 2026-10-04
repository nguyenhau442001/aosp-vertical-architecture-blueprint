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
