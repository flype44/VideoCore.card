#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/execbase.h>
#include <clib/debug_protos.h>
#include <devices/inputevent.h>

#include <proto/mathieeesingbas.h>
#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/input.h>
#include <proto/devicetree.h>
#include <proto/unicam.h>
#include <proto/icon.h>

#include <stdint.h>
#include <common/compiler.h>

// Shut off MathIEEE float injecting stuff
#define FLOAT ULONG

#include "boardinfo.h"
#include "videocore.h"
#include "mbox.h"
#include "vpu/block_copy.h"
#include "utils.h"
#include "vc4.h"
#include "vc6.h"
#include "unicam.h"
#include "buddyalloc.h"
#include "hvs.h"

int __attribute__((no_reorder)) _start()
{
        return -1;
}

extern const char deviceEnd;
extern const char deviceName[];
extern const char deviceIdString[];
extern const uint32_t InitTable[];

const struct Resident RomTag __attribute__((used)) = {
    RTC_MATCHWORD,
    (struct Resident *)&RomTag,
    (APTR)&deviceEnd,
    RTF_AUTOINIT,
    VC4CARD_VERSION,
    NT_LIBRARY,
    VC4CARD_PRIORITY,
    (char *)((intptr_t)&deviceName),
    (char *)((intptr_t)&deviceIdString),
    (APTR)InitTable,
};

const char deviceName[] = CARD_NAME;
const char deviceIdString[] = VERSION_STRING;

static int FindCard(REGARG(struct BoardInfo* bi, "a0"), REGARG(struct VideoCoreBase *VideoCoreBase, "a6"))
{
    struct ExecBase *SysBase = VideoCoreBase->vc_LibNode.ExecBase;
    APTR DeviceTreeBase = NULL;
    APTR key;

    // Cancel loading the driver if left or right shift is being held down.
    struct IORequest io;

    bug("[VC] FindCard\n");

    if (OpenDevice((STRPTR)"input.device", 0, &io, 0) == 0)
    {
        struct Library *InputBase = (struct Library *)io.io_Device;
        UWORD qual = PeekQualifier();
        CloseDevice(&io);

        if (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))
        {
            bug("[VC] Shift was pressed, ignoring VideoCore\n");
            return 0;
        }
            
    }

    VideoCoreBase->vc_UnicamVisible = FALSE;

    /* Open device tree resource */
    DeviceTreeBase = (struct Library *)OpenResource((STRPTR)"devicetree.resource");
    if (DeviceTreeBase == 0) {
        // If devicetree.resource can't be opened, this probably isn't Emu68.
        return 0;
    }
    VideoCoreBase->vc_DeviceTreeBase = DeviceTreeBase;

    /* Open mailbox resource. Without it nothing can talk to the VideoCore firmware: no RTG, the system stays on the chipset display. */
    VideoCoreBase->vc_MailboxBase = OpenResource((STRPTR)MAILBOXNAME);
    if (VideoCoreBase->vc_MailboxBase == NULL) {
        bug("[VC] Cannot open %s\n", MAILBOXNAME);
        return 0;
    }

    /* Find out base address of framebuffer and video memory size */
    GetVCMemory(&VideoCoreBase->vc_MemBase, &VideoCoreBase->vc_MemSize, VideoCoreBase);

    bug("[VC] GPU memory at %08lx, size: %ld KB\n", VideoCoreBase->vc_MemBase, (ULONG)VideoCoreBase->vc_MemSize / 1024);

    /* Set basic data in BoardInfo structure */

    /* Get the block memory which was reserved by Emu68 on early startup. It has proper caching already */
    key = DT_OpenKey("/emu68");
    if (key)
    {
        const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "vc4-mem"));

        if (reg == NULL)
        {
            return 0;
        }

        bi->MemoryBase = (APTR)reg[0];
        bi->MemorySize = reg[1];

        DT_CloseKey(key);
    }

    bi->RegisterBase = NULL;

    bug("[VC] Memory base at %08lx, size %ldMB\n", (ULONG)bi->MemoryBase, bi->MemorySize / (1024*1024));

    while (VideoCoreBase->vc_DispSize.width == 0 || VideoCoreBase->vc_DispSize.height == 0)
    {
        VideoCoreBase->vc_DispSize = GetPhysicalSize(VideoCoreBase);
    }

    bug("[VC] Physical display size: %ld x %ld\n", (ULONG)VideoCoreBase->vc_DispSize.width, (ULONG)VideoCoreBase->vc_DispSize.height);

    VideoCoreBase->vc_ActivePlane = -1;

    VideoCoreBase->vc_VideoCore6 = 0;

    key = DT_OpenKey("/gpu");
    if (key)
    {
        const char *comp = DT_GetPropValue(DT_FindProperty(key, "compatible"));

        if (comp != NULL)
        {
            if (_strcmp("brcm,bcm2711-vc5", comp) == 0)
            {
                bug("[VC] VideoCore6 detected\n");
                VideoCoreBase->vc_VideoCore6 = 1;
            }
        }
    }

