/*
    vc4vbl - probe of the vertical blank interrupt of the VideoCore IV (Raspberry Pi 3) under Emu68.

    It answers the questions which a vertical blank interrupt in VideoCore.card for the VC4 depends on, without
    the driver and without a reboot:

      (no keyword)  read only: the board, the libraries, the device tree nodes and the registers of the
                    pixelvalve of HDMI (pixelvalve2), the HVS, the ARM interrupt controller and the SMI.
      WATCH         passive: for SECS seconds, counts the frames of the HVS and the VFP_START events of the
                    pixelvalve, first with PV_INTEN as found, then with PV_INTEN armed while the interrupt
                    stays disabled in the ARM controller (no interrupt can reach the ARM). Also watches the
                    vertical blank signal of the firmware in the SMI.
      IRQ           active: a real interrupt. A server on INTB_EXTER, GPU IRQ 42 enabled in the ARM controller,
                    PV_INTEN armed, for SECS seconds; then everything is put back as it was. A limit on the
                    number of calls switches the source off if it runs away.

    Template: SECS/N,WATCH/S,IRQ/S    (SECS: 1 to 10, default 2)

    Raspberry Pi 3 only (brcm,bcm2837): on any other board it stops after the identification.

    The output is plain text, "key: value" lines, to be sent back as it is.
*/

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/interrupts.h>
#include <exec/nodes.h>
#include <hardware/intbits.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/devicetree.h>

#include <stdint.h>

static const char version[] __attribute__((used)) = "$VER: vc4vbl 1.1 (23.10.2026)";

APTR DeviceTreeBase;

/* Pixelvalve (vc4_regs.h of the Linux vc4 driver) */
#define PV_CONTROL          0x00
#define PV_INTEN            0x24
#define PV_INTSTAT          0x28
#define PV_STAT             0x2c
#define PV_INT_VFP_START    (1UL << 7)

/* HVS */
#define SCALER_DISPCTRL     0x00
#define SCALER_DISPSTAT1    0x58

/* ARM interrupt controller, brcm,bcm2835-armctrl-ic (irq-bcm2835.c): pending, enable and disable per bank */
#define ARMCTRL_PENDING_BASIC   0x00
#define ARMCTRL_PENDING_1       0x04
#define ARMCTRL_PENDING_2       0x08
#define ARMCTRL_FIQ             0x0c
#define ARMCTRL_ENABLE_1        0x10
#define ARMCTRL_ENABLE_2        0x14
#define ARMCTRL_ENABLE_BASIC    0x18
#define ARMCTRL_DISABLE_2       0x20

/* Local interrupt controller of the BCM2836/2837 */
#define LOCAL_GPU_ROUTING       0x0c
#define LOCAL_CORE0_IRQ_SOURCE  0x60

/* SMI, which the firmware uses to signal the vertical blank (vc4_firmware_kms.c) */
#define SMICS                   0x00
#define SMIDSW0                 0x14
#define SMICS_INTERRUPTS        ((1UL << 9) | (1UL << 10) | (1UL << 11))

/* System timer, 1 MHz */
#define TIMER_CLO               0x04

#define MAX_SECS                10
#define CALLS_PER_SECOND_LIMIT  2000    /* the IRQ test switches the source off beyond this */

/* dos.library Printf() takes a STRPTR (unsigned char *): one cast here instead of one per call */
#define Print(fmt, ...) Printf((CONST_STRPTR)(fmt), ##__VA_ARGS__)

static inline ULONG LE32(ULONG x) { return __builtin_bswap32(x); }

static inline ULONG Rd(ULONG addr)
{
    return LE32(*(volatile ULONG *)addr);
}

static inline void Wr(ULONG addr, ULONG value)
{
    *(volatile ULONG *)addr = LE32(value);
    asm volatile("nop");
}

/* Addresses of the blocks, translated to the 68k view (0 = not found) */
struct Blocks
{
    ULONG pv2;
    ULONG hvs;
    ULONG armctrl;
    ULONG local;
    ULONG smi;
    ULONG timer;
    ULONG pv2_irq_bank;
    ULONG pv2_irq_bit;
};

/*
    Device tree
*/

