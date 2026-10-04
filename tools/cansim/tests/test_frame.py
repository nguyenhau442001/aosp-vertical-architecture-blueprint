import pytest

from cansim.frame import (
    CanFrame,
    StuffError,
    crc15,
    decode_wire,
    encode_wire,
    stuff,
    unstuff,
)


def test_crc15_of_message_plus_crc_is_zero():
    bits = [0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1]
    crc = crc15(bits)
    crc_bits = [(crc >> (14 - i)) & 1 for i in range(15)]
    assert crc15(bits + crc_bits) == 0


def test_crc15_empty_is_zero():
    assert crc15([]) == 0


def test_stuff_inserts_opposite_bit_after_five_identical():
    assert stuff([0, 0, 0, 0, 0]) == [0, 0, 0, 0, 0, 1]
    assert stuff([1, 1, 1, 1, 1, 1]) == [1, 1, 1, 1, 1, 0, 1]


def test_stuff_bit_counts_toward_next_run():
    # 5 zeros -> stuffed 1, which starts a new run with the following four 1s
    assert stuff([0, 0, 0, 0, 0, 1, 1, 1, 1]) == [0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0]


def test_unstuff_reverses_stuff():
    raw = [0] * 12 + [1] * 7 + [0, 1, 0]
    assert unstuff(stuff(raw)) == raw


def test_unstuff_rejects_six_identical_bits():
    with pytest.raises(StuffError):
        unstuff([1, 1, 1, 1, 1, 1])


def test_frame_rejects_bad_id_and_length():
    with pytest.raises(ValueError):
        CanFrame(0x800, b"")
    with pytest.raises(ValueError):
        CanFrame(0x123, bytes(9))


def test_unstuffed_bits_layout():
    frame = CanFrame(0x3E8, bytes([0x79, 0x02]))
    bits = frame.unstuffed_bits()
    # SOF + ID(11) + RTR + IDE + r0 + DLC(4) + DATA(16) + CRC(15)
    assert len(bits) == 1 + 11 + 3 + 4 + 16 + 15
    assert bits[0] == 0
    assert bits[1:12] == [0, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0]  # 0x3E8
    assert bits[15:19] == [0, 0, 1, 0]  # DLC = 2


def test_wire_marks_stuff_bits_and_fields():
    frame = CanFrame(0x3E8, bytes([0x79, 0x02]))
    wire = encode_wire(frame)
    fields = [b.field for b in wire]
    assert fields[0] == "SOF"
    assert fields[-7:] == ["EOF"] * 7
    assert any(b.stuffed for b in wire)
    # ID 0x3E8 = 0b01111101000: 0,1,1,1,1,1 -> stuff 0 after five 1s
    id_and_stuff = [b for b in wire if b.field == "ID"]
    assert [b.value for b in id_and_stuff][:7] == [0, 1, 1, 1, 1, 1, 0]
    assert id_and_stuff[6].stuffed


@pytest.mark.parametrize(
    "can_id,data",
    [
        (0x000, b""),
        (0x7FF, bytes([0xFF] * 8)),
        (0x3E8, bytes([0x79, 0x02, 0, 0, 0, 0, 0, 0])),
        (0x123, bytes([0x00] * 8)),
    ],
)
def test_wire_roundtrip(can_id, data):
    frame = CanFrame(can_id, data)
    assert decode_wire([b.value for b in encode_wire(frame)]) == frame


def test_decode_detects_crc_error():
    wire = [b.value for b in encode_wire(CanFrame(0x3E8, bytes([0x79, 0x02])))]
    # flip a data bit that is not a stuff bit and does not create a stuff error
    idx = next(
        i
        for i, b in enumerate(encode_wire(CanFrame(0x3E8, bytes([0x79, 0x02]))))
        if b.field == "DATA" and not b.stuffed
    )
    wire[idx] ^= 1
    with pytest.raises(ValueError):
        decode_wire(wire)
