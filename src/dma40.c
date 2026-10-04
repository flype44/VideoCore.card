/*
    Copy of rectangles with a 40 bit DMA channel of the BCM2711 (/scb/dma@7e007b00, brcm,bcm2711-dma).

    A rectangle is a chain of control blocks, up to three per row: a head and a tail with 32 bit accesses, which bring the
    destination to a 16 byte boundary (a 128 bit write to an unaligned destination froze the machine in the video memory),
    and a body with 128 bit accesses and bursts of 16 (0.82 ns a byte, as the VPU). The last block raises an interrupt
    through gic400.library; the task which started the transfer sleeps in DMA40_Wait() until the handler signals it, the
    68k is free for the other tasks meanwhile (73 % of it, measured).

    The addresses of a 40 bit channel are physical (the control blocks too, divided by 32): no alias of the bus.
    Only one transfer runs at a time. Everything is checked at start (DMA40_Init) and the driver stays on the VPU and the
    CPU when something is missing.
*/

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/interrupts.h>
#include <proto/exec.h>

#include <common/compiler.h>

#include "emu68-vc4.h"
#include "mbox.h"
#include "dma40.h"

#define DMA_BASE            0xf2007000UL    /* peripherals window of Emu68: bus 0x7e007000; channel n at + 0x100 * n */
#define DMA_CS              0x00
#define DMA_CB              0x04            /* address of the control block >> 5, 0 = idle */
#define DMA_DEBUG           0x0c

#define CS_ACTIVE           (1UL << 0)
#define CS_END              (1UL << 1)
#define CS_INT              (1UL << 2)
#define CS_PROT             0x300UL         /* part of every write to CS */
#define CS_TRANSACTIONS     (1UL << 25)
#define CS_WAIT_FOR_WRITES  (1UL << 28)
#define CS_DISDEBUG         (1UL << 29)     /* set on the channels of the firmware */
#define DEBUG_RESET         (1UL << 23)

#define TI_INTEN            (1UL << 0)
#define INFO_INC            (1UL << 12)
#define INFO_128_BURST16    ((2UL << 13) | (15UL << 8))     /* 128 bit accesses, bursts of 16 */

#define DMA_TIMER()         (LE32(*(volatile ULONG *)0xf2003004UL))     /* 1 MHz */

#define DMA40_FIRST_CHAN    12      /* 11 and 14 belong to the firmware (DISDEBUG), the ARM has 12 and 13 */
#define DMA40_LAST_CHAN     13
#define DMA40_IRQ_BASE      110     /* GIC id = 110 + channel: SPI 0x5A, 0x5B of dma12, dma13 + 32 */
#define DMA40_MAX_ROWS      1280    /* up to three blocks a row */
#define DMA40_MAX_BLOCKS    (3 * DMA40_MAX_ROWS)
#define DMA40_SELFTEST      4096

static inline ULONG dma_rd(ULONG chan, ULONG off)
{
    return LE32(*(volatile ULONG *)(DMA_BASE + chan * 0x100 + off));
}

static inline void dma_wr(ULONG chan, ULONG off, ULONG value)
{
    *(volatile ULONG *)(DMA_BASE + chan * 0x100 + off) = LE32(value);
}

/* gic400.library: AddIntServerEx -30, RemIntServerEx -36, as in vblank.c
   (a call through an offset in C lost its sign with this compiler: inline asm) */
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

/* Called by gic400.library at the interrupt of the channel, as a subroutine, with the VC4Base in a1 (is_Data): it
   acknowledges (level triggered: an interrupt that is not acknowledged is a storm), counts, wakes the waiting task */
static ULONG DMA40_Interrupt(REGARG(struct VC4Base *VC4Base, "a1"))
{
    struct DMA40 *d = &VC4Base->vc4_DMA;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    ULONG cs = dma_rd(d->Chan, DMA_CS);

    if ((cs & CS_INT) == 0)
        return 0;

    dma_wr(d->Chan, DMA_CS, CS_INT | CS_END | CS_PROT);     /* writing 1 clears them */
    d->Count++;

    if (d->SigMask)
        Signal(d->Task, d->SigMask);

    return 0;
}

/* Stops the channel the way Linux does: pause, wait for the transactions, default CS, reset */
static void DMA40_Abort(struct DMA40 *d)
{
    ULONG timeout = 100;

    if (dma_rd(d->Chan, DMA_CB) == 0)
        return;

    dma_wr(d->Chan, DMA_CS, dma_rd(d->Chan, DMA_CS) & ~CS_ACTIVE);

    while ((dma_rd(d->Chan, DMA_CS) & CS_TRANSACTIONS) && --timeout)
        ;

    dma_wr(d->Chan, DMA_CS, CS_PROT);
    dma_wr(d->Chan, DMA_DEBUG, dma_rd(d->Chan, DMA_DEBUG) | DEBUG_RESET);
}

