/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

#include "wm.h"

static void max_client(void *obj, char *arg);

/* action table for /?/ namespace */
Action client_acttbl[] = {
    {"max", max_client},
    {0, 0}
};

Client *
alloc_client(Window w, XWindowAttributes *wa)
{
    XTextProperty name;
    Client *c = (Client *) cext_emallocz(sizeof(Client));
    XSetWindowAttributes fwa;
    int bw, th;
    long msize;

	/* client itself */
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

    bw = border_width(c);
    th = tab_height(c);
	c->frame.rect = c->rect;
    c->frame.rect.width += 2 * bw;
    c->frame.rect.height += bw + (th ? th : bw);
    c->frame.win = XCreateWindow(dpy, root, c->frame.rect.x, c->frame.rect.y,
						   c->frame.rect.width, c->frame.rect.height, 0,
						   DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen),
						   CWOverrideRedirect | CWBackPixmap | CWEventMask, &fwa);
	c->frame.cursor = normal_cursor;
    XDefineCursor(dpy, c->frame.win, c->frame.cursor);
    c->frame.gc = XCreateGC(dpy, c->frame.win, 0, 0);
    XSync(dpy, False);

	client = (Client **)cext_array_attach((void **)client, c, sizeof(Client *), &clientsz);

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

void
focus_client(Client *c)
{
	Page *p = page ? page[sel] : nil;
	size_t i, j;
	Client *old = sel_client();
	
	/* setup indexes */
	if(c->page != p) {
		focus_page(c->page);
		p = c->page;
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
		ungrab_client(old, AnyModifier, AnyButton);
		grab_client(old, AnyModifier, AnyButton);
    	draw_client(old);
	}
	ungrab_client(c, AnyModifier, AnyButton);
    grab_client(c, Mod1Mask, Button1);
    grab_client(c, Mod1Mask, Button3);
    XRaiseWindow(dpy, c->frame.win);
    XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
    XDefineCursor(dpy, c->win, normal_cursor);
    draw_client(c);
	XSync(dpy, False);
	/* TODO: client update */
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
	ungrab_client(c, AnyModifier, AnyButton);
    XUnmapWindow(dpy, c->win);
    set_client_state(c, WithdrawnState);
}

void
reparent_client(Client *c, Window w, int x, int y)
{
	c->attached = w == c->frame.win;
    XReparentWindow(dpy, c->win, w, x, y);
    c->ignore_unmap++;
}