/* Opens the node named by a label of /__symbols__ (e.g. "pixelvalve2"), NULL when there is none */
static APTR OpenSymbol(CONST_STRPTR label)
{
    APTR symbols = DT_OpenKey((CONST_STRPTR)"/__symbols__");
    APTR key = NULL;

    if (symbols != NULL)
    {
        APTR prop = DT_FindProperty(symbols, label);

        if (prop != NULL)
            key = DT_OpenKey((CONST_STRPTR)DT_GetPropValue(prop));

        DT_CloseKey(symbols);
    }

    return key;
}

static ULONG CellsOf(APTR key, CONST_STRPTR name, ULONG fallback)
{
    APTR prop = key ? DT_FindProperty(key, name) : NULL;

    return prop ? *(const ULONG *)DT_GetPropValue(prop) : fallback;
}

/*
    Translates a bus address of /soc to the address the 68k sees. Emu68 maps every range of /soc from
    0xf2000000 up and writes the new CPU address back into "ranges", so the translation uses the tree as it is.
*/
static ULONG BusToCpu(ULONG bus)
{
    APTR root = DT_OpenKey((CONST_STRPTR)"/");
    APTR soc = DT_OpenKey((CONST_STRPTR)"/soc");
    ULONG cpu = 0;

    if (root != NULL && soc != NULL)
    {
        ULONG child_cells = CellsOf(soc, (CONST_STRPTR)"#address-cells", 1);
        ULONG parent_cells = CellsOf(root, (CONST_STRPTR)"#address-cells", 1);
        ULONG size_cells = CellsOf(soc, (CONST_STRPTR)"#size-cells", 1);
        ULONG entry = child_cells + parent_cells + size_cells;
        APTR prop = DT_FindProperty(soc, (CONST_STRPTR)"ranges");

        if (prop != NULL && entry != 0)
        {
            const ULONG *r = DT_GetPropValue(prop);
            ULONG count = DT_GetPropLen(prop) / (4 * entry);

            for (ULONG i = 0; i < count; i++, r += entry)
            {
                ULONG r_bus = r[child_cells - 1];
                ULONG r_cpu = r[child_cells + parent_cells - 1];
                ULONG r_size = r[entry - 1];

                if (bus >= r_bus && bus - r_bus < r_size)
                {
                    cpu = r_cpu + (bus - r_bus);
                    break;
                }
            }
        }
    }

    if (soc != NULL)
        DT_CloseKey(soc);
    if (root != NULL)
        DT_CloseKey(root);

    return cpu;
}

static void PrintString(CONST_STRPTR label, APTR key, CONST_STRPTR name)
{
    APTR prop = key ? DT_FindProperty(key, name) : NULL;

    if (prop != NULL)
        Print("%s: %s\n", (LONG)label, (LONG)DT_GetPropValue(prop));
    else
        Print("%s: (none)\n", (LONG)label);
}

static void PrintCells(CONST_STRPTR label, APTR key, CONST_STRPTR name)
{
    APTR prop = key ? DT_FindProperty(key, name) : NULL;

    Print("%s:", (LONG)label);
    if (prop != NULL)
    {
        const ULONG *c = DT_GetPropValue(prop);
        ULONG n = DT_GetPropLen(prop) / 4;

        for (ULONG i = 0; i < n && i < 12; i++)
            Print(" %08lx", c[i]);
    }
    else
        Print(" (none)");
    Print("\n");
}

