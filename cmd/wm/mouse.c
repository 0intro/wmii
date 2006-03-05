/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wm.h"

Cursor
cursor_for_motion(Client *c, int x, int y)
{
	Frame *f = c->frame;
    int n, e, w, s, tn, te, tw, ts;

    if(!def.border)
        return cursor[CurNormal];

    /* rectangle attributes of client are used */
    w = x < def.border;
    e = x >= f->rect.width - def.border;
    n = y < def.border;
    s = y >= f->rect.height - def.border;

    tw = x < bar_height();
    te = x > f->rect.width - bar_height();
    tn = y < bar_height();
    ts = s > f->rect.height - bar_height();

    if((w && n) || (w && tn) || (n && tw))
        return cursor[CurNW];
    else if((e && n) || (e && tn) || (n && te))
        return cursor[CurNE];
    else if((w && s) || (w && ts) || (s && tw))
        return cursor[CurSW];
    else if((e && s) || (e && ts) || (s && te))
        return cursor[CurSE];
    else if(w)
        return cursor[CurW];
    else if(e)
        return cursor[CurE];
    else if(n)
        return cursor[CurN];
    else if(s)
        return cursor[CurS];

    return cursor[CurNormal];
}

Align
xy2align(XRectangle *rect, int x, int y)
{

    int w = x <= rect->x + rect->width / 2;
    int n = y <= rect->y + rect->height / 2;
    int e = x > rect->x + rect->width / 2;
    int s = y > rect->y + rect->height / 2;
    int nw = w && n;
    int ne = e && n;
    int sw = w && s;
    int se = e && s;

    if(nw)
        return NWEST;
    else if(ne)
        return NEAST;
    else if(se)
        return SEAST;
    else if(sw)
        return SWEST;
    else if(w)
        return WEST;
    else if(e)
        return EAST;
    else if(n)
        return NORTH;
    else if(s)
        return SOUTH;
    return CENTER;
}

Align
cursor2align(Cursor cur)
{
    if(cur == cursor[CurW])
        return WEST;
    else if(cur == cursor[CurNW])
        return NWEST;
    else if(cur == cursor[CurN])
        return NORTH;
    else if(cur == cursor[CurNE])
        return NEAST;
    else if(cur == cursor[CurE])
        return EAST;
    else if(cur == cursor[CurSE])
        return SEAST;
    else if(cur == cursor[CurS])
        return SOUTH;
    else if(cur == cursor[CurSW])
        return SWEST;
    return CENTER;              /* should not happen */
}

static int
check_vert_match(XRectangle *r, XRectangle *neighbor)
{
    /* check if neighbor matches edge */
    return (((neighbor->y <= r->y)
             && (neighbor->y + neighbor->height >= r->y))
            || ((neighbor->y >= r->y)
                && (r->y + r->height >= neighbor->y)));
}

static int
check_horiz_match(XRectangle * r, XRectangle * neighbor)
{
    /* check if neighbor matches edge */
    return (((neighbor->x <= r->x)
             && (neighbor->x + neighbor->width >= r->x))
            || ((neighbor->x >= r->x)
                && (r->x + r->width >= neighbor->x)));
}

