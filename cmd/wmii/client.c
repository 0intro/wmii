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

enum {
	ClientMask =
		  StructureNotifyMask
		| PropertyChangeMask
		| EnterWindowMask
		| FocusChangeMask,
	ButtonMask =
		  ButtonPressMask | ButtonReleaseMask
};

Client *
create_client(XWindow w, XWindowAttributes *wa) {
	Client **t, *c;
	WinAttr fwa;

	c = emallocz(sizeof(Client));
	c->border = wa->border_width;
	c->rect.min = Pt(wa->x, wa->y);
	c->rect.max = addpt(c->rect.min, Pt(wa->width, wa->height));

	c->win.type = WWindow;
	c->win.w = w;

	c->proto = winprotocols(&c->win);
	prop_client(c, XA_WM_TRANSIENT_FOR);
	prop_client(c, XA_WM_NORMAL_HINTS);
	prop_client(c, XA_WM_HINTS);
	prop_client(c, XA_WM_NAME);

	XSetWindowBorderWidth(display, w, 0);
	XAddToSaveSet(display, w);

	fwa.override_redirect = True;
	fwa.background_pixmap = ParentRelative;
	fwa.backing_store = Always;
	fwa.event_mask =
		  SubstructureRedirectMask
		| SubstructureNotifyMask
		| ExposureMask
		| EnterWindowMask
		| PointerMotionMask
		| KeyPressMask
		| ButtonPressMask
		| ButtonReleaseMask;
	c->framewin = createwindow(&scr.root, c->rect, scr.depth, InputOutput, &fwa,
		  CWOverrideRedirect
		| CWEventMask
		| CWBackPixmap
		| CWBackingStore);
	c->framewin->aux = c;
	c->win.aux = c;
	sethandler(c->framewin, &framehandler);
	sethandler(&c->win, &handlers);

	grab_button(c->framewin->w, AnyButton, AnyModifier);

	for(t=&client ;; t=&(*t)->next)
		if(!*t) {
			c->next = *t;
			*t = c;
			break;
		}

	write_event("CreateClient 0x%x\n", c->win);
	return c;
}

static int
dummy_error_handler(Display *dpy, XErrorEvent *error) {
	return 0;
}

void
destroy_client(Client *c) {
	char *dummy;
	Client **tc;
	XEvent ev;

	XGrabServer(display);
	/* In case the client is already unmapped */
	XSetErrorHandler(dummy_error_handler);

	for(tc=&client; *tc; tc=&(*tc)->next)
		if(*tc == c) {
			*tc = c->next;
			break;
		}

	dummy = nil;
	update_client_views(c, &dummy);

	unmap_client(c, WithdrawnState);
	gravitate_client(c, True);
	reparent_client(c, &scr.root, c->rect.min);

	destroywindow(c->framewin);
	sethandler(&c->win, nil);

	XSync(display, False);
	XSetErrorHandler(wmii_error_handler);
	XUngrabServer(display);
	flushevents(EnterWindowMask, False);

	while(XCheckMaskEvent(display, StructureNotifyMask, &ev))
		if(ev.type != UnmapNotify || ev.xunmap.window != c->win.w)
			dispatch_event(&ev);

	write_event("DestroyClient 0x%x\n", c->win);
	free(c);
}

void
manage_client(Client *c) {
	XTextProperty tags = { 0 };
	Client *trans;

	XGetTextProperty(display, c->win.w, &tags, atom[TagsAtom]);

	if((trans = win2client(c->trans)))
		strncpy(c->tags, trans->tags, sizeof(c->tags));
	else if(tags.nitems)
		strncpy(c->tags, (char *)tags.value, sizeof(c->tags));
	XFree(tags.value);

	gravitate_client(c, False);
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
	Rectangle *frect;
	Frame *f;
	Client *c;

	c = w->aux;
	f = c->sel;

	gravitate_client(c, True);
	if(e->value_mask & CWX)
		c->rect.min.x = e->x;
	if(e->value_mask & CWY)
		c->rect.min.y = e->y;
	if(e->value_mask & CWWidth)
		c->rect.max.x = c->rect.min.x + e->width;
	if(e->value_mask & CWHeight)
		c->rect.max.y = c->rect.min.y + e->height;
	if(e->value_mask & CWBorderWidth)
		c->border = e->border_width;
	gravitate_client(c, False);

	if((Dx(c->rect) == Dx(screen->rect))
	&& (Dy(c->rect) == Dy(screen->rect))) {
		c->fullscreen = True;
		if(c->sel) {
			if(!c->sel->area->floating)
				send_to_area(c->sel->view->area, c->sel);
			focus_client(c);
			restack_view(c->sel->view);
		}
	}

	if(c->sel->area->floating)
		frect=&c->sel->rect;
	else
		frect=&c->sel->revert;

	*frect = insetrect(c->rect, -def.border);
	frect->min.y -= labelh(def.font);

	if(c->sel->area->floating || c->fullscreen)
		resize_client(c, frect);
	else
		configure_client(c);
}

static void
destroy_event(Window *w, XDestroyWindowEvent *e) {
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
	}else if(verbose) fprintf(stderr, "enter_notify(c[NotifyInferior]) => %s\n", c->name);
}

