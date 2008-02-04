/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <ctype.h>
#include <X11/Xatom.h>
#include "fns.h"

#define Mbsearch(k, l, cmp) bsearch(k, l, nelem(l), sizeof(*l), cmp)

static Handlers handlers;

enum {
	ClientMask = StructureNotifyMask
		   | PropertyChangeMask
		   | EnterWindowMask
		   | FocusChangeMask,
	ButtonMask = ButtonPressMask
		   | ButtonReleaseMask,
};

static Group*	group;

static void
group_init(Client *c) {
	Group *g;
	long *ret;
	XWindow w;
	long n;

	w = c->w.hints->group;
	if(w == 0) {
		/* Not quite ICCCM compliant, but it seems to work. */
		n = getprop_long(&c->w, "WM_CLIENT_LEADER", "WINDOW", 0L, &ret, 1L);
		if(n == 0)
			return;
		w = *ret;
	}

	for(g=group; g; g=g->next)
		if(g->leader == w)
			break;
	if(g == nil) {
		g = emallocz(sizeof *g);
		g->leader = w;
		g->next = group;
		group = g;
	}
	c->group = g;
	g->ref++;
}

static void
group_remove(Client *c) {
	Group **gp;
	Group *g;

	g = c->group;
	if(g == nil)
		return;
	if(g->client == c)
		g->client = nil;
	g->ref--;
	if(g->ref == 0) {
		for(gp=&group; *gp; gp=&gp[0]->next)
			if(*gp == g)
				break;
		assert(*gp == g);
		gp[0] = gp[0]->next;
	}
}

static Client*
group_leader(Group *g) {
	Client *c;

	c = win2client(g->leader);
	if(c)
		return c;
	if(g->client)
		return g->client;
	/* Could do better. */
	for(c=client; c; c=c->next)
		if(c->frame && c->group == g)
			break;
	return c;
}

Client*
client_create(XWindow w, XWindowAttributes *wa) {
	Client **t, *c;
	WinAttr fwa;
	Point p;

	c = emallocz(sizeof *c);
	c->border = wa->border_width;

	c->r.min = Pt(wa->x, wa->y);
	c->r.max = addpt(c->r.min, Pt(wa->width, wa->height));

	c->w.type = WWindow;
	c->w.w = w;
	c->w.r = c->r;

	client_prop(c, xatom("WM_PROTOCOLS"));
	client_prop(c, xatom("WM_TRANSIENT_FOR"));
	client_prop(c, xatom("WM_NORMAL_HINTS"));
	client_prop(c, xatom("WM_HINTS"));
	client_prop(c, xatom("WM_CLASS"));
	client_prop(c, xatom("WM_NAME"));
	client_prop(c, xatom("_MOTIF_WM_HINTS"));

	XSetWindowBorderWidth(display, w, 0);
	XAddToSaveSet(display, w);

	fwa.override_redirect = true;
	fwa.background_pixmap = None;
	fwa.event_mask = SubstructureRedirectMask
		       | SubstructureNotifyMask
		       | ExposureMask
		       | EnterWindowMask
		       | PointerMotionMask
		       | ButtonPressMask
		       | ButtonReleaseMask;
	c->framewin = createwindow(&scr.root, c->r, scr.depth, InputOutput,
			&fwa, CWOverrideRedirect
			    | CWEventMask
			    | CWBackPixmap);
	c->framewin->aux = c;
	c->w.aux = c;
	sethandler(c->framewin, &framehandler);
	sethandler(&c->w, &handlers);

	XSelectInput(display, c->w.w, ClientMask);

	p.x = def.border;
	p.y = labelh(def.font);
	reparentwindow(&c->w, c->framewin, p);

	group_init(c);

	grab_button(c->framewin->w, AnyButton, AnyModifier);

	for(t=&client ;; t=&t[0]->next)
		if(!*t) {
			c->next = *t;
			*t = c;
			break;
		}

	ewmh_initclient(c);

	event("CreateClient %C\n", c);
	client_manage(c);
	return c;
}

