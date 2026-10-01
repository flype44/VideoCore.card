#ifndef _MAILBOX_H
#define _MAILBOX_H

/*
    VideoCore mailbox property channel definitions used by the graphics driver.

    The list is a selection of what the Raspberry Pi community knows about the property tags:
    the framebuffer and display tags, clocks, GPU memory, VPU / QPU code execution, power domains
    and EDID. Everything else (OTP, crypto, EEPROM, RTC, reboot, GPIO, ...) is left out.

    All the calls go through mailbox.resource and its MB_RawCommand(). The buffer of a call is made of
    native (big endian) longwords, the resource swaps it to little endian before sending it and back
    after the answer. That is wrong for the answers which are text or blobs (e.g. MB_GET_EDID_BLOCK,
    MB_GET_BOARD_SERIAL): the bytes of every longword come back reversed and have to be swapped again.
*/

#include <exec/types.h>

#define MAILBOXNAME "mailbox.resource"

/* Mailbox */
#define MB_SUCCESS                    (0x80000000)

/* Mailbox tags */
#define MB_GET_FIRMWARE_REV           (0x00000001)
#define MB_SET_CURSOR_INFO            (0x00008010)
#define MB_SET_CURSOR_STATE           (0x00008011)
#define MB_SET_SCREEN_GAMMA           (0x00008012)
#define MB_GET_BOARD_MODEL            (0x00010001)
#define MB_GET_BOARD_REVISION         (0x00010002)
#define MB_GET_ARM_MEMORY             (0x00010005)
#define MB_GET_VC_MEMORY              (0x00010006)

#define MB_GET_CLOCK_STATE            (0x00030001)
#define MB_GET_CLOCK_RATE             (0x00030002)
#define MB_GET_CLOCK_RATE_MAX         (0x00030004)
#define MB_GET_CLOCK_RATE_MIN         (0x00030007)
#define MB_ALLOCATE_MEMORY            (0x0003000c)
#define MB_LOCK_MEMORY                (0x0003000d)
#define MB_UNLOCK_MEMORY              (0x0003000e)
#define MB_RELEASE_MEMORY             (0x0003000f)
#define MB_EXECUTE_CODE               (0x00030010)
#define MB_EXECUTE_QPU                (0x00030011)
#define MB_SET_ENABLE_QPU             (0x00030012)
#define MB_GET_DISPMANX_HANDLE        (0x00030014)
#define MB_GET_EDID_BLOCK             (0x00030020)
#define MB_GET_EDID_BLOCK_DISPLAY     (0x00030023)
#define MB_GET_DOMAIN_STATE           (0x00030030)
#define MB_GET_CLOCK_RATE_M           (0x00030047)
#define MB_NOTIFY_DISPLAY_DONE        (0x00030066)

#define MB_SET_CLOCK_STATE            (0x00038001)
#define MB_SET_CLOCK_RATE             (0x00038002)
#define MB_SET_DOMAIN_STATE           (0x00038030)

#define MB_ALLOCATE_BUFFER            (0x00040001)
#define MB_BLANK_SCREEN               (0x00040002)
#define MB_GET_PHYSICAL_SIZE          (0x00040003)
#define MB_GET_VIRTUAL_SIZE           (0x00040004)
#define MB_GET_DEPTH                  (0x00040005)
#define MB_GET_PIXEL_ORDER            (0x00040006)
#define MB_GET_ALPHA_MODE             (0x00040007)
#define MB_GET_PITCH                  (0x00040008)
#define MB_GET_VIRTUAL_OFFSET         (0x00040009)
#define MB_GET_OVERSCAN               (0x0004000a)
#define MB_GET_PALETTE                (0x0004000b)
#define MB_GET_LAYER                  (0x0004000c)
#define MB_GET_TRANSFORM              (0x0004000d)
#define MB_GET_VSYNC                  (0x0004000e)
#define MB_FB_GET_NUM_DISPLAYS        (0x00040013)
#define MB_FB_GET_DISPLAY_SETTINGS    (0x00040014)
#define MB_FB_GET_DISPLAY_ID          (0x00040016)
#define MB_FB_GET_DISPLAY_TIMING      (0x00040017)
#define MB_FB_GET_DISPLAY_CFG         (0x00040018)