static void
snap_move(XRectangle * r, XRectangle * rects,
          unsigned int num, int snapw, int snaph)
{

    int i, j, w = 0, n = 0, e = 0, s = 0;

    /* snap to other windows */
    for(i = 0; i <= snapw && !(w && e); i++) {

        for(j = 0; j < num && !(w && e); j++) {

            /* check west neighbors leftwards */
            if(!w) {
                if(r->x - i == (rects[j].x + rects[j].width)) {
                    /*
                     * west edge of neighbor found, check
                     * vert match
                     */
                    w = check_vert_match(r, &rects[j]);
                    if(w)
                        r->x = rects[j].x + rects[j].width;
                }
            }
            /* check west neighbors rightwards */
            if(!w) {
                if(r->x + i == (rects[j].x + rects[j].width)) {
                    /*
                     * west edge of neighbor found, check
                     * vert match
                     */
                    w = check_vert_match(r, &rects[j]);
                    if(w)
                        r->x = rects[j].x + rects[j].width;
                }
            }
            /* check east neighbors leftwards */
            if(!e) {
                if(r->x + r->width - i == rects[j].x) {
                    /*
                     * east edge of neighbor found, check
                     * vert match
                     */
                    e = check_vert_match(r, &rects[j]);
                    if(e)
                        r->x = rects[j].x - r->width;
                }
            }
            /* check east neighbors rightwards */
            if(!e) {
                if(r->x + r->width + i == rects[j].x) {
                    /*
                     * east edge of neighbor found, check
                     * vert match
                     */
                    e = check_vert_match(r, &rects[j]);
                    if(e)
                        r->x = rects[j].x - r->width;
                }
            }
        }

        /* snap to west screen border */
        if(!w && (r->x - i == rect.x)) {
            w = 1;
            r->x = rect.x;
        }
        /* snap to west screen border */
        if(!w && (r->x + i == rect.x)) {
            w = 1;
            r->x = rect.x;
        }
        /* snap to east screen border */
        if(!e && (r->x + r->width - i == rect.width)) {
            e = 1;
            r->x = rect.x + rect.width - r->width;
        }
        if(!e && (r->x + r->width + i == rect.width)) {
            e = 1;
            r->x = rect.x + rect.width - r->width;
        }
    }

    for(i = 0; i <= snaph && !(n && s); i++) {

        for(j = 0; j < num && !(n && s); j++) {
            /* check north neighbors upwards */
            if(!n) {
                if(r->y - i == (rects[j].y + rects[j].height)) {
                    /*
                     * north edge of neighbor found,
                     * check horiz match
                     */
                    n = check_horiz_match(r, &rects[j]);
                    if(n)
                        r->y = rects[j].y + rects[j].height;
                }
            }
            /* check north neighbors downwards */
            if(!n) {
                if(r->y + i == (rects[j].y + rects[j].height)) {
                    /*
                     * north edge of neighbor found,
                     * check horiz match
                     */
                    n = check_horiz_match(r, &rects[j]);
                    if(n)
                        r->y = rects[j].y + rects[j].height;
                }
            }
            /* check south neighbors upwards */
            if(!s) {
                if(r->y + r->height - i == rects[j].y) {
                    /*
                     * south edge of neighbor found,
                     * check horiz match
                     */
                    s = check_horiz_match(r, &rects[j]);
                    if(s)
                        r->y = rects[j].y - r->height;
                }
            }
            /* check south neighbors downwards */
            if(!s) {
                if(r->y + r->height + i == rects[j].y) {
                    /*
                     * south edge of neighbor found,
                     * check horiz match
                     */
                    s = check_horiz_match(r, &rects[j]);
                    if(s)
                        r->y = rects[j].y - r->height;
                }
            }
        }

        /* snap to north screen border */
        if(!n && (r->y - i == rect.y)) {
            n = 1;
            r->y = rect.y;
        }
        if(!n && (r->y + i == rect.y)) {
            n = 1;
            r->y = rect.y;
        }
        /* snap to south screen border */
        if(!s && (r->y + r->height - i == rect.height)) {
            s = 1;
            r->y = rect.y + rect.height - r->height;
        }
        if(!s && (r->y + r->height + i == rect.height)) {
            s = 1;
            r->y = rect.y + rect.height - r->height;
        }
    }
}

static void
draw_pseudo_border(XRectangle * r)
{
    XRectangle pseudo = *r;

    pseudo.x += 2;
    pseudo.y += 2;
    pseudo.width -= 4;
    pseudo.height -= 4;
    XDrawRectangles(dpy, root, gc_xor, &pseudo, 1);
    XSync(dpy, False);
}


