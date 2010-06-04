/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

XftColor*
xftcolor(Color col) {
	XftColor *c;

	c = emallocz(sizeof *c);
	*c = (XftColor){ pixelvalue(col), col };
	return freelater(c);
}
