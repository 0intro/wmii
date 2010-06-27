#include <stdbool.h>

enum DebugOpt {
	D9p	= 1<<0,
	DDnd	= 1<<1,
	DEvent	= 1<<2,
	DEwmh	= 1<<3,
	DFocus	= 1<<4,
	DGeneric= 1<<5,
	DStack  = 1<<6,
	NDebugOpt = 7,
};

#define Debug(x) if(((debugflag|debugfile)&(x)) && setdebug(x))
#define Dprint(x, ...) BLOCK( if((debugflag|debugfile)&(x)) debug(x, __VA_ARGS__) )

void	debug(int, const char*, ...);
void	dwrite(int, void*, int, bool);
bool	setdebug(int);
void	vdebug(int, const char*, va_list);

long	debugflag;
long	debugfile;
