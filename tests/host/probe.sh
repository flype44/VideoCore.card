#!/bin/bash
cd /mnt/c/Developers/VideoCore.card
ND=$HOME/amiga-gcc/m68k-amigaos/ndk-include
for f in vc4 vc6; do
  echo "=== $f"
  gcc -fsyntax-only -include tests/host/prelude.h -Itests/host/stubs -Iinclude -Isrc -Ibuild/Picasso96Develop-ICOMP/Picasso96DevelopPublic/Include -I$ND src/$f.c 2>&1 | head -25
done
