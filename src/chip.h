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

/* A memory window (the PiP of a program, see memory-window.c) as a plane of the display list, in pixels of the display */
struct WindowPlane {
    ULONG       Address;                /* pixel memory of the source bitmap */
    ULONG       BytesPerRow;
    RGBFTYPE    Format;
    WORD        X;                      /* position on the display */
    WORD        Y;
    UWORD       Width;                  /* size on the display, the size of the source */
    UWORD       Height;
    UWORD       Alpha;                  /* opacity over what is under it, 12 bits: 0xfff is opaque */
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
    ULONG       WindowPlaneWords;       /* words of the plane of a memory window, 0 if the family has none */
    int       (*WriteWindow)(struct BoardInfo *b, const struct WindowPlane *window, int pos);
    ULONG     (*WindowPosition)(const struct WindowPlane *window);   /* the position word of that plane */
    ULONG     (*WindowAlpha)(const struct WindowPlane *window);      /* its alpha word, the one after the position */
};

/* Gives the BoardInfo the functions both families share and then those of the family */
void Chip_Init(struct BoardInfo *bi, const struct ChipFamily *family);

/* Writes the planes of the screen again, with the memory window as it is now: after a change of the window which
   its position word cannot show (it appears, disappears, changes size, format or memory) */
void Chip_RebuildPlanes(struct BoardInfo *b);

/* Writes the position and the opacity of the plane of the memory window which is in the planes; the window moved or
   became more or less transparent, or the panning changed */
void Chip_UpdateWindowPlane(struct VideoCoreBase *VideoCoreBase, const struct WindowPlane *window);

/* For the card: the DPMS levels through the display power of the firmware */
void Chip_SetDPMSLevel(REGARG(struct BoardInfo *b, "a0"), REGARG(ULONG level, "d0"));

/* Writes a word of the planes which is not the position of the sprite, in both copies when there are two */
void Chip_Poke(struct VideoCoreBase *VideoCoreBase, volatile uint32_t *word, ULONG value);

/* The families need this one for the display lists they write */
UWORD Chip_CalculateBytesPerRow(REGARG(struct BoardInfo *b, "a0"), REGARG(UWORD width, "d0"), REGARG(RGBFTYPE format, "d7"));

#endif /* _CHIP_H */
