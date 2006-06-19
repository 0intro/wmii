/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

#include "wm.h"

#define CLIENT_MASK		(StructureNotifyMask | PropertyChangeMask | EnterWindowMask)

static void
update_client_name(Client *c)
{
	XTextProperty name;
	XClassHint ch;
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
		if(XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success
				&& n > 0 && *list)
		{
			cext_strlcpy(c->name, *list, sizeof(c->name));
			XFreeStringList(list);
		}
	}
	XFree(name.value);
	if(XGetClassHint(dpy, c->win, &ch)) {
		snprintf(c->props, sizeof(c->props), "%s:%s:%s",
				ch.res_class ? ch.res_class : "",
				ch.res_name ? ch.res_name : "",
				c->name);
		if(ch.res_class)
			XFree(ch.res_class);
		if(ch.res_name)
			XFree(ch.res_name);
	}
}

Client *
create_client(Window w, XWindowAttributes *wa)
{
	Client **t, *c = (Client *) cext_emallocz(sizeof(Client));
	XSetWindowAttributes fwa;
	long msize;
	unsigned int i;
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
	if(c->size.flags & PMinSize && c->size.flags & PMaxSize
		&& c->size.min_width == c->size.max_width
		&& c->size.min_height == c->size.max_height)
			c->fixedsize = True;
	else
		c->fixedsize = False;
	XAddToSaveSet(dpy, c->win);
	update_client_name(c);
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

	for(t=&client, i=0; *t; t=&(*t)->next, i++);
	c->next = *t; /* *t == nil */
	*t = c;

	snprintf(buf, sizeof(buf), "CreateClient %d\n", i);
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
		grab_mouse(c->framewin, def.mod, Button3);
	}
	else
		grab_mouse(c->framewin, AnyModifier, Button1);
}

void
focus_client(Client *c, Bool restack)
{
	Client *old;
	Frame *f;
	Client *old_in_area;
	View *v;
	static char buf[256];

	if(!sel_screen)
		return;

	old = sel_client();
	f = c->sel;
	old_in_area = sel_client_of_area(f->area);
	v = f->area->view;
	v->sel = f->area;
	f->area->sel = f;
	c->floating = (f->area == v->area);
	if(restack)
		restack_view(v);
	else {
		if(old)
			update_client_grab(old, False);
		update_client_grab(c, True);
	}

	if(!c->floating && f->area->mode == Colstack)
		arrange_column(f->area, False);
	XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
	if(old && old != old_in_area && old != c)
		draw_frame(old->sel);
	if(old_in_area && old_in_area != c)
		draw_frame(old_in_area->sel);
	draw_frame(c->sel);
	XSync(dpy, False);
	snprintf(buf, sizeof(buf), "ClientFocus %d\n", idx_of_client(c));
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
	Frame *f = c->sel;
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
		if(c->size.flags & PMinSize && c->size.flags & PMaxSize
			&& c->size.min_width == c->size.max_width
			&& c->size.min_height == c->size.max_height)
				c->fixedsize = True;
		else
			c->fixedsize = False;
		break;
	}
	if(e->atom == XA_WM_NAME || e->atom == net_atom[NetWMName]) {
		update_client_name(c);
		if(c->frame)
			draw_frame(c->sel);
	}
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
	if(c->sel->area->view == sel)
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
	View *v;
	Client **tc;

	XGrabServer(dpy);
	XSetErrorHandler(dummy_error_handler);

	if(c->frame) {
		c->rect.x = c->sel->rect.x;
		c->rect.y = c->sel->rect.y;
	}

	for(v=view; v; v=v->next)
		detach_from_view(v, c);

	unmap_client(c);

	reparent_client(c, root, c->rect.x, c->rect.y);
	XFreeGC(dpy, c->gc);
	XDestroyWindow(dpy, c->framewin);

	for(tc=&client; *tc && *tc != c; tc=&(*tc)->next);
	cext_assert(*tc == c);
	*tc = c->next;

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
	return sel && sel->sel->sel ? sel->sel->sel->client : nil;
}

