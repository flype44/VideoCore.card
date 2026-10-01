#ifndef STUB_PROTO_UNICAM_H
#define STUB_PROTO_UNICAM_H
#include <exec/types.h>
#include <resources/unicam.h>
ULONG UnicamGetConfig(void);
ULONG UnicamGetMode(void);
ULONG UnicamGetSize(void);
ULONG UnicamGetCropSize(void);
ULONG UnicamGetCropOffset(void);
ULONG UnicamGetKernel(void);
APTR  UnicamGetFramebuffer(void);
ULONG UnicamGetFramebufferSize(void);
void  UnicamStart(APTR buffer, ULONG a, ULONG b, ULONG c, ULONG d, ULONG e);
ULONG UnicamConstructDL(APTR dl, ULONG offset);
#endif
