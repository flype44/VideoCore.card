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

#endif /* _VBLANK_H */
