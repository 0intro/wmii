/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <ctype.h>
#include <strings.h>
#include <signal.h>
#include <X11/Xatom.h>
#include "fns.h"

#define Mbsearch(k, l, cmp) bsearch(k, l, nelem(l), sizeof(*l), cmp)

static Handlers handlers;
static Handlers ignorehandlers;

enum {
	ClientMask = StructureNotifyMask
		   | PropertyChangeMask
		   | EnterWindowMask
		   | FocusChangeMask,
	ButtonMask = ButtonPressMask
		   | ButtonReleaseMask,
};

static Group*	group;

void
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
		free(ret);
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

void
group_remove(Client *c) {
	Group **gp;
	Group *g;

	g = c->group;
	c->group = nil;
	if(g == nil)
		return;
	if(g->client == c)
		g->client = nil;
	g->ref--;
	if(g->ref == 0) {
		for(gp=&group; *gp; gp=&gp[0]->next)
			if(*gp == g) break;
		assert(*gp == g);
		gp[0] = gp[0]->next;
		free(g);
	}
}

Client*
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
	char **host = nil;
	ulong *pid = nil;

	c = emallocz(sizeof *c);
	c->fullscreen = -1;
	c->border = wa->border_width;

	c->r = rectsetorigin(Rect(0, 0, wa->width, wa->height),
			     Pt(wa->x, wa->y));

	c->w.type = WWindow;
	c->w.xid = w;
	c->w.r = c->r;
	c->w.aux = c;

	setborder(&c->w, 0, &(Color){0});

	client_prop(c, xatom("WM_PROTOCOLS"));
	client_prop(c, xatom("WM_TRANSIENT_FOR"));
	client_prop(c, xatom("WM_NORMAL_HINTS"));
	client_prop(c, xatom("WM_HINTS"));
	client_prop(c, xatom("WM_CLASS"));
	client_prop(c, xatom("WM_NAME"));
	client_prop(c, xatom("_MOTIF_WM_HINTS"));

	gethostname(hostname, sizeof(hostname) - 1);
	if(getprop_textlist(&c->w, "WM_CLIENT_MACHINE", &host) &&
	   getprop_ulong(&c->w, Net("WM_PID"), "CARDINAL", 0, &pid, 1) &&
	   !strcmp(hostname, *host))
		c->pid = (int)*pid;
	freestringlist(host);
	free(pid);

	c->rgba = render_argb_p(c->w.visual);
	client_reparent(c);

	sethandler(&c->w, &handlers);
	pushhandler(&c->w, &ignorehandlers, nil);

	selectinput(&c->w, ClientMask);

	group_init(c);

	grab_button(c->framewin->xid, AnyButton, AnyModifier);

	for(t=&client ;; t=&t[0]->next)
		if(!*t) {
			c->next = *t;
			*t = c;
			break;
		}


	/*
	 * It's actually possible for a window to be destroyed
	 * before we get a chance to reparent it. Check for that
	 * now, because otherwise we'll wind up mapping a
	 * perceptibly empty frame before it's destroyed.
	 */
	traperrors(true);
	XAddToSaveSet(display, w);
	if(traperrors(false)) {
		client_destroy(c);
		return nil;
	}

	ewmh_initclient(c);

	event("CreateClient %#C\n", c);
	client_manage(c);
	return c;
}

void
client_reparent(Client *c) {
	Window *fw;
	WinAttr wa;
	bool rgba;

	rgba = c->rgba | RGBA_P(def.normcolor) | RGBA_P(def.focuscolor);

	fw = c->framewin;
	if(fw && (fw->depth == 32) == rgba)
		return;

	wa.background_pixmap = None;
	wa.bit_gravity = NorthWestGravity;
	wa.event_mask = ButtonPressMask
		       | ButtonReleaseMask
		       | EnterWindowMask
		       | ExposureMask
		       | PointerMotionMask
		       | StructureNotifyMask
		       | SubstructureNotifyMask
		       | SubstructureRedirectMask;
	wa.override_redirect = true;
	if(rgba)
		c->framewin = createwindow_rgba(&scr.root, c->r,
				&wa, CWBackPixmap
				   | CWBitGravity
				   | CWEventMask
				   | CWOverrideRedirect);
	else
		c->framewin = createwindow(&scr.root, c->r, scr.depth, InputOutput,
				&wa, CWBackPixmap
				   | CWBitGravity
				   | CWEventMask
				   | CWOverrideRedirect);

	c->framewin->aux = c;
	sethandler(c->framewin, &framehandler);
	pushhandler(c->framewin, &ignorehandlers, nil);
	reparentwindow(&c->w, c->framewin, ZP);
	if(fw)
		destroywindow(fw);
}

