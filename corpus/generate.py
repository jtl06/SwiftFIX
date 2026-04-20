#!/usr/bin/env python3
"""Regenerate SwiftFIX corpus directories.

Two modes:

* Canonical (default): rebuilds the 10 hand-crafted messages in
  corpus/valid/ from fixed templates. Test byte-patterns in
  integration/tests/test_end_to_end.cpp pin to these; do not reorder
  or remove entries without updating that test.

      python3 corpus/generate.py

* Bulk: generates N procedurally-varied messages, concatenated back-to-back
  into a single binary blob at corpus/bulk.stream, using a seeded RNG.
  This matches what FIX looks like on the wire (continuous TCP bytes,
  messages demarcated only by their own 8=.../9=<bodylength>/10=cksm
  structure). The bench harness parses this with FIX::Parser, which is
  the stock stream-splitting path.

      python3 corpus/generate.py --bulk 1000 --seed 42

Both modes can run in one invocation: `--bulk N` leaves canonical alone
unless `--canonical` is also passed.
"""
from __future__ import annotations

import argparse
import random
import string
import sys
from datetime import datetime, timedelta
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


# --- Canonical templates ---------------------------------------------------
# Hand-crafted, byte-stable. Tests pin to these.

TEMPLATES: list[tuple[str, list[tuple[int, str]]]] = [
    ("01_logon", [
        (35, "A"), (49, "SENDER"), (56, "TARGET"),
        (34, "1"), (52, "20260101-00:00:00.000"),
        (98, "0"), (108, "30"),
    ]),
    ("02_heartbeat", [
        (35, "0"), (49, "SENDER"), (56, "TARGET"),
        (34, "2"), (52, "20260101-00:00:30.000"),
    ]),
    ("03_test_request", [
        (35, "1"), (49, "SENDER"), (56, "TARGET"),
        (34, "3"), (52, "20260101-00:00:31.000"),
        (112, "TEST123"),
    ]),
    ("04_resend_request", [
        (35, "2"), (49, "SENDER"), (56, "TARGET"),
        (34, "4"), (52, "20260101-00:00:32.000"),
        (7, "10"), (16, "20"),
    ]),
    ("05_new_order_single_buy", [
        (35, "D"), (49, "SENDER"), (56, "TARGET"),
        (34, "5"), (52, "20260101-00:01:00.000"),
        (11, "ORD00001"), (21, "1"), (55, "IBM"), (54, "1"),
        (60, "20260101-00:01:00.000"), (38, "100"), (40, "2"),
        (44, "150.50"), (59, "0"),
    ]),
    ("06_new_order_single_sell", [
        (35, "D"), (49, "SENDER"), (56, "TARGET"),
        (34, "6"), (52, "20260101-00:01:05.000"),
        (11, "ORD00002"), (21, "1"), (55, "AAPL"), (54, "2"),
        (60, "20260101-00:01:05.000"), (38, "50"), (40, "1"), (59, "0"),
    ]),
    ("07_execution_report_filled", [
        (35, "8"), (49, "BROKER"), (56, "SENDER"),
        (34, "3"), (52, "20260101-00:01:00.123"),
        (37, "EXEC00001"), (17, "FILL00001"), (150, "F"), (39, "2"),
        (55, "IBM"), (54, "1"), (38, "100"), (32, "100"), (31, "150.50"),
        (151, "0"), (14, "100"), (6, "150.50"),
    ]),
    ("08_execution_report_partial", [
        (35, "8"), (49, "BROKER"), (56, "SENDER"),
        (34, "4"), (52, "20260101-00:01:06.456"),
        (37, "EXEC00002"), (17, "FILL00002"), (150, "F"), (39, "1"),
        (55, "AAPL"), (54, "2"), (38, "50"), (32, "20"), (31, "200.25"),
        (151, "30"), (14, "20"), (6, "200.25"),
    ]),
    ("09_order_cancel_request", [
        (35, "F"), (49, "SENDER"), (56, "TARGET"),
        (34, "7"), (52, "20260101-00:02:00.000"),
        (41, "ORD00001"), (11, "CXL00001"), (55, "IBM"), (54, "1"),
        (60, "20260101-00:02:00.000"),
    ]),
    ("10_logout", [
        (35, "5"), (49, "SENDER"), (56, "TARGET"),
        (34, "99"), (52, "20260101-00:10:00.000"),
        (58, "End of session"),
    ]),
]


