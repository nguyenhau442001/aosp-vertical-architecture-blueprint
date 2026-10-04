# Roadmap: From CAN Wire to VHAL

This repo grows one layer at a time. Each commit adds **one** unit that can be
built, run and tested on its own, starting from the electrical signal on the
wire and ending at a `VehiclePropValue` that `CarPropertyManager` delivers to
an app.

```
 Layer                          Where in repo                      Commit
 ─────────────────────────────  ─────────────────────────────────  ──────
 7  App / CarPropertyManager    (stock AAOS)                         -
 6  VHAL (IVehicleHardware)     vhal/aosp/                           C8
 5  Signal -> VehicleProperty   native/canbridge/                    C7
 4  CAN transport (SocketCAN)   native/cantransport/                 C6
 3  Signal codec (bits -> phys) native/cansignal/                    C5
 2  Signal database (DBC)       vehicle/dbc/ + tools/cansim/dbc.py   C4
 1  Frame + CAN_H / CAN_L       tools/cansim/                        C2-C3
 0  Concepts                    docs/can/                            C1
```

| Commit | Unit | You learn |
|--------|------|-----------|
| C1 | CAN primer docs | What CAN_H / CAN_L are, frame layout, where Android sits |
| C2 | `cansim` frame encoder | SOF/ID/DLC/CRC-15/bit stuffing, bit by bit |
| C3 | `cansim` physical layer | Differential voltages, recessive vs dominant, decoding a waveform |
| C4 | DBC + Python codec | How a raw byte becomes `23.5 °C` (factor, offset, endianness) |
| C5 | C++ `libcansignal` | Same codec in C++, unit-tested, reusable on device |
| C6 | C++ transport | Reading `struct can_frame` from SocketCAN, or TCP from the host simulator |
| C7 | `canbridge` daemon | Mapping a decoded signal to a VHAL property id + area id |
| C8 | AOSP `CanVehicleHardware` | Plugging the bridge into the real VHAL AIDL service |
| C9 | Guide: add a new signal | The checklist you repeat for every new vehicle signal |

Docs to read in order:

1. [docs/can/01-can-basics.md](can/01-can-basics.md)
2. [docs/vhal/adding-a-new-signal.md](vhal/adding-a-new-signal.md) (from C9)
