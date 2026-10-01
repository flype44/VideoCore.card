/*
    HVS support shared by the VideoCore families: the scaling kernels of the display list memory.
*/

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>

#include <proto/exec.h>
#include <proto/mathieeesingbas.h>
#include <proto/unicam.h>

#include <resources/unicam.h>

#include <common/compiler.h>

/* Make sure MathIEEE will not force gcc to do weird FLOT convertions when calling lib functions */
#define FLOAT ULONG

#include "videocore.h"
#include "vc4.h"
#include "buddyalloc.h"
#include "hvs.h"

static int mitchell_netravali(ULONG x, ULONG b, ULONG c, struct Library *MathIeeeSingBasBase)
{
    const ULONG float_6 = 0x40c00000;
    const ULONG float_0p5 = 0x3f000000;
    const ULONG float_255 = 0x437f0000;
    const ULONG float_2 = 0x40000000;
    const ULONG float_1 = 0x3f800000;

    ULONG k;

    x = IEEESPAbs(x);
    
    if (IEEESPCmp(x, float_1) < 0) {
        const ULONG float_18 = 0x41900000;
        const ULONG float_12 = 0x41400000;
        const ULONG float_9 = 0x41100000;
        
        ULONG a1, a2, a3;
        a1 = IEEESPAdd(
                float_12,
                IEEESPNeg(
                    IEEESPAdd(
                        IEEESPMul(
                            float_9,
                            b),
                        IEEESPMul(
                            float_6,
                            c)))
            );
        a1 = IEEESPMul(IEEESPMul(IEEESPMul(a1, x), x), x);

        a2 = IEEESPSub(
                IEEESPAdd(
                    IEEESPMul(float_12, b),
                    IEEESPMul(float_6, c)),
                float_18
            );
        a2 = IEEESPMul(IEEESPMul(a2, x), x);

        a3 = IEEESPAdd(
                float_6,
                IEEESPNeg(
                    IEEESPMul(
                        float_2,
                        b)
                    )
                );   

        k = IEEESPAdd(IEEESPAdd(a1, a2), a3);
        //k = (12.0 - 9.0 * b - 6.0 * c) * x * x * x + (-18.0 + 12.0 * b + 6.0 * c) * x * x + (6.0 - 2.0 * b);
    }
    else if (IEEESPCmp(x, float_2) < 0) {
        const ULONG float_8 = 0x41000000;
        const ULONG float_24 = 0x41c00000;
        const ULONG float_30 = 0x41f00000;
        const ULONG float_m12 = 0xc1400000;
        const ULONG float_m48 = 0xc2400000;
        ULONG a1, a2, a3;
        
        a1 = IEEESPNeg(
            IEEESPAdd(
                b,
                IEEESPMul(
                    float_6,
                    c
                )
            )
        );
        a1 = IEEESPMul(IEEESPMul(IEEESPMul(a1, x), x), x);

        a2 = IEEESPAdd(
            IEEESPMul(
                float_6,
                b
            ),
            IEEESPMul(
                float_30,
                c
            )
        );
        a2 = IEEESPMul(IEEESPMul(a2, x), x);

        a3 = IEEESPMul(
            IEEESPAdd(
                IEEESPMul(
                    float_m12,
                    b
                ),
                IEEESPMul(
                    float_m48,
                    c
                )
            ),
            x
        );

        k = IEEESPAdd(
            a1,
            IEEESPAdd(
                a2,
                a3
            )
        );
        k = IEEESPAdd(
            k,
            IEEESPAdd(
                IEEESPMul(
                    float_8,
                    b
                ),
                IEEESPMul(
                    float_24,
                    c
                )
            )
        );

        //k = (-b - 6.0 * c) * x * x * x + (6.0 * b + 30.0 * c) * x * x + (-12.0 * b - 48.0 * c) * x + 8.0 * b + 24.0 * c;
    }
    else
        k = 0;
    
    k = IEEESPMul(
        float_255,
        k
    );
    k = IEEESPAdd(
        IEEESPDiv(
            k,
            float_6
        ),
        float_0p5
    );
    //k = 255.0 * k / 6.0 + 0.5;

    return IEEESPFix(k);
}

int compute_scaling_kernel(volatile uint32_t *dlist_memory, ULONG offset, ULONG b, ULONG c)
{
    struct ExecBase *SysBase = *(struct ExecBase **)4;
    struct Library *MathIeeeSingBasBase = OpenLibrary("mathieeesingbas.library", 0);

    if (MathIeeeSingBasBase != NULL)
    {
        uint32_t half_kernel[6] = {0, 0, 0, 0, 0, 0};

        for (int i=0; i < 16; i++) {
            const ULONG float_7p5 = 0x40f00000;
            const ULONG float_2 = 0x40000000;
            ULONG x = IEEESPFlt(i);
            x = IEEESPDiv(x, float_7p5);
            x = IEEESPNeg(x);
            x = IEEESPAdd(x, float_2);

            int val = mitchell_netravali(x, b, c, MathIeeeSingBasBase);

            half_kernel[i / 3] |= (val & 0x1ff) << (9 * (i % 3));
        }
        half_kernel[5] |= half_kernel[5] << 9;

        for (int i=0; i<11; i++) {
            if (i < 6) {
                wr32le(&dlist_memory[offset + i], half_kernel[i]);
            } else {
                wr32le(&dlist_memory[offset + i], half_kernel[11 - i - 1]);
            }
        }

        CloseLibrary(MathIeeeSingBasBase);
    }

    return offset;
}