def write_canonical(out_dir: Path, dry_run: bool) -> None:
    if not dry_run:
        out_dir.mkdir(parents=True, exist_ok=True)
    for name, tags in TEMPLATES:
        frame = render(tags)
        path = out_dir / name
        if dry_run:
            print(f"{path}: {len(frame)} bytes  "
                  f"{frame.decode('ascii').replace(SOH, '|')}")
        else:
            path.write_bytes(frame)
            print(f"wrote {path} ({len(frame)} bytes)")


# --- Bulk generator --------------------------------------------------------
# Distribution targets (see BulkGenerator.BUILDERS) roughly match what a
# busy market-access session sees: ExecutionReport dominates, MarketData
# shows up in bursts, admin traffic (Heartbeat / TestRequest) is steady
# background. Tune weights if your target workload skews differently.

SYMBOLS = [
    "AAPL", "MSFT", "AMZN", "GOOGL", "META", "NVDA", "TSLA", "BRK.B",
    "JPM", "V", "WMT", "UNH", "JNJ", "MA", "PG", "HD", "BAC", "AVGO",
    "LLY", "ABBV", "CVX", "XOM", "KO", "MRK", "PEP", "IBM", "ORCL",
    "CRM", "ADBE", "NFLX", "SPY", "QQQ", "IWM",
    "DIS", "NKE", "MCD", "SBUX", "CSCO", "INTC", "AMD", "QCOM", "TXN",
    "MU", "AMAT", "LRCX", "KLAC", "PYPL", "SQ", "SHOP", "UBER", "LYFT",
    "ABNB", "DASH", "COIN", "MSTR", "PLTR", "SNOW", "DDOG", "CRWD",
    "ZS", "OKTA", "NET", "MDB", "TEAM", "WDAY", "NOW",
]

SENDERS = ["SENDER", "BUYSIDE1", "BUYSIDE2", "HFT01", "ALGO_A",
           "PRIME01", "DMA_DESK", "INST_FLOW", "RETAIL_AGG"]
TARGETS = ["TARGET", "BROKER_X", "ECN_Y", "EXCH_Z",
           "DARK_POOL_A", "LIT_VENUE_B", "MM_QUOTE_C"]

# Word pool for variable-length free-text fields. Keeps the generator
# deterministic while producing realistic-looking headlines / reasons.
TEXT_WORDS = [
    "order", "filled", "partial", "reject", "price", "limit", "market",
    "side", "buy", "sell", "quote", "spread", "depth", "book", "halt",
    "resume", "auction", "close", "open", "session", "venue", "route",
    "reroute", "latency", "timeout", "retry", "amend", "cancel", "replace",
    "allocation", "account", "fill", "liquidity", "maker", "taker",
]