/* Builds the chain of the rectangle and starts the channel. `src`, `dst` are physical, `pitch` is the distance of the
   rows of both. The destination above the source: the rows, and the blocks of a row, go from the last to the first, or
   a block would overwrite source bytes that a later one still has to read (the caller has excluded an overlap inside
   a row). */
static void DMA40_Start(struct VC4Base *VC4Base, ULONG src, ULONG dst, ULONG width, ULONG height, ULONG pitch)
{
    struct DMA40 *d = &VC4Base->vc4_DMA;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    ULONG *cb = (ULONG *)d->CbPhys;
    BOOL reverse = dst > src;
    ULONG k = 0, y, part;

    for (y = 0; y < height; y++)
    {
        ULONG ry = reverse ? height - 1 - y : y;
        ULONG s = src + ry * pitch;
        ULONG t = dst + ry * pitch;
        ULONG head = (0 - t) & 15, body, tail, off[3], len[3];

        if (head > width)
            head = width;
        body = (width - head) & ~15UL;
        tail = width - head - body;

        off[0] = 0;             len[0] = head;
        off[1] = head;          len[1] = body;
        off[2] = head + body;   len[2] = tail;

        for (part = 0; part < 3; part++)
        {
            ULONG p = reverse ? 2 - part : part;
            ULONG *c = cb + k * 8;
            ULONG info = INFO_INC | (p == 1 ? INFO_128_BURST16 : 0);

            if (len[p] == 0)
                continue;

            c[0] = LE32(0);                         /* ti: the interrupt of the last block is set below */
            c[1] = LE32(s + off[p]);                /* src, then srci = upper address bits | transfer info */
            c[2] = LE32(info);
            c[3] = LE32(t + off[p]);                /* dst, dsti */
            c[4] = LE32(info);
            c[5] = LE32(len[p]);
            c[6] = LE32((d->CbPhys + (k + 1) * 32) >> 5);   /* next block */
            c[7] = 0;
            k++;
        }
    }

    cb[(k - 1) * 8 + 6] = 0;
    cb[(k - 1) * 8] |= LE32(TI_INTEN);
    CacheClearE(cb, 32 * k, CACRF_ClearD);

    /* The task which waits is the one which starts: its own signal. Without one DMA40_Wait() polls. */
    d->SigBit = AllocSignal(-1);
    d->Task = FindTask(NULL);
    d->SigMask = d->SigBit >= 0 ? 1UL << d->SigBit : 0;
    if (d->SigMask)
        SetSignal(0, d->SigMask);

    d->Target = d->Count + 1;
    d->Pending = TRUE;

    dma_wr(d->Chan, DMA_CS, CS_END | CS_PROT);
    dma_wr(d->Chan, DMA_CB, d->CbPhys >> 5);
    dma_wr(d->Chan, DMA_CS, CS_WAIT_FOR_WRITES | CS_ACTIVE | CS_PROT);
}

void DMA40_Wait(struct VC4Base *VC4Base)
{
    struct DMA40 *d = &VC4Base->vc4_DMA;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;

    if (!d->Pending)
        return;

    if (d->SigMask != 0 && FindTask(NULL) == d->Task)
    {
        /* Asleep until the handler signals: the others have the 68k */
        while (d->Count < d->Target)
            Wait(d->SigMask);

        FreeSignal(d->SigBit);
    }
    else
    {
        /* Another task than the one which started it, or no signal: poll, at most a second */
        ULONG t0 = DMA_TIMER();

        while (d->Count < d->Target && DMA_TIMER() - t0 < 1000000)
            ;

        if (d->Count < d->Target)
        {
            DMA40_Abort(d);
            d->Ok = FALSE;
        }
    }

    d->SigMask = 0;
    d->Pending = FALSE;
}

BOOL DMA40_CopyRect(struct VC4Base *VC4Base, ULONG src, ULONG dst, ULONG width, ULONG height, ULONG pitch)
{
    struct DMA40 *d = &VC4Base->vc4_DMA;
    LONG gap = (LONG)dst - (LONG)src;

    if (!d->Ok)
        return FALSE;

    if (width == 0 || height == 0 || height > DMA40_MAX_ROWS || ((width | src | dst | pitch) & 3) || pitch < width)
        return FALSE;

    /* Source and destination overlapping inside a row (a shift to the side by less than the width) is not for the DMA:
       a shift to the left by 8 bytes froze the machine in the video memory, and the order of the reads and writes of
       one row is not guaranteed. A move by whole rows (a scroll) never does. */
    if (gap > -(LONG)width && gap < (LONG)width)
        return FALSE;

    /* One transfer at a time */
    DMA40_Wait(VC4Base);

    if (!d->Ok)
        return FALSE;

    DMA40_Start(VC4Base, src, dst, width, height, pitch);

    return TRUE;
}

