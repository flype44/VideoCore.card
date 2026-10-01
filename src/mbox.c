/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*
    The requests go through mailbox.resource: it serialises the callers, converts the buffer between
    big and little endian and takes care of the DMA caches. The buffers below hold native longwords.
*/

#include <exec/types.h>
#include <exec/execbase.h>

#include <proto/exec.h>
#include <proto/mailbox.h>

#include <stdint.h>

#include "emu68-vc4.h"
#include "mbox.h"

void GetVCMemory(void **base, uint32_t *size, struct VC4Base *VC4Base)
{
    APTR MailboxBase = VC4Base->vc4_MailboxBase;
    ULONG FBReq[8];

    FBReq[0] = 4 * 8;               // Length
    FBReq[1] = 0;                   // Request
    FBReq[2] = MB_GET_VC_MEMORY;
    FBReq[3] = 8;
    FBReq[4] = 0;
    FBReq[5] = 0;                   // Base
    FBReq[6] = 0;                   // Size
    FBReq[7] = 0;

    MB_RawCommand(FBReq);

    if (base)
        *base = (void *)(intptr_t)FBReq[5];

    if (size)
        *size = FBReq[6];
}

struct Size GetPhysicalSize(struct VC4Base *VC4Base)
{
    APTR MailboxBase = VC4Base->vc4_MailboxBase;
    ULONG FBReq[8];
    struct Size dimension;

    FBReq[0] = 4 * 8;
    FBReq[1] = 0;
    FBReq[2] = MB_GET_PHYSICAL_SIZE;
    FBReq[3] = 8;
    FBReq[4] = 0;
    FBReq[5] = 0;                   // Width
    FBReq[6] = 0;                   // Height
    FBReq[7] = 0;

    MB_RawCommand(FBReq);

    dimension.width = FBReq[5];
    dimension.height = FBReq[6];

    return dimension;
}

void SetPhysicalSize(struct Size size, struct VC4Base *VC4Base)
{
    APTR MailboxBase = VC4Base->vc4_MailboxBase;
    ULONG FBReq[8];

    FBReq[0] = 4 * 8;
    FBReq[1] = 0;
    FBReq[2] = MB_SET_PHYSICAL_SIZE;
    FBReq[3] = 8;
    FBReq[4] = 0;
    FBReq[5] = size.width;
    FBReq[6] = size.height;
    FBReq[7] = 0;

    MB_RawCommand(FBReq);
}

void SetVirtualSize(struct Size size, struct VC4Base *VC4Base)
{
    APTR MailboxBase = VC4Base->vc4_MailboxBase;
    ULONG FBReq[8];

    FBReq[0] = 4 * 8;
    FBReq[1] = 0;
    FBReq[2] = MB_SET_VIRTUAL_SIZE;
    FBReq[3] = 8;
    FBReq[4] = 0;
    FBReq[5] = size.width;
    FBReq[6] = size.height;
    FBReq[7] = 0;

    MB_RawCommand(FBReq);
}

void SetDepth(uint8_t depth, struct VC4Base *VC4Base)
{
    APTR MailboxBase = VC4Base->vc4_MailboxBase;
    ULONG FBReq[7];

    FBReq[0] = 4 * 7;
    FBReq[1] = 0;
    FBReq[2] = MB_SET_DEPTH;
    FBReq[3] = 4;
    FBReq[4] = 0;
    FBReq[5] = depth;
    FBReq[6] = 0;

    MB_RawCommand(FBReq);
}

void AllocateBuffer(uint32_t alignment, void **base, uint32_t *size, struct VC4Base *VC4Base)
{
    APTR MailboxBase = VC4Base->vc4_MailboxBase;
    ULONG FBReq[8];

    FBReq[0] = 4 * 8;
    FBReq[1] = 0;
    FBReq[2] = MB_ALLOCATE_BUFFER;
    FBReq[3] = 8;
    FBReq[4] = 0;
    FBReq[5] = alignment;           // Alignment, the base address in the answer
    FBReq[6] = 0;                   // Size
    FBReq[7] = 0;

    MB_RawCommand(FBReq);

    if (base)
        *base = (void *)(intptr_t)FBReq[5];

    if (size)
        *size = FBReq[6];
}

