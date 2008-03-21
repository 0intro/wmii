#include <fmt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <util.h>
#include <ixp.h>
#include <x11.h>

#define BLOCK(x) do { x; }while(0)

#ifndef EXTERN
# define EXTERN extern
#endif

extern Handlers	handlers;

EXTERN bool	running;

EXTERN Window	win;
EXTERN Window	frame;
EXTERN long	xtime;

EXTERN char	buffer[8092];
EXTERN char*	_buffer;

static char*	const _buf_end = buffer + sizeof buffer;

#define bufclear() \
	BLOCK( _buffer = buffer; _buffer[0] = '\0' )
#define bufprint(...) \
	_buffer = seprint(_buffer, _buf_end, __VA_ARGS__)

