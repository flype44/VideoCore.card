#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>

#include <proto/exec.h>
#include <proto/unicam.h>
#include <proto/mathieeesingbas.h>

#include <hardware/cia.h>
#include <common/compiler.h>
#include <resources/unicam.h>

// Shut off MathIEEE float injecting stuff
#define FLOAT ULONG

#include "videocore.h"
#include "vc6.h"
#include "boardinfo.h"
#include "mbox.h"
#include "buddyalloc.h"
#include "hvs.h"
#include "chip.h"

static void VC6_SetSpritePosition(REGARG(struct BoardInfo *b, "a0"), REGARG(WORD x, "d0"),
                                  REGARG(WORD y, "d1"), REGARG(RGBFTYPE format, "d7"));

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

/* The sprite plane: the hardware cursor, its palette and the end of the display list */
static int VC6_WriteSprite(struct BoardInfo *b, const struct Panning *pan, int cnt)
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;
    volatile uint32_t *displist = (uint32_t *)VideoCoreBase->vc_Family->DisplayList;

    int mouse_pos = cnt;
    cnt = mouse_pos + 1;

    VideoCoreBase->vc_MouseCoord = &displist[cnt];
    wr32le(&displist[cnt++], VC6_POS0_X(pan->SpriteX) |
                             VC6_POS0_Y(pan->SpriteY));
    wr32le(&displist[cnt++], (VC6_SCALER_POS2_ALPHA_MODE_PIPELINE << VC6_SCALER_POS2_ALPHA_MODE_SHIFT) | VC6_SCALER_POS2_ALPHA(0xfff));
    wr32le(&displist[cnt++], VC6_POS1_H(pan->SpriteHeight) | VC6_POS1_W(pan->SpriteWidth));
    wr32le(&displist[cnt++], VC6_POS2_H(MAXSPRITEHEIGHT) | VC6_POS2_W(MAXSPRITEWIDTH));
    wr32le(&displist[cnt++], 0xdeadbeef); // Scratch written by HVS

    wr32le(&displist[cnt++], 0xc0000000 | (ULONG)VideoCoreBase->vc_SpriteShape);
    wr32le(&displist[cnt++], 0xdeadbeef); // Scratch written by HVS

    // Write pitch
    wr32le(&displist[cnt++], MAXSPRITEWIDTH);

    int clut_off = cnt;
    wr32le(&displist[cnt++], 0xc0000000 | (HVS_PALETTE << 2));

    // LMB address - just behind LMB of main plane
    wr32le(&displist[cnt++], 16 * b->ModeInfo->Width / 2);

    // Write PPF Scaling
    wr32le(&displist[cnt++], (pan->Scale << 8) | VideoCoreBase->vc_Scaler | VideoCoreBase->vc_Phase);
    if (b->ModeInfo->Flags & GMF_DOUBLESCAN)
        wr32le(&displist[cnt++], ((pan->Scale << 7) & ~0xff) | VideoCoreBase->vc_Scaler | VideoCoreBase->vc_Phase);
    else
        wr32le(&displist[cnt++], (pan->Scale << 8) | VideoCoreBase->vc_Scaler | VideoCoreBase->vc_Phase);
    wr32le(&displist[cnt++], 0); // Scratch written by HVS

    // Write scaling kernel offset in dlist
    wr32le(&displist[cnt++], pan->SpriteKernel);
    wr32le(&displist[cnt++], pan->SpriteKernel);
    wr32le(&displist[cnt++], pan->SpriteKernel);
    wr32le(&displist[cnt++], pan->SpriteKernel);

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
    VideoCoreBase->vc_MousePalette = &displist[cnt];
    wr32le(&displist[cnt++], VideoCoreBase->vc_SpriteColors[0]);
    wr32le(&displist[cnt++], VideoCoreBase->vc_SpriteColors[1]);
    wr32le(&displist[cnt++], VideoCoreBase->vc_SpriteColors[2]);

    return cnt;
}

/* The main plane: the screen, scaled to the display unless it has the size of the display. Returns the
   index of the word after the plane. */
