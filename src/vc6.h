#ifndef _VC6_H
#define _VC6_H

#include <stdint.h>
#include "boardinfo.h"

#define VC6_CONTROL_FORMAT(n)       (n & 0x1f)
#define VC6_CONTROL_END             (1<<31)
#define VC6_CONTROL_VALID           (1<<30)
#define VC6_CONTROL_WORDS(n)        (((n) & 0x3f) << 24)
#define VC6_CONTROL0_FIXED_ALPHA    (1<<19)
#define VC6_CONTROL0_HFLIP          (1<<31)
#define VC6_CONTROL0_VFLIP          (1<<15)
#define VC6_CONTROL_PIXEL_ORDER(n)  ((n & 3) << 13)
#define VC6_CONTROL_SCL1(scl)       ((scl) << 8)
#define VC6_CONTROL_SCL0(scl)       ((scl) << 5)
#define VC6_CONTROL_UNITY           (1<<15)
#define VC6_CONTROL_ALPHA_EXPAND    (1<<12)
#define VC6_CONTROL_RGB_EXPAND      (1<<11)

#define VC6_POS0_X(n) (n & 0x2fff)
#define VC6_POS0_Y(n) ((n & 0x2fff) << 16)

#define VC6_POS1_W(n) (n & 0xffff)
#define VC6_POS1_H(n) ((n & 0xffff) << 16)

#define VC6_POS2_W(n) (n & 0xffff)
#define VC6_POS2_H(n) ((n & 0xffff) << 16)

#define VC6_SCALER_POS2_ALPHA_MODE_MASK             0xc0000000
#define VC6_SCALER_POS2_ALPHA_MODE_SHIFT            30
#define VC6_SCALER_POS2_ALPHA_MODE_PIPELINE         0
#define VC6_SCALER_POS2_ALPHA_MODE_FIXED            1
#define VC6_SCALER_POS2_ALPHA_MODE_FIXED_NONZERO    2
#define VC6_SCALER_POS2_ALPHA_MODE_FIXED_OVER_0x07  3
#define VC6_SCALER_POS2_ALPHA_PREMULT               (1 << 29)
#define VC6_SCALER_POS2_ALPHA_MIX                   (1 << 28)
#define VC6_SCALER_POS2_ALPHA(n)                    (((n) << 4) & 0xfff0)

#define VC6_SCALER_POS2_HEIGHT_MASK                 0x3fff0000
#define VC6_SCALER_POS2_HEIGHT_SHIFT                16

#define VC6_SCALER_POS2_WIDTH_MASK                  0x00003fff
#define VC6_SCALER_POS2_WIDTH_SHIFT                 0

void VC6_InitChip(struct BoardInfo *bi);

#endif /* _VC6_H */
