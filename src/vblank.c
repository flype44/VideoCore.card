/*
    The vertical blank interrupt of the pixelvalve of HDMI0 for Picasso96.

    The interrupt is a SPI of the GIC-400, delivered to a 68k handler by gic400.library (which Emu68 has in its
    ROM). The pixelvalve raises it at the start of the vertical front porch, i.e. when the beam leaves the picture.
    The handler acknowledges it and runs the soft interrupt of rtg.library, which wakes the tasks waiting in
    WaitBOVP() / WaitTOF() and serves the double buffering.

    The pixelvalve only raises it while somebody needs it: rtg.library calls SetInterrupt() when a task starts to
    wait, and the handler switches it off again when nobody is left, so that there is no cost when idle.
*/

#include <exec/types.h>
#include <exec/interrupts.h>

#include <proto/exec.h>
#include <proto/devicetree.h>

#include <common/compiler.h>

#include "videocore.h"
#include "boardinfo.h"
#include "vblank.h"

/* Pixelvalve 2 drives HDMI0 on the BCM2711, /soc/pixelvalve@7e20a000 */
#define PV2_NODE            "/soc/pixelvalve@7e20a000"
#define PV2_BASE            (ARM_IO_BASE + 0x20a000)

#define PV_INTEN            0x24
#define PV_INTSTAT          0x28

#define PV_INT_VFP_START    (1UL << 7)      /* start of the vertical front porch: the picture is over */

#define GIC400_NAME         "gic400.library"

/* Reads of the pixelvalve while waiting for the first interrupt: about 20 ms are enough, this is a few frames */
#define VBLANK_TEST_READS   1000000

static inline ULONG PV_Read(ULONG reg)
{
    return LE32(*(volatile ULONG *)(PV2_BASE + reg));
}

static inline void PV_Write(ULONG reg, ULONG value)
{
    wr32le((volatile uint32_t *)(PV2_BASE + reg), value);
}

/* gic400.library. The three functions used here have the same numbers and registers since version 1.0 and return
   0 when it went well. Version 1.3 is the first one which finds the GIC through the interrupt-parent of the device
   tree and which has the error codes of today, the ROM of Emu68 has 1.4. Its LVOs, from the sfd:
   AddIntServerEx -30, RemIntServerEx -36 */
#define GIC400_MIN_VERSION  1
#define GIC400_MIN_REVISION 3

static LONG GIC_AddIntServerEx(struct Library *base, ULONG irq, UBYTE priority, BOOL edge, struct Interrupt *interrupt)
{
    register LONG res asm("d0") = (LONG)irq;
    register ULONG d1 asm("d1") = priority;
    register LONG d2 asm("d2") = edge;
    register struct Library *a6 asm("a6") = base;
    register struct Interrupt *a1 asm("a1") = interrupt;

    asm volatile("jsr -30(%%a6)" : "+r"(res), "+r"(d1), "+r"(d2) : "r"(a6), "r"(a1) : "a0", "cc", "memory");

    return res;
}

static LONG GIC_RemIntServerEx(struct Library *base, ULONG irq, struct Interrupt *interrupt)
{
    register LONG res asm("d0") = (LONG)irq;
    register struct Library *a6 asm("a6") = base;
    register struct Interrupt *a1 asm("a1") = interrupt;

    asm volatile("jsr -36(%%a6)" : "+r"(res) : "r"(a6), "r"(a1) : "d1", "a0", "cc", "memory");

    return res;
}

/* The GIC interrupt id of the pixelvalve, from its "interrupts" property: type, number, flags. A SPI (type 0)
   has the id number + 32. Returns 0 when it cannot be found. */