void
client_manage(Client *c) {
	Client *trans;
	Frame *f;
	char *tags;

	if(Dx(c->r) == Dx(screen->r))
	if(Dy(c->r) == Dy(screen->r))
	if(c->w.ewmh.type == 0)
		fullscreen(c, true);

	tags = getprop_string(&c->w, "_WMII_TAGS");

	trans = win2client(c->trans);
	if(trans == nil && c->group)
		trans = group_leader(c->group);

	if(tags && (!trans || starting))
		utflcpy(c->tags, tags, sizeof c->tags);
	else if(trans)
		utflcpy(c->tags, trans->tags, sizeof c->tags);
	free(tags);

	/* Maybe not the best idea... */
	if(!c->trans || !c->tags[0])
		apply_rules(c);
	if(c->tags[0])
		apply_tags(c, c->tags);
	else
		apply_tags(c, "sel");

	if(!starting)
		view_update_all();

	bool newgroup = !c->group
		     || c->group->ref == 1
		     || selclient() && (selclient()->group == c->group)
		     || group_leader(c->group) && !client_viewframe(group_leader(c->group), c->sel->view);

	f = c->sel;
	if(!(c->w.ewmh.type & TypeSplash))
	if(newgroup) {
		if(f->area != f->view->sel)
			f->view->oldsel = f->view->sel;
		frame_focus(f);
	}else {
		frame_restack(c->sel, c->sel->area->sel);
		view_restack(c->sel->view);
	}

	flushenterevents();
}

static int /* Temporary Xlib error handler */
ignoreerrors(Display *d, XErrorEvent *e) {
	USED(d, e);
	return 0;
}

void
client_destroy(Client *c) {
	int (*handler)(Display*, XErrorEvent*);
	Rectangle r;
	char *none;
	Client **tc;
	bool hide;

	unmapwin(c->framewin);

	for(tc=&client; *tc; tc=&tc[0]->next)
		if(*tc == c) {
			*tc = c->next;
			break;
		}

	r = client_grav(c, ZR);

	hide = false;	
	if(!c->sel || c->sel->view != screen->sel)
		hide = true;

	XGrabServer(display);

	/* In case the client is already unmapped */
	handler = XSetErrorHandler(ignoreerrors);

	none = nil;
	client_setviews(c, &none);
	sethandler(&c->w, nil);
	refree(&c->tagre);
	refree(&c->tagvre);
	free(c->retags);

	if(hide)
		reparentwindow(&c->w, &scr.root, screen->r.max);
	else
		reparentwindow(&c->w, &scr.root, r.min);
	destroywindow(c->framewin);

	sync();
	XSetErrorHandler(handler);
	XUngrabServer(display);

	ewmh_destroyclient(c);
	group_remove(c);
	event("DestroyClient %C\n", c);

	flushenterevents();
	flushevents(FocusChangeMask, true);
	free(c->w.hints);
	free(c);
}

/* Convenience functions */
Frame*
client_viewframe(Client *c, View *v) {              
	Frame *f;

	for(f=c->frame; f; f=f->cnext)
		if(f->view == v)
			break;
	return f;
} 

Client*
selclient(void) {
	if(screen->sel->sel->sel)
		return screen->sel->sel->sel->client;
	return nil;
}

Client*
win2client(XWindow w) {
	Client *c;
	for(c=client; c; c=c->next)
		if(c->w.w == w) break;
	return c;
}

int
Cfmt(Fmt *f) {
	Client *c;

	c = va_arg(f->args, Client*);
	if(c)
		return fmtprint(f, "%W", &c->w);
	return fmtprint(f, "<nil>");
}

char*
clientname(Client *c) {
	if(c)
		return c->name;
	return "<nil>";
}

