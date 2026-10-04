"""CAN_H / CAN_L physical layer (ISO 11898-2 high-speed CAN, idealised).

Transmit: wire bits -> sampled voltages on both wires.
Receive:  sampled voltages -> differential comparator -> bits -> CanFrame.
"""

import random
from dataclasses import dataclass
from typing import List, Optional, Sequence, TextIO

from .frame import CanFrame, WireBit, decode_wire

RECESSIVE = 2.5
DOMINANT_H = 3.5
DOMINANT_L = 1.5

# Receiver thresholds on V_diff = CAN_H - CAN_L
DOMINANT_THRESHOLD = 0.9
RECESSIVE_THRESHOLD = 0.5

SAMPLE_POINT = 0.75  # fraction of the bit time where the receiver samples


@dataclass(frozen=True)
class Sample:
    t: float
    can_h: float
    can_l: float
    bit: int

    @property
    def v_diff(self) -> float:
        return self.can_h - self.can_l


def to_waveform(
    wire: Sequence[WireBit],
    bitrate: int = 500_000,
    samples_per_bit: int = 10,
    idle_bits: int = 3,
    common_mode_noise_v: float = 0.0,
    diff_noise_v: float = 0.0,
    seed: Optional[int] = None,
) -> List[Sample]:
    """Render wire bits to voltages, with idle recessive bits on both sides.

    common_mode_noise_v: same random offset added to both wires (EMI pickup).
    diff_noise_v: independent random offset per wire (breaks the frame if big).
    """
    rng = random.Random(seed)
    dt = 1.0 / bitrate / samples_per_bit
    bits = [1] * idle_bits + [b.value for b in wire] + [1] * idle_bits
    samples: List[Sample] = []
    for i, bit in enumerate(bits):
        h, l = (RECESSIVE, RECESSIVE) if bit else (DOMINANT_H, DOMINANT_L)
        for k in range(samples_per_bit):
            cm = rng.uniform(-common_mode_noise_v, common_mode_noise_v)
            nh = rng.uniform(-diff_noise_v, diff_noise_v)
            nl = rng.uniform(-diff_noise_v, diff_noise_v)
            t = (i * samples_per_bit + k) * dt
            samples.append(Sample(t, h + cm + nh, l + cm + nl, bit))
    return samples


def _comparator(samples: Sequence[Sample]) -> List[int]:
    """Differential receiver with hysteresis between the two thresholds."""
    state = 1
    out = []
    for s in samples:
        if s.v_diff > DOMINANT_THRESHOLD:
            state = 0
        elif s.v_diff < RECESSIVE_THRESHOLD:
            state = 1
        out.append(state)
    return out


def receive(samples: Sequence[Sample], samples_per_bit: int = 10) -> CanFrame:
    """Hard-sync on the SOF edge, sample each bit at SAMPLE_POINT, decode."""
    levels = _comparator(samples)
    try:
        sof = levels.index(0)
    except ValueError:
        raise ValueError("bus stayed recessive: no SOF found") from None
    offset = int(SAMPLE_POINT * samples_per_bit)
    bits = [levels[i] for i in range(sof + offset, len(levels), samples_per_bit)]
    return decode_wire(bits)


_FIELD_TAGS = {
    "SOF": "S", "ID": "I", "RTR": "R", "IDE": "E", "r0": "0", "DLC": "L",
    "DATA": "D", "CRC": "C", "CRC_DEL": "c", "ACK": "A", "ACK_DEL": "a", "EOF": "F",
}


def ascii_plot(wire: Sequence[WireBit], width: int = 2) -> str:
    """Terminal view: CAN_H on top row when dominant, CAN_L on bottom row."""
    top, mid, bot, bitrow, field, stuffrow = [], [], [], [], [], []
    for b in wire:
        dominant = b.value == 0
        top.append(("█" if dominant else " ") * width)
        mid.append((" " if dominant else "─") * width)
        bot.append(("█" if dominant else " ") * width)
        bitrow.append(str(b.value).ljust(width))
        field.append(_FIELD_TAGS.get(b.field, "?").ljust(width))
        stuffrow.append(("^" if b.stuffed else " ").ljust(width))
    return "\n".join(
        [
            "3.5V CAN_H |" + "".join(top),
            "2.5V both  |" + "".join(mid),
            "1.5V CAN_L |" + "".join(bot),
            "bit        |" + "".join(bitrow),
            "field      |" + "".join(field),
            "stuff      |" + "".join(stuffrow),
            "legend: S=SOF I=ID R=RTR E=IDE 0=r0 L=DLC D=DATA C=CRC "
            "c=CRC_DEL A=ACK a=ACK_DEL F=EOF ^=stuff bit",
        ]
    )


def write_csv(samples: Sequence[Sample], out: TextIO) -> None:
    out.write("t_s,can_h_v,can_l_v,v_diff_v,bit\n")
    for s in samples:
        out.write(f"{s.t:.9f},{s.can_h:.4f},{s.can_l:.4f},{s.v_diff:.4f},{s.bit}\n")


def write_vcd(samples: Sequence[Sample], out: TextIO) -> None:
    """Value Change Dump, viewable in GTKWave or PulseView."""
    out.write("$timescale 1ns $end\n$scope module can $end\n")
    out.write("$var real 64 h CAN_H $end\n")
    out.write("$var real 64 l CAN_L $end\n")
    out.write("$var wire 1 b BIT $end\n")
    out.write("$upscope $end\n$enddefinitions $end\n")
    prev = None
    for s in samples:
        cur = (round(s.can_h, 4), round(s.can_l, 4), s.bit)
        if cur == prev:
            continue
        out.write(f"#{round(s.t * 1e9)}\n")
        if prev is None or cur[0] != prev[0]:
            out.write(f"r{cur[0]:g} h\n")
        if prev is None or cur[1] != prev[1]:
            out.write(f"r{cur[1]:g} l\n")
        if prev is None or cur[2] != prev[2]:
            out.write(f"{cur[2]}b\n")
        prev = cur