static ULONG VBlank_FindIrq(struct VideoCoreBase *VideoCoreBase)
{
    APTR DeviceTreeBase = VideoCoreBase->vc_DeviceTreeBase;
    ULONG irq = 0;
    APTR key = DT_OpenKey(PV2_NODE);

    if (key != NULL)
    {
        APTR prop = DT_FindProperty(key, "interrupts");

        if (prop != NULL && DT_GetPropLen(prop) >= 3 * sizeof(ULONG))
        {
            const ULONG *cells = DT_GetPropValue(prop);

            if (cells[0] == 0)
                irq = cells[1] + 32;
        }

        DT_CloseKey(key);
    }

    return irq;
}

/*
    Called by gic400.library at the interrupt of the pixelvalve, with the BoardInfo in a1 (the is_Data of the
    interrupt), in interrupt context: nothing which can wait.
*/
static ULONG VBlank_Interrupt(REGARG(struct BoardInfo *bi, "a1"))
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)bi->CardBase;
    struct ExecBase *SysBase = VideoCoreBase->vc_LibNode.ExecBase;
    struct VBlank *vblank = &VideoCoreBase->vc_VBlank;
    BOOL waiting;

    if ((PV_Read(PV_INTSTAT) & PV_INT_VFP_START) == 0)
        return 0;

    PV_Write(PV_INTSTAT, PV_INT_VFP_START);     /* write 1 to clear */
    vblank->Count++;

    /* Nobody waits and nothing is double buffered: switch the interrupt off until SetInterrupt() is called, or
       until a position of the sprite arrives. This comes first: a position queued after it has switched the
       interrupt on again, one queued before it is seen below. */
    waiting = !((bi->WaitQ.mlh_Head == NULL || bi->WaitQ.mlh_Head->mln_Succ == NULL) && bi->DoubleBufferList == NULL);
    if (!waiting)
        PV_Write(PV_INTEN, 0);

    /* The flag goes first: a word queued while this one is written sets it again */
    if (vblank->SpritePending)
    {
        vblank->SpritePending = FALSE;

        if (VideoCoreBase->vc_MouseCoord != NULL)
            wr32le(&VideoCoreBase->vc_MouseCoord[0], vblank->SpriteWord);
    }

    if (waiting)
        Cause(&bi->SoftInterrupt);

    return 1;
}

void VBlank_WriteSprite(struct VideoCoreBase *VideoCoreBase, ULONG word)
{
    struct VBlank *vblank = &VideoCoreBase->vc_VBlank;

    if (VideoCoreBase->vc_MouseCoord == NULL)
        return;

    if (vblank->Gic == NULL)
    {
        wr32le(&VideoCoreBase->vc_MouseCoord[0], word);
        return;
    }

    vblank->SpriteWord = word;
    vblank->SpritePending = TRUE;

    /* Switch the interrupt on, from a status which is clear: the next one is the next vertical blank */
    PV_Write(PV_INTSTAT, PV_INT_VFP_START);
    PV_Write(PV_INTEN, PV_INT_VFP_START);
}

/* rtg.library enables the interrupt when a task starts to wait and disables it at the screen switches.
   Returns the previous state. */
static BOOL VBlank_SetInterrupt(REGARG(struct BoardInfo *b, "a0"), REGARG(BOOL enable, "d0"))
{
    BOOL previous = (PV_Read(PV_INTEN) & PV_INT_VFP_START) != 0;

    if (enable)
    {
        PV_Write(PV_INTSTAT, PV_INT_VFP_START);
        PV_Write(PV_INTEN, PV_INT_VFP_START);
    }
    else
        PV_Write(PV_INTEN, 0);

    return previous;
}

