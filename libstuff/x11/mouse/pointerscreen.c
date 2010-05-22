/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

int
pointerscreen(void) {
	XWindow win;
	Point pt;
	uint ui;
	int i;
	
	return XQueryPointer(display, scr.root.xid, &win, &win, &i, &i,
			     &pt.x, &pt.y, &ui);
}
