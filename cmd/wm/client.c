/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

#include "wm.h"

Client *alloc_client(Window w)
{
	static int id = 0;
	char buf[MAX_BUF];
	char buf2[MAX_BUF];
	Client *c = (Client *) cext_emallocz(sizeof(Client));

	c->win = w;
	snprintf(buf, MAX_BUF, "/detached/c/%d", id);
	c->file[C_PREFIX] = ixp_create(ixps, buf);
	win_prop(dpy, c->win, XA_WM_NAME, buf2, MAX_BUF);
	snprintf(buf, MAX_BUF, "/detached/c/%d/name", id);
	c->file[C_NAME] = wmii_create_ixpfile(ixps, buf, buf2);
	id++;
	cext_attach_item(&clients, c);
	XSelectInput(dpy, c->win, CLIENT_MASK);
	return c;
}

void sel_client(Client * c)
{
	Frame *f = 0;
	/* sel client */
	f = c->frame;
	cext_top_item(&f->clients, c);
	f->file[F_SEL_CLIENT]->content = c->file[C_PREFIX]->content;
	XRaiseWindow(dpy, c->win);
	XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
	invoke_wm_event(def[WM_EVENT_CLIENT_UPDATE]);
}

void set_client_state(Client * c, int state)
{
	long data[2];

	data[0] = (long) state;
	data[1] = (long) None;
	XChangeProperty(dpy, c->win, wm_state, wm_state, 32,
					PropModeReplace, (unsigned char *) data, 2);
}

void show_client(Client * c)
{
	XSelectInput(dpy, c->win, CLIENT_MASK & ~StructureNotifyMask);
	XMapWindow(dpy, c->win);
	XSelectInput(dpy, c->win, CLIENT_MASK);
	set_client_state(c, NormalState);
	grab_client(c, Mod1Mask, Button1);
	grab_client(c, Mod1Mask, Button3);
}

void hide_client(Client * c)
{
	ungrab_client(c, AnyModifier, AnyButton);
	XSelectInput(dpy, c->win, CLIENT_MASK & ~StructureNotifyMask);
	XUnmapWindow(dpy, c->win);
	XSelectInput(dpy, c->win, CLIENT_MASK);
	set_client_state(c, WithdrawnState);
}

void reparent_client(Client * c, Window w, int x, int y)
{
	XSelectInput(dpy, c->win, CLIENT_MASK & ~StructureNotifyMask);
	XReparentWindow(dpy, c->win, w, x, y);
	XSelectInput(dpy, c->win, CLIENT_MASK);
}

