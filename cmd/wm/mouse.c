/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>

#include "wm.h"

Cursor cursor_for_motion(Frame * f, int x, int y)
{
	int n, e, w, s, tn, te, tw, ts;
	int tabh, bw;

	bw = border_width(f);
	tabh = tab_height(f);

	if (!f || !bw)
		return normal_cursor;

	/* rectangle attributes of client are used */
	w = x < bw;
	e = x >= f->rect.width - bw;
	n = y < bw;
	s = y >= f->rect.height - bw;

	tw = x < (tabh ? tabh : 2 * bw);
	te = x > f->rect.width - (tabh ? tabh : 2 * bw);
	tn = y < (tabh ? tabh : 2 * bw);
	ts = s > f->rect.height - (tabh ? tabh : 2 * bw);

	if ((w && n) || (w && tn) || (n && tw))
		return nw_cursor;
	else if ((e && n) || (e && tn) || (n && te))
		return ne_cursor;
	else if ((w && s) || (w && ts) || (s && tw))
		return sw_cursor;
	else if ((e && s) || (e && ts) || (s && te))
		return se_cursor;
	else if (w)
		return w_cursor;
	else if (e)
		return e_cursor;
	else if (n)
		return n_cursor;
	else if (s)
		return s_cursor;

	return normal_cursor;
}

Align xy_to_align(XRectangle * rect, int x, int y)
{

	int w = x <= rect->x + rect->width / 2;
	int n = y <= rect->y + rect->height / 2;
	int e = x > rect->x + rect->width / 2;
	int s = y > rect->y + rect->height / 2;
	int nw = w && n;
	int ne = e && n;
	int sw = w && s;
	int se = e && s;

	if (nw)
		return NWEST;
	else if (ne)
		return NEAST;
	else if (se)
		return SEAST;
	else if (sw)
		return SWEST;
	else if (w)
		return WEST;
	else if (e)
		return EAST;
	else if (n)
		return NORTH;
	else if (s)
		return SOUTH;
	return CENTER;
}

Align cursor_to_align(Cursor cursor)
{
	if (cursor == w_cursor)
		return WEST;
	else if (cursor == nw_cursor)
		return NWEST;
	else if (cursor == n_cursor)
		return NORTH;
	else if (cursor == ne_cursor)
		return NEAST;
	else if (cursor == e_cursor)
		return EAST;
	else if (cursor == se_cursor)
		return SEAST;
	else if (cursor == s_cursor)
		return SOUTH;
	else if (cursor == sw_cursor)
		return SWEST;
	return CENTER;				/* should not happen */
}

static int check_vert_match(XRectangle * r, XRectangle * neighbor)
{
	/* check if neighbor matches edge */
	return (((neighbor->y <= r->y)
			 && (neighbor->y + neighbor->height >= r->y))
			|| ((neighbor->y >= r->y)
				&& (r->y + r->height >= neighbor->y)));
}

