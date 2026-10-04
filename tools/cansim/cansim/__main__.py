"""Command line entry: python3 -m cansim <command> ..."""

import argparse
import sys
from pathlib import Path

from .dbc import load_dbc
from .frame import CanFrame, encode_wire
from .physical import ascii_plot, receive, to_waveform, write_csv, write_vcd


DEFAULT_DBC = Path(__file__).resolve().parents[3] / "vehicle" / "dbc" / "blueprint.dbc"


def parse_frame(text: str) -> CanFrame:
    """candump/cansend notation: 3E8#7902 (hex id, hex payload)."""
    try:
        can_id, _, payload = text.partition("#")
        return CanFrame(int(can_id, 16), bytes.fromhex(payload))
    except ValueError as e:
        raise argparse.ArgumentTypeError(f"bad frame '{text}': {e}") from None


def parse_assignment(text: str):
    name, sep, value = text.partition("=")
    if not sep:
        raise argparse.ArgumentTypeError(f"expected Signal=value, got '{text}'")
    return name, float(value)


def cmd_encode(args) -> int:
    msg = load_dbc(args.dbc).by_name[args.message]
    frame = CanFrame(msg.can_id, msg.encode(dict(args.values)))
    print(frame)
    if args.wave:
        args.frame = frame
        return cmd_wave(args)
    return 0


def cmd_decode(args) -> int:
    msg = load_dbc(args.dbc).by_id.get(args.frame.can_id)
    if msg is None:
        print(f"{args.frame.can_id:03X} not in DBC")
        return 1
    print(f"{msg.name} ({args.frame})")
    for sig in msg.signals:
        print(f"  {sig.name:<20} raw={sig.extract(args.frame.data):<8} "
              f"value={sig.decode(args.frame.data):g} {sig.unit}")
    return 0


def _add_wave_options(w: argparse.ArgumentParser) -> None:
    w.add_argument("--bitrate", type=int, default=500_000)
    w.add_argument("--samples-per-bit", type=int, default=10)
    w.add_argument("--cm-noise", type=float, default=0.0,
                   help="common-mode noise amplitude in volts")
    w.add_argument("--diff-noise", type=float, default=0.0,
                   help="per-wire noise amplitude in volts")
    w.add_argument("--seed", type=int)
    w.add_argument("--csv", help="write sampled voltages to CSV")
    w.add_argument("--vcd", help="write VCD for GTKWave/PulseView")


def cmd_wave(args) -> int:
    wire = encode_wire(args.frame)
    samples = to_waveform(
        wire,
        bitrate=args.bitrate,
        samples_per_bit=args.samples_per_bit,
        common_mode_noise_v=args.cm_noise,
        diff_noise_v=args.diff_noise,
        seed=args.seed,
    )
    stuffed = sum(b.stuffed for b in wire)
    print(f"frame {args.frame}  dlc={args.frame.dlc}  wire bits={len(wire)} "
          f"(stuff bits={stuffed})  bit time={1e6 / args.bitrate:g} us")
    print(ascii_plot(wire))
    if args.csv:
        with open(args.csv, "w") as f:
            write_csv(samples, f)
        print(f"csv -> {args.csv}")
    if args.vcd:
        with open(args.vcd, "w") as f:
            write_vcd(samples, f)
        print(f"vcd -> {args.vcd}")
    try:
        print(f"receiver decoded: {receive(samples, args.samples_per_bit)}")
    except ValueError as e:
        print(f"receiver error: {e}")
        return 1
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="cansim", description=__doc__)
    sub = p.add_subparsers(dest="cmd", required=True)

    w = sub.add_parser("wave", help="render one frame as CAN_H/CAN_L voltages")
    w.add_argument("frame", type=parse_frame, help="e.g. 3E8#7902000000000000")
    _add_wave_options(w)
    w.set_defaults(func=cmd_wave)

    e = sub.add_parser("encode", help="build a frame from DBC signal values")
    e.add_argument("message", help="DBC message name, e.g. THERMAL_STATUS")
    e.add_argument("values", nargs="+", type=parse_assignment,
                   help="Signal=value in physical units, e.g. OutsideTemp=23.3")
    e.add_argument("--dbc", default=DEFAULT_DBC)
    e.add_argument("--wave", action="store_true", help="also show CAN_H/CAN_L")
    _add_wave_options(e)
    e.set_defaults(func=cmd_encode)

    d = sub.add_parser("decode", help="decode a frame with the DBC")
    d.add_argument("frame", type=parse_frame)
    d.add_argument("--dbc", default=DEFAULT_DBC)
    d.set_defaults(func=cmd_decode)
    return p


def main(argv=None) -> int:
    args = build_parser().parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
