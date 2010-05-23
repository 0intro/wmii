/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

int
grabpointer(Window *w, Window *confine, Cursor cur, int mask) {
	XWindow cw;
	
	cw = None;
	if(confine)
		cw = confine->xid;
	return XGrabPointer(display, w->xid, false /* owner events */, mask,
		GrabModeAsync, GrabModeAsync, cw, cur, CurrentTime
		) == GrabSuccess;
}
