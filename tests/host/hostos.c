/* The parts of the host test which need the host headers (they clash with the AmigaOS ones) */
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

/* The registers and display list memory of the HVS are at fixed addresses, give the driver some memory there */
int host_map_fixed(unsigned long addr, unsigned long len)
{
    void *p = mmap((void *)addr, len, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE | MAP_NORESERVE, -1, 0);
    if (p == MAP_FAILED || (unsigned long)p != addr) {
        fprintf(stderr, "cannot map %lx\n", addr);
        return 0;
    }
    return 1;
}

void host_clear(unsigned long addr, unsigned long len) { memset((void *)addr, 0, len); }
