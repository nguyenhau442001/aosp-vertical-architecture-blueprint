"""Periodic frame generation and delivery to the device under test.

Wire format over TCP = Linux struct can_frame (16 bytes, can_id little-endian),
identical to what native/cantransport TcpCanTransport expects.
"""

import socket
import struct
import threading
from typing import Dict, List, Optional, Tuple

from .dbc import Message
from .frame import CanFrame

WIRE_FORMAT = "<IB3x8s"
WIRE_SIZE = struct.calcsize(WIRE_FORMAT)
DEFAULT_PORT = 29536


def to_wire16(frame: CanFrame) -> bytes:
    return struct.pack(WIRE_FORMAT, frame.can_id, frame.dlc, frame.data.ljust(8, b"\0"))


def from_wire16(blob: bytes) -> CanFrame:
    can_id, dlc, data = struct.unpack(WIRE_FORMAT, blob)
    return CanFrame(can_id, data[:dlc])


class Ramp:
    """Triangle wave between lo and hi with the given period in seconds."""

    def __init__(self, lo: float, hi: float, period_s: float):
        if period_s <= 0:
            raise ValueError("ramp period must be > 0")
        self.lo, self.hi, self.period_s = lo, hi, period_s

    @classmethod
    def parse(cls, text: str) -> Tuple[str, "Ramp"]:
        """'OutsideTemp:-10:40:20' -> ('OutsideTemp', Ramp(-10, 40, 20))."""
        name, lo, hi, period = text.split(":")
        return name, cls(float(lo), float(hi), float(period))

    def value(self, t: float) -> float:
        phase = (t % self.period_s) / self.period_s
        tri = 2 * phase if phase < 0.5 else 2 * (1 - phase)
        return self.lo + (self.hi - self.lo) * tri


class SignalSource:
    """Current physical values of one message; produces frames on demand."""

    def __init__(self, message: Message, values: Dict[str, float],
                 counter: Optional[str] = None, ramps: Optional[Dict[str, Ramp]] = None):
        self.message = message
        self.values = dict(values)
        self.counter = counter
        self.ramps = dict(ramps or {})
        self._lock = threading.Lock()
        for name in list(self.values) + list(self.ramps) + ([counter] if counter else []):
            message.signal(name)  # raises KeyError on typos

    def set(self, name: str, value: float) -> None:
        self.message.signal(name)
        with self._lock:
            self.ramps.pop(name, None)
            self.values[name] = value

    def frame_at(self, t: float) -> CanFrame:
        with self._lock:
            for name, ramp in self.ramps.items():
                sig = self.message.signal(name)
                self.values[name] = round(ramp.value(t) / sig.factor) * sig.factor
            if self.counter:
                sig = self.message.signal(self.counter)
                _, hi = sig.raw_range()
                self.values[self.counter] = (self.values.get(self.counter, -1) + 1) % (hi + 1)
            data = self.message.encode(self.values)
        return CanFrame(self.message.can_id, data)


class TcpFrameServer:
    """Accepts any number of clients and broadcasts every frame to all of them."""

    def __init__(self, host: str = "127.0.0.1", port: int = DEFAULT_PORT):
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind((host, port))
        self._sock.listen()
        self.port = self._sock.getsockname()[1]
        self._clients: List[socket.socket] = []
        self._lock = threading.Lock()
        self._running = True
        threading.Thread(target=self._accept_loop, daemon=True).start()

    @property
    def client_count(self) -> int:
        with self._lock:
            return len(self._clients)

    def _accept_loop(self) -> None:
        while self._running:
            try:
                conn, _ = self._sock.accept()
            except OSError:
                return
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            with self._lock:
                self._clients.append(conn)

    def broadcast(self, frame: CanFrame) -> None:
        blob = to_wire16(frame)
        with self._lock:
            alive = []
            for c in self._clients:
                try:
                    c.sendall(blob)
                    alive.append(c)
                except OSError:
                    c.close()
            self._clients = alive

    def close(self) -> None:
        self._running = False
        self._sock.close()
        with self._lock:
            for c in self._clients:
                c.close()
            self._clients = []


class SocketCanSender:
    """Writes frames to a Linux SocketCAN interface such as vcan0."""

    def __init__(self, ifname: str):
        if not hasattr(socket, "AF_CAN"):
            raise OSError("SocketCAN is only available on Linux")
        self._sock = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        self._sock.bind((ifname,))

    def broadcast(self, frame: CanFrame) -> None:
        self._sock.send(to_wire16(frame))

    def close(self) -> None:
        self._sock.close()
