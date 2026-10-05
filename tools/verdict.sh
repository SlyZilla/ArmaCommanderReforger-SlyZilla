#!/bin/bash
# Claude's helper: the verdict of a finished test request (R11, 5 Oct review).
#   bash tools/verdict.sh <id>
# PASS only when the launcher says completed AND the run printed "[ACR] VERDICT PASS".
# A run with only the legacy TEST COMPLETE marker is "NO VERDICT", never a pass.
cd "$(dirname "$0")/../.acr-requests" || exit 1
r="$1.result.txt"
[ -f "$r" ] || { echo "VERDICT MISSING: no result for $1"; exit 2; }
status=$(sed '1s/^\xEF\xBB\xBF//' "$r" | grep -m1 '^status=' | cut -d= -f2- | tr -d '\r')
build=$(cat "$1.build.txt" 2>/dev/null || echo "unknown build")
line=$(grep -m1 '\[ACR\] VERDICT' "$r" | sed 's/.*\[ACR\] VERDICT //' | tr -d '\r')
fails=$(grep -c 'REGRESS FAIL' "$r")
passes=$(grep -c 'REGRESS PASS' "$r")
if [ "$status" != "completed" ]; then v="FAIL (launcher: $status)"
elif [ -z "$line" ]; then v="NO VERDICT (legacy test: TEST COMPLETE only)"
else v="$line"; fi
echo "RUN $1 | $(grep -m1 '^preset=' "$r") | build $build | REGRESS lines: $passes pass, $fails fail"
echo "VERDICT $v"
grep 'REGRESS FAIL\|REGRESS SKIP' "$r" | head -20
case "$v" in PASS\ *) exit 0;; *) exit 1;; esac
