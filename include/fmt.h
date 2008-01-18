#ifndef _FMT_H_
#define _FMT_H_ 1
/*
 * The authors of this software are Rob Pike and Ken Thompson.
 *              Copyright (c) 2002 by Lucent Technologies.
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES MAKE ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */

#include <stdarg.h>
#include <utf.h>

typedef struct Fmt	Fmt;
struct Fmt{
	unsigned char	runes;		/* output buffer is runes or chars? */
	void	*start;			/* of buffer */
	void	*to;			/* current place in the buffer */
	void	*stop;			/* end of the buffer; overwritten if flush fails */
	int	(*flush)(Fmt *);	/* called when to == stop */
	void	*farg;			/* to make flush a closure */
	int	nfmt;			/* num chars formatted so far */
	va_list	args;			/* args passed to dofmt */
	int	r;			/* % format Rune */
	int	width;
	int	prec;
	unsigned long	flags;
};

enum{
	FmtWidth	= 1,
	FmtLeft		= FmtWidth << 1,
	FmtPrec		= FmtLeft << 1,
	FmtSharp	= FmtPrec << 1,
	FmtSpace	= FmtSharp << 1,
	FmtSign		= FmtSpace << 1,
	FmtZero		= FmtSign << 1,
	FmtUnsigned	= FmtZero << 1,
	FmtShort	= FmtUnsigned << 1,
	FmtLong		= FmtShort << 1,
	FmtVLong	= FmtLong << 1,
	FmtComma	= FmtVLong << 1,
	FmtByte		= FmtComma << 1,
	FmtLDouble	= FmtByte << 1,

	FmtFlag		= FmtLDouble << 1
};

extern	int	(*fmtdoquote)(int);

#ifdef VARARGCK
# pragma varargck	argpos	fmtprint	2
# pragma varargck	argpos	fprint		2
# pragma varargck	argpos	print		1
# pragma varargck	argpos	runeseprint	3
# pragma varargck	argpos	runesmprint	1
# pragma varargck	argpos	runesnprint	3
# pragma varargck	argpos	runesprint	2
# pragma varargck	argpos	seprint		3
# pragma varargck	argpos	smprint		1
# pragma varargck	argpos	snprint		3
# pragma varargck	argpos	sprint		2

# pragma varargck	type	"lld"	vlong
# pragma varargck	type	"llx"	vlong
# pragma varargck	type	"lld"	uvlong
# pragma varargck	type	"llx"	uvlong
# pragma varargck	type	"ld"	long
# pragma varargck	type	"lx"	long
# pragma varargck	type	"lb"	long
# pragma varargck	type	"ld"	ulong
# pragma varargck	type	"lx"	ulong
# pragma varargck	type	"lb"	ulong
# pragma varargck	type	"d"	int
# pragma varargck	type	"x"	int
# pragma varargck	type	"c"	int
# pragma varargck	type	"C"	int
# pragma varargck	type	"b"	int
# pragma varargck	type	"d"	uint
# pragma varargck	type	"x"	uint
# pragma varargck	type	"c"	uint
# pragma varargck	type	"C"	uint
# pragma varargck	type	"b"	uint
# pragma varargck	type	"f"	double
# pragma varargck	type	"e"	double
# pragma varargck	type	"g"	double
# pragma varargck	type	"s"	char*
# pragma varargck	type	"q"	char*
# pragma varargck	type	"S"	Rune*
# pragma varargck	type	"Q"	Rune*
# pragma varargck	type	"r"	void
# pragma varargck	type	"%"	void
# pragma varargck	type	"n"	int*
# pragma varargck	type	"p"	uintptr_t
# pragma varargck	type	"p"	void*
# pragma varargck	flag	','
# pragma varargck	flag	'h'
# pragma varargck	type	"<"	void*
# pragma varargck	type	"["	void*
# pragma varargck	type	"H"	void*
# pragma varargck	type	"lH"	void*
#endif

/* Edit .+1,/^$/ | cfn $PLAN9/src/lib9/fmt/?*.c | grep -v static |grep -v __ */
int		dofmt(Fmt*, const char *fmt);
int		dorfmt(Fmt*, const Rune *fmt);
double		fmtcharstod(int(*f)(void*), void*);
int		fmtfdflush(Fmt*);
int		fmtfdinit(Fmt*, int fd, char *buf, int size);
int		fmtinstall(int, int (*f)(Fmt*));
int		fmtprint(Fmt*, const char*, ...);
int		fmtrune(Fmt*, int);
int		fmtrunestrcpy(Fmt*, Rune*);
int		fmtstrcpy(Fmt*, const char*);
char*		fmtstrflush(Fmt*);
int		fmtstrinit(Fmt*);
double		fmtstrtod(const char*, char**);
int		fmtvprint(Fmt*, const char*, va_list);
int		fprint(int, const char*, ...);
int		print(const char*, ...);
void		quotefmtinstall(void);
int		quoterunestrfmt(Fmt*);
int		quotestrfmt(Fmt*);
Rune*		runefmtstrflush(Fmt*);
int		runefmtstrinit(Fmt*);
Rune*		runeseprint(Rune*,Rune*, const char*, ...);
Rune*		runesmprint(const char*, ...);
int		runesnprint(Rune*, int, const char*, ...);
int		runesprint(Rune*, const char*, ...);
Rune*		runevseprint(Rune*, Rune *, const char*, va_list);
Rune*		runevsmprint(const char*, va_list);
int		runevsnprint(Rune*, int, const char*, va_list);
char*		seprint(char*, char*, const char*, ...);
char*		smprint(const char*, ...);
int		snprint(char*, int, const char *, ...);
int		sprint(char*, const char*, ...);
int		vfprint(int, const char*, va_list);
char*		vseprint(char*, char*, const char*, va_list);
char*		vsmprint(const char*, va_list);
int		vsnprint(char*, int, const char*, va_list);

#endif