uint32_t GetPitch(struct VC4Base *VC4Base)
{
    APTR MailboxBase = VC4Base->vc4_MailboxBase;
    ULONG FBReq[7];

    FBReq[0] = 4 * 7;
    FBReq[1] = 0;
    FBReq[2] = MB_GET_PITCH;
    FBReq[3] = 4;
    FBReq[4] = 0;
    FBReq[5] = 0;                   // Bytes per line
    FBReq[6] = 0;

    MB_RawCommand(FBReq);

    return FBReq[5];
}

void ReleaseBuffer(struct VC4Base *VC4Base)
{
    APTR MailboxBase = VC4Base->vc4_MailboxBase;
    ULONG FBReq[6];

    FBReq[0] = 4 * 6;
    FBReq[1] = 0;
    FBReq[2] = MB_RELEASE_BUFFER;
    FBReq[3] = 0;
    FBReq[4] = 0;
    FBReq[5] = 0;

    MB_RawCommand(FBReq);
}

int BlankScreen(int blank, struct VC4Base *VC4Base)
{
    APTR MailboxBase = VC4Base->vc4_MailboxBase;
    ULONG FBReq[7];

    /* This is the request the driver sent before it used mailbox.resource: the tag is
       MB_GET_PHYSICAL_SIZE, not MB_BLANK_SCREEN */
    FBReq[0] = 4 * 7;
    FBReq[1] = 0;
    FBReq[2] = MB_GET_PHYSICAL_SIZE;
    FBReq[3] = 4;
    FBReq[4] = 0;
    FBReq[5] = blank ? 1 : 0;
    FBReq[6] = 0;

    MB_RawCommand(FBReq);

    return FBReq[5] & 1;
}

uint32_t AllocateMemory(uint32_t size, uint32_t alignment, uint32_t flags, struct VC4Base *VC4Base)
{
    APTR MailboxBase = VC4Base->vc4_MailboxBase;
    ULONG FBReq[9];

    FBReq[0] = 4 * 9;
    FBReq[1] = 0;
    FBReq[2] = MB_ALLOCATE_MEMORY;
    FBReq[3] = 12;
    FBReq[4] = 0;
    FBReq[5] = size;
    FBReq[6] = alignment;
    FBReq[7] = flags;
    FBReq[8] = 0;

    MB_RawCommand(FBReq);

    /* Handle of the block */
    return FBReq[5];
}

uint32_t LockMemory(uint32_t handle, struct VC4Base *VC4Base)
{
    APTR MailboxBase = VC4Base->vc4_MailboxBase;
    ULONG FBReq[7];

    FBReq[0] = 4 * 7;
    FBReq[1] = 0;
    FBReq[2] = MB_LOCK_MEMORY;
    FBReq[3] = 4;
    FBReq[4] = 0;
    FBReq[5] = handle;
    FBReq[6] = 0;

    MB_RawCommand(FBReq);

    /* Address of the block, in the view of the VPU */
    return FBReq[5];
}

uint32_t ExecuteCode(uint32_t addr, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3,
                     uint32_t arg4, uint32_t arg5, struct VC4Base *VC4Base)
{
    APTR MailboxBase = VC4Base->vc4_MailboxBase;
    ULONG FBReq[13];

    FBReq[0] = 4 * 13;
    FBReq[1] = 0;
    FBReq[2] = MB_EXECUTE_CODE;
    FBReq[3] = 28;
    FBReq[4] = 0;
    FBReq[5] = addr;                // code address
    FBReq[6] = arg0;                // r0
    FBReq[7] = arg1;                // r1
    FBReq[8] = arg2;                // r2
    FBReq[9] = arg3;                // r3
    FBReq[10] = arg4;               // r4
    FBReq[11] = arg5;               // r5
    FBReq[12] = 0;

    MB_RawCommand(FBReq);

    /* r0 when the code returned */
    return FBReq[5];
}

