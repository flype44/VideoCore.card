#ifndef _MEMORY_WINDOW_H
#define _MEMORY_WINDOW_H

#include <exec/types.h>
#include <utility/tagitem.h>

#include "boardinfo.h"
#include "chip.h"

struct VideoCoreBase;

/*
    The special feature SFT_MEMORYWINDOW of Picasso96: a bitmap which a program draws in, shown in a window of
    its screen (picture in picture, p96PIP_OpenTags() with P96PIP_Type = P96PIPT_MemoryWindow). Picasso96 opens the
    Intuition window and tells the rectangle of its interior with FA_Left, FA_Top, FA_Width and FA_Height, as it
    moves. The driver gives the bitmap, shows it as a plane of the display list, between the screen and the mouse
    pointer, and follows the panning of the screen.

    Transparency: Picasso96 has no attribute for it, so FA_Brightness (P96PIP_Brightness for a program) is taken as
    the transparency of the window over what is under it. It is 0 by default, and Picasso96 sends it with every
    change of attributes, so 0 must stay opaque: 0 is opaque, 0x80000000 about half transparent, 0xffffffff
    invisible. The twelve upper bits are used (the alpha of the plane has twelve bits).

    For now: one window shown at a time, only on a screen which has the size of the display (not scaled), the
    window not scaled, sources in the direct colour formats (no palette).
*/

/* The four hooks of the special features, see special-features.c. They return what BoardInfo wants: the feature
   data (NULL if it cannot be made), the number of attributes taken, and TRUE when deleted. */
APTR  MemoryWindow_Create(struct BoardInfo *bi, struct TagItem *tags);
ULONG MemoryWindow_Set(struct BoardInfo *bi, APTR feature, struct TagItem *tags);
ULONG MemoryWindow_Get(struct BoardInfo *bi, APTR feature, struct TagItem *tags);
BOOL  MemoryWindow_Delete(struct BoardInfo *bi, APTR feature);

/*
    The window which is shown, as a plane of the display: TRUE and *window filled in when there is one and the
    screen as it is now can show it, else FALSE. Called by the code which writes the planes.
*/
BOOL  MemoryWindow_Plane(struct VideoCoreBase *VideoCoreBase, struct WindowPlane *window);

#endif /* _MEMORY_WINDOW_H */
