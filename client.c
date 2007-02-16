/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
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

#define CLIENT_MASK		(StructureNotifyMask | PropertyChangeMask | EnterWindowMask)
#define ButtonMask		(ButtonPressMask | ButtonReleaseMask)

Client *
create_client(Window w, XWindowAttributes *wa) {
	Client **t, *c;
	XSetWindowAttributes fwa;
	long msize;

	c = emallocz(sizeof(Client));
	c->win = w;
	c->rect.x = wa->x;
	c->rect.y = wa->y;
	c->border = wa->border_width;
	c->rect.width = wa->width;
	c->rect.height = wa->height;
	c->proto = win_proto(c->win);
	update_client_name(c);
	gravitate_client(c, False);

	c->fixedsize = False;
	XGetTransientForHint(blz.dpy, c->win, &c->trans);
	if(!XGetWMNormalHints(blz.dpy, c->win, &c->size, &msize) || !c->size.flags)
		c->size.flags = PSize;
	if(c->size.flags & PMinSize && c->size.flags & PMaxSize
		&& c->size.min_width == c->size.max_width
		&& c->size.min_height == c->size.max_height)
			c->fixedsize = True;
	if(c->rect.width == screen->rect.width
	&& c->rect.height == screen->rect.height)
		c->fullscreen = True;

	XSetWindowBorderWidth(blz.dpy, c->win, 0);
	XAddToSaveSet(blz.dpy, c->win);
	fwa.override_redirect = 1;
	fwa.background_pixmap = ParentRelative;
	fwa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask
		| ExposureMask | PointerMotionMask | KeyPressMask
		| ButtonPressMask | ButtonReleaseMask;
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
		/* valuemask */	CWOverrideRedirect | CWEventMask | CWBackPixmap,
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

void
manage_client(Client *c) {
	XTextProperty tags;
	Client *trans;

	tags.nitems = 0;
	XGetTextProperty(blz.dpy, c->win, &tags, tags_atom);
	if((c->trans) && (trans = client_of_win(c->trans)))
		strncpy(c->tags, trans->tags, sizeof(c->tags));
	else if(tags.nitems)
		strncpy(c->tags, (char *)tags.value, sizeof(c->tags));
	XFree(tags.value);

	reparent_client(c, c->framewin, c->rect.x, c->rect.y);

	if(!strlen(c->tags))
		apply_rules(c);
	else
		apply_tags(c, c->tags);

	if(!starting)
		update_views();
	XSync(blz.dpy, False);

	if(c->sel->area->view == screen->sel)
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
	if(!f->area->floating || f == f->area->stack) {
		XUngrabButton(blz.dpy, AnyButton, AnyModifier, c->framewin);
		grab_button(c->framewin, Button1, def.mod);
		grab_button(c->framewin, Button3, def.mod);
	}
	else
		grab_button(c->framewin, AnyButton, AnyModifier);
}

/* convenience function */
void
focus(Client *c, Bool restack) {
	View *v;
	Frame *f;

	if(!(f = c->sel)) return;
	v = f->area->view;
	arrange_column(f->area, False);
	focus_view(screen, v);
	focus_frame(c->sel, restack);
}

void
set_client_state(Client * c, int state)
{
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
		XSelectInput(blz.dpy, c->win, CLIENT_MASK & ~StructureNotifyMask);
		XMapWindow(blz.dpy, c->win);
		XSelectInput(blz.dpy, c->win, CLIENT_MASK);
	}
	set_client_state(c, NormalState);
	c->mapped = 1;
}

