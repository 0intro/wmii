/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * (C)opyright MMVI-MMVII Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "wmii.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

static void update_client_name(Client *c);

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
create_client(Window w, XWindowAttributes *wa) {
	Client **t, *c;
	XSetWindowAttributes fwa;

	c = emallocz(sizeof(Client));
	c->win = w;
	c->rect.x = wa->x;
	c->rect.y = wa->y;
	c->border = wa->border_width;
	c->rect.width = wa->width;
	c->rect.height = wa->height;

	c->proto = win_proto(c->win);
	prop_client(c, XA_WM_TRANSIENT_FOR);
	prop_client(c, XA_WM_NORMAL_HINTS);
	prop_client(c, XA_WM_HINTS);
	prop_client(c, XA_WM_NAME);

	XSetWindowBorderWidth(blz.dpy, c->win, 0);
	XAddToSaveSet(blz.dpy, c->win);

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
	c->framewin = XCreateWindow(
		/* display */	blz.dpy,
		/* parent */	blz.root,
		/* x */		c->rect.x,
		/* y */		c->rect.y,
		/* width */	c->rect.width + 2 * def.border,
		/* height */	c->rect.height + def.border + labelh(&def.font),
		/* border */	0,
		/* depth */	DefaultDepth(blz.dpy, blz.screen),
		/* class */	CopyFromParent,
		/* visual */	DefaultVisual(blz.dpy, blz.screen),
		/* valuemask */	CWOverrideRedirect | CWEventMask | CWBackPixmap | CWBackingStore,
		/* attributes */&fwa
		);

	c->gc = XCreateGC(blz.dpy, c->framewin, 0, 0);
	XSync(blz.dpy, False);

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

	XGrabServer(blz.dpy);
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
	reparent_client(c, blz.root, c->rect.x, c->rect.y);

	XFreeGC(blz.dpy, c->gc);
	XDestroyWindow(blz.dpy, c->framewin);
	XSync(blz.dpy, False);

	XSetErrorHandler(wmii_error_handler);
	XUngrabServer(blz.dpy);
	flush_masked_events(EnterWindowMask);

	while(XCheckMaskEvent(blz.dpy, StructureNotifyMask, &ev))
		if(ev.type != UnmapNotify || ev.xunmap.window != c->win)
			if(handler[ev.type])
				handler[ev.type](&ev);

	write_event("DestroyClient 0x%x\n", c->win);
	free(c);
}

void
manage_client(Client *c) {
	XTextProperty tags = { 0 };
	Client *trans;

	XGetTextProperty(blz.dpy, c->win, &tags, tags_atom);

	if((trans = client_of_win(c->trans)))
		strncpy(c->tags, trans->tags, sizeof(c->tags));
	else if(tags.nitems)
		strncpy(c->tags, (char *)tags.value, sizeof(c->tags));
	XFree(tags.value);

	gravitate_client(c, False);
	reparent_client(c, c->framewin, def.border, labelh(&def.font));

	if(!strlen(c->tags))
		apply_rules(c);
	else
		apply_tags(c, c->tags);

	if(!starting)
		update_views();
	XSync(blz.dpy, False);

	if(c->sel->view == screen->sel)
		focus(c, True);
	flush_masked_events(EnterWindowMask);
}

Client *
sel_client() {
	if(screen->sel && screen->sel->sel->sel)
		return screen->sel->sel->sel->client;
	return nil;
}

Client *
client_of_win(Window w) {
	Client *c;
	for(c=client; c; c=c->next)
		if(c->win == w) break;
	return c;
}

Frame *
frame_of_win(Window w) {
	Client *c;
	for(c=client; c; c=c->next)
		if(c->framewin == w) break;
	if(c)
		return c->frame;
	return nil;
}

static void
update_client_name(Client *c) {
	XTextProperty name;
	XClassHint ch;
	int n;
	char **list = nil;

	name.nitems = 0;
	c->name[0] = 0;
	XGetTextProperty(blz.dpy, c->win, &name, net_atom[NetWMName]);
	if(!name.nitems)
		XGetWMName(blz.dpy, c->win, &name);
	if(!name.nitems)
		return;
	if(name.encoding == XA_STRING)
		strncpy(c->name, (char *)name.value, sizeof(c->name));
	else {
		if(XmbTextPropertyToTextList(blz.dpy, &name, &list, &n) >= Success
				&& n > 0 && *list)
		{
			strncpy(c->name, *list, sizeof(c->name));
			XFreeStringList(list);
		}
	}
	XFree(name.value);
	if(XGetClassHint(blz.dpy, c->win, &ch)) {
		snprintf(c->props, sizeof(c->props),
				"%s:%s:%s",
				str_nil(ch.res_class),
				str_nil(ch.res_name),
				c->name);
		if(ch.res_class)
			XFree(ch.res_class);
		if(ch.res_name)
			XFree(ch.res_name);
	}
}