static int check_horiz_match(XRectangle * r, XRectangle * neighbor)
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
	for (i = 0; i <= snapw && !(w && e); i++) {

		for (j = 0; j < num && !(w && e); j++) {

			/* check west neighbors leftwards */
			if (!w) {
				if (r->x - i == (rects[j].x + rects[j].width)) {
					/*
					 * west edge of neighbor found, check
					 * vert match
					 */
					w = check_vert_match(r, &rects[j]);
					if (w)
						r->x = rects[j].x + rects[j].width;
				}
			}
			/* check west neighbors rightwards */
			if (!w) {
				if (r->x + i == (rects[j].x + rects[j].width)) {
					/*
					 * west edge of neighbor found, check
					 * vert match
					 */
					w = check_vert_match(r, &rects[j]);
					if (w)
						r->x = rects[j].x + rects[j].width;
				}
			}
			/* check east neighbors leftwards */
			if (!e) {
				if (r->x + r->width - i == rects[j].x) {
					/*
					 * east edge of neighbor found, check
					 * vert match
					 */
					e = check_vert_match(r, &rects[j]);
					if (e)
						r->x = rects[j].x - r->width;
				}
			}
			/* check east neighbors rightwards */
			if (!e) {
				if (r->x + r->width + i == rects[j].x) {
					/*
					 * east edge of neighbor found, check
					 * vert match
					 */
					e = check_vert_match(r, &rects[j]);
					if (e)
						r->x = rects[j].x - r->width;
				}
			}
		}

		/* snap to west screen border */
		if (!w && (r->x - i == rect.x)) {
			w = 1;
			r->x = rect.x;
		}
		/* snap to west screen border */
		if (!w && (r->x + i == rect.x)) {
			w = 1;
			r->x = rect.x;
		}
		/* snap to east screen border */
		if (!e && (r->x + r->width - i == rect.width)) {
			e = 1;
			r->x = rect.x + rect.width - r->width;
		}
		if (!e && (r->x + r->width + i == rect.width)) {
			e = 1;
			r->x = rect.x + rect.width - r->width;
		}
	}

	for (i = 0; i <= snaph && !(n && s); i++) {

		for (j = 0; j < num && !(n && s); j++) {
			/* check north neighbors upwards */
			if (!n) {
				if (r->y - i == (rects[j].y + rects[j].height)) {
					/*
					 * north edge of neighbor found,
					 * check horiz match
					 */
					n = check_horiz_match(r, &rects[j]);
					if (n)
						r->y = rects[j].y + rects[j].height;
				}
			}
			/* check north neighbors downwards */
			if (!n) {
				if (r->y + i == (rects[j].y + rects[j].height)) {
					/*
					 * north edge of neighbor found,
					 * check horiz match
					 */
					n = check_horiz_match(r, &rects[j]);
					if (n)
						r->y = rects[j].y + rects[j].height;
				}
			}
			/* check south neighbors upwards */
			if (!s) {
				if (r->y + r->height - i == rects[j].y) {
					/*
					 * south edge of neighbor found,
					 * check horiz match
					 */
					s = check_horiz_match(r, &rects[j]);
					if (s)
						r->y = rects[j].y - r->height;
				}
			}
			/* check south neighbors downwards */
			if (!s) {
				if (r->y + r->height + i == rects[j].y) {
					/*
					 * south edge of neighbor found,
					 * check horiz match
					 */
					s = check_horiz_match(r, &rects[j]);
					if (s)
						r->y = rects[j].y - r->height;
				}
			}
		}

		/* snap to north screen border */
		if (!n && (r->y - i == rect.y)) {
			n = 1;
			r->y = rect.y;
		}
		if (!n && (r->y + i == rect.y)) {
			n = 1;
			r->y = rect.y;
		}
		/* snap to south screen border */
		if (!s && (r->y + r->height - i == rect.height)) {
			s = 1;
			r->y = rect.y + rect.height - r->height;
		}
		if (!s && (r->y + r->height + i == rect.height)) {
			s = 1;
			r->y = rect.y + rect.height - r->height;
		}
	}
}

static void draw_pseudo_border(XRectangle * r)
{
	XRectangle pseudo = *r;

	pseudo.x += 2;
	pseudo.y += 2;
	pseudo.width -= 4;
	pseudo.height -= 4;
	XDrawRectangles(dpy, root, xorgc, &pseudo, 1);
	XSync(dpy, False);
}


