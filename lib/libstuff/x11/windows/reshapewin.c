/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
reshapewin(Window *w, Rectangle r) {
	assert(w->type == WWindow);
	assert(Dx(r) > 0 && Dy(r) > 0); /* Rather than an X error. */

	configwin(w, r, w->border);
}
