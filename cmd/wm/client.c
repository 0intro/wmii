/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

#include "wm.h"

static void handle_before_read_client(IXPServer * s, File * file);
static void handle_after_write_client(IXPServer * s, File * file);

static void max_client(void *obj, char *arg);

/* action table for /?/ namespace */
Action client_acttbl[] = {
    {"max", max_client},
    {0, 0}
};

Client **
attach_client_to_array(Client *c, Client **array, size_t *size)
{
	size_t i;
	if(!array) {
		*size = 2;
		array = cext_emallocz(sizeof(Client *) * (*size));
	}
	for(i = 0; (i < (*size)) && array[i]; i++);
	if(i >= (*size)) {
		Client **tmp = array;
		(*size) *= 2;
		array = cext_emallocz(sizeof(Client *) * (*size));
		for(i = 0; tmp[i]; i++)
			array[i] = tmp[i];
		free(tmp);
	}
	array[i] = c;
	return array;
}

void
detach_client_from_array(Client *c, Client **array)
{
	size_t i;
	for(i = 0; array[i] != c; i++);
	for(; array[i + 1]; i++)
		array[i] = array[i + 1];
	array[i] = nil;
}

Client *
alloc_client(Window w, XWindowAttributes *wa)
{
    XTextProperty name;
    Client *c = (Client *) cext_emallocz(sizeof(Client));
    XSetWindowAttributes fwa;
    static int id = 1;
    char buf[MAX_BUF];
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

    snprintf(buf, MAX_BUF, "/detached/%d", id);
    c->file[C_PREFIX] = ixp_create(ixps, buf);
    snprintf(buf, MAX_BUF, "/detached/%d/name", id);
    c->file[C_NAME] = ixp_create(ixps, buf);
	c->file[C_NAME]->before_read = handle_before_read_client;
    snprintf(buf, MAX_BUF, "/detached/%d/border", id);
    c->file[C_BORDER] =
        wmii_create_ixpfile(ixps, buf, def[WM_BORDER]->content);
    c->file[C_BORDER]->after_write = handle_after_write_client;
    snprintf(buf, MAX_BUF, "/detached/%d/tab", id);
    c->file[C_TAB] = wmii_create_ixpfile(ixps, buf, def[WM_TAB]->content);
    c->file[C_TAB]->after_write = handle_after_write_client;
    snprintf(buf, MAX_BUF, "/detached/%d/handleinc", id);
    c->file[C_HANDLE_INC] =
        wmii_create_ixpfile(ixps, buf, def[WM_HANDLE_INC]->content);
    c->file[C_HANDLE_INC]->after_write = handle_after_write_client;
    snprintf(buf, MAX_BUF, "/detached/%d/geometry", id);
    c->file[C_GEOMETRY] = ixp_create(ixps, buf);
    c->file[C_GEOMETRY]->before_read = handle_before_read_client;
    c->file[C_GEOMETRY]->after_write = handle_after_write_client;
    snprintf(buf, MAX_BUF, "/detached/%d/ctl", id);
    c->file[C_CTL] = ixp_create(ixps, buf);
    c->file[C_CTL]->after_write = handle_after_write_client;
    id++;

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

	client = attach_client_to_array(c, client, &clientsz);

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
	Page *p = page ? page[sel_page] : nil;
	size_t i, j;
	Client *old = sel_client();
	
	/* setup indexes */
	if(c->page != p) {
		focus_page(c->page);
		p = c->page;
	}
	p->is_column = c->column != nil;
	p->file[P_SEL_PREFIX]->content = c->file[P_PREFIX]->content;
	if(p->is_column) {
		for(i = 0; (i < p->columnsz) && p->column[i]; i++) {
			Column *col = p->column[i];
			for(j = 0; (j < col->clientsz) && col->client[j] && (c != col->client[j]); j++);
			if((j < col->clientsz) && col->client[j]) {
				p->sel_column = i;
				col->sel = j;
				break;
			}
		}
	}
	else {
		for(i = 0; (i < p->floatingsz) && p->floating[i] && (p->floating[i] != c); i++);
		if((i < p->floatingsz) && p->floating[i])
			p->sel_float = i;
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
    invoke_wm_event(def[WM_EVENT_CLIENT_UPDATE]);
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
        invoke_wm_event(def[WM_EVENT_CLIENT_UPDATE]);
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
    ixp_remove_file(ixps, c->file[C_PREFIX]);
	detach_client_from_array(c, detached);
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
	d.font = font;
	d.gc = c->frame.gc;

	if(c == sel_client()) {
		d.bg = blitz_loadcolor(dpy, screen, def[WM_SEL_BG_COLOR]->content);
		d.fg = blitz_loadcolor(dpy, screen, def[WM_SEL_FG_COLOR]->content);
		d.border = blitz_loadcolor(dpy, screen, def[WM_SEL_BORDER_COLOR]->content);
	} else {
		d.bg = blitz_loadcolor(dpy, screen, def[WM_NORM_BG_COLOR]->content);
		d.fg = blitz_loadcolor(dpy, screen, def[WM_NORM_FG_COLOR]->content);
		d.border = blitz_loadcolor(dpy, screen, def[WM_NORM_BORDER_COLOR]->content);
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
		p = page[sel_page];

    reparent_client(c, c->frame.win, c->rect.x, c->rect.y);
	c->page = p;
	wmii_move_ixpfile(c->file[C_PREFIX], p->file[P_CLIENT_PREFIX]);

	if(p->is_column)
		attach_column(c);
	else
		p->floating = attach_client_to_array(c, p->floating, &p->floatingsz);
    map_client(c);
	XMapWindow(dpy, c->frame.win);
	focus_client(c);

    invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
}

void
detach_client(Client *c, Bool unmap)
{
	wmii_move_ixpfile(c->file[C_PREFIX], def[WM_DETACHED_PREFIX]);
	if(c->column)
		detach_column(c);
	else {
		detach_client_from_array(c, c->page->floating);
    	if(!c->destroyed) {
        	if(!unmap) {
            	detached = attach_client_to_array(c, detached, &detachedsz);
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
		if(p->is_column) {
			Column *col = p->column[p->sel_column];
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
        return font->ascent + font->descent + 4;
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

	if(c->column)
		resize_column(c, r, pt);

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
		c->frame.rect = c->column ? c->column->rect : rect;
		XRaiseWindow(dpy, c->frame.win);
		resize_client(c, &c->frame.rect, nil);
	}
	c->maximized = !c->maximized;
}

