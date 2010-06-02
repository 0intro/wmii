/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

static Handlers	handlers;

void
xdnd_initwindow(Window *w) {
	long l;

	l = 3; /* They are insane. Why is this an ATOM?! */
	changeprop_long(w, "XdndAware", "ATOM", &l, 1);
	pushhandler(w, &handlers, nil);
}

typedef struct Dnd Dnd;
struct Dnd {
	XWindow		source;
	Rectangle	r;
};

static bool
clientmessage_event(Window *w, void *aux, XClientMessageEvent *e) {
	Dnd *dnd;
	long *l;
	Rectangle r;
	Point p;
	long pos, siz;
	ulong msg;

	dnd = nil;
	msg = e->message_type;
	l = e->data.l;
	Dprint(DDnd, "ClientMessage: %A\n", msg);

	if(msg == xatom("XdndEnter")) {
		if(e->format != 32)
			return false;
		if(w->dnd == nil)
			w->dnd = emallocz(sizeof *dnd);
		dnd = w->dnd;
		dnd->source = l[0];
		dnd->r = ZR;
		return false;
	}else
	if(msg == xatom("XdndLeave")) {
		if(e->format != 32)
			return false;
		if(w->dnd) {
			free(w->dnd);
			w->dnd = nil;
		}
		return false;
	}else
	if(msg == xatom("XdndPosition")) {
		if(e->format != 32)
			return false;
		r = ZR;
		dnd = w->dnd;
		if(dnd) {
			p.x = (ulong)l[2] >> 16;
			p.y = (ulong)l[2] & 0xffff;
			p = subpt(p, w->r.min);
			Dprint(DDnd, "\tw: %W\n", w);
			Dprint(DDnd, "\tp: %P\n", p);
			if(eqrect(dnd->r, ZR) || !rect_haspoint_p(dnd->r, p))
				if(w->handler->dndmotion)
					dnd->r = w->handler->dndmotion(w, w->aux, p);
			r = dnd->r;
			if(!eqrect(r, ZR))
				r = rectaddpt(r, w->r.min);
			Dprint(DDnd, "\tr: %R\n", r);
		}
		pos = (r.min.x<<16) | r.min.y;
		siz = (Dx(r)<<16) | Dy(r);
		sendmessage(window(l[0]), "XdndStatus", e->window, 0, pos, siz, 0);
		return false;
	}

	return true;
}

static Handlers handlers = {
	.message = clientmessage_event
};

