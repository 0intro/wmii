/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <math.h>
#include <string.h>
#include <X11/extensions/shape.h>
#include <util.h>
#include "dat.h"
#include "fns.h"

int divw, divh;
GC divgc;
GC maskgc;

char *modes[] = {
	[Coldefault] =	"default",
	[Colstack] =	"stack",
	[Colmax] =	"max",
};

Divide *
win2div(Window w) {
	Divide *d;
	
	for(d = divs; d; d = d->next)
		if(d->w == w) return d;
	return nil;
}

int
str2colmode(const char *str) {
	int i;
	
	for(i = 0; i < nelem(modes); i++)
		if(strcasecmp(str, modes[i]))
			return i;
	return -1;
}

static void
draw_pmap(Pixmap pm, GC gc, Bool color) {
	XPoint pt[4];

	pt[0] = (XPoint){ 0, 0 };
	pt[1] = (XPoint){ divw/2 - 1, divw/2 - 1 };
	pt[2] = (XPoint){ divw/2, divw/2 - 1 };
	pt[3] = (XPoint){ divw - 1, 0 };

	if(color)
		XSetForeground(blz.dpy, gc, def.normcolor.bg);
	else {
		XSetForeground(blz.dpy, gc, 0);
		XFillRectangle(blz.dpy, pm, gc, 0, 0, divw, divh);
		XSetForeground(blz.dpy, gc, 1);
	}

	XFillPolygon(blz.dpy, pm, gc, pt, nelem(pt), Convex, CoordModeOrigin);

	if(color)
		XSetForeground(blz.dpy, gc, def.normcolor.border);

	XDrawLines(blz.dpy, pm, gc, pt, nelem(pt), CoordModeOrigin);
	XDrawRectangle(blz.dpy, pm, gc, pt[1].x, pt[1].y, 1, screen->rect.height);
}

void
update_dividers() {
	if(divmap) {
		XFreePixmap(blz.dpy, divmap);
		XFreePixmap(blz.dpy, divmask);
		XFreeGC(blz.dpy, divgc);
		XFreeGC(blz.dpy, maskgc);
	}

	divw = 2 * (labelh(&def.font) / 3);
	divw = max(divw, 10);
	divh = screen->rect.height;

	divmap = XCreatePixmap(blz.dpy, blz.root,
				divw, divh,
				DefaultDepth(blz.dpy, blz.screen));
	divmask = XCreatePixmap(blz.dpy, blz.root,
				divw, divh,
				1);
	divgc = XCreateGC(blz.dpy, divmap, 0, 0);
	maskgc = XCreateGC(blz.dpy, divmask, 0, 0);

	draw_pmap(divmap, divgc, True);
	draw_pmap(divmask, maskgc, False);
}

static Divide*
get_div(Divide **dp) {
	XSetWindowAttributes wa;
	Divide *d;

	if(*dp)
		return *dp;

	d = emallocz(sizeof *d);

	wa.override_redirect = True;
	wa.background_pixmap = ParentRelative;
	wa.event_mask =
		  SubstructureRedirectMask
		| ExposureMask
		| EnterWindowMask
		| PointerMotionMask
		| ButtonPressMask
		| ButtonReleaseMask;
	d->w = XCreateWindow(
		/* display */	blz.dpy,
		/* parent */	blz.root,
		/* x, y */		0, 0,
		/* w, h */		1, 1,
		/* border */	0,
		/* depth */	DefaultDepth(blz.dpy, blz.screen),
		/* class */	CopyFromParent,
		/* visual */	DefaultVisual(blz.dpy, blz.screen),
		/* valuemask */	CWOverrideRedirect | CWEventMask | CWBackPixmap,
		/* attributes */&wa
		);

	*dp = d;
	return d;
}

static void
map_div(Divide *d) {
	if(!d->mapped)
		XMapWindow(blz.dpy, d->w);
	d->mapped = 1;
}

static void
unmap_div(Divide *d) {
	if(d->mapped)
		XUnmapWindow(blz.dpy, d->w);
	d->mapped = 0;
}

static void
move_div(Divide *d, int x) {
	d->x = x - divw/2;

	XMoveResizeWindow(blz.dpy,
			d->w,
			d->x, 0,
			divw, divh);
	map_div(d);
}

void
update_divs() {
	Divide **dp, *d;
	Area *a;
	View *v;

	update_dividers();
	
	v = screen->sel;
	dp = &divs;
	for(a = v->area->next; a; a = a->next) {
		d = get_div(dp);
		dp = &d->next;
		d->x = a->rect.x - divw/2;
		move_div(d, a->rect.x);
		if(!a->next) {
			d = get_div(dp);
			dp = &d->next;
			move_div(d, r_east(&a->rect));
		}
	}
	for(d = *dp; d; d = d->next)
		unmap_div(d);
}

