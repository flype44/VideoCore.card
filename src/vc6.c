#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>

#include <proto/exec.h>
#include <proto/unicam.h>
#include <proto/mathieeesingbas.h>

#include <graphics/rastport.h>
#include <hardware/cia.h>
#include <common/compiler.h>
#include <resources/unicam.h>

// Shut off MathIEEE float injecting stuff
#define FLOAT ULONG

#include "emu68-vc4.h"
#include "vc6.h"
#include "boardinfo.h"
#include "mbox.h"
#include "buddyalloc.h"
#include "vpu.h"
#include "dma40.h"

UWORD VC6_CalculateBytesPerRow(REGARG(struct BoardInfo *b, "a0"), REGARG(UWORD width, "d0"), REGARG(RGBFTYPE format, "d7"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;

    if (!b)
        return 0;

    UWORD pitch = width;

    if (0)
    {
        bug("[VC4] CalculateBytesPerRow pitch %ld, format %lx\n", pitch, format);
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

void VC6_SetDAC(REGARG(struct BoardInfo *b, "a0"), REGARG(RGBFTYPE format, "d7"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    
    if (0)
        bug("[VC4] SetDAC\n");
    // Used to set the color format of the video card's RAMDAC.
    // This needs no handling, since the PiStorm doesn't really have a RAMDAC or a video card chipset.
}

void VC6_SetGC(REGARG(struct BoardInfo *b, "a0"), REGARG(struct ModeInfo *mode_info, "a1"), REGARG(BOOL border, "d0"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
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
        bug("[VC4] SetGC %ld x %ld x %ld\n", dim.width, dim.height, mode_info->Depth);
    }

    if (need_switch) {
        VC4Base->vc4_LastPanning.lp_Addr = NULL;
        //init_display(dim, mode_info->Depth, &VC4Base->vc4_Framebuffer, &VC4Base->vc4_Pitch, VC4Base);
    }
}

static const ULONG mode_table[] = {
    [RGBFB_A8R8G8B8] = VC6_CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGBA8888) | VC6_CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_RGBA),
    [RGBFB_A8B8G8R8] = VC6_CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGBA8888) | VC6_CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_BGRA),
    [RGBFB_B8G8R8A8] = VC6_CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGBA8888) | VC6_CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_ARGB),
    [RGBFB_R8G8B8A8] = VC6_CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGBA8888) | VC6_CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_ABGR),

    [RGBFB_R8G8B8] = VC6_CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGB888) | VC6_CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XBGR),
    [RGBFB_B8G8R8] = VC6_CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGB888) | VC6_CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XRGB),

    [RGBFB_R5G6B5PC] = VC6_CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGB565) | VC6_CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XRGB),
    [RGBFB_R5G5B5PC] = VC6_CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGB555) | VC6_CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XRGB),

    [RGBFB_R5G6B5] = VC6_CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGB565) | VC6_CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XRGB),
    [RGBFB_R5G5B5] = VC6_CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGB555) | VC6_CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XRGB),

    [RGBFB_B5G6R5PC] = VC6_CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGB565) | VC6_CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XBGR),
    [RGBFB_B5G5R5PC] = VC6_CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGB555) | VC6_CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XBGR),

    [RGBFB_CLUT] = VC6_CONTROL_FORMAT(HVS_PIXEL_FORMAT_PALETTE) | VC6_CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XBGR)
};

static const UBYTE bpp_table[] = {
    [RGBFB_A8R8G8B8] = 4,
    [RGBFB_A8B8G8R8] = 4,
    [RGBFB_B8G8R8A8] = 4,
    [RGBFB_R8G8B8A8] = 4,

    [RGBFB_R8G8B8] = 3,
    [RGBFB_B8G8R8] = 3,

    [RGBFB_R5G6B5PC] = 2,
    [RGBFB_R5G5B5PC] = 2,

    [RGBFB_R5G6B5] = 2,
    [RGBFB_R5G5B5] = 2,

    [RGBFB_B5G6R5PC] = 2,
    [RGBFB_B5G5R5PC] = 2,

    [RGBFB_CLUT] = 1
};

UWORD VC6_SetSwitch(REGARG(struct BoardInfo *b, "a0"), REGARG(UWORD enabled, "d0"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    volatile ULONG *displist = (ULONG *)0xf2404000;

    if (0)
    {
        bug("[VC4] SetSwitch %ld\n", enabled);
    }

    if (VC4Base->vc4_Enabled != enabled) {
        VC4Base->vc4_Enabled = enabled;

        switch(enabled) {
            case 0:
                blank_screen(1, VC4Base);
                break;
            default:
                blank_screen(0, VC4Base);
                break;
        }
    }
    
    /* If switch mode is selected */
    if (VC4Base->vc4_SwitchMode != None)
    {
        UWORD en = enabled;

        /* Invert the switch mode */
        if (VC4Base->vc4_SwitchInverted)
        {
            en = 1 - en;
        }

        switch (VC4Base->vc4_SwitchMode)
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
                    VC4Base->vc4_UnicamVisible = TRUE;
                    wr32le((volatile uint32_t *)0xf2400024, BUDDY_OFFSET(VC4Base->vc4_UnicamDL));
                }
                else {
                    VC4Base->vc4_UnicamVisible = FALSE;
                    wr32le((volatile uint32_t *)0xf2400024, BUDDY_OFFSET(VC4Base->vc4_ActivePlane));
                }
                break;
        }
    }

    return 1 - enabled;
}

