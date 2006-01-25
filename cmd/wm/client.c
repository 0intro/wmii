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

void
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
    id++;

	/* client.frame */
    fwa.override_redirect = 1;
    fwa.background_pixmap = ParentRelative;
	fwa.event_mask = SubstructureRedirectMask | ExposureMask | ButtonPressMask | PointerMotionMask;

    bw = border_width(c);
    th = tab_height(c);
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

	attach_client_to_array(c, clients, &clientssz);

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

/*
void
focus_client(Client *new, Client *old)
{
	if(old && (old != new)) {
		ungrab_client(old, AnyModifier, AnyButton);
		grab_client(old, AnyModifier, AnyButton);
    	draw_frame(old->frame);
	}
	ungrab_client(new, AnyModifier, AnyButton);
    grab_client(new, Mod1Mask, Button1);
    grab_client(new, Mod1Mask, Button3);
    XRaiseWindow(dpy, new->win);
    XSetInputFocus(dpy, new->win, RevertToPointerRoot, CurrentTime);
    XDefineCursor(dpy, new->win, normal_cursor);
    draw_frame(new->frame);
    invoke_wm_event(def[WM_EVENT_CLIENT_UPDATE]);
	XSync(dpy, False);
}
*/

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
	c->framed = w == c->frame.win;
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
	if(c->framed) {
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
        if(c->framed)
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
    detach_detached(c);
    free(c);
}

/* speed reasoned function for client property change */
void
draw_client(Client *c)
{
    unsigned int tabh = tab_height(c);
    Draw d = { 0 };

    if(!tabh)
        return;

    d.drawable = c->frame.win;
    d.gc = c->frame.gc;
    d.rect.x = 0;
    d.rect.y = 0;
    d.rect.width = c->frame.rect.width;
    d.rect.height = tabh;
    d.data = c->name;
    d.font = font;

    if(c == sel_client()) {
        d.bg = blitz_loadcolor(dpy, screen, def[WM_SEL_BG_COLOR]->content);
        d.fg = blitz_loadcolor(dpy, screen, def[WM_SEL_FG_COLOR]->content);
        d.border = blitz_loadcolor(dpy, screen, def[WM_SEL_BORDER_COLOR]->content);
    } else {
        d.bg = blitz_loadcolor(dpy, screen, def[WM_NORM_BG_COLOR]->content);
        d.fg = blitz_loadcolor(dpy, screen, def[WM_NORM_FG_COLOR]->content);
        d.border = blitz_loadcolor(dpy, screen, def[WM_NORM_BORDER_COLOR]->content);
    }
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
    if(!pages)
		focus_page(alloc_page());
	p = pages[sel_page];

    /* XXX: do we need */ resize_client(c, &c->rect, 0);
    reparent_client(c, c->frame.win, c->rect.x, c->rect.y);
	c->page = p;
	c->managed = p->is_managed;

	if(c->managed)
		attach_column(c);
	else {
		attach_client_to_array(c, p->floating, &p->floatingsz);
    	map_client(c);
	}

    invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
}

void
detach_client(Client *c, Bool unmap)
{
	Client *c = sel_client();

	if(c->managed)
		detach_column(c);
	else {
		detach_client_from_array(c, c->page->floating);
    	if(!c->destroyed) {
        	if(!unmap) {
            	attach_client_to_array(c, detached, &detachedsz);
            	unmap_client(c);
        	}
        	c->rect.x = f->rect.x;
        	c->rect.y = f->rect.y;
        	reparent_client(c, root, c->rect.x, c->rect.y);
    	}
	}
	c->page = nil;
    if(c->destroyed)
        destroy_client(c);
    focus_page(sel_page);
}

Client *
sel_client()
{
	Layout *l = sel_layout();
	return l ? l->def->sel(l) : nil;
}

Client *
clientat(Client * clients, size_t idx)
{
    size_t i = 0;
    Client *c = clients;
    for(; (i != idx) && c; c = c->next)
        i++;
    return c;
}

