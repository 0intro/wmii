/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <string.h>
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
	if(true)
		tray.win = createwindow(&scr.root, Rect(0, 0, 1, 1), scr.depth, InputOutput,
					       &wa, CWBackPixmap
						  | CWBitGravity
						  | CWEventMask);
	else {
		wa.colormap = XCreateColormap(display, scr.root.xid, scr.visual32, AllocNone);
		tray.win = createwindow_visual(&scr.root, Rect(0, 0, 1, 1), 32, scr.visual32, InputOutput,
					       &wa, CWBackPixmap
						  | CWBorderPixel
						  | CWColormap
						  | CWBitGravity
						  | CWEventMask);
		XFreeColormap(display, wa.colormap);
	}

	sethandler(tray.win, &handlers);
	pushhandler(&scr.root, &root_handlers, nil);
	selectinput(&scr.root, scr.root.eventmask | PropertyChangeMask);

	changeprop_string(tray.win, "_WMII_TAGS", tray.tags);

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
		unmapwin(tray.win);
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

static bool
message_event(Window *w, void *aux, XClientMessageEvent *ev) {

	Dprint("tray_message: %s\n", XGetAtomName(display, ev->message_type));
	return false;
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

