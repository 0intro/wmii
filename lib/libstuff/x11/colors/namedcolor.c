/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

bool
namedcolor(char *name, Color *ret) {
	XColor c, c2;

	if(XAllocNamedColor(display, scr.colormap, name, &c, &c2)) {
		*ret = (Color) {
			c.pixel, {
				c.red,
				c.green,
				c.blue,
				0xffff
			},
		};
		return true;
	}
	return false;
}
