
typedef unsigned long	ulong;
typedef unsigned int	uint;
typedef unsigned char	uchar;

#define _XOPEN_SOURCE 600
#define IXP_P9_STRUCTS
#define IXP_NO_P9_
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stuff/util.h>
#include <X11/Xlib.h>

#define strdup my_strdup
#define smprint my_smprint
#define vsmprint my_vsmprint

static char*	smprint(const char*, ...);
static char*	vsmprint(const char*, va_list);
static char*	strdup(const char*);

#define nil ((void*)0)
#define nelem(ary) (sizeof(ary) / sizeof(*ary))

