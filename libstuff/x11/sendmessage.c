/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "x11.h"

void
sendmessage(Window *w, char *name, long l0, long l1, long l2, long l3, long l4) {
	XClientMessageEvent e;

	e.type = ClientMessage;
	e.window = w->xid;
	e.message_type = xatom(name);
	e.format = 32;
	e.data.l[0] = l0;
	e.data.l[1] = l1;
	e.data.l[2] = l2;
	e.data.l[3] = l3;
	e.data.l[4] = l4;
	sendevent(w, false, NoEventMask, (XEvent*)&e);
}
