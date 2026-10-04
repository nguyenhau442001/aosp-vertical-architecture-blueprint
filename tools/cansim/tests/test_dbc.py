from pathlib import Path

import pytest

from cansim.dbc import Signal, load_dbc, parse_dbc

DBC_PATH = Path(__file__).resolve().parents[3] / "vehicle" / "dbc" / "blueprint.dbc"


@pytest.fixture(scope="module")
def db():
    return load_dbc(DBC_PATH)


def test_parses_message_and_signals(db):
    msg = db.by_name["THERMAL_STATUS"]
    assert msg.can_id == 0x3E8
    assert msg.dlc == 8
    assert [s.name for s in msg.signals] == [
        "OutsideTemp",
        "CabinTempRow1Left",
        "CabinTempRow1Right",
        "EvaporatorTemp",
        "ThermalCounter",
    ]
    sig = msg.signal("OutsideTemp")
    assert (sig.start, sig.length, sig.little_endian, sig.signed) == (0, 16, True, False)
    assert (sig.factor, sig.offset, sig.unit) == (0.1, -40, "degC")


def test_intel_decode_matches_primer_example(db):
    msg = db.by_id[0x3E8]
    values = msg.decode(bytes([0x79, 0x02, 0, 0, 0, 0, 0, 0]))
    assert values["OutsideTemp"] == pytest.approx(23.3)


def test_motorola_signed_layout():
    sig = Signal("T", start=39, length=16, little_endian=False, signed=True,
                 factor=0.01, offset=0, minimum=-50, maximum=150)
    data = sig.insert(bytearray(8), -1234)  # raw two's complement 0xFB2E
    assert data[4:6] == bytes([0xFB, 0x2E])
    assert sig.extract(bytes(data)) == -1234
    assert sig.decode(bytes(data)) == pytest.approx(-12.34)


def test_motorola_crosses_byte_boundary_not_aligned():
    # 12-bit big-endian signal starting at bit 11 (byte1 bit3) -> byte1[3:0], byte2[7:0]
    sig = Signal("X", start=11, length=12, little_endian=False)
    data = sig.insert(bytearray(8), 0xABC)
    assert data[1] == 0x0A
    assert data[2] == 0xBC
    assert sig.extract(bytes(data)) == 0xABC


def test_encode_decode_roundtrip_all_signals(db):
    msg = db.by_name["THERMAL_STATUS"]
    values = {
        "OutsideTemp": 23.3,
        "CabinTempRow1Left": 21.5,
        "CabinTempRow1Right": 24.0,
        "EvaporatorTemp": -3.25,
        "ThermalCounter": 9,
    }
    decoded = msg.decode(msg.encode(values))
    for name, value in values.items():
        assert decoded[name] == pytest.approx(value)


def test_encode_rejects_out_of_range(db):
    with pytest.raises(ValueError):
        db.by_name["THERMAL_STATUS"].encode({"OutsideTemp": 200})


def test_unspecified_signals_encode_as_zero_raw(db):
    data = db.by_name["THERMAL_STATUS"].encode({"OutsideTemp": -40})
    assert data == bytes(8)


def test_parse_rejects_overlapping_signals():
    text = """
BO_ 1 M: 8 X
 SG_ A : 0|8@1+ (1,0) [0|255] "" Y
 SG_ B : 4|8@1+ (1,0) [0|255] "" Y
"""
    with pytest.raises(ValueError):
        parse_dbc(text)
