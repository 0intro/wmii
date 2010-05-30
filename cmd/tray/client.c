/* Copyright ©2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"
#include <string.h>

static Handlers	handlers;

static void client_cleanup(XEmbed*);

void
client_manage(XWindow w) {
	Client **cp;
	Client *c;
	WinAttr wa;
	int size;

	c = emallocz(sizeof *c);
	c->w.type = WWindow;
	c->w.xid = w;
	c->w.aux = c;

	Dprint("client_manage(%W)\n", &c->w);

	traperrors(true);
	XAddToSaveSet(display, w);
	c->xembed = xembed_swallow(tray.win, &c->w, client_cleanup);
	if(traperrors(false)) {
		fprint(1, "client_manage(0x%ulx): Caught error.\n", w);
		if(c->xembed)
			xembed_disown(c->xembed);
		return;
	}

	wa.background_pixel = tray.selcolors.bg.pixel;
	size = max(tray.iconsize / 4, 4);

	c->indicator = createwindow(tray.win, Rect(0, 0, size, size), scr.depth,
				    InputOutput, &wa, CWBackPixel);
	setborder(c->indicator, 1, tray.selcolors.border);

	sethandler(&c->w, &handlers);

	for(cp=&tray.clients; *cp; cp=&(*cp)->next)
		;
	*cp = c;

	tray_update();
}

void
client_disown(Client *c) {

	Dprint("client_disown(%W)\n", &c->w);
	xembed_disown(c->xembed);
}

static void
client_cleanup(XEmbed *e) {
	Client **cp;
	Client *c;

	c = e->w->aux;
	destroywindow(c->indicator);

	for(cp=&tray.clients; *cp; cp=&(*cp)->next)
		if(*cp == c) {
			*cp = c->next;
			break;
		}
	cleanupwindow(&c->w);
	free(c);
	tray_update();
}

Client*
client_find(Window *w) {
	Client *c;

	for(c=tray.clients; c; c=c->next)
		if(&c->w == w)
			return c;
	return nil;
}

void
message_cancel(Client *c, long id) {
	Message *m, **mp;

	for(mp=&c->message; (m = *mp) && m->id != id; mp=&m->next)
		;

	if(m) {
		*mp = m->next;
		free(m->msg.data);
		free(m);
	}
}

bool
client_hasmessage(Client *c) {
	Message *m;

	for(m=c->message; m; m=m->next)
		if(m->msg.pos == m->msg.end)
			return true;
	return false;
}

void
client_opcode(Client *c, long message, long l1, long l2, long l3) {
	Message *m, **mp;

	Dprint("client_opcode(%p, %s, %lx, %lx, %lx)\n",
	       c,
	       message == TrayRequestDock   ? "TrayRequestDock" :
	       message == TrayBeginMessage  ? "TrayBeginMessage" :
	       message == TrayCancelMessage ? "TrayCancelMessage" :
	       sxprint("%lx", message),
	       l1, l2, l3);

	if(message == TrayBeginMessage)
		message_cancel(c, l1);
	else if(message == TrayBeginMessage) {
		if(l2 > 5 * 1024) /* Don't bother with absurdly large messages. */
			return;

		m = emallocz(sizeof *m);
		m->timeout = l1;
		m->msg     = ixp_message(emallocz(l2), l2, MsgPack);
		m->id      = l3;

		/* Add the message to the end of the queue. */
		for(mp=&c->message; *mp; mp=&(*mp)->next)
			;
		*mp = m;
	}
}

void
client_message(Client *c, long type, int format, ClientMessageData* data) {
	Message *m;

	if(format == 8 && type == NET("SYSTEM_TRAY_MESSAGE_DATA")) {
		/* Append the data to the last incomplete message. */
		for(m = c->message; m && m->msg.pos >= m->msg.end; m++)
			;
		if(m) {
			memcpy(m->msg.pos, data, min(20, m->msg.end - m->msg.pos));
			m->msg.pos += min(20, m->msg.end - m->msg.pos);
		}
	}
}

static bool
config_event(Window *w, void *aux, XConfigureEvent *e) {
	Client *c;

	c = aux;
	if(false)
		movewin(c->indicator, addpt(w->r.min, Pt(1, 1)));
	return false;
}

static bool
configreq_event(Window *w, void *aux, XConfigureRequestEvent *e) {

	Dprint("configreq_event(%W)\n", w);
	/* This seems, sadly, to be necessary. */
	tray_update();
	return false;
}

static bool
map_event(Window *w, void *aux, XMapEvent *e) {

	Dprint("client map_event(%W)\n", w);
	w->mapped = true;
	tray_update();
	return false;
}

static bool
unmap_event(Window *w, void *aux, XUnmapEvent *e) {

	Dprint("client map_event(%W)\n", w);
	tray_update();
	return false;
}

static bool
reparent_event(Window *w, void *aux, XReparentEvent *e) {

	Dprint("client reparent_event(%W)\n", w);
	return false;
}

static Handlers handlers = {
	.config = config_event,
	.configreq = configreq_event,
	.map = map_event,
	.unmap = unmap_event,
	.reparent = reparent_event,
};

