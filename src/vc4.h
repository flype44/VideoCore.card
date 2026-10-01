#ifndef _VC4_H
#define _VC4_H

#include <stdint.h>
#include "boardinfo.h"

#define CONTROL_FORMAT(n)       (n & 0xf)
#define CONTROL_END             (1<<31)
#define CONTROL_VALID           (1<<30)
#define CONTROL_WORDS(n)        (((n) & 0x3f) << 24)
#define CONTROL0_FIXED_ALPHA    (1<<19)
#define CONTROL0_HFLIP          (1<<16)
#define CONTROL0_VFLIP          (1<<15)
#define CONTROL_PIXEL_ORDER(n)  ((n & 3) << 13)
#define CONTROL_SCL1(scl)       ((scl) << 8)
#define CONTROL_SCL0(scl)       ((scl) << 5)
#define CONTROL_UNITY           (1<<4)

#define POS0_X(n) (n & 0xfff)
#define POS0_Y(n) ((n & 0xfff) << 12)
#define POS0_ALPHA(n) ((n & 0xff) << 24)

#define POS1_W(n) (n & 0xffff)
#define POS1_H(n) ((n & 0xffff) << 16)

#define POS2_W(n) (n & 0xffff)
#define POS2_H(n) ((n & 0xffff) << 16)

#define SCALER_POS2_ALPHA_MODE_MASK             0xc0000000
#define SCALER_POS2_ALPHA_MODE_SHIFT            30
#define SCALER_POS2_ALPHA_MODE_PIPELINE         0
#define SCALER_POS2_ALPHA_MODE_FIXED            1
#define SCALER_POS2_ALPHA_MODE_FIXED_NONZERO    2
#define SCALER_POS2_ALPHA_MODE_FIXED_OVER_0x07  3
#define SCALER_POS2_ALPHA_PREMULT               (1 << 29)
#define SCALER_POS2_ALPHA_MIX                   (1 << 28)

#define SCALER_POS2_HEIGHT_MASK                 0x0fff0000
#define SCALER_POS2_HEIGHT_SHIFT                16

#define SCALER_POS2_WIDTH_MASK                  0x00000fff
#define SCALER_POS2_WIDTH_SHIFT                 0

void VC4_InitChip(struct BoardInfo *bi);

#endif /* _VC4_H */
