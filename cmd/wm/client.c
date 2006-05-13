/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

#include "wm.h"

#define CLIENT_MASK		(StructureNotifyMask | PropertyChangeMask | EnterWindowMask)

static Vector *
vector_of_clients(ClientVector *cv)
{
	return (Vector *) cv;
}

static void
update_client_name(Client *c)
{
	XTextProperty name;
	int n;
	char **list = nil;

	name.nitems = 0;
	c->name[0] = 0;
	XGetTextProperty(dpy, c->win, &name, net_atom[NetWMName]);
	if(!name.nitems)
		XGetWMName(dpy, c->win, &name);
	if(!name.nitems)
		return;
	if(name.encoding == XA_STRING)
		cext_strlcpy(c->name, (char *)name.value, sizeof(c->name));
	else {
		if(Xi18nTextPropertyToTextList(dpy, &name, &list, &n) >= Success
				&& n > 0 && *list)
		{
			cext_strlcpy(c->name, *list, sizeof(c->name));
			XFreeStringList(list);
		}
	}
	XFree(name.value);
}

Client *
create_client(Window w, XWindowAttributes *wa)
{
	Client *c = (Client *) cext_emallocz(sizeof(Client));
	XSetWindowAttributes fwa;
	XClassHint ch;
	long msize;
	static unsigned int id = 1;
	static char buf[256];

	c->id = id++;
	c->win = w;
	c->rect.x = wa->x;
	c->rect.y = wa->y;
	c->border = wa->border_width;
	c->rect.width = wa->width + 2 * c->border;
	c->rect.height = wa->height + 2 * c->border;
	XSetWindowBorderWidth(dpy, c->win, 0);
	c->proto = win_proto(c->win);
	XGetTransientForHint(dpy, c->win, &c->trans);
	if(!XGetWMNormalHints(dpy, c->win, &c->size, &msize) || !c->size.flags)
		c->size.flags = PSize;
	XAddToSaveSet(dpy, c->win);
	update_client_name(c);
	if(XGetClassHint(dpy, c->win, &ch)) {
		snprintf(c->classinst, sizeof(c->classinst), "%s:%s",
				ch.res_class ? ch.res_class : "",
				ch.res_name ? ch.res_name : "");
		if(ch.res_class)
			XFree(ch.res_class);
		if(ch.res_name)
			XFree(ch.res_name);
	}
	fwa.override_redirect = 1;
	fwa.background_pixmap = ParentRelative;
	fwa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask
		| ExposureMask | ButtonPressMask | ButtonReleaseMask;

	c->framewin = XCreateWindow(dpy, root, c->rect.x, c->rect.y,
			c->rect.width + 2 * def.border,
			c->rect.height + def.border + height_of_bar(), 0,
			DefaultDepth(dpy, screen), CopyFromParent,
			DefaultVisual(dpy, screen),
			CWOverrideRedirect | CWBackPixmap | CWEventMask, &fwa);
	c->gc = XCreateGC(dpy, c->framewin, 0, 0);
	XSync(dpy, False);
	cext_vattach(vector_of_clients(&client), c);
	snprintf(buf, sizeof(buf), "CreateClient %d\n", client.size - 1);
	write_event(buf);
	return c;
}

void
set_client_state(Client * c, int state)
{
	long data[2];

	data[0] = (long) state;
	data[1] = (long) None;
	XChangeProperty(dpy, c->win, wm_atom[WMState], wm_atom[WMState], 32,
			PropModeReplace, (unsigned char *) data, 2);
}

void
update_client_grab(Client *c, Bool is_sel)
{
	if(is_sel) {
		ungrab_mouse(c->framewin, AnyModifier, AnyButton);
		grab_mouse(c->framewin, def.mod, Button1);
		grab_mouse(c->framewin, def.mod, Button2);
		grab_mouse(c->framewin, def.mod, Button3);
	}
	else
		grab_mouse(c->framewin, AnyModifier, Button1);
}

