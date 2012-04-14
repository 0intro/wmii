#include <stdbool.h>

enum DebugOpt {
	D9p	= 1<<0,
	DDnd	= 1<<1,
	DEvent	= 1<<2,
	DEvents	= 1<<3,
	DEwmh	= 1<<4,
	DFocus	= 1<<5,
	DGeneric= 1<<6,
	DStack  = 1<<7,
	NDebugOpt = 8,
};

#define Debug(x) if(((debugflag|debugfile)&(x)) && setdebug(x))
#define Dprint(x, ...) BLOCK( if((debugflag|debugfile)&(x)) debug(x, __VA_ARGS__) )

void	debug(int, const char*, ...);
void	dwrite(int, void*, int, bool);
bool	setdebug(int);
void	vdebug(int, const char*, va_list);

long	debugflag;
long	debugfile;
