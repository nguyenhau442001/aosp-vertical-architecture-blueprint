# cansim

Bit-level CAN 2.0A simulator. Pure Python 3, no dependencies (pytest for tests).

```bash
cd tools/cansim
python3 -m pytest -q
```

## `wave`: one frame as CAN_H / CAN_L

```bash
python3 -m cansim wave 3E8#7902
```

```
frame 3E8#7902  dlc=2  wire bits=63 (stuff bits=3)  bit time=2 us
3.5V CAN_H |████          ████  ██████████  ██████ ...
2.5V both  |    ──────────    ──          ──      ──...
1.5V CAN_L |████          ████  ██████████  ██████ ...
bit        |0 0 1 1 1 1 1 0 0 1 0 0 0 0 0 1 0 0 0 1 ...
field      |S I I I I I I I I I I I I R E E 0 L L L ...
stuff      |              ^               ^        ...
receiver decoded: 3E8#7902
```

- Top row: CAN_H at 3.5 V (dominant). Bottom row: CAN_L at 1.5 V (dominant).
  Middle row: both wires at 2.5 V (recessive).
- `^` marks bits inserted by bit stuffing.
- The last line is the simulated receiver: differential comparator, hard sync
  on SOF, sample at 75 % of each bit, destuff, check CRC.

Experiments:

```bash
# EMI hitting both wires equally: still decodes (differential rejection)
python3 -m cansim wave 3E8#7902 --cm-noise 1.5 --seed 3

# Noise on each wire separately: receiver sees garbage
python3 -m cansim wave 3E8#7902 --diff-noise 2 --seed 3

# Dump to files for GTKWave / PulseView / a spreadsheet
python3 -m cansim wave 3E8#7902 --vcd /tmp/frame.vcd --csv /tmp/frame.csv
```

## `encode` / `decode`: physical values via the DBC

Default database: [vehicle/dbc/blueprint.dbc](../../vehicle/dbc/blueprint.dbc).

```bash
python3 -m cansim encode THERMAL_STATUS OutsideTemp=23.3 EvaporatorTemp=-3.25
# 3E8#79020000FEBB0000  (prints the frame; add --wave to see CAN_H/CAN_L)

python3 -m cansim decode 3E8#7902000000000000
# THERMAL_STATUS (3E8#7902000000000000)
#   OutsideTemp          raw=633      value=23.3 degC
#   ...
```

`physical = raw * factor + offset`. Intel (`@1`) signals count bits from the
LSB of byte 0 upward; Motorola (`@0`) signals give the MSB position and walk
down within a byte, then jump to bit 7 of the next byte.

## `serve`: feed frames to the device

Sends one DBC message periodically. Default: TCP server on `127.0.0.1:29536`
streaming 16-byte Linux `struct can_frame` records, which
`native/cantransport` (`tcp:` transport) reads.

```bash
# Outside temperature sweeps -10..40 °C every 20 s, counter increments, 10 Hz
python3 -m cansim serve THERMAL_STATUS --ramp OutsideTemp:-10:40:20 \
    --counter ThermalCounter --period 0.1

# While it runs, type a value and press Enter to pin it:
OutsideTemp=30

# Several messages at once (comma separated). Qualify a signal name with
# MESSAGE. when two messages share it.
python3 -m cansim serve THERMAL_STATUS,BATTERY_THERMAL OutsideTemp=20 CellTempMax=35
```

Check the stream with the native dumper (built by `make test-native`):

```bash
native/build/bp-candump tcp:127.0.0.1:29536
# 3E8#2002000000000002
# 3E8#2D02000000000003
```

On the Android emulator the device connects to its own localhost, so forward
the port back to the host first:

```bash
adb reverse tcp:29536 tcp:29536
```

On Linux (UTM VM) with a virtual CAN interface, write to SocketCAN instead:

```bash
./scripts/setup_vcan.sh            # creates vcan0
python3 -m cansim serve THERMAL_STATUS OutsideTemp=23.3 --socketcan vcan0
candump vcan0                      # can-utils, or native/build/bp-candump socketcan:vcan0
```

## Modules

| Module | Role |
|--------|------|
| `cansim/frame.py` | `CanFrame`, CRC-15, bit stuffing, wire encode/decode |
| `cansim/physical.py` | Voltages, noise, differential receiver, ASCII/CSV/VCD output |
| `cansim/dbc.py` | DBC parser, Intel/Motorola bit extraction, physical <-> raw |
| `cansim/stream.py` | Periodic frames, ramps, counters, TCP server, SocketCAN sender |