#if 0
    VideoCoreBase->vc_VPU_CopyBlock = (APTR)upload_code(vpu_block_copy, sizeof(vpu_block_copy), VideoCoreBase);

    RawDoFmt("[vc4] VPU CopyBlock pointer at %08lx\n", &VideoCoreBase->vc_VPU_CopyBlock, (APTR)putch, NULL);
#endif

//UNICAM

    APTR UnicamBase = OpenResource("unicam.resource");
    if (UnicamBase != NULL)
    {
        VideoCoreBase->vc_UnicamBase = UnicamBase;
        VideoCoreBase->vc_Unicambuffer = UnicamGetFramebuffer();
        VideoCoreBase->vc_UnicambufferSize = UnicamGetFramebufferSize();
        ULONG mode = UnicamGetMode();
        ULONG size = UnicamGetSize();
        UnicamStart(VideoCoreBase->vc_Unicambuffer, 1, (mode >> 8) & 0xff, size >> 16, size & 0xffff, mode & 0xff);
    }

    return 1;
}

#include "messages.h"
#include "chip.h"
#include "task.h"

/* The value of a ToolType which switches something on: YES, TRUE or 1 */
static BOOL ToolTypeIsOn(struct Library *IconBase, CONST_STRPTR value)
{
    return MatchToolValue(value, "YES") || MatchToolValue(value, "TRUE") || MatchToolValue(value, "1");
}

/* The values of the ToolType VC4_SWITCH_METHOD */
static const struct {
    const char *name;
    enum SwitchMode mode;
} switch_methods[] = {
    { "CTS",  CTS  },
    { "DTR",  DTR  },
    { "RTS",  RTS  },
    { "SEL",  SEL  },
    { "CSI",  CSI  },
    { "NONE", None },
};

