/*
    The special feature SFT_MEMORYWINDOW, see memory-window.h.
*/

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <utility/tagitem.h>

#include <proto/exec.h>
#include <proto/utility.h>

#include <common/compiler.h>

#include "videocore.h"
#include "boardinfo.h"
#include "chip.h"
#include "memory-window.h"

#define MWF_ACTIVE      (1 << 0)    /* FA_Active: the program wants the window to be seen */
#define MWF_ONBOARD     (1 << 1)    /* FA_Onboard: its screen is the one displayed */

#define MAX_SOURCE_WIDTH    4088

/* The formats the plane can show: those of the main plane, except the palette */
#define WINDOW_FORMATS ( \
    (1UL << RGBFB_A8R8G8B8) | (1UL << RGBFB_A8B8G8R8) | (1UL << RGBFB_B8G8R8A8) | (1UL << RGBFB_R8G8B8A8) | \
    (1UL << RGBFB_R8G8B8)   | (1UL << RGBFB_B8G8R8)   | \
    (1UL << RGBFB_R5G6B5PC) | (1UL << RGBFB_R5G5B5PC) | (1UL << RGBFB_R5G6B5) | (1UL << RGBFB_R5G5B5) | \
    (1UL << RGBFB_B5G6R5PC) | (1UL << RGBFB_B5G5R5PC))

struct MemoryWindow {
    struct BoardInfo *BoardInfo;
    ULONG             Flags;
    UWORD             Left;             /* the interior of the window, in pixels of the screen */
    UWORD             Top;
    UWORD             Width;
    UWORD             Height;
    UWORD             SourceWidth;      /* the bitmap */
    UWORD             SourceHeight;
    RGBFTYPE          Format;
    ULONG             Brightness;       /* FA_Brightness, taken as transparency: see memory-window.h */
    struct BitMap    *BitMap;
};

static struct BitMap *MemoryWindow_AllocBitMap(struct BoardInfo *bi, UWORD width, UWORD height, RGBFTYPE format)
{
    struct TagItem tags[] = {
        { ABMA_RGBFormat,   format },
        { ABMA_Clear,       TRUE },
        { ABMA_Displayable, TRUE },
        { ABMA_Visible,     TRUE },
        { ABMA_Alignment,   64 },
        { TAG_DONE,         0 }
    };

    return bi->AllocBitMap(bi, width, height, tags);
}

static void MemoryWindow_FreeBitMap(struct BoardInfo *bi, struct BitMap *bm)
{
    struct TagItem tags[] = { { TAG_DONE, 0 } };

    if (bm != NULL)
        bi->FreeBitMap(bi, bm, tags);
}

APTR MemoryWindow_Create(struct BoardInfo *bi, struct TagItem *tags)
{
    struct ExecBase *SysBase = bi->ExecBase;
    struct Library *UtilityBase = bi->UtilBase;
    struct MemoryWindow *mw;
    ULONG width = GetTagData(FA_SourceWidth, 0, tags);
    ULONG height = GetTagData(FA_SourceHeight, 0, tags);
    ULONG format = GetTagData(FA_Format, RGBFB_NONE, tags);

    if (!(bi->Flags & BIF_VIDEOWINDOW))
        return NULL;

    if (width == 0 || width > MAX_SOURCE_WIDTH || height == 0 || format >= 32 || !((WINDOW_FORMATS >> format) & 1))
        return NULL;

    mw = AllocMem(sizeof(struct MemoryWindow), MEMF_PUBLIC | MEMF_CLEAR);
    if (mw == NULL)
        return NULL;

    mw->BoardInfo = bi;
    mw->SourceWidth = width;
    mw->SourceHeight = height;
    mw->Format = format;
    mw->BitMap = MemoryWindow_AllocBitMap(bi, width, height, format);

    if (mw->BitMap == NULL)
    {
        FreeMem(mw, sizeof(struct MemoryWindow));
        return NULL;
    }

    /* The rest of the tags: position, state, ... */
    MemoryWindow_Set(bi, mw, tags);

    return mw;
}

