/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
setgccol(Image *dst, Color c) {
	XSetForeground(display, dst->gc, pixelvalue(c));
}
