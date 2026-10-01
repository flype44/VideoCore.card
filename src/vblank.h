#ifndef _VBLANK_H
#define _VBLANK_H

#include <common/compiler.h>

#include "boardinfo.h"

/*
    A vertical blank interrupt for Picasso96 (BIF_VBLANKINTERRUPT, SetInterrupt, HardInterrupt), taken from the
    pixelvalve of HDMI0 through the GIC-400 and gic400.library. With it rtg.library lets WaitBOVP(), WaitTOF()
    and the double buffering sleep until the blank instead of calling WaitVerticalSync(), which spins.

    Returns TRUE when the interrupt is set up. Anything missing (VC4, no gic400.library, a pixelvalve or an
    interrupt in use by someone else, ...) leaves the BoardInfo as it is and returns FALSE.
*/
BOOL VBlank_Init(struct BoardInfo *bi);

/*
    Writes the position word of the sprite plane (the first word of vc_MouseCoord). The HVS draws a shifted copy of
    the sprite for a frame when that word changes in the middle of one, so with the interrupt the word waits for
    the next vertical blank, written by the handler (the latest one wins). Without the interrupt it is written at once.
*/
void VBlank_WriteSprite(struct VideoCoreBase *VideoCoreBase, ULONG word);

/* Takes the interrupt out again: the source is switched off, the handler removed, gic400.library closed. Does
   nothing when VBlank_Init() did not set it up. */
void VBlank_Exit(struct VideoCoreBase *VideoCoreBase);

#endif /* _VBLANK_H */