void
focus_client(Client *c, Bool restack)
{
	Client *old = sel_client();
	Frame *f = c->frame.data[c->sel];
	Client *old_in_area = sel_client_of_area(f->area);
	View *v = f->area->view;
	int i = idx_of_area(f->area);
	static char buf[256];

	v->sel = i;
	f->area->sel = idx_of_frame(f);
	if(restack)
		restack_view(v);
	else {
		if(old)
			update_client_grab(old, False);
		update_client_grab(c, True);
	}

	if(i > 0 && f->area->mode == Colstack)
		arrange_column(f->area, False);
	XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
	if(old && old != old_in_area && old != c)
		draw_client(old);
	if(old_in_area && old_in_area != c)
		draw_client(old_in_area);
	draw_client(c);
	XSync(dpy, False);
	snprintf(buf, sizeof(buf), "ClientFocus %d\n", idx_of_client_id(c->id));
	write_event(buf);
}

void
map_client(Client *c)
{
	XSelectInput(dpy, c->win, CLIENT_MASK & ~StructureNotifyMask);
	XMapWindow(dpy, c->win);
	XSelectInput(dpy, c->win, CLIENT_MASK);
	set_client_state(c, NormalState);
}

void
unmap_client(Client *c)
{
	XSelectInput(dpy, c->win, CLIENT_MASK & ~StructureNotifyMask);
	XUnmapWindow(dpy, c->win);
	XSelectInput(dpy, c->win, CLIENT_MASK);
	set_client_state(c, WithdrawnState);
}

void
reparent_client(Client *c, Window w, int x, int y)
{
	XSelectInput(dpy, c->win, CLIENT_MASK & ~StructureNotifyMask);
	XReparentWindow(dpy, c->win, w, x, y);
	XSelectInput(dpy, c->win, CLIENT_MASK);
}

void
configure_client(Client *c)
{
	XConfigureEvent e;
	Frame *f = c->frame.data[c->sel];
	e.type = ConfigureNotify;
	e.event = c->win;
	e.window = c->win;
	e.x = c->rect.x;
	e.y = c->rect.y;
	if(f) {
		e.x += f->rect.x;
		e.y += f->rect.y;
	}
	e.width = c->rect.width;
	e.height = c->rect.height;
	e.border_width = c->border;
	e.above = None;
	e.override_redirect = False;
	XSelectInput(dpy, c->win, CLIENT_MASK & ~StructureNotifyMask);
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *) & e);
	XSelectInput(dpy, c->win, CLIENT_MASK);
	XSync(dpy, False);
}

static void
send_client_message(Window w, Atom a, long value)
{
	XEvent e;
	e.type = ClientMessage;
	e.xclient.window = w;
	e.xclient.message_type = a;
	e.xclient.format = 32;
	e.xclient.data.l[0] = value;
	e.xclient.data.l[1] = CurrentTime;

	XSendEvent(dpy, w, False, NoEventMask, &e);
	XSync(dpy, False);
}

void
kill_client(Client * c)
{
	if(c->proto & WM_PROTOCOL_DELWIN)
		send_client_message(c->win, wm_atom[WMProtocols], wm_atom[WMDelete]);
	else
		XKillClient(dpy, c->win);
}

void
prop_client(Client *c, XPropertyEvent *e)
{
	long msize;

	if(e->atom == wm_atom[WMProtocols]) {
		/* update */
		c->proto = win_proto(c->win);
		return;
	}
	switch (e->atom) {
	case XA_WM_TRANSIENT_FOR:
		XGetTransientForHint(dpy, c->win, &c->trans);
		break;
	case XA_WM_NORMAL_HINTS:
		if(!XGetWMNormalHints(dpy, c->win, &c->size, &msize) || !c->size.flags) {
			c->size.flags = PSize;
		}
		break;
	}
	if(e->atom == XA_WM_NAME || e->atom == net_atom[NetWMName]) {
		update_client_name(c);
		if(c->frame.size)
			draw_client(c);
	}
}