void VC6_SetPanning(REGARG(struct BoardInfo *b, "a0"), REGARG(UBYTE *addr, "a1"), 
                    REGARG(UWORD width, "d0"), REGARG(WORD x_offset, "d1"), 
                    REGARG(WORD y_offset, "d2"), REGARG(RGBFTYPE format, "d7"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    int unity = 0;
    ULONG scale_x = 0;
    ULONG scale_y = 0;
    ULONG scale = 0;
    ULONG recip_x = 0;
    ULONG recip_y = 0;
    UWORD offset_x = 0;
    UWORD offset_y = 0;
    ULONG calc_width = 0;
    ULONG calc_height = 0;
    ULONG sprite_width = 0;
    ULONG sprite_height = 0;
    ULONG bytes_per_row = VC6_CalculateBytesPerRow(b, width, format);
    ULONG bytes_per_pix = bytes_per_row / width;
    UWORD pos = 0;
    ULONG plane = -1;
    int offset_only = 0;

    if (0) {
        bug("[VC4] SetPanning %lx %ld %ld %ld %lx\n", addr, width, x_offset, y_offset, format);
    }

    if (VC4Base->vc4_LastPanning.lp_Addr != NULL && 
        width == VC4Base->vc4_LastPanning.lp_Width &&
        format == VC4Base->vc4_LastPanning.lp_Format)
    {
        if (addr == VC4Base->vc4_LastPanning.lp_Addr && x_offset == VC4Base->vc4_LastPanning.lp_X && y_offset == VC4Base->vc4_LastPanning.lp_Y) {
            if (0) {
                bug("[VC4] same panning as before. Skipping now\n");
            }
            return;
        }

        offset_only = 1;
    }

    VC4Base->vc4_LastPanning.lp_Addr = addr;
    VC4Base->vc4_LastPanning.lp_Width = width;
    VC4Base->vc4_LastPanning.lp_X = x_offset;
    VC4Base->vc4_LastPanning.lp_Y = y_offset;
    VC4Base->vc4_LastPanning.lp_Format = format;

    if (format != RGBFB_CLUT &&
        b->ModeInfo->Width == VC4Base->vc4_DispSize.width &&
        b->ModeInfo->Height == VC4Base->vc4_DispSize.height)
    {
        unity = 1;
        sprite_width = MAXSPRITEWIDTH;
        sprite_height = MAXSPRITEHEIGHT;

        VC4Base->vc4_ScaleX = 0x10000;
        VC4Base->vc4_ScaleY = 0x10000;
        VC4Base->vc4_OffsetX = 0;
        VC4Base->vc4_OffsetY = 0;
        scale = 0x10000;
    }
    else
    {
        ULONG factor_y = (b->ModeInfo->Flags & GMF_DOUBLESCAN) ? 0x20000 : 0x10000;
        scale_x = 0x10000 * b->ModeInfo->Width / VC4Base->vc4_DispSize.width;
        scale_y = factor_y * b->ModeInfo->Height / VC4Base->vc4_DispSize.height;

        recip_x = 0xffffffff / scale_x;
        recip_y = 0xffffffff / scale_y;

        // Select larger scaling factor from X and Y, but it need to fit
        if (((factor_y * b->ModeInfo->Height) / scale_x) > VC4Base->vc4_DispSize.height) {
            scale = scale_y;
        }
        else {
            scale = scale_x;
        }

        if (VC4Base->vc4_IntegerScaler)
        {
            scale = 0x10000 / (ULONG)(0x10000 / scale);
        }

        VC4Base->vc4_ScaleX = scale;
        VC4Base->vc4_ScaleY = (b->ModeInfo->Flags & GMF_DOUBLESCAN) ? scale >> 1 : scale;

        calc_width = (0x10000 * b->ModeInfo->Width) / scale;
        calc_height = (factor_y * b->ModeInfo->Height) / scale;

        sprite_width = (0x10000 * MAXSPRITEWIDTH) / scale;
        sprite_height = (factor_y * MAXSPRITEHEIGHT) / scale;

        offset_x = (VC4Base->vc4_DispSize.width - calc_width) >> 1;
        offset_y = (VC4Base->vc4_DispSize.height - calc_height) >> 1;

        VC4Base->vc4_OffsetX = offset_x;
        VC4Base->vc4_OffsetY = offset_y;

        if (0)
            bug("[VC4] Selected scale: %08lx (X: %08lx, Y: %08lx, 1/X: %08lx, 1/Y: %08lx)\n"
                "[VC4] Scaled size: %ld x %ld, offset X %ld, offset Y %ld\n", scale, scale_x, scale_y, recip_x, recip_y,
                calc_width, calc_height, offset_x, offset_y);
    }

    volatile uint32_t *displist = (uint32_t *)0xf2404000;
   
    if (unity) {
        if (offset_only) {
            plane = VC4Base->vc4_ActivePlane;
            pos = BUDDY_OFFSET(plane);
            wr32le(&displist[pos + 5], 0xc0000000 | (ULONG)addr + y_offset * bytes_per_row + x_offset * bytes_per_pix);
            if (VC4Base->vc4_SpriteVisible)
                VC6_SetSpritePosition(b, VC4Base->vc4_MouseX, VC4Base->vc4_MouseY, format);
        }
        else {
            plane = BuddyAlloc(VC4Base, 8 + 20 + 4 + 8);
            pos = BUDDY_OFFSET(plane);
            int cnt = pos + 1;

            VC4Base->vc4_PlaneCoord = &displist[cnt];
            wr32le(&displist[cnt++], VC6_POS0_X(offset_x) | VC6_POS0_Y(offset_y));
            wr32le(&displist[cnt++], (VC6_SCALER_POS2_ALPHA_MODE_FIXED << VC6_SCALER_POS2_ALPHA_MODE_SHIFT) | VC6_SCALER_POS2_ALPHA(0xfff));
            wr32le(&displist[cnt++], VC6_POS2_H(b->ModeInfo->Height) | VC6_POS2_W(b->ModeInfo->Width));
            wr32le(&displist[cnt++], 0xdeadbeef);

            wr32le(&displist[cnt++], 0xc0000000 | (ULONG)addr + y_offset * bytes_per_row + x_offset * bytes_per_pix);
            wr32le(&displist[cnt++], 0xdeadbeef);
            wr32le(&displist[cnt++], bytes_per_row);

            wr32le(&displist[pos],
                VC6_CONTROL_VALID
                | VC6_CONTROL_WORDS(cnt - pos)
                | VC6_CONTROL_UNITY
                | VC6_CONTROL_ALPHA_EXPAND
                | VC6_CONTROL_RGB_EXPAND
                | mode_table[format]);

            VC4Base->vc4_PlaneScalerX = NULL;
            VC4Base->vc4_PlaneScalerY = NULL;

            int mouse_pos = cnt;
            cnt = mouse_pos + 1;

            VC4Base->vc4_MouseCoord = &displist[cnt];
            wr32le(&displist[cnt++], VC6_POS0_X(offset_x + VC4Base->vc4_MouseX - x_offset) |
                                     VC6_POS0_Y(offset_y + VC4Base->vc4_MouseY - y_offset));
            wr32le(&displist[cnt++], (VC6_SCALER_POS2_ALPHA_MODE_PIPELINE << VC6_SCALER_POS2_ALPHA_MODE_SHIFT) | VC6_SCALER_POS2_ALPHA(0xfff));
            wr32le(&displist[cnt++], VC6_POS1_H(sprite_height) | VC6_POS1_W(sprite_width));
            wr32le(&displist[cnt++], VC6_POS2_H(MAXSPRITEHEIGHT) | VC6_POS2_W(MAXSPRITEWIDTH));
            wr32le(&displist[cnt++], 0xdeadbeef); // Scratch written by HVS

            wr32le(&displist[cnt++], 0xc0000000 | (ULONG)VC4Base->vc4_SpriteShape);
            wr32le(&displist[cnt++], 0xdeadbeef); // Scratch written by HVS

            // Write pitch
            wr32le(&displist[cnt++], MAXSPRITEWIDTH);

            int clut_off = cnt;
            wr32le(&displist[cnt++], 0xc0000000 | (0x300 << 2));

            // LMB address - just behind LMB of main plane
            wr32le(&displist[cnt++], 16 * b->ModeInfo->Width / 2);

            // Write PPF Scaling
            wr32le(&displist[cnt++], (scale << 8) | VC4Base->vc4_Scaler | VC4Base->vc4_Phase);
            if (b->ModeInfo->Flags & GMF_DOUBLESCAN)
                wr32le(&displist[cnt++], ((scale << 7) & ~0xff) | VC4Base->vc4_Scaler | VC4Base->vc4_Phase);
            else
                wr32le(&displist[cnt++], (scale << 8) | VC4Base->vc4_Scaler | VC4Base->vc4_Phase);
            wr32le(&displist[cnt++], 0); // Scratch written by HVS

            ULONG unity_kernel = BUDDY_OFFSET(VC4Base->vc4_UnityKernel);

            // Write scaling kernel offset in dlist
            wr32le(&displist[cnt++], unity_kernel);
            wr32le(&displist[cnt++], unity_kernel);
            wr32le(&displist[cnt++], unity_kernel);
            wr32le(&displist[cnt++], unity_kernel);

            wr32le(&displist[mouse_pos],
                VC6_CONTROL_VALID               |
                VC6_CONTROL_WORDS(cnt-mouse_pos)    |
                VC6_CONTROL_ALPHA_EXPAND      |
                VC6_CONTROL_RGB_EXPAND        |
                mode_table[RGBFB_CLUT]
            );

            wr32le(&displist[cnt++], 0x80000000);
            wr32le(&displist[clut_off], 0xc0000000 | (cnt << 2));

            wr32le(&displist[cnt++], 0x00000000);
            VC4Base->vc4_MousePalette = &displist[cnt];
            wr32le(&displist[cnt++], VC4Base->vc4_SpriteColors[0]);
            wr32le(&displist[cnt++], VC4Base->vc4_SpriteColors[1]);
            wr32le(&displist[cnt++], VC4Base->vc4_SpriteColors[2]);

#if 0
            for (int i=pos; i < cnt; i++) {
                ULONG args[] = {
                    i, LE32(displist[i])
                };

                bug("[VC6] dlist[%ld] = %08lx\n", i, LE32(displist[i]));
            }
#endif

        }
    } else {
        if (offset_only) {
            plane = VC4Base->vc4_ActivePlane;
            pos = BUDDY_OFFSET(plane);
            wr32le(&displist[pos + 6], 0xc0000000 | (ULONG)addr + y_offset * bytes_per_row + x_offset * bytes_per_pix);
            if (VC4Base->vc4_SpriteVisible)
                VC6_SetSpritePosition(b, VC4Base->vc4_MouseX, VC4Base->vc4_MouseY, format);
        }
        else 
        {
            plane = BuddyAlloc(VC4Base, 2*20 + 4 + 8);
            pos = BUDDY_OFFSET(plane);
            int cnt = pos + 1;

            VC4Base->vc4_PlaneCoord = &displist[cnt];
            wr32le(&displist[cnt++], VC6_POS0_X(offset_x) | VC6_POS0_Y(offset_y));
            wr32le(&displist[cnt++], (VC6_SCALER_POS2_ALPHA_MODE_FIXED << VC6_SCALER_POS2_ALPHA_MODE_SHIFT) | VC6_SCALER_POS2_ALPHA(0xfff));
            wr32le(&displist[cnt++], VC6_POS1_H(calc_height) | VC6_POS1_W(calc_width));
            wr32le(&displist[cnt++], VC6_POS2_H(b->ModeInfo->Height) | VC6_POS2_W(b->ModeInfo->Width));
            wr32le(&displist[cnt++], 0xdeadbeef); // Scratch written by HVS

            wr32le(&displist[cnt++], 0xc0000000 | (ULONG)addr + y_offset * bytes_per_row + x_offset * bytes_per_pix);
            wr32le(&displist[cnt++], 0xdeadbeef); // Scratch written by HVS

            // Write pitch
            wr32le(&displist[cnt++], bytes_per_row);

            // Palette mode - offset of palette placed in dlist
            if (format == RGBFB_CLUT) {
                wr32le(&displist[cnt++], 0xc0000000 | (0x300 << 2));
            }

            // LMB address
            wr32le(&displist[cnt++], 0);

            // Write PPF Scaling
            VC4Base->vc4_PlaneScalerX = &displist[cnt];
            VC4Base->vc4_PlaneScalerY = &displist[cnt+1];

            wr32le(&displist[cnt++], (scale << 8) | VC4Base->vc4_Scaler | VC4Base->vc4_Phase);
            if (b->ModeInfo->Flags & GMF_DOUBLESCAN)
                wr32le(&displist[cnt++], ((scale << 7) & ~0xff) | VC4Base->vc4_Scaler | VC4Base->vc4_Phase);
            else
                wr32le(&displist[cnt++], (scale << 8) | VC4Base->vc4_Scaler | VC4Base->vc4_Phase);
            wr32le(&displist[cnt++], 0); // Scratch written by HVS

            ULONG kernel_start = BUDDY_OFFSET(VC4Base->vc4_ScalingKernel);

            // Write scaling kernel offset in dlist
            VC4Base->vc4_Kernel = &displist[cnt];
            wr32le(&displist[cnt++], kernel_start);
            wr32le(&displist[cnt++], kernel_start);
            wr32le(&displist[cnt++], kernel_start);
            wr32le(&displist[cnt++], kernel_start);

            wr32le(&displist[pos],
                VC6_CONTROL_VALID             |
                VC6_CONTROL_WORDS(cnt-pos)    |
                VC6_CONTROL_ALPHA_EXPAND      |
                VC6_CONTROL_RGB_EXPAND        |
                mode_table[format]
            );

            int mouse_pos = cnt;
            cnt = mouse_pos + 1;

            VC4Base->vc4_MouseCoord = &displist[cnt];
            wr32le(&displist[cnt++], VC6_POS0_X(offset_x + 0x10000 * (VC4Base->vc4_MouseX - x_offset) / VC4Base->vc4_ScaleX) |
                                     VC6_POS0_Y(offset_y + 0x10000 * (VC4Base->vc4_MouseY - y_offset) / VC4Base->vc4_ScaleY));
            wr32le(&displist[cnt++], (VC6_SCALER_POS2_ALPHA_MODE_PIPELINE << VC6_SCALER_POS2_ALPHA_MODE_SHIFT) | VC6_SCALER_POS2_ALPHA(0xfff));
            wr32le(&displist[cnt++], VC6_POS1_H(sprite_height) | VC6_POS1_W(sprite_width));
            wr32le(&displist[cnt++], VC6_POS2_H(MAXSPRITEHEIGHT) | VC6_POS2_W(MAXSPRITEWIDTH));
            wr32le(&displist[cnt++], 0xdeadbeef); // Scratch written by HVS

            wr32le(&displist[cnt++], 0xc0000000 | (ULONG)VC4Base->vc4_SpriteShape);
            wr32le(&displist[cnt++], 0xdeadbeef); // Scratch written by HVS

            // Write pitch
            wr32le(&displist[cnt++], MAXSPRITEWIDTH);

            int clut_off = cnt;
            wr32le(&displist[cnt++], 0xc0000000 | (0x300 << 2));

            // LMB address - just behind LMB of main plane
            wr32le(&displist[cnt++], 16 * b->ModeInfo->Width / 2);

            // Write PPF Scaling
            wr32le(&displist[cnt++], (scale << 8) | VC4Base->vc4_Scaler | VC4Base->vc4_Phase);
            if (b->ModeInfo->Flags & GMF_DOUBLESCAN)
                wr32le(&displist[cnt++], ((scale << 7) & ~0xff) | VC4Base->vc4_Scaler | VC4Base->vc4_Phase);
            else
                wr32le(&displist[cnt++], (scale << 8) | VC4Base->vc4_Scaler | VC4Base->vc4_Phase);
            wr32le(&displist[cnt++], 0); // Scratch written by HVS

            // Write scaling kernel offset in dlist
            wr32le(&displist[cnt++], kernel_start);
            wr32le(&displist[cnt++], kernel_start);
            wr32le(&displist[cnt++], kernel_start);
            wr32le(&displist[cnt++], kernel_start);

            wr32le(&displist[mouse_pos],
                VC6_CONTROL_VALID               |
                VC6_CONTROL_WORDS(cnt-mouse_pos)    |
                VC6_CONTROL_ALPHA_EXPAND      |
                VC6_CONTROL_RGB_EXPAND        |
                mode_table[RGBFB_CLUT]
            );

            wr32le(&displist[cnt++], 0x80000000);
            wr32le(&displist[clut_off], 0xc0000000 | (cnt << 2));

            wr32le(&displist[cnt++], 0x00000000);
            VC4Base->vc4_MousePalette = &displist[cnt];
            wr32le(&displist[cnt++], VC4Base->vc4_SpriteColors[0]);
            wr32le(&displist[cnt++], VC4Base->vc4_SpriteColors[1]);
            wr32le(&displist[cnt++], VC4Base->vc4_SpriteColors[2]);

#if 0
            for (int i=pos; i < cnt; i++) {
                ULONG args[] = {
                    i, LE32(displist[i])
                };

                bug("[VC6] dlist[%ld] = %08lx\n", i, LE32(displist[i]));
            }
#endif
        }
    }

    if (plane != VC4Base->vc4_ActivePlane)
    {
        volatile ULONG *stat = (ULONG*)(0xf2400000 + SCALER_DISPSTAT1);

        // Wait for vertical blank before updating the display list
        do { asm volatile("nop"); } while((LE32(*stat) & 0xfff) != VC4Base->vc4_DispSize.height);

        wr32le((volatile uint32_t *)0xf2400024, pos);
        BuddyFree(VC4Base, VC4Base->vc4_ActivePlane);
        VC4Base->vc4_ActivePlane = plane;
    }
}

void VC6_SetColorArray(REGARG(struct BoardInfo *b, "a0"), REGARG(UWORD start, "d0"), REGARG(UWORD num, "d1"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    volatile uint32_t *displist = (uint32_t *)0xf2404000;

    // Sets the color components of X color components for 8-bit paletted display modes.
    if (!b->CLUT)
        return;
    
    if (0)
    {
        bug("[VC4] SetColorArray %ld %ld\n", start, num);
    }

    int j = start + num;
    
    for(int i = start; i < j; i++) {
        unsigned long xrgb = 0xff000000 | (b->CLUT[i].Blue) | (b->CLUT[i].Green << 8) | (b->CLUT[i].Red << 16);
        wr32le(&displist[0x300 + i], xrgb);
    }
}


APTR VC6_CalculateMemory(REGARG(struct BoardInfo *b, "a0"), REGARG(unsigned long addr, "a1"), 
                         REGARG(RGBFTYPE format, "d7"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;

    if (0)
    {
        bug("[VC4] CalculateMemory %lx %lx\n", addr, format);
    }

    return (APTR)addr;
}


enum fake_rgbftypes {
    RGBF_8BPP_CLUT,
    RGBF_24BPP_RGB,
    RGBF_24BPP_BGR,
    RGBF_16BPP_RGB565_PC,
    RGBF_16BPP_RGB555_PC,
	RGBF_32BPP_ARGB,
    RGBF_32BPP_ABGR,
	RGBF_32BPP_RGBA,
    RGBF_32BPP_BGRA,
    RGBF_16BPP_RGB565,
    RGBF_16BPP_RGB555,
    RGBF_16BPP_BGR565_PC,
    RGBF_16BPP_BGR555_PC,
    RGBF_YUV_422_0,  // (Actually 4:2:0?) Just a duplicate of RGBF_YUV_422?
    RGBF_YUV_411,    // No, these are 4:2:0
    RGBF_YUV_411_PC, // No, these are 4:2:0
    RGBF_YUV_422,
    RGBF_YUV_422_PC,
    RGBF_YUV_422_PLANAR,
    RGBF_YUV_422_PLANAR_PC,
};
#define BIP(a) (1 << a)

ULONG VC6_GetCompatibleFormats(REGARG(struct BoardInfo *b, "a0"), REGARG(RGBFTYPE format, "d7"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    if (0)
    {
        bug("[VC4] GetCompatibleFormats %lx\n", format);
    }
    //return BIP(RGBF_8BPP_CLUT) | BIP(RGBF_24BPP_RGB) | BIP(RGBF_24BPP_BGR) | BIP(RGBF_32BPP_ARGB) | BIP(RGBF_32BPP_ABGR) | BIP(RGBF_32BPP_RGBA) | BIP(RGBF_32BPP_BGRA);
    return 0xFFFFFFFF;
}

//static int display_enabled = 0;
UWORD VC6_SetDisplay(REGARG(struct BoardInfo *b, "a0"), REGARG(UWORD enabled, "d0"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;

#if 0
    if (1)
    {
        bug("[VC4] SetDisplay %ld\n", enabled);
    }
    if (enabled) {
        blank_screen(0, VC4Base);
    } else {
        blank_screen(1, VC4Base);
    }
#endif

    return 1;
}

LONG VC6_ResolvePixelClock(REGARG(struct BoardInfo *b, "a0"), REGARG(struct ModeInfo *mode_info, "a1"),
                           REGARG(ULONG pixel_clock, "d0"), REGARG(RGBFTYPE format, "d7"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    
    if (0)
    {
        bug("[VC4] ResolvePixelClock %lx %ld %lx\n", mode_info, pixel_clock, format);
    }

    ULONG clock = mode_info->HorTotal * mode_info->VerTotal * VC4Base->vc4_VertFreq;

    if (b->ModeInfo->Flags & GMF_DOUBLESCAN)
        clock <<= 1;

    mode_info->PixelClock = clock;
    mode_info->pll1.Clock = 0;
    mode_info->pll2.ClockDivide = 1;

    return 0;
}

ULONG VC6_GetPixelClock(REGARG(struct BoardInfo *b, "a0"), REGARG(struct ModeInfo *mode_info, "a1"),
                        REGARG(ULONG index, "d0"), REGARG(RGBFTYPE format, "d7"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    
    ULONG clock = mode_info->HorTotal * mode_info->VerTotal * VC4Base->vc4_VertFreq;

    if (b->ModeInfo->Flags & GMF_DOUBLESCAN)
        clock <<= 1;

    return clock;
}

// None of these five really have to do anything.
void VC6_SetClock(REGARG(struct BoardInfo *b, "a0"))
{
}

void VC6_SetMemoryMode(REGARG(struct BoardInfo *b, "a0"), REGARG(RGBFTYPE format, "d7"))
{
}

void VC6_SetWriteMask(REGARG(struct BoardInfo *b, "a0"), REGARG(UBYTE mask, "d0"))
{
}

void VC6_SetClearMask(REGARG(struct BoardInfo *b, "a0"), REGARG(UBYTE mask, "d0"))
{
}

void VC6_SetReadPlane(REGARG(struct BoardInfo *b, "a0"), REGARG(UBYTE plane, "d0"))
{
}

void VC6_SetSprite(REGARG(struct BoardInfo *b, "a0"), REGARG(BOOL enable, "d0"), REGARG(RGBFTYPE format, "d7"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;

    VC4Base->vc4_SpriteVisible = enable;

    if (enable) {
        LONG _x;
        LONG _y;

        if (VC4Base->vc4_ScaleX)
            _x = 0x10000 * VC4Base->vc4_MouseX / VC4Base->vc4_ScaleX;
        else
            _x = VC4Base->vc4_MouseX;

        if (VC4Base->vc4_ScaleY)
            _y = 0x10000 * VC4Base->vc4_MouseY / VC4Base->vc4_ScaleY;
        else
            _y = VC4Base->vc4_MouseY;

        if (VC4Base->vc4_MouseCoord) {
            wr32le(&VC4Base->vc4_MouseCoord[0], VC6_POS0_X(_x) | VC6_POS0_Y(_y));
        }
    }
    else
    {
        if (VC4Base->vc4_MouseCoord) {
            wr32le(&VC4Base->vc4_MouseCoord[0], VC6_POS0_X(-1) | VC6_POS0_Y(-1));
        }
    }
}

void VC6_SetSpritePosition(REGARG(struct BoardInfo *b, "a0"), REGARG(WORD x, "d0"), 
                           REGARG(WORD y, "d1"), REGARG(RGBFTYPE format, "d7"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;

/*
    x = b->MouseX - b->XOffset;
    y = b->MouseY - b->YOffset;
*/

    VC4Base->vc4_MouseX = x;
    VC4Base->vc4_MouseY = y;

    x -= VC4Base->vc4_LastPanning.lp_X;
    y -= VC4Base->vc4_LastPanning.lp_Y;

    LONG _x;
    LONG _y;

    if (VC4Base->vc4_ScaleX)
        _x = 0x10000 * x / VC4Base->vc4_ScaleX;
    else
        _x = x;

    if (VC4Base->vc4_ScaleY)
        _y = 0x10000 * y / VC4Base->vc4_ScaleY;
    else
        _y = y;

    _x += VC4Base->vc4_OffsetX;
    _y += VC4Base->vc4_OffsetY;

    if (VC4Base->vc4_MouseCoord) {   
        wr32le(&VC4Base->vc4_MouseCoord[0], VC6_POS0_X(_x) | VC6_POS0_Y(_y));
    }
}

void VC6_SetSpriteImage(REGARG(struct BoardInfo *b, "a0"), REGARG(RGBFTYPE format, "d7"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = *(struct ExecBase **)4;

    for (int i=0; i < MAXSPRITEWIDTH * MAXSPRITEHEIGHT; i++)
        VC4Base->vc4_SpriteShape[i] = 0;

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
                VC4Base->vc4_SpriteShape[y * MAXSPRITEWIDTH + x] = pix;
                mask = mask >> 1;
            }
        }
    }
    else if (b->Flags & BIF_BIGSPRITE) {
        UWORD *data = b->MouseImage;
        UWORD height = b->MouseHeight / 2;      // rtg.library gives the doubled height, the image has half of it

        if (height > MAXSPRITEHEIGHT / 2)
            height = MAXSPRITEHEIGHT / 2;

        data += 2;

        for (int y=0; y < height; y++) {
            UWORD p0 = *data++;
            UWORD p1 = *data++;
            UWORD mask = 0x8000;
            for (int x=0; x < 16; x++) {
                UBYTE pix = 0;
                if (p0 & mask) pix |= 1;
                if (p1 & mask) pix |= 2;
                VC4Base->vc4_SpriteShape[2 * y * MAXSPRITEWIDTH + 2*x] = pix;
                VC4Base->vc4_SpriteShape[2 * y * MAXSPRITEWIDTH + 2*x + 1] = pix;
                VC4Base->vc4_SpriteShape[(2 * y + 1) * MAXSPRITEWIDTH + 2*x] = pix;
                VC4Base->vc4_SpriteShape[(2 * y + 1) * MAXSPRITEWIDTH + 2*x + 1] = pix;
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
                VC4Base->vc4_SpriteShape[y * MAXSPRITEWIDTH + x] = pix;
                mask = mask >> 1;
            }
        }
    }

    CacheClearE(VC4Base->vc4_SpriteShape, MAXSPRITEHEIGHT * MAXSPRITEWIDTH, CACRF_ClearD);
}

void VC6_SetSpriteColor(REGARG(struct BoardInfo *b, "a0"), REGARG(UBYTE idx, "d0"),
                        REGARG(UBYTE R, "d1"), REGARG(UBYTE G, "d2"), REGARG(UBYTE B, "d3"),
                        REGARG(RGBFTYPE format, "d7"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    if (idx < 3) {
        VC4Base->vc4_SpriteColors[idx] = (VC4Base->vc4_SpriteAlpha << 24) | (R << 16) | (G << 8) | B;
        if (VC4Base->vc4_MousePalette) {
            wr32le(&VC4Base->vc4_MousePalette[idx], VC4Base->vc4_SpriteColors[idx]);
        }
    }
}

ULONG VC6_GetVBeamPos(REGARG(struct BoardInfo *b, "a0"))
{
    volatile ULONG *stat = (ULONG*)(0xf2400000 + SCALER_DISPSTAT1);
    ULONG vbeampos = LE32(*stat) & 0xfff;

    return vbeampos;
}

void VC6_WaitVerticalSync(REGARG(struct BoardInfo *b, "a0"), REGARG(BOOL toggle, "d0"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    volatile ULONG *stat = (ULONG*)(0xf2400000 + SCALER_DISPSTAT1);

    // Wait until current vbeampos is lower than the one obtained above
    do { asm volatile("nop"); } while((LE32(*stat) & 0xfff) != VC4Base->vc4_DispSize.height);
}

/* WaitBlitter - VPU calls are synchronous (mailbox), there is nothing to wait for, except a copy of the DMA (the
   rectangles of BlitRect, see dma40.c): that one runs on after its hook returned, and the task sleeps here until it is over.

   One thing could still need a barrier here: the VPU writes the destination of a copy through the cached alias of
   the bus (0x80000000, see VC6_BlitRect), and dirty lines of its cache could in theory reach the memory after the
   call returned, over what the CPU wrote there meanwhile. It was looked for on the RPi4B (2026-10-04) and not seen:
   the CPU reads the right data as soon as the call returns, a chain of 40 copies, each reading what the previous one
   wrote, was exact, and a pattern written by the CPU over the destination right after a copy was never overwritten
   in the 2 ms that followed (6 cases of alignment and direction, 8 copies of 1.4 to 1.6 MB each, 0 wrong longwords).
   That is a short window and the test could not be shown able to fail, so this is "not observed", not "impossible".

   If it ever shows up (stale pixels that come back after a copy), define VC6_WAITBLITTER_FLUSH: a call of an empty
   VPU routine without the "do not flush the caches" flag (the lowest bit of the address of the routine, see
   VPU_RectCopy) makes the firmware flush the caches of the VPU. Not tried on the hardware, and it costs a mailbox round
   trip (about 95 us) every time, because rtg.library calls WaitBlitter after each accelerated operation. */
void VC6_WaitBlitter(REGARG(struct BoardInfo *b, "a0"))
{
    /* A copy of the DMA runs on after its hook returned: rtg.library calls this right after each accelerated
       operation, the task sleeps here until the interrupt of the channel */
    DMA40_Wait((struct VC4Base *)b->CardBase);

#ifdef VC6_WAITBLITTER_FLUSH
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    static const UBYTE vpu_rts[] = { 0x5a, 0x00 };  /* rts */
    static ULONG vpu_nop;

    if (vpu_nop == 0)
        vpu_nop = upload_code(vpu_rts, sizeof(vpu_rts), VC4Base);

    if (vpu_nop != 0)
        call_code(vpu_nop, 0, 0, 0, 0, 0, 0, VC4Base);
#endif
}

/* FillRect - not accelerated yet, let Picasso96 do it */
void VC6_FillRect(REGARG(struct BoardInfo *b, "a0"), REGARG(struct RenderInfo *r, "a1"),
                  REGARG(WORD x, "d0"), REGARG(WORD y, "d1"),
                  REGARG(WORD w, "d2"), REGARG(WORD h, "d3"),
                  REGARG(ULONG color, "d4"), REGARG(UBYTE mask, "d5"),
                  REGARG(RGBFTYPE format, "d7"))
{
    b->FillRectDefault(b, r, x, y, w, h, color, mask, format);
}

/* BlitRect - copy a rectangle inside the bitmap using the VPU */
void VC6_BlitRect(REGARG(struct BoardInfo *b, "a0"), REGARG(struct RenderInfo *r, "a1"),
                  REGARG(WORD x, "d0"), REGARG(WORD y, "d1"),
                  REGARG(WORD dx, "d2"), REGARG(WORD dy, "d3"),
                  REGARG(WORD w, "d4"), REGARG(WORD h, "d5"),
                  REGARG(UBYTE mask, "d6"), REGARG(RGBFTYPE format, "d7"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    ULONG bpp = bpp_table[format];
    ULONG stride = r->BytesPerRow;
    ULONG srcp = (ULONG)r->Memory + y * stride + x * bpp;
    ULONG dstp = (ULONG)r->Memory + dy * stride + dx * bpp;
    LONG gap = (LONG)(srcp - dstp);
    /* The VPU reads through the uncached alias of the bus (0xc0000000) and writes through the cached one
       (0x80000000): writing is what costs, about three times less this way. A move of less than 64 bytes
       inside a row keeps the uncached alias for both: the routines then copy the end of the lines with scalar
       stores, which do not mix with the cached alias. */
    ULONG src = 0xc0000000 + srcp;
    ULONG dst = ((gap > -64 && gap < 64) ? 0xc0000000 : 0x80000000) + dstp;
    ULONG height = h;
    ULONG width = w * bpp;

    /* One transfer of the DMA at a time, and nothing else may touch the bitmap before it is over */
    DMA40_Wait(VC4Base);

    /* The DMA copies the rectangle without the 68k (it sleeps in WaitBlitter): a scroll, or any move by whole rows. It takes
       neither a mask nor a shift to the side by less than the width, which the VPU or the CPU do below. */
    if (mask == 0xff && VC4Base->vc4_DMA.Ok && width * height >= VC4Base->vc4_DMA.Min &&
        (ULONG)r->Memory >= 0x01000000 && (ULONG)r->Memory < 0x40000000 &&
        DMA40_CopyRect(VC4Base, srcp, dstp, width, height, stride))
        return;

    /* Only bitmaps located in the memory visible to the VPU can be handled, and
       small rectangles are cheaper to copy by the CPU than a mailbox round trip */
    if (width * height < VC4Base->vc4_VPUMinSize ||
        (ULONG)r->Memory < 0x01000000 || (ULONG)r->Memory >= 0x40000000)
    {
        b->BlitRectDefault(b, r, x, y, dx, dy, w, h, mask, format);
        return;
    }

    /* The VPU copy knows nothing about masks */
    if (mask != 0xff || VPU_RectCopy((APTR)src, (APTR)dst, width, height, stride - width, stride - width, VC4Base) == 0)
    {
        b->BlitRectDefault(b, r, x, y, dx, dy, w, h, mask, format);
    }
}

/* Inverts 8 pixels, the mask is the literal of the bits to invert. A single read of the video
   memory is slow, eight of them in one burst are not. */
#define EORI(n, m) "eori.l #" m ",%%d" #n "\n\t"
#define INVERT8(p, m) asm volatile ( \
    "movem.l (%0),%%d0-%%d7\n\t" \
    EORI(0, m) EORI(1, m) EORI(2, m) EORI(3, m) EORI(4, m) EORI(5, m) EORI(6, m) EORI(7, m) \
    "movem.l %%d0-%%d7,(%0)" \
    : : "a"(p) : "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "cc", "memory")

#define INVERT_RECT(row, stride, w, h, m, value) \
    for (; h > 0; h--) \
    { \
        ULONG *p = row; \
        WORD n = w; \
        for (; n >= 8; n -= 8, p += 8) \
            INVERT8(p, m); \
        for (; n > 0; n--) \
            *p++ ^= value; \
        row = (ULONG *)((ULONG)row + stride); \
    }

/* Inverts the colour bits of the pixels of a rectangle of 32 bit pixels, like Picasso96 the
   alpha byte is left alone: the first byte of the pixel for A8R8G8B8 and A8B8G8R8, the last one
   for B8G8R8A8 and R8G8B8A8 */
static void invert_rect_32(struct RenderInfo *r, WORD x, WORD y, WORD w, WORD h, RGBFTYPE format)
{
    ULONG stride = r->BytesPerRow;
    ULONG *row = (ULONG *)((ULONG)r->Memory + y * stride + x * 4);

    if (format == RGBFB_A8R8G8B8 || format == RGBFB_A8B8G8R8)
    {
        INVERT_RECT(row, stride, w, h, "0x00ffffff", 0x00ffffffUL)
    }
    else
    {
        INVERT_RECT(row, stride, w, h, "0xffffff00", 0xffffff00UL)
    }
}

/* InvertRect - 32 bit pixels only, the other formats are left to Picasso96 */
void VC6_InvertRect(REGARG(struct BoardInfo *b, "a0"), REGARG(struct RenderInfo *r, "a1"),
                    REGARG(WORD x, "d0"), REGARG(WORD y, "d1"),
                    REGARG(WORD w, "d2"), REGARG(WORD h, "d3"),
                    REGARG(UBYTE mask, "d4"), REGARG(RGBFTYPE format, "d7"))
{
    if (bpp_table[format] == 4 && mask == 0xff)
        invert_rect_32(r, x, y, w, h, format);
    else
        b->InvertRectDefault(b, r, x, y, w, h, mask, format);
}

/* Stores 8 pixels, taken from two entries of four pixels of a table, in two bursts of 4. Only d0 to d3
   are used: with all of d0 to d7 GCC has no register left for the loop and uses the stack. */
#define PUT8(p, a, b) asm volatile ( \
    "movem.l (%1),%%d0-%%d3\n\t" \
    "movem.l %%d0-%%d3,(%0)\n\t" \
    "movem.l (%2),%%d0-%%d3\n\t" \
    "movem.l %%d0-%%d3,16(%0)" \
    : : "a"(p), "a"(a), "a"(b) : "d0", "d1", "d2", "d3", "memory")

/* The 8 bits at the bit offset off (0 to 7) of the byte string at ptr, in one instruction: much cheaper
   than two byte reads, a shift and a mask */
#define BFEXTU8(v, ptr, off) \
    asm("bfextu (%1){%2:8},%0" : "=d"(v) : "a"(ptr), "d"(off), "m"(*(const UBYTE (*)[2])(ptr)) : "cc")

/* The pixels of the 16 nibbles. Not on the stack: a program may have a small one (P96Speed has 4096 bytes), and
   the stack is aligned on 2 bytes only, which makes the movem.l reads slow. Hooks run under the BoardLock, so
   one table is enough. */
static ULONG template_tab[16][4] __attribute__((aligned(16)));

/* BlitTemplate for 32 bit pixels in the draw modes which write every pixel, JAM2 (FgPen for the bits
   which are set, BgPen for the others) and JAM2 | INVERSVID (the other way round). The template has
   one bit per pixel, the most significant bit first, and the first XOffset bits of every row are
   skipped. Returns 0 if the draw mode is not handled.
   JAM1 is left to Picasso96: it only writes the pixels of the bits which are set, which means scattered
   writes, about 100 ns each in the video memory, and this is no faster than the default code. */
static int blit_template_32(struct RenderInfo *r, struct Template *t, WORD x, WORD y, WORD w, WORD h)
{
    ULONG stride = r->BytesPerRow;
    ULONG c1, c0;
    ULONG (*tab)[4] = template_tab;
    WORD row;

    switch (t->DrawMode)
    {
        case JAM2:
            c1 = t->FgPen;
            c0 = t->BgPen;
            break;

        case JAM2 | INVERSVID:
            c1 = t->BgPen;
            c0 = t->FgPen;
            break;

        default:
            return 0;
    }

    if (w <= 0 || h <= 0)
        return 1;

    /* The pixels of a nibble come from a table, a burst stores 8 of them */
    if (w >= 16)
    {
        ULONG n, j;

        for (n = 0; n < 16; n++)
            for (j = 0; j < 4; j++)
                tab[n][j] = (n & (8 >> j)) ? c1 : c0;
    }

    for (row = 0; row < h; row++)
    {
        const UBYTE *s = (const UBYTE *)t->Memory + row * t->BytesPerRow + (t->XOffset >> 3);
        ULONG *p = (ULONG *)((ULONG)r->Memory + (y + row) * stride + x * 4);
        ULONG sh = t->XOffset & 7;
        WORD n8 = w >= 16 ? w >> 3 : 0;
        WORD rem = w - 8 * n8;
        WORD g, k;

        for (g = 0; g < n8; g++, p += 8)
        {
            ULONG v;

            BFEXTU8(v, s + g, sh);

            PUT8(p, tab[v >> 4], tab[v & 15]);
        }

        /* The last pixels, or the whole row when it is short, one by one */
        for (k = 0; k < rem; k++)
        {
            ULONG idx = sh + 8 * n8 + k;

            p[k] = ((s[idx >> 3] >> (7 - (idx & 7))) & 1) ? c1 : c0;
        }
    }

    return 1;
}

/* BlitTemplate - 32 bit pixels with a full plane mask only, the other cases are left to Picasso96 */
void VC6_BlitTemplate(REGARG(struct BoardInfo *b, "a0"), REGARG(struct RenderInfo *r, "a1"),
                      REGARG(struct Template *t, "a2"),
                      REGARG(WORD x, "d0"), REGARG(WORD y, "d1"),
                      REGARG(WORD w, "d2"), REGARG(WORD h, "d3"),
                      REGARG(UBYTE mask, "d4"), REGARG(RGBFTYPE format, "d7"))
{
    if (bpp_table[format] != 4 || mask != 0xff || blit_template_32(r, t, x, y, w, h) == 0)
        b->BlitTemplateDefault(b, r, t, x, y, w, h, mask, format);
}

/* The logical copies of BlitRectNoMaskComplete: OR (minterm 0xe), XOR (0x6), XNOR (0x9) and AND (0x8) between two
   different bitmaps of 32 bit B8G8R8A8 pixels. This is how Picasso96 draws an icon with a mask: two XNOR and one OR
   for each, a few thousand of them for a window moved across a dock of PowerWindowsNG, 360 us each because its own
   routine reads the video memory one longword at a time (about 180 ns each). Here the pixels are read and written
   by three at a time with movem.l.
   The first three bytes of a pixel follow the operation. The last one does not, it is what Picasso96 gives (looked
   at by comparing 20810 pixels of its routine in RAM and in the board memory): the byte of the source for OR,
   source | destination for XOR and XNOR, and the AND like the others. */
#define LOAD3 \
    "movem.l (%0),%%d0-%%d2\n\t" \
    "movem.l (%1),%%d3-%%d5\n\t"

#define STORE3 \
    "movem.l %%d3-%%d5,(%1)"

#define XOR_PIXEL(s, d) \
    "move.l %%d" #s ",%%d6\n\t" "and.l %%d" #d ",%%d6\n\t" "and.l #255,%%d6\n\t" \
    "eor.l %%d" #s ",%%d" #d "\n\t" "or.l %%d6,%%d" #d "\n\t"

#define XNOR_PIXEL(s, d) \
    "move.l %%d" #s ",%%d6\n\t" "or.l %%d" #d ",%%d6\n\t" "and.l #255,%%d6\n\t" \
    "eor.l %%d" #s ",%%d" #d "\n\t" "not.l %%d" #d "\n\t" "and.l #-256,%%d" #d "\n\t" "or.l %%d6,%%d" #d "\n\t"

static void VC6_MintermBlit(struct RenderInfo *rs, struct RenderInfo *rt,
                            WORD x, WORD y, WORD dx, WORD dy, WORD w, WORD h, UBYTE minterm)
{
    UBYTE *srow = (UBYTE *)rs->Memory + y * rs->BytesPerRow + x * 4;
    UBYTE *drow = (UBYTE *)rt->Memory + dy * rt->BytesPerRow + dx * 4;

    for (; h > 0; h--)
    {
        ULONG *s = (ULONG *)srow;
        ULONG *d = (ULONG *)drow;
        WORD n = w;

        switch (minterm)
        {
            case 0x0e:
                for (; n >= 3; n -= 3, s += 3, d += 3)
                    asm volatile (LOAD3
                        "and.l %2,%%d3\n\t" "and.l %2,%%d4\n\t" "and.l %2,%%d5\n\t"
                        "or.l %%d0,%%d3\n\t" "or.l %%d1,%%d4\n\t" "or.l %%d2,%%d5\n\t"
                        STORE3
                        : : "a"(s), "a"(d), "d"(0xffffff00UL) : "d0", "d1", "d2", "d3", "d4", "d5", "cc", "memory");
                for (; n > 0; n--, s++, d++)
                    *d = *s | (*d & 0xffffff00);
                break;

            case 0x06:
                for (; n >= 3; n -= 3, s += 3, d += 3)
                    asm volatile (LOAD3 XOR_PIXEL(0, 3) XOR_PIXEL(1, 4) XOR_PIXEL(2, 5) STORE3
                        : : "a"(s), "a"(d) : "d0", "d1", "d2", "d3", "d4", "d5", "d6", "cc", "memory");
                for (; n > 0; n--, s++, d++)
                    *d = (*s ^ *d) | (*s & *d & 0xff);
                break;

            case 0x09:
                for (; n >= 3; n -= 3, s += 3, d += 3)
                    asm volatile (LOAD3 XNOR_PIXEL(0, 3) XNOR_PIXEL(1, 4) XNOR_PIXEL(2, 5) STORE3
                        : : "a"(s), "a"(d) : "d0", "d1", "d2", "d3", "d4", "d5", "d6", "cc", "memory");
                for (; n > 0; n--, s++, d++)
                    *d = (~(*s ^ *d) & 0xffffff00) | ((*s | *d) & 0xff);
                break;

            default:    /* 0x08 */
                for (; n >= 3; n -= 3, s += 3, d += 3)
                    asm volatile (LOAD3
                        "and.l %%d0,%%d3\n\t" "and.l %%d1,%%d4\n\t" "and.l %%d2,%%d5\n\t"
                        STORE3
                        : : "a"(s), "a"(d) : "d0", "d1", "d2", "d3", "d4", "d5", "cc", "memory");
                for (; n > 0; n--, s++, d++)
                    *d &= *s;
                break;
        }

        srow += rs->BytesPerRow;
        drow += rt->BytesPerRow;
    }
}

/* A plain copy of 32 bit pixels between two different bitmaps with the CPU, 8 pixels (32 bytes) at a time with
   movem.l: a read of the video memory is slow (about 180 ns for a longword) but a burst is not (about 54 ns for
   16 bytes). It is what Picasso96's Default does not do, and the VPU does not pay for the small ones. */
static void VC6_CopyBlock(struct RenderInfo *rs, struct RenderInfo *rt,
                          WORD x, WORD y, WORD dx, WORD dy, WORD w, WORD h)
{
    UBYTE *srow = (UBYTE *)rs->Memory + y * rs->BytesPerRow + x * 4;
    UBYTE *drow = (UBYTE *)rt->Memory + dy * rt->BytesPerRow + dx * 4;

    for (; h > 0; h--)
    {
        ULONG *s = (ULONG *)srow;
        ULONG *d = (ULONG *)drow;
        WORD n = w;

        for (; n >= 8; n -= 8, s += 8, d += 8)
            asm volatile (
                "movem.l (%0),%%d0-%%d7\n\t"
                "movem.l %%d0-%%d7,(%1)"
                : : "a"(s), "a"(d) : "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "memory");

        for (; n > 0; n--)
            *d++ = *s++;

        srow += rs->BytesPerRow;
        drow += rt->BytesPerRow;
    }
}

/* BlitRectNoMaskComplete - copy a rectangle between two bitmaps, using the VPU for the big ones */
void VC6_BlitRectNoMaskComplete(REGARG(struct BoardInfo *b, "a0"), REGARG(struct RenderInfo *rs, "a1"),
                                REGARG(struct RenderInfo *rt, "a2"),
                                REGARG(WORD x, "d0"), REGARG(WORD y, "d1"),
                                REGARG(WORD dx, "d2"), REGARG(WORD dy, "d3"),
                                REGARG(WORD w, "d4"), REGARG(WORD h, "d5"),
                                REGARG(UBYTE minterm, "d6"), REGARG(RGBFTYPE format, "d7"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    ULONG bpp = bpp_table[format];
    ULONG width = w * bpp;
    ULONG height = h;

    /* Only plain copies between two different bitmaps located in the memory visible to the VPU can be
       handled (a copy inside a bitmap is BlitRect's, and the Default of Picasso96 sends it there), and
       small rectangles are cheaper to copy by the CPU than a mailbox round trip. Two different bitmaps
       never overlap: the destination goes through the cached alias of the bus, as in VC6_BlitRect(). */
    if (minterm == 0x0C && width * height >= VC4Base->vc4_VPUCopyMin &&
        (ULONG)rs->Memory >= 0x01000000 && (ULONG)rs->Memory < 0x40000000 &&
        (ULONG)rt->Memory >= 0x01000000 && (ULONG)rt->Memory < 0x40000000 &&
        rs->Memory != rt->Memory)
    {
        ULONG src = 0xc0000000 + (ULONG)rs->Memory + y * rs->BytesPerRow + x * bpp;
        ULONG dst = 0x80000000 + (ULONG)rt->Memory + dy * rt->BytesPerRow + dx * bpp;

        if (VPU_RectCopy((APTR)src, (APTR)dst, width, height, rs->BytesPerRow - width, rt->BytesPerRow - width, VC4Base) != 0)
            return;
    }

    /* The small plain copies of 32 bit pixels between two different bitmaps are done with bursts by the CPU */
    if (minterm == 0x0C && bpp == 4 && rs->Memory != rt->Memory)
    {
        VC6_CopyBlock(rs, rt, x, y, dx, dy, w, h);
        return;
    }

    /* OR, XOR, XNOR and AND between two bitmaps of B8G8R8A8 pixels, or between two areas of one bitmap that do
       not overlap (the icons of a dock are drawn like that: the OR of Picasso96's three calls) */
    if (format == RGBFB_B8G8R8A8 &&
        (rs->Memory != rt->Memory || x + w <= dx || dx + w <= x || y + h <= dy || dy + h <= y) &&
        (minterm == 0x0e || minterm == 0x06 || minterm == 0x09 || minterm == 0x08))
    {
        VC6_MintermBlit(rs, rt, x, y, dx, dy, w, h, minterm);
        return;
    }

    b->BlitRectNoMaskCompleteDefault(b, rs, rt, x, y, dx, dy, w, h, minterm, format);
}

/* Unicam DisplayList */
void VC6_ConstructUnicamDL(struct VC4Base *VC4Base)
{
    APTR UnicamBase = VC4Base->vc4_UnicamBase;
    int unity = 0;
    ULONG scale_x = 0;
    ULONG scale_y = 0;
    ULONG scale = 0;
    ULONG recip_x = 0;
    ULONG recip_y = 0;
    ULONG calc_width = 0;
    ULONG calc_height = 0;
    ULONG offset_x = 0;
    ULONG offset_y = 0;

    UBYTE scaler;
    UBYTE phase;

    const ULONG fullHeight = UnicamGetSize() & 0xffff;
    const ULONG fullWidth = UnicamGetSize() >> 16;
    const ULONG bpp = (UnicamGetMode() & 0xff) / 8;
    const ULONG aspect = UnicamGetMode() >> 16;

    ULONG config = UnicamGetConfig();

    ULONG KernelC = UnicamGetKernel();
    ULONG KernelB = KernelC >> 16;
    KernelC &= 0xffff;

    ULONG crop_w = UnicamGetCropSize();
    ULONG crop_h = crop_w & 0xffff;
    crop_w >>= 16;

    ULONG crop_x = UnicamGetCropOffset();
    ULONG crop_y = crop_x & 0xffff;
    crop_x >>= 16;

    scaler = (config & UNICAMF_SCALER) >> UNICAMB_SCALER;
    phase = (config & UNICAMF_PHASE) >> UNICAMB_PHASE;

    ULONG cnt = 0x300; // Initial pointer to UnicamDL

    volatile uint32_t *displist = (uint32_t *)0xf2404000;

    if (crop_w == VC4Base->vc4_DispSize.width &&
        crop_h == VC4Base->vc4_DispSize.height && aspect == 1000)
    {
        unity = 1;
    }
    else
    {
        scale_x = 0x10000 * ((crop_w * aspect) / 1000) / VC4Base->vc4_DispSize.width;
        scale_y = 0x10000 * crop_h / VC4Base->vc4_DispSize.height;

        recip_x = 0xffffffff / scale_x;
        recip_y = 0xffffffff / scale_y;

        // Select larger scaling factor from X and Y, but it need to fit
        if (((0x10000 * crop_h) / scale_x) > VC4Base->vc4_DispSize.height) {
            scale = scale_y;
        }
        else {
            scale = scale_x;
        }

        if (config & UNICAMF_INTEGER)
        {
            scale = 0x10000 / (ULONG)(0x10000 / scale);
        }

        scale_x = scale * 1000 / aspect;
        scale_y = scale;

        calc_width = (0x10000 * crop_w) / scale_x;
        calc_height = (0x10000 * crop_h) / scale_y;

        offset_x = (VC4Base->vc4_DispSize.width - calc_width) >> 1;
        offset_y = (VC4Base->vc4_DispSize.height - calc_height) >> 1;
    }

    ULONG startAddress = (ULONG)VC4Base->vc4_Unicambuffer;
    startAddress += crop_x * bpp;
    startAddress += crop_y * fullWidth * bpp;

    if (unity)
    {
        /* Unity scaling is simple, reserve less space for display list */
        cnt -= 16;

        VC4Base->vc4_UnicamDL = cnt;

        /* Set control reg */
        ULONG control =
            VC6_CONTROL_VALID
            | VC6_CONTROL_WORDS(8)
            | VC6_CONTROL_UNITY
            | VC6_CONTROL_ALPHA_EXPAND
            | VC6_CONTROL_RGB_EXPAND;

        if (bpp == 2)
            control |= mode_table[RGBFB_R5G6B5PC];
        else if (bpp == 3)
            control |= mode_table[RGBFB_R8G8B8];

        wr32le(&displist[cnt++], control);

        /* Center it on the screen */
        wr32le(&displist[cnt++], VC6_POS0_X(offset_x) | VC6_POS0_Y(offset_y));
        wr32le(&displist[cnt++], (VC6_SCALER_POS2_ALPHA_MODE_FIXED << VC6_SCALER_POS2_ALPHA_MODE_SHIFT) | VC6_SCALER_POS2_ALPHA(0xfff));
        wr32le(&displist[cnt++], VC6_POS2_H(crop_h) | VC6_POS2_W(crop_w));
        wr32le(&displist[cnt++], 0xdeadbeef);

        /* Set address */
        wr32le(&displist[cnt++], 0xc0000000 | startAddress);
        wr32le(&displist[cnt++], 0xdeadbeef);
        wr32le(&displist[cnt++], fullWidth * bpp);

        /* Done */
        wr32le(&displist[cnt++], 0x80000000);
    }
    else
    {
        cnt -= 24;
       
        VC4Base->vc4_UnicamDL = cnt;

        /* Set control reg */
        ULONG control =
            VC6_CONTROL_VALID
            | VC6_CONTROL_WORDS(17)
            | VC6_CONTROL_ALPHA_EXPAND
            | VC6_CONTROL_RGB_EXPAND;

        if (bpp == 2)
            control |= mode_table[RGBFB_R5G6B5PC];
        else if (bpp == 3)
            control |= mode_table[RGBFB_R8G8B8];

        wr32le(&displist[cnt++], control);

        /* Center plane on the screen */
        wr32le(&displist[cnt++], VC6_POS0_X(offset_x) | VC6_POS0_Y(offset_y));
        wr32le(&displist[cnt++], (VC6_SCALER_POS2_ALPHA_MODE_FIXED << VC6_SCALER_POS2_ALPHA_MODE_SHIFT) | VC6_SCALER_POS2_ALPHA(0xfff));
        wr32le(&displist[cnt++], VC6_POS1_H(calc_height) | VC6_POS1_W(calc_width));
        wr32le(&displist[cnt++], VC6_POS2_H(crop_h) | VC6_POS2_W(crop_w));
        wr32le(&displist[cnt++], 0xdeadbeef); // Scratch written by HVS

        /* Set address and pitch */
        wr32le(&displist[cnt++], 0xc0000000 | startAddress);
        wr32le(&displist[cnt++], 0xdeadbeef);
        wr32le(&displist[cnt++], fullWidth * bpp);

        /* LMB address */
        wr32le(&displist[cnt++], 0);

        /* Set PPF Scaler */
        wr32le(&displist[cnt++], (scale_x << 8) | (scaler << 30) | phase);
        wr32le(&displist[cnt++], (scale_y << 8) | (scaler << 30) | phase);
        wr32le(&displist[cnt++], 0); // Scratch written by HVS

        ULONG unicam_scaling = -1;

        if (config & UNICAMF_SMOOTHING)
        {
            VC4Base->vc4_UnicamKernel = BuddyAlloc(VC4Base, 11);
            unicam_scaling = BUDDY_OFFSET(VC4Base->vc4_UnicamKernel);

            wr32le(&displist[cnt++], unicam_scaling);
            wr32le(&displist[cnt++], unicam_scaling);
            wr32le(&displist[cnt++], unicam_scaling);
            wr32le(&displist[cnt++], unicam_scaling);
        }
        else
        {
            ULONG unity_kernel = BUDDY_OFFSET(VC4Base->vc4_UnityKernel);

            wr32le(&displist[cnt++], unity_kernel);
            wr32le(&displist[cnt++], unity_kernel);
            wr32le(&displist[cnt++], unity_kernel);
            wr32le(&displist[cnt++], unity_kernel);
        }

        /* Done */
        wr32le(&displist[cnt++], 0x80000000);

        /* Put scaling kernel here... */
        if (config & UNICAMF_SMOOTHING)
        {
            struct ExecBase *SysBase = VC4Base->vc4_SysBase;
            struct Library *MathIeeeSingBasBase = OpenLibrary("mathieeesingbas.library", 0);

            ULONG float_kernel_b = IEEESPDiv(
                IEEESPFlt(KernelB),
                0x447a0000  // 1000.0
            );

            ULONG float_kernel_c = IEEESPDiv(
                IEEESPFlt(KernelC),
                0x447a0000  // 1000.0
            );

            CloseLibrary(MathIeeeSingBasBase);

            compute_scaling_kernel((volatile uint32_t *)displist, unicam_scaling, float_kernel_b, float_kernel_c);
        }
    }
}
