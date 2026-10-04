# Adding a New Vehicle Signal (CAN -> VHAL -> App)

This is the checklist to repeat for every signal. The worked example is the
hottest battery cell temperature from the BMS, added in the commit
`feat(dbc): add BATTERY_THERMAL as worked example` (`git log --grep BATTERY_THERMAL`).
Read that diff next to this page: it touches exactly the files listed below.

```
 Step  Layer                     File(s) you touch                             Check with
 ────  ────────────────────────  ────────────────────────────────────────────  ─────────────────────────
  1    Signal spec (DBC)         vehicle/dbc/blueprint.dbc                     cansim encode/decode/wave
  2    Pick a VHAL property      tools/cansim/cansim/vhal.py (vendor only)     cansim explain
  3    Bind signal -> property   vehicle/dbc/blueprint.dbc (BA_ lines)         -
  4    Generate C++ table        native/canbridge/generated/ (make codegen)    make test-py
  5    Tests                     tools/cansim/tests/, native/canbridge/test/   make test
  6    Host end-to-end           -                                             cansim serve + bp-canbridge
  7    Device                    VHAL config JSON (SYSTEM props only)          dumpsys --can-status, cmd car_service
  8    App                       AndroidManifest permission + CarPropertyManager  app UI / logcat
```

---

## Step 1: Describe the signal in the DBC

In a real project the network team owns the DBC and gives you the message.
You still need to read and check it. Collect:

| Question | Example answer |
|----------|----------------|
| Which ECU sends it? | BMS |
| CAN id, length, period | `0x3E9`, 8 bytes, 100 ms |
| Start bit, length, byte order, signed? | bit 0, 8 bits, Intel, unsigned |
| Scale and offset | `phys = raw * 1 - 40` |
| Range and unit | -40 .. 125 °C |

```
BO_ 1001 BATTERY_THERMAL: 8 BMS
 SG_ CellTempMax : 0|8@1+ (1,-40) [-40|125] "degC" HeadUnit
```

Check it before writing any C++:

```bash
cd tools/cansim
python3 -m cansim encode BATTERY_THERMAL CellTempMax=47     # 3E9#5700000000000000
python3 -m cansim decode 3E9#5700000000000000               # CellTempMax raw=87 value=47 degC
python3 -m cansim encode BATTERY_THERMAL CellTempMax=47 --wave   # see it as CAN_H / CAN_L
```

If you have a log from the real car (`candump`), decode a few real frames and
compare with the instrument cluster. Wrong byte order is the most common bug:
a Motorola (`@0`) signal decoded as Intel gives values that jump wildly.

## Step 2: Pick the VHAL property

```
                    Is there a property in VehicleProperty.aidl with the same meaning?
                         │ yes                                        │ no
                         ▼                                            ▼
            use it (SYSTEM group)                         define a VENDOR property
   e.g. ENV_OUTSIDE_TEMPERATURE 0x11600703       e.g. VENDOR_BATTERY_CELL_TEMP_MAX 0x21600101
   apps and CarService already know it           only your apps know it
```

A property id is four fields OR-ed together:

```bash
python3 -m cansim explain 0x21600101
# 0x21600101 = VENDOR | GLOBAL | FLOAT | 0x0101
python3 -m cansim explain HVAC_TEMPERATURE_CURRENT
# 0x15600502 = SYSTEM | SEAT | FLOAT | 0x0502
```

| Field | Choose |
|-------|--------|
| Group | `SYSTEM` if standard, else `VENDOR` |
| Area | `GLOBAL` (one value for the car), `SEAT`, `WINDOW`, `DOOR`, `WHEEL`, ... |
| Type | `FLOAT` for physical values, `INT32` for enums/counts, `BOOLEAN` for flags |
| Unique id | Standard: fixed by AOSP. Vendor: any unused 16-bit number in your project |

For a non-global property, also pick the **area id**. Seat areas are bit masks
(`ROW_1_LEFT = 0x1`, `ROW_1_RIGHT = 0x4`, ...). The emulator's HVAC zones are
`49` (left = ROW_1_LEFT | ROW_2_LEFT | ROW_2_CENTER) and `68` (right).

Vendor property: give it a name in `VENDOR_PROPERTIES` in
`tools/cansim/cansim/vhal.py`:

```python
VENDOR_PROPERTIES = {
    "VENDOR_BATTERY_CELL_TEMP_MAX": 0x21600101,  # VENDOR | GLOBAL | FLOAT | 0x0101
}
```

**Change mode**: `ON_CHANGE` for values that move in steps (most CAN signals).
`CONTINUOUS` for values a client samples at a rate (speed, outside
temperature in the stock config). For a SYSTEM property, match what the
VHAL config JSON already says for it.

## Step 3: Bind the signal to the property

Attributes at the end of the DBC:

```
BA_ "VhalProperty" SG_ 1001 CellTempMax "VENDOR_BATTERY_CELL_TEMP_MAX";
```

Optional, when needed:

```
BA_ "VhalAreaId" SG_ 1000 CabinTempRow1Left 49;
BA_ "VhalChangeMode" SG_ 1000 OutsideTemp "CONTINUOUS";
BA_ "VhalMaxSampleRate" SG_ 1000 OutsideTemp 2;
```

