# Host test of the display lists

The chip files of the driver (`vc4.c`, `vc6.c`, `chip.c`, `buddyalloc.c`) are built **unchanged** for the host
and called through the `BoardInfo`, like Picasso96 does. The memory of the HVS (registers and display list memory)
is mapped at its real addresses, AmigaOS and the other modules are replaced by `hoststubs.c`.

`golden.c` runs a grid of cases for each family (display and screen sizes, pixel formats, integer scaler, hardware
sprite, unicam, panning and bitmap changes) and prints the words of the display list and the registers they leave.

- `expected/dlgolden.txt`: the output of the code before the refactoring of `SetPanning`.
- `check.sh [tree]`: builds the test on a source tree (the repository by default, or a `git archive` of a commit) and
  compares its output with `expected/dlgolden.txt`. `SAME` means the HVS gets the same words.
- `mutate.sh`: changes one word of the VC6 display list in a copy of the tree to show that the test notices it.

Run from WSL (the tools are Linux ones): `bash /mnt/c/Developers/VideoCore.card/tests/host/check.sh`. It needs gcc, the
NDK headers of `m68k-amigaos-gcc` (`~/amiga-gcc/m68k-amigaos/ndk-include`) and the Picasso96 headers.

A screen bigger than the display with the integer scaler divides by zero in `SetPanning`
(`0x10000 / (0x10000 / scale)` with `scale > 0x10000`): the grid leaves these cases out.
