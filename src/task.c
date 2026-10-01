/*
    The task of the driver: it owns the message port "VideoCore" and answers the messages of the
    clients (see messages.h), the ones which concern the HVS are handled in hvs.c.
*/

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/tasks.h>
#include <exec/ports.h>

#include <proto/exec.h>

#include "videocore.h"
#include "hvs.h"
#include "utils.h"
#include "messages.h"
#include "task.h"

static void vc_Task()
{
    struct ExecBase *SysBase = *(struct ExecBase **)4;
    struct Task *me = FindTask(NULL);
    struct VideoCoreBase *VideoCoreBase = me->tc_UserData;
    struct MsgPort *port = CreateMsgPort();
    ULONG sigset;

    port->mp_Node.ln_Name = "VideoCore";
    AddPort(port);

    VideoCoreBase->vc_Port = port;
    
    do {
        sigset = Wait(SIGBREAKF_CTRL_C | (1 << port->mp_SigBit));
        if (sigset & (1 << port->mp_SigBit))
        {
            struct Message *msg;
            
            while((msg = GetMsg(port)))
            {
                if (msg->mn_Length == sizeof(struct VC4Msg)) {
                    struct VC4Msg *vmsg = (struct VC4Msg *)msg;
                    switch (vmsg->cmd) {
                        case VCMD_SET_KERNEL:
                            HVS_SetKernel(VideoCoreBase, vmsg->SetKernel.kernel, vmsg->SetKernel.b, vmsg->SetKernel.c);
                            break;

                        case VCMD_GET_KERNEL:
                            vmsg->GetKernel.kernel = VideoCoreBase->vc_UseKernel;
                            vmsg->GetKernel.b = VideoCoreBase->vc_Kernel_B;
                            vmsg->GetKernel.c = VideoCoreBase->vc_Kernel_C;
                            break;

                        case VCMD_GET_SCALER:
                            vmsg->GetScaler.val = HVS_GetScaler(VideoCoreBase);
                            break;

                        case VCMD_SET_SCALER:
                            HVS_SetScaler(VideoCoreBase, vmsg->SetScaler.val);
                            break;

                        case VCMD_GET_PHASE:
                            vmsg->GetPhase.val = HVS_GetPhase(VideoCoreBase);
                            break;

                        case VCMD_SET_PHASE:
                            HVS_SetPhase(VideoCoreBase, vmsg->SetPhase.val);
                            break;

                        case VCMD_UPDATE_UNICAM_DL:
                            HVS_UpdateUnicamDL(VideoCoreBase);
                            break;
                    }
                }
                ReplyMsg(msg);
            }
        }
    } while ((sigset & SIGBREAKF_CTRL_C) == 0);

    RemPort(port);
    DeleteMsgPort(port);
}

void Task_Start(struct VideoCoreBase *VideoCoreBase)
{
    VideoCoreBase->vc_Task = NewCreateTask(
        TASKTAG_PC,         (Tag)vc_Task,
        TASKTAG_NAME,       (Tag)"VideoCore Task",
        TASKTAG_STACKSIZE,  10240,
        TASKTAG_USERDATA,   (Tag)VideoCoreBase,
        TAG_DONE
    );
}
