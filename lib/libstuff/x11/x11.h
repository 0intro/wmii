/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */

#define _X11_VISIBLE
#define pointerwin __pointerwin
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <strings.h>
#include <unistd.h>
#include <bio.h>
#include <stuff/x.h>
#include <stuff/util.h>
#undef  pointerwin

extern int	(*xlib_errorhandler) (Display*, XErrorEvent*);

void	configwin(Window*, Rectangle, int);
XPoint*	convpts(Point*, int);
int	errorhandler(Display*, XErrorEvent*);
void	setgccol(Image*, Color*);
XftColor*	xftcolor(Image*, Color*);
XftDraw*	xftdrawable(Image*);