#define MB_TEST_PHYSICAL_SIZE         (0x00044003)
#define MB_TEST_VIRTUAL_SIZE          (0x00044004)
#define MB_TEST_DEPTH                 (0x00044005)
#define MB_TEST_PIXEL_ORDER           (0x00044006)
#define MB_TEST_ALPHA_MODE            (0x00044007)
#define MB_TEST_VIRTUAL_OFFSET        (0x00044009)
#define MB_TEST_OVERSCAN              (0x0004400a)
#define MB_TEST_PALETTE               (0x0004400b)
#define MB_TEST_LAYER                 (0x0004400c)
#define MB_TEST_TRANSFORM             (0x0004400d)
#define MB_TEST_VSYNC                 (0x0004400e)

#define MB_RELEASE_BUFFER             (0x00048001)
#define MB_SET_PHYSICAL_SIZE          (0x00048003)
#define MB_SET_VIRTUAL_SIZE           (0x00048004)
#define MB_SET_DEPTH                  (0x00048005)
#define MB_SET_PIXEL_ORDER            (0x00048006)
#define MB_SET_ALPHA_MODE             (0x00048007)
#define MB_SET_PITCH                  (0x00048008)
#define MB_SET_VIRTUAL_OFFSET         (0x00048009)
#define MB_SET_OVERSCAN               (0x0004800a)
#define MB_SET_PALETTE                (0x0004800b)
#define MB_SET_LAYER                  (0x0004800c)
#define MB_SET_TRANSFORM              (0x0004800d)
#define MB_SET_VSYNC                  (0x0004800e)
#define MB_SET_BACKLIGHT              (0x0004800f)
#define MB_FB_SET_DISPLAY_NUM         (0x00048013)
#define MB_FB_SET_TIMING              (0x00048017)
#define MB_FB_SET_DISPLAY_POWER       (0x00048019)

/* clock id */
#define CLK_EMMC_ID                   (0x1)
#define CLK_UART_ID                   (0x2)
#define CLK_ARM_ID                    (0x3)
#define CLK_CORE_ID                   (0x4)
#define CLK_V3D_ID                    (0x5)
#define CLK_H264_ID                   (0x6)
#define CLK_ISP_ID                    (0x7)
#define CLK_SDRAM_ID                  (0x8)
#define CLK_PIXEL_ID                  (0x9)
#define CLK_PWM_ID                    (0xA)
#define CLK_HEVC_ID                   (0xB)
#define CLK_EMMC2_ID                  (0xC)
#define CLK_M2MC_ID                   (0xD)
#define CLK_PIXEL_BVB_ID              (0xE)

/* power domain id, for MB_GET_DOMAIN_STATE / MB_SET_DOMAIN_STATE */
#define DOMAIN_UNICAM1                (14)

/* MB_ALLOCATE_MEMORY 'flags' bitmask */
#define MEM_FLAG_DISCARDABLE          (1 << 0)  /* can be resized to 0 at any time */
#define MEM_FLAG_NORMAL               (0 << 2)  /* normal allocating alias */
#define MEM_FLAG_DIRECT               (1 << 2)  /* 0xC alias uncached */
#define MEM_FLAG_COHERENT             (2 << 2)  /* 0x8 alias, cache coherent */
#define MEM_FLAG_L1_NONALLOCATING     (MEM_FLAG_DIRECT | MEM_FLAG_COHERENT) /* 0x4 alias */
#define MEM_FLAG_ZERO                 (1 << 4)  /* initialise buffer to zero */
#define MEM_FLAG_NO_INIT              (1 << 5)  /* don't initialise (default is initialise to 0) */
#define MEM_FLAG_HINT_PERMALOCK       (1 << 6)  /* likely to be locked for long periods of time */

#endif /* _MAILBOX_H */