void
match_sizehints(Client *c, XRectangle *r, Bool floating, BlitzAlign sticky)
{
	XSizeHints *s = &c->size;
	unsigned int dx = 2 * def.border;
	unsigned int dy = def.border + height_of_bar();
	unsigned int hdiff, wdiff;

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
resize_client(Client *c, XRectangle *r, Bool ignore_xcall)
{
	Frame *f = c->sel;
	Bool floating = (f->area == f->area->view->area);

	BlitzAlign stickycorner = 0;;
	if(f->rect.x != r->x && f->rect.x + f->rect.width == r->x + r->width)
		stickycorner |= EAST;
	else
		stickycorner |= WEST;
	if(f->rect.y != r->y && f->rect.y + f->rect.height == r->y + r->height)
		stickycorner |= SOUTH;
	else    
		stickycorner |= NORTH;

	f->rect = *r;

	if((f->area->mode != Colstack) || (f->area->sel == f))
		match_sizehints(c, &c->sel->rect, floating, stickycorner);

	if(!ignore_xcall) {
		if(floating &&
				(c->rect.width >= rect.width) &&
				(c->rect.height >= rect.height))
		{
			f->rect.x = -def.border;
			f->rect.y = -height_of_bar();
		}
		if(f->area->view == sel)
			XMoveResizeWindow(dpy, c->framewin, f->rect.x,
					f->rect.y, f->rect.width, f->rect.height);
		else
			XMoveResizeWindow(dpy, c->framewin, 2 * rect.width + f->rect.x,
					f->rect.y, f->rect.width, f->rect.height);
	}

	c->rect.x = def.border;
	c->rect.y = height_of_bar();
	if((f->area->sel == f) || (f->area->mode != Colstack)) {
		c->rect.width = f->rect.width - 2 * def.border;
		c->rect.height = f->rect.height - def.border - height_of_bar();
	}
	XMoveResizeWindow(dpy, c->win, c->rect.x, c->rect.y,
						c->rect.width, c->rect.height);
	configure_client(c);
}

void
newcol_client(Client *c, char *arg)
{
	Frame *f = c->sel;
	Area *to, *a = f->area;
	View *v = a->view;

	if(a == v->area)
		return;
	if(!f->anext && f == a->frame)
		return;

	if(!strncmp(arg, "prev", 5)) {
		for(to=v->area; to && to->next != a; to=to->next);
		to = new_column(v, to, 0);
		send_to_area(to, a, c);
	}
	else if(!strncmp(arg, "next", 5)) {
		to = new_column(v, a, 0);
		send_to_area(to, a, c);
	}
	else
		return;
	flush_masked_events(EnterWindowMask);
}

void
move_client(Client *c, char *arg)
{
	Frame *f = c->sel;
	XRectangle new = f->rect;
	int x, y;

	if(sscanf(arg, "%d %d", &x, &y) != 2)
		return;
	new.x += x;
	new.y += y;
	if(idx_of_area(f->area))
		resize_column(f->client, &new, nil);
	else
		resize_client(f->client, &new, False);
}

void
size_client(Client *c, char *arg)
{
	Frame *f = c->sel;
	XRectangle new = f->rect;
	int w, h;

	if(sscanf(arg, "%d %d", &w, &h) != 2)
		return;
	new.width += w;
	new.height += h;
	if(f->area != f->area->view->area)
		resize_column(f->client, &new, nil);
	else
		resize_client(f->client, &new, False);
}

char *
send_client(Frame *f, char *arg)
{
	Area *to, *a;
	Client *c;
	Frame *tf;
	View *v;
	int i, j;
	a = f->area;
	v = a->view;
	c = f->client;
	i = idx_of_area(a);
	j = idx_of_frame(f);
	static char Ebadvalue[] = "bad value";

	if((i == -1) || (j == -1))
		return 0;

	if(i && !strncmp(arg, "prev", 5)) {
		if(a == v->area)
			return Ebadvalue;
		for(to=v->area->next; to && a != to->next; to=to->next);
		if(!to && (f->anext || f != a->frame))
				to=new_column(v, v->area, 0);
		if(!to)
			return Ebadvalue;
		send_to_area(to, a, c);
	}
	else if(i && !strncmp(arg, "next", 5)) {
		if(a == v->area)
			return Ebadvalue;
		if(!(to = a->next) && (f->anext || f!= a->frame))
			to = new_column(v, a, 0);
		if(!to)
			return Ebadvalue;
		send_to_area(to, a, c);
	}
	else if(!strncmp(arg, "toggle", 7)) {
		if(a != v->area)
			to = v->area;
		else if(c->revert && c->revert != v->area)
			to = c->revert;
		else
			to = v->area->next;
		send_to_area(to, a, c);
	}
	else if(i && !strncmp(arg, "up", 3)) {
		for(tf=a->frame; tf && tf->anext != f; tf=tf->anext);
		if(!tf)
			return Ebadvalue;
		remove_frame(f);
		insert_frame(tf, f, True);
		arrange_column(a, False);
		focus_client(c, True);
	}
	else if(i && !strncmp(arg, "down", 5)) {
		if(!f->anext)
			return Ebadvalue;
		remove_frame(f);
		insert_frame(f->anext, f, False);
		arrange_column(a, False);
		focus_client(c, True);
	}
	else if(i) {
		if(sscanf(arg, "%d", &j) != 1)
			return Ebadvalue;
		for(to=v->area; to && j; to=to->next, j--);
		send_to_area(to, a, c);
	}
	else
		return Ebadvalue;
	flush_masked_events(EnterWindowMask);
	return nil;
}

void
resize_all_clients()
{
	Client *c;
	for(c = client; c; c=c->next) {
		if(c->frame && c->sel->area) {
			if(idx_of_area(c->sel->area))
				resize_column(c, &c->sel->rect, nil);
			else
				resize_client(c, &c->sel->rect, False);
		}
	}
	flush_masked_events(EnterWindowMask);
}

/* convenience function */
void
focus(Client *c, Bool restack)
{
	Frame *f = c->sel;
	View *v;

	if(!f)
		return;

	v = f->area->view;
	if(sel != v)
		focus_view(v);
	focus_client(c, restack);
}

Client *
client_of_id(unsigned short id)
{
	Client *c;
	for(c=client; c && c->id != id; c=c->next);
	return c;
}

int
idx_of_client(Client *c)
{
	Client *cl;
	int i = 0;
	for(cl=client; cl && cl != c; cl=cl->next, i++);
	return cl ? i : -1;
}

Client *
client_of_win(Window w)
{
	Client *c;

	for(c=client; c && c->win != w; c=c->next);
	return c;
}

Client *
selected_client()
{
	return sel && sel->sel->sel ? sel->sel->sel->client : nil;
}

void
draw_clients()
{
	Client *c;
	for(c=client; c; c=c->next)
		if(c->sel && (c->sel->area->view == sel))
			draw_frame(c->sel);
}

static Bool
permit_tag(const char *tag)
{
	static char *exclude[] = { "sel", "status" };
	unsigned int i;
	for(i = 0; i < (sizeof(exclude) / sizeof(exclude[0])); i++)
		if(!strcmp(exclude[i], tag))
			return False;
	return True;
}

void
apply_tags(Client *c, const char *tags)
{
	unsigned int i, j = 0, n;
	char buf[256];
	char *toks[16], *apply[16];

	cext_strlcpy(buf, tags, sizeof(buf));
	if(!(n = cext_tokenize(toks, 16, buf, '+')))
		return;

	for(i = 0; i < n; i++) {
		if(!strncmp(toks[i], "~", 2))
			c->floating = True;
		else if(!strncmp(toks[i], "!", 2)) {
			if(view)
				apply[j++] = sel->name;
			else
				apply[j++] = "nil";
		}
		else if(permit_tag(toks[i]))
			apply[j++] = toks[i];
	}

	c->tags[0] = 0;
	for(i = 0; i < j; i++) {
		cext_strlcat(c->tags, apply[i], sizeof(c->tags) - strlen(c->tags) - 1);
		if(i + 1 < j)
			cext_strlcat(c->tags, "+", sizeof(c->tags) - strlen(c->tags) - 1);
	}

	if(!strlen(c->tags))
		apply_tags(c, "nil");
}

static void
match_tags(Client *c, const char *prop)
{
	Rule *r;
	regmatch_t tmpregm;

	for(r=def.tagrules.rule; r; r=r->next)
		if(!regexec(&r->regex, prop, 1, &tmpregm, 0))
			if(!strlen(c->tags) || !strncmp(c->tags, "nil", 4))
				apply_tags(c, r->value);
}

void
apply_rules(Client *c)
{
	if(def.tagrules.string)
		match_tags(c, c->props);

	if(!strlen(c->tags))
		apply_tags(c, "nil");
}

char *
message_client(Client *c, char *message)
{
	static char Ebadcmd[] = "bad command";

	if(!strncmp(message, "kill", 5)) {
		kill_client(c);
		return nil;
	}
	return Ebadcmd;
}
