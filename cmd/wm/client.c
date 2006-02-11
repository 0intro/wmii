/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

#include "wm.h"

/*
static void max_client(void *obj, char *arg);
*/

/* action table for /?/ namespace */
/*
Action client_acttbl[] = {
    {"max", max_client},
    {0, 0}
};
*/

Client *
alloc_client(Window w, XWindowAttributes *wa)
{
    XTextProperty name;
    Client *c = (Client *) cext_emallocz(sizeof(Client));
    XSetWindowAttributes fwa;
    int bw = def.border, bh;
    long msize;
	static unsigned short id = 1;

	/* client itself */
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
    XGetWMName(dpy, c->win, &name);
	if(name.value) {
		cext_strlcpy(c->name, (char *)name.value, sizeof(c->name));
    	free(name.value);
	}

	/* client.frame */
    fwa.override_redirect = 1;
    fwa.background_pixmap = ParentRelative;
	fwa.event_mask = SubstructureRedirectMask | ExposureMask | ButtonPressMask | PointerMotionMask;

	c->frame.bar = def.bar;
	c->frame.border = bw;
    bh = bar_height(c);
	c->frame.rect = c->rect;
    c->frame.rect.width += 2 * bw;
    c->frame.rect.height += bw + (bh ? bh : bw);
    c->frame.win = XCreateWindow(dpy, root, c->frame.rect.x, c->frame.rect.y,
						   c->frame.rect.width, c->frame.rect.height, 0,
						   DefaultDepth(dpy, screen), CopyFromParent,
						   DefaultVisual(dpy, screen),
						   CWOverrideRedirect | CWBackPixmap | CWEventMask, &fwa);
	c->frame.cursor = normal_cursor;
    XDefineCursor(dpy, c->frame.win, c->frame.cursor);
    c->frame.gc = XCreateGC(dpy, c->frame.win, 0, 0);
    XSync(dpy, False);

	client = (Client **)cext_array_attach((void **)client, c, sizeof(Client *), &clientsz);
	nclient++;

    return c;
}

void
set_client_state(Client * c, int state)
{
    long data[2];

    data[0] = (long) state;
    data[1] = (long) None;
    XChangeProperty(dpy, c->win, wm_state, wm_state, 32, PropModeReplace,
                    (unsigned char *) data, 2);
}

static void
client_name_event(Client *c)
{
	char buf[256];
	snprintf(buf, sizeof(buf), "CN %s\n", c->name);
	broadcast_event(buf);
}

void
focus_client(Client *c)
{
	Page *p = page ? page[sel] : nil;
	size_t i, j;
	Client *old = sel_client();
	
	/* setup indexes */
	if(c->area->page != p) {
		focus_page(c->area->page);
		p = c->area->page;
	}
	for(i = 0; i < p->narea; i++) {
		Area *a = p->area[i];
		for(j = 0; j < a->nclient && (c != a->client[j]); j++);
		if(j < a->nclient) {
			p->sel = i;
			a->sel = j;
			break;
		}
	}
	
	if(old && (old != c)) {
		c->revert = old;
		grab_mouse(old->win, AnyModifier, Button1);
    	draw_client(old);
	}
	ungrab_mouse(c->win, AnyModifier, AnyButton);
    grab_mouse(c->win, Mod1Mask, Button1);
    grab_mouse(c->win, Mod1Mask, Button3);
    XRaiseWindow(dpy, c->frame.win);
    XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
    XDefineCursor(dpy, c->win, normal_cursor);
    draw_client(c);
	XSync(dpy, False);
	client_name_event(c);
}

void
map_client(Client * c)
{
    XMapRaised(dpy, c->win);
    set_client_state(c, NormalState);
}

void
unmap_client(Client * c)
{
	ungrab_mouse(c->win, AnyModifier, AnyButton);
    XUnmapWindow(dpy, c->win);
    set_client_state(c, WithdrawnState);
}

void
reparent_client(Client *c, Window w, int x, int y)
{
    XReparentWindow(dpy, c->win, w, x, y);
    c->ignore_unmap++;
}