void
draw_client(Client *c)
{
	BlitzDraw d = { 0 };
	Frame *f;
	char buf[256];
	int fidx;
	unsigned int w;

	if(!c->frame.size)
		return; /* might not have been attached atm */

	f = c->frame.data[c->sel];
	fidx = idx_of_frame(f);
	d.drawable = c->framewin;
	d.font = blitzfont;
	d.gc = c->gc;

	if(c == sel_client())
		d.color = def.sel;
	else
		d.color = def.norm;

	/* draw border */
	if(def.border) {
		d.rect = f->rect;
		d.rect.x = d.rect.y = 0;
		d.notch = &c->rect;
		blitz_drawlabel(dpy, &d);
		blitz_drawborder(dpy, &d);
	}
	d.rect.x = 0;
	d.rect.y = 0;
	d.rect.height = height_of_bar();
	d.notch = nil;

	/* mode bar */
	d.align = CENTER;
	snprintf(buf, sizeof(buf), "%s%d/%d",
		/* if */	!idx_of_area(f->area) ? "~" : "",
				fidx + 1, f->area->frame.size);
	w = d.rect.width = d.rect.height + blitz_textwidth(dpy, &blitzfont, buf);
	if(w > f->rect.width)
		return;
	d.rect.x = f->rect.width - d.rect.width; 
	d.data = buf;
	
	if(f->area->sel == fidx)
		d.color = def.sel;
	else
		d.color = def.norm;
	blitz_drawlabel(dpy, &d);
	blitz_drawborder(dpy, &d);
	d.rect.x = 0;

	if(c == sel_client())
		d.color = def.sel;
	else
		d.color = def.norm;

	/* tag bar */
	d.rect.width = d.rect.height + blitz_textwidth(dpy, &blitzfont, c->tags);
	if(d.rect.width + w > f->rect.width)
		return;
	if(d.rect.width > f->rect.width / 3)
		d.rect.width = f->rect.width / 3;
	d.data = c->tags;
	blitz_drawlabel(dpy, &d);
	blitz_drawborder(dpy, &d);
	d.rect.x += d.rect.width;

	/* title bar */
	d.align = WEST;
	if(d.rect.x + w > f->rect.width)
		return;
	d.rect.width = f->rect.width - (d.rect.x + w);
	d.data = c->name;
	blitz_drawlabel(dpy, &d);
	blitz_drawborder(dpy, &d);

	XSync(dpy, False);
}