void
mouse_move(Client *c)
{
    int px = 0, py = 0, wex, wey, ex, ey, first = 1, i;
    Window dummy;
    XEvent ev;
    /* borders */
    int snapw = rect.width * def.snap / 1000;
    int snaph = rect.height * def.snap / 1000;
    unsigned int num;
    unsigned int dmask;
    XRectangle *rects = rectangles(&num);
    XRectangle frect = c->frame->rect;
    XPoint pt;

    XQueryPointer(dpy, c->framewin, &dummy, &dummy, &i, &i, &wex, &wey, &dmask);
    XTranslateCoordinates(dpy, c->framewin, root, wex, wey, &ex, &ey, &dummy);
    pt.x = ex;
    pt.y = ey;
    XSync(dpy, False);
    XGrabServer(dpy);
    while(XGrabPointer
          (dpy, root, False, ButtonMotionMask | ButtonReleaseMask,
           GrabModeAsync, GrabModeAsync, None, cursor[CurMove],
           CurrentTime) != GrabSuccess)
        usleep(20000);

    for(;;) {
        while(!XCheckMaskEvent
              (dpy, ButtonReleaseMask | ButtonMotionMask, &ev)) {
            usleep(20000);
            continue;
        }

        switch (ev.type) {
        case ButtonRelease:
            if(!first) {
                draw_pseudo_border(&frect);
                resize_client(c, &frect, &pt, False);
            }
            free(rects);
            XUngrabPointer(dpy, CurrentTime /* ev.xbutton.time */ );
            XUngrabServer(dpy);
            XSync(dpy, False);
            return;
            break;
        case MotionNotify:
            pt.x = ev.xmotion.x;
            pt.y = ev.xmotion.y;
            XTranslateCoordinates(dpy, c->framewin, root, ev.xmotion.x,
                                  ev.xmotion.y, &px, &py, &dummy);
            if(first)
                first = 0;
            else
                draw_pseudo_border(&frect);
            frect.x = px - ex;
            frect.y = py - ey;
            snap_move(&frect, rects, num, snapw, snaph);
            draw_pseudo_border(&frect);
            break;
        }
    }
}

static void
snap_resize(XRectangle * r, XRectangle * o, Align align,
            XRectangle * rects, unsigned int num, int px, int ox, int py,
            int oy, int snapw, int snaph)
{
    int i, j, pend = 0;
    int w, h;

    /* x */
    switch (align) {
    case NEAST:
    case EAST:
    case SEAST:
        w = px - r->x + (o->width - ox);
        if(w < 10)
            break;
        r->width = w;
        if(w <= snapw)
            break;
        /* snap to border */
        for(i = 0; !pend && (i < snapw); i++) {
            if(r->x + r->width - i == rect.x + rect.width) {
                r->width -= i;
                break;
            }
            if(r->x + r->width + i == rect.x + rect.width) {
                r->width += i;
                break;
            }
            for(j = 0; j < num; j++) {
                if(r->x + r->width - i == rects[j].x) {
                    pend = check_vert_match(r, &rects[j]);
                    if(pend) {
                        r->width -= i;
                        break;
                    }
                }
                if(r->x + r->width + i == rects[j].x) {
                    pend = check_vert_match(r, &rects[j]);
                    if(pend) {
                        r->width += i;
                        break;
                    }
                }
            }
        }
        break;
    case NWEST:
    case WEST:
    case SWEST:
        w = r->width + r->x - px + ox;
        if(w < 10)
            break;
        r->width = w;
        r->x = px - ox;
        if(w <= snapw)
            break;
        /* snap to border */
        for(i = 0; !pend && (i < snapw); i++) {
            if(r->x - i == rect.x) {
                r->x -= i;
                r->width += i;
                break;
            }
            if(r->x + i == rect.x) {
                r->x += i;
                r->width -= i;
                break;
            }
            for(j = 0; j < num; j++) {
                if(r->x - i == rects[j].x + rects[j].width) {
                    pend = check_vert_match(r, &rects[j]);
                    if(pend) {
                        r->x -= i;
                        r->width += i;
                        break;
                    }
                }
                if(r->x + i == rects[j].x + rects[j].width) {
                    pend = check_vert_match(r, &rects[j]);
                    if(pend) {
                        r->x += i;
                        r->width -= i;
                        break;
                    }
                }
            }
        }
        break;
    default:
        break;
    }

    /* y */
    pend = 0;
    switch (align) {
    case SWEST:
    case SOUTH:
    case SEAST:
        h = py - r->y + (o->height - oy);
        if(h < 10)
            break;
        r->height = h;
        if(h <= snaph)
            break;
        /* snap to border */
        for(i = 0; !pend && (i < snaph); i++) {
            if(r->y + r->height - i == rect.y + rect.height) {
                r->height -= i;
                break;
            }
            if(r->y + r->height + i == rect.y + rect.height) {
                r->height += i;
                break;
            }
            for(j = 0; j < num; j++) {
                if(r->y + r->height - i == rects[j].y) {
                    pend = check_horiz_match(r, &rects[j]);
                    if(pend) {
                        r->height -= i;
                        break;
                    }
                }
                if(r->y + r->height + i == rects[j].y) {
                    pend = check_horiz_match(r, &rects[j]);
                    if(pend) {
                        r->height += i;
                        break;
                    }
                }
            }
        }
        break;
    case NWEST:
    case NORTH:
    case NEAST:
        h = r->height + r->y - py + oy;
        if(h < 10)
            break;
        r->height = h;
        r->y = py - oy;
        if(h <= snaph)
            break;
        /* snap to border */
        for(i = 0; !pend && (i < snaph); i++) {
            if(r->y - i == rect.y) {
                r->y -= i;
                r->height += i;
                break;
            }
            if(r->y + i == rect.y) {
                r->y += i;
                r->height -= i;
                break;
            }
            for(j = 0; j < num; j++) {
                if(r->y - i == rects[j].y + rects[j].height) {
                    pend = check_horiz_match(r, &rects[j]);
                    if(pend) {
                        r->y -= i;
                        r->height += i;
                        break;
                    }
                }
                if(r->y + i == rects[j].y + rects[j].height) {
                    pend = check_horiz_match(r, &rects[j]);
                    if(pend) {
                        r->y += i;
                        r->height -= i;
                        break;
                    }
                }
            }
        }
        break;
    default:
        break;
    }
}