static int VC6_WritePlane(struct BoardInfo *b, const struct Panning *pan, int pos)
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;
    volatile uint32_t *displist = (uint32_t *)VideoCoreBase->vc_Family->DisplayList;
    int cnt = pos + 1;

    if (pan->Unity) {
        VideoCoreBase->vc_PlaneCoord = &displist[cnt];
        wr32le(&displist[cnt++], VC6_POS0_X(pan->OffsetX) | VC6_POS0_Y(pan->OffsetY));
        wr32le(&displist[cnt++], (VC6_SCALER_POS2_ALPHA_MODE_FIXED << VC6_SCALER_POS2_ALPHA_MODE_SHIFT) | VC6_SCALER_POS2_ALPHA(0xfff));
        wr32le(&displist[cnt++], VC6_POS2_H(b->ModeInfo->Height) | VC6_POS2_W(b->ModeInfo->Width));
        wr32le(&displist[cnt++], 0xdeadbeef);

        wr32le(&displist[cnt++], 0xc0000000 | pan->Address);
        wr32le(&displist[cnt++], 0xdeadbeef);
        wr32le(&displist[cnt++], pan->BytesPerRow);

        wr32le(&displist[pos],
            VC6_CONTROL_VALID
            | VC6_CONTROL_WORDS(cnt - pos)
            | VC6_CONTROL_UNITY
            | VC6_CONTROL_ALPHA_EXPAND
            | VC6_CONTROL_RGB_EXPAND
            | mode_table[pan->Format]);

        VideoCoreBase->vc_PlaneScalerX = NULL;
        VideoCoreBase->vc_PlaneScalerY = NULL;
    } else {
        VideoCoreBase->vc_PlaneCoord = &displist[cnt];
        wr32le(&displist[cnt++], VC6_POS0_X(pan->OffsetX) | VC6_POS0_Y(pan->OffsetY));
        wr32le(&displist[cnt++], (VC6_SCALER_POS2_ALPHA_MODE_FIXED << VC6_SCALER_POS2_ALPHA_MODE_SHIFT) | VC6_SCALER_POS2_ALPHA(0xfff));
        wr32le(&displist[cnt++], VC6_POS1_H(pan->Height) | VC6_POS1_W(pan->Width));
        wr32le(&displist[cnt++], VC6_POS2_H(b->ModeInfo->Height) | VC6_POS2_W(b->ModeInfo->Width));
        wr32le(&displist[cnt++], 0xdeadbeef); // Scratch written by HVS

        wr32le(&displist[cnt++], 0xc0000000 | pan->Address);
        wr32le(&displist[cnt++], 0xdeadbeef); // Scratch written by HVS

        // Write pitch
        wr32le(&displist[cnt++], pan->BytesPerRow);

        // Palette mode - offset of palette placed in dlist
        if (pan->Format == RGBFB_CLUT) {
            wr32le(&displist[cnt++], 0xc0000000 | (HVS_PALETTE << 2));
        }

        // LMB address
        wr32le(&displist[cnt++], 0);

        // Write PPF Scaling
        VideoCoreBase->vc_PlaneScalerX = &displist[cnt];
        VideoCoreBase->vc_PlaneScalerY = &displist[cnt+1];

        wr32le(&displist[cnt++], (pan->Scale << 8) | VideoCoreBase->vc_Scaler | VideoCoreBase->vc_Phase);
        if (b->ModeInfo->Flags & GMF_DOUBLESCAN)
            wr32le(&displist[cnt++], ((pan->Scale << 7) & ~0xff) | VideoCoreBase->vc_Scaler | VideoCoreBase->vc_Phase);
        else
            wr32le(&displist[cnt++], (pan->Scale << 8) | VideoCoreBase->vc_Scaler | VideoCoreBase->vc_Phase);
        wr32le(&displist[cnt++], 0); // Scratch written by HVS

        // Write scaling kernel offset in dlist
        VideoCoreBase->vc_Kernel = &displist[cnt];
        wr32le(&displist[cnt++], pan->Kernel);
        wr32le(&displist[cnt++], pan->Kernel);
        wr32le(&displist[cnt++], pan->Kernel);
        wr32le(&displist[cnt++], pan->Kernel);

        wr32le(&displist[pos],
            VC6_CONTROL_VALID             |
            VC6_CONTROL_WORDS(cnt-pos)    |
            VC6_CONTROL_ALPHA_EXPAND      |
            VC6_CONTROL_RGB_EXPAND        |
            mode_table[pan->Format]
        );
    }

    return cnt;
}

//static int display_enabled = 0;
// None of these five really have to do anything.
static void VC6_SetSprite(REGARG(struct BoardInfo *b, "a0"), REGARG(BOOL enable, "d0"), REGARG(RGBFTYPE format, "d7"))
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;

    VideoCoreBase->vc_SpriteVisible = enable;

    if (enable) {
        LONG _x;
        LONG _y;

        if (VideoCoreBase->vc_ScaleX)
            _x = 0x10000 * VideoCoreBase->vc_MouseX / VideoCoreBase->vc_ScaleX;
        else
            _x = VideoCoreBase->vc_MouseX;

        if (VideoCoreBase->vc_ScaleY)
            _y = 0x10000 * VideoCoreBase->vc_MouseY / VideoCoreBase->vc_ScaleY;
        else
            _y = VideoCoreBase->vc_MouseY;

        if (VideoCoreBase->vc_MouseCoord) {
            wr32le(&VideoCoreBase->vc_MouseCoord[0], VC6_POS0_X(_x) | VC6_POS0_Y(_y));
        }
    }
    else
    {
        if (VideoCoreBase->vc_MouseCoord) {
            wr32le(&VideoCoreBase->vc_MouseCoord[0], VC6_POS0_X(-1) | VC6_POS0_Y(-1));
        }
    }
}

