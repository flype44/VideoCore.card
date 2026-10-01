#ifndef _CHIP_H
#define _CHIP_H

#include <common/compiler.h>

#include "boardinfo.h"
#include "videocore.h"

/* What tells a family of the VideoCore from the other one. The functions are the ones of the BoardInfo which write
   the words of the display lists of the family. */
struct ChipFamily {
    const char *Name;                   /* VC4 or VC6 */
    APTR        DisplayList;            /* the display list memory of the HVS */
    ULONG       UnityPlaneWords;        /* words of a native screen: main plane, sprite plane, palette */
    ULONG       ScaledPlaneWords;       /* words of a scaled screen: scaler and kernel words in both planes */
    UWORD       UnityAddressWord;       /* index of the pixel address in the main plane of a native screen */
    UWORD       ScaledAddressWord;      /* the same in the main plane of a scaled screen */
    void      (*ConstructUnicamDL)(struct VideoCoreBase *VideoCoreBase);
    APTR        SetPanning;
    APTR        SetSprite;
    APTR        SetSpritePosition;
};

/* Gives the BoardInfo the functions both families share and then those of the family */
void Chip_Init(struct BoardInfo *bi, const struct ChipFamily *family);

/* For the card: the DPMS levels through the display power of the firmware */
void Chip_SetDPMSLevel(REGARG(struct BoardInfo *b, "a0"), REGARG(ULONG level, "d0"));

/* The families need this one for the display lists they write */
UWORD Chip_CalculateBytesPerRow(REGARG(struct BoardInfo *b, "a0"), REGARG(UWORD width, "d0"), REGARG(RGBFTYPE format, "d7"));

#endif /* _CHIP_H */
