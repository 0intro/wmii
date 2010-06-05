/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <string.h>
#include <strings.h>
#include "fns.h"

static Handlers handlers;
static Handlers root_handlers;

void
restrut(Window *w, int orientation) {
	enum { Left, Right, Top, Bottom };
	Rectangle strut[4];
	Rectangle r;

	r = w->r;
	memset(strut, 0, sizeof strut);
	if(Dx(r) < Dx(scr.rect)/2 && orientation != OHorizontal) {
		if(r.min.x <= scr.rect.min.x) {
			strut[Left] = r;
			strut[Left].min.x = 0;
			strut[Left].max.x -= scr.rect.min.x;
		}
		if(r.max.x >= scr.rect.max.x) {
			strut[Right] = r;
			strut[Right].min.x -= scr.rect.max.x;
			strut[Right].max.x = 0;
		}
	}
	if(Dy(r) < Dy(scr.rect)/2 && orientation != OVertical) {
		if(r.min.y <= scr.rect.min.y) {
			strut[Top] = r;
			strut[Top].min.y = 0;
			strut[Top].max.y -= scr.rect.min.y;
		}
		if(r.max.y >= scr.rect.max.y) {
			strut[Bottom] = r;
			strut[Bottom].min.y -= scr.rect.max.y;
			strut[Bottom].max.y = 0;
		}
	}

#if 0
#define pstrut(name) \
	if(!eqrect(strut[name], ZR)) \
		fprint(2, "strut["#name"] = %R\n", strut[name])
	pstrut(Left);
	pstrut(Right);
	pstrut(Top);
	pstrut(Bottom);
#endif

	ewmh_setstrut(w, strut);
}

void
tray_init(void) {
	WinAttr wa;
	XWMHints hints = { 0, };

	wa.background_pixmap = None;
	wa.bit_gravity = NorthEastGravity;
	wa.border_pixel = 0;
	wa.event_mask = ExposureMask
		      | ButtonPressMask
		      | ButtonReleaseMask
		      | StructureNotifyMask
		      | SubstructureNotifyMask
		      /* Disallow clients reconfiguring themselves. */
		      | SubstructureRedirectMask;
	tray.win = createwindow(&scr.root, Rect(0, 0, 1, 1), scr.depth, InputOutput,
				       &wa, CWBackPixmap
					  | CWBitGravity
					  | CWEventMask);

	sethandler(tray.win, &handlers);
	pushhandler(&scr.root, &root_handlers, nil);
	selectinput(&scr.root, scr.root.eventmask | PropertyChangeMask);


	changeprop_string(tray.win, "_WMII_TAGS", tray.tags);

	changeprop_ulong(tray.win, "XdndAware", "ATOM", (ulong[1]){ 5 }, 1);

	changeprop_ulong(tray.selection->owner, Net("SYSTEM_TRAY_VISUAL"), "VISUALID",
			 &scr.visual->visualid, 1);
	changeprop_long(tray.win, Net("WM_WINDOW_TYPE"), "ATOM",
			(long[1]){ TYPE("DOCK") }, 1);

	changeprop_string(tray.win, Net("WM_NAME"), "witray");
	changeprop_textlist(tray.win, "WM_NAME", "STRING",
			    (char*[2]){ "witray", nil });
	changeprop_textlist(tray.win, "WM_CLASS", "STRING",
			    (char*[3]){ "witray", "witray", nil });
	changeprop_textlist(tray.win, "WM_COMMAND", "STRING", program_args);

	hints.flags = InputHint;
	hints.input = false;
	XSetWMHints(display, tray.win->xid, &hints);
	tray_resize(tray.win->r);
}

static void
tray_unmap(void) {
	unmapwin(tray.win);
	sendevent(&scr.root, false, SubstructureNotifyMask,
		  &(XUnmapEvent){
			.type = UnmapNotify,
			.event = scr.root.xid,
			.window = tray.win->xid
		  });
}

static void
tray_draw(Rectangle r) {
	int borderwidth;

	if(!tray.pixmap)
		return;

	borderwidth = 1;

	r = rectsetorigin(r, ZP);
	border(tray.pixmap, r, borderwidth, tray.selcolors.border);
	r = insetrect(r, borderwidth);
	fill(tray.pixmap, r, tray.selcolors.bg);
	XClearWindow(display, tray.win->xid);
}

void
tray_resize(Rectangle r) {
	WinHints hints;
	Image *oldimage;
	WinAttr wa;

	hints = ZWinHints;
	hints.position = true;
	hints.min = hints.max = Pt(Dx(r), Dy(r));
	hints.grav = Pt(tray.edge & East ? 0 :
			tray.edge & West ? 2 : 1,
			tray.edge & North ? 0 :
			tray.edge & South ? 2 : 1);
	/* Not necessary, since we specify fixed size, but... */
	// hints.base = Pt(2, 2);
	// hints.inc  = Pt(tray.iconsize, tray.iconsize);
	sethints(tray.win, &hints);

	if(!eqrect(tray.win->r, r)) {
		oldimage = tray.pixmap;

		tray.pixmap = allocimage(Dx(r), Dy(r), tray.win->depth);
		tray_draw(r);
		wa.background_pixmap = tray.pixmap->xid;
		setwinattr(tray.win, &wa, CWBackPixmap);

		freeimage(oldimage);
	}

	tray.r = r;
	tray.win->r = ZR; /* Force the configure event. */
	reshapewin(tray.win, r);
	restrut(tray.win, tray.orientation);
}

void
tray_update(void) {
	Rectangle r;
	Point p, offset, padding;
	Client *c;

	r = Rect(0, 0, tray.iconsize, tray.iconsize);
	padding = Pt(tray.padding, tray.padding);
	offset = padding;
	Dprint("tray_update()\n");
	for(c=tray.clients; c; c=c->next) {
		if(c->w.mapped) {
			reshapewin(&c->w, rectaddpt(r, offset));
			/* This seems, sadly, to be necessary. */
			sendevent(&c->w, false, StructureNotifyMask, &(XEvent){
				  .xconfigure = {
					.type = ConfigureNotify,
					.event = c->w.xid,
					.window = c->w.xid,
					.above = None,
					.x = c->w.r.min.x,
					.y = c->w.r.min.y,
					.width = Dx(c->w.r),
					.height = Dy(c->w.r),
					.border_width = 0,
				  }
			  });

			movewin(c->indicator, addpt(offset, Pt(2, 2)));
			if(tray.orientation == OHorizontal)
				offset.x += tray.iconsize + tray.padding;
			else
				offset.y += tray.iconsize + tray.padding;
		}
		if(c->w.mapped && client_hasmessage(c))
			mapwin(c->indicator);
		else
			unmapwin(c->indicator);
	}

	if(eqpt(offset, padding))
		tray_unmap();
	else {
		if(tray.orientation == OHorizontal)
			offset.y += tray.iconsize + tray.padding;
		else
			offset.x += tray.iconsize + tray.padding;

		r = Rpt(ZP, offset);
		p = subpt(scr.rect.max, r.max);
		if(tray.edge & East)
			p.x = 0;
		if(tray.edge & North)
			p.y = 0;
		tray_resize(rectaddpt(r, p));
		mapwin(tray.win);
	}

	tray_draw(tray.win->r);
}

static bool
config_event(Window *w, void *aux, XConfigureEvent *ev) {

	USED(aux);
	if(ev->send_event) {
		/*
		 * Per ICCCM §4.2.3, the window manager sends
		 * synthetic configure events in the root coordinate
		 * space when it changes the window configuration.
		 * This code assumes wmii's generous behavior of
		 * supplying all relevant members in every configure
		 * notify event.
		 */
		w->r = rectaddpt(rectsetorigin(Rect(0, 0, ev->width, ev->height),
					       Pt(ev->x, ev->y)),
				 Pt(ev->border_width, ev->border_width));
		restrut(w, tray.orientation);
	}
	return false;
}

static bool
expose_event(Window *w, void *aux, XExposeEvent *ev) {

	USED(w, aux, ev);
	tray_draw(tray.win->r);
	return false;
}

typedef struct Dnd Dnd;

struct Dnd {
	ulong	source;
	ulong	dest;
	long	data[4];
	Point	p;
	bool	have_actions;
};

static Dnd dnd;

#define Point(l) Pt((ulong)(l) >> 16, (ulong)(l) & 0xffff)
#define Long(p) ((long)(((ulong)(p).x << 16) | (ulong)(p).y))
#define sendmessage(...) BLOCK( \
		Dprint("(%W) %s 0x%ulx, 0x%ulx, 0x%ulx, 0x%ulx, 0x%ulx\n", __VA_ARGS__); \
		sendmessage(__VA_ARGS__); \
       )