static void VC6_SetSpritePosition(REGARG(struct BoardInfo *b, "a0"), REGARG(WORD x, "d0"), 
                           REGARG(WORD y, "d1"), REGARG(RGBFTYPE format, "d7"))
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;

/*
    x = b->MouseX - b->XOffset;
    y = b->MouseY - b->YOffset;
*/

    VideoCoreBase->vc_MouseX = x;
    VideoCoreBase->vc_MouseY = y;

    x -= VideoCoreBase->vc_LastPanning.lp_X;
    y -= VideoCoreBase->vc_LastPanning.lp_Y;

    LONG _x;
    LONG _y;

    if (VideoCoreBase->vc_ScaleX)
        _x = 0x10000 * x / VideoCoreBase->vc_ScaleX;
    else
        _x = x;

    if (VideoCoreBase->vc_ScaleY)
        _y = 0x10000 * y / VideoCoreBase->vc_ScaleY;
    else
        _y = y;

    _x += VideoCoreBase->vc_OffsetX;
    _y += VideoCoreBase->vc_OffsetY;

    if (VideoCoreBase->vc_MouseCoord) {   
        wr32le(&VideoCoreBase->vc_MouseCoord[0], VC6_POS0_X(_x) | VC6_POS0_Y(_y));
    }
}

/* Unicam DisplayList */
static void VC6_ConstructUnicamDL(struct VideoCoreBase *VideoCoreBase)
{
    APTR UnicamBase = VideoCoreBase->vc_UnicamBase;
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

    volatile uint32_t *displist = (uint32_t *)VideoCoreBase->vc_Family->DisplayList;

    if (crop_w == VideoCoreBase->vc_DispSize.width &&
        crop_h == VideoCoreBase->vc_DispSize.height && aspect == 1000)
    {
        unity = 1;
    }
    else
    {
        scale_x = 0x10000 * ((crop_w * aspect) / 1000) / VideoCoreBase->vc_DispSize.width;
        scale_y = 0x10000 * crop_h / VideoCoreBase->vc_DispSize.height;

        recip_x = 0xffffffff / scale_x;
        recip_y = 0xffffffff / scale_y;

        // Select larger scaling factor from X and Y, but it need to fit
        if (((0x10000 * crop_h) / scale_x) > VideoCoreBase->vc_DispSize.height) {
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

        offset_x = (VideoCoreBase->vc_DispSize.width - calc_width) >> 1;
        offset_y = (VideoCoreBase->vc_DispSize.height - calc_height) >> 1;
    }

    ULONG startAddress = (ULONG)VideoCoreBase->vc_Unicambuffer;
    startAddress += crop_x * bpp;
    startAddress += crop_y * fullWidth * bpp;

    if (unity)
    {
        /* Unity scaling is simple, reserve less space for display list */
        cnt -= 16;

        VideoCoreBase->vc_UnicamDL = cnt;

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
       
        VideoCoreBase->vc_UnicamDL = cnt;

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
            VideoCoreBase->vc_UnicamKernel = BuddyAlloc(VideoCoreBase, 11);
            unicam_scaling = BUDDY_OFFSET(VideoCoreBase->vc_UnicamKernel);

            wr32le(&displist[cnt++], unicam_scaling);
            wr32le(&displist[cnt++], unicam_scaling);
            wr32le(&displist[cnt++], unicam_scaling);
            wr32le(&displist[cnt++], unicam_scaling);
        }
        else
        {
            ULONG unity_kernel = BUDDY_OFFSET(VideoCoreBase->vc_UnityKernel);

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
            struct ExecBase *SysBase = VideoCoreBase->vc_LibNode.ExecBase;
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

/* Fills the BoardInfo with the functions of VideoCore 6 */
/* What makes VC6 itself */
const struct ChipFamily VC6_Family = {
    "VC6",
    (APTR)VC6_DISPLAY_LIST,
    8 + 20 + 4 + 8,
    2*20 + 4 + 8,
    5,
    6,
    0x10000,
    VC6_ConstructUnicamDL,
    VC6_WritePlane,
    VC6_WriteSprite,
    VC6_SetSprite,
    VC6_SetSpritePosition
};
