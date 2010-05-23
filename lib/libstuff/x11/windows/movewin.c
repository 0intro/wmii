/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
movewin(Window *w, Point pt) {
	Rectangle r;

	assert(w->type == WWindow);
	r = rectsetorigin(w->r, pt);
	reshapewin(w, r);
}
