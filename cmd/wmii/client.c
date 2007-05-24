/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xatom.h>
#include <util.h>
#include "dat.h"
#include "fns.h"

#define Mbsearch(k, l, cmp) bsearch(k, l, nelem(l), sizeof(*l), cmp)

static Handlers handlers;

Rectangle gravclient(Client*, Rectangle);

enum {
	ClientMask =
		  StructureNotifyMask
		| PropertyChangeMask
		| EnterWindowMask
		| FocusChangeMask,
	ButtonMask =
		  ButtonPressMask
		| ButtonReleaseMask
};

Client *
create_client(XWindow w, XWindowAttributes *wa) {
	Client **t, *c;
	WinAttr fwa;

	c = emallocz(sizeof(Client));
	c->border = wa->border_width;

	c->r.min = Pt(wa->x, wa->y);
	c->r.max = addpt(c->r.min, Pt(wa->width, wa->height));

	c->w.type = WWindow;
	c->w.w = w;
	c->w.r = c->r;

	prop_client(c, xatom("WM_PROTOCOLS"));
	prop_client(c, xatom("WM_TRANSIENT_FOR"));
	prop_client(c, xatom("WM_NORMAL_HINTS"));
	prop_client(c, xatom("WM_HINTS"));
	prop_client(c, xatom("WM_CLASS"));
	prop_client(c, xatom("WM_NAME"));
	prop_client(c, xatom("_MOTIF_WM_HINTS"));

	XSetWindowBorderWidth(display, w, 0);
	XAddToSaveSet(display, w);
	XSelectInput(display, c->w.w, ClientMask);

	fwa.override_redirect = True;
	fwa.background_pixmap = None;
	fwa.event_mask =
			  SubstructureRedirectMask
			| SubstructureNotifyMask
			| ExposureMask
			| EnterWindowMask
			| PointerMotionMask
			| ButtonPressMask
			| ButtonReleaseMask;
	c->framewin = createwindow(&scr.root, c->r, scr.depth, InputOutput, &fwa,
			  CWOverrideRedirect
			| CWEventMask
			| CWBackPixmap);
	c->framewin->aux = c;
	c->w.aux = c;
	sethandler(c->framewin, &framehandler);
	sethandler(&c->w, &handlers);

	grab_button(c->framewin->w, AnyButton, AnyModifier);

	for(t=&client ;; t=&(*t)->next)
		if(!*t) {
			c->next = *t;
			*t = c;
			break;
		}

	write_event("CreateClient 0x%x\n", c->w.w);
	manage_client(c);
	return c;
}

void
manage_client(Client *c) {
	Rectangle r;
	Point p;
	Client *trans;
	char *tags;

	tags = gettextproperty(&c->w, "_WIN_TAGS");

	if((trans = win2client(c->trans)))
		strncpy(c->tags, trans->tags, sizeof(c->tags));
	else if(tags)
		strncpy(c->tags, tags, sizeof(c->tags));

	free(tags);

	r = c->w.r;
	p.x = def.border;
	p.y = labelh(def.font);
	reparentwindow(&c->w, c->framewin, p);

	if(!strlen(c->tags))
		apply_rules(c);
	else
		apply_tags(c, c->tags);

	if(c->w.hints->position || starting) {
		r = gravclient(c, r);
		if(c->sel->area->floating)
			resize_client(c, &r);
		else
			c->sel->revert = r;
	}

	if(!starting)
		update_views();

	if(c->sel->view == screen->sel)
		focus(c, True);
	flushevents(EnterWindowMask, False);
}

static int
ignoreerrors(Display *d, XErrorEvent *e) {
	return 0;
}

void
destroy_client(Client *c) {
	int (*handler)(Display*, XErrorEvent*);
	Rectangle r;
	char *dummy;
	Client **tc;
	Bool hide;

	Debug fprintf(stderr, "client.c:destroy_client(%p) %s\n", c, c->name);

	for(tc=&client; *tc; tc=&(*tc)->next)
		if(*tc == c) {
			*tc = c->next;
			break;
		}

	r = c->w.r;
	if(c->sel) {
		r = gravclient(c, ZR);
		r = frame2client(c->sel, r);
	}

	hide = False;	
	if(!c->sel || c->sel->view != screen->sel)
		hide = True;

	XGrabServer(display);

	/* In case the client is already unmapped */
	handler = XSetErrorHandler(ignoreerrors);

	dummy = nil;
	update_client_views(c, &dummy);
	unmap_client(c, IconicState);
	sethandler(&c->w, nil);

	if(hide)
		reparentwindow(&c->w, &scr.root, screen->r.max);
	else
		reparentwindow(&c->w, &scr.root, r.min);
	destroywindow(c->framewin);

	XSync(display, False);
	XSetErrorHandler(handler);
	XUngrabServer(display);

	write_event("DestroyClient 0x%x\n", clientwin(c));

	flushevents(EnterWindowMask, False);
	free(c);
}

