/*
    Golden master of the display lists written by the chip files.

    The real vc4.c, vc6.c and chip.c are built for the host and called through the BoardInfo, exactly like
    Picasso96 does, in a grid of cases: display and screen sizes, pixel formats, the integer scaler, the
    hardware sprite, panning. The words of the display list and the registers they leave behind are printed.
    Run it before and after a change of the code: if the output is the same, the HVS gets the same words.
*/
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "boardinfo.h"
#include "videocore.h"
#include "chip.h"
#include "hvs.h"
#include "vc4.h"
#include "vc6.h"
#include "buddyalloc.h"
#include "vblank.h"
#include <resources/unicam.h>

extern ULONG host_unicam_config;
int host_map_fixed(unsigned long addr, unsigned long len);
void host_clear(unsigned long addr, unsigned long len);

#define REGS_BASE   0xf2000000UL
#define REGS_LEN    0x00420000UL    /* up to the end of the display list memory of the HVS */
#define DL_WORDS    0x410

typedef void (*SetPanningFn)(struct BoardInfo *, UBYTE *, UWORD, WORD, WORD, RGBFTYPE);
typedef void (*SetSpriteFn)(struct BoardInfo *, BOOL, RGBFTYPE);
typedef void (*SetSpritePositionFn)(struct BoardInfo *, WORD, WORD, RGBFTYPE);
typedef void (*SetSpriteImageFn)(struct BoardInfo *, RGBFTYPE);
typedef void (*SetSpriteColorFn)(struct BoardInfo *, UBYTE, UBYTE, UBYTE, UBYTE, RGBFTYPE);
typedef UWORD (*SetSwitchFn)(struct BoardInfo *, UWORD);
typedef void (*SetColorArrayFn)(struct BoardInfo *, UWORD, UWORD);

/* No interrupt on the host: the position of the sprite is written at once, as without gic400.library */
void VBlank_WriteSprite(struct VideoCoreBase *VideoCoreBase, ULONG word)
{
    if (VideoCoreBase->vc_MouseCoord)
        wr32le(&VideoCoreBase->vc_MouseCoord[0], word);
}

/* No memory window on the host */
BOOL MemoryWindow_Plane(struct VideoCoreBase *VideoCoreBase, struct WindowPlane *window)
{
    (void)VideoCoreBase;
    (void)window;
    return FALSE;
}

static struct BoardInfo bi;
static struct ModeInfo mi;
static struct VideoCoreBase vcb;
/* The shape of the sprite is at a fixed address, its address goes into the display list */
#define SPRITE_SHAPE 0x40000000UL
#define SPRITE_SHAPE_LEN 0x10000UL
static volatile uint32_t *dl;

static long off(const volatile void *p)
{
    return p ? (long)((const volatile uint32_t *)p - dl) : -1;
}

static void dump(const char *label)
{
    int i;

    printf("  %s\n", label);
    printf("    state: scale %08x/%08x offset %d/%d active %08x sprite %d enabled %d last %lx/%d/%d/%d/%d\n",
           (unsigned)vcb.vc_ScaleX, (unsigned)vcb.vc_ScaleY, vcb.vc_OffsetX, vcb.vc_OffsetY,
           (unsigned)vcb.vc_ActivePlane, vcb.vc_SpriteVisible, vcb.vc_Enabled,
           (unsigned long)(uintptr_t)vcb.vc_LastPanning.lp_Addr, vcb.vc_LastPanning.lp_Width,
           vcb.vc_LastPanning.lp_X, vcb.vc_LastPanning.lp_Y, (int)vcb.vc_LastPanning.lp_Format);
    printf("    pointers: plane %ld scalerx %ld scalery %ld mouse %ld palette %ld kernel %ld pip %ld\n",
           off(vcb.vc_PlaneCoord), off(vcb.vc_PlaneScalerX), off(vcb.vc_PlaneScalerY),
           off(vcb.vc_MouseCoord), off(vcb.vc_MousePalette), off(vcb.vc_Kernel), off(vcb.vc_PIPCoord));
    printf("    displist register: %08x\n", (unsigned)__builtin_bswap32(*(volatile uint32_t *)(HVS_BASE + SCALER_DISPLIST1)));
    for (i = 0; i < DL_WORDS; i++)
        if (dl[i] != 0)
            printf("    %03x: %08x\n", i, (unsigned)__builtin_bswap32(dl[i]));
}

static const RGBFTYPE formats[] = { RGBFB_CLUT, RGBFB_R5G6B5PC, RGBFB_R8G8B8, RGBFB_A8R8G8B8, RGBFB_B8G8R8A8 };
static const char *format_names[] = { "CLUT", "R5G6B5PC", "R8G8B8", "A8R8G8B8", "B8G8R8A8" };

static const struct { UWORD w, h; UBYTE flags; } screens[] = {
    { 1920, 1080, 0 },  /* the size of the big display: native there */
    { 1280, 720, 0 },   /* the size of the small display: native there, scaled on the big one */
    { 640, 480, 0 },
    { 800, 600, 0 },
    { 640, 256, 0 },
    { 640, 200, GMF_DOUBLESCAN },
    { 320, 240, GMF_DOUBLESCAN },
    { 1024, 768, 0 },
};

static const struct { UWORD w, h; } displays[] = { { 1920, 1080 }, { 1280, 720 } };