void
draw_div(Divide *d) {
	XCopyArea(
		blz.dpy,
		divmap, d->w,
		divgc,
		/* x, y */	0, 0,
		/* w, h */	divw, divh,
		/* dest x, y */	0, 0
		);
	XShapeCombineMask (
		/* dpy */	blz.dpy,
		/* dst */	d->w,
		/* type */	ShapeBounding,
		/* off x, y */	0, 0,
		/* src */	divmask,
		/* op */	ShapeSet
		);
}

static void
scale_column(Area *a) {
	Frame *f, **fp;
	uint minh, yoff, dy;
	uint ncol, nuncol;
	uint colh, uncolh;
	int surplus, i, j;

	if(!a->frame)
		return;

	/* This works by comparing heights based on a surplus of their
	 * minimum size. We start by subtracting the minimum size, then
	 * scale the surplus, and add back the minimum size later. This
	 * is based on the size of the client, rather than the frame, so
	 * increment gaps can be equalized later */
	/* Frames that can't be accomodated are pushed to the floating layer */

	minh = labelh(&def.font);
	colh = labelh(&def.font);
	uncolh = minh + frame_delta_h();

	ncol = 0;
	nuncol = 0;
	dy = 0;
	for(f=a->frame; f; f=f->anext)
		if(f->collapsed)
			ncol++;
		else
			nuncol++;

	surplus = a->rect.height;
	surplus -= ncol * colh;
	surplus -= nuncol * uncolh;
	if(surplus < 0) {
		i = ceil((float)(-surplus)/(uncolh - colh));
		if(i >= nuncol)
			i = nuncol - 1;
		nuncol -= i;
		ncol += i;
		surplus += i * (uncolh - colh);
	}
	if(surplus < 0) {
		i = ceil((float)(-surplus)/colh);
		if(i > ncol)
			i = ncol;
		ncol -= i;
		surplus += i * colh;
	}

	i = ncol - 1;
	j = nuncol - 1;
	for(f=a->frame; f; f=f->anext) {
		if(f == a->sel)
			j++;
		if(!f->collapsed) {
			if(j < 0 && f != a->sel)
				f->collapsed = True;
			else {
				if(f->crect.height <= minh)
					f->crect.height = 1;
				else
					f->crect.height -= minh;
				dy += f->crect.height;
			}
			j--;
		}
	}
	for(fp=&a->frame; *fp;) {
		f = *fp;
		if(f == a->sel)
			i++;
		if(f->collapsed) {
			if(i < 0 && f != a->sel) {
				f->collapsed = False;
				send_to_area(f->view->area, f);
				continue;
			}
			i--;
		}
		fp=&f->anext;
	}

	i = nuncol;
	for(f=a->frame; f; f=f->anext) {
		f->rect.x = a->rect.x;
		f->rect.width = a->rect.width;
		if(!f->collapsed) {
			i--;
			f->rect.height = (float)f->crect.height / dy * surplus;
			if(!i)
				f->rect.height = surplus;
			f->rect.height += minh + frame_delta_h();
			apply_sizehints(f->client, &f->rect, False, True, NWEST);

			dy -= f->crect.height;
			surplus -= f->rect.height - frame_delta_h() - minh;
		}else
			f->rect.height = labelh(&def.font);
	}

	yoff = a->rect.y;
	i = nuncol;
	for(f=a->frame; f; f=f->anext) {
		f->rect.y = yoff;
		f->rect.x = a->rect.x;
		f->rect.width = a->rect.width;
		if(f->collapsed)
			yoff += f->rect.height;
		else{
			i--;
			f->rect.height += surplus / nuncol;
			if(!i)
				f->rect.height += surplus % nuncol;
			yoff += f->rect.height;
		}
	}
}

void
arrange_column(Area *a, Bool dirty) {
	Frame *f;

	if(a->floating || !a->frame)
		return;

	switch(a->mode) {
	case Coldefault:
		for(f=a->frame; f; f=f->anext) {
			f->collapsed = False;
			if(dirty)
				f->crect.height = 100;
		}
		break;
	case Colstack:
		for(f=a->frame; f; f=f->anext)
			f->collapsed = (f != a->sel);
		break;
	case Colmax:
		for(f=a->frame; f; f=f->anext) {
			f->collapsed = False;
			f->rect = a->rect;
		}
		goto resize;
	default:
		break;
	}
	scale_column(a);
resize:
	if(a->view == screen->sel) {
		restack_view(a->view);
		resize_client(a->sel->client, &a->sel->rect);
		for(f=a->frame; f; f=f->anext)
			if(!f->collapsed && f != a->sel)
				resize_client(f->client, &f->rect);
		for(f=a->frame; f; f=f->anext)
			if(f->collapsed && f != a->sel)
				resize_client(f->client, &f->rect);
	}
}

