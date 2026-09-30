#!/usr/bin/env python3
"""Prints where a frame's heap allocations come from.

Reads the benchmark's output (the BENCHMARK_RESULT line printed by
`karakale --benchmark N`) from stdin and shows allocations and bytes per
frame for each profiler section, nested as the sections are in the code.

    ./scripts/dev.sh ./build/native-release/bin/karakale --benchmark 600 \
        | ./scripts/alloc_breakdown.py

With --report it reads a saved report instead (the JSON files the web
benchmark writes to docs/benchmarks/results/).

With --require-zero it exits with an error if any frame after the warmup
allocated, which CI uses to keep the steady-state frame allocation-free.
"""

import argparse
import json
import sys

# Section nesting, mirroring the ScopedTimers in the code: a parent's numbers
# include its children's
TREE = [
    ("frame", 0),
    ("update_enemies", 1),
    ("pathfinding", 2),
    ("line_of_sight", 2),
    ("update_player", 1),
    ("camera", 1),
    ("render", 1),
    ("render_walls", 2),
    ("render_objects", 2),
    ("render_draw", 2),
    ("render_hud", 2),
    ("present", 1),
]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--require-zero", action="store_true",
                        help="fail if any measured frame allocated")
    parser.add_argument("--report", metavar="FILE",
                        help="read a saved JSON report instead of stdin")
    args = parser.parse_args()

    report = None
    if args.report:
        with open(args.report) as report_file:
            report = json.load(report_file)
    else:
        for line in sys.stdin:
            if line.startswith("BENCHMARK_RESULT "):
                report = json.loads(line[len("BENCHMARK_RESULT "):])
    if report is None:
        sys.exit("no BENCHMARK_RESULT line on stdin")

    allocations = report["section_allocations"]
    allocated_bytes = report["section_allocated_bytes"]
    total = allocations["frame"]["mean"] or 1.0

    print(f"{report['frames']} frames measured (after {report['warmup_frames']} warmup)")
    print(f"{'section':<22}{'allocs/frame':>14}{'bytes/frame':>14}{'share':>8}")
    for name, depth in TREE:
        count = allocations[name]["mean"]
        size = allocated_bytes[name]["mean"]
        label = "  " * depth + name
        print(f"{label:<22}{count:>14.1f}{size:>14.0f}{count / total:>8.0%}")
    other = report["unattributed_allocations"]["mean"]
    print(f"{'  (outside sections)':<22}{other:>14.1f}{'':>14}{other / total:>8.0%}")

    if args.require_zero:
        samples = report["samples"]["allocations"]
        allocating = [i for i, count in enumerate(samples) if count > 0]
        if allocating:
            sys.exit(f"{len(allocating)} of {len(samples)} frames allocated "
                     f"(first at measured frame {allocating[0]}); "
                     "the steady-state frame must not touch the heap")
        print(f"OK: none of {len(samples)} frames allocated")


if __name__ == "__main__":
    main()
