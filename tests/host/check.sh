#!/bin/bash
# check.sh <source tree> : builds the host test on a tree and compares its output with expected/dlgolden.txt
SRC=${1:-/mnt/c/Developers/VideoCore.card}
T=/mnt/c/Developers/VideoCore.card/tests/host
bash $T/build.sh $SRC /tmp/dlgolden_check >/dev/null || exit 2
/tmp/dlgolden_check 2>/dev/null > /tmp/dlgolden_check.txt || exit 3
if cmp -s /tmp/dlgolden_check.txt $T/expected/dlgolden.txt; then
  echo "SAME as expected ($(wc -l < /tmp/dlgolden_check.txt) lines)"
else
  echo "DIFFERENT"; diff /tmp/dlgolden_check.txt $T/expected/dlgolden.txt | head -${2:-12}; exit 1
fi
