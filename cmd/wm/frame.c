/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

static void handle_after_write_frame(IXPServer * s, File * file);
static void handle_before_read_frame(IXPServer * s, File * file);

Frame *
alloc_frame(XRectangle * r)
{
    XSetWindowAttributes wa;
    static int id = 0;
    char buf[MAX_BUF];
    Frame *f = (Frame *) cext_emallocz(sizeof(Frame));
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
    wa.event_mask =
        SubstructureRedirectMask | ExposureMask | ButtonPressMask |
        PointerMotionMask;

    bw = border_width(f);
    th = tab_height(f);
    f->rect.width += 2 * bw;
    f->rect.height += bw + (th ? th : bw);
    f->win = XCreateWindow(dpy, root, f->rect.x, f->rect.y, f->rect.width,
                           f->rect.height, 0, DefaultDepth(dpy, screen_num),
                           CopyFromParent, DefaultVisual(dpy, screen_num),
                           CWOverrideRedirect | CWBackPixmap | CWEventMask,
                           &wa);
    XDefineCursor(dpy, f->win, f->cursor);
    f->gc = XCreateGC(dpy, f->win, 0, 0);
    XSync(dpy, False);
    return f;
}

Frame *
win_to_frame(Window w)
{
    Page *p;
    for(p = pages; p; p = p->next) {
        Frame *f;
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
destroy_frame(Frame * f)
{
    XFreeGC(dpy, f->gc);
    XDestroyWindow(dpy, f->win);
    ixp_remove_file(ixps, f->file[F_PREFIX]);
    free(f);
}

unsigned int
tab_height(Frame * f)
{
    if(blitz_strtonum(f->file[F_TAB]->content, 0, 1))
        return font->ascent + font->descent + 4;
    return 0;
}

unsigned int
border_width(Frame * f)
{
    if(blitz_strtonum(f->file[F_BORDER]->content, 0, 1))
        return BORDER_WIDTH;
    return 0;
}

static void
check_dimensions(Frame * f, unsigned int tabh, unsigned int bw)
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
resize_incremental(Frame * f, unsigned int tabh, unsigned int bw)
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
resize_frame(Frame * f, XRectangle * r, XPoint * pt)
{
    Layout *l = f->layout;
    unsigned int tabh = tab_height(f);
    unsigned int bw = border_width(f);
    Client *c;

    l->def->resize(f, r, pt);

    /* resize if client requests special size */
    check_dimensions(f, tabh, bw);

    if(f->file[F_HANDLE_INC]->content
       && ((char *) f->file[F_HANDLE_INC]->content)[0] == '1')
        resize_incremental(f, tabh, bw);

    XMoveResizeWindow(dpy, f->win, f->rect.x, f->rect.y, f->rect.width,
                      f->rect.height);

    for(c = f->clients; c; c = c->next) {
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
draw_frame(Frame * f)
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
            d.bg = blitz_loadcolor(dpy, screen_num, def[WM_SEL_BG_COLOR]->content);
            d.fg = blitz_loadcolor(dpy, screen_num, def[WM_SEL_FG_COLOR]->content);
            d.border = blitz_loadcolor(dpy, screen_num, def[WM_SEL_BORDER_COLOR]->content);
        } else {
            d.bg = blitz_loadcolor(dpy, screen_num, def[WM_NORM_BG_COLOR]->content);
            d.fg = blitz_loadcolor(dpy, screen_num, def[WM_NORM_FG_COLOR]->content);
            d.border = blitz_loadcolor(dpy, screen_num, def[WM_NORM_BORDER_COLOR]->content);
        }
        d.rect = f->rect;
        d.rect.x = d.rect.y = 0;
        d.notch = &notch;

        blitz_drawlabel(dpy, &d);
    }
    draw_clients(f);
    XSync(dpy, False);
}

void
handle_frame_buttonpress(XButtonEvent * e, Frame * f)
{
    Align align;
    int bindex, cindex = e->x / (f->rect.width / f->nclients);
    Client *new = clientat(f->clients, cindex);
    f->layout->def->focus(f->layout, new, False);
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
attach_client_to_frame(Frame * f, Client * client)
{
    Client *c;
    for(c = f->clients; c && c->next; c = c->next);
    if(!c) {
        f->clients = client;
        client->prev = client->next = nil;
    } else {
        client->prev = c;
        client->next = nil;
        c->next = client;
    }
	f->sel = client;
    f->nclients++;
    client->frame = f;
    resize_frame(f, &f->rect, 0);
    reparent_client(client, f->win, client->rect.x, client->rect.y);
    map_client(client);
}

void
detach_client_from_frame(Client * c, Bool unmap)
{
    Frame *f = c->frame;

    c->frame = nil;
    if(f->sel == c)
		f->sel = nil;

    if(f->clients == c) {
        if(c->next)
            c->next->prev = nil;
        f->clients = c->next;
    } else {
        c->prev->next = c->next;
        if(c->next)
            c->next->prev = c->prev;
    }

    f->nclients--;

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

static Frame *
handle_before_read_frames(IXPServer *s, File *file, Layout *l)
{
    Frame *f;
    char buf[32];
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

static Frame *
handle_after_write_frames(IXPServer *s, File *file, Layout *l)
{
    Frame *f;
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

Frame *
sel_frame()
{
	Client *c = sel_client();
	return c ? c->frame : nil;
}