class BulkGenerator:
    # (method_name, weight). Weights are normalized automatically by
    # random.choices, but keeping them as fractions-summing-to-1 makes
    # the distribution readable at a glance.
    BUILDERS: list[tuple[str, float]] = [
        ("execution_report",            0.32),
        ("new_order_single",            0.18),
        ("market_data_snapshot",        0.10),
        ("new_order_single_with_allocs", 0.05),
        ("heartbeat",                   0.10),
        ("test_request",                0.04),
        ("order_cancel_request",        0.05),
        ("logon",                       0.02),
        ("logout",                      0.02),
        ("resend_request",              0.02),
        ("news",                        0.08),
        ("market_data_incremental",     0.02),
    ]

    def __init__(self, seed: int) -> None:
        self.r = random.Random(seed)
        self.seq = 0
        self.start = datetime(2026, 1, 1)

    # ---- primitives ------------------------------------------------------

    def _next_seq(self) -> int:
        self.seq += 1
        return self.seq

    def _ts(self) -> str:
        t = self.start + timedelta(milliseconds=self.seq * 10)
        return t.strftime("%Y%m%d-%H:%M:%S") + f".{t.microsecond // 1000:03d}"

    def _cl_ord_id(self) -> str:
        return "CL" + "".join(self.r.choices(
            string.ascii_uppercase + string.digits, k=10))

    def _exec_id(self) -> str:
        return "EX" + "".join(self.r.choices(
            string.ascii_uppercase + string.digits, k=10))

    def _order_id(self) -> str:
        return "OR" + "".join(self.r.choices(
            string.ascii_uppercase + string.digits, k=10))

    def _price(self) -> str:
        # Log-normal around ~$100; clamped so we never emit <$0.01.
        return f"{max(0.01, self.r.lognormvariate(mu=4.6, sigma=0.8)):.2f}"

    def _qty(self) -> int:
        return self.r.choice([5, 10, 50, 75, 100, 200, 500, 1000, 5000, 10000])

    def _symbol(self) -> str:
        return self.r.choice(SYMBOLS)

    def _text(self, min_words: int, max_words: int) -> str:
        n = self.r.randint(min_words, max_words)
        return " ".join(self.r.choice(TEXT_WORDS) for _ in range(n))

    def _header(self, msg_type: str) -> list[tuple[int, str]]:
        return [
            (35, msg_type),
            (49, self.r.choice(SENDERS)),
            (56, self.r.choice(TARGETS)),
            (34, str(self._next_seq())),
            (52, self._ts()),
        ]

    # ---- per-message-type builders --------------------------------------

    def heartbeat(self) -> list[tuple[int, str]]:
        return self._header("0")

    def test_request(self) -> list[tuple[int, str]]:
        return self._header("1") + [(112, self._cl_ord_id())]

    def resend_request(self) -> list[tuple[int, str]]:
        begin = self.r.randint(1, 1000)
        end = begin + self.r.randint(1, 50)
        return self._header("2") + [(7, str(begin)), (16, str(end))]

    def logon(self) -> list[tuple[int, str]]:
        return self._header("A") + [
            (98, "0"),
            (108, str(self.r.choice([10, 30, 60]))),
        ]

    def logout(self) -> list[tuple[int, str]]:
        return self._header("5")

    def new_order_single(self) -> list[tuple[int, str]]:
        ord_type = self.r.choice(["1", "2"])    # 1=Market, 2=Limit
        tif = self.r.choice(["0", "1", "3"])     # Day / GTC / IOC
        side = self.r.choice(["1", "2"])
        fields = self._header("D") + [
            (11, self._cl_ord_id()),
            (21, "1"),
            (55, self._symbol()),
            (54, side),
            (60, self._ts()),
            (38, str(self._qty())),
            (40, ord_type),
        ]
        if ord_type == "2":
            fields.append((44, self._price()))
        fields.append((59, tif))
        return fields

    def new_order_single_with_allocs(self) -> list[tuple[int, str]]:
        ord_type = self.r.choice(["1", "2"])
        tif = self.r.choice(["0", "1", "3"])
        side = self.r.choice(["1", "2"])
        qty = self._qty()
        fields = self._header("D") + [
            (11, self._cl_ord_id()),
            (21, "1"),
            (55, self._symbol()),
            (54, side),
            (60, self._ts()),
            (38, str(qty)),
            (40, ord_type),
        ]
        if ord_type == "2":
            fields.append((44, self._price()))
        fields.append((59, tif))

        # NoAllocs repeating group — tests group-depth on the hot path.
        n = self.r.randint(2, 5)
        fields.append((78, str(n)))
        base = qty // n
        for i in range(n):
            share = qty - base * (n - 1) if i == n - 1 else base
            fields.append((79, f"ACCT{i:02d}"))
            fields.append((80, str(share)))
        return fields

    def execution_report(self) -> list[tuple[int, str]]:
        exec_type = self.r.choices(
            ["F", "0", "4", "8"],           # trade, new, cancelled, rejected
            weights=[0.70, 0.15, 0.10, 0.05],
        )[0]
        ord_status_map = {"F": "2", "0": "0", "4": "4", "8": "8"}
        side = self.r.choice(["1", "2"])
        qty = self._qty()
        px = self._price()
        fields = self._header("8") + [
            (37, self._order_id()),
            (17, self._exec_id()),
            (150, exec_type),
            (39, ord_status_map[exec_type]),
            (55, self._symbol()),
            (54, side),
            (38, str(qty)),
        ]
        if exec_type == "F":
            last_qty = self.r.randint(1, qty)
            fields += [
                (32, str(last_qty)),
                (31, px),
                (151, str(qty - last_qty)),
                (14, str(last_qty)),
                (6, px),
            ]
        else:
            fields += [(151, str(qty)), (14, "0"), (6, "0")]
        # ~15% of execution reports carry a free-text note (Text, tag 58).
        # Short-to-medium length — pushes value-side SOH distance past the
        # typical 8-20 bytes into the AVX2 sweet spot.
        if self.r.random() < 0.15:
            fields.append((58, self._text(4, 14)))
        return fields

    def order_cancel_request(self) -> list[tuple[int, str]]:
        return self._header("F") + [
            (41, self._cl_ord_id()),
            (11, self._cl_ord_id()),
            (55, self._symbol()),
            (54, self.r.choice(["1", "2"])),
            (60, self._ts()),
        ]

    def market_data_snapshot(self) -> list[tuple[int, str]]:
        # NoMDEntries group — the other place group-depth matters.
        n = self.r.randint(2, 10)
        fields = self._header("W") + [
            (262, self._cl_ord_id()),
            (55, self._symbol()),
            (268, str(n)),
        ]
        for _ in range(n):
            fields += [
                (269, self.r.choice(["0", "1"])),   # MDEntryType (bid/offer)
                (270, self._price()),               # MDEntryPx
                (271, str(self._qty())),            # MDEntrySize
            ]
        return fields

    def market_data_incremental(self) -> list[tuple[int, str]]:
        # Deeper-book bursts: up to 25 entries per message. Pushes the
        # per-message body well past a single 32-byte SIMD window.
        n = self.r.randint(8, 25)
        fields = self._header("X") + [(268, str(n))]
        for _ in range(n):
            fields += [
                (279, self.r.choice(["0", "1", "2"])),  # MDUpdateAction
                (269, self.r.choice(["0", "1"])),
                (55, self._symbol()),
                (270, self._price()),
                (271, str(self._qty())),
            ]
        return fields

    def news(self) -> list[tuple[int, str]]:
        # News messages (35=B) carry long free-text fields — Headline (148),
        # Subject (147), and a LinesOfText group (tag 33 / tag 58 per line).
        # Value lengths here routinely span 50-300 bytes, which is the
        # regime where the AVX2 bulk SOH pass dominates scalar.
        n_lines = self.r.randint(1, 6)
        fields = self._header("B") + [
            (148, self._text(4, 16)),                 # Headline
            (147, self._text(8, 24)),                 # Subject
            (33, str(n_lines)),                       # LinesOfText count
        ]
        for _ in range(n_lines):
            fields.append((58, self._text(6, 40)))    # Text
        return fields

    # ---- mixer -----------------------------------------------------------

    def generate_one(self) -> tuple[str, bytes]:
        names = [b[0] for b in self.BUILDERS]
        weights = [b[1] for b in self.BUILDERS]
        method_name = self.r.choices(names, weights=weights)[0]
        tags = getattr(self, method_name)()
        return method_name, render(tags)


