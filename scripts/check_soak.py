#!/usr/bin/env python3
"""Checks a soak session's report (the SOAK_RESULT line of `karakale
--soak` or benchmarks/run_web_soak.mjs), read from stdin.

Fails unless nothing was allocated after startup and the session really went
where the script sends it: through a pickup, a door, a secret, the story,
a level's results and the next one's briefing, a level transition and a
player's death.
"""

import json
import sys


def main():
    report = None
    for line in sys.stdin:
        if line.startswith("SOAK_RESULT "):
            report = json.loads(line[len("SOAK_RESULT "):])
    if report is None:
        sys.exit("no SOAK_RESULT line on stdin")

    for phase, count in report["phases"].items():
        print(f"{phase:<18}{count:>8} allocations")
    failures = []
    if report["allocations"] != 0:
        failures.append(f"{report['allocations']} allocations "
                        f"({report['bytes']} bytes) after startup")
    if report["max_level"] < 2:
        failures.append("the session never reached the second level")
    if not report["saw_result"]:
        failures.append("the session never reached the result screen")
    if not report.get("took_pickup"):
        failures.append("the session never took a pickup")
    if not report.get("opened_door"):
        failures.append("the session never opened a door")
    if not report.get("saw_stats"):
        failures.append("the session never showed a level's results")
    if not report.get("found_secret"):
        failures.append("the session never pushed a secret")
    if not report.get("saw_briefing"):
        failures.append("the session never showed a briefing")
    if not report.get("saw_story"):
        failures.append("the session never told a page of story")
    if failures:
        sys.exit("soak failed: " + "; ".join(failures))
    print(f"OK: {report['frames']} frames, no allocation after startup")


if __name__ == "__main__":
    main()