Rectangle
client_grav(Client *c, Rectangle rd) {
	Rectangle r, cr;
	Point sp;
	WinHints *h;

	h = c->w.hints;

	if(eqrect(rd, ZR)) {
		if(c->sel) {
			r = c->sel->floatr;
			cr = frame_rect2client(c, r, true);
		}else {
			cr = c->r;
			r = frame_client2rect(c, cr, true);
			r = rectsetorigin(r, cr.min);
		}
		sp = subpt(cr.min, r.min);
		r = gravitate(r, cr, h->grav);
		if(!h->gravstatic)
			r = rectsubpt(r, sp);
		return frame_rect2client(c, r, true);
	}else {
		r = frame_client2rect(c, rd, true);
		sp = subpt(rd.min, r.min);
		r = gravitate(rd, r, h->grav);
		if(!h->gravstatic)
			r = rectaddpt(r, sp);
		return frame_client2rect(c, r, true);
	}
}

bool
client_floats_p(Client *c) {
	return c->trans
            || c->floating
	    || c->fixedsize
	    || c->titleless
	    || c->borderless
	    || c->fullscreen
	    || (c->w.ewmh.type & (TypeDialog|TypeSplash|TypeDock));
}

Frame*
client_groupframe(Client *c, View *v) {
	if(c->group && c->group->client)
		return client_viewframe(c->group->client, v);
	return nil;
}

Rectangle
frame_hints(Frame *f, Rectangle r, Align sticky) {
	Rectangle or;
	Point p;
	Client *c;

	c = f->client;
	if(c->w.hints == nil)
		return r;

	or = r;
	r = frame_rect2client(c, r, f->area->floating);
	r = sizehint(c->w.hints, r);
	r = frame_client2rect(c, r, f->area->floating);

	if(!f->area->floating) {
		/* Not allowed to grow */
		if(Dx(r) > Dx(or))
			r.max.x = r.min.x+Dx(or);
		if(Dy(r) > Dy(or))
			r.max.y = r.min.y+Dy(or);
	}

	p = ZP;
	if((sticky&(East|West)) == East)
		p.x = Dx(or) - Dx(r);
	if((sticky&(North|South)) == South)
		p.y = Dy(or) - Dy(r);
	return rectaddpt(r, p);
}

static void
client_setstate(Client * c, int state) {
	long data[] = { state, None };

	changeprop_long(&c->w, "WM_STATE", "WM_STATE", data, nelem(data));
}

void
client_map(Client *c) {
	if(!c->w.mapped) {
		mapwin(&c->w);
		client_setstate(c, NormalState);
	}
}

void
client_unmap(Client *c, int state) {
	if(c->w.mapped) {
		unmapwin(&c->w);
		client_setstate(c, state);
	}
}

int
map_frame(Client *c) {
	return mapwin(c->framewin);
}

int
unmap_frame(Client *c) {
	return unmapwin(c->framewin);
}

void
focus(Client *c, bool user) {
	View *v;
	Frame *f;

	USED(user);
	f = c->sel;
	if(!f)
		return;
	/*
	if(!user && c->noinput)
		return;
	 */

	v = f->view;
	if(v != screen->sel)
		view_focus(screen, v);
	frame_focus(c->sel);
}

void
client_focus(Client *c) {
	static long id;
	flushevents(FocusChangeMask, true);

	Dprint(DFocus, "client_focus([%C]%s) %ld\n", c, clientname(c), id++);

	if(c) {
		if(c->noinput)
			return;
		if(c->group)
			c->group->client = c;
	}

	sync();
	flushevents(FocusChangeMask, true);
	Dprint(DFocus, "client_focus([%C]%s) %ld\n", c, clientname(c), id);
	if(screen->focus != c) {
		Dprint(DFocus, "\t[%C]%s => [%C]%s\n", screen->focus, clientname(screen->focus),
				c, clientname(c));
		if(c)
			setfocus(&c->w, RevertToParent);
		else
			setfocus(screen->barwin, RevertToParent);
		event("ClientFocus %C\n", c);

		sync();
		flushevents(FocusChangeMask, true);
	}
}

