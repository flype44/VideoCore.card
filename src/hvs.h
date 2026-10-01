#ifndef _HVS_H
#define _HVS_H

#include <exec/types.h>
#include <stdint.h>

int compute_nearest_neighbour_kernel(volatile uint32_t *dlist_memory, ULONG offset);
int compute_scaling_kernel(volatile uint32_t *dlist_memory, ULONG offset, ULONG b, ULONG c);

struct VC4Base;

/* What the task does for the messages of the clients: they touch the HVS display lists and registers.
   The ones which change the display list wait for the vertical blank first. */
void HVS_Init(struct VC4Base *VC4Base);
void HVS_ShowUnicam(struct VC4Base *VC4Base);
void HVS_SetKernel(struct VC4Base *VC4Base, ULONG kernel, ULONG b, ULONG c);
ULONG HVS_GetScaler(struct VC4Base *VC4Base);
void HVS_SetScaler(struct VC4Base *VC4Base, ULONG val);
ULONG HVS_GetPhase(struct VC4Base *VC4Base);
void HVS_SetPhase(struct VC4Base *VC4Base, ULONG val);
void HVS_UpdateUnicamDL(struct VC4Base *VC4Base);

#endif /* _HVS_H */
