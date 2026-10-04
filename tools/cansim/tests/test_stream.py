import socket
import time
from pathlib import Path

import pytest

from cansim.dbc import load_dbc, parse_dbc
from cansim.frame import CanFrame
from cansim.stream import (
    WIRE_SIZE,
    Bus,
    Ramp,
    SignalSource,
    TcpFrameServer,
    from_wire16,
    to_wire16,
)

DBC_PATH = Path(__file__).resolve().parents[3] / "vehicle" / "dbc" / "blueprint.dbc"


@pytest.fixture(scope="module")
def thermal():
    return load_dbc(DBC_PATH).by_name["THERMAL_STATUS"]


def test_wire16_matches_native_layout():
    blob = to_wire16(CanFrame(0x3E8, bytes([0x79, 0x02, 0, 0, 0, 0, 0, 0])))
    assert WIRE_SIZE == 16
    assert blob == bytes([0xE8, 0x03, 0, 0, 8, 0, 0, 0, 0x79, 0x02, 0, 0, 0, 0, 0, 0])
    assert from_wire16(blob) == CanFrame(0x3E8, bytes([0x79, 0x02, 0, 0, 0, 0, 0, 0]))


def test_ramp_is_triangle():
    r = Ramp(-10, 40, 20)
    assert r.value(0) == -10
    assert r.value(10) == 40
    assert r.value(5) == pytest.approx(15)
    assert r.value(20) == -10


def test_counter_wraps(thermal):
    src = SignalSource(thermal, {"OutsideTemp": 20}, counter="ThermalCounter")
    counters = [thermal.decode(src.frame_at(0).data)["ThermalCounter"] for _ in range(17)]
    assert counters[:3] == [0, 1, 2]
    assert counters[15] == 15
    assert counters[16] == 0


def test_set_overrides_ramp(thermal):
    src = SignalSource(thermal, {}, ramps={"OutsideTemp": Ramp(0, 50, 10)})
    src.set("OutsideTemp", 23.3)
    assert thermal.decode(src.frame_at(5).data)["OutsideTemp"] == pytest.approx(23.3)


def test_unknown_signal_is_rejected(thermal):
    with pytest.raises(KeyError):
        SignalSource(thermal, {"NoSuchSignal": 1})


def test_tcp_server_broadcasts_frames(thermal):
    server = TcpFrameServer(port=0)
    try:
        client = socket.create_connection(("127.0.0.1", server.port), timeout=2)
        deadline = time.time() + 2
        while server.client_count == 0 and time.time() < deadline:
            time.sleep(0.01)
        frame = SignalSource(thermal, {"OutsideTemp": 23.3}).frame_at(0)
        server.broadcast(frame)
        blob = b""
        while len(blob) < WIRE_SIZE:
            blob += client.recv(WIRE_SIZE - len(blob))
        assert from_wire16(blob) == frame
        client.close()
    finally:
        server.close()


TWO_MESSAGES = parse_dbc("""
BO_ 16 A: 2 X
 SG_ Temp : 0|8@1+ (1,-40) [-40|125] "" Y
 SG_ Cnt : 8|4@1+ (1,0) [0|15] "" Y
BO_ 17 B: 2 X
 SG_ Temp : 0|8@1+ (1,-40) [-40|125] "" Y
 SG_ Level : 8|8@1+ (1,0) [0|100] "" Y
""")


def test_bus_resolves_unique_and_qualified_names():
    bus = Bus(TWO_MESSAGES, ["A", "B"], values=[("Level", 50), ("A.Temp", 20), ("B.Temp", 30)],
              counters=["Cnt"])
    frames = bus.frames_at(0)
    assert [f.can_id for f in frames] == [16, 17]
    assert TWO_MESSAGES.by_name["A"].decode(frames[0].data)["Temp"] == 20
    assert TWO_MESSAGES.by_name["B"].decode(frames[1].data) == {"Temp": 30, "Level": 50}
    bus.set("Level", 75)
    assert TWO_MESSAGES.by_name["B"].decode(bus.frames_at(0)[1].data)["Level"] == 75


def test_bus_rejects_ambiguous_and_unknown_names():
    with pytest.raises(KeyError, match="ambiguous"):
        Bus(TWO_MESSAGES, ["A", "B"], values=[("Temp", 1)])
    with pytest.raises(KeyError):
        Bus(TWO_MESSAGES, ["A"], values=[("Level", 1)])
    with pytest.raises(KeyError):
        Bus(TWO_MESSAGES, ["C"])