void
unmap_client(Client *c, int state) {
	if(c->mapped) {
		XSelectInput(blz.dpy, c->win, CLIENT_MASK & ~StructureNotifyMask);
		XUnmapWindow(blz.dpy, c->win);
		XSelectInput(blz.dpy, c->win, CLIENT_MASK);
	/* Always set this, since we don't care anymore once it's been destroyed */
		c->unmapped++;
	}
	set_client_state(c, state);
	c->mapped = 0;
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
	XSelectInput(blz.dpy, c->win, CLIENT_MASK & ~StructureNotifyMask);
	XReparentWindow(blz.dpy, c->win, w, x, y);
	XSelectInput(blz.dpy, c->win, CLIENT_MASK);
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
		write_event("%sUrgent 0x%x %s\n", cnot, client->win, cwrite);
		c->urgent = urgent;
		if(c->sel) {
			if(c->sel->view == screen->sel) {
				update_frame_widget_colors(c->sel);
				draw_frame(c->sel);
			}
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
prop_client(Client *c, XPropertyEvent *e) {
	XWMHints *wmh;
	long msize;

	if(e->atom == wm_atom[WMProtocols]) {
		c->proto = win_proto(c->win);
		return;
	}
	switch (e->atom) {
	case XA_WM_TRANSIENT_FOR:
		XGetTransientForHint(blz.dpy, c->win, &c->trans);
		break;
	case XA_WM_NORMAL_HINTS:
		if(!XGetWMNormalHints(blz.dpy, c->win, &c->size, &msize) || !c->size.flags) {
			c->size.flags = PSize;
		}
		c->fixedsize = False;
		if(c->size.flags & PMinSize && c->size.flags & PMaxSize
			&& c->size.min_width == c->size.max_width
			&& c->size.min_height == c->size.max_height)
				c->fixedsize = True;
		break;
	case XA_WM_HINTS:
		wmh = XGetWMHints(blz.dpy, c->win);
		set_urgent(c, (wmh->flags & XUrgencyHint) != 0, False);
		XFree(wmh);
		break;
	}
	if(e->atom == XA_WM_NAME || e->atom == net_atom[NetWMName]) {
		update_client_name(c);
		if(c->frame)
			draw_frame(c->sel);
	}
}

void
gravitate_client(Client *c, Bool invert) {
	int dx = 0, dy = 0;
	int gravity = NorthWestGravity;

	if(c->size.flags & PWinGravity) {
		gravity = c->size.win_gravity;
	}
	/* y */
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
	/* x */
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

static int
dummy_error_handler(Display *dpy, XErrorEvent *error) {
	return 0;
}

void
destroy_client(Client *c) {
	char *dummy = nil;
	Client **tc;

	XGrabServer(blz.dpy);
	XSetErrorHandler(dummy_error_handler);
	if(c->frame) {
		c->rect.x = c->sel->rect.x;
		c->rect.y = c->sel->rect.y;
	}
	for(tc=&client; *tc; tc=&(*tc)->next)
		if(*tc == c) {
			*tc = c->next;
			break;
		}

	update_client_views(c, &dummy);

	unmap_client(c, WithdrawnState);
	reparent_client(c, blz.root, c->rect.x, c->rect.y);
	XFreeGC(blz.dpy, c->gc);
	XDestroyWindow(blz.dpy, c->framewin);
	XSync(blz.dpy, False);

	XSetErrorHandler(wmii_error_handler);
	XUngrabServer(blz.dpy);
	flush_masked_events(EnterWindowMask);

	write_event("DestroyClient 0x%x\n", c->win);
	free(c);
}

void
match_sizehints(Client *c, XRectangle *r, Bool floating, BlitzAlign sticky) {
	XSizeHints *s = &c->size;
	uint dx = 2 * def.border;
	uint dy = def.border + labelh(&def.font);
	uint hdiff, wdiff;

	if(floating && (s->flags & PMinSize)) {
		if(r->width < s->min_width + dx) {
			wdiff = (s->min_width + dx) - r->width;
			r->width += wdiff;
			if((sticky & EAST) && !(sticky & WEST))
				r->x -= wdiff;
		}
		if(r->height < s->min_height + dy) {
			hdiff = (s->min_height + dy) - r->height;
			r->height += hdiff;
			if((sticky & SOUTH) && !(sticky & NORTH))
				r->y -= hdiff;
		}
	}
	if(floating && (s->flags & PMaxSize)) {
		if(r->width > s->max_width + dx) {
			wdiff = r->width - (s->max_width + dx);
			r->width -= wdiff;
			if((sticky & EAST) && !(sticky & WEST))
			r->x += wdiff;
		}
		if(r->height > s->max_height + dy) {
			hdiff = r->height - (s->max_height + dy);
			r->height -= hdiff;
			if((sticky & SOUTH) && !(sticky & NORTH))
				r->y += hdiff;
		}
	}
	if(s->flags & PResizeInc) {
		int w = 0, h = 0;
		if(s->flags & PBaseSize) {
			w = s->base_width;
			h = s->base_height;
		} else if(s->flags & PMinSize) {
			/* base_{width,height} default to min_{width,height} */
			w = s->min_width;
			h = s->min_height;
		}
		/* client_width = base_width + i * s->width_inc for an integer i */
		w = r->width - dx - w;
		if(s->width_inc > 0) {
			wdiff = w % s->width_inc;
			r->width -= wdiff;
			if((sticky & EAST) && !(sticky & WEST))
				r->x += wdiff;
		}
		h = r->height - dy - h;
		if(s->height_inc > 0) {
			hdiff = h % s->height_inc;
			r->height -= hdiff;
			if((sticky & SOUTH) && !(sticky & NORTH))
				r->y += hdiff;
		}
	}
}

void
focus_client(Client *c) {
	if(verbose)
		fprintf(stderr, "focus_client(%p)\n", c);
	if(screen->focus != c) {
		if(c)
			XSetInputFocus(blz.dpy, c->win, RevertToParent, CurrentTime);
		else
			XSetInputFocus(blz.dpy, screen->barwin, RevertToParent, CurrentTime);
	}
}

void
resize_client(Client *c, XRectangle *r) {
	Frame *f;

	f = c->sel;
	resize_frame(f, r);

	if(f->area->view == screen->sel) 
		XMoveResizeWindow(blz.dpy, c->framewin,
				f->rect.x, f->rect.y,
				f->rect.width, f->rect.height);
	else {
		unmap_client(c, IconicState);
		unmap_frame(c);
		return;
	}

	c->rect = f->rect;
	if(f->area->mode == Colmax
	&& f->area->sel != f) {
		unmap_frame(c);
		unmap_client(c, IconicState);
	}else if(f->collapsed) {
		map_frame(c);
		unmap_client(c, IconicState);
	}else {
		XMoveResizeWindow(blz.dpy, c->win,
				f->crect.x, f->crect.y,
				f->crect.width, f->crect.height);
		map_client(c);
		map_frame(c);
		configure_client(c);
	}
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
	int cmp;
	Frame *f;
	Frame **fp = &c->frame;

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
			free(f);
			if(c->sel == f)
				c->sel = *fp;
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
	int len;
	Bool add;
	char buf[512], last;
	char *toks[32], *cur;

	buf[0] = 0;
	for(n = 0; tags[n]; n++)
		if(tags[n] != ' ' && tags[n] != '\t') break;
	if(tags[n] == '+' || tags[n] == '-')
		strncpy(buf, c->tags, sizeof(c->tags));
	strncat(buf, &tags[n], sizeof(buf) - strlen(buf));
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
	qsort(toks, j, sizeof(char *), compare_tags);
	len = sizeof(c->tags);
	if(!j) return;
	for(i=0, n=0; i < j && len > 1; i++)
		if(!n || strcmp(toks[i], toks[n-1])) {
			if(i)
				strncat(c->tags, "+", len);
			len -= strlen(c->tags);
			strncat(c->tags, toks[i], len);
			len -= strlen(c->tags);
			toks[n++] = toks[i];
		}
	toks[n] = nil;
	update_client_views(c, toks);
	XChangeProperty(blz.dpy, c->win, tags_atom, XA_STRING, 8,
			PropModeReplace, (uchar *)c->tags, strlen(c->tags));
}

static void
match_tags(Client *c, const char *prop) {
	Rule *r;
	regmatch_t tmpregm;

	for(r=def.tagrules.rule; r; r=r->next)
		if(!regexec(&r->regex, prop, 1, &tmpregm, 0))
			if(!strlen(c->tags) || !strncmp(c->tags, "nil", 4))
				apply_tags(c, r->value);
}

void
apply_rules(Client *c) {
	if(def.tagrules.string)
		match_tags(c, c->props);
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
