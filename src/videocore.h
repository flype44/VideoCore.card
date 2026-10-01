#ifndef _VIDEOCORE_H
#define _VIDEOCORE_H

#include <exec/types.h>
#include <exec/libraries.h>

#include <dos/dos.h>
#include <intuition/intuitionbase.h>
#include <libraries/expansionbase.h>
#include <stdint.h>

#include "boardinfo.h"

#define STR(s) #s
#define XSTR(s) STR(s)

#define VC4CARD_VERSION  1
#define VC4CARD_REVISION 0
#define VC4CARD_PRIORITY 0

#define CLOCK_HZ        25000000

struct Size {
    UWORD width;
    UWORD height;
};

enum SwitchMode {
    None = 0,
    CTS,
    RTS,
    DTR,
    SEL,
    CSI,
};

struct VideoCoreBase {
    struct CardBase         vc_LibNode;
    BPTR                    vc_SegList;
    struct ExecBase *       vc_SysBase;
    struct ExpansionBase *  vc_ExpansionBase;
    struct DOSBase *        vc_DOSBase;
    struct IntuitionBase *  vc_IntuitionBase;
    APTR                    vc_DeviceTreeBase;
    APTR                    vc_UnicamBase;
    APTR                    vc_MailboxBase;
    APTR                    vc_HVS;
    APTR                    vc_DisplayList;    // HVS display list memory, set by the InitChip of the family
    void                  (*vc_ConstructUnicamDL)(struct VideoCoreBase *);  // display list of Unicam for an old unicam.resource, set by InitChip
    APTR                    vc_BuddyAllocator;
    APTR                    vc_MemBase;
    uint32_t                vc_MemSize;
    APTR                    vc_Unicambuffer;
    ULONG                   vc_UnicambufferSize;
    ULONG                   vc_UnicamDL;
    APTR                    vc_Framebuffer;
    uint32_t                vc_Pitch;
    uint16_t                vc_Enabled;
    uint8_t                 vc_VideoCore6;

    struct Size             vc_DispSize;

    APTR                    vc_VPU_CopyBlock;

    ULONG                   vc_ActivePlane;
    ULONG                   vc_FreePlane;

    ULONG                   vc_Scaler;
    UBYTE                   vc_Phase;
    ULONG                   vc_VertFreq;
    ULONG                   vc_Kernel_B; // FLOAT!
    ULONG                   vc_Kernel_C; // FLOAT!
    UBYTE                   vc_UseKernel;
    UBYTE                   vc_SpriteAlpha;
    UBYTE                   vc_SpriteVisible;

    ULONG                   vc_ScaleX;
    ULONG                   vc_ScaleY;

    WORD                    vc_MouseX;
    WORD                    vc_MouseY;
    WORD                    vc_OffsetX;
    WORD                    vc_OffsetY;

    volatile uint32_t *     vc_PlaneCoord;
    volatile uint32_t *     vc_PlaneScalerX;
    volatile uint32_t *     vc_PlaneScalerY;
    volatile uint32_t *     vc_MouseCoord;
    volatile uint32_t *     vc_MousePalette;
    volatile uint32_t *     vc_PIPCoord;
    volatile uint32_t *     vc_Kernel;

    ULONG                   vc_SpriteColors[3];

    struct MsgPort          *vc_Port;
    struct Task             *vc_Task;

    struct {
        APTR        lp_Addr;
        UWORD       lp_Width;
        WORD        lp_X;
        WORD        lp_Y;
        RGBFTYPE    lp_Format;
    }                       vc_LastPanning;
    
    UBYTE *                 vc_SpriteShape;
    enum SwitchMode         vc_SwitchMode;
    UBYTE                   vc_SwitchInverted;
    UBYTE                   vc_IntegerScaler;
    UBYTE                   vc_UnicamVisible;
    ULONG                   vc_ScalingKernel;
    ULONG                   vc_UnityKernel;
    ULONG                   vc_UnicamKernel;

    BOOL                    vc_UseDPMS;
    LONG                    vc_DisplayID;
    ULONG                   vc_DisplayNum;
};