static void
match_horiz(Area *a, XRectangle *r) {
	Frame *f;

	for(f=a->frame; f; f=f->anext) {
		f->rect.x = r->x;
		f->rect.width = r->width;
		resize_frame(f, &f->rect);
	}
}

void
resize_column(Frame *f, XRectangle *new) {
	Area *west, *east, *a;
	Frame *north, *south;
	View *v;
	BlitzAlign sticky;
	uint min_height;
	uint min_width;

	a = f->area;
	v = a->view;
	min_height = 2 * labelh(&def.font);
	min_width = screen->rect.width/NCOL;

	for(west=v->area->next; west; west=west->next)
		if(west->next == a) break;
	east = a->next;
	for(north=a->frame; north; north=north->anext)
		if(north->anext == f) break;
	south = f->anext;
	/* validate (and trim if necessary) horizontal resize */
	sticky = get_sticky(&f->rect, new);
	if(new->width < min_width) {
		if(sticky & EAST)
			new->x = r_east(&a->rect) - min_width;
		new->width = min_width;
	}
	if(west && !(sticky & WEST)) {
		if(new->x < 0 || new->x < (west->rect.x + min_width)) {
			new->width -= (west->rect.x + min_width) - new->x;
			new->x = west->rect.x + min_width;
		}
	} else {
		new->width += new->x - a->rect.x;
		new->x = a->rect.x;
	}
	if(east && !(sticky & EAST)) {
		if(r_east(new) > r_east(&east->rect) - min_width)
			new->width = r_east(&east->rect) - min_width - new->x;
	} else
		new->width = r_east(&a->rect) - new->x;
	if(new->width < min_width)
		goto AfterHorizontal;
	/* horizontal resize */
	sticky = get_sticky(&a->rect, new);
	if(west && !(sticky & WEST)) {
		west->rect.width = new->x - west->rect.x;
		a->rect.width += a->rect.x - new->x;
		a->rect.x = new->x;
		match_horiz(a, &a->rect);
		match_horiz(west, &west->rect);
		//relax_column(west);
	}
	if(east && !(sticky & EAST)) {
		east->rect.width -= r_east(new) - east->rect.x;
		east->rect.x = r_east(new);
		a->rect.width = r_east(new) - a->rect.x;
		match_horiz(a, &a->rect);
		match_horiz(east, &east->rect);
	}
AfterHorizontal:
	/* skip vertical resize unless the column is in equal mode */
	if(a->mode != Coldefault)
		goto AfterVertical;
	/* validate (and trim if necessary) vertical resize */
	sticky = get_sticky(&f->rect, new);
	if(new->height < min_height) {
		if((f->rect.height < min_height) && sticky & (NORTH|SOUTH))
			goto AfterVertical;
		if(sticky & SOUTH)
			new->y = r_south(&f->rect) - min_height;
		new->height = min_height;
	}
	if(north && !(sticky & NORTH))
		if(new->y < 0 || new->y < (north->rect.y + min_height)) {
			new->height -= (north->rect.y + min_height) - new->y;
			new->y = north->rect.y + min_height;
		}
	if(south && !(sticky & SOUTH)) {
		if(r_south(new) > r_south(&south->rect) - min_height)
			new->height = r_south(&south->rect) - min_height - new->y;
	}
	if(new->height < min_height)
		goto AfterVertical;
	/* vertical resize */
	if(north && !(sticky & NORTH)) {
		north->rect.height = new->y - north->rect.y;
		f->rect.height += f->rect.y - new->y;
		f->rect.y = new->y;
		resize_frame(north, &north->rect);
		resize_frame(f, &f->rect);
	}
	if(south && !(sticky & SOUTH)) {
		south->rect.height -= r_south(new) - south->rect.y;
		south->rect.y = r_south(new);
		f->rect.y = new->y;
		f->rect.height = new->height;
		resize_frame(f, &f->rect);
		resize_frame(south, &south->rect);
	}
AfterVertical:
	arrange_column(a, False);
	focus_view(screen, v);
}

Area *
new_column(View *v, Area *pos, uint w) {
	Area *a = create_area(v, pos, w);
	if(!a)
		return nil;
	arrange_view(v);
	return a;
}
