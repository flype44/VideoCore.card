#ifndef _CHIP_H
#define _CHIP_H

#include <common/compiler.h>

#include "boardinfo.h"
#include "videocore.h"

/* The numbers of a panning which the words of the planes are made of */
struct Panning {
    int   Unity;                        /* the screen has the size of the display: no scaling */
    RGBFTYPE Format;
    ULONG Address;                      /* address of the first pixel to show */
    ULONG BytesPerRow;
    ULONG Scale;                        /* scale of the screen on the display, 16.16 */
    ULONG OffsetX;                      /* position of the screen on the display */
    ULONG OffsetY;
    ULONG Width;                        /* size of the screen on the display */
    ULONG Height;
    ULONG Kernel;                       /* offset of the scaling kernel in the display list memory */
    ULONG SpriteWidth;                  /* size of the sprite plane on the display */
    ULONG SpriteHeight;
    ULONG SpriteX;                      /* position of the sprite plane on the display */
    ULONG SpriteY;
    ULONG SpriteKernel;                 /* offset of the scaling kernel of the sprite in the display list memory */
};

/* What tells a family of the VideoCore from the other one: the numbers of its display lists and the functions which
   write their words. WritePlane and WriteSprite return the index of the word after what they wrote. */
struct ChipFamily {
    const char *Name;                   /* VC4 or VC6 */
    APTR        DisplayList;            /* the display list memory of the HVS */
    ULONG       UnityPlaneWords;        /* words of a native screen: main plane, sprite plane, palette */
    ULONG       ScaledPlaneWords;       /* words of a scaled screen: scaler and kernel words in both planes */
    UWORD       UnityAddressWord;       /* index of the pixel address in the main plane of a native screen */
    UWORD       ScaledAddressWord;      /* the same in the main plane of a scaled screen */
    ULONG       UnityScale;             /* the scale left in the planes of a native screen */
    void      (*ConstructUnicamDL)(struct VideoCoreBase *VideoCoreBase);
    int       (*WritePlane)(struct BoardInfo *b, const struct Panning *pan, int pos);
    int       (*WriteSprite)(struct BoardInfo *b, const struct Panning *pan, int cnt);
    APTR        SetSprite;
    APTR        SetSpritePosition;
};

/* Gives the BoardInfo the functions both families share and then those of the family */
void Chip_Init(struct BoardInfo *bi, const struct ChipFamily *family);

/* For the card: the DPMS levels through the display power of the firmware */
void Chip_SetDPMSLevel(REGARG(struct BoardInfo *b, "a0"), REGARG(ULONG level, "d0"));

/* Writes a word of the planes which is not the position of the sprite, in both copies when there are two */
void Chip_Poke(struct VideoCoreBase *VideoCoreBase, volatile uint32_t *word, ULONG value);

/* The families need this one for the display lists they write */
UWORD Chip_CalculateBytesPerRow(REGARG(struct BoardInfo *b, "a0"), REGARG(UWORD width, "d0"), REGARG(RGBFTYPE format, "d7"));

#endif /* _CHIP_H */
