#include "utils.h"
#include <stdarg.h>
#include <stdint.h>

#include <proto/exec.h>
#include <proto/utility.h>
#include <clib/alib_protos.h>

int _strlen(CONST_STRPTR str)
{
    int len = 0;

    while (*str++) len++;

    return len;
}

int _strcmp(const char *s1, const char *s2)
{
    while (*s1 == *s2++)
        if (*s1++ == '\0')
            return (0);
    return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

/* The ASCII letters in lower case, the names and values of the ToolTypes are plain ASCII */
static int _tolower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

/* Like _strcmp(), without caring about the case */
int _stricmp(const char *s1, const char *s2)
{
    while (_tolower((unsigned char)*s1) == _tolower((unsigned char)*s2++))
        if (*s1++ == '\0')
            return 0;

    return _tolower((unsigned char)*s1) - _tolower((unsigned char)*(s2 - 1));
}

/* If the ToolType is "name" or "name=value" returns the value, which is empty when there is none, otherwise
   NULL. The name is compared without caring about the case and blanks are allowed around the '=', that is
   how FindToolType() of icon.library works. */
CONST_STRPTR MatchToolType(CONST_STRPTR tooltype, CONST_STRPTR name)
{
    while (*name)
        if (_tolower((unsigned char)*tooltype++) != _tolower((unsigned char)*name++))
            return NULL;

    while (*tooltype == ' ' || *tooltype == '\t')
        tooltype++;

    if (*tooltype == '=')
    {
        tooltype++;

        while (*tooltype == ' ' || *tooltype == '\t')
            tooltype++;

        return tooltype;
    }

    if (*tooltype == '\0')
        return tooltype;

    return NULL;
}

/* The value of a ToolType which switches something on: YES, TRUE or 1, whatever the case */
BOOL YesOrTrue(CONST_STRPTR value)
{
    return _stricmp(value, "YES") == 0 || _stricmp(value, "TRUE") == 0 || _stricmp(value, "1") == 0;
}

/* The decimal number at the start of the string, the digits end at the first other character */
ULONG _atoul(CONST_STRPTR str)
{
    ULONG num = 0;

    while (*str >= '0' && *str <= '9')
        num = num * 10 + (*str++ - '0');

    return num;
}

struct Task * NewCreateTaskTags(struct TagItem *tags, struct Library *UtilityBase)
{
    struct ExecBase *SysBase = *(struct ExecBase **)4;
    struct Task *task = NULL;

    APTR entry = (APTR)GetTagData(TASKTAG_PC, 0, tags);
    APTR task_name = (APTR)GetTagData(TASKTAG_NAME, (ULONG)"task", tags);
    APTR userdata = (APTR)GetTagData(TASKTAG_USERDATA, 0, tags);
    ULONG task_name_len = _strlen(task_name) + 1;
    UBYTE priority = GetTagData(TASKTAG_PRI, 0, tags);
    ULONG stacksize = GetTagData(TASKTAG_STACKSIZE, 8192, tags);
    ULONG args[] = {
        GetTagData(TASKTAG_ARG1, 0, tags),
        GetTagData(TASKTAG_ARG2, 0, tags),
        GetTagData(TASKTAG_ARG3, 0, tags),
        GetTagData(TASKTAG_ARG4, 0, tags)
    };
    ULONG argcnt = 0;

    if (FindTagItem(TASKTAG_ARG4, tags)) {
        argcnt = 4;
    }
    else if (FindTagItem(TASKTAG_ARG3, tags)) {
        argcnt = 3;
    }
    else if (FindTagItem(TASKTAG_ARG2, tags)) {
        argcnt = 2;
    }
    else if (FindTagItem(TASKTAG_ARG1, tags)) {
        argcnt = 1;
    }

    if (entry != NULL)
    {
        task = AllocMem(sizeof(struct Task), MEMF_PUBLIC | MEMF_CLEAR);
        struct MemList *ml = AllocMem(sizeof(struct MemList) + 2*sizeof(struct MemEntry), MEMF_PUBLIC | MEMF_CLEAR);
        ULONG *stack = AllocMem(stacksize, MEMF_PUBLIC | MEMF_CLEAR);
        ULONG *sp = (ULONG *)((ULONG)stack + stacksize);
        STRPTR name_copy = AllocMem(task_name_len, MEMF_PUBLIC | MEMF_CLEAR);
        
        CopyMem((APTR)task_name, name_copy, task_name_len);

        ml->ml_NumEntries = 3;
        ml->ml_ME[0].me_Un.meu_Addr = task;
        ml->ml_ME[0].me_Length = sizeof(struct Task);

        ml->ml_ME[1].me_Un.meu_Addr = stack;
        ml->ml_ME[1].me_Length = stacksize;

        ml->ml_ME[2].me_Un.meu_Addr = name_copy;
        ml->ml_ME[2].me_Length = task_name_len;

        sp -= argcnt;

        for (int i=0; i < argcnt; i++) {
            sp[i] = args[i];
        }

        task->tc_UserData = userdata;
        task->tc_SPLower = stack;
        task->tc_SPUpper = (APTR)((ULONG)stack + stacksize);
        task->tc_SPReg = sp;

        task->tc_Node.ln_Name = name_copy;
        task->tc_Node.ln_Type = NT_TASK;
        task->tc_Node.ln_Pri = priority;

        NewList(&task->tc_MemEntry);
        AddHead(&task->tc_MemEntry, &ml->ml_Node);

        AddTask(task, entry, NULL);
    }
    
    return task;
}