static bool
apply_rules(Client *c) {
	IxpMsg m;
	Rule *r;
	Ruleval *rv;
	bool ret, more;

	ret = true;
	more = true;
	for(r=def.rules.rule; r && more; r=r->next)
		if(regexec(r->regex, c->props, nil, 0)) {
			more = false;
			for(rv=r->values; rv; rv=rv->next) {
				if(!strcmp(rv->key, "continue"))
					more = true;
				else if(!strcmp(rv->key, "tags"))
					utflcpy(c->tags, rv->value, sizeof c->tags);
				else if(!strcmp(rv->key, "force-tags")) {
					utflcpy(c->tags, rv->value, sizeof c->tags);
					ret = false;
				}else {
					bufclear();
					bufprint("%s %s", rv->key, rv->value);
					m = ixp_message(buffer, _buf_end - buffer, MsgPack);
					if(waserror())
						warning("processing rule %q=%q: %r", rv->key, rv->value);
					else {
						message_client(c, &m);
						poperror();
					}
				}
			}
		}
	return ret;
}

void
client_manage(Client *c) {
	Client *leader;
	char *tags;
	bool dotags;

	if(Dx(c->r) == Dx(screen->r))
	if(Dy(c->r) == Dy(screen->r))
	if(c->w.ewmh.type == 0)
		fullscreen(c, true, -1);

	dotags = apply_rules(c);

	if(!c->tags[0] || dotags) {
		leader = win2client(c->trans);
		if(leader == nil && c->group)
			leader = group_leader(c->group);

		tags = getprop_string(&c->w, "_WMII_TAGS");
		if(tags)
			utflcpy(c->tags, tags, sizeof c->tags);
		else if(leader)
			utflcpy(c->tags, leader->tags, sizeof c->tags);
		free(tags);
	}

	if(c->tags[0])
		client_applytags(c, c->tags);
	else
		client_applytags(c, "sel");

	if(!starting)
		view_update_all();

	bool newgroup = !c->group
		     || c->group->ref == 1
		     || selclient() && (selclient()->group == c->group)
		     || group_leader(c->group)
		        && !client_viewframe(group_leader(c->group),
					     c->sel->view);

	/* f = c->sel; */
	if(!(c->w.ewmh.type & TypeSplash))
		if(newgroup) {
			/* XXX: Look over this.
			if(f->area != f->view->sel)
				f->view->oldsel = f->view->sel;
			*/
		}else {
			frame_restack(c->sel, c->sel->area->sel);
			view_restack(c->sel->view);
		}
}

void
client_destroy(Client *c) {
	Rectangle r;
	char *none;
	Client **tc;
	bool hide;

	unmapwin(c->framewin);
	client_seturgent(c, false, UrgClient);

	for(tc=&client; *tc; tc=&tc[0]->next)
		if(*tc == c) {
			*tc = c->next;
			break;
		}

	r = client_grav(c, ZR);

	hide = (!c->sel || c->sel->view != selview);

	/* In case the client is already destroyed. */
	traperrors(true);

	sethandler(&c->w, nil);
	if(hide)
		reparentwindow(&c->w, &scr.root, screen->r.max);
	else
		reparentwindow(&c->w, &scr.root, r.min);

	if(starting >= 0)
		XRemoveFromSaveSet(display, c->w.xid);

	none = nil;
	client_setviews(c, &none);
	if(starting >= 0) {
		client_unmap(c, WithdrawnState);
		delproperty(&c->w, "_WMII_TAGS");
	}
	refree(&c->tagre);
	refree(&c->tagvre);
	free(c->retags);

	traperrors(false);

	destroywindow(c->framewin);

	ewmh_destroyclient(c);
	group_remove(c);
	if(starting >= 0)
		event("DestroyClient %#C\n", c);

	event_flush(FocusChangeMask, true);
	cleanupwindow(&c->w);
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
	if(selview->sel->sel)
		return selview->sel->sel->client;
	return nil;
}

