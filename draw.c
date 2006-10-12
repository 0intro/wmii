/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "wm.h"

void
drawbg(Display *dpy, Drawable drawable, GC gc, XRectangle rect,
			BlitzColor c, Bool border)
{
	XPoint points[5];
	XSetForeground(dpy, gc, c.bg);
	XFillRectangles(dpy, drawable, gc, &rect, 1);
	if(!border)
		return;
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

void
drawcursor(Display *dpy, Drawable drawable, GC gc,
				int x, int y, unsigned int h, BlitzColor c)
{
	XSegment s[5];

	XSetForeground(dpy, gc, c.fg);
	XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
	s[0].x1 = x - 1;
	s[0].y1 = s[0].y2 = y;
	s[0].x2 = x + 2;
	s[1].x1 = x - 1;
	s[1].y1 = s[1].y2 = y + 1;
	s[1].x2 = x + 2;
	s[2].x1 = s[2].x2 = x;
	s[2].y1 = y;
	s[2].y2 = y + h;
	s[3].x1 = x - 1;
	s[3].y1 = s[3].y2 = y + h;
	s[3].x2 = x + 2;
	s[4].x1 = x - 1;
	s[4].y1 = s[4].y2 = y + h - 1;
	s[4].x2 = x + 2;
	XDrawSegments(dpy, drawable, gc, s, 5);
}