void
client_resize(Client *c, Rectangle r) {
	Frame *f;

	f = c->sel;
	frame_resize(f, r);

	if(f->area->view != screen->sel) {
		client_unmap(c, IconicState);
		unmap_frame(c);
		return;
	}

	c->r = rectaddpt(f->crect, f->r.min);

	if((f->area->mode == Colmax) && (f->area->sel != f)) {
		unmap_frame(c);
		client_unmap(c, IconicState);
	}else if(f->collapsed) {
		reshapewin(c->framewin, f->r);
		map_frame(c);
		client_unmap(c, IconicState);
	}else {
		client_map(c);
		reshapewin(c->framewin, f->r);
		reshapewin(&c->w, f->crect);
		map_frame(c);
		client_configure(c);
		ewmh_framesize(c);
	}
	sync(); /* Not ideal. */
	flushenterevents();
	flushevents(FocusChangeMask|ExposureMask, true);
}

void
client_setcursor(Client *c, Cursor cur) {
	WinAttr wa;

	if(c->cursor != cur) {
		c->cursor = cur;
		wa.cursor = cur;
		setwinattr(c->framewin, &wa, CWCursor);
	}
}

void
client_configure(Client *c) {
	XConfigureEvent e;
	Rectangle r;

	r = rectsubpt(c->r, Pt(c->border, c->border));

	e.type = ConfigureNotify;
	e.event = c->w.w;
	e.window = c->w.w;
	e.above = None;
	e.override_redirect = false;

	e.x = r.min.x;
	e.y = r.min.y;
	e.width = Dx(r);
	e.height = Dy(r);
	e.border_width = c->border;

	sendevent(&c->w, false, StructureNotifyMask, (XEvent*)&e);
}

void
client_message(Client *c, char *msg, long l2) {
	sendmessage(&c->w, "WM_PROTOCOLS", xatom(msg), xtime, l2, 0, 0);
}

void
client_kill(Client *c, bool nice) {
	if(nice && (c->proto & ProtoDelete)) {
		client_message(c, "WM_DELETE_WINDOW", 0);
		ewmh_pingclient(c);
	}else
		XKillClient(display, c->w.w);
}

void
fullscreen(Client *c, int fullscreen) {
	Frame *f;
	bool wassel;
	
	if(fullscreen == Toggle)
		fullscreen = c->fullscreen ^ On;
	if(fullscreen == c->fullscreen)
		return;

	event("Fullscreen %C %s\n", c, (fullscreen ? "on" : "off"));
	c->fullscreen = fullscreen;
	ewmh_updatestate(c);

	if(!fullscreen)
		for(f=c->frame; f; f=f->cnext) {
			if(f->oldarea == 0) {
				frame_resize(f, f->floatr);
				if(f->view == screen->sel) /* FIXME */
					client_resize(f->client, f->r);

			}
			else if(f->oldarea > 0) {
				wassel = (f == f->area->sel);
				area_moveto(view_findarea(f->view, f->oldarea, true), f);
				if(wassel)
					frame_focus(f);
			}
		}
	else {
		for(f=c->frame; f; f=f->cnext)
			f->oldarea = -1;
		if((f = c->sel))
			if(f->view == screen->sel)
				view_focus(screen, f->view);
	}
}

void
client_seturgent(Client *c, bool urgent, int from) {
	XWMHints *wmh;
	char *cfrom, *cnot;
	Frame *f, *ff;
	Area *a;

	if(urgent == Toggle)
		urgent = c->urgent ^ On;

	cfrom = (from == UrgManager ? "Manager" : "Client");
	cnot = (urgent ? "" : "Not");

	if(urgent != c->urgent) {
		event("%sUrgent %C %s\n", cnot, c, cfrom);
		c->urgent = urgent;
		ewmh_updatestate(c);
		if(c->sel) {
			if(c->sel->view == screen->sel)
				frame_draw(c->sel);
			for(f=c->frame; f; f=f->cnext) {
				SET(ff);
				if(!urgent)
					for(a=f->view->area; a; a=a->next)
						for(ff=a->frame; ff; ff=ff->anext)
							if(ff->client->urgent) break;
				if(urgent || ff == nil)
					event("%sUrgentTag %s %s\n", cnot, cfrom, f->view->name);
			}
		}
	}

	if(from == UrgManager) {
		wmh = XGetWMHints(display, c->w.w);
		if(wmh == nil)
			wmh = emallocz(sizeof *wmh);

		wmh->flags &= ~XUrgencyHint;
		if(urgent)
			wmh->flags |= XUrgencyHint;
		XSetWMHints(display, c->w.w, wmh);
		XFree(wmh);
	}
}