ULONG MemoryWindow_Set(struct BoardInfo *bi, APTR feature, struct TagItem *tags)
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)bi->CardBase;
    struct Library *UtilityBase = bi->UtilBase;
    struct MemoryWindow *mw = feature;
    struct TagItem *tstate = tags;
    struct TagItem *tag;
    UWORD old_left = mw->Left;
    UWORD old_top = mw->Top;
    ULONG old_brightness = mw->Brightness;
    int new_bitmap = FALSE;
    int rebuild = FALSE;
    int shown;
    ULONG taken = 0;

    while ((tag = NextTagItem(&tstate)) != NULL)
    {
        ULONG value = tag->ti_Data;

        taken++;

        switch (tag->ti_Tag)
        {
            case FA_Active:
                if (value)
                    mw->Flags |= MWF_ACTIVE;
                else
                    mw->Flags &= ~MWF_ACTIVE;
                break;

            case FA_Onboard:
                if (value)
                    mw->Flags |= MWF_ONBOARD;
                else
                    mw->Flags &= ~MWF_ONBOARD;
                break;

            case FA_Left:
                mw->Left = value;
                break;

            case FA_Top:
                mw->Top = value;
                break;

            case FA_Width:
                mw->Width = value;
                break;

            case FA_Height:
                mw->Height = value;
                break;

            case FA_Brightness:
                mw->Brightness = value;
                break;

            case FA_SourceWidth:
                if (value != mw->SourceWidth && value > 0 && value <= MAX_SOURCE_WIDTH)
                {
                    mw->SourceWidth = value;
                    new_bitmap = TRUE;
                }
                break;

            case FA_SourceHeight:
                if (value != mw->SourceHeight && value > 0)
                {
                    mw->SourceHeight = value;
                    new_bitmap = TRUE;
                }
                break;

            case FA_Format:
                if (value != mw->Format && value < 32 && ((WINDOW_FORMATS >> value) & 1))
                {
                    mw->Format = value;
                    new_bitmap = TRUE;
                }
                break;
        }
    }

    /* A new size or format is a new bitmap, at another address: the plane must be written again */
    if (new_bitmap)
    {
        MemoryWindow_FreeBitMap(bi, mw->BitMap);
        mw->BitMap = MemoryWindow_AllocBitMap(bi, mw->SourceWidth, mw->SourceHeight, mw->Format);
        rebuild = TRUE;
    }

    shown = (mw->Flags & (MWF_ACTIVE | MWF_ONBOARD)) == (MWF_ACTIVE | MWF_ONBOARD) && mw->BitMap != NULL;

    if (shown)
    {
        if (VideoCoreBase->vc_MemoryWindow != mw)
        {
            VideoCoreBase->vc_MemoryWindow = mw;
            rebuild = TRUE;
        }
    }
    else if (VideoCoreBase->vc_MemoryWindow == mw)
    {
        VideoCoreBase->vc_MemoryWindow = NULL;
        rebuild = TRUE;
    }

    if (rebuild)
    {
        Chip_RebuildPlanes(bi);
    }
    else if (shown && (mw->Left != old_left || mw->Top != old_top || mw->Brightness != old_brightness))
    {
        /* The window moved or became more or less transparent: only its position and alpha words change */
        struct WindowPlane window;

        if (MemoryWindow_Plane(VideoCoreBase, &window))
            Chip_UpdateWindowPlane(VideoCoreBase, &window);
    }

    return taken;
}

ULONG MemoryWindow_Get(struct BoardInfo *bi, APTR feature, struct TagItem *tags)
{
    struct Library *UtilityBase = bi->UtilBase;
    struct MemoryWindow *mw = feature;
    struct TagItem *tstate = tags;
    struct TagItem *tag;
    ULONG taken = 0;

    while ((tag = NextTagItem(&tstate)) != NULL)
    {
        ULONG *value = (ULONG *)tag->ti_Data;

        taken++;

        /* The window cannot be scaled yet: it has the size of its source */
        switch (tag->ti_Tag)
        {
            case FA_MinWidth:
            case FA_MaxWidth:
                *value = mw->SourceWidth;
                break;

            case FA_MinHeight:
            case FA_MaxHeight:
                *value = mw->SourceHeight;
                break;

            case FA_Format:
                *value = mw->Format;
                break;

            case FA_BitMap:
                *value = (ULONG)mw->BitMap;
                break;

            case FA_Brightness:
                *value = mw->Brightness;
                break;

            case FA_Active:
                *value = (mw->Flags & MWF_ACTIVE) != 0;
                break;

            case FA_Onboard:
                *value = (mw->Flags & MWF_ONBOARD) != 0;
                break;

            default:
                taken--;
                break;
        }
    }

    return taken;
}

BOOL MemoryWindow_Delete(struct BoardInfo *bi, APTR feature)
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)bi->CardBase;
    struct ExecBase *SysBase = bi->ExecBase;
    struct MemoryWindow *mw = feature;

    /* Picasso96 has already set FA_Active to false, which took the window out of the planes */
    if (VideoCoreBase->vc_MemoryWindow == mw)
    {
        VideoCoreBase->vc_MemoryWindow = NULL;
        Chip_RebuildPlanes(bi);
    }

    MemoryWindow_FreeBitMap(bi, mw->BitMap);
    FreeMem(mw, sizeof(struct MemoryWindow));

    return TRUE;
}

BOOL MemoryWindow_Plane(struct VideoCoreBase *VideoCoreBase, struct WindowPlane *window)
{
    struct MemoryWindow *mw = VideoCoreBase->vc_MemoryWindow;
    struct BoardInfo *bi;

    /* Only on a screen of the size of the display: the window is not scaled */
    if (mw == NULL || mw->BitMap == NULL || VideoCoreBase->vc_ScaleX != 0x10000 || VideoCoreBase->vc_ScaleY != 0x10000)
        return FALSE;

    bi = mw->BoardInfo;

    window->Address = bi->GetBitMapAttr(bi, mw->BitMap, GBMA_MEMORY);
    window->BytesPerRow = bi->GetBitMapAttr(bi, mw->BitMap, GBMA_BYTESPERROW);
    window->Format = mw->Format;
    window->X = VideoCoreBase->vc_OffsetX + mw->Left - VideoCoreBase->vc_LastPanning.lp_X;
    window->Y = VideoCoreBase->vc_OffsetY + mw->Top - VideoCoreBase->vc_LastPanning.lp_Y;
    window->Width = mw->SourceWidth;
    window->Height = mw->SourceHeight;
    window->Alpha = 0xfff - (mw->Brightness >> 20);     /* the 12 bits at the top of FA_Brightness, upside down */

    return TRUE;
}
