/*
 * compiler directive on Plan 9
 */

/*
 * easiest way to make sure these are defined
 */
#ifndef KENC
# ifndef USED
#  define USED(x) if(x);else
# endif
#endif

#define uchar	_p9uchar
#define ushort	_p9ushort
#define uint	_p9uint
#define ulong	_p9ulong
typedef unsigned char		uchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;
typedef long long		vlong;
typedef unsigned long long	uvlong;

#include <utf.h>
#include <stdint.h>
#include <fmt.h>
#include <string.h>
#include <unistd.h>

#define OREAD		O_RDONLY
#define OWRITE	O_WRONLY

#define	OCEXEC 0
#define	ORCLOSE	0
#define	OTRUNC	0

#define	exits(x)	exit(x && *x ? 1 : 0)

#undef	nil
#define	nil	((void*)0)

#undef	nelem
#define	nelem(x)	(sizeof (x)/sizeof (x)[0])