static void
dnd_updatestatus(ulong dest) {
	if(dest == dnd.dest)
		return;

	if(dnd.dest && dnd.dest != ~0UL)
		sendmessage(window(dnd.dest), "XdndLeave", tray.win->xid, 0, 0, 0, 0);
	dnd.dest = dest;
	if(dest)
		sendmessage(window(dest), "XdndEnter", tray.win->xid,
			    dnd.data[0], dnd.data[1], dnd.data[2], dnd.data[3]);
	else
		sendmessage(window(dnd.source), "XdndStatus", tray.win->xid, (1<<1),
			    Long(tray.win->r.min), (Dx(tray.win->r)<<16) | Dy(tray.win->r), 0UL);
}

static void
copyprop_long(Window *src, Window *dst, char *atom, char *type, long max) {
	long *data;
	long n;

	/* Round trip. Really need to switch to XCB. */
	if((n = getprop_long(src, atom, type, 0, &data, max)))
		changeprop_long(dst, atom, type, data, n);
	free(data);
}

static void
copyprop_char(Window *src, Window *dst, char *atom, char *type, long max) {
	uchar *data;
	ulong actual, n;
	int format;

	n = getprop(src, atom, type, &actual, &format, 0, &data, max);
	if(n > 0 && format == 8 && xatom(type) == actual)
		changeprop_char(dst, atom, type, (char*)data, n);
	free(data);
}