/* X11 stuff */
void
update_class(Client *c) {
	char *str;

	str = utfrune(c->props, L':');
	if(str)
		str = utfrune(str+1, L':');
	if(str == nil) {
		strcpy(c->props, "::");
		str = c->props + 1;
	}
	utflcpy(str+1, c->name, sizeof c->props);
}

static void
client_updatename(Client *c) {
	char *str;

	c->name[0] = '\0';

	str = getprop_string(&c->w, "_NET_WM_NAME");
	if(str == nil)
		str = getprop_string(&c->w, "WM_NAME");
	if(str)
		utflcpy(c->name, str, sizeof c->name);
	free(str);

	update_class(c);
	if(c->sel)
		frame_draw(c->sel);
}

static void
updatemwm(Client *c) {
	enum {
		All =		0x1,
		Border =	0x2,
		Title =		0x8,
		FlagDecor =	0x2,
		Flags =		0,
		Decor =		2,
	};
	Rectangle r;
	ulong *ret;
	int n;

	/* To quote Metacity, or KWin quoting Metacity:
	 *   We support MWM hints deemed non-stupid
	 * Our definition of non-stupid is a bit less lenient than
	 * theirs, though. In fact, we don't really even support the
	 * idea of supporting the hints that we support, but apps
	 * like xmms (which noone should use) break if we don't.
	 */

	n = getprop_long(&c->w, "_MOTIF_WM_HINTS", "_MOTIF_WM_HINTS",
			0L, (long**)&ret, 3L);

	/* FIXME: Should somehow handle all frames. */
	if(c->sel)
		r = client_grav(c, ZR);

	c->borderless = 0;
	c->titleless = 0;
	if(n >= 3 && (ret[Flags]&FlagDecor)) {
		if(ret[Decor]&All)
			ret[Decor] ^= ~0;
		c->borderless = !(ret[Decor]&Border);
		c->titleless = !(ret[Decor]&Title);
	}
	free(ret);

	if(c->sel) {
		r = client_grav(c, r);
		client_resize(c, r);
		frame_draw(c->sel);
	}
}

void
client_prop(Client *c, Atom a) {
	XWMHints *wmh;
	char **class;
	int n;

	if(a == xatom("WM_PROTOCOLS"))
		c->proto = ewmh_protocols(&c->w);
	else
	if(a == xatom("_NET_WM_NAME"))
		goto wmname;
	else
	if(a == xatom("_MOTIF_WM_HINTS"))
		updatemwm(c);
	else
	switch (a) {
	default:
		ewmh_prop(c, a);
		break;
	case XA_WM_TRANSIENT_FOR:
		XGetTransientForHint(display, c->w.w, &c->trans);
		break;
	case XA_WM_NORMAL_HINTS:
		sethints(&c->w);
		if(c->w.hints)
			c->fixedsize = eqpt(c->w.hints->min, c->w.hints->max);
		break;
	case XA_WM_HINTS:
		wmh = XGetWMHints(display, c->w.w);
		if(wmh) {
			c->noinput = !((wmh->flags&InputFocus) && wmh->input);
			client_seturgent(c, (wmh->flags & XUrgencyHint) != 0, UrgClient);
			XFree(wmh);
		}
		break;
	case XA_WM_CLASS:
		n = getprop_textlist(&c->w, "WM_CLASS", &class);
		snprint(c->props, sizeof c->props, "%s:%s:",
				(n > 0 ? class[0] : "<nil>"),
				(n > 1 ? class[1] : "<nil>"));
		freestringlist(class);
		update_class(c);
		break;
	case XA_WM_NAME:
wmname:
		client_updatename(c);
		break;
	}
}

