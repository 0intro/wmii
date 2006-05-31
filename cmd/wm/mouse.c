/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * (C)opyright MMVI Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wm.h"

#define ButtonMask      (ButtonPressMask | ButtonReleaseMask)
#define MouseMask       (ButtonMask | PointerMotionMask)

static void
snap_line(XRectangle *rects, int num, int x1, int y1, int x2, int y2,
          int snapw, BlitzAlign mask, int *delta);

static void
rect_morph_xy(XRectangle *rect, int dx, int dy, BlitzAlign mask) {
    if(mask & NORTH) {
        rect->y += dy;
        rect->height -= dy;
    }
    if(mask & WEST) {
        rect->x += dx;
        rect->width -= dx;
    }
    if(mask & EAST)
        rect->width += dx;
    if(mask & SOUTH)
        rect->height += dy;
}

void
snap_rect(XRectangle *rects, int num, XRectangle *current,
          BlitzAlign mask, int snap) {
    int dx = snap + 1, dy = snap + 1;

    if(mask & NORTH)
        snap_line(rects, num, current->x, current->y,
                  current->x + current->width, current->y,
                  snap, mask, &dy);
    if(mask & EAST)
        snap_line(rects, num, current->x + current->width, current->y,
                  current->x + current->width, current->y + current->height,
                  snap, mask, &dx);
    if(mask & SOUTH)
        snap_line(rects, num, current->x, current->y + current->height,
                  current->x + current->width, current->y + current->height,
                  snap, mask, &dy);
    if(mask & WEST)
        snap_line(rects, num, current->x, current->y,
                  current->x, current->y + current->height,
                  snap, mask, &dx);

    rect_morph_xy(current, abs(dx) <= snap ? dx : 0,
                           abs(dy) <= snap ? dy : 0, mask);
}

static void
snap_line(XRectangle *rects, int num, int x1, int y1, int x2, int y2,
          int snapw, BlitzAlign mask, int *delta) {
    int i, t_xy;

    /* horizontal */
    if(y1 == y2 && (mask & (NORTH|SOUTH))) {
        for(i=0; i < num; i++) {
            if(!((rects[i].x + rects[i].width < x1) ||
                 (rects[i].x > x2))) {

                if(abs(rects[i].y - y1) <= abs(*delta))
                    *delta = rects[i].y - y1;

                t_xy = rects[i].y + rects[i].height;
                if(abs(t_xy - y1) < abs(*delta))
                    *delta = t_xy - y1;
            }
        }
    }
    else if (mask & (EAST|WEST)) {
        /* This is the same as above, tr/xy/yx/, 
         *                            s/width/height/, s/height/width/ */
        for(i=0; i < num; i++) {
            if(!((rects[i].y + rects[i].height < y1) ||
                 (rects[i].y > y2))) {

                if(abs(rects[i].x - x1) <= abs(*delta))
                    *delta = rects[i].x - x1;

                t_xy = rects[i].x + rects[i].width;
                if(abs(t_xy - x1) < abs(*delta))
                    *delta = t_xy - x1;
            }
        }
    }
}

static void
draw_xor_border(XRectangle *r)
{
	XRectangle xor = *r;

	xor.x += 2;
	xor.y += 2;
	xor.width -= 4; 
	xor.height -= 4;
	XSetLineAttributes(dpy, xorgc, 1, LineSolid, CapNotLast, JoinMiter);
	XDrawLine(dpy, root, xorgc, xor.x + 2, xor.y +  xor.height / 2,
			xor.x + xor.width - 2, xor.y + xor.height / 2);
	XDrawLine(dpy, root, xorgc, xor.x + xor.width / 2, xor.y + 2,
			xor.x + xor.width / 2, xor.y + xor.height - 2);
	XSetLineAttributes(dpy, xorgc, 4, LineSolid, CapNotLast, JoinMiter);
	XDrawRectangles(dpy, root, xorgc, &xor, 1);
	XSync(dpy, False);
}

void
do_mouse_resize(Client *c, BlitzAlign align)
{
	int px = 0, py = 0, i, ox, oy;
	Window dummy;
	XEvent ev;
	unsigned int num = 0, di;
	Frame *f = c->frame.data[c->sel];
	int aidx = idx_of_area(f->area);
	int snap = aidx ? 0 : rect.height / 66;
	XRectangle *rects = aidx ? nil : rects_of_view(f->area->view, &num);
	XRectangle frect = f->rect;
	XRectangle origin = frect;
	XPoint pt;

	XQueryPointer(dpy, c->framewin, &dummy, &dummy, &ox, &oy, &i, &i, &di);
	pt.x = ox; pt.y = oy;
	XSync(dpy, False);

	if(XGrabPointer(dpy, c->framewin, False, MouseMask, GrabModeAsync, GrabModeAsync,
			None, cursor[CurResize], CurrentTime) != GrabSuccess)
		return;
	
	XGrabServer(dpy);
	draw_xor_border(&frect);
	for(;;) {
		XMaskEvent(dpy, MouseMask | ExposureMask, &ev);
		switch (ev.type) {
		case ButtonRelease:
			draw_xor_border(&frect);
			if(aidx)
				resize_column(c, &frect, (align == CENTER) ? &pt : nil);
			else
				resize_client(c, &frect, False);
			if(rects)
				free(rects);
			XUngrabServer(dpy);
			XUngrabPointer(dpy, CurrentTime);
			XSync(dpy, False);
			return;
			break;
		case MotionNotify:
			draw_xor_border(&frect);

			pt.x = ev.xmotion.x;
			pt.y = ev.xmotion.y;
			XTranslateCoordinates(dpy, c->framewin, root, ev.xmotion.x,
					ev.xmotion.y, &px, &py, &dummy);

			rect_morph_xy(&origin, px-ox, py-oy, align);
			frect=origin;
			ox=px; oy=py;

			if(!aidx)
				snap_rect(rects, num, &frect, align, snap);

			draw_xor_border(&frect);
			break;
		case Expose:
			(handler[Expose])(&ev);
			break;
		default: break;
		}
	}
}

void
grab_mouse(Window w, unsigned long mod, unsigned int button)
{
	XGrabButton(dpy, button, mod, w, False, ButtonMask,
			GrabModeAsync, GrabModeSync, None, None);
	if((mod != AnyModifier) && num_lock_mask) {
		XGrabButton(dpy, button, mod | num_lock_mask, w, False, ButtonMask,
				GrabModeAsync, GrabModeSync, None, None);
		XGrabButton(dpy, button, mod | num_lock_mask | LockMask, w, False,
				ButtonMask, GrabModeAsync, GrabModeSync, None, None);
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
