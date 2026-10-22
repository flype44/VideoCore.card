#ifndef _VBLANK_H
#define _VBLANK_H

#include <exec/types.h>
#include "boardinfo.h"
#include "emu68-vc4.h"

/*
    Vertical blank interrupt for Picasso96 (BIF_VBLANKINTERRUPT, SetInterrupt, HardInterrupt), taken from the
    pixelvalve of HDMI0 through the GIC-400 and gic400.library. With it rtg.library lets WaitBOVP(), WaitTOF()
    and the double buffering sleep until the blank instead of calling WaitVerticalSync(), which spins.

    VBlank_Init() returns TRUE when the interrupt is set up. Anything missing (VC4, no gic400.library or a too
    old one, no interrupt in the device tree, no interrupt seen) leaves the BoardInfo as it is and returns FALSE.
*/
BOOL VBlank_Init(struct BoardInfo *bi);
void VBlank_Exit(struct VC4Base *VC4Base);

#endif /* _VBLANK_H */
