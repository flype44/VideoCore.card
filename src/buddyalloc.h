#ifndef SRC_BUDDYALLOC_H_
#define SRC_BUDDYALLOC_H_

#include "videocore.h"

void BuddyInit(struct VideoCoreBase *base);
ULONG BuddyAlloc(struct VideoCoreBase *base, UWORD size);
void BuddyFree(struct VideoCoreBase *base, ULONG id);

#define BUDDY_SIZE(x) ((UWORD)((x) >> 16))
#define BUDDY_OFFSET(x)  ((UWORD)((x) & 0xffff))

#endif // SRC_BUDDYALLOC_H_
