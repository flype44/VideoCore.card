#ifndef STUB_PROTO_EXEC_H
#define STUB_PROTO_EXEC_H
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
void *AllocMem(ULONG size, ULONG flags);
void FreeMem(void *p, ULONG size);
void CacheClearE(void *addr, ULONG len, ULONG caches);
void *OpenLibrary(const char *name, ULONG version);
void CloseLibrary(void *lib);
void *OpenResource(const char *name);
void Forbid(void);
void Permit(void);
void Disable(void);
void Enable(void);
#endif