/*
    Looks a block up by its label, prints its node and returns its 68k address. Without the label, the bus
    address of the block on the BCM2835/2837 is used (0 = none).
*/
static ULONG FindBlock(CONST_STRPTR label, ULONG fallback_bus, ULONG *irq_bank, ULONG *irq_bit)
{
    APTR key = OpenSymbol(label);
    ULONG cpu = 0;

    Print("node.%s:", (LONG)label);
    if (key == NULL)
    {
        cpu = fallback_bus ? BusToCpu(fallback_bus) : 0;
        Print(" (no label) address: bus %08lx -> 68k %08lx\n", fallback_bus, cpu);
        return cpu;
    }
    Print(" %s\n", (LONG)DT_GetKeyName(key));

    Print("  ");
    PrintString((CONST_STRPTR)"compatible", key, (CONST_STRPTR)"compatible");
    Print("  ");
    PrintString((CONST_STRPTR)"status", key, (CONST_STRPTR)"status");
    Print("  ");
    PrintCells((CONST_STRPTR)"reg", key, (CONST_STRPTR)"reg");
    Print("  ");
    PrintCells((CONST_STRPTR)"interrupts", key, (CONST_STRPTR)"interrupts");

    APTR reg = DT_FindProperty(key, (CONST_STRPTR)"reg");
    if (reg != NULL && DT_GetPropLen(reg) >= 4)
    {
        ULONG bus = *(const ULONG *)DT_GetPropValue(reg);
        cpu = BusToCpu(bus);
        Print("  address: bus %08lx -> 68k %08lx\n", bus, cpu);
    }

    APTR irq = DT_FindProperty(key, (CONST_STRPTR)"interrupts");
    if (irq != NULL && DT_GetPropLen(irq) >= 8 && irq_bank != NULL && irq_bit != NULL)
    {
        const ULONG *c = DT_GetPropValue(irq);
        *irq_bank = c[0];
        *irq_bit = c[1];
    }

    DT_CloseKey(key);

    return cpu;
}

/*
    Identification and read-only dump
*/

static void PrintLibrary(CONST_STRPTR name)
{
    struct Library *lib;
    UWORD ver = 0, rev = 0;
    char id[80];

    id[0] = 0;

    /* Everything is copied under Forbid(), the library may go away after it; the output comes after Permit() */
    Forbid();
    lib = (struct Library *)FindName(&SysBase->LibList, name);
    if (lib != NULL)
    {
        const char *s = lib->lib_IdString ? (const char *)lib->lib_IdString : "";
        ULONG n = 0;

        ver = lib->lib_Version;
        rev = lib->lib_Revision;
        while (s[n] != 0 && s[n] != '\r' && s[n] != '\n' && n < sizeof(id) - 1)
        {
            id[n] = s[n];
            n++;
        }
        id[n] = 0;
    }
    Permit();

    if (lib != NULL)
        Print("lib.%s: %ld.%ld %s\n", (LONG)name, (LONG)ver, (LONG)rev, (LONG)id);
    else
        Print("lib.%s: not loaded\n", (LONG)name);
}

/* TRUE when one of the strings of the "compatible" list of the root is brcm,bcm2837 (Pi 3 B, 3 B+) */
static BOOL IsBCM2837(void)
{
    static const char wanted[] = "brcm,bcm2837";
    APTR root = DT_OpenKey((CONST_STRPTR)"/");
    APTR prop = root ? DT_FindProperty(root, (CONST_STRPTR)"compatible") : NULL;
    BOOL found = FALSE;

    if (prop != NULL)
    {
        const char *s = DT_GetPropValue(prop);
        ULONG len = DT_GetPropLen(prop);
        ULONG pos = 0;

        /* A list of strings, each one ended by a zero */
        while (pos < len && !found)
        {
            ULONG i = 0;

            while (wanted[i] != 0 && pos + i < len && s[pos + i] == wanted[i])
                i++;
            if (wanted[i] == 0 && pos + i < len && s[pos + i] == 0)
                found = TRUE;

            while (pos < len && s[pos] != 0)
                pos++;
            pos++;
        }
    }

    if (root != NULL)
        DT_CloseKey(root);

    return found;
}

static void Identify(void)
{
    APTR root = DT_OpenKey((CONST_STRPTR)"/");

    PrintString((CONST_STRPTR)"board.model", root, (CONST_STRPTR)"model");
    PrintString((CONST_STRPTR)"board.compatible", root, (CONST_STRPTR)"compatible");
    if (root != NULL)
        DT_CloseKey(root);

    APTR emu68 = DT_OpenKey((CONST_STRPTR)"/emu68");
    PrintString((CONST_STRPTR)"emu68.idstring", emu68, (CONST_STRPTR)"idstring");
    if (emu68 != NULL)
        DT_CloseKey(emu68);

    Print("exec: %ld.%ld\n", (LONG)SysBase->LibNode.lib_Version, (LONG)SysBase->LibNode.lib_Revision);
    PrintLibrary((CONST_STRPTR)"rtg.library");
    PrintLibrary((CONST_STRPTR)"Picasso96API.library");
    PrintLibrary((CONST_STRPTR)"VideoCore.card");
    PrintLibrary((CONST_STRPTR)"gic400.library");
}