/* Starts everything and proves it: a channel that is idle and not the firmware's, gic400.library 1.3 or later, an
   interrupt id that nobody uses, memory for the control blocks, and a copy of 4 KB which must arrive intact and raise
   exactly one interrupt. Otherwise the driver stays on the VPU and the CPU, as before. */
void DMA40_Init(struct VC4Base *VC4Base)
{
    struct DMA40 *d = &VC4Base->vc4_DMA;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    ULONG chan, bus, t0, i;
    UBYTE *scratch;

    d->Ok = FALSE;

    if (!d->Enabled)
        return;

    for (chan = DMA40_FIRST_CHAN; chan <= DMA40_LAST_CHAN; chan++)
    {
        ULONG cs = dma_rd(chan, DMA_CS);

        if ((cs & (CS_DISDEBUG | CS_ACTIVE)) == 0 && dma_rd(chan, DMA_CB) == 0)
            break;
    }

    if (chan > DMA40_LAST_CHAN)
        return;

    d->Chan = chan;
    d->Irq = DMA40_IRQ_BASE + chan;

    /* END and INT of the session before, if any (it can have left them set), or the interrupt would be raised at once */
    dma_wr(chan, DMA_CS, CS_INT | CS_END | CS_PROT);

    d->Gic = OpenLibrary((CONST_STRPTR)"gic400.library", 0);
    if (d->Gic == NULL)
        return;

    /* Whoever else uses the interrupt has a handler registered and AddIntServerEx() below refuses. What an OS restart
       left behind is not a user: it does not reset the GIC nor the peripherals, so after a ColdReboot the id is still
       enabled in the hardware (the same as the vertical blank, see vblank.c). Do not look at its state. */
    if (d->Gic->lib_Version < 1 || (d->Gic->lib_Version == 1 && d->Gic->lib_Revision < 3))
    {
        CloseLibrary(d->Gic);
        d->Gic = NULL;
        return;
    }

    /* The control blocks and, behind them, 2 x 4 KB for the self test: coherent memory of the GPU, 32 byte aligned */
    bus = vc_alloc_locked(32 * DMA40_MAX_BLOCKS + 2 * DMA40_SELFTEST, 32, VC4Base);
    if (bus == 0 || (bus & 31))
    {
        CloseLibrary(d->Gic);
        d->Gic = NULL;
        return;
    }

    d->CbPhys = bus & 0x3fffffffUL;
    scratch = (UBYTE *)(d->CbPhys + 32 * DMA40_MAX_BLOCKS);

    d->Int.is_Node.ln_Type = NT_INTERRUPT;
    d->Int.is_Node.ln_Pri = 0;
    d->Int.is_Node.ln_Name = (char *)"VideoCore DMA";
    d->Int.is_Data = VC4Base;
    d->Int.is_Code = (void (*)())DMA40_Interrupt;
    d->Count = 0;
    d->Pending = FALSE;
    d->SigMask = 0;

    if (GIC_AddIntServerEx(d->Gic, d->Irq, 0, FALSE, &d->Int) != 0)
    {
        CloseLibrary(d->Gic);
        d->Gic = NULL;
        return;
    }

    /* The self test: 4 KB through the real path (one block, 128 bit), polled for the interrupt, a few ms at most */
    for (i = 0; i < DMA40_SELFTEST; i++)
    {
        scratch[i] = (UBYTE)(i * 7 + 3);
        scratch[DMA40_SELFTEST + i] = 0;
    }

    CacheClearE(scratch, 2 * DMA40_SELFTEST, CACRF_ClearD);

    DMA40_Start(VC4Base, (ULONG)scratch, (ULONG)scratch + DMA40_SELFTEST, DMA40_SELFTEST, 1, DMA40_SELFTEST);

    t0 = DMA_TIMER();
    while (d->Count < d->Target && DMA_TIMER() - t0 < 50000)
        ;

    /* DMA40_Start() took a signal of this task: give it back, the test polled */
    if (d->SigMask)
        FreeSignal(d->SigBit);
    d->SigMask = 0;
    d->Pending = FALSE;

    d->Ok = (d->Count == 1);

    if (d->Ok)
    {
        CacheClearE(scratch, 2 * DMA40_SELFTEST, CACRF_ClearD);

        for (i = 0; i < DMA40_SELFTEST; i++)
            if (scratch[DMA40_SELFTEST + i] != (UBYTE)(i * 7 + 3))
            {
                d->Ok = FALSE;
                break;
            }
    }
    else
        DMA40_Abort(d);

    if (!d->Ok)
    {
        GIC_RemIntServerEx(d->Gic, d->Irq, &d->Int);
        CloseLibrary(d->Gic);
        d->Gic = NULL;
    }
}
