/*
    What the driver gets from AmigaOS, the mailbox and the other modules, for the host test:
    memory, a few libraries which do nothing, and the functions of hvs.c which only leave a mark
    in the display list so that a change in the way they are called shows.
*/
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <exec/types.h>

/* stdlib.h and the AmigaOS headers both define struct timeval: the two are never included together */
void *calloc(size_t n, size_t size);
void free(void *p);
struct VideoCoreBase;

void bug(const char *format, ...) { (void)format; }

void *AllocMem(ULONG size, ULONG flags) { (void)flags; return calloc(1, size); }
void FreeMem(void *p, ULONG size) { (void)size; free(p); }
void CacheClearE(void *addr, ULONG len, ULONG caches) { (void)addr; (void)len; (void)caches; }
void *OpenLibrary(const char *name, ULONG version) { (void)name; (void)version; return (void *)1; }
void CloseLibrary(void *lib) { (void)lib; }
void *OpenResource(const char *name) { (void)name; return NULL; }
void Forbid(void) {}
void Permit(void) {}

/* The state of unicam.resource, the tests set it */
ULONG host_unicam_config = 0;
ULONG UnicamGetConfig(void) { return host_unicam_config; }
ULONG UnicamGetMode(void) { return 0x0000200c; }
ULONG UnicamGetSize(void) { return (720 << 16) | 576; }
ULONG UnicamGetCropSize(void) { return (720 << 16) | 576; }
ULONG UnicamGetCropOffset(void) { return 0; }
ULONG UnicamGetKernel(void) { return 0; }
APTR  UnicamGetFramebuffer(void) { return (APTR)0x30000000; }
ULONG UnicamGetFramebufferSize(void) { return 720 * 576 * 2; }
void  UnicamStart(APTR b, ULONG a1, ULONG a2, ULONG a3, ULONG a4, ULONG a5) { (void)b; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; }
ULONG UnicamConstructDL(APTR dl, ULONG offset) { if (dl) ((uint32_t *)dl)[offset] = 0x0dcdc0de; return 20; }

ULONG host_ieeespflt(LONG v) { float f = (float)v; ULONG r; memcpy(&r, &f, 4); return r; }
ULONG host_ieeespdiv(ULONG a, ULONG b) { float fa, fb, fr; ULONG r; memcpy(&fa, &a, 4); memcpy(&fb, &b, 4); fr = fa / fb; memcpy(&r, &fr, 4); return r; }

/* hvs.c: the kernels are not built, they leave a mark at their offset */
int compute_nearest_neighbour_kernel(volatile uint32_t *dl, ULONG offset) { dl[offset] = 0x4e4e4e4e; return 0; }
int compute_scaling_kernel(volatile uint32_t *dl, ULONG offset, ULONG b, ULONG c) { dl[offset] = 0x53535353 ^ b ^ c; return 0; }

/* mbox.c: the calls to the firmware do nothing, the functions under test do not make them */
int BlankScreen(int blank, struct VideoCoreBase *base) { (void)blank; (void)base; return 0; }
int SetDisplayPower(int32_t display_id, uint32_t state, struct VideoCoreBase *base) { (void)display_id; (void)state; (void)base; return 1; }