static void DumpRegisters(const struct Blocks *b)
{
    if (b->pv2)
    {
        Print("pv2.regs:");
        for (ULONG off = 0; off <= 0x30; off += 4)
            Print(" %08lx", Rd(b->pv2 + off));
        Print("\n");
        Print("pv2.INTEN: %08lx  pv2.INTSTAT: %08lx  pv2.STAT: %08lx\n",
               Rd(b->pv2 + PV_INTEN), Rd(b->pv2 + PV_INTSTAT), Rd(b->pv2 + PV_STAT));
    }
    if (b->hvs)
        Print("hvs.DISPCTRL: %08lx  hvs.DISPSTAT1: %08lx (line %ld)\n", Rd(b->hvs + SCALER_DISPCTRL),
               Rd(b->hvs + SCALER_DISPSTAT1), (LONG)(Rd(b->hvs + SCALER_DISPSTAT1) & 0xfff));
    if (b->armctrl)
    {
        Print("armctrl.pending: basic %08lx  1 %08lx  2 %08lx  fiq %08lx\n",
               Rd(b->armctrl + ARMCTRL_PENDING_BASIC), Rd(b->armctrl + ARMCTRL_PENDING_1),
               Rd(b->armctrl + ARMCTRL_PENDING_2), Rd(b->armctrl + ARMCTRL_FIQ));
        Print("armctrl.enable: basic %08lx  1 %08lx  2 %08lx\n", Rd(b->armctrl + ARMCTRL_ENABLE_BASIC),
               Rd(b->armctrl + ARMCTRL_ENABLE_1), Rd(b->armctrl + ARMCTRL_ENABLE_2));
    }
    if (b->local)
        Print("local.gpu_routing: %08lx  local.core0_irq_source: %08lx\n",
               Rd(b->local + LOCAL_GPU_ROUTING), Rd(b->local + LOCAL_CORE0_IRQ_SOURCE));
    if (b->smi)
        Print("smi.SMICS: %08lx  smi.SMIDSW0: %08lx\n", Rd(b->smi + SMICS), Rd(b->smi + SMIDSW0));
}

/*
    WATCH: passive observation
*/

static void WatchPhase(const struct Blocks *b, CONST_STRPTR phase, ULONG secs)
{
    ULONG frames = 0, vfp = 0, pend42 = 0, smi_events = 0;
    ULONG first = 0, last = 0;
    ULONG prev_line = Rd(b->hvs + SCALER_DISPSTAT1) & 0xfff;
    ULONG prev_smi = b->smi ? (Rd(b->smi + SMICS) & SMICS_INTERRUPTS) : 0;
    ULONG start = Rd(b->timer + TIMER_CLO);

    /* Forbid: a task switch in the middle would hide whole frames from the HVS line count */
    Forbid();

    Wr(b->pv2 + PV_INTSTAT, PV_INT_VFP_START);

    while (Rd(b->timer + TIMER_CLO) - start < secs * 1000000)
    {
        ULONG line = Rd(b->hvs + SCALER_DISPSTAT1) & 0xfff;

        if (line < prev_line)
            frames++;
        prev_line = line;

        if (Rd(b->pv2 + PV_INTSTAT) & PV_INT_VFP_START)
        {
            ULONG now = Rd(b->timer + TIMER_CLO);

            if (vfp == 0)
                first = now;
            last = now;
            vfp++;

            if (Rd(b->armctrl + ARMCTRL_PENDING_2) & (1UL << b->pv2_irq_bit))
                pend42++;

            Wr(b->pv2 + PV_INTSTAT, PV_INT_VFP_START);
        }

        if (b->smi)
        {
            ULONG s = Rd(b->smi + SMICS) & SMICS_INTERRUPTS;

            if (s != 0 && prev_smi == 0)
                smi_events++;
            prev_smi = s;
        }
    }

    Permit();

    Print("watch.%s: secs %ld  hvs_frames %ld  vfp_start %ld  pending2_bit%ld_with_vfp %ld  smi_events %ld\n",
           (LONG)phase, (LONG)secs, (LONG)frames, (LONG)vfp, (LONG)b->pv2_irq_bit, (LONG)pend42, (LONG)smi_events);
    if (vfp > 1)
        Print("watch.%s.period_us: %ld\n", (LONG)phase, (LONG)((last - first) / (vfp - 1)));
}