int compute_nearest_neighbour_kernel(volatile uint32_t *dlist_memory, ULONG offset)
{
    struct ExecBase *SysBase = *(struct ExecBase **)4;
    uint32_t half_kernel[6] = {0, 0, 0, 0, 0, 0};

    for (int i=0; i < 16; i++) {
        int val = i < 8 ? 0 : 255;
        half_kernel[i / 3] |= (val & 0x1ff) << (9 * (i % 3));
    }
    half_kernel[5] |= half_kernel[5] << 9;

    for (int i=0; i<11; i++) {
        if (i < 6) {
            wr32le(&dlist_memory[offset + i], half_kernel[i]);
        } else {
            wr32le(&dlist_memory[offset + i], half_kernel[11 - i - 1]);
        }
    }

    return offset;
}

/* Wait for the vertical blank before the display list is updated */
static void HVS_WaitVBlank(struct VideoCoreBase *VideoCoreBase)
{
    volatile ULONG *stat = (ULONG*)(0xf2400000 + SCALER_DISPSTAT1);

    do { asm volatile("nop"); } while((LE32(*stat) & 0xfff) != VideoCoreBase->vc_DispSize.height);
}

void HVS_SetKernel(struct VideoCoreBase *VideoCoreBase, ULONG kernel, ULONG b, ULONG c)
{
    ULONG new_scaling_kernel = BuddyAlloc(VideoCoreBase, 11);
    ULONG kernel_start = BUDDY_OFFSET(new_scaling_kernel);

    if (kernel)
        compute_scaling_kernel(VideoCoreBase->vc_DisplayList, kernel_start, b, c);
    else
        compute_nearest_neighbour_kernel(VideoCoreBase->vc_DisplayList, kernel_start);

    if (VideoCoreBase->vc_Kernel)
    {
        HVS_WaitVBlank(VideoCoreBase);

        wr32le(&VideoCoreBase->vc_Kernel[0], kernel_start);
        wr32le(&VideoCoreBase->vc_Kernel[1], kernel_start);
        wr32le(&VideoCoreBase->vc_Kernel[2], kernel_start);
        wr32le(&VideoCoreBase->vc_Kernel[3], kernel_start);

        wr32le(&VideoCoreBase->vc_MouseCoord[12], kernel_start);
        wr32le(&VideoCoreBase->vc_MouseCoord[13], kernel_start);
        wr32le(&VideoCoreBase->vc_MouseCoord[14], kernel_start);
        wr32le(&VideoCoreBase->vc_MouseCoord[15], kernel_start);
    }

    BuddyFree(VideoCoreBase, VideoCoreBase->vc_ScalingKernel);
    VideoCoreBase->vc_ScalingKernel = new_scaling_kernel;
}

ULONG HVS_GetScaler(struct VideoCoreBase *VideoCoreBase)
{
    if (VideoCoreBase->vc_PlaneScalerX)
        return (LE32(*VideoCoreBase->vc_PlaneScalerX) >> 30) & 3;

    return 0;
}

void HVS_SetScaler(struct VideoCoreBase *VideoCoreBase, ULONG scaler)
{
    HVS_WaitVBlank(VideoCoreBase);

    if (VideoCoreBase->vc_PlaneScalerX) {
        ULONG val = LE32(*VideoCoreBase->vc_PlaneScalerX);
        val = (val & 0x3fffffff) | (scaler << 30);
        wr32le(VideoCoreBase->vc_PlaneScalerX, val);
    }
    if (VideoCoreBase->vc_PlaneScalerY) {
        ULONG val = LE32(*VideoCoreBase->vc_PlaneScalerY);
        val = (val & 0x3fffffff) | (scaler << 30);
        wr32le(VideoCoreBase->vc_PlaneScalerY, val);
    }

    if (VideoCoreBase->vc_ScaleX != 0x10000) {
        ULONG val = LE32(VideoCoreBase->vc_MouseCoord[9]);
        val = (val & 0x3fffffff) | (scaler << 30);
        wr32le(&VideoCoreBase->vc_MouseCoord[9], val);

        val = LE32(VideoCoreBase->vc_MouseCoord[10]);
        val = (val & 0x3fffffff) | (scaler << 30);
        wr32le(&VideoCoreBase->vc_MouseCoord[10], val);
    }
}

ULONG HVS_GetPhase(struct VideoCoreBase *VideoCoreBase)
{
    if (VideoCoreBase->vc_PlaneScalerX)
        return LE32(*VideoCoreBase->vc_PlaneScalerX) & 0xff;

    return 0;
}

