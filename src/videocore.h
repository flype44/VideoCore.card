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

/* The peripherals, at the addresses Emu68 maps them */
#define ARM_IO_BASE         0xf2000000
#define SYSTEM_TIMER_CLO    (ARM_IO_BASE + 0x3004)      /* 1 MHz counter */
#define HVS_BASE            (ARM_IO_BASE + 0x400000)

#define CLOCK_HZ        25000000

struct ChipFamily;

struct Size {
    UWORD width;
    UWORD height;
};

/* The vertical blank interrupt of a display, see vblank.c. One per display: HDMI0 and HDMI1 each have their own. */
struct VBlank {
    struct Library *       Gic;                 // gic400.library, open for as long as the interrupt is registered
    ULONG                  Irq;                 // GIC interrupt id
    struct Interrupt *     Interrupt;           // what was registered with it, to take it out again
    volatile ULONG         Count;               // interrupts seen
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
    APTR                    vc_DeviceTreeBase;
    APTR                    vc_UnicamBase;
    APTR                    vc_MailboxBase;
    APTR                    vc_HVS;
    const struct ChipFamily *vc_Family;         // VC4 or VC6, set by Chip_Init()
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
    struct VBlank           vc_VBlank;

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

#endif /* _VIDEOCORE_H */
