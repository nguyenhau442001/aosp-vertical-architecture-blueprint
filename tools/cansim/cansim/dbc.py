"""Minimal DBC reader and signal codec.

Supports what this repo needs: BO_ messages, SG_ signals (Intel and Motorola,
signed and unsigned, factor/offset/min/max/unit) and BA_ attributes on signals.
Multiplexed signals and extended (29-bit) ids are rejected.
"""

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Mapping, Union

_BO = re.compile(r"^BO_\s+(\d+)\s+(\w+)\s*:\s*(\d+)\s+(\w+)")
_SG = re.compile(
    r"^SG_\s+(\w+)\s*(\S+)?\s*:\s*(\d+)\|(\d+)@([01])([+-])\s*"
    r"\(([^,]+),([^)]+)\)\s*\[([^|]+)\|([^\]]+)\]\s*\"([^\"]*)\"\s*(.*)$"
)
_BA_SG = re.compile(r'^BA_\s+"(\w+)"\s+SG_\s+(\d+)\s+(\w+)\s+(.+?)\s*;')


@dataclass
class Signal:
    name: str
    start: int
    length: int
    little_endian: bool = True
    signed: bool = False
    factor: float = 1.0
    offset: float = 0.0
    minimum: float = 0.0
    maximum: float = 0.0
    unit: str = ""
    attributes: Dict[str, Union[str, float]] = field(default_factory=dict)

    def bit_positions(self) -> List[int]:
        """Absolute bit positions (byte*8 + bit), MSB of the signal first."""
        if self.little_endian:
            return [self.start + i for i in reversed(range(self.length))]
        positions = []
        pos = self.start
        for _ in range(self.length):
            positions.append(pos)
            pos = pos + 15 if pos % 8 == 0 else pos - 1
        return positions

    def extract(self, data: bytes) -> int:
        """Raw integer value (sign-extended when the signal is signed)."""
        raw = 0
        for pos in self.bit_positions():
            raw = (raw << 1) | ((data[pos // 8] >> (pos % 8)) & 1)
        if self.signed and raw & (1 << (self.length - 1)):
            raw -= 1 << self.length
        return raw

    def insert(self, data: bytearray, raw: int) -> bytearray:
        lo, hi = self.raw_range()
        if not lo <= raw <= hi:
            raise ValueError(f"{self.name}: raw {raw} outside {lo}..{hi}")
        raw &= (1 << self.length) - 1
        for i, pos in enumerate(reversed(self.bit_positions())):
            byte, bit = divmod(pos, 8)
            if (raw >> i) & 1:
                data[byte] |= 1 << bit
            else:
                data[byte] &= ~(1 << bit) & 0xFF
        return data

    def raw_range(self):
        if self.signed:
            return -(1 << (self.length - 1)), (1 << (self.length - 1)) - 1
        return 0, (1 << self.length) - 1

    def decode(self, data: bytes) -> float:
        return self.extract(data) * self.factor + self.offset

    def to_raw(self, physical: float) -> int:
        has_range = not (self.minimum == 0 and self.maximum == 0)
        if has_range and not self.minimum <= physical <= self.maximum:
            raise ValueError(
                f"{self.name}: {physical} outside [{self.minimum}, {self.maximum}]"
            )
        return round((physical - self.offset) / self.factor)


@dataclass
class Message:
    can_id: int
    name: str
    dlc: int
    sender: str
    signals: List[Signal] = field(default_factory=list)

    def signal(self, name: str) -> Signal:
        for s in self.signals:
            if s.name == name:
                return s
        raise KeyError(f"{self.name} has no signal {name}")

    def decode(self, data: bytes) -> Dict[str, float]:
        if len(data) < self.dlc:
            raise ValueError(f"{self.name}: need {self.dlc} bytes, got {len(data)}")
        return {s.name: s.decode(data) for s in self.signals}

    def encode(self, values: Mapping[str, float]) -> bytes:
        data = bytearray(self.dlc)
        for name, value in values.items():
            sig = self.signal(name)
            sig.insert(data, sig.to_raw(value))
        return bytes(data)


@dataclass
class Database:
    messages: List[Message]

    @property
    def by_id(self) -> Dict[int, Message]:
        return {m.can_id: m for m in self.messages}

    @property
    def by_name(self) -> Dict[str, Message]:
        return {m.name: m for m in self.messages}


def _number(text: str) -> float:
    value = float(text)
    return int(value) if value.is_integer() else value


def _check_overlap(msg: Message) -> None:
    used: Dict[int, str] = {}
    for sig in msg.signals:
        for pos in sig.bit_positions():
            if pos >= msg.dlc * 8:
                raise ValueError(f"{msg.name}.{sig.name} exceeds {msg.dlc} bytes")
            if pos in used:
                raise ValueError(f"{msg.name}: {sig.name} overlaps {used[pos]} at bit {pos}")
            used[pos] = sig.name


def parse_dbc(text: str) -> Database:
    messages: List[Message] = []
    current = None
    for line_no, raw_line in enumerate(text.splitlines(), 1):
        line = raw_line.strip()
        if m := _BO.match(line):
            can_id = int(m.group(1))
            if can_id > 0x7FF:
                raise ValueError(f"line {line_no}: only 11-bit ids supported")
            current = Message(can_id, m.group(2), int(m.group(3)), m.group(4))
            messages.append(current)
        elif line.startswith("SG_"):
            m = _SG.match(line)
            if not m or current is None:
                raise ValueError(f"line {line_no}: cannot parse signal: {line}")
            if m.group(2):
                raise ValueError(f"line {line_no}: multiplexed signals not supported")
            current.signals.append(Signal(
                name=m.group(1),
                start=int(m.group(3)),
                length=int(m.group(4)),
                little_endian=m.group(5) == "1",
                signed=m.group(6) == "-",
                factor=_number(m.group(7)),
                offset=_number(m.group(8)),
                minimum=_number(m.group(9)),
                maximum=_number(m.group(10)),
                unit=m.group(11),
            ))
        elif m := _BA_SG.match(line):
            attr, can_id, sig_name, value = m.groups()
            msg = next((x for x in messages if x.can_id == int(can_id)), None)
            if msg is None:
                raise ValueError(f"line {line_no}: attribute for unknown message {can_id}")
            value = value.strip()
            msg.signal(sig_name).attributes[attr] = (
                value.strip('"') if value.startswith('"') else _number(value)
            )
    for msg in messages:
        _check_overlap(msg)
    return Database(messages)


def load_dbc(path: Union[str, Path]) -> Database:
    return parse_dbc(Path(path).read_text())
