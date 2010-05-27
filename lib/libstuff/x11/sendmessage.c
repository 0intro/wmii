/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "x11.h"
#include <string.h>

void
sendmessage(Window *w, char *name, long l0, long l1, long l2, long l3, long l4) {

	clientmessage(w, name, NoEventMask, 32, (ClientMessageData){ .l = { l0, l1, l2, l3, l4 } });
}

void
clientmessage(Window *w, char *name, long mask, int format, ClientMessageData data) {
	XClientMessageEvent e;

	e.type = ClientMessage;
	e.window = w->xid;
	e.message_type = xatom(name);
	e.format = format;
	bcopy(&data, &e.data, sizeof(data));
	sendevent(w, false, mask, (XEvent*)&e);
}