/* Handlers */
static void
configreq_event(Window *w, XConfigureRequestEvent *e) {
	Rectangle r, cr;
	Client *c;

	c = w->aux;

	r = client_grav(c, ZR);
	r.max = subpt(r.max, r.min);

	if(e->value_mask & CWX)
		r.min.x = e->x;
	if(e->value_mask & CWY)
		r.min.y = e->y;
	if(e->value_mask & CWWidth)
		r.max.x = e->width;
	if(e->value_mask & CWHeight)
		r.max.y = e->height;

	if(e->value_mask & CWBorderWidth)
		c->border = e->border_width;

	r.max = addpt(r.min, r.max);
	cr = r;
	r = client_grav(c, r);

	if(c->sel->area->floating) {
		client_resize(c, r);
		sync();
		flushenterevents();
	}else {
		c->sel->floatr = r;
		client_configure(c);
	}
}

static void
destroy_event(Window *w, XDestroyWindowEvent *e) {
	USED(w, e);

	client_destroy(w->aux);
}

static void
enter_event(Window *w, XCrossingEvent *e) {
	Client *c;
	
	c = w->aux;
	if(e->detail != NotifyInferior) {
		if(screen->focus != c) {
			Dprint(DGeneric, "enter_notify([%C]%s)\n", c, c->name);
			focus(c, false);
		}
		client_setcursor(c, cursor[CurNormal]);
	}else
		Dprint(DGeneric, "enter_notify(%C[NotifyInferior]%s)\n", c, c->name);
}

static void
focusin_event(Window *w, XFocusChangeEvent *e) {
	Client *c, *old;

	c = w->aux;

	print_focus("focusin_event", c, c->name);

	if(e->mode == NotifyGrab)
		screen->hasgrab = c;

	old = screen->focus;
	screen->focus = c;
	if(c != old) {
		if(c->sel)
			frame_draw(c->sel);
	}
}

static void
focusout_event(Window *w, XFocusChangeEvent *e) {
	Client *c;

	c = w->aux;
	if((e->mode == NotifyWhileGrabbed) && (screen->hasgrab != &c_root)) {
		if(screen->focus)
			screen->hasgrab = screen->focus;
	}else if(screen->focus == c) {
		print_focus("focusout_event", &c_magic, "<magic>");
		screen->focus = &c_magic;
		if(c->sel)
			frame_draw(c->sel);
	}
}

static void
unmap_event(Window *w, XUnmapEvent *e) {
	Client *c;
	
	c = w->aux;
	if(!e->send_event)
		c->unmapped--;
	client_destroy(c);
}

static void
map_event(Window *w, XMapEvent *e) {
	Client *c;

	USED(e);
	
	c = w->aux;
	if(c == selclient())
		client_focus(c);
}

static void
property_event(Window *w, XPropertyEvent *e) {
	Client *c;

	if(e->state == PropertyDelete)
		return;

	c = w->aux;
	client_prop(c, e->atom);
}

static Handlers handlers = {
	.configreq = configreq_event,
	.destroy = destroy_event,
	.enter = enter_event,
	.focusin = focusin_event,
	.focusout = focusout_event,
	.map = map_event,
	.unmap = unmap_event,
	.property = property_event,
};

/* Other */
void
client_setviews(Client *c, char **tags) {
	Frame **fp, *f;
	int cmp;

	fp = &c->frame;
	while(*fp || *tags) {
		SET(cmp);
		while(*fp) {
			if(*tags) {
				cmp = strcmp(fp[0]->view->name, *tags);
				if(cmp >= 0)
					break;
			}

			f = *fp;
			view_detach(f);
			*fp = f->cnext;
			if(c->sel == f)
				c->sel = *fp;
			free(f);
		}
		if(*tags) {
			if(!*fp || cmp > 0) {
				f = frame_create(c, view_create(*tags));
				if(f->view == screen->sel || !c->sel)
					c->sel = f;
				kludge = c;
				view_attach(f->view, f);
				kludge = nil;
				f->cnext = *fp;
				*fp = f;
			}
			if(fp[0]) fp=&fp[0]->cnext;
			tags++;
		}
	}
	if(c->sel == nil)
		c->sel = c->frame;
}

