from pathlib import Path

import pytest

from cansim.codegen import generate_header
from cansim.dbc import load_dbc, parse_dbc
from cansim.vhal import (
    HVAC_LEFT,
    HVAC_RIGHT,
    KNOWN_PROPERTIES,
    VENDOR_PROPERTIES,
    explain_property_id,
    make_property_id,
    resolve_property,
)

REPO = Path(__file__).resolve().parents[3]
DBC_PATH = REPO / "vehicle" / "dbc" / "blueprint.dbc"
GENERATED = REPO / "native" / "canbridge" / "generated" / "BlueprintSignals.h"


def test_known_ids_match_their_anatomy():
    assert make_property_id("SYSTEM", "GLOBAL", "FLOAT", 0x0703) == \
        KNOWN_PROPERTIES["ENV_OUTSIDE_TEMPERATURE"]
    assert make_property_id("SYSTEM", "SEAT", "FLOAT", 0x0502) == \
        KNOWN_PROPERTIES["HVAC_TEMPERATURE_CURRENT"]
    assert make_property_id("SYSTEM", "GLOBAL", "INT32", 0x0400) == \
        KNOWN_PROPERTIES["GEAR_SELECTION"]


def test_hvac_zones_match_emulator_config():
    assert (HVAC_LEFT, HVAC_RIGHT) == (49, 68)


def test_explain_vendor_property():
    assert explain_property_id(0x21600101) == "0x21600101 = VENDOR | GLOBAL | FLOAT | 0x0101"


def test_vendor_ids_live_in_vendor_group():
    for name, prop_id in VENDOR_PROPERTIES.items():
        assert prop_id & 0xF0000000 == 0x20000000, name
    assert make_property_id("VENDOR", "GLOBAL", "FLOAT", 0x0101) == \
        VENDOR_PROPERTIES["VENDOR_BATTERY_CELL_TEMP_MAX"]


def test_battery_cell_temp_is_bound_to_vendor_property():
    text = generate_header(load_dbc(DBC_PATH), "vehicle/dbc/blueprint.dbc")
    assert '{0x3E9, 8, &kBATTERY_THERMAL_CellTempMax, 0x21600101, 0, ' \
           '"VENDOR_BATTERY_CELL_TEMP_MAX", ChangeMode::kOnChange, 0.0f, 0.0f},' in text


def test_resolve_property_accepts_names_and_numbers():
    assert resolve_property("ENV_OUTSIDE_TEMPERATURE") == 0x11600703
    assert resolve_property("0x21600101") == 0x21600101
    with pytest.raises(ValueError):
        resolve_property("NOT_A_PROPERTY")


def test_dbc_attributes_drive_bindings():
    db = load_dbc(DBC_PATH)
    sig = db.by_name["THERMAL_STATUS"].signal("CabinTempRow1Left")
    assert sig.attributes == {"VhalProperty": "HVAC_TEMPERATURE_CURRENT", "VhalAreaId": 49}


def test_codegen_emits_specs_and_bindings():
    text = generate_header(load_dbc(DBC_PATH), "vehicle/dbc/blueprint.dbc")
    assert 'inline constexpr SignalSpec kTHERMAL_STATUS_OutsideTemp{"OutsideTemp", 0, 16, ' \
           'ByteOrder::kIntel, false, 0.1, -40.0, -40.0, 125.0};' in text
    assert '{0x3E8, 8, &kTHERMAL_STATUS_OutsideTemp, 0x11600703, 0, ' \
           '"ENV_OUTSIDE_TEMPERATURE", ChangeMode::kContinuous, 1.0f, 2.0f},' in text
    assert "EvaporatorTemp, 0x" not in text  # no VhalProperty attribute -> not routed


def test_codegen_defaults_to_on_change():
    dbc = """
BO_ 1 M: 8 X
 SG_ A : 0|8@1+ (1,0) [0|255] "" Y
BA_ "VhalProperty" SG_ 1 A "0x21600001";
"""
    text = generate_header(parse_dbc(dbc), "x.dbc")
    assert '"0x21600001", ChangeMode::kOnChange, 0.0f, 0.0f},' in text


def test_codegen_rejects_bad_change_mode():
    dbc = """
BO_ 1 M: 8 X
 SG_ A : 0|8@1+ (1,0) [0|255] "" Y
BA_ "VhalProperty" SG_ 1 A "0x21600001";
BA_ "VhalChangeMode" SG_ 1 A "SOMETIMES";
"""
    with pytest.raises(ValueError):
        generate_header(parse_dbc(dbc), "x.dbc")


def test_codegen_rejects_unsupported_type():
    dbc = """
BO_ 1 M: 8 X
 SG_ A : 0|8@1+ (1,0) [0|255] "" Y
BA_ "VhalProperty" SG_ 1 A "0x21100001";
"""
    with pytest.raises(ValueError):
        generate_header(parse_dbc(dbc), "x.dbc")


def test_checked_in_header_is_up_to_date():
    expected = generate_header(load_dbc(DBC_PATH), "vehicle/dbc/blueprint.dbc")
    assert GENERATED.read_text() == expected, "run: make codegen"
