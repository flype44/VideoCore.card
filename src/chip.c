/*
    What the two families of the VideoCore, VC4 and VC6, do the same way: the functions of the BoardInfo
    which are not about the display list words of a family. Chip_Init() gives them to the BoardInfo, the
    InitChip of the family then sets its own ones.
*/

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>

#include <proto/exec.h>
#include <proto/mathieeesingbas.h>
#include <proto/unicam.h>

#include <resources/unicam.h>
#include <hardware/cia.h>

#include <common/compiler.h>

/* Make sure MathIEEE will not force gcc to do weird FLOT convertions when calling lib functions */
#define FLOAT ULONG

#include "videocore.h"
#include "boardinfo.h"
#include "mbox.h"
#include "buddyalloc.h"
#include "hvs.h"
#include "chip.h"

UWORD Chip_CalculateBytesPerRow(REGARG(struct BoardInfo *b, "a0"), REGARG(UWORD width, "d0"), REGARG(RGBFTYPE format, "d7"))
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;
    struct ExecBase *SysBase = VideoCoreBase->vc_LibNode.ExecBase;

    if (!b)
        return 0;

    UWORD pitch = width;

    if (0)
    {
        bug("[VC] CalculateBytesPerRow pitch %ld, format %lx\n", pitch, format);
    }
    

    switch(format) {
        case RGBFB_CLUT:
            return pitch;
        default:
            return 128;
        case RGBFB_R5G6B5PC: case RGBFB_R5G5B5PC:
        case RGBFB_R5G6B5: case RGBFB_R5G5B5:
        case RGBFB_B5G6R5PC: case RGBFB_B5G5R5PC:
            return (width * 2);
        case RGBFB_R8G8B8: case RGBFB_B8G8R8:
            // Should actually return width * 3, but I'm not sure if
            // the Pi VC supports 24-bit color formats.
            // P96 will sometimes magically pad these to 32-bit anyway.
            return (width * 3);
        case RGBFB_B8G8R8A8: case RGBFB_R8G8B8A8:
        case RGBFB_A8B8G8R8: case RGBFB_A8R8G8B8:
            return (width * 4);
    }
}

static APTR Chip_CalculateMemory(REGARG(struct BoardInfo *b, "a0"), REGARG(unsigned long addr, "a1"), REGARG(RGBFTYPE format, "d7"))
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;
    struct ExecBase *SysBase = VideoCoreBase->vc_LibNode.ExecBase;

    if (0)
    {
        bug("[VC] CalculateMemory %lx %lx\n", addr, format);
    }

    return (APTR)addr;
}

static ULONG Chip_GetCompatibleFormats(REGARG(struct BoardInfo *b, "a0"), REGARG(RGBFTYPE format, "d7"))
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;
    struct ExecBase *SysBase = VideoCoreBase->vc_LibNode.ExecBase;
    if (0)
    {
        bug("[VC] GetCompatibleFormats %lx\n", format);
    }
    return 0xFFFFFFFF;
}

static ULONG Chip_GetPixelClock(REGARG(struct BoardInfo *b, "a0"), REGARG(struct ModeInfo *mode_info, "a1"),
                    REGARG(ULONG index, "d0"), REGARG(RGBFTYPE format, "d7"))
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;
    
    ULONG clock = mode_info->HorTotal * mode_info->VerTotal * VideoCoreBase->vc_VertFreq;

    if (b->ModeInfo->Flags & GMF_DOUBLESCAN)
        clock <<= 1;

    return clock;
}

static ULONG Chip_GetVBeamPos(REGARG(struct BoardInfo *b, "a0"))
{
    volatile ULONG *stat = (ULONG*)(HVS_BASE + SCALER_DISPSTAT1);
    ULONG vbeampos = LE32(*stat) & 0xfff;

    return vbeampos;
}

static LONG Chip_ResolvePixelClock(REGARG(struct BoardInfo *b, "a0"), REGARG(struct ModeInfo *mode_info, "a1"),
                       REGARG(ULONG pixel_clock, "d0"), REGARG(RGBFTYPE format, "d7"))
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;
    struct ExecBase *SysBase = VideoCoreBase->vc_LibNode.ExecBase;
    
    if (0)
    {
        bug("[VC] ResolvePixelClock %lx %ld %lx\n", mode_info, pixel_clock, format);
    }

    ULONG clock = mode_info->HorTotal * mode_info->VerTotal * VideoCoreBase->vc_VertFreq;

    if (b->ModeInfo->Flags & GMF_DOUBLESCAN)
        clock <<= 1;

    mode_info->PixelClock = clock;
    mode_info->pll1.Clock = 0;
    mode_info->pll2.ClockDivide = 1;

    return 0;
}