void HVS_SetPhase(struct VideoCoreBase *VideoCoreBase, ULONG phase)
{
    HVS_WaitVBlank(VideoCoreBase);

    if (VideoCoreBase->vc_PlaneScalerX) {
        ULONG val = LE32(*VideoCoreBase->vc_PlaneScalerX);
        val = (val & 0xffffff00) | (phase & 0xff);
        wr32le(VideoCoreBase->vc_PlaneScalerX, val);
    }
    if (VideoCoreBase->vc_PlaneScalerY) {
        ULONG val = LE32(*VideoCoreBase->vc_PlaneScalerY);
        val = (val & 0xffffff00) | (phase & 0xff);
        wr32le(VideoCoreBase->vc_PlaneScalerY, val);
    }

    if (VideoCoreBase->vc_ScaleX != 0x10000) {
        ULONG val = LE32(VideoCoreBase->vc_MouseCoord[9]);
        val = (val & 0xffffff00) | (phase & 0xff);
        wr32le(&VideoCoreBase->vc_MouseCoord[9], val);

        val = LE32(VideoCoreBase->vc_MouseCoord[10]);
        val = (val & 0xffffff00) | (phase & 0xff);
        wr32le(&VideoCoreBase->vc_MouseCoord[10], val);
    }
}

void HVS_UpdateUnicamDL(struct VideoCoreBase *VideoCoreBase)
{
    /* Check if unicam.resource is there and the version is right */
    APTR UnicamBase = VideoCoreBase->vc_UnicamBase;
    struct Library *ub = UnicamBase;

    if (ub != NULL && (ub->lib_Version > 1 || (ub->lib_Version == 1 && ub->lib_Revision >= 2))) {
        ULONG sz = (7 + UnicamConstructDL(NULL, 0)) & ~7;
        ULONG idx = BuddyAlloc(VideoCoreBase, sz);

        /* Alloc slot for unicam displaylist and initialize it by unicam itself */
        UnicamConstructDL(VideoCoreBase->vc_DisplayList, BUDDY_OFFSET(idx));

        if (VideoCoreBase->vc_UnicamVisible) {
            wr32le((volatile uint32_t *)0xf2400024, BUDDY_OFFSET(idx));
        }

        /* Set the new pointer to unicam display list */
        BuddyFree(VideoCoreBase, VideoCoreBase->vc_UnicamDL);
        VideoCoreBase->vc_UnicamDL = idx;
    }
}

/* Builds the scaling kernels and the display list of Unicam in the display list memory */
void HVS_Init(struct VideoCoreBase *VideoCoreBase)
{
    APTR UnicamBase = VideoCoreBase->vc_UnicamBase;

    VideoCoreBase->vc_ScalingKernel = BuddyAlloc(VideoCoreBase, 11);
    ULONG kernel_start = BUDDY_OFFSET(VideoCoreBase->vc_ScalingKernel);

    if (VideoCoreBase->vc_UseKernel)
        compute_scaling_kernel(VideoCoreBase->vc_DisplayList, kernel_start, VideoCoreBase->vc_Kernel_B, VideoCoreBase->vc_Kernel_C);
    else
        compute_nearest_neighbour_kernel(VideoCoreBase->vc_DisplayList, kernel_start);

    VideoCoreBase->vc_UnityKernel = BuddyAlloc(VideoCoreBase, 11);
    ULONG unity_kernel = BUDDY_OFFSET(VideoCoreBase->vc_UnityKernel);

    compute_nearest_neighbour_kernel(VideoCoreBase->vc_DisplayList, unity_kernel);

    /* If unicam.resource is new enough, let it construct unicam display list */
    struct Library *ub = (struct Library *)UnicamBase;
    if (ub != NULL)
    {
        if (ub->lib_Version > 1 || (ub->lib_Version == 1 && ub->lib_Revision >= 2)) {
            ULONG sz = (7 + UnicamConstructDL(NULL, 0)) & ~7;
            ULONG idx = 0;
            
            bug("[VC] Constructing Unicam DL using unicam.resource\n");

            VideoCoreBase->vc_UnicamDL = BuddyAlloc(VideoCoreBase, sz);
            idx = BUDDY_OFFSET(VideoCoreBase->vc_UnicamDL);

            UnicamConstructDL(VideoCoreBase->vc_DisplayList, idx);
        }
        else
        {
            VideoCoreBase->vc_ConstructUnicamDL(VideoCoreBase);
        }
    }
}

/* If Unicam was activated on boot the display list of Unicam has to be the displayed one */
void HVS_ShowUnicam(struct VideoCoreBase *VideoCoreBase)
{
    APTR UnicamBase = VideoCoreBase->vc_UnicamBase;

    if (UnicamBase != NULL)
    {
        if ((UnicamGetConfig() & UNICAMF_BOOT) != 0) 
        {
            VideoCoreBase->vc_UnicamVisible = TRUE;
            /* Both vc4 and vc6 switch the same way */
            wr32le((volatile uint32_t *)0xf2400024, VideoCoreBase->vc_UnicamDL);
        }
    }
}