static void
focusin_event(Window *w, XFocusChangeEvent *e) {
	Client *c, *old;

	c = w->aux;

	//print_focus(c, c->name);
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
		if((screen->focus)
		&& (screen->hasgrab != screen->focus))
			screen->hasgrab = screen->focus;
		if(screen->hasgrab == c)
			return;
	}else if(e->mode != NotifyGrab) {
		if(screen->focus == c) {
			//print_focus(&c_magic, "<magic>");
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
		if(c->win.w == w) break;
	return c;
}

static void
update_client_name(Client *c) {
	XTextProperty name;
	XClassHint ch = {0};
	int n;
	char **list = nil;

	c->name[0] = 0;

	name.nitems = 0;
	XGetTextProperty(display, c->win.w, &name, atom[NetWMName]);
	if(!name.nitems)
		XGetWMName(display, c->win.w, &name);
	if(!name.nitems)
		return;

	if(name.encoding == XA_STRING)
		strncpy(c->name, (char *)name.value, sizeof(c->name));
	else	if(XmbTextPropertyToTextList(display, &name, &list, &n) >= Success)
		if(n > 0 && *list) {
			strncpy(c->name, *list, sizeof(c->name));
			XFreeStringList(list);
		}
	XFree(name.value);

	XGetClassHint(display, c->win.w, &ch);
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
	XChangeProperty(
		/* display */	display,
		/* parent */	c->win.w,
		/* property */	atom[WMState],
		/* type */	atom[WMState],
		/* format */	32,
		/* mode */	PropModeReplace,
		/* data */	(uchar *) data,
		/* npositions */2
		);
}

void
map_client(Client *c) {
	if(!c->mapped) {
		XSelectInput(display, c->win.w, ClientMask & ~StructureNotifyMask);
		XMapWindow(display, c->win.w);
		XSelectInput(display, c->win.w, ClientMask);
		set_client_state(c, NormalState);
		c->mapped = 1;
	}
}

void
unmap_client(Client *c, int state) {
	if(c->mapped) {
		c->unmapped++;
		XSelectInput(display, c->win.w, ClientMask & ~StructureNotifyMask);
		XUnmapWindow(display, c->win.w);
		XSelectInput(display, c->win.w, ClientMask);
		set_client_state(c, state);
		c->mapped = 0;
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
	XSelectInput(display, c->win.w, ClientMask & ~StructureNotifyMask);
	XReparentWindow(display, c->win.w, w->w, pt.x, pt.y);
	XSelectInput(display, c->win.w, ClientMask);
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
	
	r = rectaddpt(f->crect, f->rect.min);
	r = insetrect(r, -c->border);

	e.type = ConfigureNotify;
	e.event = c->win.w;
	e.window = c->win.w;
	e.x = r.min.x;
	e.y = r.max.x;
	e.width = Dx(r);
	e.height = Dy(r);
	e.border_width = c->border;
	e.above = None;
	e.override_redirect = False;
	XSendEvent(display, c->win.w, False,
			StructureNotifyMask, (XEvent *) & e);
	XSync(display, False);
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
		send_client_message(c->win.w, atom[WMProtocols], atom[WMDelete]);
	else
		XKillClient(display, c->win.w);
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
		write_event("%sUrgent 0x%x %s\n", cnot, c->win, cwrite);
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
		wmh = XGetWMHints(display, c->win.w);
		if(wmh) {
			if(urgent)
				wmh->flags |= XUrgencyHint;
			else
				wmh->flags &= ~XUrgencyHint;
			XSetWMHints(display, c->win.w, wmh);
			XFree(wmh);
		}
	}
}

void
prop_client(Client *c, Atom a) {
	XWMHints *wmh;
	long msize;

	if(a ==  atom[WMProtocols])
		c->proto = winprotocols(&c->win);
	else if(a== atom[NetWMName]) {
wmname:
		update_client_name(c);
		if(c->frame)
			draw_frame(c->sel);
	}
	else switch (a) {
	case XA_WM_TRANSIENT_FOR:
		XGetTransientForHint(display, c->win.w, &c->trans);
		break;
	case XA_WM_NORMAL_HINTS:
		if(!XGetWMNormalHints(display, c->win.w, &c->size, &msize) || !c->size.flags)
			c->size.flags = PSize;
		c->fixedsize = False;
		if((c->size.flags & PMinSize) && (c->size.flags & PMaxSize)
		&&(c->size.min_width == c->size.max_width)
		&&(c->size.min_height == c->size.max_height))
				c->fixedsize = True;
		break;
	case XA_WM_HINTS:
		wmh = XGetWMHints(display, c->win.w);
		if(wmh) {
			set_urgent(c, (wmh->flags & XUrgencyHint) != 0, False);
			XFree(wmh);
		}
		break;
	case XA_WM_NAME:
		goto wmname;
	}
}

void
gravitate_client(Client *c, Bool invert) {
	Point d;
	int gravity;

	gravity = NorthWestGravity;
	if(c->size.flags & PWinGravity) {
		gravity = c->size.win_gravity;
	}

	d.y = 0;
	switch (gravity) {
	case StaticGravity:
	case NorthWestGravity:
	case NorthGravity:
	case NorthEastGravity:
		d.y = labelh(def.font);
		break;
	case EastGravity:
	case CenterGravity:
	case WestGravity:
		d.y = -(Dy(c->rect) / 2) + labelh(def.font);
		break;
	case SouthEastGravity:
	case SouthGravity:
	case SouthWestGravity:
		d.y = -Dy(c->rect);
		break;
	default:
		break;
	}

	d.x = 0;
	switch (gravity) {
	case StaticGravity:
	case NorthWestGravity:
	case WestGravity:
	case SouthWestGravity:
		d.x = def.border;
		break;
	case NorthGravity:
	case CenterGravity:
	case SouthGravity:
		d.x = -(Dx(c->rect) / 2) + def.border;
		break;
	case NorthEastGravity:
	case EastGravity:
	case SouthEastGravity:
		d.x = -(Dy(c->rect) + def.border);
		break;
	default:
		break;
	}

	if(invert)
		rectsubpt(c->rect, d);
	else
		rectaddpt(c->rect, d);
}

void
apply_sizehints(Client *c, Rectangle *r, Bool floating, Bool frame, Align sticky) {
	XSizeHints *s;
	Rectangle r2;
	uint bw, bh;

	bw = 0;
	bh = 0;
	s = &c->size;

	r2 = rectsubpt(*r, r->min);
	if(frame)
		r2 = frame2client(c->sel, r2);

	if(s->flags & PMinSize) {
		bw = s->min_width;
		bh = s->min_height;
		if(floating) {
			if(Dx(r2) < s->min_width)
				r2.max.x = s->min_width;
			if(Dy(r2) < s->min_height)
				r2.max.y = s->min_height;
		}
	}
	if(s->flags & PMaxSize) {
		if(Dx(r2) > s->max_width)
			r2.max.x = s->min_width;
		if(Dy(r2) > s->max_height)
			r2.max.y = s->min_height;
	}

	if(s->flags & PBaseSize) {
		bw = s->base_width;
		bh = s->base_height;
	}

	if(s->flags & PResizeInc) {
		if(s->width_inc > 0) 
			r2.max.x -= (Dx(r2) - bw) % s->width_inc;
		if(s->height_inc > 0)
			r2.max.y -= (Dy(r2) - bh) % s->height_inc;
	}

	if((s->flags & (PBaseSize|PMinSize)) == PMinSize) {
		bw = 0;
		bh = 0;
	}

	if(s->flags & PAspect) {
		double min, max, initial;

		min = (double)s->min_aspect.x / s->min_aspect.y;
		max = (double)s->max_aspect.x / s->max_aspect.y;
		initial = (double)(Dx(r2) - bw) / (Dy(r2) - bh);
		if(initial < min)
			r2.max.y = bh + (Dx(r2) - bw) / min;
		if(initial > max)
			r2.max.x = bw + (Dy(r2) - bh) * max;
	}

	if(frame)
		r2 = client2frame(c->sel, r2);

	if(!(s->flags & PMinSize) || !floating) {
		/* Not allowed to grow */
		if(Dx(r2) > Dx(*r))
			r2.max.x =Dx(*r);
		if(Dy(r2) > Dy(*r))
			r2.max.y = Dy(*r);
	}

	if((sticky & (EAST|WEST)) == EAST)
		r->min.x = r->max.x - Dx(r2);
	if((sticky & (NORTH|SOUTH)) == SOUTH)
		r->min.y = r->max.y - Dy(r2);
	*r = rectaddpt(r2, r->min);
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
		fprintf(stderr, "focus_client(%p) => %s\n", c, (c ? c->name : nil));
	if(screen->focus != c) {
		if(c && verbose)
			fprintf(stderr, "\t%s => %s\n", (screen->focus ? screen->focus->name : "<nil>"),
					(c ? c->name : "<nil>"));
		if(c)
			XSetInputFocus(display, c->win.w, RevertToParent, CurrentTime);
		else
			XSetInputFocus(display, screen->barwin->w, RevertToParent, CurrentTime);
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

	c->rect = rectaddpt(f->crect, f->rect.min);

	if((f->area->mode == Colmax)
	&& (f->area->sel != f)) {
		unmap_frame(c);
		unmap_client(c, IconicState);
	}else if(f->collapsed) {
		reshapewin(c->framewin, f->rect);
		map_frame(c);
		unmap_client(c, IconicState);
	}else {
		reshapewin(&c->win, f->crect);
		map_client(c);
		reshapewin(c->framewin, f->rect);
		map_frame(c);
		configure_client(c);
	}
	
	flushevents(FocusChangeMask|ExposureMask, True);
}

void
newcol_client(Client *c, char *arg) {
	Frame *f = c->sel;
	Area *to, *a = f->area;
	View *v = a->view;

	if(a->floating)
		return;
	if(!f->anext && f == a->frame)
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
	XChangeProperty(display, c->win.w, atom[TagsAtom], XA_STRING, 8,
			PropModeReplace, (uchar *)c->tags, strlen(c->tags));
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
