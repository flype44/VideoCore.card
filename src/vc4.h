#ifndef _VC4_H
#define _VC4_H

#include <stdint.h>
#include "boardinfo.h"

/* The display list memory of the HVS */
#define VC4_DISPLAY_LIST        (HVS_BASE + 0x2000)

#define VC4_CONTROL_FORMAT(n)       (n & 0xf)
#define VC4_CONTROL_END             (1<<31)
#define VC4_CONTROL_VALID           (1<<30)
#define VC4_CONTROL_WORDS(n)        (((n) & 0x3f) << 24)
#define VC4_CONTROL0_FIXED_ALPHA    (1<<19)
#define VC4_CONTROL0_HFLIP          (1<<16)
#define VC4_CONTROL0_VFLIP          (1<<15)
#define VC4_CONTROL_PIXEL_ORDER(n)  ((n & 3) << 13)
#define VC4_CONTROL_SCL1(scl)       ((scl) << 8)
#define VC4_CONTROL_SCL0(scl)       ((scl) << 5)
#define VC4_CONTROL_UNITY           (1<<4)

#define VC4_POS0_X(n) (n & 0xfff)
#define VC4_POS0_Y(n) ((n & 0xfff) << 12)
#define VC4_POS0_ALPHA(n) ((n & 0xff) << 24)

#define VC4_POS1_W(n) (n & 0xffff)
#define VC4_POS1_H(n) ((n & 0xffff) << 16)

#define VC4_POS2_W(n) (n & 0xffff)
#define VC4_POS2_H(n) ((n & 0xffff) << 16)

#define VC4_SCALER_POS2_ALPHA_MODE_MASK             0xc0000000
#define VC4_SCALER_POS2_ALPHA_MODE_SHIFT            30
#define VC4_SCALER_POS2_ALPHA_MODE_PIPELINE         0
#define VC4_SCALER_POS2_ALPHA_MODE_FIXED            1
#define VC4_SCALER_POS2_ALPHA_MODE_FIXED_NONZERO    2
#define VC4_SCALER_POS2_ALPHA_MODE_FIXED_OVER_0x07  3
#define VC4_SCALER_POS2_ALPHA_PREMULT               (1 << 29)
#define VC4_SCALER_POS2_ALPHA_MIX                   (1 << 28)

#define VC4_SCALER_POS2_HEIGHT_MASK                 0x0fff0000
#define VC4_SCALER_POS2_HEIGHT_SHIFT                16

#define VC4_SCALER_POS2_WIDTH_MASK                  0x00000fff
#define VC4_SCALER_POS2_WIDTH_SHIFT                 0

/* In chip.h: struct ChipFamily */
extern const struct ChipFamily VC4_Family;

#endif /* _VC4_H */