static bool
message_event(Window *w, void *aux, XClientMessageEvent *e) {
	Client *c;
	long *l;
	Rectangle r;
	Point p;
	ulong msg;

	msg = e->message_type;
	l = e->data.l;
	Dprint("ClientMessage: %A\n", msg);
	if(e->format == 32)
		Dprint("\t0x%ulx, 0x%ulx, 0x%ulx, 0x%ulx, 0x%ulx\n",
		       l[0], l[1], l[2], l[3], l[4]);

	if(msg == xatom("XdndEnter")) {
		if(e->format != 32)
			return false;
		dnd = (Dnd){0};
		dnd.dest = ~0UL;
		dnd.source = l[0];
		bcopy(&l[1], dnd.data, sizeof dnd.data);

		copyprop_long(window(dnd.source), tray.win, "XdndSelection", "ATOM", 128);
		if(l[1] & 0x01)
			copyprop_long(window(dnd.source), tray.win, "XdndTypeList", "ATOM", 128);
		return false;
	}else
	if(msg == xatom("XdndLeave")) {
		if(e->format != 32)
			return false;
		dnd.source = 0UL;
		if(dnd.dest)
			sendmessage(window(dnd.dest), "XdndLeave", tray.win->xid, l[1], 0, 0, 0);
		return false;
	}else
	if(msg == xatom("XdndPosition")) {
		if(e->format != 32)
			return false;

		if(!dnd.have_actions && l[4] == xatom("XdndActionAsk")) {
			dnd.have_actions = true;
			copyprop_long(window(dnd.source), tray.win, "XdndActionList", "ATOM", 16);
			copyprop_char(window(dnd.source), tray.win, "XdndActionDescription", "ATOM", 16 * 32);
		}

		dnd.p = subpt(Point(l[2]), tray.win->r.min);
		for(c=tray.clients; c; c=c->next)
			if(rect_haspoint_p(c->w.r, dnd.p)) {
				dnd_updatestatus(c->w.xid);
				sendmessage(&c->w, "XdndPosition", tray.win->xid, l[1], l[2], l[3], l[4]);
				return false;
			}
		dnd_updatestatus(0UL);
		return false;
	}else
	if(msg == xatom("XdndStatus")) {
		if(e->format != 32)
			return false;
		if(l[0] != dnd.dest)
			return false;

		for(c=tray.clients; c; c=c->next)
			if(c->w.xid == dnd.dest) {
				p = Point(l[2]);
				r = Rpt(p, addpt(p, Point(l[3])));
				r = rect_intersection(r, rectaddpt(c->w.r, tray.win->r.min));

				sendmessage(window(dnd.source), "XdndStatus", tray.win->xid, l[1],
					    Long(r.min), (Dx(r)<<16) | Dy(r), l[4]);
				break;
			}
		return false;
	}else
	if(msg == xatom("XdndDrop") || msg == xatom("XdndFinished")) {
		if(e->format != 32)
			return false;

		for(c=tray.clients; c; c=c->next)
			if(c->w.xid == dnd.dest) {
				sendmessage(&c->w, atomname(msg),
					    tray.win->xid, l[1], l[2], 0L, 0L);
				break;
			}
		return false;
	}

	return true;
}

static Handlers handlers = {
	.message = message_event,
	.config = config_event,
	.expose = expose_event,
};

static bool
property_event(Window *w, void *aux, XPropertyEvent *ev) {
	if(ev->atom == NET("CURRENT_DESKTOP"))
		tray_resize(tray.r);
	Debug if(ev->atom == NET("CURRENT_DESKTOP"))
		print("property_event(_NET_CURRENT_DESKTOP)\n");
	return false;
}

static Handlers root_handlers = {
	.property = property_event,
};

