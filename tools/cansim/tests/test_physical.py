import io

import pytest

from cansim.frame import CanFrame, encode_wire
from cansim.physical import (
    DOMINANT_H,
    DOMINANT_L,
    RECESSIVE,
    ascii_plot,
    receive,
    to_waveform,
    write_vcd,
)

FRAME = CanFrame(0x3E8, bytes([0x79, 0x02, 0, 0, 0, 0, 0, 0]))


def test_levels_follow_bit_values():
    wave = to_waveform(encode_wire(FRAME), samples_per_bit=4, idle_bits=0)
    sof = wave[:4]
    assert all(s.can_h == DOMINANT_H and s.can_l == DOMINANT_L for s in sof)
    eof = wave[-4:]
    assert all(s.can_h == RECESSIVE and s.can_l == RECESSIVE for s in eof)


def test_time_axis_matches_bitrate():
    wave = to_waveform(encode_wire(FRAME), bitrate=500_000, samples_per_bit=10)
    assert wave[1].t - wave[0].t == pytest.approx(2e-6 / 10)


def test_receive_roundtrip():
    wave = to_waveform(encode_wire(FRAME))
    assert receive(wave) == FRAME


def test_common_mode_noise_is_rejected():
    # 1 V of noise on both wires at once: differential receiver does not care
    wave = to_waveform(encode_wire(FRAME), common_mode_noise_v=1.0, seed=7)
    assert receive(wave) == FRAME


def test_large_differential_noise_breaks_frame():
    wave = to_waveform(encode_wire(FRAME), diff_noise_v=2.0, seed=7)
    with pytest.raises(ValueError):
        receive(wave)


def test_ascii_plot_has_three_voltage_rows():
    text = ascii_plot(encode_wire(CanFrame(0x001, b"")))
    lines = text.splitlines()
    assert any(line.startswith("3.5V") for line in lines)
    assert any(line.startswith("2.5V") for line in lines)
    assert any(line.startswith("1.5V") for line in lines)


def test_vcd_header_and_changes():
    out = io.StringIO()
    write_vcd(to_waveform(encode_wire(FRAME), samples_per_bit=2), out)
    text = out.getvalue()
    assert "$var real 64 h CAN_H $end" in text
    assert "$var real 64 l CAN_L $end" in text
    assert "r3.5 h" in text and "r1.5 l" in text