/* Convenience functions */
Client *
selclient(void) {
	if(screen->sel->sel->sel)
		return screen->sel->sel->sel->client;
	return nil;
}

Client *
win2client(XWindow w) {
	Client *c;
	for(c=client; c; c=c->next)
		if(c->w.w == w) break;
	return c;
}

uint
clientwin(Client *c) {
	if(c)
		return (uint)c->w.w;
	return 0;
}

char *
clientname(Client *c) {
	if(c)
		return c->name;
	return "<nil>";
}

Rectangle
gravclient(Client *c, Rectangle rd) {
	Rectangle r;
	Point sp;
	WinHints *h;

	h = c->w.hints;
	r = client2frame(c->sel, c->w.r);
	sp = Pt(def.border, labelh(def.font));

	if(eqrect(rd, ZR)) {
		if(c->sel->area->floating)
			r = c->sel->r;
		else
			r = c->sel->revert;
		r = gravitate(r, c->w.r, h->grav);
		if(h->gravstatic)
			r = rectaddpt(r, sp);
	}else {
		r = gravitate(rd, r, h->grav);
		if(h->gravstatic)
			r = rectsubpt(r, sp);
	}
	return r;
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
	r = frame2client(f, r);
	r = sizehint(c->w.hints, r);
	r = client2frame(f, r);

	if(!f->area->floating) {
		/* Not allowed to grow */
		if(Dx(r) > Dx(or))
			r.max.x =r.min.x+Dx(or);
		if(Dy(r) > Dy(or))
			r.max.y = r.min.y+Dy(or);
	}

	p = ZP;
	if((sticky&(EAST|WEST)) == EAST)
		p.x = Dx(or) - Dx(r);
	if((sticky&(NORTH|SOUTH)) == SOUTH)
		p.y = Dy(or) - Dy(r);

	return rectaddpt(r, p);
}

static void
set_client_state(Client * c, int state) {
	long data[] = { state, None };
	changeprop(&c->w, "WM_STATE", "WM_STATE", data, nelem(data));
}

void
map_client(Client *c) {
	if(!c->w.mapped) {
		mapwin(&c->w);
		set_client_state(c, NormalState);
	}
}

