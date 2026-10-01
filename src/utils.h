#ifndef _UTILS_H
#define _UTILS_H

#include <exec/tasks.h>
#include <exec/libraries.h>
#include <utility/tagitem.h>

enum {
    TASKTAG_NAME = TAG_USER,
    TASKTAG_AFFINITY,
    TASKTAG_PRI,
    TASKTAG_PC,
    TASKTAG_STACKSIZE,
    TASKTAG_USERDATA,
    TASKTAG_ARG1,
    TASKTAG_ARG2,
    TASKTAG_ARG3,
    TASKTAG_ARG4,
};

/* Small C library functions, the driver is built without libc */
int _strlen(CONST_STRPTR str);
int _strcmp(const char *s1, const char *s2);
ULONG _atoul(CONST_STRPTR str);

/* The utility.library to use is the one of the BoardInfo, rtg.library gives it to the card */
struct Task * NewCreateTaskTags(struct TagItem *tags, struct Library *UtilityBase);

#define NewCreateTask(UtilityBase, ...)          \
    ({ struct TagItem tags[] = { __VA_ARGS__ }; struct Task *t = NewCreateTaskTags(tags, UtilityBase); t; })

#endif /* _UTILS_H */
