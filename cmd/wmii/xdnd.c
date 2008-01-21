/* Copyright ©2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

void
xdnd_init(void) {
	long l;

	l = 3; /* They are insane. Why is this an ATOM?! */
	changeprop_long(screen->barwin, "XdndAware", "ATOM", &l, 1);
}

int
xdnd_clientmessage(XClientMessageEvent *e) {
	static Bar *oldbar;
	Rectangle r;
	Point p;
	long *l;
	Bar *b;
	long pos, siz;
	int msg;

	msg = e->message_type;
	l = e->data.l;
	Dprint(DDnd, "ClientMessage: %A\n", msg);

	if(msg == xatom("XdndEnter")) {
		if(e->format != 32)
			return -1;
		oldbar = nil;
		return 1;
	}else
	if(msg == xatom("XdndLeave")) {
		if(e->format != 32)
			return -1;
		oldbar = nil;
		return 1;
	}else
	if(msg == xatom("XdndPosition")) {
		if(e->format != 32)
			return -1;
		p.x = (ulong)l[2] >> 16;
		p.y = (ulong)l[2] & 0xffff;
		p = subpt(p, screen->barwin->r.min);
		Dprint(DDnd, "\tp: %P\n", p);
		/* XXX: This should be done in bar.c. */
		for(b=screen->bar[BarLeft]; b; b=b->next)
			if(rect_haspoint_p(p, b->r)) {
				if(b != oldbar)
					event("LeftBarDND 1 %s\n", b->name);
				break;
			}
		if(b == nil)
		for(b=screen->bar[BarRight]; b; b=b->next)
			if(rect_haspoint_p(p, b->r)) {
				if(b != oldbar)
					event("RightBarDND 1 %s\n", b->name);
				break;
			}
		pos = 0;
		siz = 0;
		oldbar = b;
		if(b) {
			r = rectaddpt(b->r, screen->barwin->r.min);
			Dprint(DDnd, "\tr: %R\n", r);
			pos = (r.min.x<<16) | r.min.y;
			siz = (Dx(r)<<16) | Dy(r);
		}
		sendmessage(window(l[0]), "XdndStatus", e->window, 0, pos, siz, 0);
		return 1;
	}

	return 0;
}

