#ifndef _CHIP_H
#define _CHIP_H

#include <common/compiler.h>

#include "boardinfo.h"

/* The functions of the BoardInfo which both families share */
void Chip_Init(struct BoardInfo *bi);

/* For the card: the DPMS levels through the display power of the firmware */
void Chip_SetDPMSLevel(REGARG(struct BoardInfo *b, "a0"), REGARG(ULONG level, "d0"));

/* The families need this one for the display lists they write */
UWORD Chip_CalculateBytesPerRow(REGARG(struct BoardInfo *b, "a0"), REGARG(UWORD width, "d0"), REGARG(RGBFTYPE format, "d7"));

#endif /* _CHIP_H */