static int InitCard(REGARG(struct BoardInfo* bi, "a0"), REGARG(const char **ToolTypes, "a1"), REGARG(struct VideoCoreBase *VideoCoreBase, "a6"))
{
    struct ExecBase *SysBase = VideoCoreBase->vc_LibNode.ExecBase;
    struct Library *MathIeeeSingBasBase = OpenLibrary("mathieeesingbas.library", 0);

    BuddyInit(VideoCoreBase);

    if (MathIeeeSingBasBase == NULL)
        return 0;

    bi->BoardName = "VideoCore";
    bi->BoardType = BT_PiStorm;
    bi->PaletteChipType = PCT_PiStorm;
    bi->GraphicsControllerType = GCT_PiStorm;

    bi->Flags |= BIF_GRANTDIRECTACCESS | BIF_FLICKERFIXER | BIF_HARDWARESPRITE; // | BIF_BLITTER;
    bi->RGBFormats = 
        RGBFF_TRUEALPHA | 
        RGBFF_TRUECOLOR | 
        RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B5G6R5PC | RGBFF_B5G5R5PC | // RGBFF_HICOLOR | 
        RGBFF_CLUT; //RGBFF_HICOLOR | RGBFF_TRUEALPHA | RGBFF_CLUT;
    bi->SoftSpriteFlags = 0;
    bi->BitsPerCannon = 8;

    for(int i = 0; i < MAXMODES; i++) {
        bi->MaxHorValue[i] = 8192;
        bi->MaxVerValue[i] = 8192;
        bi->MaxHorResolution[i] = 8192;
        bi->MaxVerResolution[i] = 8192;
        bi->PixelClockCount[i] = 1;
    }

    bi->MemoryClock = CLOCK_HZ;

    /* The chip fills the functions of the BoardInfo */
    if (VideoCoreBase->vc_VideoCore6)
        VC6_InitChip(bi);
    else
        VC4_InitChip(bi);

    
    bug("[VC] Measuring refresh rate\n");

    Disable();

    volatile ULONG *stat = (ULONG*)(HVS_BASE + SCALER_DISPSTAT1);

    ULONG cnt1 = *stat & LE32(0x3f << 12);
    ULONG cnt2;

    /* Wait for the very next frame */
    do { cnt2 = *stat & LE32(0x3f << 12); } while(cnt2 == cnt1);
    
    /* Get current tick number */
    ULONG tick1 = LE32(*(volatile uint32_t*)SYSTEM_TIMER_CLO);

    /* Wait for the very next frame */
    do { cnt1 = *stat & LE32(0x3f << 12); } while(cnt2 == cnt1);

    /* Get current tick number */
    ULONG tick2 = LE32(*(volatile uint32_t*)SYSTEM_TIMER_CLO);

    Enable();

    const ULONG float_1000000 = 0x49742400;
    const ULONG float_1000 = 0x447a0000;
    const ULONG float_0p5 = 0x3f000000;

    ULONG delta = IEEESPFlt(tick2 - tick1);
    ULONG hz = IEEESPDiv(float_1000000, delta);
    ULONG mHz = IEEESPMul(float_1000, hz);
    mHz = IEEESPAdd(mHz, float_0p5);   // + 0.5
    mHz = IEEESPFix(mHz);
    hz = IEEESPAdd(hz, float_0p5);
    hz = IEEESPFix(hz);

    bug("[VC] Detected refresh rate of %ld.%03ld Hz\n", mHz / 1000, mHz % 1000);
    VideoCoreBase->vc_VertFreq = hz;

    VideoCoreBase->vc_Phase = 128;
    VideoCoreBase->vc_Scaler = 0xc0000000;
    VideoCoreBase->vc_UseKernel = 1;
    VideoCoreBase->vc_SpriteAlpha = 255;
    VideoCoreBase->vc_SwitchMode = None;
    VideoCoreBase->vc_SwitchInverted = 0;
    VideoCoreBase->vc_Kernel_B = 0x3e800000; // 0.25
    VideoCoreBase->vc_Kernel_C = 0x3f400000; // 0.75
    VideoCoreBase->vc_IntegerScaler = 0;
    VideoCoreBase->vc_UseDPMS = FALSE;

    APTR UnicamBase = VideoCoreBase->vc_UnicamBase;

    /* If Unicam was activated on boot, pre-select CSI switch mode */
    if (UnicamBase != NULL && (UnicamGetConfig() & UNICAMF_BOOT) != 0) VideoCoreBase->vc_SwitchMode = CSI;

    /* The ToolTypes are read with the calls of icon.library, which do not tell upper and lower case apart. rtg.library
       has read the icon with them to give us the array, so the library is there. */
    struct Library *IconBase = OpenLibrary("icon.library", 0);

    if (IconBase != NULL && ToolTypes != NULL)
    {
        CONST_STRPTR value;

        if ((value = FindToolType((CONST_STRPTR *)ToolTypes, "VC4_LEGACY_ID")) != NULL && (*value == 0 || ToolTypeIsOn(IconBase, value)))
        {
            bi->BoardType = BT_uaegfx;
            bi->PaletteChipType = PCT_S3ViRGE;
            bi->GraphicsControllerType = GCT_S3ViRGE;
        }
        if ((value = FindToolType((CONST_STRPTR *)ToolTypes, "VC4_PHASE")) != NULL)
        {
            ULONG num = _atoul(value);

            VideoCoreBase->vc_Phase = num;
            bug("[VC] Setting VC4 phase to %ld\n", num);
        }
        if ((value = FindToolType((CONST_STRPTR *)ToolTypes, "VC4_VERT")) != NULL)
        {
            ULONG num = _atoul(value);

            VideoCoreBase->vc_VertFreq = num;
            bug("[VC] Setting vertical frequency to %ld\n", num);
        }
        if ((value = FindToolType((CONST_STRPTR *)ToolTypes, "VC4_SCALER")) != NULL)
        {
            switch(value[0]) {
                case '0':
                    VideoCoreBase->vc_Scaler = 0x00000000;
                    break;
                case '1':
                    VideoCoreBase->vc_Scaler = 0x40000000;
                    break;
                case '2':
                    VideoCoreBase->vc_Scaler = 0x80000000;
                    break;
                case '3':
                    VideoCoreBase->vc_Scaler = 0xc0000000;
                    break;
            }

            bug("[VC] Setting VC4 scaler to %lx\n", VideoCoreBase->vc_Scaler);
        }
        if ((value = FindToolType((CONST_STRPTR *)ToolTypes, "VC4_KERNEL")) != NULL)
        {
            ULONG num = _atoul(value);

            if (num == 0)
                VideoCoreBase->vc_UseKernel = 0;
            else
                VideoCoreBase->vc_UseKernel = 1;
        }
        if ((value = FindToolType((CONST_STRPTR *)ToolTypes, "VC4_KERNEL_B")) != NULL)
        {
            ULONG num = _atoul(value);

            VideoCoreBase->vc_Kernel_B = IEEESPDiv(
                IEEESPFlt(num),
                0x447a0000  // 1000.0
            );

            bug("[VC] Mitchel-Netravali B %ld\n", num);
        }
        if ((value = FindToolType((CONST_STRPTR *)ToolTypes, "VC4_SPRITE_OPACITY")) != NULL)
        {
            ULONG num = _atoul(value);

            if (num > 255) num=255;

            VideoCoreBase->vc_SpriteAlpha = num;
            bug("[VC] Sprite opacity set to %ld\n", num);
        }
        if ((value = FindToolType((CONST_STRPTR *)ToolTypes, "VC4_KERNEL_C")) != NULL)
        {
            ULONG num = _atoul(value);

            VideoCoreBase->vc_Kernel_C = IEEESPDiv(
                IEEESPFlt(num),
                0x447a0000  // 1000.0
            );

            bug("[VC] Mitchel-Netravali C %ld\n", num);
        }
        if ((value = FindToolType((CONST_STRPTR *)ToolTypes, "VC4_SWITCH_METHOD")) != NULL)
        {
            /*
                Find out method for switching between HDMI and RGB signals. Currently following
                methods are available:
                CTS - the CTS dignal gathered from CIA port will be used for switching
                      When CTS is set to logic 1, RGB source is selected
                      When CTS is set to logic 0, HDMI source is selected

                When no VC4_SWITCH_METHOD is selected, the driver will let user decide what to
                do and will not attempt to perform any switching
            */
            int i;

            /* MatchToolValue() does not tell the case apart and takes a list like DTR|RTS */
            for (i = 0; i < sizeof(switch_methods) / sizeof(switch_methods[0]); i++)
            {
                if (MatchToolValue(value, switch_methods[i].name))
                {
                    VideoCoreBase->vc_SwitchMode = switch_methods[i].mode;
                    break;
                }
            }
        }
        if ((value = FindToolType((CONST_STRPTR *)ToolTypes, "VC4_SWITCH_INVERT")) != NULL)
        {
            /* Invert the default behavior for selected RGB/HDMI switch mode */
            if (ToolTypeIsOn(IconBase, value))
                VideoCoreBase->vc_SwitchInverted = 1;
        }
        if ((value = FindToolType((CONST_STRPTR *)ToolTypes, "VC4_INTEGER_SCALING")) != NULL)
        {
            /* Scale by integer factors only */
            if (ToolTypeIsOn(IconBase, value))
                VideoCoreBase->vc_IntegerScaler = 1;
        }
        if ((value = FindToolType((CONST_STRPTR *)ToolTypes, "VC4_DPMS")) != NULL && (*value == 0 || ToolTypeIsOn(IconBase, value)))
        {
            /* Expose DPMS support to Picasso96, 
             * using the mailbox display power tag */
            VideoCoreBase->vc_UseDPMS = TRUE;
        }
    }

    if (IconBase != NULL)
        CloseLibrary(IconBase);

    /* The display power control of the firmware, for Picasso96 DPMS */
    if (VideoCoreBase->vc_UseDPMS)
    {
        /* obtain the RPi primary display id, or -1 if not supported by the RPi firmware.
         * 
         * +---------+----------------+------------+
         * | display | display_number | display_id |
         * +---------+----------------+------------+
         * | hdmi-0  |            0UL |         2L | primary hdmi
         * | hdmi-1  |            1UL |         7L | secondary hdmi
         * +---------+----------------+------------+
         */
        
        /* obtain the primary hdmi display num */
        VideoCoreBase->vc_DisplayNum = 0UL;
        
        /* obtain the primary hdmi display id */
        VideoCoreBase->vc_DisplayID = GetDisplayID(
            VideoCoreBase->vc_DisplayNum, VideoCoreBase);
        
        /* attach the Picasso96 method if the display id is valid */
        if (VideoCoreBase->vc_DisplayID >= 0) {
            bi->SetDPMSLevel = (void *)Chip_SetDPMSLevel;

            /* the firmware keeps the display power state from one boot to the next:
             * a display which DPMS has switched off before a reboot would stay off,
             * so switch it on */
            SetDisplayPower(VideoCoreBase->vc_DisplayID, 1, VideoCoreBase);
        }
    }

    /* Scaling kernels and the display list of Unicam */
    HVS_Init(VideoCoreBase);

    /* The task which answers the messages of the clients */
    Task_Start(bi);

    VideoCoreBase->vc_SpriteShape = AllocMem(MAXSPRITEWIDTH * MAXSPRITEHEIGHT, MEMF_FAST | MEMF_REVERSE | MEMF_CLEAR);

    bug("[VC] InitCard ready\n");

    /* If Unicam was activated on boot, make sure the pass-through is active at this moment */
    HVS_ShowUnicam(VideoCoreBase);

    CloseLibrary(MathIeeeSingBasBase);

    return 1;
}

