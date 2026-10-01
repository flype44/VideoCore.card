/* Included before every source of the host build: the AmigaOS register annotations mean nothing on the host.
   __STORMGCC__ makes boardinfo.h skip its own definition of the __REGxx macros, they are plain parameters here. */
#ifndef HOST_PRELUDE_H
#define HOST_PRELUDE_H
#define __STORMGCC__ 1
#define __INTELLISENSE__ 1
#define __stdargs
#define __saveds
#define __REGD0(x) x
#define __REGA0(x) x
#define __REGD1(x) x
#define __REGA1(x) x
#define __REGD2(x) x
#define __REGA2(x) x
#define __REGD3(x) x
#define __REGA3(x) x
#define __REGD4(x) x
#define __REGA4(x) x
#define __REGD5(x) x
#define __REGA5(x) x
#define __REGD6(x) x
#define __REGA6(x) x
#define __REGD7(x) x
#define __REGA7(x) x
#endif
