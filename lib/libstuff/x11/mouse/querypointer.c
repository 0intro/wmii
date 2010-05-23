/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

Point
querypointer(Window *w) {
	XWindow win;
	Point pt;
	uint ui;
	int i;
	
	XQueryPointer(display, w->xid, &win, &win, &i, &i, &pt.x, &pt.y, &ui);
	return pt;
}