Client*
win2client(XWindow w) {
	Client *c;
	for(c=client; c; c=c->next)
		if(c->w.xid == w) break;
	return c;
}

int
Cfmt(Fmt *f) {
	Client *c;

	c = va_arg(f->args, Client*);
	if(c)
		if(f->flags & FmtSharp)
			return fmtprint(f, "%W", &c->w);
		else
			return fmtprint(f, "%s", c->name);
	return fmtprint(f, "<nil>");
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
	if(c->floating == Never)
		return false;
	return c->trans
	    || c->floating
	    || c->fixedsize
	    || c->titleless
	    || c->borderless
	    || c->fullscreen >= 0
	    || (c->w.ewmh.type & (TypeDialog|TypeSplash|TypeDock|TypeMenu|TypeToolbar));
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
	WinHints h;
	Point p;
	Client *c;

	c = f->client;
	if(c->w.hints == nil)
		return r;

	or = r;
	h = frame_gethints(f);
	r = sizehint(&h, r);

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
	if(c->w.mapped)
		unmapwin(&c->w);
	client_setstate(c, state);
}

int
client_mapframe(Client *c) {
	return mapwin(c->framewin);
}

int
client_unmapframe(Client *c) {
	return unmapwin(c->framewin);
}

void
focus(Client *c, bool user) {
	View *v;
	Frame *f;

	Dprint(DFocus, "focus(%#C, %d)\n", c, user);
	if(!c->nofocus || user)
	if((f = c->sel)) {
		v = f->view;
		if(v != selview)
			view_focus(screen, v);
		frame_focus(c->sel);
		view_restack(c->sel->view);
	}
}

void
client_focus(Client *c) {
	/* Round trip. */

	if(c && c->group)
		c->group->client = c;

	sync();
	event_flush(FocusChangeMask, true);

	Dprint(DFocus, "client_focus([%#C]%C) collapsed=%s\n",
	       c, c, c && c->sel->collapsed ? "true" : "false");
	Dprint(DFocus, "\t[%#C]%C\n\t=> [%#C]%C\n",
	       disp.focus, disp.focus, c, c);

	if(disp.focus != c) {
		if(c && !c->sel->collapsed) {
			if(!c->noinput)
				setfocus(&c->w, RevertToParent);
			else if(c->proto & ProtoTakeFocus) {
				event_updatextime();
				client_message(c, "WM_TAKE_FOCUS", 0);
			}
		}else
			setfocus(screen->barwin, RevertToParent);

		sync();
		event_flush(FocusChangeMask, true);
	}
}

void
client_resize(Client *c, Rectangle r) {
	Frame *f;

	f = c->sel;
	frame_resize(f, r);

	if(f->view != selview) {
		client_unmap(c, IconicState);
		client_unmapframe(c);
		return;
	}

	c->r = rectaddpt(f->crect, f->r.min);

	if(f->collapsed) {
		if(f->area->max && !resizing)
			client_unmapframe(c);
		else {
			reshapewin(c->framewin, f->r);
			movewin(&c->w, f->crect.min);
			client_mapframe(c);
		}
		client_unmap(c, IconicState);
	}else {
		client_map(c);
		reshapewin(c->framewin, f->r);
		reshapewin(&c->w, f->crect);
		client_mapframe(c);
		if(!eqrect(c->r, c->configr))
			client_configure(c);
		ewmh_framesize(c);
	}
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
	Rectangle r;

	c->configr = c->r;
	r = rectsubpt(c->r, Pt(c->border, c->border));

	sendevent(&c->w, false, StructureNotifyMask,
		  &(XConfigureEvent) {
			  .type = ConfigureNotify,
			  .event = c->w.xid,
			  .window = c->w.xid,

			  .x = r.min.x,
			  .y = r.min.y,
			  .width = Dx(r),
			  .height = Dy(r),
			  .border_width = c->border,
		  });
}