BOOL VBlank_Init(struct BoardInfo *bi)
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)bi->CardBase;
    struct ExecBase *SysBase = VideoCoreBase->vc_LibNode.ExecBase;
    struct Library *gic;
    ULONG irq;

    /* The GIC-400 is the one of the Pi 4 */
    if (!VideoCoreBase->vc_VideoCore6)
        return FALSE;

    irq = VBlank_FindIrq(VideoCoreBase);
    if (irq == 0)
    {
        bug("[VC] VBlank: no interrupt in the device tree\n");
        return FALSE;
    }

    gic = OpenLibrary((CONST_STRPTR)GIC400_NAME, 0);
    if (gic == NULL)
    {
        bug("[VC] VBlank: no %s\n", (ULONG)GIC400_NAME);
        return FALSE;
    }

    if (gic->lib_Version < GIC400_MIN_VERSION ||
        (gic->lib_Version == GIC400_MIN_VERSION && gic->lib_Revision < GIC400_MIN_REVISION))
    {
        bug("[VC] VBlank: %s %ld.%ld is too old\n", (ULONG)GIC400_NAME, (ULONG)gic->lib_Version, (ULONG)gic->lib_Revision);
        CloseLibrary(gic);
        return FALSE;
    }

    /* Whoever else uses this interrupt has registered a handler and AddIntServerEx() below then refuses. What is
       left in the hardware by the previous session (Emu68 restarts without resetting the GIC and the pixelvalve)
       is not a user: the id may still be enabled in the GIC and PV_INTEN may still be armed. Start from scratch. */
    PV_Write(PV_INTEN, 0);
    PV_Write(PV_INTSTAT, PV_INT_VFP_START);

    bi->HardInterrupt.is_Data = bi;
    bi->HardInterrupt.is_Code = (void (*)())VBlank_Interrupt;

    /* Level triggered, priority 0. The pixelvalve raises nothing before SetInterrupt() */
    if (GIC_AddIntServerEx(gic, irq, 0, FALSE, &bi->HardInterrupt) != 0)
    {
        bug("[VC] VBlank: cannot register interrupt %ld\n", irq);
        CloseLibrary(gic);
        return FALSE;
    }

    /* Being registered does not mean that the interrupt arrives: let the pixelvalve raise one and see it come,
       the handler switches the source off by itself afterwards. A frame is 17 ms, the register reads are the clock
       of this wait and the limit is a few frames long. If nothing comes, undo everything and stay on the polling
       of WaitVerticalSync(). */
    VideoCoreBase->vc_VBlank.Count = 0;
    PV_Write(PV_INTSTAT, PV_INT_VFP_START);
    PV_Write(PV_INTEN, PV_INT_VFP_START);

    for (ULONG n = 0; n < VBLANK_TEST_READS && VideoCoreBase->vc_VBlank.Count == 0; n++)
        (void)PV_Read(PV_INTSTAT);

    PV_Write(PV_INTEN, 0);

    if (VideoCoreBase->vc_VBlank.Count == 0)
    {
        bug("[VC] VBlank: no interrupt %ld, staying on polling\n", irq);
        GIC_RemIntServerEx(gic, irq, &bi->HardInterrupt);
        CloseLibrary(gic);
        return FALSE;
    }

    /* The last step: tell rtg.library. gic400.library stays open for as long as the driver lives. */
    VideoCoreBase->vc_VBlank.Gic = gic;
    VideoCoreBase->vc_VBlank.Irq = irq;
    VideoCoreBase->vc_VBlank.Interrupt = &bi->HardInterrupt;

    bi->SetInterrupt = (void *)VBlank_SetInterrupt;
    bi->Flags |= BIF_VBLANKINTERRUPT;

    bug("[VC] VBlank: interrupt %ld of the pixelvalve of HDMI0\n", irq);

    return TRUE;
}

void VBlank_Exit(struct VideoCoreBase *VideoCoreBase)
{
    struct ExecBase *SysBase = VideoCoreBase->vc_LibNode.ExecBase;
    struct VBlank *vblank = &VideoCoreBase->vc_VBlank;

    if (vblank->Gic == NULL)
        return;

    PV_Write(PV_INTEN, 0);
    GIC_RemIntServerEx(vblank->Gic, vblank->Irq, vblank->Interrupt);
    CloseLibrary(vblank->Gic);

    vblank->Gic = NULL;
    vblank->Interrupt = NULL;
}