void
gravitate_client(Client *c, Bool invert)
{
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
		dy = height_of_bar();
		break;
	case EastGravity:
	case CenterGravity:
	case WestGravity:
		dy = -(c->rect.height / 2) + height_of_bar();
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

void
manage_client(Client *c)
{
	Client *trans;

	if(c->trans && (trans = client_of_win(c->trans)))
		cext_strlcpy(c->tags, trans->tags, sizeof(c->tags));
	if(!strlen(c->tags))
		apply_rules(c);

	reparent_client(c, c->framewin, c->rect.x, c->rect.y);
	update_views();
	map_client(c);
	XMapWindow(dpy, c->framewin);
	XSync(dpy, False);
	if(c->frame.data[c->sel]->area->view == view.data[sel])
		focus_client(c, False);
	flush_masked_events(EnterWindowMask);
}

static int
dummy_error_handler(Display *dpy, XErrorEvent *error)
{
	return 0;
}

void
destroy_client(Client *c)
{
	unsigned int i;

	XGrabServer(dpy);
	XSetErrorHandler(dummy_error_handler);

	for(i = 0; i < view.size; i++)
		detach_from_view(view.data[i], c);

	unmap_client(c);

	if(c->frame.size) {
		c->rect.x = c->frame.data[c->sel]->rect.x;
		c->rect.y = c->frame.data[c->sel]->rect.y;
	}

	reparent_client(c, root, c->rect.x, c->rect.y);
	XFreeGC(dpy, c->gc);
	XDestroyWindow(dpy, c->framewin);
	cext_vdetach(vector_of_clients(&client), c);
	update_views();
	free(c);

	XSync(dpy, False);
	XSetErrorHandler(wmii_error_handler);
	XUngrabServer(dpy);
	flush_masked_events(EnterWindowMask);
}

Client *
sel_client()
{
	return view.size ? sel_client_of_view(view.data[sel]) : nil;
}

static void
match_sizehints(Client *c)
{
	XSizeHints *s = &c->size;

	if(s->flags & PMinSize) {
		if(c->rect.width < c->size.min_width)
			c->rect.width = c->size.min_width;
		if(c->rect.height < c->size.min_height)
			c->rect.height = c->size.min_height;
	}
	if(s->flags & PMaxSize) {
		if(c->rect.width > c->size.max_width)
			c->rect.width = c->size.max_width;
		if(c->rect.height > c->size.max_height)
			c->rect.height = c->size.max_height;
	}

	if(s->flags & PResizeInc) {
		int w = 0, h = 0;

		if(c->size.flags & PBaseSize) {
			w = c->size.base_width;
			h = c->size.base_height;
		} else if(c->size.flags & PMinSize) {
			/* base_{width,height} default to min_{width,height} */
			w = c->size.min_width;
			h = c->size.min_height;
		}
		/* client_width = base_width + i * c->size.width_inc for an integer i */
		w = c->frame.data[c->sel]->rect.width - 2 * def.border - w;
		if(s->width_inc > 0)
			c->frame.data[c->sel]->rect.width -= w % s->width_inc;

		h = c->frame.data[c->sel]->rect.height - def.border - height_of_bar() - h;
		if(s->height_inc > 0)
			c->frame.data[c->sel]->rect.height -= h % s->height_inc;
	}
}

void
resize_client(Client *c, XRectangle *r, Bool ignore_xcall)
{
	Frame *f = c->frame.data[c->sel];
	int fidx = idx_of_frame(f);
	f->rect = *r;

	if((f->area->mode != Colstack) || (f->area->sel == fidx))
		match_sizehints(c);

	if(!ignore_xcall) {
		if(!idx_of_area(f->area) &&
				(c->rect.width >= rect.width) &&
				(c->rect.height >= rect.height))
		{
			f->rect.x = -def.border;
			f->rect.y = -height_of_bar();
		}
		if(f->area->view == view.data[sel])
			XMoveResizeWindow(dpy, c->framewin, f->rect.x,
					f->rect.y, f->rect.width, f->rect.height);
		else
			XMoveResizeWindow(dpy, c->framewin, 2 * rect.width + f->rect.x,
					f->rect.y, f->rect.width, f->rect.height);
	}

	c->rect.x = def.border;
	c->rect.y = height_of_bar();
	if((f->area->sel == fidx) || (f->area->mode != Colstack)) {
		c->rect.width = f->rect.width - 2 * def.border;
		c->rect.height = f->rect.height - def.border - height_of_bar();
	}
	XMoveResizeWindow(dpy, c->win, c->rect.x, c->rect.y,
						c->rect.width, c->rect.height);
	configure_client(c);
}

void
select_client(Client *c, char *arg)
{
	Frame *f = c->frame.data[c->sel];
	Area *a = f->area;
	int i = idx_of_frame(f);
	if(i == -1)
		return;
	if(!strncmp(arg, "prev", 5)) {
		if(!i)
			i = a->frame.size - 1;
		else
			i--;
	} else if(!strncmp(arg, "next", 5)) {
		if(i + 1 < a->frame.size)
			i++;
		else
			i = 0;
	}
	else {
		const char *errstr;
		i = cext_strtonum(arg, 0, a->frame.size - 1, &errstr);
		if(errstr)
			return;
	}
	focus_client(a->frame.data[i]->client, True);
	flush_masked_events(EnterWindowMask);
}

void
swap_client(Client *c, char *arg)
{
	Frame *f1 = c->frame.data[c->sel], *f2;
	Area *o, *a = f1->area;
	View *v = a->view;
	int i = idx_of_area(a), j = idx_of_frame(f1);

	if(i == -1 || j == -1)
		return;

	if(!strncmp(arg, "prev", 5) && i) {
		if(i <= 1)
			return;
		else
			o = v->area.data[i - 1];
		goto Swaparea;
	}
	else if(!strncmp(arg, "next", 5) && i) {
		if(i + 1 < v->area.size)
			o = v->area.data[i + 1];
		else
			return;
Swaparea:
		if(o == a)
			return;
		f2 = o->frame.data[o->sel];

		f1->client = f2->client;
		f2->client = c;
		f1->client->frame.data[f1->client->sel] = f1;
		f2->client->frame.data[f2->client->sel] = f2;

		arrange_column(o, False);
	}
	else if(!strncmp(arg, "up", 3) && i) {
		if(j)
			i = j - 1;
		else
			return;
		a->frame.data[j] = a->frame.data[i];
		a->frame.data[i] = f1;
	}
	else if(!strncmp(arg, "down", 5) && i) {
		if(j + 1 < a->frame.size)
			i = j + 1;
		else
			return;
		a->frame.data[j] = a->frame.data[i];
		a->frame.data[i] = f1;
	}
	if(idx_of_area(a))
		arrange_column(a, False);
	focus_client(c, True);
	flush_masked_events(EnterWindowMask);
}

void
send_client_to(Client *c, char *arg)
{
	const char *errstr;
	Frame *f = c->frame.data[c->sel];
	Area *to, *a = f->area;
	View *v = a->view;
	int i = idx_of_area(a);

	if(i == -1)
		return;

	if(!strncmp(arg, "prev", 5) && i) {
		if(i > 1)
			to = v->area.data[i - 1];
		else if(a->frame.size > 1) {
			if(!(to = new_left_column(v)))
				return;
		}
		else
			return;
	}
	else if(!strncmp(arg, "next", 5) && i) {
		if(i < v->area.size - 1)
			to = v->area.data[i + 1];
		else if(a->frame.size > 1) {
			if(!(to = new_right_column(v)))
				return;
		}
		else
			return;
	}
	else if(!strncmp(arg, "toggle", 7)) {
		if(i)
			to = v->area.data[0];
		else if(c->revert && c->revert != v->area.data[0])
			to = c->revert;
		else
			to = v->area.data[1];
	}
	else {
		i = cext_strtonum(arg, 0, v->area.size - 1, &errstr);
		if(errstr)
			return;
		to = v->area.data[i];
	}
	send_to_area(to, a, c);
	flush_masked_events(EnterWindowMask);
}

void
resize_all_clients()
{
	unsigned int i;
	for(i = 0; i < client.size; i++) {
		Client *c = client.data[i];
		if(c->frame.size && c->frame.data[c->sel]->area) {
			if(idx_of_area(c->frame.data[c->sel]->area))
				resize_column(c, &c->frame.data[c->sel]->rect, nil);
			else
				resize_client(c, &c->frame.data[c->sel]->rect, False);
		}
	}
	flush_masked_events(EnterWindowMask);
}

/* convenience function */
void
focus(Client *c, Bool restack)
{
	Frame *f = c->frame.size ? c->frame.data[c->sel] : nil;
	View *v;

	if(!f)
		return;

	v = f->area->view;
	if(view.data[sel] != v)
		focus_view(v);
	focus_client(c, restack);
}

int
idx_of_client_id(unsigned short id)
{
	int i;
	for(i = 0; i < client.size; i++)
		if(client.data[i]->id == id)
			return i;
	return -1;
}

Client *
client_of_win(Window w)
{
	unsigned int i;

	for(i = 0; (i < client.size) && client.data[i]; i++)
		if(client.data[i]->win == w)
			return client.data[i];
	return nil;
}

void
draw_clients()
{
	unsigned int i;
	for(i = 0; i < client.size; i++) {
		Client *c = client.data[i];
		if(c->frame.size && (c->frame.data[c->sel]->area->view == view.data[sel]))
			draw_client(c);
	}
}

