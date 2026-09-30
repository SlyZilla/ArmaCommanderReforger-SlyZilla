#!/bin/bash
# Claude's helper (runs in the Cowork VM, not on Windows):
#   bash tools/runtest.sh <unique id> <preset> <max polls of 5 s>
# Drops a request for the test launcher (tools/ACR-Watcher.ps1) and prints the
# result header plus script errors. Use a new id every time (old results stay).
cd "$(dirname "$0")/../.acr-requests" || exit 1
echo $2 > $1.request
for i in $(seq 1 $3); do sleep 5; [ -f $1.result.txt ] && break; done
head -6 $1.result.txt 2>/dev/null
grep "(E)" $1.result.txt 2>/dev/null | grep -v "SCR_SpawnLogic\|SCR_MapUIElementContainer\|SCR_PlayerArsenalLoadout\|ScenarioFramework\|SpinningWidget\|SCR_ScenarioUICommon\|Can't compile" | head -20
