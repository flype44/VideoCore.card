/*
    The dispatcher of the special features of Picasso96, see special-features.h.
*/

#include <exec/types.h>
#include <utility/tagitem.h>

#include <common/compiler.h>

#include "videocore.h"
#include "boardinfo.h"
#include "chip.h"
#include "memory-window.h"
#include "special-features.h"

static APTR Special_CreateFeature(REGARG(struct BoardInfo *bi, "a0"), REGARG(ULONG type, "d0"),
                                  REGARG(struct TagItem *tags, "a1"))
{
    switch (type)
    {
        case SFT_MEMORYWINDOW:
            return MemoryWindow_Create(bi, tags);

        default:
            return NULL;
    }
}

static ULONG Special_SetFeatureAttrs(REGARG(struct BoardInfo *bi, "a0"), REGARG(APTR feature, "a1"),
                                     REGARG(ULONG type, "d0"), REGARG(struct TagItem *tags, "a2"))
{
    switch (type)
    {
        case SFT_MEMORYWINDOW:
            return MemoryWindow_Set(bi, feature, tags);

        default:
            return 0;
    }
}

static ULONG Special_GetFeatureAttrs(REGARG(struct BoardInfo *bi, "a0"), REGARG(APTR feature, "a1"),
                                     REGARG(ULONG type, "d0"), REGARG(struct TagItem *tags, "a2"))
{
    switch (type)
    {
        case SFT_MEMORYWINDOW:
            return MemoryWindow_Get(bi, feature, tags);

        default:
            return 0;
    }
}

static BOOL Special_DeleteFeature(REGARG(struct BoardInfo *bi, "a0"), REGARG(APTR feature, "a1"),
                                  REGARG(ULONG type, "d0"))
{
    switch (type)
    {
        case SFT_MEMORYWINDOW:
            return MemoryWindow_Delete(bi, feature);

        default:
            return FALSE;
    }
}

void Special_Init(struct BoardInfo *bi)
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)bi->CardBase;

    /* The memory window is a plane of the display list: the family has to know how to write it */
    if (VideoCoreBase->vc_Family->WriteWindow == NULL)
        return;

    bi->CreateFeature = (void *)Special_CreateFeature;
    bi->SetFeatureAttrs = (void *)Special_SetFeatureAttrs;
    bi->GetFeatureAttrs = (void *)Special_GetFeatureAttrs;
    bi->DeleteFeature = (void *)Special_DeleteFeature;

    /* The flag of the picture in picture of a memory area */
    bi->Flags |= BIF_VIDEOWINDOW;
}