void
client_message(Client *c, char *msg, long l2) {
	sendmessage(&c->w, "WM_PROTOCOLS", xatom(msg), event_xtime, l2, 0, 0);
}

void
client_kill(Client *c, bool nice) {

	if(!nice) {
		if(c->pid)
			kill(c->pid, SIGKILL);
		XKillClient(display, c->w.xid);
	}
	else if(c->proto & ProtoDelete) {
		c->dead = 1;
		client_message(c, "WM_DELETE_WINDOW", 0);
		ewmh_checkresponsive(c);
	}
	else
		XKillClient(display, c->w.xid);
}

void
fullscreen(Client *c, int fullscreen, long screen) {
	Client *leader;
	Frame *f;
	bool wassel;

	if(fullscreen == Toggle)
		fullscreen = (c->fullscreen >= 0) ^ On;
	if(fullscreen == (c->fullscreen >= 0))
		return;

	event("Fullscreen %#C %s\n", c, (fullscreen ? "on" : "off"));

	c->fullscreen = -1;
	if(!fullscreen)
		for(f=c->frame; f; f=f->cnext) {
			if(f->oldarea == 0) {
				frame_resize(f, f->floatr);
				if(f->view == selview) /* FIXME */
					client_resize(f->client, f->r);

			}
			else if(f->oldarea > 0) {
				wassel = (f == f->area->sel);
				area_moveto(view_findarea(f->view, f->oldscreen, f->oldarea, true),
					    f);
				if(wassel)
					frame_focus(f);
			}
		}
	else {
		c->fullscreen = 0;
		if(screen >= 0)
			c->fullscreen = screen;
		else if(c->sel)
			c->fullscreen = ownerscreen(c->r);
		else if(c->group && (leader = group_leader(c->group)) && leader->sel)
			c->fullscreen = ownerscreen(leader->r);
		else if(selclient())
			c->fullscreen = ownerscreen(selclient()->r);

		for(f=c->frame; f; f=f->cnext)
			f->oldarea = -1;
		if((f = c->sel))
			view_update(f->view);
	}
	ewmh_updatestate(c);
}

void
client_seturgent(Client *c, int urgent, int from) {
	XWMHints *wmh;
	char *cfrom, *cnot;
	Frame *f;

	if(urgent == Toggle)
		urgent = c->urgent ^ On;

	cfrom = (from == UrgManager ? "Manager" : "Client");
	cnot = (urgent ? "" : "Not");

	if(urgent != c->urgent) {
		event("%sUrgent %#C %s\n", cnot, c, cfrom);
		c->urgent = urgent;
		ewmh_updatestate(c);
		if(c->sel)
			frame_draw(c->sel);

		for(f=c->frame; f; f=f->cnext)
			view_update_urgency(f->view, cfrom);
	}

	if(from == UrgManager) {
		wmh = XGetWMHints(display, c->w.xid);
		if(wmh == nil)
			wmh = emallocz(sizeof *wmh);

		wmh->flags &= ~XUrgencyHint;
		if(urgent)
			wmh->flags |= XUrgencyHint;
		XSetWMHints(display, c->w.xid, wmh);
		XFree(wmh);
	}
}

/* X11 stuff */
void
update_class(Client *c) {

	snprint(c->props, sizeof c->props, "%s:%s", c->class, c->name);
}

static void
client_updatename(Client *c) {
	char *str;

	c->name[0] = '\0';
	if((str = windowname(&c->w))) {
		utflcpy(c->name, str, sizeof c->name);
		free(str);
	}

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
	 *
	 *   We support MWM hints deemed non-stupid
	 *
	 * Our definition of non-stupid is a bit less lenient than
	 * theirs, though. In fact, we don't really even support the
	 * idea of supporting the hints that we support, but apps
	 * like xmms (which no one should use) break if we don't.
	 */

	n = getprop_ulong(&c->w, "_MOTIF_WM_HINTS", "_MOTIF_WM_HINTS",
			0L, &ret, 3L);

	/* FIXME: Should somehow handle all frames of a client. */
	if(c->sel)
		r = client_grav(c, ZR);

	c->borderless = 0;
	c->titleless = 0;
	if(n >= 3 && (ret[Flags] & FlagDecor)) {
		if(ret[Decor] & All)
			ret[Decor] ^= ~0;
		c->borderless = !(ret[Decor] & Border);
		c->titleless = !(ret[Decor] & Title);
	}
	free(ret);

	if(false && c->sel) {
		c->sel->floatr = client_grav(c, r);
		if(c->sel->area->floating) {
			client_resize(c, c->sel->floatr);
			frame_draw(c->sel);
		}
	}
}

