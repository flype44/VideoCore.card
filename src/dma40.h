#ifndef _DMA40_H
#define _DMA40_H

#include "emu68-vc4.h"

/* Starts the DMA40 channel and proves it with a self test; leaves vc4_DMA.Ok FALSE (and the driver on the VPU and the
   CPU) if anything is missing */
void DMA40_Init(struct VC4Base *VC4Base);

/* Flags of DMA40_CopyRect() */
#define DMA40F_MOVE         (1UL << 0)  /* the areas may overlap (a scroll inside one bitmap): the pitches must be equal */
#define DMA40F_SRC_CACHED   (1UL << 1)  /* the source is in RAM that the CPU caches (a bitmap in RAM, not the RTG memory) */
#define DMA40F_DST_CACHED   (1UL << 2)  /* the same for the destination */

/* Copies the rectangle of `width` bytes and `height` rows from the physical address `src` (rows `srcpitch` bytes apart)
   to `dst` (rows `dstpitch` apart). Without DMA40F_MOVE the two areas must not overlap at all (two different bitmaps).
   Returns FALSE and does nothing when the DMA cannot or should not take it (the caller then uses the VPU or the CPU);
   TRUE when the transfer is running: it is over only after DMA40_Wait() */
BOOL DMA40_CopyRect(struct VC4Base *VC4Base, ULONG src, ULONG dst, ULONG width, ULONG height,
                    ULONG srcpitch, ULONG dstpitch, ULONG flags);

/* Fills the rectangle of `width` bytes and `height` rows at the physical address `dst` (rows `dstpitch` apart) with the
   32 bit `value` repeated (a pixel of a 32 bit format, in the byte order of the memory: the long word as the 68k writes it).
   The memory is the RTG memory (not cached by the CPU). Same answers as DMA40_CopyRect() */
BOOL DMA40_FillRect(struct VC4Base *VC4Base, ULONG dst, ULONG width, ULONG height, ULONG dstpitch, ULONG value);

/* Waits for the running transfer: sleeps until the interrupt of the channel, does nothing when none runs */
void DMA40_Wait(struct VC4Base *VC4Base);

#endif /* _DMA40_H */
