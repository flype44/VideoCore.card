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

static inline ULONG PV_Read(ULONG reg)
{
    return LE32(*(volatile ULONG *)(PV2_BASE + reg));
}

static inline void PV_Write(ULONG reg, ULONG value)
{
    wr32le((volatile uint32_t *)(PV2_BASE + reg), value);
}

/* gic400.library, version 1.4 of the ROM of Emu68 has what is needed here. Its LVOs, from the sfd:
   AddIntServerEx -30, RemIntServerEx -36, GetIntStatus -42 */
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

static LONG GIC_GetIntStatus(struct Library *base, ULONG irq, BOOL *pending, BOOL *active, BOOL *enabled)
{
    register LONG res asm("d0") = (LONG)irq;
    register struct Library *a6 asm("a6") = base;
    register BOOL *a1 asm("a1") = pending;
    register BOOL *a2 asm("a2") = active;
    register BOOL *a3 asm("a3") = enabled;

    asm volatile("jsr -42(%%a6)" : "+r"(res) : "r"(a6), "r"(a1), "r"(a2), "r"(a3) : "d1", "a0", "cc", "memory");

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

    if ((PV_Read(PV_INTSTAT) & PV_INT_VFP_START) == 0)
        return 0;

    PV_Write(PV_INTSTAT, PV_INT_VFP_START);     /* write 1 to clear */

    /* Nobody waits and nothing is double buffered: switch the interrupt off until SetInterrupt() is called */
    if (bi->WaitQ.mlh_Head->mln_Succ == NULL && bi->DoubleBufferList == NULL)
        PV_Write(PV_INTEN, 0);
    else
        Cause(&bi->SoftInterrupt);

    return 1;
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
    BOOL pending = FALSE, active = FALSE, enabled = FALSE;
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

    /* Nobody else may use this interrupt */
    GIC_GetIntStatus(gic, irq, &pending, &active, &enabled);
    if (enabled || PV_Read(PV_INTEN) != 0)
    {
        bug("[VC] VBlank: interrupt %ld is in use\n", irq);
        CloseLibrary(gic);
        return FALSE;
    }

    bi->HardInterrupt.is_Data = bi;
    bi->HardInterrupt.is_Code = (void (*)())VBlank_Interrupt;

    /* Level triggered, priority 0. The pixelvalve raises nothing before SetInterrupt() */
    if (GIC_AddIntServerEx(gic, irq, 0, FALSE, &bi->HardInterrupt) != 0)
    {
        bug("[VC] VBlank: cannot register interrupt %ld\n", irq);
        CloseLibrary(gic);
        return FALSE;
    }

    bi->SetInterrupt = (void *)VBlank_SetInterrupt;
    bi->Flags |= BIF_VBLANKINTERRUPT;

    bug("[VC] VBlank: interrupt %ld of the pixelvalve of HDMI0\n", irq);

    /* gic400.library stays open for as long as the driver lives */
    return TRUE;
}