void
configure_client(Client * c)
{
    XConfigureEvent e;
    e.type = ConfigureNotify;
    e.event = c->win;
    e.window = c->win;
    e.x = c->rect.x;
    e.y = c->rect.y;
	if(c->area) {
    	e.x += c->frame.rect.x;
    	e.y += c->frame.rect.y;
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
close_client(Client * c)
{
    if(c->proto & PROTO_DEL)
        send_client_message(c->win, wm_protocols, wm_delete);
    else
        XKillClient(dpy, c->win);
}

void
handle_client_property(Client *c, XPropertyEvent *e)
{
    XTextProperty name;
    long msize;

    if(e->atom == wm_protocols) {
        /* update */
        c->proto = win_proto(c->win);
        return;
    }
    switch (e->atom) {
    case XA_WM_NAME:
        XGetWMName(dpy, c->win, &name);
        if(name.value) {
			cext_strlcpy(c->name, (char*) name.value, sizeof(c->name));
        	free(name.value);
		}
        if(c->area)
            draw_client(c);
		client_name_event(c);
        break;
    case XA_WM_TRANSIENT_FOR:
        XGetTransientForHint(dpy, c->win, &c->trans);
        break;
    case XA_WM_NORMAL_HINTS:
        if(!XGetWMNormalHints(dpy, c->win, &c->size, &msize)
           || !c->size.flags) {
            c->size.flags = PSize;
        }
        break;
    }
}

void
destroy_client(Client * c)
{
	size_t i;
	for(i = 0; i < ndet; i++)
		if(det[i] == c) {
			cext_array_detach((void **)det, c, &detsz);
			ndet--;
			break;
		}
    XFreeGC(dpy, c->frame.gc);
    XDestroyWindow(dpy, c->frame.win);
	cext_array_detach((void **)client, c, &clientsz);
	nclient--;
    free(c);
}

/* speed reasoned function for client property change */
void
draw_client(Client *c)
{
    Draw d = { 0 };
    unsigned int bh = bar_height(c);
    unsigned int bw = c->frame.border;
    XRectangle notch;

	d.align = WEST;
	d.drawable = c->frame.win;
	d.font = xfont;
	d.gc = c->frame.gc;

	if(c == sel_client())
		d.color = def.sel;
	else
		d.color = def.norm;

	/* draw border */
    if(bw) {
        notch.x = bw;
        notch.y = bw;
        notch.width = c->frame.rect.width - 2 * bw;
        notch.height = c->frame.rect.height - 2 * bw;
        d.rect = c->frame.rect;
        d.rect.x = d.rect.y = 0;
        d.notch = &notch;

        blitz_drawlabel(dpy, &d);
    }
    XSync(dpy, False);

	/* draw bar */
    if(!bh)
        return;

    d.rect.x = 0;
    d.rect.y = 0;
    d.rect.width = c->frame.rect.width;
    d.rect.height = bh;
	d.notch = nil;
    d.data = c->name;
    blitz_drawlabel(dpy, &d);
    XSync(dpy, False);
}

void
gravitate(Client * c, unsigned int tabh, unsigned int bw, int invert)
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
    default:                   /* don't care */
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
    default:                   /* don't care */
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
attach_client(Client *c)
{
	Page *p;
    if(!page)
		p = alloc_page();
	else
		p = page[sel];

    reparent_client(c, c->frame.win, c->rect.x, c->rect.y);

	if(p->sel)
		attach_column(c);
	else {
		Area *a = p->area[0];
		a->client = (Client **)cext_array_attach((void **)a->client, c,
						sizeof(Client *), &a->clientsz);
		a->nclient++;
		c->area = a;
	}
	resize_client(c, &c->frame.rect, nil);
    map_client(c);
	XMapWindow(dpy, c->frame.win);
	focus_client(c);
}

void
detach_client(Client *c, Bool unmap)
{
	if(c->area) {
		size_t i;
		for(i = 0; i < nclient; i++)
			if(client[i]->revert == c)
				client[i]->revert = nil;

		if(area_to_index(c->area) > 0)
			detach_column(c);
		else {
			Area *a = c->area;
			cext_array_detach((void **)a->client, c, &a->clientsz);
			a->nclient--;
			if(!c->destroyed) {
				if(!unmap) {
					det = (Client **)cext_array_attach((void **)det, c,
							sizeof(Client *), &detsz);
					ndet++;
					unmap_client(c);
				}
				c->rect.x = c->frame.rect.x;
				c->rect.y = c->frame.rect.y;
				reparent_client(c, root, c->rect.x, c->rect.y);
			}
		}
	}
	c->area = nil;
	if(c->revert)
		focus_client(c->revert);
    if(c->destroyed)
        destroy_client(c);
}

Client *
sel_client_of_page(Page *p)
{
	if(p) {
		Area *a = p->narea ? p->area[p->sel] : nil;
		return (a && a->nclient) ? a->client[a->sel] : nil;
	}
	return nil;
}

Client *
sel_client()
{
	return npage ? sel_client_of_page(page[sel]) : nil;
}

Client *
win_to_frame(Window w)
{
	size_t i;
	for(i = 0; (i < clientsz) && client[i]; i++)
		if(client[i]->frame.win == w)
			return client[i];
	return nil;
}

unsigned int
bar_height(Client * c)
{
    if(c->frame.bar)
        return xfont->ascent + xfont->descent + 4;
    return 0;
}

static void
check_dimensions(Client *c, unsigned int tabh, unsigned int bw)
{
    if(c->size.flags & PMinSize) {
        if(c->frame.rect.width - 2 * bw < c->size.min_width)
            c->frame.rect.width = c->size.min_width + 2 * bw;
        if(c->frame.rect.height - bw - (tabh ? tabh : bw) < c->size.min_height)
            c->frame.rect.height = c->size.min_height + bw + (tabh ? tabh : bw);
    }
    if(c->size.flags & PMaxSize) {
        if(c->frame.rect.width - 2 * bw > c->size.max_width)
            c->frame.rect.width = c->size.max_width + 2 * bw;
        if(c->frame.rect.height - bw - (tabh ? tabh : bw) > c->size.max_height)
            c->frame.rect.height = c->size.max_height + bw + (tabh ? tabh : bw);
    }
}

static void
resize_incremental(Client *c, unsigned int tabh, unsigned int bw)
{
    XSizeHints *s = &c->size;

    /* increment stuff, see chapter 4.1.2.3 of the ICCCM Manual */
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
        w = c->frame.rect.width - 2 * bw - w;
        if(s->width_inc > 0)
            c->frame.rect.width -= w % s->width_inc;

        h = c->frame.rect.height - bw - (tabh ? tabh : bw) - h;
        if(s->height_inc > 0)
            c->frame.rect.height -= h % s->height_inc;
    }
}

void
resize_client(Client *c, XRectangle *r, XPoint *pt)
{
    unsigned int bh = bar_height(c);
    unsigned int bw = c->frame.border;

	if(area_to_index(c->area) > 0)
		resize_column(c, r, pt);
	else
		c->frame.rect = *r;

    /* resize if client requests special size */
    check_dimensions(c, bh, bw);

    if(def.inc)
    	resize_incremental(c, bh, bw);

    XMoveResizeWindow(dpy, c->frame.win, c->frame.rect.x, c->frame.rect.y,
					  c->frame.rect.width, c->frame.rect.height);

	c->rect.x = bw;
	c->rect.y = bh ? bh : bw;
	c->rect.width = c->frame.rect.width - 2 * bw;
	c->rect.height = c->frame.rect.height - bw - (bh ? bh : bw);
	XMoveResizeWindow(dpy, c->win, c->rect.x, c->rect.y, c->rect.width, c->rect.height);
	configure_client(c);
}

/*
static void
max_client(void *obj, char *arg)
{
	Client *c = obj;

	if(c->maximized) {
		c->frame.rect = c->frame.revert;
		resize_client(c, &c->frame.revert, nil);
	}
	else {
		c->frame.revert = c->frame.rect;
		c->frame.rect = c->area->rect;
		XRaiseWindow(dpy, c->frame.win);
		resize_client(c, &c->frame.rect, nil);
	}
	c->maximized = !c->maximized;
}
*/

int
cid_to_index(Area *a, unsigned short id)
{
	int i;
	for(i = 0; i < a->nclient; i++)
		if(a->client[i]->id == id)
			return i;
	return -1;
}

int
client_to_index(Client *c)
{
	int i;
	Area *a = c->area;
	for(i = 0; i < a->nclient; i++)
		if(a->client[i] == c)
			return i;
	return -1;
}