Client *
alloc_frame(XRectangle * r)
{
    XSetWindowAttributes wa;
    static int id = 1;
    char buf[MAX_BUF];
    Client *f = (Client *) cext_emallocz(sizeof(Client));
    int bw, th;

    f->rect = *r;
    f->cursor = normal_cursor;

    snprintf(buf, MAX_BUF, "/detached/%d", id);
    f->file[F_PREFIX] = ixp_create(ixps, buf);
    snprintf(buf, MAX_BUF, "/detached/%d/name", id);
    f->file[F_NAME] = ixp_create(ixps, buf);
	f->file[F_NAME]->before_read = handle_before_read_frame;
    snprintf(buf, MAX_BUF, "/detached/%d/border", id);
    f->file[F_BORDER] =
        wmii_create_ixpfile(ixps, buf, def[WM_BORDER]->content);
    f->file[F_BORDER]->after_write = handle_after_write_frame;
    snprintf(buf, MAX_BUF, "/detached/%d/tab", id);
    f->file[F_TAB] = wmii_create_ixpfile(ixps, buf, def[WM_TAB]->content);
    f->file[F_TAB]->after_write = handle_after_write_frame;
    snprintf(buf, MAX_BUF, "/detached/%d/handleinc", id);
    f->file[F_HANDLE_INC] =
        wmii_create_ixpfile(ixps, buf, def[WM_HANDLE_INC]->content);
    f->file[F_HANDLE_INC]->after_write = handle_after_write_frame;
    snprintf(buf, MAX_BUF, "/detached/%d/geometry", id);
    f->file[F_GEOMETRY] = ixp_create(ixps, buf);
    f->file[F_GEOMETRY]->before_read = handle_before_read_frame;
    f->file[F_GEOMETRY]->after_write = handle_after_write_frame;
    id++;

    wa.override_redirect = 1;
    wa.background_pixmap = ParentRelative;
    wa.event_mask = SubstructureRedirectMask | ExposureMask | ButtonPressMask | PointerMotionMask;

    bw = border_width(c);
    th = tab_height(c);
    c->frame.rect.width += 2 * bw;
    c->frame.rect.height += bw + (th ? th : bw);
    c->frame.win = XCreateWindow(dpy, root, c->frame.rect.x, c->frame.rect.y,
						   c->frame.rect.width, c->frame.rect.height, 0,
						   DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen),
						   CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
	c->frame.cursor = normal_cursor;
    XDefineCursor(dpy, c->frame.win, c->frame.cursor);
    c->frame.gc = XCreateGC(dpy, c->frame.win, 0, 0);
    XSync(dpy, False);
    return f;
}

Client *
win_to_frame(Window w)
{
    Page *p;
    for(p = pages; p; p = p->next) {
        Client *f;
		if(p->managed->def)
			for(f = p->managed->def->frames(p->managed); f; f = f->next)
				if(f->win == w)
					return f;
        for(f = p->floating->def->frames(p->floating); f; f = f->next)
            if(f->win == w)
                return f;
    }
    return nil;
}

void
destroy_frame(Client * f)
{
    XFreeGC(dpy, f->gc);
    XDestroyWindow(dpy, f->win);
    ixp_remove_file(ixps, f->file[F_PREFIX]);
    free(f);
}

unsigned int
tab_height(Client * f)
{
    if(blitz_strtonum(f->file[F_TAB]->content, 0, 1))
        return font->ascent + font->descent + 4;
    return 0;
}

unsigned int
border_width(Client * f)
{
    if(blitz_strtonum(f->file[F_BORDER]->content, 0, 1))
        return BORDER_WIDTH;
    return 0;
}

