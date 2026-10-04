"""CAN 2.0A (11-bit id) data frame <-> bit stream on the wire.

Bit value convention follows the bus: 0 = dominant, 1 = recessive.
"""

from dataclasses import dataclass
from typing import List, Sequence

CRC15_POLY = 0x4599
STUFF_RUN = 5
EOF_BITS = 7

HEADER_BITS = 1 + 11 + 1 + 1 + 1 + 4  # SOF, ID, RTR, IDE, r0, DLC


class StuffError(ValueError):
    """Six identical consecutive bits inside the stuffed region."""


@dataclass(frozen=True)
class CanFrame:
    can_id: int
    data: bytes

    def __post_init__(self):
        if not 0 <= self.can_id <= 0x7FF:
            raise ValueError(f"standard CAN id must be 0..0x7FF, got {self.can_id:#x}")
        if len(self.data) > 8:
            raise ValueError(f"classic CAN carries at most 8 bytes, got {len(self.data)}")
        object.__setattr__(self, "data", bytes(self.data))

    @property
    def dlc(self) -> int:
        return len(self.data)

    def unstuffed_bits(self) -> List[int]:
        """SOF through CRC sequence, before bit stuffing."""
        bits = [0]  # SOF
        bits += _to_bits(self.can_id, 11)
        bits += [0, 0, 0]  # RTR (data frame), IDE (standard), r0
        bits += _to_bits(self.dlc, 4)
        for byte in self.data:
            bits += _to_bits(byte, 8)
        bits += _to_bits(crc15(bits), 15)
        return bits

    def __str__(self) -> str:
        return f"{self.can_id:03X}#{self.data.hex().upper()}"


@dataclass(frozen=True)
class WireBit:
    value: int
    field: str
    stuffed: bool = False


def _to_bits(value: int, width: int) -> List[int]:
    return [(value >> (width - 1 - i)) & 1 for i in range(width)]


def _from_bits(bits: Sequence[int]) -> int:
    value = 0
    for b in bits:
        value = (value << 1) | b
    return value


def crc15(bits: Sequence[int]) -> int:
    """CRC-15/CAN over a bit sequence (MSB first, init 0)."""
    crc = 0
    for bit in bits:
        crc_next = bit ^ ((crc >> 14) & 1)
        crc = (crc << 1) & 0x7FFF
        if crc_next:
            crc ^= CRC15_POLY
    return crc


def stuff(bits: Sequence[int]) -> List[int]:
    out: List[int] = []
    run_value, run_len = None, 0
    for bit in bits:
        out.append(bit)
        run_len = run_len + 1 if bit == run_value else 1
        run_value = bit
        if run_len == STUFF_RUN:
            stuff_bit = 1 - bit
            out.append(stuff_bit)
            run_value, run_len = stuff_bit, 1
    return out


def unstuff(bits: Sequence[int]) -> List[int]:
    out: List[int] = []
    run_value, run_len = None, 0
    skip_next = False
    for bit in bits:
        if skip_next:
            if bit == run_value:
                raise StuffError("six identical bits in stuffed region")
            run_value, run_len = bit, 1
            skip_next = False
            continue
        out.append(bit)
        run_len = run_len + 1 if bit == run_value else 1
        run_value = bit
        if run_len == STUFF_RUN:
            skip_next = True
    return out


def _field_names(dlc: int) -> List[str]:
    return (
        ["SOF"]
        + ["ID"] * 11
        + ["RTR", "IDE", "r0"]
        + ["DLC"] * 4
        + ["DATA"] * (8 * dlc)
        + ["CRC"] * 15
    )


def encode_wire(frame: CanFrame, ack: bool = True) -> List[WireBit]:
    """Full frame as transmitted, with stuff bits marked.

    ack=True simulates at least one receiver pulling the ACK slot dominant.
    """
    raw = frame.unstuffed_bits()
    names = _field_names(frame.dlc)
    wire: List[WireBit] = []
    run_value, run_len = None, 0
    for bit, name in zip(raw, names):
        wire.append(WireBit(bit, name))
        run_len = run_len + 1 if bit == run_value else 1
        run_value = bit
        if run_len == STUFF_RUN:
            stuff_bit = 1 - bit
            wire.append(WireBit(stuff_bit, name, stuffed=True))
            run_value, run_len = stuff_bit, 1
    wire.append(WireBit(1, "CRC_DEL"))
    wire.append(WireBit(0 if ack else 1, "ACK"))
    wire.append(WireBit(1, "ACK_DEL"))
    wire += [WireBit(1, "EOF")] * EOF_BITS
    return wire


def decode_wire(bits: Sequence[int]) -> CanFrame:
    """Parse one frame starting at SOF. Raises ValueError on any violation."""
    if not bits or bits[0] != 0:
        raise ValueError("frame must start with dominant SOF")

    raw: List[int] = []
    run_value, run_len = None, 0
    skip_next = False
    needed = HEADER_BITS
    pos = 0
    while len(raw) < needed:
        if pos >= len(bits):
            raise ValueError("bit stream ended inside stuffed region")
        bit = bits[pos]
        pos += 1
        if skip_next:
            if bit == run_value:
                raise StuffError(f"stuff error at wire bit {pos - 1}")
            run_value, run_len = bit, 1
            skip_next = False
            continue
        raw.append(bit)
        run_len = run_len + 1 if bit == run_value else 1
        run_value = bit
        if run_len == STUFF_RUN:
            skip_next = True
        if len(raw) == HEADER_BITS:
            dlc = min(_from_bits(raw[15:19]), 8)
            needed = HEADER_BITS + 8 * dlc + 15
    # A stuff bit may follow the last CRC bit.
    if skip_next:
        if pos >= len(bits) or bits[pos] == run_value:
            raise StuffError("missing stuff bit after CRC")
        pos += 1

    can_id = _from_bits(raw[1:12])
    if raw[12] != 0 or raw[13] != 0:
        raise ValueError("only standard data frames are supported")
    dlc = min(_from_bits(raw[15:19]), 8)
    data_end = HEADER_BITS + 8 * dlc
    data = bytes(_from_bits(raw[i:i + 8]) for i in range(HEADER_BITS, data_end, 8))
    if crc15(raw[:data_end]) != _from_bits(raw[data_end:data_end + 15]):
        raise ValueError("CRC mismatch")

    tail = list(bits[pos:pos + 3])
    if len(tail) < 3 or tail[0] != 1 or tail[2] != 1:
        raise ValueError("form error in CRC/ACK delimiter")
    return CanFrame(can_id, data)
