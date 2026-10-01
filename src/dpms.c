/*
    DPMS support of Picasso96: the display power of the firmware, for any VideoCore family.
*/

#include <exec/types.h>

#include <common/compiler.h>

#include "videocore.h"
#include "boardinfo.h"
#include "mbox.h"
#include "dpms.h"

/* Display power on or off */
static void SetDPMSLevel(REGARG(struct BoardInfo *b, "a0"), REGARG(ULONG level, "d0"))
{
    struct VideoCoreBase *VideoCoreBase = (struct VideoCoreBase *)b->CardBase;
    if (0)
    {
        bug("[VC] SetDPMSLevel(%ld)\n", level);
    }

    /* display power on or off */
    BOOL ret = SetDisplayPower(VideoCoreBase->vc_DisplayID, 
        (level == DPMS_OFF) ? 0 : 1, VideoCoreBase);

    /* display power debug */
    if (0)
    {
        bug("[VC] SetDisplayPower(display_id: %ld, state: %ld): %ld\n", 
            VideoCoreBase->vc_DisplayID, (level == DPMS_OFF) ? 0 : 1, ret);
    }
}

/* Gets the display id of the firmware and, if it knows it, gives Picasso96 the DPMS method. Only when the
   ToolType VC4_DPMS asked for it. */
void DPMS_Init(struct BoardInfo *bi, struct VideoCoreBase *VideoCoreBase)
{
    if (VideoCoreBase->vc_UseDPMS)
    {
        /* obtain the RPi primary display id, or -1 if not supported by the RPi firmware.
         * 
         * +---------+----------------+------------+
         * | display | display_number | display_id |
         * +---------+----------------+------------+
         * | hdmi-0  |            0UL |         2L | primary hdmi
         * | hdmi-1  |            1UL |         7L | secondary hdmi
         * +---------+----------------+------------+
         */
        
        /* obtain the primary hdmi display num */
        VideoCoreBase->vc_DisplayNum = 0UL;
        
        /* obtain the primary hdmi display id */
        VideoCoreBase->vc_DisplayID = GetDisplayID(
            VideoCoreBase->vc_DisplayNum, VideoCoreBase);
        
        /* attach the Picasso96 method if the display id is valid */
        if (VideoCoreBase->vc_DisplayID >= 0) {
            bi->SetDPMSLevel = (void *)SetDPMSLevel;

            /* the firmware keeps the display power state from one boot to the next:
             * a display which DPMS has switched off before a reboot would stay off,
             * so switch it on */
            SetDisplayPower(VideoCoreBase->vc_DisplayID, 1, VideoCoreBase);
        }
    }
}
