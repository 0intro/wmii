/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
warppointer(Point pt) {
	/* Nasty kludge for xephyr, xnest. */
	static int havereal = -1;
	static char* real;

	if(havereal == -1) {
		real = getenv("REALDISPLAY");
		havereal = real != nil;
	}
	if(havereal)
		system(sxprint("DISPLAY=%s wiwarp %d %d", real, pt.x, pt.y));

	XWarpPointer(display,
		/* src, dest w */ None, scr.root.xid,
		/* src_rect */	0, 0, 0, 0,
		/* target */	pt.x, pt.y);
}
