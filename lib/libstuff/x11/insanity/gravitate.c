/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

Rectangle
gravitate(Rectangle rc, Rectangle rf, Point grav) {
	Point d;

	/* Get delta between frame and client rectangles */
	d = subpt(subpt(rf.max, rf.min),
		  subpt(rc.max, rc.min));

	/* Divide by 2 and apply gravity */
	d = divpt(d, Pt(2, 2));
	d = mulpt(d, grav);

	return rectsubpt(rc, d);
}