bool
client_prop(Client *c, Atom a) {
	WinHints h;
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
		return true;
	case XA_WM_TRANSIENT_FOR:
		XGetTransientForHint(display, c->w.xid, &c->trans);
		break;
	case XA_WM_NORMAL_HINTS:
		memset(&h, 0, sizeof h);
		if(c->w.hints)
			bcopy(c->w.hints, &h, sizeof h);
		gethints(&c->w);
		if(c->w.hints)
			c->fixedsize = eqpt(c->w.hints->min, c->w.hints->max);
		if(memcmp(&h, c->w.hints, sizeof h))
			if(c->sel)
				view_update(c->sel->view);
		break;
	case XA_WM_HINTS:
		wmh = XGetWMHints(display, c->w.xid);
		if(wmh) {
			c->noinput = (wmh->flags&InputFocus) && !wmh->input;
			client_seturgent(c, (wmh->flags & XUrgencyHint) != 0, UrgClient);
			XFree(wmh);
		}
		break;
	case XA_WM_CLASS:
		n = getprop_textlist(&c->w, "WM_CLASS", &class);
		snprint(c->class, sizeof c->class, "%s:%s",
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
	return false;
}

/* Handlers */
static bool
configreq_event(Window *w, void *aux, XConfigureRequestEvent *e) {
	Rectangle r;
	Client *c;

	c = aux;

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
	r = client_grav(c, r);

	if(c->sel->area->floating)
		client_resize(c, r);
	else {
		c->sel->floatr = r;
		client_configure(c);
	}
	return false;
}

static bool
destroy_event(Window *w, void *aux, XDestroyWindowEvent *e) {
	USED(w, e);

	client_destroy(aux);
	return false;
}

static bool
enter_event(Window *w, void *aux, XCrossingEvent *e) {
	Client *c;

	c = aux;
	if(e->detail != NotifyInferior) {
		if(e->detail != NotifyVirtual)
		if(e->serial > event_lastconfigure && disp.focus != c) {
			Dprint(DFocus, "enter_notify([%#C]%s)\n", c, c->name);
			focus(c, false);
		}
		client_setcursor(c, cursor[CurNormal]);
	}else
		Dprint(DFocus, "enter_notify(%#C[NotifyInferior]%s)\n", c, c->name);
	return false;
}

static bool
focusin_event(Window *w, void *aux, XFocusChangeEvent *e) {
	Client *c, *old;

	c = aux;

	print_focus("focusin_event", c, c->name);

	if(e->mode == NotifyGrab)
		disp.hasgrab = c;

	old = disp.focus;
	disp.focus = c;
	if(c != old) {
		event("ClientFocus %#C\n", c);
		if(c->sel)
			frame_draw(c->sel);
	}
	return false;
}

static bool
focusout_event(Window *w, void *aux, XFocusChangeEvent *e) {
	Client *c;

	c = aux;
	if((e->mode == NotifyWhileGrabbed) && (disp.hasgrab != &c_root)) {
		if(disp.focus)
			disp.hasgrab = disp.focus;
	}else if(disp.focus == c) {
		print_focus("focusout_event", &c_magic, "<magic>");
		disp.focus = &c_magic;
		if(c->sel)
			frame_draw(c->sel);
	}
	return false;
}

static bool
unmap_event(Window *w, void *aux, XUnmapEvent *e) {
	Client *c;

	c = aux;
	if(!e->send_event && w->parent != c->framewin)
		c->w.unmapped++;
	if(e->send_event || c->w.unmapped < 0)
		client_destroy(c);
	return false;
}

static bool
map_event(Window *w, void *aux, XMapEvent *e) {
	Client *c;

	USED(e);

	c = aux;
	if(c == selclient())
		client_focus(c);
	return true;
}

static bool
property_event(Window *w, void *aux, XPropertyEvent *e) {

	if(e->state == PropertyDelete) /* FIXME */
		return true;
	return client_prop(aux, e->atom);
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
				Dprint(DGeneric, "%#C %p %R %R %R %C\n", c, c->sel, c->r, f->floatr, c->sel ? c->sel->floatr : ZR, c);
				if(f->view == selview || !c->sel)
					c->sel = f;
				kludge = c; /* FIXME */
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
	if(c->sel)
		frame_draw(c->sel);
}

static int
bsstrcmp(const void *a, const void *b) {
	return strcmp((char*)a, *(char**)b);
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

char*
client_extratags(Client *c) {
	Fmt fmt;
	Frame *f;
	char *toks[32];
	char **tags;
	int i;

	i = 0;
	toks[i++] = "";
	for(f=c->frame; f && i < nelem(toks)-1; f=f->cnext)
		if(f != c->sel)
			toks[i++] = f->view->name;
	toks[i] = nil;
	tags = comm(CLeft, toks, c->retags);

	if(i == 1 && !c->tagre.regex && !c->tagvre.regex) {
		free(tags);
		return nil;
	}

	fmtstrinit(&fmt);
	if(i > 1)
		join(tags, "+", &fmt);
	free(tags);

	if(c->tagre.regex)
		fmtprint(&fmt, "+/%s/", c->tagre.regex);
	if(c->tagvre.regex)
		fmtprint(&fmt, "-/%s/", c->tagvre.regex);
	return fmtstrflush(&fmt);
}

bool
client_applytags(Client *c, const char *tags) {
	Fmt fmt;
	uint i, j, k;
	char buf[512];
	char *toks[32];
	char **p;
	char *cur, *s;
	int add, old;

	buf[0] = 0;
	if(memchr("+-^", tags[0], 4))
		utflcpy(buf, c->tags, sizeof c->tags);
	else {
		refree(&c->tagre);
		refree(&c->tagvre);
	}
	strlcat(buf, tags, sizeof buf);

	j = 0;
	s = buf;
	old = '+';
	while((cur = mask(&s, &add, &old))) {
		/* Check for regex. */
		if(cur[0] == '/') {
			cur++;
			*strrchr(cur, '/') = '\0';
			if(add == '+')
				reinit(&c->tagre, cur);
			else if(add == '-')
				reinit(&c->tagvre, cur);
		}
		else if(!strcmp(cur, "~"))
			c->floating = add ? On : Never;
		else {
			trim(cur, " \t\r\n");
			if(!strcmp(cur, "sel"))
				cur = selview->name;
			else if(Mbsearch(cur, badtags, bsstrcmp))
				continue;

			if(j < nelem(toks)-1) {
				if(add == '^')
					add = bsearch(cur, toks, j, sizeof *toks, bsstrcmp) ? '-' : '+';
				if(add == '+')
					toks[j++] = cur;
				else {
					for(i = 0, k = 0; i < j; i++)
						if(strcmp(toks[i], cur))
							toks[k++] = toks[i];
					j = k;
				}
			}
		}
	}

	toks[j] = nil;
	qsort(toks, j, sizeof *toks, strpcmp);
	uniq(toks);

	fmtstrinit(&fmt);
	join(toks, "+", &fmt);
	if(c->tagre.regex)
		fmtprint(&fmt, "+/%s/", c->tagre.regex);
	if(c->tagvre.regex)
		fmtprint(&fmt, "-/%s/", c->tagvre.regex);

	s = fmtstrflush(&fmt);
	utflcpy(c->tags, s, sizeof c->tags);
	changeprop_string(&c->w, "_WMII_TAGS", c->tags);
	free(s);

	free(c->retags);
	p = view_names();
	grep(p, c->tagre.regc, 0);
	grep(p, c->tagvre.regc, GInvert);
	c->retags = comm(CRight, toks, p);
	free(p);

	if(c->retags[0] == nil && toks[0] == nil) {
		toks[0] = "orphans";
		toks[1] = nil;
	}

	p = comm(~0, c->retags, toks);
	client_setviews(c, p);
	free(p);
	return true;
}