void
update_client_grab(Client *c) {
	Frame *f;
	f = c->sel;
	if((f->client != sel_client())
	|| (f->area->floating && f != f->area->stack)) {
		if(verbose)
			fprintf(stderr, "update_client_grab(%p) AnyButton => %s\n", c, str_nil(c->name));
		grab_button(c->framewin, AnyButton, AnyModifier);
	}else {
		if(verbose)
			fprintf(stderr, "update_client_grab(%p) def.mod => %s\n", c, str_nil(c->name));
		XUngrabButton(blz.dpy, AnyButton, AnyModifier, c->framewin);
		grab_button(c->framewin, Button1, def.mod);
		grab_button(c->framewin, Button3, def.mod);
	}
}

void
set_client_state(Client * c, int state) {
	long data[] = { state, None };
	XChangeProperty(
		/* display */	blz.dpy,
		/* parent */	c->win,
		/* property */	wm_atom[WMState],
		/* type */	wm_atom[WMState],
		/* format */	32,
		/* mode */	PropModeReplace,
		/* data */	(uchar *) data,
		/* npositions */2
		);
}

void
map_client(Client *c) {
	if(!c->mapped) {
		XSelectInput(blz.dpy, c->win, ClientMask & ~StructureNotifyMask);
		XMapWindow(blz.dpy, c->win);
		XSelectInput(blz.dpy, c->win, ClientMask);
		set_client_state(c, NormalState);
		c->mapped = 1;
	}
}

void
unmap_client(Client *c, int state) {
	if(c->mapped) {
		c->unmapped++;
		XSelectInput(blz.dpy, c->win, ClientMask & ~StructureNotifyMask);
		XUnmapWindow(blz.dpy, c->win);
		XSelectInput(blz.dpy, c->win, ClientMask);
		set_client_state(c, state);
		c->mapped = 0;
	}
}

void
map_frame(Client *c) {
	if(!c->frame_mapped) {
		XMapWindow(blz.dpy, c->framewin);
		c->frame_mapped = True;
	}
}

void
unmap_frame(Client *c) {
	if(c->frame_mapped) {
		XUnmapWindow(blz.dpy, c->framewin);
		c->frame_mapped = False;
	}
}

void
reparent_client(Client *c, Window w, int x, int y) {
	XSelectInput(blz.dpy, c->win, ClientMask & ~StructureNotifyMask);
	XReparentWindow(blz.dpy, c->win, w, x, y);
	XSelectInput(blz.dpy, c->win, ClientMask);
}

void
set_cursor(Client *c, Cursor cur) {
	XSetWindowAttributes wa;

	if(c->cursor != cur) {
		c->cursor = cur;
		wa.cursor = cur;
		XChangeWindowAttributes(blz.dpy, c->framewin, CWCursor, &wa);
	}
}

void
configure_client(Client *c) {
	XConfigureEvent e;
	Frame *f;

	f = c->sel;
	if(!f)
		return;

	e.type = ConfigureNotify;
	e.event = c->win;
	e.window = c->win;
	e.x = f->crect.x + f->rect.x - c->border;
	e.y = f->crect.y + f->rect.y - c->border;
	e.width = f->crect.width;
	e.height = f->crect.height;
	e.border_width = c->border;
	e.above = None;
	e.override_redirect = False;
	XSendEvent(blz.dpy, c->win, False,
			StructureNotifyMask, (XEvent *) & e);
	XSync(blz.dpy, False);
}

static void
send_client_message(Window w, Atom a, long value) {
	XEvent e;

	e.type = ClientMessage;
	e.xclient.window = w;
	e.xclient.message_type = a;
	e.xclient.format = 32;
	e.xclient.data.l[0] = value;
	e.xclient.data.l[1] = CurrentTime;
	XSendEvent(blz.dpy, w, False, NoEventMask, &e);
	XSync(blz.dpy, False);
}