void grab_client(Client * c, unsigned long mod, unsigned int button)
{
	XSelectInput(dpy, c->win, CLIENT_MASK & ~StructureNotifyMask);
	XGrabButton(dpy, button, mod, c->win, False,
				ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
	if ((mod != AnyModifier) && num_lock_mask) {
		XGrabButton(dpy, button, mod | num_lock_mask, c->win,
					False, ButtonPressMask, GrabModeAsync, GrabModeAsync,
					None, None);
		XGrabButton(dpy, button, mod | num_lock_mask | LockMask,
					c->win, False, ButtonPressMask, GrabModeAsync,
					GrabModeAsync, None, None);
	}
	XSelectInput(dpy, c->win, CLIENT_MASK);
	XSync(dpy, False);
}

void ungrab_client(Client * c, unsigned long mod, unsigned int button)
{
	XSelectInput(dpy, c->win, CLIENT_MASK & ~StructureNotifyMask);
	XUngrabButton(dpy, button, mod, c->win);
	if (mod != AnyModifier && num_lock_mask) {
		XUngrabButton(dpy, button, mod | num_lock_mask, c->win);
		XUngrabButton(dpy, button, mod | num_lock_mask | LockMask, c->win);
	}
	XSelectInput(dpy, c->win, CLIENT_MASK);
	XSync(dpy, False);
}

void configure_client(Client * c)
{
	XConfigureEvent e;
	e.type = ConfigureNotify;
	e.event = c->win;
	e.window = c->win;
	e.x = c->rect.x;
	e.y = c->rect.y;
	if (c->frame) {
		e.x += c->frame->rect.x;
		e.y += c->frame->rect.y;
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

void close_client(Client * c)
{
	if (c->proto & PROTO_DEL)
		send_message(dpy, c->win, wm_protocols, wm_delete);
	else
		XKillClient(dpy, c->win);
}

void init_client(Client * c, XWindowAttributes * wa)
{
	long msize;
	c->rect.x = wa->x;
	c->rect.y = wa->y;
	c->border = wa->border_width;
	c->rect.width = wa->width + 2 * c->border;
	c->rect.height = wa->height + 2 * c->border;
	XSetWindowBorderWidth(dpy, c->win, 0);
	XSelectInput(dpy, c->win, PropertyChangeMask);
	c->proto = win_proto(c->win);
	XGetTransientForHint(dpy, c->win, &c->trans);
	/* size hints */
	if (!XGetWMNormalHints(dpy, c->win, &c->size, &msize)
		|| !c->size.flags)
		c->size.flags = PSize;
	XAddToSaveSet(dpy, c->win);
}

void handle_client_property(Client * c, XPropertyEvent * e)
{
	char buf[1024];
	long msize;

	buf[0] = '\0';
	if (e->state == PropertyDelete)
		return;					/* ignore */

	if (e->atom == wm_protocols) {
		/* update */
		c->proto = win_proto(c->win);
		return;
	}
	switch (e->atom) {
	case XA_WM_NAME:
		win_prop(dpy, c->win, XA_WM_NAME, buf, sizeof(buf));
		if (strlen(buf)) {
			if (c->file[C_NAME]->content)
				free(c->file[C_NAME]->content);
			c->file[C_NAME]->content = cext_estrdup(buf);
			c->file[C_NAME]->size = strlen(buf);
		}
		if (c->frame)
			draw_client(c, nil);
		invoke_wm_event(def[WM_EVENT_CLIENT_UPDATE]);
		break;
	case XA_WM_TRANSIENT_FOR:
		XGetTransientForHint(dpy, c->win, &c->trans);
		break;
	case XA_WM_NORMAL_HINTS:
		if (!XGetWMNormalHints(dpy, c->win, &c->size, &msize)
			|| !c->size.flags) {
			c->size.flags = PSize;
		}
		break;
	}
}

void destroy_client(Client * c)
{
	cext_detach_item(&clients, c);
	ixp_remove_file(ixps, c->file[C_PREFIX]);
	if (ixps->errstr)
		fprintf(stderr, "wmiiwm: destroy_client(): %s\n", ixps->errstr);
	free(c);
}

/* speed reasoned function for client property change */
void draw_client(void *item, void *aux)
{
	Client *c = item;
	Frame *f = c->frame;
	unsigned int tw, tabh = tab_height(f);
	size_t size;
	int i;

	if (!tabh)
		return;
	size = cext_sizeof(&f->clients);
	tw = f->rect.width;
	if (size)
		tw /= size;
	i = cext_get_item_index(&f->clients, c);
	if (i < size - 1)
		draw_tab(f, c->file[C_NAME]->content, i * tw, 0, f->rect.width - (i * tw), tabh,
				 (f == get_sel_frame()) && (c == get_sel_client()));
	else
		draw_tab(f, c->file[C_NAME]->content, i * tw, 0, tw, tabh,
				 (f == get_sel_frame()) && (c == get_sel_client()));
	XSync(dpy, False);
}

void draw_clients(Frame * f)
{
	cext_iterate(&f->clients, 0, draw_client);
}

void gravitate(Client * c, unsigned int tabh, unsigned int bw, int invert)
{
	int dx = 0, dy = 0;
	int gravity = NorthWestGravity;

	if (c->size.flags & PWinGravity) {
		gravity = c->size.win_gravity;
	}
	/* y */
	switch (gravity) {
	case StaticGravity:
	case NorthWestGravity:
	case NorthGravity:
	case NorthEastGravity:
		dy = tabh;
		break;
	case EastGravity:
	case CenterGravity:
	case WestGravity:
		dy = -(c->rect.height / 2) + tabh;
		break;
	case SouthEastGravity:
	case SouthGravity:
	case SouthWestGravity:
		dy = -c->rect.height;
		break;
	default:					/* don't care */
		break;
	}

	/* x */
	switch (gravity) {
	case StaticGravity:
	case NorthWestGravity:
	case WestGravity:
	case SouthWestGravity:
		dx = bw;
		break;
	case NorthGravity:
	case CenterGravity:
	case SouthGravity:
		dx = -(c->rect.width / 2) + bw;
		break;
	case NorthEastGravity:
	case EastGravity:
	case SouthEastGravity:
		dx = -(c->rect.width + bw);
		break;
	default:					/* don't care */
		break;
	}

	if (invert) {
		dx = -dx;
		dy = -dy;
	}
	c->rect.x += dx;
	c->rect.y += dy;
}

void attach_client(Client * c)
{
	Area *a = 0;
	Frame *old = get_sel_frame();
	if (!cext_sizeof(&pages))
		alloc_page();
	/* transient stuff */
	a = get_sel_area();
	if (c && c->trans) {
		Client *t = win_to_client(c->trans);
		if (t && t->frame)
			a = t->frame->area;
	}
	a->layout->attach(a, c);
	if (old)
		draw_frame(old, nil);
	invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
}

void detach_client(Client *c) {
	if (c->frame)
		c->frame->area->layout->detach(c->frame->area, c);
	if (c->destroyed)
		destroy_client(c);
	sel_page(get_sel_page());
}

Client *get_sel_client()
{
	Frame *f = get_sel_frame();
	return f ? cext_get_top_item(&f->clients) : nil;
}
