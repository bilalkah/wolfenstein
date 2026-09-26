#!/bin/bash
# Runs clang-tidy over the first-party sources (4 jobs) using the native-debug
# build's compile_commands.json, writes every finding to
# build/tidy-report.txt and prints a count per check; exits with an error if
# there is any finding. Pass file paths to check only those files.
set -euo pipefail

cd "$(dirname "$0")/.."
[ -f build/native-debug/compile_commands.json ] || cmake --preset native-debug >/dev/null

if [ $# -gt 0 ]; then
	files=("$@")
else
	mapfile -t files < <(find app src tests -name '*.cpp')
fi

REPORT=build/tidy-report.txt
# stderr only carries clang-tidy's progress and "N warnings generated" counts,
# which include suppressed findings in system and third-party headers
run-clang-tidy -p build/native-debug -j 4 -quiet "${files[@]}" 2>/dev/null |
	grep -E '^/.*(warning|error):' | sort -u > "$REPORT" || true

echo "clang-tidy: $(wc -l < "$REPORT") findings (details in $REPORT)"
grep -oE '\[[a-z0-9.-]+(,[a-z0-9.-]+)*\]$' "$REPORT" | sort | uniq -c | sort -rn || true
# Any finding fails the run, so CI keeps the code at zero
if [ -s "$REPORT" ]; then
	cat "$REPORT"
	exit 1
fi
