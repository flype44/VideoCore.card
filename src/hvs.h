#ifndef _HVS_H
#define _HVS_H

#include <exec/types.h>
#include <stdint.h>

int compute_nearest_neighbour_kernel(volatile uint32_t *dlist_memory, ULONG offset);
int compute_scaling_kernel(volatile uint32_t *dlist_memory, ULONG offset, ULONG b, ULONG c);

struct VideoCoreBase;

/* What the task does for the messages of the clients: they touch the HVS display lists and registers.
   The ones which change the display list wait for the vertical blank first. */
void HVS_Init(struct VideoCoreBase *VideoCoreBase);
void HVS_ShowUnicam(struct VideoCoreBase *VideoCoreBase);
void HVS_SetKernel(struct VideoCoreBase *VideoCoreBase, ULONG kernel, ULONG b, ULONG c);
ULONG HVS_GetScaler(struct VideoCoreBase *VideoCoreBase);
void HVS_SetScaler(struct VideoCoreBase *VideoCoreBase, ULONG val);
ULONG HVS_GetPhase(struct VideoCoreBase *VideoCoreBase);
void HVS_SetPhase(struct VideoCoreBase *VideoCoreBase, ULONG val);
void HVS_UpdateUnicamDL(struct VideoCoreBase *VideoCoreBase);

#endif /* _HVS_H */
