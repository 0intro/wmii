#include <fmt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stuff/x.h>
#include <stuff/util.h>

#ifndef EXTERN
# define EXTERN extern
#endif

enum { DAuto, DHorizontal, DVertical };

EXTERN Handlers	handlers;
EXTERN int	direction;