static void Watch(const struct Blocks *b, ULONG secs)
{
    ULONG inten = Rd(b->pv2 + PV_INTEN);

    Print("watch.inten_found: %08lx\n", inten);

    WatchPhase(b, (CONST_STRPTR)"as_found", secs);

    /* Arm the source only when nothing can turn it into an ARM interrupt */
    if (Rd(b->armctrl + ARMCTRL_ENABLE_2) & (1UL << b->pv2_irq_bit))
    {
        Print("watch.armed: skipped, the IRQ is already enabled in the ARM controller\n");
        return;
    }

    Wr(b->pv2 + PV_INTEN, inten | PV_INT_VFP_START);
    WatchPhase(b, (CONST_STRPTR)"armed", secs);
    Wr(b->pv2 + PV_INTEN, inten);
    Wr(b->pv2 + PV_INTSTAT, PV_INT_VFP_START);

    Print("watch.inten_restored: %08lx\n", Rd(b->pv2 + PV_INTEN));
}

/*
    IRQ: a real interrupt on INTB_EXTER
*/

struct IrqTest
{
    ULONG pv2;
    ULONG armctrl;
    ULONG timer;
    ULONG bit;
    ULONG limit;
    volatile ULONG calls;           /* every call of the server, also for the other EXTER sources (CIA-B) */
    volatile ULONG vfp;             /* calls with VFP_START set */
    volatile ULONG first;
    volatile ULONG last;
    volatile UBYTE stopped;         /* the limit was reached and the source switched off */
};

static struct IrqTest test;
static struct Interrupt server;

/* Called from the stub below with the IrqTest, in interrupt context */
ULONG ServerC(struct IrqTest *t);
ULONG ServerC(struct IrqTest *t)
{
    t->calls++;

    if (t->calls > t->limit && !t->stopped)
    {
        Wr(t->pv2 + PV_INTEN, 0);
        Wr(t->armctrl + ARMCTRL_DISABLE_2, 1UL << t->bit);
        t->stopped = 1;
    }

    if (Rd(t->pv2 + PV_INTSTAT) & PV_INT_VFP_START)
    {
        ULONG now = Rd(t->timer + TIMER_CLO);

        Wr(t->pv2 + PV_INTSTAT, PV_INT_VFP_START);  /* write 1 to clear: the line goes down */
        if (t->vfp == 0)
            t->first = now;
        t->last = now;
        t->vfp++;
    }

    /* Never end the chain: the other servers of INTB_EXTER must still run */
    return 0;
}

/* exec tests the Z flag after each server of the chain, which C code does not set: a small stub does it */
void ServerStub(void);
asm(
    "       .text                   \n"
    "       .even                   \n"
    "_ServerStub:                   \n"
    "       move.l  a1,-(sp)        \n"
    "       jsr     _ServerC        \n"
    "       addq.l  #4,sp           \n"
    "       tst.l   d0              \n"
    "       rts                     \n"
);