void
grab_client(Client * c, unsigned long mod, unsigned int button)
{
    XGrabButton(dpy, button, mod, c->win, False,
                ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    if((mod != AnyModifier) && num_lock_mask) {
        XGrabButton(dpy, button, mod | num_lock_mask, c->win, False,
                    ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
        XGrabButton(dpy, button, mod | num_lock_mask | LockMask, c->win,
                    False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    }
}

void
ungrab_client(Client * c, unsigned long mod, unsigned int button)
{
    XUngrabButton(dpy, button, mod, c->win);
    if(mod != AnyModifier && num_lock_mask) {
        XUngrabButton(dpy, button, mod | num_lock_mask, c->win);
        XUngrabButton(dpy, button, mod | num_lock_mask | LockMask, c->win);
    }
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
	if(c->attached) {
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

void
close_client(Client * c)
{
    if(c->proto & PROTO_DEL)
        wmii_send_message(dpy, c->win, wm_protocols, wm_delete);
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
        if(c->attached)
            draw_client(c);
		/* TODO: client update */
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
    XFreeGC(dpy, c->frame.gc);
    XDestroyWindow(dpy, c->frame.win);
	cext_array_detach((void **)det, c, &detsz);
	ndet--;
    free(c);
}

/* speed reasoned function for client property change */
void
draw_client(Client *c)
{
    Draw d = { 0 };
    unsigned int tabh = tab_height(c);
    int bw = border_width(c);
    XRectangle notch;

	d.drawable = c->frame.win;
	d.font = xfont;
	d.gc = c->frame.gc;

	if(c == sel_client()) {
		d.bg = def.selbg;
		d.fg = def.selfg;
		d.border = def.selborder;
	} else {
		d.bg = def.normbg;
		d.fg = def.normfg;
		d.border = def.normborder;
	}

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

	/* draw tab */
    if(!tabh)
        return;

    d.rect.x = 0;
    d.rect.y = 0;
    d.rect.width = c->frame.rect.width;
    d.rect.height = tabh;
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
	c->page = p;

	if(p->sel)
		attach_column(c);
	else {
		Area *a = p->area[0];
		a->client = (Client **)cext_array_attach((void **)a->client, c,
						sizeof(Client *), &a->clientsz);
		a->nclient++;
	}
    map_client(c);
	XMapWindow(dpy, c->frame.win);
	focus_client(c);

	/* TODO: page update event */
}

void
detach_client(Client *c, Bool unmap)
{
	if(c->area)
		detach_column(c);
	else {
		cext_array_detach((void **)c->page->floating, c, &c->page->floatingsz);
    	if(!c->destroyed) {
        	if(!unmap) {
            	detached = (Client **)cext_array_attach((void **)detached, c,
								sizeof(Client *), &detachedsz);
            	unmap_client(c);
        	}
        	c->rect.x = c->frame.rect.x;
        	c->rect.y = c->frame.rect.y;
        	reparent_client(c, root, c->rect.x, c->rect.y);
    	}
	}
	c->page = nil;
    if(c->destroyed)
        destroy_client(c);
    focus_page(page[sel_page]);
}

Client *
sel_client_of_page(Page *p)
{
	if(p) {
		if(p->is_area) {
			Area *col = p->area[p->sel_area];
			return (col && col->client) ? col->client[col->sel] : nil;
		}
		else
			return p->floating ? p->floating[p->sel_float] : nil;
	}
	return nil;
}

Client *
sel_client()
{
	return page ? sel_client_of_page(page[sel_page]) : nil;
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
tab_height(Client * c)
{
    if(blitz_strtonum(c->file[C_TAB]->content, 0, 1))
        return xfont->ascent + xfont->descent + 4;
    return 0;
}

unsigned int
border_width(Client * c)
{
    if(blitz_strtonum(c->file[C_BORDER]->content, 0, 1))
        return BORDER_WIDTH;
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
    /* increment stuff, see chapter 4.1.2.3 of the ICCCM Manual */
    if(c->size.flags & PResizeInc) {
        int base_width = 0, base_height = 0;

        if(c->size.flags & PBaseSize) {
            base_width = c->size.base_width;
            base_height = c->size.base_height;
        } else if(c->size.flags & PMinSize) {
            /* base_{width,height} default to min_{width,height} */
            base_width = c->size.min_width;
            base_height = c->size.min_height;
        }
        /* client_width = base_width + i * c->size.width_inc for an integer i */
        c->frame.rect.width -=
            (c->frame.rect.width - 2 * bw - base_width) % c->size.width_inc;
        c->frame.rect.height -=
            (c->frame.rect.height - bw - (tabh ? tabh : bw) - base_height) % c->size.height_inc;
    }
}

void
resize_client(Client *c, XRectangle *r, XPoint *pt)
{
    unsigned int tabh = tab_height(c);
    unsigned int bw = border_width(c);

	if(c->area)
		resize_area(c, r, pt);

    /* resize if client requests special size */
    check_dimensions(c, tabh, bw);

    if(c->file[C_HANDLE_INC]->content
       && ((char *) c->file[C_HANDLE_INC]->content)[0] == '1')
        resize_incremental(c, tabh, bw);

    XMoveResizeWindow(dpy, c->frame.win, c->frame.rect.x, c->frame.rect.y,
					  c->frame.rect.width, c->frame.rect.height);

	c->rect.x = bw;
	c->rect.y = tabh ? tabh : bw;
	c->rect.width = c->frame.rect.width - 2 * bw;
	c->rect.height = c->frame.rect.height - bw - (tabh ? tabh : bw);
	XMoveResizeWindow(dpy, c->win, c->rect.x, c->rect.y, c->rect.width, c->rect.height);
	configure_client(c);
}

static void
handle_before_read_client(IXPServer * s, File *file)
{
	size_t i;
	char buf[32];

	for(i = 0; (i < clientsz) && client[i]; i++) {
		Client *c = client[i];
        if(file == c->file[C_GEOMETRY]) {
            snprintf(buf, sizeof(buf), "%d %d %d %d", c->frame.rect.x, c->frame.rect.y,
                     c->frame.rect.width, c->frame.rect.height);
            if(file->content)
                free(file->content);
            file->content = cext_estrdup(buf);
            file->size = strlen(buf);
			return;
        }
	    else if(file == c->file[C_NAME]) {
            if(file->content)
                free(file->content);
			file->content = cext_estrdup(c->name);
			file->size = strlen(c->name);
            return ;
		}
	}
}

static void
handle_after_write_client(IXPServer *s, File *file)
{
	size_t i;

	for(i = 0; (i < clientsz) && client[i]; i++) {
		Client *c = client[i];
		if(file == c->file[C_CTL]) {
            run_action(file, c, client_acttbl);
            return;
		}
		else if(file == c->file[C_TAB] || file == c->file[C_BORDER]
           || file == c->file[C_HANDLE_INC])
		{
			resize_client(c, &c->frame.rect, nil);
            return;
        }
		else if(file == c->file[C_GEOMETRY]) {
            char *geom = c->file[C_GEOMETRY]->content;
            if(geom && strrchr(geom, ' ')) {
                XRectangle frect = c->frame.rect;
                blitz_strtorect(&rect, &frect, geom);
                resize_client(c, &frect, 0);
            }
            return;
        }
	}
}

static void
max_client(void *obj, char *arg)
{
	Client *c = obj;

	if(c->maximized) {
		/* XXX: do we really need this ? */ c->frame.rect = c->frame.revert;
		resize_client(c, &c->frame.revert, nil);
	}
	else {
		c->frame.revert = c->frame.rect;
		c->frame.rect = c->area ? c->area->rect : rect;
		XRaiseWindow(dpy, c->frame.win);
		resize_client(c, &c->frame.rect, nil);
	}
	c->maximized = !c->maximized;
}

