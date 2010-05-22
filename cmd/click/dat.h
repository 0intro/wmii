#include <fmt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stuff/x.h>
#include <stuff/util.h>
#include <ixp.h>

#define BLOCK(x) do { x; }while(0)

#ifndef EXTERN
# define EXTERN extern
#endif

EXTERN Window	win;