void
kill_client(Client * c) {
	if(c->proto & WM_PROTOCOL_DELWIN)
		send_client_message(c->win, wm_atom[WMProtocols], wm_atom[WMDelete]);
	else
		XKillClient(blz.dpy, c->win);
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
		wmh = XGetWMHints(blz.dpy, c->win);
		if(wmh) {
			if(urgent)
				wmh->flags |= XUrgencyHint;
			else
				wmh->flags &= ~XUrgencyHint;
			XSetWMHints(blz.dpy, c->win, wmh);
			XFree(wmh);
		}
	}
}

void
prop_client(Client *c, Atom a) {
	XWMHints *wmh;
	long msize;

	if(a ==  wm_atom[WMProtocols])
		c->proto = win_proto(c->win);
	else if(a== net_atom[NetWMName]) {
wmname:
		update_client_name(c);
		if(c->frame)
			draw_frame(c->sel);
	}else switch (a) {
	case XA_WM_TRANSIENT_FOR:
		XGetTransientForHint(blz.dpy, c->win, &c->trans);
		break;
	case XA_WM_NORMAL_HINTS:
		if(!XGetWMNormalHints(blz.dpy, c->win, &c->size, &msize) || !c->size.flags)
			c->size.flags = PSize;
		c->fixedsize = False;
		if((c->size.flags & PMinSize) && (c->size.flags & PMaxSize)
		&&(c->size.min_width == c->size.max_width)
		&&(c->size.min_height == c->size.max_height))
				c->fixedsize = True;
		break;
	case XA_WM_HINTS:
		wmh = XGetWMHints(blz.dpy, c->win);
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
	int dx, dy;
	int gravity;

	gravity = NorthWestGravity;
	if(c->size.flags & PWinGravity) {
		gravity = c->size.win_gravity;
	}

	dy = 0;
	switch (gravity) {
	case StaticGravity:
	case NorthWestGravity:
	case NorthGravity:
	case NorthEastGravity:
		dy = labelh(&def.font);
		break;
	case EastGravity:
	case CenterGravity:
	case WestGravity:
		dy = -(c->rect.height / 2) + labelh(&def.font);
		break;
	case SouthEastGravity:
	case SouthGravity:
	case SouthWestGravity:
		dy = -c->rect.height;
		break;
	default:
		break;
	}

	dx = 0;
	switch (gravity) {
	case StaticGravity:
	case NorthWestGravity:
	case WestGravity:
	case SouthWestGravity:
		dx = def.border;
		break;
	case NorthGravity:
	case CenterGravity:
	case SouthGravity:
		dx = -(c->rect.width / 2) + def.border;
		break;
	case NorthEastGravity:
	case EastGravity:
	case SouthEastGravity:
		dx = -(c->rect.width + def.border);
		break;
	default:
		break;
	}

	if(invert) {
		dx = -dx;
		dy = -dy;
	}
	c->rect.x += dx;
	c->rect.y += dy;
}

void
apply_sizehints(Client *c, XRectangle *r, Bool floating, Bool frame, BlitzAlign sticky) {
	XSizeHints *s;
	XRectangle orig;
	uint bw, bh;
	
	s = &c->size;
	orig = *r;
	if(frame)
		frame2client(r);
	bw = 0;
	bh = 0;

	if(s->flags & PMinSize) {
		bw = s->min_width;
		bh = s->min_height;
		if(floating) {
			if(r->width < s->min_width)
				r->width = s->min_width;
			if(r->height < s->min_height)
				r->height = s->min_height;
		}
	}
	if(s->flags & PMaxSize) {
		if(r->width > s->max_width)
			r->width = s->max_width;
		if(r->height > s->max_height)
			r->height = s->max_height;
	}

	if(s->flags & PBaseSize) {
		bw = s->base_width;
		bh = s->base_height;
	}

	if(s->flags & PResizeInc) {
		if(s->width_inc > 0) 
			r->width -= (r->width - bw) % s->width_inc;
		if(s->height_inc > 0)
			r->height -= (r->height - bh) % s->height_inc;
	}

	if((s->flags & (PBaseSize|PMinSize)) == PMinSize) {
		bw = 0;
		bh = 0;
	}

	if(s->flags & PAspect) {
		double min, max, initial;

		min = (double)s->min_aspect.x / s->min_aspect.y;
		max = (double)s->max_aspect.x / s->max_aspect.y;
		initial = (double)(r->width - bw) / (r->height - bh);
		if(initial < min)
			r->height = bh + (r->width - bw) / min;
		if(initial > max)
			r->width = bw + (r->height - bh) * max;
	}

	if(frame)
		client2frame(r);

	if(!(s->flags & PMinSize) || !floating) {
		if(r->width > orig.width)
			r->width = orig.width;
		if(r->height > orig.height)
			r->height = orig.height;
	}
	
	if((sticky & (EAST|WEST)) == EAST)
		r->x = r_east(&orig) - r->width;
	if((sticky & (NORTH|SOUTH)) == SOUTH)
		r->y = r_south(&orig) - r->height;
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
	XEvent ev;

	while(XCheckMaskEvent(blz.dpy, FocusChangeMask, &ev))
		if(handler[ev.xany.type])
			handler[ev.xany.type](&ev);

	if(verbose)
		fprintf(stderr, "focus_client(%p) => %s\n", c, (c ? c->name : nil));
	if(screen->focus != c) {
		if(c && verbose)
			fprintf(stderr, "\t%s => %s\n", (screen->focus ? screen->focus->name : "<nil>"),
					(c ? c->name : "<nil>"));
		if(c) {
			XSetInputFocus(blz.dpy, c->win, RevertToParent, CurrentTime);
			update_client_grab(c);
		}else
			XSetInputFocus(blz.dpy, screen->barwin, RevertToParent, CurrentTime);
	}

	while(XCheckMaskEvent(blz.dpy, FocusChangeMask, &ev))
		if(handler[ev.xany.type])
			handler[ev.xany.type](&ev);
}

void
resize_client(Client *c, XRectangle *r) {
	Frame *f;
	XEvent ev;

	f = c->sel;
	resize_frame(f, r);

	if(f->area->view != screen->sel) {
		unmap_client(c, IconicState);
		unmap_frame(c);
		return;
	}

	c->rect = f->crect;
	c->rect.x += f->rect.x;
	c->rect.y += f->rect.y;
	if((f->area->mode == Colmax)
	&& (f->area->sel != f)) {
		unmap_frame(c);
		unmap_client(c, IconicState);
	}else if(f->collapsed) {
		XMoveResizeWindow(blz.dpy, c->framewin,
				f->rect.x, f->rect.y,
				f->rect.width, f->rect.height);
		map_frame(c);
		unmap_client(c, IconicState);
	}else {
		XMoveResizeWindow(blz.dpy, c->win,
				f->crect.x, f->crect.y,
				f->crect.width, f->crect.height);
		map_client(c);
		XMoveResizeWindow(blz.dpy, c->framewin,
				f->rect.x, f->rect.y,
				f->rect.width, f->rect.height);
		map_frame(c);
		configure_client(c);
	}
	
	while(XCheckMaskEvent(blz.dpy, FocusChangeMask|ExposureMask, &ev))
		if(handler[ev.xany.type])
			handler[ev.xany.type](&ev);
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
	flush_masked_events(EnterWindowMask);
}

void
move_client(Client *c, char *arg) {
	Frame *f = c->sel;
	XRectangle new = f->rect;
	int x, y;

	if(sscanf(arg, "%d %d", &x, &y) != 2)
		return;
	new.x += x;
	new.y += y;
	if(!f->area->floating)
		resize_column(f->client, &new);
	else
		resize_client(f->client, &new);
}

void
size_client(Client *c, char *arg) {
	Frame *f = c->sel;
	XRectangle new = f->rect;
	int w, h;

	if(sscanf(arg, "%d %d", &w, &h) != 2)
		return;
	new.width += w;
	new.height += h;
	if(!f->area->floating)
		resize_column(f->client, &new);
	else
		resize_client(f->client, &new);
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
			if(!to && (f->anext || f != a->frame))
				to=new_column(v, v->area, 0);
			goto send_area;
		}
		else if(!strncmp(arg, "right", 5)) {
			if(a->floating)
				return Ebadvalue;
			if(!(to = a->next) && (f->anext || f != a->frame))
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

	flush_masked_events(EnterWindowMask);
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

	flush_masked_events(EnterWindowMask);
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
	XChangeProperty(blz.dpy, c->win, tags_atom, XA_STRING, 8,
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
	if(!strncmp(message, "kill", 5))
		kill_client(c);
	else if(!strncmp(message, "Urgent", 7))
		set_urgent(c, True, True);
	else if(!strncmp(message, "NotUrgent", 10))
		set_urgent(c, False, True);
	else
		return Ebadcmd;
	return nil;
}
