#ifndef _MBOX_H
#define _MBOX_H

#include "emu68-vc4.h"
#include "mailbox.h"
#include <stdint.h>

/* The functions are named after the mailbox tag they send */
void GetVCMemory(void **base, uint32_t *size, struct VC4Base *VC4Base);
struct Size GetPhysicalSize(struct VC4Base *VC4Base);
void SetPhysicalSize(struct Size size, struct VC4Base *VC4Base);
void SetVirtualSize(struct Size size, struct VC4Base *VC4Base);
void SetDepth(uint8_t depth, struct VC4Base *VC4Base);
void AllocateBuffer(uint32_t alignment, void **base, uint32_t *size, struct VC4Base *VC4Base);
uint32_t GetPitch(struct VC4Base *VC4Base);
void ReleaseBuffer(struct VC4Base *VC4Base);
int BlankScreen(int blank, struct VC4Base *VC4Base);
uint32_t AllocateMemory(uint32_t size, uint32_t alignment, uint32_t flags, struct VC4Base *VC4Base);
uint32_t LockMemory(uint32_t handle, struct VC4Base *VC4Base);
uint32_t ExecuteCode(uint32_t addr, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3,
                     uint32_t arg4, uint32_t arg5, struct VC4Base *VC4Base);
uint32_t SetDomainState(uint32_t domain, uint32_t state, struct VC4Base *VC4Base);
int32_t GetDisplayID(uint32_t display_num, struct VC4Base *VC4Base);
int SetDisplayPower(int32_t display_id, uint32_t state, struct VC4Base *VC4Base);

/* Several calls together */
void init_display(struct Size dimensions, uint8_t depth, void **framebuffer, uint32_t *pitch, struct VC4Base *VC4Base);
uint32_t upload_code(const void *code, uint32_t code_size, struct VC4Base *VC4Base);

#endif /* _MBOX_H */
