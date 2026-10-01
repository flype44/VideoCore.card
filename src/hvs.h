#ifndef _HVS_H
#define _HVS_H

#include <exec/types.h>
#include <stdint.h>

int compute_nearest_neighbour_kernel(volatile uint32_t *dlist_memory, ULONG offset);
int compute_scaling_kernel(volatile uint32_t *dlist_memory, ULONG offset, ULONG b, ULONG c);

/* The pixel formats, the orders of their components and the registers of the HVS */
enum hvs_pixel_format {
    /* 8bpp */
    HVS_PIXEL_FORMAT_RGB332 = 0,
    /* 16bpp */
    HVS_PIXEL_FORMAT_RGBA4444 = 1,
    HVS_PIXEL_FORMAT_RGB555 = 2,
    HVS_PIXEL_FORMAT_RGBA5551 = 3,
    HVS_PIXEL_FORMAT_RGB565 = 4,
    /* 24bpp */
    HVS_PIXEL_FORMAT_RGB888 = 5,
    HVS_PIXEL_FORMAT_RGBA6666 = 6,
    /* 32bpp */
    HVS_PIXEL_FORMAT_RGBA8888 = 7,

    HVS_PIXEL_FORMAT_YCBCR_YUV420_3PLANE = 8,
    HVS_PIXEL_FORMAT_YCBCR_YUV420_2PLANE = 9,
    HVS_PIXEL_FORMAT_YCBCR_YUV422_3PLANE = 10,
    HVS_PIXEL_FORMAT_YCBCR_YUV422_2PLANE = 11,
    HVS_PIXEL_FORMAT_H264 = 12,
    HVS_PIXEL_FORMAT_PALETTE = 13,
    HVS_PIXEL_FORMAT_YUV444_RGB = 14,
    HVS_PIXEL_FORMAT_AYUV444_RGB = 15,
    HVS_PIXEL_FORMAT_RGBA1010102 = 16,
    HVS_PIXEL_FORMAT_YCBCR_10BIT = 17,
};

enum palette_type {
    PALETTE_NONE = 0, 
    PALETTE_1BPP = 1,
    PALETTE_2BPP = 2,
    PALETTE_4BPP = 3,
    PALETTE_8BPP = 4,
};

#define HVS_PIXEL_ORDER_RGBA                    0
#define HVS_PIXEL_ORDER_BGRA                    1
#define HVS_PIXEL_ORDER_ARGB                    2
#define HVS_PIXEL_ORDER_ABGR                    3

#define HVS_PIXEL_ORDER_XBRG                    0
#define HVS_PIXEL_ORDER_XRBG                    1
#define HVS_PIXEL_ORDER_XRGB                    2
#define HVS_PIXEL_ORDER_XBGR                    3

#define HVS_PIXEL_ORDER_XYCBCR			        0
#define HVS_PIXEL_ORDER_XYCRCB			        1
#define HVS_PIXEL_ORDER_YXCBCR			        2
#define HVS_PIXEL_ORDER_YXCRCB			        3

#define SCALER_CTL0_SCL_H_PPF_V_PPF             0
#define SCALER_CTL0_SCL_H_TPZ_V_PPF             1
#define SCALER_CTL0_SCL_H_PPF_V_TPZ             2
#define SCALER_CTL0_SCL_H_TPZ_V_TPZ             3
#define SCALER_CTL0_SCL_H_PPF_V_NONE            4
#define SCALER_CTL0_SCL_H_NONE_V_PPF            5
#define SCALER_CTL0_SCL_H_NONE_V_TPZ            6
#define SCALER_CTL0_SCL_H_TPZ_V_NONE            7

/* Word index of the palette of the 8 bit planes in the display list memory. Below it (up to 0x2ff) are the planes
   of the buddy allocator; the HVS also runs a list of an other channel at 0x334 (SCALER_DISPLIST0), which a palette
   there would overwrite and which would write its scratch words into the palette. */
#define HVS_PALETTE                             0x400

#define SCALER_DISPLIST0                        0x00000020
#define SCALER_DISPLIST1                        0x00000024
#define SCALER_DISPLIST2                        0x00000028

#define SCALER_DISPSTAT0                        0x00000048
#define SCALER_DISPSTAT1                        0x00000058
#define SCALER_DISPSTAT2                        0x00000068
/* The bits from low to high of a register, both included */
#define HVS_MASK(high, low)                     ((0xffffffffUL >> (31 - (high))) & ~((1UL << (low)) - 1))

#define SCALER_DISPSTATX_FRAME_COUNT_MASK       HVS_MASK(17, 12)
#define SCALER_DISPSTATX_FRAME_COUNT_SHIFT      12

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
