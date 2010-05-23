/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

XftColor*
xftcolor(Color col) {
	XftColor *c;

	c = emallocz(sizeof *c);
	*c = (XftColor) {
			  ((col.render.alpha&0xff00) << 24)
			| ((col.render.red&0xff00) << 8)
			| ((col.render.green&0xff00) << 0)
			| ((col.render.blue&0xff00) >> 8),
		col.render
	};
	return freelater(c);
}
