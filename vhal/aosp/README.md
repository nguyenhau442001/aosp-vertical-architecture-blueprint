# vhal/aosp: CAN-backed VHAL service

`CanVehicleHardware` implements `IVehicleHardware` (VHAL AIDL V3, Android 15)
as a decorator:

- Properties bound to CAN signals in the DBC (`VhalProperty` attribute) are
  served from `libblueprint_canbridge`: `getValues` reads the last decoded
  value, updates are pushed through the property-change callback, and an ECU
  that stops sending makes the property `UNAVAILABLE` / `NOT_AVAILABLE`.
- Every other property goes to the wrapped `FakeVehicleHardware`, so the
  emulator keeps working as before.

```
CarPropertyManager ── CarService ── Binder ── DefaultVehicleHal
                                                    │ IVehicleHardware
                                          CanVehicleHardware
                                           │                │
                                CAN-owned props        all other props
                                           │                │
                                  CanBridge (native/)   FakeVehicleHardware
                                           │
                       ICanTransport: tcp (cansim) | socketcan (can0, vcan0)
```

| File | Role |
|------|------|
| `include/CanVehicleHardware.h`, `src/CanVehicleHardware.cpp` | The decorator |
| `src/VehicleService.cpp` | `main()`: builds the stack, registers `IVehicle/default` |
| `vhal-blueprint-can-service.rc` | init service (`user vehicle_network`, `group inet` for TCP) |
| `vhal-blueprint-can-service.xml` | VINTF fragment (only for a full image build) |

## Build (Linux / UTM VM, AOSP android15 tree)

```bash
# once: make the repo visible to Soong
ln -s ~/aosp-vertical-architecture-blueprint $AOSP/vendor/blueprint

cd $AOSP && source build/envsetup.sh && lunch sdk_car_arm64-trunk_staging-userdebug
m android.hardware.automotive.vehicle@V3-blueprint-can-service
# out/target/product/emulator_car64_arm64/vendor/bin/hw/android.hardware.automotive.vehicle@V3-blueprint-can-service
```

Host-side unit tests do not need AOSP: `make test` at the repo root.

## Run on the emulator (macOS host)

```bash
make run                                                      # root + writable partitions
./scripts/deploy_vhal.sh path/to/android.hardware.automotive.vehicle@V3-blueprint-can-service
cd tools/cansim && python3 -m cansim serve THERMAL_STATUS \
    CabinTempRow1Left=21.5 CabinTempRow1Right=24 --ramp OutsideTemp:-10:40:20
```

`deploy_vhal.sh` stops the emulator's own VHAL and starts this one by hand.
That is enough for development. For a real image, add the module to
`PRODUCT_PACKAGES`, drop the stock VHAL, and write sepolicy for the service
(`hal_vehicle_default` domain, plus `can_socket` or `tcp_socket` rules).

## Verify, layer by layer

```bash
VHAL=android.hardware.automotive.vehicle.IVehicle/default

# 1. Bridge status: connected? frames routed? last value per binding
adb shell dumpsys $VHAL --can-status

# 2. Inject a frame without any simulator (OutsideTemp = 23.3 °C)
adb shell dumpsys $VHAL --can-inject 3E8#7902000000000000

# 3. Through CarService, the path an app uses
adb shell cmd car_service get-property-value 0x11600703      # ENV_OUTSIDE_TEMPERATURE
adb shell cmd car_service get-property-value 0x15600502 49   # HVAC_TEMPERATURE_CURRENT, left

# 4. Logs
adb logcat -s BlueprintCanVhal
```

Note: `dumpsys $VHAL --get ...` is answered by `FakeVehicleHardware` and shows
its own stored value, not the CAN value. Use step 1 or 3 instead.

## Transport

Default `tcp:127.0.0.1:29536` (cansim through `adb reverse`). On a board with
a CAN controller:

```bash
adb shell setprop persist.vendor.blueprint.can.transport socketcan:can0
adb shell stop vendor.vehicle-hal-blueprint && adb shell start vendor.vehicle-hal-blueprint
```
