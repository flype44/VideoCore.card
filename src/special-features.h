#ifndef _SPECIAL_FEATURES_H
#define _SPECIAL_FEATURES_H

#include "boardinfo.h"

/*
    The special features of Picasso96 (CreateFeature, SetFeatureAttrs, GetFeatureAttrs and DeleteFeature of
    BoardInfo): picture in picture windows and the like, see src/CLAUDE.md. Gives the BoardInfo the four hooks which
    send each feature type to its code, and BIF_VIDEOWINDOW. For now only the memory window (SFT_MEMORYWINDOW) is
    made, and only by a family of the chip which can write the plane of a window (VC6). On the others the BoardInfo
    is left as it is and Picasso96 creates no feature.
*/
void Special_Init(struct BoardInfo *bi);

#endif /* _SPECIAL_FEATURES_H */
