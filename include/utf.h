#ifndef _UTF_H_
#define _UTF_H_ 1

typedef unsigned int Rune;	/* 32 bits */

enum
{
	UTFmax		= 4,		/* maximum bytes per rune */
	Runesync	= 0x80,		/* cannot represent part of a UTF sequence (<) */
	Runeself	= 0x80,		/* rune and UTF sequences are the same (<) */
	Runeerror	= 0xFFFD,	/* decoding error in UTF */
	Runemax		= 0x10FFFF	/* maximum rune value */
};

/* Edit .+1,/^$/ | cfn $PLAN9/src/lib9/utf/?*.c | grep -v static |grep -v __ */
int		chartorune(Rune *rune, const char *str);
int		fullrune(const char *str, int n);
int		isalpharune(Rune);
int		islowerrune(Rune);
int		isspacerune(Rune);
int		istitlerune(Rune);
int		isupperrune(Rune);
int		runelen(long);
int		runenlen(const Rune*, int);
Rune*		runestrcat(Rune*, const Rune*);
Rune*		runestrchr(const Rune*, Rune);
int		runestrcmp(const Rune*, const Rune*);
Rune*		runestrcpy(Rune*, const Rune*);
Rune*		runestrdup(const Rune*) ;
Rune*		runestrecpy(Rune*, Rune *e, const Rune*);
long		runestrlen(const Rune*);
Rune*		runestrncat(Rune*, const Rune*, long);
int		runestrncmp(const Rune*, const Rune*, long);
Rune*		runestrncpy(Rune*, const Rune*, long);
Rune*		runestrrchr(const Rune*, Rune);
Rune*		runestrstr(const Rune*, const Rune*);
int		runetochar(char*, const Rune*);
Rune		tolowerrune(Rune);
Rune		totitlerune(Rune);
Rune		toupperrune(Rune);
char*		utfecpy(char*, char*, const char*);
int		utflen(const char*);
int		utfnlen(const char*, long);
char*		utfrrune(const char*, long);
char*		utfrune(const char*, long);
char*		utfutf(const char*, const char*);

#endif
