#ifndef _HVS_H
#define _HVS_H

#include <exec/types.h>
#include <stdint.h>

int compute_nearest_neighbour_kernel(volatile uint32_t *dlist_memory, ULONG offset);
int compute_scaling_kernel(volatile uint32_t *dlist_memory, ULONG offset, ULONG b, ULONG c);

#endif /* _HVS_H */