static void Irq(const struct Blocks *b, ULONG secs)
{
    ULONG inten = Rd(b->pv2 + PV_INTEN);
    BOOL enabled_before = (Rd(b->armctrl + ARMCTRL_ENABLE_2) & (1UL << b->pv2_irq_bit)) != 0;

    Print("irq.inten_found: %08lx  irq.armctrl_enabled_before: %ld\n", inten, (LONG)enabled_before);

    test.pv2 = b->pv2;
    test.armctrl = b->armctrl;
    test.timer = b->timer;
    test.bit = b->pv2_irq_bit;
    test.limit = secs * CALLS_PER_SECOND_LIMIT;

    server.is_Node.ln_Type = NT_INTERRUPT;
    server.is_Node.ln_Pri = 0;
    server.is_Node.ln_Name = (char *)"vc4vbl";
    server.is_Data = &test;
    server.is_Code = (void (*)())ServerStub;

    /* From a clean status, then the server, then the controller, then the source */
    Wr(b->pv2 + PV_INTEN, inten & ~PV_INT_VFP_START);
    Wr(b->pv2 + PV_INTSTAT, PV_INT_VFP_START);
    AddIntServer(INTB_EXTER, &server);
    Wr(b->armctrl + ARMCTRL_ENABLE_2, 1UL << b->pv2_irq_bit);
    Wr(b->pv2 + PV_INTEN, inten | PV_INT_VFP_START);

    Delay(secs * 50);

    /* Everything back as found, in the reverse order */
    Wr(b->pv2 + PV_INTEN, inten);
    if (!enabled_before)
        Wr(b->armctrl + ARMCTRL_DISABLE_2, 1UL << b->pv2_irq_bit);
    Wr(b->pv2 + PV_INTSTAT, PV_INT_VFP_START);
    RemIntServer(INTB_EXTER, &server);

    Print("irq: secs %ld  server_calls %ld  vfp_start %ld  stopped_by_limit %ld\n",
           (LONG)secs, (LONG)test.calls, (LONG)test.vfp, (LONG)test.stopped);
    if (test.vfp > 1)
        Print("irq.period_us: %ld\n", (LONG)((test.last - test.first) / (test.vfp - 1)));
    Print("irq.restored: INTEN %08lx  armctrl.enable2 %08lx\n", Rd(b->pv2 + PV_INTEN),
           Rd(b->armctrl + ARMCTRL_ENABLE_2));
}

int main(void)
{
    LONG args[3] = { 0, 0, 0 };
    struct RDArgs *rda = ReadArgs((CONST_STRPTR)"SECS/N,WATCH/S,IRQ/S", args, NULL);
    struct Blocks b = { 0 };
    ULONG secs = 2;

    if (rda == NULL)
    {
        PrintFault(IoErr(), (CONST_STRPTR)"vc4vbl");
        return RETURN_FAIL;
    }
    if (args[0] != 0)
        secs = *(LONG *)args[0];
    if (secs < 1)
        secs = 1;
    if (secs > MAX_SECS)
        secs = MAX_SECS;

    Print("vc4vbl: 1.1\n");

    DeviceTreeBase = OpenResource((CONST_STRPTR)"devicetree.resource");
    if (DeviceTreeBase == NULL)
    {
        Print("error: no devicetree.resource\n");
        FreeArgs(rda);
        return RETURN_FAIL;
    }

    Identify();

    /* The tool is for the Raspberry Pi 3 only: anything else stops here, before any register is touched */
    if (!IsBCM2837())
    {
        Print("error: this tool is for the Raspberry Pi 3 (brcm,bcm2837) only, stopped\n");
        FreeArgs(rda);
        return RETURN_WARN;
    }

    b.pv2 = FindBlock((CONST_STRPTR)"pixelvalve2", 0x7e807000, &b.pv2_irq_bank, &b.pv2_irq_bit);
    b.hvs = FindBlock((CONST_STRPTR)"hvs", 0x7e400000, NULL, NULL);
    b.armctrl = FindBlock((CONST_STRPTR)"intc", 0x7e00b200, NULL, NULL);
    b.local = FindBlock((CONST_STRPTR)"local_intc", 0x40000000, NULL, NULL);
    b.smi = FindBlock((CONST_STRPTR)"smi", 0x7e600000, NULL, NULL);
    b.timer = FindBlock((CONST_STRPTR)"system_timer", 0x7e003000, NULL, NULL);
    Print("pv2.irq: bank %ld bit %ld\n", (LONG)b.pv2_irq_bank, (LONG)b.pv2_irq_bit);

    DumpRegisters(&b);

    /* WATCH and IRQ need the pixelvalve, the HVS, the ARM controller and the timer, and the IRQ in bank 2 */
    if ((args[1] || args[2]) && (b.pv2 == 0 || b.hvs == 0 || b.armctrl == 0 || b.timer == 0 || b.pv2_irq_bank != 2))
    {
        Print("error: a block is missing or the IRQ is not in bank 2, WATCH and IRQ skipped\n");
        FreeArgs(rda);
        return RETURN_WARN;
    }

    if (args[1])
        Watch(&b, secs);
    if (args[2])
        Irq(&b, secs);

    if (args[1] || args[2])
        DumpRegisters(&b);

    Print("vc4vbl: done\n");
    FreeArgs(rda);
    return RETURN_OK;
}