static void
check_dimensions(Client * f, unsigned int tabh, unsigned int bw)
{
    Client *c = sel_client();
    if(!c)
        return;

    if(c->size.flags & PMinSize) {
        if(f->rect.width - 2 * bw < c->size.min_width) {
            f->rect.width = c->size.min_width + 2 * bw;
        }
        if(f->rect.height - bw - (tabh ? tabh : bw) < c->size.min_height) {
            f->rect.height = c->size.min_height + bw + (tabh ? tabh : bw);
        }
    }
    if(c->size.flags & PMaxSize) {
        if(f->rect.width - 2 * bw > c->size.max_width) {
            f->rect.width = c->size.max_width + 2 * bw;
        }
        if(f->rect.height - bw - (tabh ? tabh : bw) > c->size.max_height) {
            f->rect.height = c->size.max_height + bw + (tabh ? tabh : bw);
        }
    }
}

static void
resize_incremental(Client * f, unsigned int tabh, unsigned int bw)
{
    Client *c = f->sel;
    if(!c)
        return;
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
        f->rect.width -=
            (f->rect.width - 2 * bw - base_width) % c->size.width_inc;
        f->rect.height -=
            (f->rect.height - bw - (tabh ? tabh : bw) -
             base_height) % c->size.height_inc;
    }
}

void
resize_client(Client * f, XRectangle * r, XPoint * pt)
{
    Layout *l = f->layout;
    unsigned int tabh = tab_height(f);
    unsigned int bw = border_width(f);
    Client *c = f->client;

    l->def->resize(f, r, pt);

    /* resize if client requests special size */
    check_dimensions(f, tabh, bw);

    if(f->file[F_HANDLE_INC]->content
       && ((char *) f->file[F_HANDLE_INC]->content)[0] == '1')
        resize_incremental(f, tabh, bw);

    XMoveResizeWindow(dpy, f->win, f->rect.x, f->rect.y, f->rect.width,
                      f->rect.height);

	if(f->client) {
		c->rect.x = bw;
		c->rect.y = tabh ? tabh : bw;
		c->rect.width = c->frame->rect.width - 2 * bw;
		c->rect.height = c->frame->rect.height - bw - (tabh ? tabh : bw);
		XMoveResizeWindow(dpy, c->win, c->rect.x, c->rect.y, c->rect.width,
				c->rect.height);
		configure_client(c);
	}
}

/**
 * Assumes following file:
 *
 * ./sel-style/text-font	   "<value>"
 * ./sel-style/text-size	   "<int>"
 * ./sel-style/text-color	  "#RRGGBBAA"
 * ./sel-style/bg-color		"#RRGGBBAA"
 * ./sel-style/border-color	"#RRGGBBAA [#RRGGBBAA [#RRGGBBAA [#RRGGBBAA]]]"
 * ./norm-style/text-font	   "<value>"
 * ./norm-style/text-size	   "<int>"
 * ./norm-style/text-color	  "#RRGGBBAA"
 * ./norm-style/bg-color		"#RRGGBBAA"
 * ./norm-style/border-color	"#RRGGBBAA [#RRGGBBAA [#RRGGBBAA [#RRGGBBAA]]]"
 */
void
draw_frame(Client * f)
{
    Draw d = { 0 };
    int bw = border_width(f);
    XRectangle notch;
    if(bw) {
        notch.x = bw;
        notch.y = bw;
        notch.width = f->rect.width - 2 * bw;
        notch.height = f->rect.height - 2 * bw;
        d.drawable = f->win;
        d.font = font;
        d.gc = f->gc;

        /* define ground plate (i = 0) */
        if(f == sel_frame()) {
            d.bg = blitz_loadcolor(dpy, screen, def[WM_SEL_BG_COLOR]->content);
            d.fg = blitz_loadcolor(dpy, screen, def[WM_SEL_FG_COLOR]->content);
            d.border = blitz_loadcolor(dpy, screen, def[WM_SEL_BORDER_COLOR]->content);
        } else {
            d.bg = blitz_loadcolor(dpy, screen, def[WM_NORM_BG_COLOR]->content);
            d.fg = blitz_loadcolor(dpy, screen, def[WM_NORM_FG_COLOR]->content);
            d.border = blitz_loadcolor(dpy, screen, def[WM_NORM_BORDER_COLOR]->content);
        }
        d.rect = f->rect;
        d.rect.x = d.rect.y = 0;
        d.notch = &notch;

        blitz_drawlabel(dpy, &d);
    }
    draw_client(f->client);
    XSync(dpy, False);
}

