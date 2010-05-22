#include <stdbool.h>

#define Debug(x) if(((debugflag|debugfile)&(x)) && setdebug(x))
#define Dprint(x, ...) BLOCK( if((debugflag|debugfile)&(x)) debug(x, __VA_ARGS__) )

void	debug(int, const char*, ...);
void	dprint(const char*, ...);
void	dwrite(int, void*, int, bool);
bool	setdebug(int);
void	vdebug(int, const char*, va_list);

int	debugflag;
int	debugfile;
