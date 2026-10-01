#!/bin/bash
# Builds the host test: the real chip sources, a few stubs, and the driver of the test.
# usage: build.sh <source tree> <output binary>     (the tree is the repository or a git archive of a commit)
set -e
SRC=${1:-/mnt/c/Developers/VideoCore.card}
OUT=${2:-/tmp/dlgolden}
TESTS=/mnt/c/Developers/VideoCore.card/tests/host
ND=$HOME/amiga-gcc/m68k-amigaos/ndk-include
P96=/mnt/c/Developers/VideoCore.card/build/Picasso96Develop-ICOMP/Picasso96DevelopPublic/Include
INC="-include $TESTS/prelude.h -I$TESTS/stubs -I$SRC/include -I$SRC/src -I$SRC/unicam.resource/include_pub -I$P96 -I$ND"
W="-Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function"
mkdir -p /tmp/dlg.$$
for f in vc4 vc6 chip buddyalloc; do
  gcc -O1 -g -c $W $INC $SRC/src/$f.c -o /tmp/dlg.$$/$f.o
done
gcc -O1 -g -c $W $INC $TESTS/hoststubs.c -o /tmp/dlg.$$/hoststubs.o
gcc -O1 -g -c $W $INC $TESTS/golden.c -o /tmp/dlg.$$/golden.o
gcc -O1 -g -c $W -I$SRC/src $TESTS/hostos.c -o /tmp/dlg.$$/hostos.o
gcc -o $OUT /tmp/dlg.$$/*.o
rm -rf /tmp/dlg.$$
echo "built $OUT"