void
handle_frame_buttonpress(XButtonEvent * e, Client * f)
{
    Align align;
    int bindex;
	Layout *l = sel_layout();
	if(l != f->layout)
		focus_layout(f->layout);
	l = f->layout;
	l->def->focus(l, f->client, False);
    if(e->button == Button1) {
        align = cursor_to_align(f->cursor);
        if(align == CENTER)
            mouse_move(f);
        else
            mouse_resize(f, align);
        return;
    }
    bindex = WM_EVENT_B2PRESS - 2 + e->button;
    /* frame mouse handling */
    if(def[bindex]->content)
        wmii_spawn(dpy, def[bindex]->content);
}

void
detach_client_from_frame(Client * c, Bool unmap)
{
    Client *f = c->frame;

    c->frame = nil;
    if(f->sel == c)
		f->sel = nil;

	f->client = nil;

    if(!c->destroyed) {
        if(!unmap) {
            attach_detached(c);
            unmap_client(c);
        }
        c->rect.x = f->rect.x;
        c->rect.y = f->rect.y;
        reparent_client(c, root, c->rect.x, c->rect.y);
    }
}

static Client *
handle_before_read_frames(IXPServer *s, File *file, Layout *l)
{
    Client *f;
    char buf[32];
	if(!l->def)
		return nil;
    for(f = l->def->frames(l); f; f = f->next) {
        if(file == f->file[F_GEOMETRY]) {
            snprintf(buf, sizeof(buf), "%d %d %d %d", f->rect.x, f->rect.y,
                     f->rect.width, f->rect.height);
            if(file->content)
                free(file->content);
            file->content = cext_estrdup(buf);
            file->size = strlen(buf);
            return f;
        }
	    else if(file == f->file[F_NAME]) {
            if(file->content)
                free(file->content);
			if(f->sel && f->sel->name) {
				file->content = cext_estrdup(f->sel->name);
				file->size = strlen(f->sel->name);
			}
			else {
				file->content = nil;
				file->size = 0;
			}
            return f;
		}
	}
    return nil;
}

static void
handle_before_read_frame(IXPServer * s, File * file)
{
    Page *p;
    for(p = pages; p; p = p->next) {
        if(handle_before_read_frames(s, file, p->managed)
           || handle_before_read_frames(s, file, p->floating))
            return;
    }
}

static Client *
handle_after_write_frames(IXPServer *s, File *file, Layout *l)
{
    Client *f;
    for(f = l->def->frames(l); f; f = f->next) {
        if(file == f->file[F_TAB] || file == f->file[F_BORDER]
           || file == f->file[F_HANDLE_INC]) {
            f->layout->def->arrange(f->layout);
            return f;
        } else if(file == f->file[F_GEOMETRY]) {
            char *geom = f->file[F_GEOMETRY]->content;
            if(geom && strrchr(geom, ' ')) {
                XRectangle frect = f->rect;
                blitz_strtorect(&rect, &frect, geom);
                resize_frame(f, &frect, 0);
            }
            return f;
        }
    }
    return nil;
}

static void
handle_after_write_frame(IXPServer * s, File * file)
{
    Page *p;
    for(p = pages; p; p = p->next) {
        if(handle_after_write_frames(s, file, p->managed)
           || handle_after_write_frames(s, file, p->floating))
            return;
    }
}

Client *
sel_frame()
{
	Client *c = sel_client();
	return c ? c->frame : nil;
}