void mouse_move(Frame * f)
{
	int px = 0, py = 0, wex, wey, ex, ey, first = 1, i;
	Window dummy;
	XEvent ev;
	/* borders */
	int snapw = rect.width * _strtonum(def[WM_SNAP_VALUE]->content, 0, 1000) / 1000;
	int snaph = rect.height * _strtonum(def[WM_SNAP_VALUE]->content, 0, 1000) / 1000;
	unsigned int num;
	unsigned int dmask;
	XRectangle *rects = rectangles(&num);
	XRectangle frect = f->rect;
	XPoint pt;

	XQueryPointer(dpy, f->win, &dummy, &dummy, &i, &i, &wex, &wey, &dmask);
	XTranslateCoordinates(dpy, f->win, root, wex, wey, &ex, &ey, &dummy);
	pt.x = ex;
	pt.y = ey;
	XSync(dpy, False);
	XGrabServer(dpy);
	XGrabPointer(dpy, root, False, ButtonMotionMask | ButtonReleaseMask,
				 GrabModeAsync, GrabModeAsync, None, move_cursor,
				 CurrentTime);
	for (;;) {
		while (!XCheckMaskEvent(dpy,
								ButtonReleaseMask | ButtonMotionMask,
								&ev)) {
			usleep(20000);
			continue;
		}

		switch (ev.type) {
		case ButtonRelease:
			if (!first) {
				draw_pseudo_border(&frect);
				resize_frame(f, &frect, &pt);
			}
			draw_page(get_sel_page());
			free(rects);
			XUngrabPointer(dpy, CurrentTime /* ev.xbutton.time */ );
			XUngrabServer(dpy);
			XSync(dpy, False);
			return;
			break;
		case MotionNotify:
			pt.x = ev.xmotion.x;
			pt.y = ev.xmotion.y;
			XTranslateCoordinates(dpy, f->win, root, ev.xmotion.x,
								  ev.xmotion.y, &px, &py, &dummy);
			if (first)
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
		if (w < 10)
			break;
		r->width = w;
		if (w <= snapw)
			break;
		/* snap to border */
		for (i = 0; !pend && (i < snapw); i++) {
			if (r->x + r->width - i == rect.x + rect.width) {
				r->width -= i;
				break;
			}
			if (r->x + r->width + i == rect.x + rect.width) {
				r->width += i;
				break;
			}
			for (j = 0; j < num; j++) {
				if (r->x + r->width - i == rects[j].x) {
					pend = check_vert_match(r, &rects[j]);
					if (pend) {
						r->width -= i;
						break;
					}
				}
				if (r->x + r->width + i == rects[j].x) {
					pend = check_vert_match(r, &rects[j]);
					if (pend) {
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
		if (w < 10)
			break;
		r->width = w;
		r->x = px - ox;
		if (w <= snapw)
			break;
		/* snap to border */
		for (i = 0; !pend && (i < snapw); i++) {
			if (r->x - i == rect.x) {
				r->x -= i;
				r->width += i;
				break;
			}
			if (r->x + i == rect.x) {
				r->x += i;
				r->width -= i;
				break;
			}
			for (j = 0; j < num; j++) {
				if (r->x - i == rects[j].x + rects[j].width) {
					pend = check_vert_match(r, &rects[j]);
					if (pend) {
						r->x -= i;
						r->width += i;
						break;
					}
				}
				if (r->x + i == rects[j].x + rects[j].width) {
					pend = check_vert_match(r, &rects[j]);
					if (pend) {
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
		if (h < 10)
			break;
		r->height = h;
		if (h <= snaph)
			break;
		/* snap to border */
		for (i = 0; !pend && (i < snaph); i++) {
			if (r->y + r->height - i == rect.y + rect.height) {
				r->height -= i;
				break;
			}
			if (r->y + r->height + i == rect.y + rect.height) {
				r->height += i;
				break;
			}
			for (j = 0; j < num; j++) {
				if (r->y + r->height - i == rects[j].y) {
					pend = check_horiz_match(r, &rects[j]);
					if (pend) {
						r->height -= i;
						break;
					}
				}
				if (r->y + r->height + i == rects[j].y) {
					pend = check_horiz_match(r, &rects[j]);
					if (pend) {
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
		if (h < 10)
			break;
		r->height = h;
		r->y = py - oy;
		if (h <= snaph)
			break;
		/* snap to border */
		for (i = 0; !pend && (i < snaph); i++) {
			if (r->y - i == rect.y) {
				r->y -= i;
				r->height += i;
				break;
			}
			if (r->y + i == rect.y) {
				r->y += i;
				r->height -= i;
				break;
			}
			for (j = 0; j < num; j++) {
				if (r->y - i == rects[j].y + rects[j].height) {
					pend = check_horiz_match(r, &rects[j]);
					if (pend) {
						r->y -= i;
						r->height += i;
						break;
					}
				}
				if (r->y + i == rects[j].y + rects[j].height) {
					pend = check_horiz_match(r, &rects[j]);
					if (pend) {
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


void mouse_resize(Frame * f, Align align)
{
	int px = 0, py = 0, i, ox, oy, first = 1;
	Window dummy;
	XEvent ev;
	/* borders */
	int snapw = rect.width * _strtonum(def[WM_SNAP_VALUE]->content, 0, 1000) / 1000;
	int snaph = rect.height * _strtonum(def[WM_SNAP_VALUE]->content, 0, 1000) / 1000;
	unsigned int dmask;
	unsigned int num;
	XRectangle *rects = rectangles(&num);
	XRectangle frect = f->rect;
	XRectangle origin = frect;

	XQueryPointer(dpy, f->win, &dummy, &dummy, &i, &i, &ox, &oy, &dmask);
	XSync(dpy, False);
	XGrabServer(dpy);
	XGrabPointer(dpy, f->win, False, ButtonMotionMask | ButtonReleaseMask,
				 GrabModeAsync, GrabModeAsync, None, resize_cursor,
				 CurrentTime);
	for (;;) {
		while (!XCheckMaskEvent(dpy,
								ButtonReleaseMask | ButtonMotionMask,
								&ev)) {
			usleep(20000);
			continue;
		}

		switch (ev.type) {
		case ButtonRelease:
			if (!first) {
				XPoint pt;
				draw_pseudo_border(&frect);
				pt.x = px;
				pt.y = py;
				resize_frame(f, &frect, &pt);
			}
			XUngrabPointer(dpy, CurrentTime /* ev.xbutton.time */ );
			XUngrabServer(dpy);
			XSync(dpy, False);
			return;
			break;
		case MotionNotify:
			XTranslateCoordinates(dpy, f->win, root, ev.xmotion.x,
								  ev.xmotion.y, &px, &py, &dummy);

			if (first)
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


/* XXXX seems obsolete: */
void drop_move(Frame *f, XRectangle *new, XPoint *pt)
{
	Frame *f1, *f2;
	Area *a = f->area;
	Container *frames = a->layout->get_frames(a);
	int cx, cy;
	unsigned int i, idx = cext_list_get_item_index(frames, f);
	size_t size = cext_sizeof(frames);

	if ((f->rect.x == new->x) && (f->rect.y == new->y))
		return;
	cx = (pt ? pt->x : new->x + new->width / 2);
	cy = (pt ? pt->y : new->y + new->height / 2);
	f1 = cext_list_get_item(frames, idx);
	for (i = 0; i < size; i++) {
		f2 = cext_list_get_item(frames, i);
		if ((i != idx) && blitz_ispointinrect(cx, cy, &f1->rect)) {
			cext_swap_items(frames, f1, f2);
			cext_stack_top_item(frames, f2);
			a->layout->arrange(a);
			return;
		}
	}
}

void center_pointer(Frame * f)
{

	Window dummy;
	int wex, wey, ex, ey, i;
	unsigned int dmask;
	if (!f)
		return;
	XQueryPointer(dpy, f->win, &dummy, &dummy, &i, &i, &wex, &wey, &dmask);
	XTranslateCoordinates(dpy, f->win, root, wex, wey, &ex, &ey, &dummy);
	if (blitz_ispointinrect(ex, ey, &f->rect))
		return;
	/* suppress EnterNotify's while mouse warping */
	XSelectInput(dpy, root, ROOT_MASK & ~StructureNotifyMask);
	XWarpPointer(dpy, None, f->win, 0, 0, 0, 0, f->rect.width / 2,
				 f->rect.height / 2);
	XSync(dpy, False);
	XSelectInput(dpy, root, ROOT_MASK);
}