static struct VideoCoreBase * OpenLib(REGARG(ULONG version, "d0"), REGARG(struct VideoCoreBase *VideoCoreBase, "a6"))
{
    struct ExecBase *SysBase = *(struct ExecBase **)4;
    VideoCoreBase->vc_LibNode.LibBase.lib_OpenCnt++;
    VideoCoreBase->vc_LibNode.LibBase.lib_Flags &= ~LIBF_DELEXP;

    bug("[VC] OpenLib\n");

    return VideoCoreBase;
}

static ULONG ExpungeLib(REGARG(struct VideoCoreBase *VideoCoreBase, "a6"))
{
    struct ExecBase *SysBase = VideoCoreBase->vc_LibNode.ExecBase;
    BPTR segList = 0;

    if (VideoCoreBase->vc_LibNode.LibBase.lib_OpenCnt == 0)
    {
        /* Remove library from Exec's list */
        Remove(&VideoCoreBase->vc_LibNode.LibBase.lib_Node);

        /* Save seglist */
        segList = (BPTR)VideoCoreBase->vc_LibNode.SegList;

        /* Remove VideoCoreBase itself - free the memory */
        ULONG size = VideoCoreBase->vc_LibNode.LibBase.lib_NegSize + VideoCoreBase->vc_LibNode.LibBase.lib_PosSize;
        FreeMem((APTR)((ULONG)VideoCoreBase - VideoCoreBase->vc_LibNode.LibBase.lib_NegSize), size);
    }
    else
    {
        /* Library is still in use, set delayed expunge flag */
        VideoCoreBase->vc_LibNode.LibBase.lib_Flags |= LIBF_DELEXP;
    }

    /* Return 0 or segList */
    return segList;
}