static int
bsstrcmp(const void *a, const void *b) {
	return strcmp((char*)a, (char*)b);
}

static int
strpcmp(const void *ap, const void *bp) {
	char **a, **b;
	
	a = (char**)ap;
	b = (char**)bp;
	return strcmp(*a, *b);
}

static char *badtags[] = {
	".",
	"..",
	"sel",
};

void
apply_tags(Client *c, const char *tags) {
	uint i, j, k, n;
	bool add;
	char buf[512], last;
	char *toks[32];
	char **p;
	char *cur, *s;

	buf[0] = 0;

	for(n = 0; tags[n]; n++)
		if(!isspace(tags[n]))
			break;

	if(tags[n] == '+' || tags[n] == '-')
		utflcpy(buf, c->tags, sizeof c->tags);
	strlcat(buf, &tags[n], sizeof buf);

	n = 0;
	add = true;
	if(buf[0] == '+')
		n++;
	else if(buf[0] == '-') {
		n++;
		add = false;
	}

	j = 0;
	while(buf[n] && n < sizeof(buf) && j < 32) { 
		if(buf[n] == '/') {
			for(i=n+1; i < sizeof(buf) - 1; i++)
				if(buf[i] == '/')
					break;
			if(buf[i] != '/')
				goto ifnot;
			i++;
			if(buf[i] != '+'
			&& buf[i] != '-'
			&& buf[i] != '\0') /* Don't be lenient */
				goto ifnot;
			buf[i-1] = '\0';
			if(add)
				reinit(&c->tagre, buf+n+1);
			else
				reinit(&c->tagvre, buf+n+1);
			last = buf[i];
			buf[i] = '\0';
			goto next;
		}
	ifnot:
		for(i = n; i < sizeof(buf) - 1; i++)
			if(buf[i] == '+'
			|| buf[i] == '-'
			|| buf[i] == '\0')
				break;
		last = buf[i];
		buf[i] = '\0';

		trim(buf+n, " \t/");

		cur = nil;
		if(!strcmp(buf+n, "~"))
			c->floating = add;
		else
		if(!strcmp(buf+n, "!") || !strcmp(buf+n, "sel"))
			cur = screen->sel->name;
		else
		if(!Mbsearch(buf+n, badtags, bsstrcmp))
			cur = buf+n;

		if(cur && j < nelem(toks)-1) {
			if(add)
				toks[j++] = cur;
			else {
				for(i = 0, k = 0; i < j; i++)
					if(strcmp(toks[i], cur))
						toks[k++] = toks[i];
				j = k;
			}
		}

	next:
		n = i + 1;
		if(last == '+')
			add = true;
		if(last == '-')
			add = false;
		if(last == '\0')
			buf[n] = '\0';
	}

	toks[j] = nil;
	qsort(toks, j, sizeof *toks, strpcmp);
	uniq(toks);

	s = join(toks, "+");
	utflcpy(c->tags, s, sizeof c->tags);
	changeprop_string(&c->w, "_WMII_TAGS", s);
	free(s);

	free(c->retags);
	p = view_names();
	grep(p, c->tagre.regc, 0);
	grep(p, c->tagvre.regc, GInvert);
	c->retags = comm(CRight, toks, p);
	free(p);

	if(c->retags[0] == nil && toks[0] == nil) {
		if(c->tagre.regex)
			toks[0] = "orphans";
		else
			toks[0] = screen->sel->name;
		toks[1] = nil;
	}

	p = comm(~0, c->retags, toks);
	client_setviews(c, p);
	free(p);
}

void
apply_rules(Client *c) {
	Rule *r;

	if(def.tagrules.string) 	
		for(r=def.tagrules.rule; r; r=r->next)
			if(regexec(r->regex, c->props, nil, 0)) {
				apply_tags(c, r->value);
				break;
			}
}