uint32_t SetDomainState(uint32_t domain, uint32_t state, struct VC4Base *VC4Base)
{
    APTR MailboxBase = VC4Base->vc4_MailboxBase;
    ULONG FBReq[8];

    FBReq[0] = 4 * 8;
    FBReq[1] = 0;
    FBReq[2] = MB_SET_DOMAIN_STATE;
    FBReq[3] = 8;
    FBReq[4] = 0;
    FBReq[5] = domain;
    FBReq[6] = state;
    FBReq[7] = 0;

    MB_RawCommand(FBReq);

    /* State of the domain */
    return FBReq[6];
}

/* The display id of a display number, or -1 if the firmware has no such display */
int32_t GetDisplayID(uint32_t display_num, struct VC4Base *VC4Base)
{
    APTR MailboxBase = VC4Base->vc4_MailboxBase;
    ULONG FBReq[7];

    FBReq[0] = 4 * 7;
    FBReq[1] = 0;
    FBReq[2] = MB_FB_GET_DISPLAY_ID;
    FBReq[3] = 4;
    FBReq[4] = 0;
    FBReq[5] = display_num;
    FBReq[6] = 0;

    MB_RawCommand(FBReq);

    /* Request done, and the tag answered with 4 bytes of data */
    if (FBReq[1] == MB_SUCCESS && FBReq[4] == 0x80000004)
        return (int32_t)FBReq[5];

    return -1;
}

/* Returns non zero if the firmware accepted the request */
int SetDisplayPower(int32_t display_id, uint32_t state, struct VC4Base *VC4Base)
{
    APTR MailboxBase = VC4Base->vc4_MailboxBase;
    ULONG FBReq[8];

    FBReq[0] = 4 * 8;
    FBReq[1] = 0;
    FBReq[2] = MB_FB_SET_DISPLAY_POWER;
    FBReq[3] = 8;
    FBReq[4] = 0;
    FBReq[5] = display_id;
    FBReq[6] = state;
    FBReq[7] = 0;

    MB_RawCommand(FBReq);

    return FBReq[1] == MB_SUCCESS && FBReq[4] == 0x80000004;
}

void init_display(struct Size dimensions, uint8_t depth, void **framebuffer, uint32_t *pitch, struct VC4Base *VC4Base)
{
    SetPhysicalSize(dimensions, VC4Base);
    SetVirtualSize(dimensions, VC4Base);        // Virtual resolution: duplicate physical size...
    SetDepth(depth, VC4Base);

    AllocateBuffer(64, framebuffer, NULL, VC4Base);

    if (pitch)
        *pitch = GetPitch(VC4Base);
}

uint32_t upload_code(const void *code, uint32_t code_size, struct VC4Base *VC4Base)
{
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    ULONG handle;
    ULONG phys_addr;
    UBYTE *ptr;

    /* Allocate buffer for the code on VC4, 4 byte aligned */
    handle = AllocateMemory(code_size, 4, MEM_FLAG_COHERENT | MEM_FLAG_DIRECT | MEM_FLAG_HINT_PERMALOCK, VC4Base);

    /* Lock the block so that it remains alive all the time. This is the address in VPU's view! */
    phys_addr = LockMemory(handle, VC4Base);

    /* Convert address to CPU view, upload code there */
    ptr = (UBYTE *)(phys_addr & 0x3fffffff);
    for (int i=0; i < code_size; i++) {
        ptr[i] = ((UBYTE*)code)[i];
    }

    /* Clear caches to make sure code is in VPU's accessible memory */
    CacheClearE(ptr, code_size, CACRF_ClearD);

    /* Return back physical address */
    return phys_addr;
}