def write_bulk(out_path: Path, count: int, seed: int, dry_run: bool) -> None:
    """Concatenate `count` generated frames into a single binary blob.

    FIX on the wire is a continuous byte stream — there are no per-message
    file boundaries, messages are demarcated solely by 8=.../9=<n>/10=cksm.
    We mirror that here so FIX::Parser can exercise its stream-splitting
    path rather than being handed pre-split inputs.
    """
    gen = BulkGenerator(seed=seed)
    counts: dict[str, int] = {}

    # Build in memory first so a partial write can't leave a torn file.
    chunks: list[bytes] = []
    for _ in range(count):
        kind, frame = gen.generate_one()
        counts[kind] = counts.get(kind, 0) + 1
        chunks.append(frame)

    total_bytes = sum(len(c) for c in chunks)

    if dry_run:
        # Show the first few frames so a human can eyeball the structure.
        for i, frame in enumerate(chunks[:3], 1):
            print(f"  #{i}: {frame.decode('ascii').replace(SOH, '|')}")
    else:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        with out_path.open("wb") as f:
            for frame in chunks:
                f.write(frame)

    action = "would write" if dry_run else "wrote"
    print(f"\n{action} {count} messages to {out_path} "
          f"(seed={seed}, {total_bytes:,} total bytes, single stream)")
    for kind, c in sorted(counts.items(), key=lambda kv: -kv[1]):
        pct = 100.0 * c / count
        print(f"  {c:>6}  {pct:5.1f}%  {kind}")


def main(argv: list[str]) -> int:
    here = Path(__file__).parent
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dry-run", action="store_true",
                    help="print what would be written, don't touch disk")
    ap.add_argument("--out-dir", default=str(here / "valid"),
                    help="canonical corpus directory (default: corpus/valid)")
    ap.add_argument("--bulk", type=int, default=0, metavar="N",
                    help="generate N bulk messages (default: 0 = skip)")
    ap.add_argument("--bulk-stream", default=str(here / "bulk.stream"),
                    help="bulk stream file path (default: corpus/bulk.stream)")
    ap.add_argument("--seed", type=int, default=0xC0FFEE,
                    help="RNG seed for bulk mode (default: 0xC0FFEE)")
    ap.add_argument("--canonical", action="store_true",
                    help="always regenerate canonical corpus (implicit if "
                         "--bulk is not given)")
    args = ap.parse_args(argv)

    run_canonical = args.canonical or args.bulk == 0
    run_bulk = args.bulk > 0

    if run_canonical:
        write_canonical(Path(args.out_dir), args.dry_run)
    if run_bulk:
        write_bulk(Path(args.bulk_stream), args.bulk, args.seed, args.dry_run)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