static void Chip_SetClearMask(REGARG(struct BoardInfo *b, "a0"), REGARG(UBYTE mask, "d0"))
{
}

static void Chip_SetClock(REGARG(struct BoardInfo *b, "a0"))
{
}

static void Chip_SetMemoryMode(REGARG(struct BoardInfo *b, "a0"), REGARG(RGBFTYPE format, "d7"))
{
}

static void Chip_SetReadPlane(REGARG(struct BoardInfo *b, "a0"), REGARG(UBYTE plane, "d0"))
{
}

static void Chip_SetWriteMask(REGARG(struct BoardInfo *b, "a0"), REGARG(UBYTE mask, "d0"))
{
}

static void Chip_SetDAC(REGARG(struct BoardInfo *b, "a0"), REGARG(RGBFTYPE format, "d7"))
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;
    struct ExecBase *SysBase = VideoCoreBase->vc_LibNode.ExecBase;
    
    if (0)
        bug("[VC] SetDAC\n");
    // Used to set the color format of the video card's RAMDAC.
    // This needs no handling, since the PiStorm doesn't really have a RAMDAC or a video card chipset.
}

static void Chip_SetGC(REGARG(struct BoardInfo *b, "a0"), REGARG(struct ModeInfo *mode_info, "a1"), REGARG(BOOL border, "d0"))
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;
    struct ExecBase *SysBase = VideoCoreBase->vc_LibNode.ExecBase;
    struct Size dim;
    int need_switch = 0;


    if (b->ModeInfo != mode_info) {
        need_switch = 1;
        b->ModeInfo = mode_info;
    }

    dim.width = mode_info->Width;
    dim.height = mode_info->Height;
    
    if (0)
    {
        bug("[VC] SetGC %ld x %ld x %ld\n", dim.width, dim.height, mode_info->Depth);
    }

    if (need_switch) {
        VideoCoreBase->vc_LastPanning.lp_Addr = NULL;
        //init_display(dim, mode_info->Depth, &VideoCoreBase->vc_Framebuffer, &VideoCoreBase->vc_Pitch, VideoCoreBase);
    }
}

static void Chip_WaitVerticalSync(REGARG(struct BoardInfo *b, "a0"), REGARG(BOOL toggle, "d0"))
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;
    volatile ULONG *stat = (ULONG*)(HVS_BASE + SCALER_DISPSTAT1);

    // Wait until current vbeampos is lower than the one obtained above
    do { asm volatile("nop"); } while((LE32(*stat) & 0xfff) != VideoCoreBase->vc_DispSize.height);
}

static void Chip_SetSpriteImage(REGARG(struct BoardInfo *b, "a0"), REGARG(RGBFTYPE format, "d7"))
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;
    struct ExecBase *SysBase = *(struct ExecBase **)4;

    for (int i=0; i < MAXSPRITEWIDTH * MAXSPRITEHEIGHT; i++)
        VideoCoreBase->vc_SpriteShape[i] = 0;

    if ((b->Flags & (BIF_HIRESSPRITE | BIF_BIGSPRITE)) == 0)
    {
        UWORD *data = b->MouseImage;
        data += 2;

        for (int y=0; y < b->MouseHeight; y++) {
            UWORD p0 = *data++;
            UWORD p1 = *data++;
            UWORD mask = 0x8000;
            for (int x=0; x < 16; x++) {
                UBYTE pix = 0;
                if (p0 & mask) pix |= 1;
                if (p1 & mask) pix |= 2;
                VideoCoreBase->vc_SpriteShape[y * MAXSPRITEWIDTH + x] = pix;
                mask = mask >> 1;
            }
        }
    }
    else if (b->Flags & BIF_BIGSPRITE) {
        UWORD *data = b->MouseImage;
        data += 2;

        for (int y=0; y < b->MouseHeight; y++) {
            UWORD p0 = *data++;
            UWORD p1 = *data++;
            UWORD mask = 0x8000;
            for (int x=0; x < 16; x++) {
                UBYTE pix = 0;
                if (p0 & mask) pix |= 1;
                if (p1 & mask) pix |= 2;
                VideoCoreBase->vc_SpriteShape[2 * y * MAXSPRITEWIDTH + 2*x] = pix;
                VideoCoreBase->vc_SpriteShape[2 * y * MAXSPRITEWIDTH + 2*x + 1] = pix;
                VideoCoreBase->vc_SpriteShape[(2 * y + 1) * MAXSPRITEWIDTH + 2*x] = pix;
                VideoCoreBase->vc_SpriteShape[(2 * y + 1) * MAXSPRITEWIDTH + 2*x + 1] = pix;
                mask = mask >> 1;
            }
        }
    }
    else if (b->Flags & BIF_HIRESSPRITE) {
        ULONG *data = (ULONG*)b->MouseImage;
        data += 2;

        for (int y=0; y < b->MouseHeight; y++) {
            ULONG p0 = *data++;
            ULONG p1 = *data++;
            ULONG mask = 0x80000000;
            for (int x=0; x < 32; x++) {
                UBYTE pix = 0;
                if (p0 & mask) pix |= 1;
                if (p1 & mask) pix |= 2;
                VideoCoreBase->vc_SpriteShape[y * MAXSPRITEWIDTH + x] = pix;
                mask = mask >> 1;
            }
        }
    }

    CacheClearE(VideoCoreBase->vc_SpriteShape, MAXSPRITEHEIGHT * MAXSPRITEWIDTH, CACRF_ClearD);
}

