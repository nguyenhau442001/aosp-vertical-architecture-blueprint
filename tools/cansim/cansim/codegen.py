"""Generate the C++ signal/binding table consumed by native/canbridge.

DBC is the single source of truth. A signal is routed to VHAL when it carries
the attribute `VhalProperty` (and optionally `VhalAreaId`, default 0 = global).
"""

from typing import List

from .dbc import Database
from .vhal import explain_property_id, resolve_property, value_type_of

SUPPORTED_TYPES = {"FLOAT", "INT32", "BOOLEAN"}

_HEADER = """\
// AUTO-GENERATED from {source} by `python3 -m cansim codegen`. DO NOT EDIT.
// Edit the DBC, then run: make codegen

#pragma once

#include "canbridge/SignalBinding.h"

namespace blueprint::can::generated {{
"""

_FOOTER = """\
}}  // namespace blueprint::can::generated
"""


def _cpp_double(value: float) -> str:
    text = repr(float(value))
    return text if ("." in text or "e" in text) else text + ".0"


def _signal_var(msg_name: str, sig_name: str) -> str:
    return f"k{msg_name}_{sig_name}"


def generate_header(db: Database, source: str) -> str:
    out: List[str] = [_HEADER.format(source=source)]
    bindings: List[str] = []

    for msg in db.messages:
        out.append(f"// {msg.name}: id 0x{msg.can_id:03X}, {msg.dlc} bytes, sender {msg.sender}")
        for sig in msg.signals:
            order = "ByteOrder::kIntel" if sig.little_endian else "ByteOrder::kMotorola"
            out.append(
                f"inline constexpr SignalSpec {_signal_var(msg.name, sig.name)}{{"
                f"\"{sig.name}\", {sig.start}, {sig.length}, {order}, "
                f"{'true' if sig.signed else 'false'}, {_cpp_double(sig.factor)}, "
                f"{_cpp_double(sig.offset)}, {_cpp_double(sig.minimum)}, "
                f"{_cpp_double(sig.maximum)}}};"
            )
            prop = sig.attributes.get("VhalProperty")
            if not prop:
                continue
            prop_id = resolve_property(str(prop))
            vtype = value_type_of(prop_id)
            if vtype not in SUPPORTED_TYPES:
                raise ValueError(f"{msg.name}.{sig.name}: {vtype} properties not supported")
            area = int(sig.attributes.get("VhalAreaId", 0))
            bindings.append(
                f"        {{0x{msg.can_id:03X}, {msg.dlc}, &{_signal_var(msg.name, sig.name)}, "
                f"0x{prop_id:08X}, {area}, \"{prop}\"}},  "
                f"// {explain_property_id(prop_id)}"
            )
        out.append("")

    out.append("inline constexpr SignalBinding kBindings[] = {")
    out.extend(bindings)
    out.append("};")
    out.append("")
    out.append(_FOOTER.format())
    return "\n".join(out)