void bug(const char * restrict format, ...);

/* Endian support */

static inline uint64_t LE64(uint64_t x) { return __builtin_bswap64(x); }
static inline uint32_t LE32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint16_t LE16(uint16_t x) { return __builtin_bswap16(x); }

static inline void wr32le(volatile uint32_t *addr, uint32_t value) {
    *addr = LE32(value);
    asm volatile("nop");
}

enum hvs_pixel_format {
    /* 8bpp */
    HVS_PIXEL_FORMAT_RGB332 = 0,
    /* 16bpp */
    HVS_PIXEL_FORMAT_RGBA4444 = 1,
    HVS_PIXEL_FORMAT_RGB555 = 2,
    HVS_PIXEL_FORMAT_RGBA5551 = 3,
    HVS_PIXEL_FORMAT_RGB565 = 4,
    /* 24bpp */
    HVS_PIXEL_FORMAT_RGB888 = 5,
    HVS_PIXEL_FORMAT_RGBA6666 = 6,
    /* 32bpp */
    HVS_PIXEL_FORMAT_RGBA8888 = 7,

    HVS_PIXEL_FORMAT_YCBCR_YUV420_3PLANE = 8,
    HVS_PIXEL_FORMAT_YCBCR_YUV420_2PLANE = 9,
    HVS_PIXEL_FORMAT_YCBCR_YUV422_3PLANE = 10,
    HVS_PIXEL_FORMAT_YCBCR_YUV422_2PLANE = 11,
    HVS_PIXEL_FORMAT_H264 = 12,
    HVS_PIXEL_FORMAT_PALETTE = 13,
    HVS_PIXEL_FORMAT_YUV444_RGB = 14,
    HVS_PIXEL_FORMAT_AYUV444_RGB = 15,
    HVS_PIXEL_FORMAT_RGBA1010102 = 16,
    HVS_PIXEL_FORMAT_YCBCR_10BIT = 17,
};

enum palette_type {
    PALETTE_NONE = 0, 
    PALETTE_1BPP = 1,
    PALETTE_2BPP = 2,
    PALETTE_4BPP = 3,
    PALETTE_8BPP = 4,
};

#define HVS_PIXEL_ORDER_RGBA                    0
#define HVS_PIXEL_ORDER_BGRA                    1
#define HVS_PIXEL_ORDER_ARGB                    2
#define HVS_PIXEL_ORDER_ABGR                    3

#define HVS_PIXEL_ORDER_XBRG                    0
#define HVS_PIXEL_ORDER_XRBG                    1
#define HVS_PIXEL_ORDER_XRGB                    2
#define HVS_PIXEL_ORDER_XBGR                    3

#define HVS_PIXEL_ORDER_XYCBCR			        0
#define HVS_PIXEL_ORDER_XYCRCB			        1
#define HVS_PIXEL_ORDER_YXCBCR			        2
#define HVS_PIXEL_ORDER_YXCRCB			        3

#define SCALER_CTL0_SCL_H_PPF_V_PPF             0
#define SCALER_CTL0_SCL_H_TPZ_V_PPF             1
#define SCALER_CTL0_SCL_H_PPF_V_TPZ             2
#define SCALER_CTL0_SCL_H_TPZ_V_TPZ             3
#define SCALER_CTL0_SCL_H_PPF_V_NONE            4
#define SCALER_CTL0_SCL_H_NONE_V_PPF            5
#define SCALER_CTL0_SCL_H_NONE_V_TPZ            6
#define SCALER_CTL0_SCL_H_TPZ_V_NONE            7

#define SCALER_DISPSTAT0                        0x00000048
#define SCALER_DISPSTAT1                        0x00000058
#define SCALER_DISPSTAT2                        0x00000068
#define SCALER_DISPSTATX_FRAME_COUNT_MASK       VC4_MASK(17, 12)
#define SCALER_DISPSTATX_FRAME_COUNT_SHIFT      12

#endif /* _VIDEOCORE_H */