A signal without `VhalProperty` stays in the DBC but is not routed (see
`EvaporatorTemp`).

## Step 4: Generate the C++ table

```bash
make codegen
```

This rewrites `native/canbridge/generated/BlueprintSignals.h`. Read the diff:
one new `SignalSpec` line per signal and one new `kBindings` row per routed
signal. Commit it; the AOSP build does not run Python. A test fails if you
forget to regenerate.

## Step 5: Tests

Add one decode test and one routing test, with a real-looking frame:

- Python, `tools/cansim/tests/test_dbc.py`: encode physical -> bytes -> decode.
- C++, `native/canbridge/test/CanBridgeTest.cpp`: frame -> `PropertyUpdate`
  with the right `propId`, `areaId`, value.

```bash
make test
```

## Step 6: Host end-to-end (no Android needed)

```bash
# terminal 1: fake ECUs
cd tools/cansim && python3 -m cansim serve THERMAL_STATUS,BATTERY_THERMAL \
    OutsideTemp=20 --ramp CellTempMax:25:60:30 --counter BatteryThermalCounter

# terminal 2: what VHAL will receive
native/build/bp-canbridge tcp:127.0.0.1:29536
# VENDOR_BATTERY_CELL_TEMP_MAX area=0   value=47
```

Stop terminal 1: after 1 s every property prints `UNAVAILABLE`. That is what
apps see when the ECU is asleep or the cable is unplugged.

## Step 7: On the device

1. **Property config.** VHAL only exposes properties that have a
   `VehiclePropConfig`.
   - SYSTEM properties: must exist in the VHAL config JSON
     (`DefaultProperties.json` for the emulator). If missing, the service logs
     `... has no VehiclePropConfig` in `logcat -s BlueprintCanVhal`.
   - VENDOR properties: `CanVehicleHardware` declares them from the DBC
     attributes (READ access, change mode, sample rates). Nothing to edit.
2. **Build and deploy** (see [vhal/aosp/README.md](../../vhal/aosp/README.md)):
   ```bash
   m android.hardware.automotive.vehicle@V3-blueprint-can-service     # Linux VM
   ./scripts/deploy_vhal.sh <binary>                                 # macOS host
   ```
3. **Verify, bottom-up:**
   ```bash
   VHAL=android.hardware.automotive.vehicle.IVehicle/default
   adb shell dumpsys $VHAL --can-status                     # bridge connected? value cached?
   adb shell dumpsys $VHAL --can-inject 3E9#5700000000000000   # no simulator needed
   adb shell cmd car_service get-property-value 0x21600101  # through CarService
   ```

## Step 8: In the app

Permissions come from CarService, per property:

| Property | Read permission |
|----------|-----------------|
| `ENV_OUTSIDE_TEMPERATURE` | `android.car.permission.CAR_EXTERIOR_ENVIRONMENT` |
| `HVAC_TEMPERATURE_CURRENT` | `android.car.permission.CONTROL_CAR_CLIMATE` |
| Any VENDOR property | `android.car.permission.CAR_VENDOR_EXTENSION` (signature/privileged) |

```kotlin
val car = Car.createCar(context)
val cpm = car.getCarManager(Car.PROPERTY_SERVICE) as CarPropertyManager

cpm.registerCallback(object : CarPropertyManager.CarPropertyEventCallback {
    override fun onChangeEvent(value: CarPropertyValue<*>) {
        if (value.status == CarPropertyValue.STATUS_AVAILABLE) {
            Log.i(TAG, "cell max = ${value.value as Float} °C")
        } else {
            Log.w(TAG, "BMS not reporting")      // signal timeout in canbridge
        }
    }
    override fun onErrorEvent(propertyId: Int, areaId: Int) {}
}, 0x21600101, CarPropertyManager.SENSOR_RATE_ONCHANGE)
```

---

## Troubleshooting: which layer is broken?

| Symptom | Layer | Look at |
|---------|-------|---------|
| `cansim wave` says `receiver error` | Physical / frame | Noise settings, frame syntax |
| Value off by a constant | DBC offset | `cansim decode` on a real frame |
| Value wildly wrong or jumps by 256 | DBC byte order / start bit | `@0` vs `@1`, start bit numbering |
| `bp-candump` prints nothing | Transport | Simulator running? `adb reverse`? `ip link` for vcan |
| `bp-candump` prints frames, `bp-canbridge` nothing | Binding | CAN id, DLC, `VhalProperty` attribute, `make codegen` |
| `--can-status` shows value, `get-property-value` fails | VHAL config / CarService | `logcat -s BlueprintCanVhal`, missing config for a SYSTEM prop |
| App gets `SecurityException` | Permission | Table in step 8 |
| App sees `STATUS_UNAVAILABLE` | Source | ECU not sending; `--can-status` shows `connected` and counters |

## When the signal is not on CAN

The same steps apply when the sensor is wired to the SoC instead (for
example a thermistor on an ADC with a kernel IIO/thermal driver exposing
`/sys/class/thermal/thermal_zoneN/temp`). Only step 1 changes: instead of a
DBC message you need a source that turns that sysfs value into a
`PropertyUpdate`. Everything from `CanVehicleHardware` upward stays as is.
