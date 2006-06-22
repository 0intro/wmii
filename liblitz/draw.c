/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include "blitz.h"

void
blitz_drawbg(Display *dpy, Drawable drawable, GC gc, XRectangle rect, BlitzColor c)
{
	XPoint points[5];
	XSetForeground(dpy, gc, c.bg);
	XFillRectangles(dpy, drawable, gc, &rect, 1);
	XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
	XSetForeground(dpy, gc, c.border);
	points[0].x = rect.x;
	points[0].y = rect.y;
	points[1].x = rect.width - 1;
	points[1].y = 0;
	points[2].x = 0;
	points[2].y = rect.height - 1;
	points[3].x = -(rect.width - 1);
	points[3].y = 0;
	points[4].x = 0;
	points[4].y = -(rect.height - 1);
	XDrawLines(dpy, drawable, gc, points, 5, CoordModePrevious);
}