void
mouse_resize(Client *c, Align align)
{
    int px = 0, py = 0, i, ox, oy, first = 1;
    Window dummy;
    XEvent ev;
    /* borders */
    int snapw = rect.width * def.snap / 1000;
    int snaph = rect.height * def.snap / 1000;
    unsigned int dmask;
    unsigned int num;
    XRectangle *rects = rectangles(&num);
    XRectangle frect = c->frame->rect;
    XRectangle origin = frect;

    XQueryPointer(dpy, c->framewin, &dummy, &dummy, &i, &i, &ox, &oy, &dmask);
    XSync(dpy, False);
    XGrabServer(dpy);
    while(XGrabPointer
          (dpy, c->framewin, False, ButtonMotionMask | ButtonReleaseMask,
           GrabModeAsync, GrabModeAsync, None, cursor[CurResize],
           CurrentTime) != GrabSuccess)
        usleep(20000);

    for(;;) {
        while(!XCheckMaskEvent
              (dpy, ButtonReleaseMask | ButtonMotionMask, &ev)) {
            usleep(20000);
            continue;
        }

        switch (ev.type) {
        case ButtonRelease:
            if(!first) {
                XPoint pt;
                draw_pseudo_border(&frect);
                pt.x = px;
                pt.y = py;
                resize_client(c, &frect, &pt, False);
            }
            XUngrabPointer(dpy, CurrentTime /* ev.xbutton.time */ );
            XUngrabServer(dpy);
            XSync(dpy, False);
            return;
            break;
        case MotionNotify:
            XTranslateCoordinates(dpy, c->framewin, root, ev.xmotion.x,
                                  ev.xmotion.y, &px, &py, &dummy);

            if(first)
                first = 0;
            else
                draw_pseudo_border(&frect);

            snap_resize(&frect, &origin, align, rects, num, px,
                        ox, py, oy, snapw, snaph);
            draw_pseudo_border(&frect);
            break;
        }
    }
}

void
grab_mouse(Window w, unsigned long mod, unsigned int button)
{
    XGrabButton(dpy, button, mod, w, False,
                ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    if((mod != AnyModifier) && num_lock_mask) {
        XGrabButton(dpy, button, mod | num_lock_mask, w, False,
                    ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
        XGrabButton(dpy, button, mod | num_lock_mask | LockMask, w,
                    False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    }
}

void
ungrab_mouse(Window w, unsigned long mod, unsigned int button)
{
    XUngrabButton(dpy, button, mod, w);
    if(mod != AnyModifier && num_lock_mask) {
        XUngrabButton(dpy, button, mod | num_lock_mask, w);
        XUngrabButton(dpy, button, mod | num_lock_mask | LockMask, w);
    }
}

char *
warp_mouse(char *arg)
{
	const char *errstr;
	char *sx = arg, *sy;
	unsigned int x, y;
	sy = strchr(sx, ' ');
	if(!sy)
		return "invalid argument";
	*sy = 0;
	sy++;
	x = cext_strtonum(sx, 0, rect.width, &errstr);
	if(errstr)
		return "invalid x argument";
	y = cext_strtonum(sy, 0, rect.height, &errstr);
	if(errstr)
		return "invalid y argument";
	XWarpPointer(dpy, None, root, 0, 0, 0, 0, x, y);
	XSync(dpy, False);
	return nil;
}
