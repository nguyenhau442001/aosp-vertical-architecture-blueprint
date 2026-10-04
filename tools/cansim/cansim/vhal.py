"""VHAL property id anatomy (hardware/interfaces/automotive/vehicle/aidl).

    propId = group | area_type | value_type | unique_id

    bits 31..28  VehiclePropertyGroup  (SYSTEM 0x1, VENDOR 0x2, BACKPORTED 0x3)
    bits 27..24  VehicleArea           (GLOBAL, SEAT, WINDOW, ...)
    bits 23..16  VehiclePropertyType   (INT32, FLOAT, BOOLEAN, ...)
    bits 15..0   unique id inside the group
"""

GROUP = {"SYSTEM": 0x10000000, "VENDOR": 0x20000000, "BACKPORTED": 0x30000000}

AREA = {
    "GLOBAL": 0x01000000,
    "WINDOW": 0x03000000,
    "MIRROR": 0x04000000,
    "SEAT": 0x05000000,
    "DOOR": 0x06000000,
    "WHEEL": 0x07000000,
    "VENDOR": 0x08000000,
}

TYPE = {
    "STRING": 0x00100000,
    "BOOLEAN": 0x00200000,
    "INT32": 0x00400000,
    "INT32_VEC": 0x00410000,
    "INT64": 0x00500000,
    "INT64_VEC": 0x00510000,
    "FLOAT": 0x00600000,
    "FLOAT_VEC": 0x00610000,
    "BYTES": 0x00700000,
    "MIXED": 0x00E00000,
}

# Subset of VehicleProperty.aidl used by signals in this repo.
KNOWN_PROPERTIES = {
    "PERF_VEHICLE_SPEED": 0x11600207,
    "ENGINE_COOLANT_TEMP": 0x11600301,
    "ENGINE_OIL_TEMP": 0x11600304,
    "FUEL_LEVEL": 0x11600307,
    "EV_BATTERY_LEVEL": 0x11600309,
    "GEAR_SELECTION": 0x11400400,
    "PARKING_BRAKE_ON": 0x11200402,
    "HVAC_TEMPERATURE_CURRENT": 0x15600502,
    "HVAC_TEMPERATURE_SET": 0x15600503,
    "ENV_OUTSIDE_TEMPERATURE": 0x11600703,
}

# VehicleAreaSeat bits and the HVAC zones used by the emulator's default config.
SEAT = {
    "ROW_1_LEFT": 0x0001,
    "ROW_1_CENTER": 0x0002,
    "ROW_1_RIGHT": 0x0004,
    "ROW_2_LEFT": 0x0010,
    "ROW_2_CENTER": 0x0020,
    "ROW_2_RIGHT": 0x0040,
}
HVAC_LEFT = SEAT["ROW_1_LEFT"] | SEAT["ROW_2_LEFT"] | SEAT["ROW_2_CENTER"]  # 49
HVAC_RIGHT = SEAT["ROW_1_RIGHT"] | SEAT["ROW_2_RIGHT"]  # 68


def make_property_id(group: str, area: str, value_type: str, unique_id: int) -> int:
    if not 0 <= unique_id <= 0xFFFF:
        raise ValueError("unique id must fit in 16 bits")
    return GROUP[group] | AREA[area] | TYPE[value_type] | unique_id


def _name_of(table, bits):
    return next((k for k, v in table.items() if v == bits), f"{bits:#x}")


def explain_property_id(prop_id: int) -> str:
    group = _name_of(GROUP, prop_id & 0xF0000000)
    area = _name_of(AREA, prop_id & 0x0F000000)
    value_type = _name_of(TYPE, prop_id & 0x00FF0000)
    return f"{prop_id:#010x} = {group} | {area} | {value_type} | {prop_id & 0xFFFF:#06x}"


def resolve_property(text: str) -> int:
    """Standard property name or a literal id such as 0x21600101."""
    if text in KNOWN_PROPERTIES:
        return KNOWN_PROPERTIES[text]
    try:
        return int(text, 0)
    except ValueError:
        raise ValueError(f"unknown VHAL property '{text}' (add it to KNOWN_PROPERTIES "
                         f"or use a numeric id)") from None


def value_type_of(prop_id: int) -> str:
    return _name_of(TYPE, prop_id & 0x00FF0000)