void
unmap_client(Client *c, int state) {
	if(c->w.mapped) {
		unmapwin(&c->w);
		set_client_state(c, state);
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
focus(Client *c, Bool restack) {
	View *v;
	Frame *f;

	f = c->sel;
	if(!f)
		return;

	v = f->area->view;
	if(v != screen->sel)
		focus_view(screen, v);
	focus_frame(c->sel, restack);
}

void
focus_client(Client *c) {
	flushevents(FocusChangeMask, True);

	Debug fprintf(stderr, "focus_client(%p[%x]) => %s\n", c,  clientwin(c), clientname(c));

	if((c == nil || !c->noinput) && screen->focus != c) {
		Debug fprintf(stderr, "\t%s => %s\n", clientname(screen->focus), clientname(c));

		if(c)
			setfocus(&c->w, RevertToParent);
		else
			setfocus(screen->barwin, RevertToParent);

		XSync(display, False);
		flushevents(FocusChangeMask, True);
	}
}

void
resize_client(Client *c, Rectangle *r) {
	Frame *f;

	f = c->sel;
	resize_frame(f, *r);

	if(f->area->view != screen->sel) {
		unmap_client(c, IconicState);
		unmap_frame(c);
		return;
	}

	c->r = rectaddpt(f->crect, f->r.min);

	if((f->area->mode == Colmax) && (f->area->sel != f)) {
		unmap_frame(c);
		unmap_client(c, IconicState);
	}else if(f->collapsed) {
		reshapewin(c->framewin, f->r);
		map_frame(c);
		unmap_client(c, IconicState);
	}else {
		map_client(c);
		reshapewin(c->framewin, f->r);
		reshapewin(&c->w, f->crect);
		map_frame(c);
		configure_client(c);
	}

	flushevents(FocusChangeMask|ExposureMask, True);
}

void
set_cursor(Client *c, Cursor cur) {
	WinAttr wa;

	if(c->cursor != cur) {
		c->cursor = cur;
		wa.cursor = cur;
		setwinattr(c->framewin, &wa, CWCursor);
	}
}

void
configure_client(Client *c) {
	XConfigureEvent e;
	Rectangle r;
	Frame *f;

	f = c->sel;
	if(!f)
		return;

	r = rectaddpt(f->crect, f->r.min);
	r = insetrect(r, -c->border);

	e.type = ConfigureNotify;
	e.event = c->w.w;
	e.window = c->w.w;
	e.x = r.min.x;
	e.y = r.min.y;
	e.width = Dx(r);
	e.height = Dy(r);
	e.border_width = c->border;
	e.above = None;
	e.override_redirect = False;
	XSendEvent(display, c->w.w, False, StructureNotifyMask, (XEvent*)&e);
}

static void
send_client_message(Client *c, char *name, char *value) {
	XEvent e;

	e.type = ClientMessage;
	e.xclient.window = c->w.w;
	e.xclient.message_type = xatom(name);
	e.xclient.format = 32;
	e.xclient.data.l[0] = xatom(value);
	e.xclient.data.l[1] = CurrentTime;
	XSendEvent(display, c->w.w, False, NoEventMask, &e);
	XSync(display, False);
}

void
kill_client(Client * c) {
	if(c->proto & WM_PROTOCOL_DELWIN)
		send_client_message(c, "WM_PROTOCOLS", "WM_DELETE_WINDOW");
	else
		XKillClient(display, c->w.w);
}

void
set_urgent(Client *c, Bool urgent, Bool write) {
	XWMHints *wmh;
	char *cwrite, *cnot;
	Frame *f, *ff;
	Area *a;

	cwrite = (write ? "Manager" : "Client");
	cnot = (urgent ? "" : "Not");

	if(urgent != c->urgent) {
		write_event("%sUrgent 0x%x %s\n", cnot, clientwin(c), cwrite);
		c->urgent = urgent;
		if(c->sel) {
			if(c->sel->view == screen->sel)
				draw_frame(c->sel);
			for(f=c->frame; f; f=f->cnext) {
				if(!urgent)
					for(a=f->view->area; a; a=a->next)
						for(ff=a->frame; ff; ff=ff->anext)
							if(ff->client->urgent) break;
				if(urgent || ff == nil)
					write_event("%sUrgentTag %s %s\n", cnot, cwrite, f->view->name);
			}
		}
	}

	if(write) {
		wmh = XGetWMHints(display, c->w.w);
		if(wmh) {
			wmh->flags &= ~XUrgencyHint;
			if(urgent)
				wmh->flags |= XUrgencyHint;
			XSetWMHints(display, c->w.w, wmh);
			XFree(wmh);
		}
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
	utfecpy(str+1, c->props+sizeof(c->props), c->name);
}

static void
update_client_name(Client *c) {
	char *str;

	c->name[0] = '\0';

	str = gettextproperty(&c->w, "_NET_WM_NAME)");
	if(str == nil)
		str = gettextproperty(&c->w, "WM_NAME");
	if(str)
		utfecpy(c->name, c->name+sizeof(c->name), str);
	free(str);

	update_class(c);
	if(c->sel)
		draw_frame(c->sel);
}

static void
updatemwm(Client *c) {
	enum {
		All =		0x1,
		Border =	0x2,
		Title =	0x8,
	};
	Rectangle r;
	ulong *ret, decor;
	Atom real;
	int n;

	n = getproperty(&c->w, "_MOTIF_WM_HINTS", "_MOTIF_WM_HINTS", &real, 
				2L, (uchar**)&ret, 1L);

	if(n == 0) {
		c->borderless = 0;
		c->titleless = 0;
	}else {
		decor = *ret;
		free(ret);

		if(c->sel)
			r = frame2client(c->sel, c->sel->r);

		if(decor&All)
			decor ^= ~0;
		c->borderless = ((decor&Border)==0);
		c->titleless = ((decor&Title)==0);

		if(c->sel) {
			r = client2frame(c->sel, c->sel->r);
			resize_client(c, &r);
			draw_frame(c->sel);
		}
	}
}

void
prop_client(Client *c, Atom a) {
	XWMHints *wmh;
	char **class;
	int n;

	if(a == xatom("WM_PROTOCOLS")) {
		c->proto = winprotocols(&c->w);
	}
	else if(a == xatom("_NET_WM_NAME")) {
		goto wmname;
	}
	else if(a == xatom("_MOTIF_WM_HINTS")) {
		updatemwm(c);
	}else switch (a) {
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
			set_urgent(c, (wmh->flags & XUrgencyHint) != 0, False);
			XFree(wmh);
		}
		break;
	case XA_WM_CLASS:
		n = gettextlistproperty(&c->w, "WM_CLASS", &class);
		snprintf(c->props, sizeof(c->props), "%s:%s:",
				(n > 0 ? class[0] : "<nil>"),
				(n > 1 ? class[1] : "<nil>"));
		freestringlist(class);
		update_class(c);
		break;
	case XA_WM_NAME:
wmname:
		update_client_name(c);
		break;
	}
}

/* Handlers */
static void
configreq_event(Window *w, XConfigureRequestEvent *e) {
	Rectangle r;
	Point p;
	Frame *f;
	Client *c;

	c = w->aux;
	f = c->sel;

	p = ZP;
	r = gravclient(c, ZR);
	if(e->value_mask&CWX)
		p.x = e->x - r.min.x;
	if(e->value_mask&CWY)
		p.y = e->y - r.min.y;
	if(e->value_mask&CWWidth)
		r.max.x = r.min.x + e->width;
	if(e->value_mask&CWHeight)
		r.max.y = r.min.y + e->height;
	if(e->value_mask&CWBorderWidth)
		c->border = e->border_width;
	r = rectaddpt(r, p);
	r = gravclient(c, r);

	if((Dx(r) == Dx(screen->r))
	&& (Dy(r) == Dy(screen->r))) {
		c->fullscreen = True;
		if(f) {
			if(!f->area->floating)
				send_to_area(f->view->area, f);
			focus_client(c);
			restack_view(f->view);
		}
	}

	if(c->sel->area->floating)
		resize_client(c, &r);
	else {
		c->sel->revert = r;
		configure_client(c);
	}
}

static void
destroy_event(Window *w, XDestroyWindowEvent *e) {
	Debug fprintf(stderr, "client.c:destroy_event(%x)\n", (uint)w->w);
	destroy_client(w->aux);
}

static void
enter_event(Window *w, XCrossingEvent *e) {
	Client *c;
	
	c = w->aux;
	if(e->detail != NotifyInferior) {
		if(screen->focus != c) {
			Debug fprintf(stderr, "enter_notify(c) => %s\n", c->name);
			focus(c, False);
		}
		set_cursor(c, cursor[CurNormal]);
	}else Debug
		fprintf(stderr, "enter_notify(c[NotifyInferior]) => %s\n", c->name);
}

static void
focusin_event(Window *w, XFocusChangeEvent *e) {
	Client *c, *old;

	c = w->aux;

	print_focus(c, c->name);

	if(e->mode == NotifyGrab)
		screen->hasgrab = c;

	old = screen->focus;
	screen->focus = c;
	if(c != old) {
		if(c->sel)
			draw_frame(c->sel);
	}
}

static void
focusout_event(Window *w, XFocusChangeEvent *e) {
	Client *c;

	c = w->aux;
	if((e->mode == NotifyWhileGrabbed) && (screen->hasgrab != &c_root)) {
		if((screen->focus) && (screen->hasgrab != screen->focus))
			screen->hasgrab = screen->focus;
		if(screen->hasgrab == c)
			return;
	}else if(screen->focus == c) {
		print_focus(&c_magic, "<magic>");
		screen->focus = &c_magic;
		if(c->sel)
			draw_frame(c->sel);
	}
}

static void
unmap_event(Window *w, XUnmapEvent *e) {
	Client *c;
	
	c = w->aux;
	if(!e->send_event)
		c->unmapped--;
	destroy_client(c);
}

static void
map_event(Window *w, XMapEvent *e) {
	Client *c;
	
	c = w->aux;
	if(c == selclient())
		focus_client(c);
}

static void
property_event(Window *w, XPropertyEvent *e) {
	Client *c;

	if(e->state == PropertyDelete)
		return;

	c = w->aux;
	prop_client(c, e->atom);
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
#if 0 /* Not used at the moment */
void
newcol_client(Client *c, char *arg) {
	Frame *f;
	Area *to, *a;
	View *v;

	f = c->sel;
	a = f->area;
	v = f->view;

	if(a->floating)
		return;
	if((f->anext == nil) && (f->aprev == nil))
		return;

	if(!strncmp(arg, "prev", 5)) {
		for(to=v->area; to; to=to->next)
			if(to->next == a) break;
		to = new_column(v, to, 0);
		send_to_area(to, f);
	}
	else if(!strncmp(arg, "next", 5)) {
		to = new_column(v, a, 0);
		send_to_area(to, f);
	}
	else
		return;
	flushevents(EnterWindowMask, False);
}
#endif

void
update_client_views(Client *c, char **tags) {
	Frame **fp, *f;
	int cmp;

	fp = &c->frame;
	while(*fp || *tags) {
		while(*fp) {
			if(*tags) {
				cmp = strcmp((*fp)->view->name, *tags);
				if(cmp >= 0)
					break;
			}

			f = *fp;
			detach_from_area(f);
			*fp = f->cnext;
			if(c->sel == f)
				c->sel = *fp;
			free(f);
		}
		if(*tags) {
			if(!*fp || cmp > 0) {
				f = create_frame(c, get_view(*tags));
				if(f->view == screen->sel || !c->sel)
					c->sel = f;
				attach_to_view(f->view, f);
				f->cnext = *fp;
				*fp = f;
			}
			if(*fp) fp=&(*fp)->cnext;
			tags++;
		}
	}
	update_views();
}

static int
bsstrcmp(const void *a, const void *b) {
	return strcmp((char*)a, (char*)b);
}

static int
strpcmp(const void *a, const void *b) {
	return strcmp(*(char **)a, *(char **)b);
}

static char *badtags[] = {
	".",
	"..",
	"sel",
};

void
apply_tags(Client *c, const char *tags) {
	uint i, j, k, n;
	Bool add;
	char buf[512], last;
	char *toks[32], *cur;

	buf[0] = 0;

	for(n = 0; tags[n]; n++)
		if(!isspace(tags[n]))
			break;

	if(tags[n] == '+' || tags[n] == '-')
		strncpy(buf, c->tags, sizeof(c->tags));

	strlcat(buf, &tags[n], sizeof(buf));
	trim(buf, " \t/");

	n = 0;
	add = True;
	if(buf[0] == '+')
		n++;
	else if(buf[0] == '-') {
		n++;
		add = False;
	}

	j = 0;
	while(buf[n] && n < sizeof(buf) && j < 32) { 
		for(i = n; i < sizeof(buf) - 1; i++)
			if(buf[i] == '+' || buf[i] == '-' || buf[i] == '\0')
				break;
		last = buf[i];
		buf[i] = '\0';

		cur = nil;
		if(!strcmp(buf+n, "~"))
			c->floating = add;
		else if(!strcmp(buf+n, "!"))
			cur = screen->sel->name;
		else if(!Mbsearch(buf+n, badtags, bsstrcmp))
			cur = buf+n;

		n = i + 1;
		if(cur) {
			if(add)
				toks[j++] = cur;
			else {
				for(i = 0, k = 0; i < j; i++)
					if(strcmp(toks[i], cur))
						toks[k++] = toks[i];
				j = k;
			}
		}

		switch(last) {
		case '+':
			add = True;
			break;
		case '-':
			add = False;
			break;
		case '\0':
			buf[n] = '\0';
			break;
		}
	}

	if(!j)
		return;

	qsort(toks, j, sizeof(char *), strpcmp);

	c->tags[0] = '\0';
	for(i=0, n=0; i < j; i++)
		if(n == 0 || strcmp(toks[i], toks[n-1])) {
			if(i > 0)
				strlcat(c->tags, "+", sizeof(c->tags));
			strlcat(c->tags, toks[i], sizeof(c->tags));
			toks[n++] = toks[i];
		}
	toks[n] = nil;

	update_client_views(c, toks);

	changeprop(&c->w, "_WIN_TAGS", "UTF8_STRING", c->tags, strlen(c->tags));
}

void
apply_rules(Client *c) {
	Rule *r;
	regmatch_t rm;

	if(strlen(c->tags))
		return;

	if(def.tagrules.string) 	
		for(r=def.tagrules.rule; r; r=r->next)
			if(!regexec(&r->regex, c->props, 1, &rm, 0)) {
				apply_tags(c, r->value);
				if(strcmp(c->tags, "nil"))
					break;
			}
	if(c->tags[0] == '\0')
		apply_tags(c, "nil");
}