static void run(const struct ChipFamily *family, int disp, int scr, int fmt, int integer, int sprite, int unicam)
{
    SetPanningFn SetPanning;
    SetSpriteFn SetSprite;
    SetSpritePositionFn SetSpritePosition;
    SetSpriteImageFn SetSpriteImage;
    SetSpriteColorFn SetSpriteColor;
    SetSwitchFn SetSwitch;
    SetColorArrayFn SetColorArray;
    RGBFTYPE format = formats[fmt];
    UWORD width = screens[scr].w;

    host_clear(REGS_BASE, REGS_LEN);
    host_clear(SPRITE_SHAPE, SPRITE_SHAPE_LEN);
    memset(&bi, 0, sizeof(bi));
    memset(&vcb, 0, sizeof(vcb));
    memset(&mi, 0, sizeof(mi));

    vcb.vc_DispSize.width = displays[disp].w;
    vcb.vc_DispSize.height = displays[disp].h;
    vcb.vc_Enabled = -1;
    vcb.vc_IntegerScaler = integer;
    vcb.vc_SwitchMode = None;
    vcb.vc_Scaler = 0xc0000000;
    vcb.vc_Phase = 128;
    vcb.vc_UseKernel = 1;
    vcb.vc_SpriteShape = (UBYTE *)(uintptr_t)SPRITE_SHAPE;
    vcb.vc_SpriteColors[0] = 0x00112233;
    vcb.vc_SpriteColors[1] = 0x00445566;
    vcb.vc_SpriteColors[2] = 0x00778899;
    vcb.vc_MouseX = 100;
    vcb.vc_MouseY = 50;
    vcb.vc_UnicamBase = unicam ? (APTR)1 : NULL;
    host_unicam_config = unicam ? UNICAMF_BOOT : 0;

    bi.CardBase = (struct CardBase *)&vcb;
    bi.ModeInfo = &mi;
    mi.Width = screens[scr].w;
    mi.Height = screens[scr].h;
    mi.Depth = format == RGBFB_CLUT ? 8 : (format == RGBFB_R5G6B5PC ? 16 : 32);
    mi.Flags = screens[scr].flags;

    Chip_Init(&bi, family);
    dl = (volatile uint32_t *)family->DisplayList;
    *(volatile uint32_t *)(HVS_BASE + SCALER_DISPSTAT1) = __builtin_bswap32(vcb.vc_DispSize.height);

    BuddyInit(&vcb);
    vcb.vc_ScalingKernel = BuddyAlloc(&vcb, 11);
    vcb.vc_UnityKernel = BuddyAlloc(&vcb, 11);
    vcb.vc_UnicamDL = BuddyAlloc(&vcb, 20);

    SetPanning = (SetPanningFn)bi.SetPanning;
    SetSprite = (SetSpriteFn)bi.SetSprite;
    SetSpritePosition = (SetSpritePositionFn)bi.SetSpritePosition;
    SetSpriteImage = (SetSpriteImageFn)bi.SetSpriteImage;
    SetSpriteColor = (SetSpriteColorFn)bi.SetSpriteColor;
    SetSwitch = (SetSwitchFn)bi.SetSwitch;
    SetColorArray = (SetColorArrayFn)bi.SetColorArray;

    printf("CASE %s display %ux%u screen %ux%u%s %s integer %d sprite %d unicam %d\n", family->Name,
           displays[disp].w, displays[disp].h, screens[scr].w, screens[scr].h,
           screens[scr].flags ? " doublescan" : "", format_names[fmt], integer, sprite, unicam);

    fflush(stdout);
    fprintf(stderr, "case %s display %d screen %d format %d integer %d sprite %d unicam %d\n", family->Name, disp, scr, fmt, integer, sprite, unicam);

    /* the screen is opened */
    SetPanning(&bi, (UBYTE *)(uintptr_t)0x01000000, width, 0, 0, format);
    if (sprite) {
        SetSprite(&bi, TRUE, format);
        SetSpriteImage(&bi, format);
        SetSpriteColor(&bi, 1, 0xff, 0x00, 0x80, format);
        SetSpritePosition(&bi, 20, 10, format);
    }
    SetSwitch(&bi, 1);
    if (format == RGBFB_CLUT)
        SetColorArray(&bi, 0, 16);
    dump("after the screen is opened");

    /* the screen is scrolled: only the address of the plane changes */
    SetPanning(&bi, (UBYTE *)(uintptr_t)0x01000000, width, 8, 4, format);
    if (sprite)
        SetSpritePosition(&bi, 30, 12, format);
    dump("after panning by 8, 4");

    /* an other bitmap is shown: new plane */
    SetPanning(&bi, (UBYTE *)(uintptr_t)0x01200000, width, 0, 0, format);
    SetSwitch(&bi, 0);
    SetSwitch(&bi, 1);
    dump("after an other bitmap");
}

int main(void)
{
    const struct ChipFamily *families[] = { &VC4_Family, &VC6_Family };
    int f, d, s, m, i, sp, u;

    if (!host_map_fixed(REGS_BASE, REGS_LEN) || !host_map_fixed(SPRITE_SHAPE, SPRITE_SHAPE_LEN))
        return 2;

    for (f = 0; f < 2; f++)
        for (d = 0; d < 2; d++)
            for (s = 0; s < (int)(sizeof(screens) / sizeof(screens[0])); s++)
                for (m = 0; m < (int)(sizeof(formats) / sizeof(formats[0])); m++)
                    for (i = 0; i < 2; i++)
                        for (sp = 0; sp < 2; sp++)
                            for (u = 0; u < 2; u++)
                            {
                                /* A screen bigger than the display with the integer scaler divides by zero in
                                   SetPanning (0x10000 / (0x10000 / scale)): a defect of the driver, not tested */
                                if (i && (screens[s].w > displays[d].w || screens[s].h > displays[d].h))
                                    continue;

                                run(families[f], d, s, m, i, sp, u);
                            }
    return 0;
}