static ULONG CloseLib(REGARG(struct VideoCoreBase *VideoCoreBase, "a6"))
{
    if (VideoCoreBase->vc_LibNode.LibBase.lib_OpenCnt != 0)
        VideoCoreBase->vc_LibNode.LibBase.lib_OpenCnt--;
    
    if (VideoCoreBase->vc_LibNode.LibBase.lib_OpenCnt == 0)
    {
        if (VideoCoreBase->vc_LibNode.LibBase.lib_Flags & LIBF_DELEXP)
            return ExpungeLib(VideoCoreBase);
    }

    return 0;
}


static uint32_t ExtFunc()
{
    return 0;
}

struct VideoCoreBase * vc_Init(REGARG(struct VideoCoreBase *base, "d0"), REGARG(BPTR seglist, "a0"), REGARG(struct ExecBase *SysBase, "a6"))
{
    struct VideoCoreBase *VideoCoreBase = base;
    VideoCoreBase->vc_LibNode.SegList = (APTR)seglist;
    VideoCoreBase->vc_LibNode.ExecBase = SysBase;
    VideoCoreBase->vc_LibNode.LibBase.lib_Revision = VC4CARD_REVISION;
    VideoCoreBase->vc_Enabled = -1;

    return VideoCoreBase;
}

static uint32_t vc_functions[] = {
    (uint32_t)OpenLib,
    (uint32_t)CloseLib,
    (uint32_t)ExpungeLib,
    (uint32_t)ExtFunc,
    (uint32_t)FindCard,
    (uint32_t)InitCard,
    -1
};

const uint32_t InitTable[4] = {
    sizeof(struct VideoCoreBase), 
    (uint32_t)vc_functions, 
    0, 
    (uint32_t)vc_Init
};
