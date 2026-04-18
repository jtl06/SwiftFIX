#!/usr/bin/env python3
"""Regenerate corpus/valid/ synthetic FIX messages.

Authoritative source for the valid corpus. Every template below is expanded
into a well-formed FIX.4.4 frame with correctly-computed BodyLength (tag 9)
and CheckSum (tag 10) before being written to disk.

Usage:
    python3 corpus/generate.py              # regenerates corpus/valid/*
    python3 corpus/generate.py --dry-run    # prints what it would write

Templates are intentionally written as a list of (tag, value) pairs rather
than as pre-formatted strings so we can compute the length/checksum
post-hoc. BeginString (8) and BodyLength (9) and CheckSum (10) are inserted
automatically — do not include them in templates.
"""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path
from typing import Iterable

SOH = "\x01"
BEGIN_STRING = "FIX.4.4"


def render(tags: Iterable[tuple[int, str]]) -> bytes:
    """Render a (tag, value) sequence into a FIX frame with computed 9/10."""
    body = "".join(f"{t}={v}{SOH}" for t, v in tags)
    body_length = len(body)
    prefix = f"8={BEGIN_STRING}{SOH}9={body_length}{SOH}"
    head_and_body = prefix + body
    checksum = sum(head_and_body.encode("ascii")) % 256
    trailer = f"10={checksum:03d}{SOH}"
    return (head_and_body + trailer).encode("ascii")


# --- templates -------------------------------------------------------------
#
# Each entry: (filename, [(tag, value), ...]).
#
# Tag values are strings because that's what FIX is on the wire. Don't
# include tags 8, 9, or 10 — render() handles those.

TEMPLATES: list[tuple[str, list[tuple[int, str]]]] = [
    # 1. Logon
    ("01_logon", [
        (35, "A"),
        (49, "SENDER"),
        (56, "TARGET"),
        (34, "1"),
        (52, "20260101-00:00:00.000"),
        (98, "0"),            # EncryptMethod = None
        (108, "30"),          # HeartBtInt
    ]),

    # 2. Heartbeat
    ("02_heartbeat", [
        (35, "0"),
        (49, "SENDER"),
        (56, "TARGET"),
        (34, "2"),
        (52, "20260101-00:00:30.000"),
    ]),

    # 3. TestRequest
    ("03_test_request", [
        (35, "1"),
        (49, "SENDER"),
        (56, "TARGET"),
        (34, "3"),
        (52, "20260101-00:00:31.000"),
        (112, "TEST123"),
    ]),

    # 4. ResendRequest
    ("04_resend_request", [
        (35, "2"),
        (49, "SENDER"),
        (56, "TARGET"),
        (34, "4"),
        (52, "20260101-00:00:32.000"),
        (7, "10"),
        (16, "20"),
    ]),

    # 5. NewOrderSingle (limit buy)
    ("05_new_order_single_buy", [
        (35, "D"),
        (49, "SENDER"),
        (56, "TARGET"),
        (34, "5"),
        (52, "20260101-00:01:00.000"),
        (11, "ORD00001"),
        (21, "1"),             # HandlInst = automated
        (55, "IBM"),
        (54, "1"),             # Side = buy
        (60, "20260101-00:01:00.000"),
        (38, "100"),           # OrderQty
        (40, "2"),             # OrdType = limit
        (44, "150.50"),        # Price
        (59, "0"),             # TimeInForce = Day
    ]),

    # 6. NewOrderSingle (market sell)
    ("06_new_order_single_sell", [
        (35, "D"),
        (49, "SENDER"),
        (56, "TARGET"),
        (34, "6"),
        (52, "20260101-00:01:05.000"),
        (11, "ORD00002"),
        (21, "1"),
        (55, "AAPL"),
        (54, "2"),             # Side = sell
        (60, "20260101-00:01:05.000"),
        (38, "50"),
        (40, "1"),             # OrdType = market
        (59, "0"),
    ]),

    # 7. ExecutionReport (filled)
    ("07_execution_report_filled", [
        (35, "8"),
        (49, "BROKER"),
        (56, "SENDER"),
        (34, "3"),
        (52, "20260101-00:01:00.123"),
        (37, "EXEC00001"),     # OrderID
        (17, "FILL00001"),     # ExecID
        (150, "F"),            # ExecType = trade
        (39, "2"),             # OrdStatus = filled
        (55, "IBM"),
        (54, "1"),
        (38, "100"),
        (32, "100"),           # LastShares
        (31, "150.50"),        # LastPx
        (151, "0"),            # LeavesQty
        (14, "100"),           # CumQty
        (6, "150.50"),         # AvgPx
    ]),

    # 8. ExecutionReport (partial fill)
    ("08_execution_report_partial", [
        (35, "8"),
        (49, "BROKER"),
        (56, "SENDER"),
        (34, "4"),
        (52, "20260101-00:01:06.456"),
        (37, "EXEC00002"),
        (17, "FILL00002"),
        (150, "F"),
        (39, "1"),             # OrdStatus = partial
        (55, "AAPL"),
        (54, "2"),
        (38, "50"),
        (32, "20"),
        (31, "200.25"),
        (151, "30"),
        (14, "20"),
        (6, "200.25"),
    ]),

    # 9. OrderCancelRequest
    ("09_order_cancel_request", [
        (35, "F"),
        (49, "SENDER"),
        (56, "TARGET"),
        (34, "7"),
        (52, "20260101-00:02:00.000"),
        (41, "ORD00001"),      # OrigClOrdID
        (11, "CXL00001"),
        (55, "IBM"),
        (54, "1"),
        (60, "20260101-00:02:00.000"),
    ]),

    # 10. Logout
    ("10_logout", [
        (35, "5"),
        (49, "SENDER"),
        (56, "TARGET"),
        (34, "99"),
        (52, "20260101-00:10:00.000"),
        (58, "End of session"),  # Text
    ]),
]


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true",
                    help="print what would be written, don't touch disk")
    ap.add_argument("--out-dir",
                    default=str(Path(__file__).parent / "valid"),
                    help="target directory (default: corpus/valid)")
    args = ap.parse_args(argv)

    out_dir = Path(args.out_dir)
    if not args.dry_run:
        out_dir.mkdir(parents=True, exist_ok=True)

    for name, tags in TEMPLATES:
        frame = render(tags)
        path = out_dir / name
        if args.dry_run:
            print(f"{path}: {len(frame)} bytes")
            print("  " + frame.decode("ascii").replace(SOH, "|"))
        else:
            path.write_bytes(frame)
            print(f"wrote {path} ({len(frame)} bytes)")

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
