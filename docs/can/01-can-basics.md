# CAN Basics for Android Automotive Developers

Goal of this page: enough CAN knowledge to follow one temperature reading from
a thermistor in the dashboard all the way to `CarPropertyManager`.

## 1. The full path of one signal

```
 NTC thermistor ──► ADC in climate ECU (MCU)
                         │  raw = 0x01F7
                         ▼
               ECU firmware packs raw value into a CAN message
               (layout defined by the DBC file)
                         │
                         ▼
               CAN transceiver (e.g. TJA1042) drives CAN_H / CAN_L
 ════════════════════ twisted pair, 120 Ω at each end ════════════════════
                         │
                         ▼
               CAN controller in the head unit
               (SoC built-in, MCP2515 over SPI, or a vehicle MCU)
                         │
                         ▼
               Linux kernel driver  ->  network interface "can0" (SocketCAN)
                         │  struct can_frame { id, dlc, data[8] }
                         ▼
               Userspace daemon: decode with DBC rules -> 23.5 °C
                         │
                         ▼
               VHAL (IVehicleHardware) -> VehiclePropValue
                         │  ENV_OUTSIDE_TEMPERATURE = 23.5
                         ▼
               CarService -> CarPropertyManager -> App
```

Many production head units never touch CAN directly: a separate vehicle MCU
owns the CAN bus and talks to the Android SoC over SPI/UART/Ethernet. The
layers above "Userspace daemon" stay the same; only the transport changes.
That is why this repo keeps transport (C6) separate from decoding (C5).

## 2. Physical layer: CAN_H and CAN_L

CAN uses two wires and reads the **difference** between them, so noise that
hits both wires equally cancels out.

| Bus state | Logic bit | CAN_H | CAN_L | V_diff = H - L |
|-----------|-----------|-------|-------|----------------|
| Recessive | 1 | ~2.5 V | ~2.5 V | ~0 V |
| Dominant  | 0 | ~3.5 V | ~1.5 V | ~2 V |

Receiver thresholds (ISO 11898-2):

- `V_diff > 0.9 V` -> dominant (0)
- `V_diff < 0.5 V` -> recessive (1)
- In between: undefined

Key consequence: **dominant wins**. If one node sends 0 and another sends 1 at
the same time, the bus shows 0. This is "wired-AND" and it is how arbitration
works: while sending the ID, a node that sends 1 but reads back 0 knows it
lost and stops. Lower ID = higher priority.

Timing: at 500 kbit/s (common for powertrain/body), one bit = 2 µs.

## 3. Classic CAN frame (CAN 2.0A, 11-bit ID)

```
 SOF | ID[11] | RTR | IDE | r0 | DLC[4] | DATA[0..64] | CRC[15] | CRC_DEL | ACK | ACK_DEL | EOF[7] | IFS[3]
  0     ...      0     0    0    0..8      bytes         ...        1        0*     1        1111111  111
```

| Field | Bits | Meaning |
|-------|------|---------|
| SOF | 1 | Start of frame, always dominant |
| ID | 11 | Message identifier, also the priority |
| RTR | 1 | 0 = data frame, 1 = remote request |
| IDE | 1 | 0 = standard 11-bit ID |
| r0 | 1 | Reserved, dominant |
| DLC | 4 | Data length in bytes (0..8) |
| DATA | 8 x DLC | Payload |
| CRC | 15 | CRC-15, polynomial `0x4599`, over SOF..DATA |
| CRC delimiter | 1 | Recessive |
| ACK slot | 1 | Sender sends recessive; any receiver that got the frame right pulls it dominant |
| ACK delimiter | 1 | Recessive |
| EOF | 7 | Recessive |

### Bit stuffing

From SOF through the CRC sequence, after **5 identical bits in a row** the
transmitter inserts one bit of the opposite value. Receivers remove it. This
guarantees edges so every node can keep its clock in sync. Six identical bits
inside that region is a stuff error.

You will see stuffing happen bit by bit in `tools/cansim` (C2).

## 4. From bytes to a physical value (DBC)

The frame only carries bytes. A **DBC file** says what they mean:

```
BO_ 1000 THERMAL_STATUS: 8 ClimateECU
 SG_ OutsideTemp : 0|16@1+ (0.1,-40) [-40|125] "degC" HeadUnit
```

Read it as: message id 1000 (0x3E8), 8 bytes. Signal `OutsideTemp` starts at
bit 0, is 16 bits long, little-endian (`@1`), unsigned (`+`).
`physical = raw * 0.1 + (-40)`.

So raw `0x0279` (633) -> `633 * 0.1 - 40 = 23.3 °C`.

## 5. Where Linux and Android see it

```bash
# On a Linux machine (or the UTM VM) with no real hardware:
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

candump vcan0                       # watch frames
cansend vcan0 3E8#7902000000000000  # OutsideTemp = 23.3 °C
```

Userspace reads `struct can_frame` from a `socket(PF_CAN, SOCK_RAW, CAN_RAW)`.
The Android emulator has no CAN interface, so this repo also offers a TCP
transport: the host simulator streams the same 16-byte `can_frame` structs and
the device connects through `adb reverse` (C6).

## 6. Glossary

| Term | Meaning |
|------|---------|
| ECU | Electronic Control Unit, an MCU on the bus |
| DBC | Text database of messages and signals (Vector format) |
| Signal | A bit field inside a message with scale/offset/unit |
| SocketCAN | Linux CAN networking stack (`can0`, `vcan0`) |
| VHAL | Vehicle HAL, AIDL service `android.hardware.automotive.vehicle.IVehicle` |
| Property | A VHAL value identified by `propId` + `areaId` |
