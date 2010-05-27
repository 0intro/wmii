#pragma once
#define _XOPEN_SOURCE 600

#include <sys/types.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef nil
#  define nil	((void*)0)
#endif

#ifndef nelem
#  define nelem(ary) (sizeof(ary) / sizeof(*ary))
#endif

#ifndef offsetof
#  define offsetof(type, member) ((size_t)&((type*)0)->member)
#endif
#define structmember(ptr, type, offset) \
	(*(type*)((char*)(ptr) + (offset)))

#undef uchar
#undef ushort
#undef uint
#undef ulong
#undef uvlong
#undef vlong
#define uchar	_x_uchar
#define ushort	_x_ushort
#define uint	_x_uint
#define ulong	_x_ulong
#define uvlong	_x_uvlong
#define vlong	_x_vlong

typedef unsigned char		uchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;
typedef unsigned long long	uvlong;
typedef long long	vlong;

#define BLOCK(x) do { x; }while(0)

static inline void
_used(long a, ...) {
	if(a){}
}
#ifndef KENC
#  undef USED
#  undef SET
#  define USED(...) _used((long)__VA_ARGS__)
#  define SET(x) (x = 0)
/* # define SET(x) USED(&x) GCC 4 is 'too smart' for this. */
#endif
