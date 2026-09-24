#!/usr/bin/env python3
"""Prints where a frame's heap allocations come from.

Reads the benchmark's output (the BENCHMARK_RESULT line printed by
`wolfenstein --benchmark N`) from stdin and shows allocations and bytes per
frame for each profiler section, nested as the sections are in the code.

    ./scripts/dev.sh ./build/native-release/bin/wolfenstein --benchmark 600 \
        | ./scripts/alloc_breakdown.py
"""

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
    ("camera", 2),
    ("render", 1),
    ("render_walls", 2),
    ("render_objects", 2),
    ("render_draw", 2),
    ("render_hud", 2),
    ("present", 1),
]


def main():
    report = None
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


if __name__ == "__main__":
    main()