static void Chip_SetSpriteColor(REGARG(struct BoardInfo *b, "a0"), REGARG(UBYTE idx, "d0"),
                    REGARG(UBYTE R, "d1"), REGARG(UBYTE G, "d2"), REGARG(UBYTE B, "d3"),
                    REGARG(RGBFTYPE format, "d7"))
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;
    if (idx < 3) {
        VideoCoreBase->vc_SpriteColors[idx] = (VideoCoreBase->vc_SpriteAlpha << 24) | (R << 16) | (G << 8) | B;
        if (VideoCoreBase->vc_MousePalette) {
            wr32le(&VideoCoreBase->vc_MousePalette[idx], VideoCoreBase->vc_SpriteColors[idx]);
        }
    }
}

static UWORD Chip_SetSwitch(REGARG(struct BoardInfo *b, "a0"), REGARG(UWORD enabled, "d0"))
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;
    struct ExecBase *SysBase = VideoCoreBase->vc_LibNode.ExecBase;
    volatile ULONG *displist = (ULONG *)VideoCoreBase->vc_DisplayList;

    if (0)
    {
        bug("[VC] SetSwitch %ld\n", enabled);
    }

    if (VideoCoreBase->vc_Enabled != enabled) {
        VideoCoreBase->vc_Enabled = enabled;

        switch(enabled) {
            case 0:
                BlankScreen(1, VideoCoreBase);
                break;
            default:
                BlankScreen(0, VideoCoreBase);
                break;
        }
    }

    /* If switch mode is selected */
    if (VideoCoreBase->vc_SwitchMode != None)
    {
        UWORD en = enabled;

        /* Invert the switch mode */
        if (VideoCoreBase->vc_SwitchInverted)
        {
            en = 1 - en;
        }

        switch (VideoCoreBase->vc_SwitchMode)
        {
            case CTS:
                ((volatile struct CIA *)0xbfd000)->ciaddra |= CIAF_COMCTS;
                if (en) ((volatile struct CIA *)0xbfd000)->ciapra &= ~CIAF_COMCTS;
                else ((volatile struct CIA *)0xbfd000)->ciapra |= CIAF_COMCTS;
                break;

            case RTS:
                ((volatile struct CIA *)0xbfd000)->ciaddra |= CIAF_COMRTS;
                if (en) ((volatile struct CIA *)0xbfd000)->ciapra &= ~CIAF_COMRTS;
                else ((volatile struct CIA *)0xbfd000)->ciapra |= CIAF_COMRTS;
                break;

            case DTR:
                ((volatile struct CIA *)0xbfd000)->ciaddra |= CIAF_COMDTR;
                if (en) ((volatile struct CIA *)0xbfd000)->ciapra &= ~CIAF_COMDTR;
                else ((volatile struct CIA *)0xbfd000)->ciapra |= CIAF_COMDTR;
                break;

            case SEL:
                ((volatile struct CIA *)0xbfd000)->ciaddra |= CIAF_PRTRSEL;
                if (en) ((volatile struct CIA *)0xbfd000)->ciapra &= ~CIAF_PRTRSEL;
                else ((volatile struct CIA *)0xbfd000)->ciapra |= CIAF_PRTRSEL;
                break;
           case CSI:
                if (!en) {
                    VideoCoreBase->vc_UnicamVisible = TRUE;
                    wr32le((volatile uint32_t *)(HVS_BASE + SCALER_DISPLIST1), BUDDY_OFFSET(VideoCoreBase->vc_UnicamDL));
                }
                else {
                    VideoCoreBase->vc_UnicamVisible = FALSE;
                    wr32le((volatile uint32_t *)(HVS_BASE + SCALER_DISPLIST1), BUDDY_OFFSET(VideoCoreBase->vc_ActivePlane));
                }
                break;
        }
    }

    return 1 - enabled;
}

