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

## Modules

| Module | Role |
|--------|------|
| `cansim/frame.py` | `CanFrame`, CRC-15, bit stuffing, wire encode/decode |
| `cansim/physical.py` | Voltages, noise, differential receiver, ASCII/CSV/VCD output |
