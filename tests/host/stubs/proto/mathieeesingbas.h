#ifndef STUB_PROTO_MATHIEEESINGBAS_H
#define STUB_PROTO_MATHIEEESINGBAS_H
#include <exec/types.h>
/* The library base the real macros need is not used here */
ULONG host_ieeespflt(LONG v);
ULONG host_ieeespdiv(ULONG a, ULONG b);
#define IEEESPFlt(v)    host_ieeespflt(v)
#define IEEESPDiv(a, b) host_ieeespdiv(a, b)
#endif