static void Chip_SetColorArray(REGARG(struct BoardInfo *b, "a0"), REGARG(UWORD start, "d0"), REGARG(UWORD num, "d1"))
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;
    struct ExecBase *SysBase = VideoCoreBase->vc_LibNode.ExecBase;
    volatile uint32_t *displist = (uint32_t *)VideoCoreBase->vc_DisplayList;

    // Sets the color components of X color components for 8-bit paletted display modes.
    if (!b->CLUT)
        return;
    
    if (0)
    {
        bug("[VC] SetColorArray %ld %ld\n", start, num);
    }

    int j = start + num;
    
    for(int i = start; i < j; i++) {
        unsigned long xrgb = 0xff000000 | (b->CLUT[i].Blue) | (b->CLUT[i].Green << 8) | (b->CLUT[i].Red << 16);
        wr32le(&displist[0x300 + i], xrgb);
    }
}

static UWORD Chip_SetDisplay(REGARG(struct BoardInfo *b, "a0"), REGARG(UWORD enabled, "d0"))
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;
    struct ExecBase *SysBase = VideoCoreBase->vc_LibNode.ExecBase;
#if 0
    if (0)
    {
        bug("[VC] SetDisplay %ld\n", enabled);
    }
    if (enabled) {
        BlankScreen(0, VideoCoreBase);
    } else {
        BlankScreen(1, VideoCoreBase);
    }
#endif
    return 1;
}

/* Gives the BoardInfo the functions both families share */
void Chip_Init(struct BoardInfo *bi)
{
    // Basic P96 functions needed for "dumb frame buffer" operation
    bi->SetSwitch = (void *)Chip_SetSwitch;
    bi->SetColorArray = (void *)Chip_SetColorArray;
    bi->SetDAC = (void *)Chip_SetDAC;
    bi->SetGC = (void *)Chip_SetGC;
    bi->CalculateBytesPerRow = (void *)Chip_CalculateBytesPerRow;
    bi->CalculateMemory = (void *)Chip_CalculateMemory;
    bi->GetCompatibleFormats = (void *)Chip_GetCompatibleFormats;
    bi->SetDisplay = (void *)Chip_SetDisplay;

    bi->ResolvePixelClock = (void *)Chip_ResolvePixelClock;
    bi->GetPixelClock = (void *)Chip_GetPixelClock;
    bi->SetClock = (void *)Chip_SetClock;

    bi->SetMemoryMode = (void *)Chip_SetMemoryMode;
    bi->SetWriteMask = (void *)Chip_SetWriteMask;
    bi->SetClearMask = (void *)Chip_SetClearMask;
    bi->SetReadPlane = (void *)Chip_SetReadPlane;

    bi->WaitVerticalSync = (void *)Chip_WaitVerticalSync;

    // Additional functions for "blitter" acceleration and vblank handling
    //bi->SetInterrupt = (void *)NULL;

    //bi->WaitBlitter = (void *)NULL;

    //bi->ScrollPlanar = (void *)NULL;
    //bi->UpdatePlanar = (void *)NULL;

    //bi->BlitPlanar2Chunky = (void *)BlitPlanar2Chunky;
    //bi->BlitPlanar2Direct = (void *)BlitPlanar2Direct;

    //bi->FillRect = (void *)FillRect;
    //bi->InvertRect = (void *)InvertRect;
    //bi->BlitRect = (void *)BlitRect;
    //bi->BlitTemplate = (void *)BlitTemplate;
    //bi->BlitPattern = (void *)BlitPattern;
    //bi->DrawLine = (void *)DrawLine;
    //bi->BlitRectNoMaskComplete = (void *)BlitRectNoMaskComplete;
    //bi->EnableSoftSprite = (void *)NULL;

    //bi->AllocCardMemAbs = (void *)NULL;
    //bi->SetSplitPosition = (void *)NULL;
    //bi->ReInitMemory = (void *)NULL;
    //bi->WriteYUVRect = (void *)NULL;
    //bi->GetVSyncState = (void *)GetVSyncState;
    bi->GetVBeamPos = (void *)Chip_GetVBeamPos;
    //bi->SetDPMSLevel = (void *)NULL;
    //bi->ResetChip = (void *)NULL;
    //bi->GetFeatureAttrs = (void *)NULL;
    //bi->AllocBitMap = (void *)NULL;
    //bi->FreeBitMap = (void *)NULL;
    //bi->GetBitMapAttr = (void *)NULL;

    bi->SetSpriteImage = (void *)Chip_SetSpriteImage;
    bi->SetSpriteColor = (void *)Chip_SetSpriteColor;

    //bi->CreateFeature = (void *)NULL;
    //bi->SetFeatureAttrs = (void *)NULL;
    //bi->DeleteFeature = (void *)NULL;
}
