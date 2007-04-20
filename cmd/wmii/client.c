/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xatom.h>
#include <util.h>
#include "dat.h"
#include "fns.h"

static void update_client_name(Client *c);
static Handlers handlers;

static char Ebadcmd[] = "bad command",
		Ebadvalue[] = "bad value";

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

	c->proto = winprotocols(&c->w);
	prop_client(c, XA_WM_TRANSIENT_FOR);
	prop_client(c, XA_WM_NORMAL_HINTS);
	prop_client(c, XA_WM_HINTS);
	prop_client(c, XA_WM_NAME);

	XSetWindowBorderWidth(display, w, 0);
	XAddToSaveSet(display, w);
	XSelectInput(display, c->w.w, ClientMask);

	fwa.override_redirect = True;
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
			| CWEventMask);
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
	return c;
}

static int
ignoreerrors(Display *d, XErrorEvent *e) {
	return 0;
}

Rectangle
gravclient(Client *c, Rectangle rd) {
	Rectangle r;
	Point sp;
	WinHints *h;

	h = c->w.hints;
	r = client2frame(nil, c->w.r);
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

void
destroy_client(Client *c) {
	int (*handler)(Display*, XErrorEvent*);
	Rectangle r;
	char *dummy;
	Client **tc;
	XEvent ev;

	if(verbose) fprintf(stderr, "client.c:destroy_client(%p) %s\n", c, c->name);

	for(tc=&client; *tc; tc=&(*tc)->next)
		if(*tc == c) {
			*tc = c->next;
			break;
		}

	XGrabServer(display);

	/* In case the client is already unmapped */
	handler = XSetErrorHandler(ignoreerrors);

	r = gravclient(c, ZR);
	r = frame2client(nil, r);

	dummy = nil;
	update_client_views(c, &dummy);

	unmap_client(c, WithdrawnState);
	reparent_client(c, &scr.root, r.min);

	write_event("DestroyClient 0x%x\n", (uint)c->w.w);

	destroywindow(c->framewin);
	sethandler(&c->w, nil);

	XSync(display, False);
	XSetErrorHandler(handler);

	XUngrabServer(display);
	flushevents(EnterWindowMask, False);

	while(XCheckMaskEvent(display, StructureNotifyMask, &ev))
		if(ev.type != UnmapNotify || ev.xunmap.window != c->w.w)
			dispatch_event(&ev);

	free(c);
}

void
manage_client(Client *c) {
	XTextProperty tags = { 0 };
	Client *trans;

	XGetTextProperty(display, c->w.w, &tags, atom[TagsAtom]);

	if((trans = win2client(c->trans)))
		strncpy(c->tags, trans->tags, sizeof(c->tags));
	else if(tags.nitems)
		strncpy(c->tags, (char *)tags.value, sizeof(c->tags));
	XFree(tags.value);

	gravclient(c, c->w.r);
	reparent_client(c, c->framewin, Pt(def.border, labelh(def.font)));

	if(!strlen(c->tags))
		apply_rules(c);
	else
		apply_tags(c, c->tags);

	if(!starting)
		update_views();
	XSync(display, False);

	if(c->sel->view == screen->sel)
		focus(c, True);
	flushevents(EnterWindowMask, False);
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

	if((Dx(r) == Dx(screen->r)) && (Dy(r) == Dy(screen->r))) {
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
	if(verbose) fprintf(stderr, "client.c:destroy_event(%x)\n", (uint)w->w);
	destroy_client(w->aux);
}

static void
enter_event(Window *w, XCrossingEvent *e) {
	Client *c;
	
	c = w->aux;
	if(e->detail != NotifyInferior) {
		if(screen->focus != c) {
			if(verbose) fprintf(stderr, "enter_notify(c) => %s\n", c->name);
			focus(c, False);
		}
		set_cursor(c, cursor[CurNormal]);
	}else if(verbose)
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
		if(old && old->sel)
			draw_frame(old->sel);
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
	}else if(e->mode != NotifyGrab) {
		if(screen->focus == c) {
			print_focus(&c_magic, "<magic>");
			screen->focus = &c_magic;
		}
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

Client *
selclient() {
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

static void
update_client_name(Client *c) {
	XClassHint ch = {0};
	char *str;

	c->name[0] = '0';

	str = gettextproperty(&c->w, atom[NetWMName]);
	if(str == nil)
		str = gettextproperty(&c->w, XA_WM_NAME);
	if(str)
		utfecpy(c->name, c->name+sizeof(c->name), str);
	free(str);

	XGetClassHint(display, c->w.w, &ch);
	snprintf(c->props, sizeof(c->props), "%s:%s:%s",
			str_nil(ch.res_class),
			str_nil(ch.res_name),
			c->name);
	if(ch.res_class)
		XFree(ch.res_class);
	if(ch.res_name)
		XFree(ch.res_name);
}

void
set_client_state(Client * c, int state) {
	long data[] = { state, None };
	changeproperty(&c->w, atom[WMState], atom[WMState], 32, (uchar*)data, nelem(data));
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
reparent_client(Client *c, Window *w, Point pt) {
	reparentwindow(&c->w, w, pt);
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
send_client_message(XWindow w, Atom a, long value) {
	XEvent e;

	e.type = ClientMessage;
	e.xclient.window = w;
	e.xclient.message_type = a;
	e.xclient.format = 32;
	e.xclient.data.l[0] = value;
	e.xclient.data.l[1] = CurrentTime;
	XSendEvent(display, w, False, NoEventMask, &e);
	XSync(display, False);
}

void
kill_client(Client * c) {
	if(c->proto & WM_PROTOCOL_DELWIN)
		send_client_message(c->w.w, atom[WMProtocols], atom[WMDelete]);
	else
		XKillClient(display, c->w.w);
}

static void
set_urgent(Client *c, Bool urgent, Bool write) {
	XWMHints *wmh;
	char *cwrite, *cnot;
	Frame *f, *ff;
	Area *a;

	cwrite = "Client";
	if(write)
		cwrite = "Manager";
	cnot = "Not";
	if(urgent)
		cnot = "";

	if(urgent != c->urgent) {
		write_event("%sUrgent 0x%x %s\n", cnot, c->w, cwrite);
		c->urgent = urgent;
		if(c->sel) {
			if(c->sel->view == screen->sel)
				draw_frame(c->sel);
			if(!urgent || c->sel->view != screen->sel)
				for(f=c->frame; f; f=f->cnext) {
					for(a=f->view->area; a; a=a->next)
						for(ff=a->frame; ff; ff=ff->anext)
							if(ff->client->urgent) break;
					if(!ff)
						write_event("%sUrgentTag %s %s\n", cnot, cwrite, f->view->name);
				}
		}
	}

	if(write) {
		wmh = XGetWMHints(display, c->w.w);
		if(wmh) {
			if(urgent)
				wmh->flags |= XUrgencyHint;
			else
				wmh->flags &= ~XUrgencyHint;
			XSetWMHints(display, c->w.w, wmh);
			XFree(wmh);
		}
	}
}

void
prop_client(Client *c, Atom a) {
	XWMHints *wmh;

	if(a ==  atom[WMProtocols])
		c->proto = winprotocols(&c->w);
	else if(a== atom[NetWMName]) {
wmname:
		update_client_name(c);
		if(c->frame)
			draw_frame(c->sel);
	}
	else switch (a) {
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
			set_urgent(c, (wmh->flags & XUrgencyHint) != 0, False);
			XFree(wmh);
		}
		break;
	case XA_WM_NAME:
		goto wmname;
	}
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

	if(verbose)
		fprintf(stderr, "focus_client(%p[%x]) => %s\n", c, 
			(c ? (uint)c->w.w : 0), (c ? c->name : nil));

	if(screen->focus != c) {
		if(verbose)
			fprintf(stderr, "\t%s => %s\n",
					(screen->focus ? screen->focus->name : "<nil>"),
					(c ? c->name : "<nil>"));
		if(c)
			XSetInputFocus(display, c->w.w, RevertToParent, CurrentTime);
		else
			XSetInputFocus(display, screen->barwin->w, RevertToParent, CurrentTime);
		XSync(display, False);
	}

	flushevents(FocusChangeMask, True);
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
		reshapewin(&c->w, f->crect);
		map_client(c);
		reshapewin(c->framewin, f->r);
		map_frame(c);
		configure_client(c);
	}
	
	flushevents(FocusChangeMask|ExposureMask, True);
}

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

char *
send_client(Frame *f, char *arg, Bool swap) {
	Area *to, *a;
	Client *c;
	Frame *tf;
	View *v;
	Bool before;
	int j;

	a = f->area;
	v = a->view;
	c = f->client;

	if(!strncmp(arg, "toggle", 7)) {
		if(!a->floating)
			to = v->area;
		else if(c->revert && !c->revert->floating)
			to = c->revert;
		else
			to = v->area->next;
		goto send_area;
	}else if(!a->floating) {
		if(!strncmp(arg, "left", 5)) {
			if(a->floating)
				return Ebadvalue;
			for(to=v->area->next; to; to=to->next)
				if(a == to->next) break;
			if(!to && !swap && (f->anext || f != a->frame))
				to=new_column(v, v->area, 0);
			goto send_area;
		}
		else if(!strncmp(arg, "right", 5)) {
			if(a->floating)
				return Ebadvalue;
			to = a->next;
			if(!to && !swap && (f->anext || f != a->frame))
				to = new_column(v, a, 0);
			goto send_area;
		}
		else if(!strncmp(arg, "up", 3)) {
			for(tf=a->frame; tf; tf=tf->anext)
				if(tf->anext == f) break;
			before = True;
			goto send_frame;
		}
		else if(!strncmp(arg, "down", 5)) {
			tf = f->anext;
			before = False;
			goto send_frame;
		}
		else {
			if(sscanf(arg, "%d", &j) != 1)
				return Ebadvalue;
			for(to=v->area; to; to=to->next)
				if(!--j) break;
			goto send_area;
		}
	}
	return Ebadvalue;

send_frame:
	if(!tf)
		return Ebadvalue;
	if(!swap) {
		remove_frame(f);
		insert_frame(tf, f, before);
	}else
		swap_frames(f, tf);
	arrange_column(a, False);

	flushevents(EnterWindowMask, False);
	focus_frame(f, True);
	update_views();
	return nil;

send_area:
	if(!to)
		return Ebadvalue;
	if(!swap)
		send_to_area(to, f);
	else if(to->sel)
		swap_frames(f, to->sel);

	flushevents(EnterWindowMask, False);
	focus_frame(f, True);
	update_views();
	return nil;
}

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
compare_tags(const void *a, const void *b) {
	return strcmp(*(char **)a, *(char **)b);
}

void
apply_tags(Client *c, const char *tags) {
	uint i, j, k, n;
	Bool add;
	char buf[512], last;
	char *toks[32], *cur;

	buf[0] = 0;
	for(n = 0; tags[n]; n++)
		if(tags[n] != ' ' && tags[n] != '\t') break;
	if(tags[n] == '+' || tags[n] == '-')
		strncpy(buf, c->tags, sizeof(c->tags));
	strlcat(buf, &tags[n], sizeof(buf));
	trim(buf, " \t/");

	n = 0;
	j = 0;
	add = True;
	if(buf[0] == '+')
		n++;
	else if(buf[0] == '-') {
		n++;
		add = False;
	}
	while(buf[n] && n < sizeof(buf) && j < 32) { 
		for(i = n; i < sizeof(buf) - 1; i++)
			if(buf[i] == '+'
			|| buf[i] == '-'
			|| buf[i] == '\0')
				break;
		last = buf[i];
		buf[i] = '\0';

		cur = nil;
		if(!strncmp(&buf[n], "~", 2))
			c->floating = add;
		else if(!strncmp(&buf[n], "!", 2))
			cur = view ? screen->sel->name : "nil";
		else if(strncmp(&buf[n], "sel", 4)
		     && strncmp(&buf[n], ".", 2)
		     && strncmp(&buf[n], "..", 3))
			cur = &buf[n];

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

	c->tags[0] = '\0';
	if(!j)
		return;
	qsort(toks, j, sizeof(char *), compare_tags);

	for(i=0, n=0; i < j; i++)
		if(!n || strcmp(toks[i], toks[n-1])) {
			if(i)
				strlcat(c->tags, "+", sizeof(c->tags));
			strlcat(c->tags, toks[i], sizeof(c->tags));
			toks[n++] = toks[i];
		}
	toks[n] = nil;

	update_client_views(c, toks);

	changeproperty(&c->w, atom[TagsAtom], atom[Utf8String], 8, (uchar*)c->tags, strlen(c->tags));
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
				if(strlen(c->tags) && strcmp(c->tags, "nil"))
					break;
			}
	if(!strlen(c->tags))
		apply_tags(c, "nil");
}

char *
message_client(Client *c, char *message) {
	if(!strcmp(message, "kill"))
		kill_client(c);
	else if(!strcmp(message, "Urgent"))
		set_urgent(c, True, True);
	else if(!strcmp(message, "NotUrgent"))
		set_urgent(c, False, True);
	else
		return Ebadcmd;
	return nil;
}
