/* Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <math.h>
#include <stdio.h>
#include <util.h>
#include "dat.h"
#include "fns.h"

Frame *
create_frame(Client *c, View *v) {
	static ushort id = 1;
	Frame *f = emallocz(sizeof(Frame));

	f->id = id++;
	f->client = c;
	f->view = v;
	if(c->sel) {
		f->revert = c->sel->revert;
		f->rect = c->sel->rect;
	}
	else{
		c->sel = f;
		f->revert = f->rect = c->rect;
		f->revert.width = f->rect.width += 2 * def.border;
		f->revert.height = f->rect.height += frame_delta_h();
	}
	f->collapsed = False;

	return f;
}

void
remove_frame(Frame *f) {
	Area *a;
	Frame **ft;

	a = f->area;
	for(ft = &a->frame; *ft; ft=&(*ft)->anext)
		if(*ft == f) break;
	*ft = f->anext;

	if(a->floating) {
		for(ft = &a->stack; *ft; ft=&(*ft)->snext)
			if(*ft == f) break;
		*ft = f->snext;
	}
}

void
insert_frame(Frame *pos, Frame *f, Bool before) {
	Frame *ft, **p;
	Area *a;

	a = f->area;

	if(before) {
		for(ft=a->frame; ft; ft=ft->anext)
			if(ft->anext == pos) break;
		pos=ft;
	}

	p = &a->frame;
	if(pos)
		p = &pos->anext;

	f->anext = *p;
	*p = f;

	if(a->floating) {
		f->snext = a->stack;
		a->stack = f;
	}
}

void
frame2client(Frame *f, XRectangle *r) {
	if(f->area->floating) {
		r->width = max(r->width - def.border * 2, 1);
		r->height = max(r->height - frame_delta_h(), 1);
	}else {
		r->width = max(r->width - 2, 1);
		r->height = max(r->height - labelh(&def.font) - 1, 1);
	}
}

void
client2frame(Frame *f, XRectangle *r) {
	if(f->area->floating) {
		r->width += def.border * 2;
		r->height += frame_delta_h();
	}else {
		r->width += 2;
		r->height +=labelh(&def.font) + 1;
	}
}

void
resize_frame(Frame *f, XRectangle *r) {
	BlitzAlign stickycorner;
	Client *c;

	c = f->client;
	stickycorner = get_sticky(&f->rect, r);

	f->rect = *r;
	f->crect = *r;
	apply_sizehints(c, &f->crect, f->area->floating, True, stickycorner);

	if(f->area->floating)
		f->rect = f->crect;

	frame2client(f, &f->crect);

	if(f->crect.height < labelh(&def.font))
		f->collapsed = True;
	else
		f->collapsed = False;

	if(f->crect.width < labelh(&def.font)) {
		f->rect.width = frame_delta_h();
		f->collapsed = True;
	}

	if(f->collapsed) {
		f->rect.height = labelh(&def.font);
		f->crect = f->rect;
	}
	f->crect.y = labelh(&def.font);
	f->crect.x = (f->rect.width - f->crect.width) / 2;
	

	if(f->area->floating) {
		if(c->fullscreen) {
			f->crect.width = screen->rect.width;
			f->crect.height = screen->rect.height;

			f->rect = f->crect;
			f->rect.x = -def.border;
			f->rect.y = -labelh(&def.font);
			client2frame(f, &f->rect);
		}else
			check_frame_constraints(&f->rect);
	}
}

void
set_frame_cursor(Frame *f, int x, int y) {
	XRectangle r;
	Cursor cur;

	if(f->area->floating
	&& !ptinrect(x, y, &f->titlebar)
	&& !ptinrect(x, y, &f->crect)
	&& !ingrabbox(f, x, y)) {
	 	r = f->rect;
	 	r.x = 0;
	 	r.y = 0;
	 	cur = cursor_of_quad(quadrant(&r, x, y));
		set_cursor(f->client, cur);
	} else
		set_cursor(f->client, cursor[CurNormal]);
}

Bool
frame_to_top(Frame *f) {
	Frame **tf;
	Area *a;

	a = f->area;
	if(!a->floating || f == a->stack)
		return False;
	for(tf=&a->stack; *tf; tf=&(*tf)->snext)
		if(*tf == f) break;
	*tf = f->snext;
	f->snext = a->stack;
	a->stack = f;
	update_client_grab(f->client);
	return True;
}

void
swap_frames(Frame *fa, Frame *fb) {
	XRectangle trect;
	Area *a;
	Frame **fp_a, **fp_b, *ft;

	if(fa == fb) return;

	a = fa->area;
	for(fp_a = &a->frame; *fp_a; fp_a = &(*fp_a)->anext)
		if(*fp_a == fa) break;
	a = fb->area;
	for(fp_b = &a->frame; *fp_b; fp_b = &(*fp_b)->anext)
		if(*fp_b == fb) break;

	if(fa->anext == fb) {
		*fp_a = fb;
		fa->anext = fb->anext;
		fb->anext = fa;
	} else if(fb->anext == fa) {
		*fp_b = fa;
		fb->anext = fa->anext;
		fa->anext = fb;
	} else {
		*fp_a = fb;
		*fp_b = fa;
		ft = fb->anext;
		fb->anext = fa->anext;
		fa->anext = ft;
	}

	if(fb->area->sel == fb)
		fb->area->sel = fa;
	if(fa->area->sel == fa)
		fa->area->sel = fb;

	fb->area = fa->area;
	fa->area = a;

	trect = fa->rect;
	fa->rect = fb->rect;
	fb->rect = trect;
}

void
focus_frame(Frame *f, Bool restack) {
	Frame *old, *old_in_a;
	View *v;
	Area *a, *old_a;

	a = f->area;
	v = f->view;
	old = v->sel->sel;
	old_a = v->sel;
	old_in_a = a->sel;

	a->sel = f;

	if(a != old_a)
		focus_area(f->area);

	if(v != screen->sel)
		return;

	focus_client(f->client);

	if(!a->floating
	&& ((a->mode == Colstack) || (a->mode == Colmax)))
		arrange_column(a, False);

	if((f != old)
	&& (f->area == old_a))
			write_event("ClientFocus 0x%x\n", f->client->win);

	if(restack)
		restack_view(v);
}

int
frame_delta_h() {
	return def.border + labelh(&def.font);
}

int
ingrabbox(Frame *f, int x, int y) {
	int dx, h;

	if(f->area->floating)
		return 0;

	h = labelh(&def.font) / 3;
	h = max(h, 4);

	if((f == f->area->frame) && f->area->next)
		if(x >= f->rect.width - h) {
			dx = x - (f->rect.width - h);
			if(y <= dx)
				return 1;
		}
	if((f == f->area->frame) && (f->area != f->view->area->next))
		if(x <= h && y <= h - x)
			return 1;

	return 0;
}

void
draw_frame(Frame *f) {
	BlitzBrush br = { 0 };
	Frame *tf;

	if(f->view != screen->sel)
		return;

	br.blitz = &blz;
	br.font = &def.font;
	br.drawable = pmap;
	br.gc = f->client->gc;
	if(f->client == screen->focus)
		br.color = def.focuscolor;
	else
		br.color = def.normcolor;
	if(!f->area->floating && f->area->mode == Colmax)
		for(tf = f->area->frame; tf; tf=tf->anext)
			if(tf->client == screen->focus) {
				br.color = def.focuscolor;
				break;
			}

	br.rect = f->rect;
	br.rect.x = 0;
	br.rect.y = 0;
	draw_tile(&br);

	br.rect.x += def.font.height - 3;
	br.rect.width -= br.rect.x;
	br.rect.height = labelh(&def.font);
	draw_label(&br, f->client->name);

	br.border = 1;
	br.rect.width += br.rect.x;
	br.rect.x = 0;
	f->titlebar.x = br.rect.x + 3;
	f->titlebar.height = br.rect.height - 3;
	f->titlebar.y = br.rect.y + 3;
	f->titlebar.width = br.rect.width - 6;
	draw_border(&br);
	br.rect.height = f->rect.height;
	if(def.border)
		draw_border(&br);

	if(f->client->urgent)
		br.color.bg = br.color.fg;
	br.rect.x = 2;
	br.rect.y = 2;
	br.rect.height = labelh(&def.font) - 4;
	br.rect.width = def.font.height - 3;
	f->grabbox = br.rect;
	draw_tile(&br);

#if 0
	if(!f->area->floating) {
		XSetLineAttributes(blz.dpy, br.gc, 1, LineSolid, CapButt, JoinMiter);
		h = labelh(&def.font) / 3;
		h = max(h, 4);
		if((f == f->area->frame) && f->area->next) {
			pt[0] = (XPoint){ f->rect.width - h, 0 };
			pt[1] = (XPoint){ f->rect.width, h };
			pt[2] = (XPoint){ f->rect.width, 0 };
			XSetForeground(blz.dpy, br.gc, def.normcolor.bg);
			XFillPolygon(blz.dpy, br.drawable, br.gc, pt, 3, Convex, CoordModeOrigin);
			XSetForeground(blz.dpy, br.gc, br.color.border);
			XDrawLines(blz.dpy, br.drawable, br.gc, pt, 2, CoordModeOrigin);
		}
		if((f == f->area->frame) && (f->area != f->view->area->next)) {
			pt[0] = (XPoint){ h, 0 };
			pt[1] = (XPoint){ 0, h };
			pt[2] = (XPoint){ 0, 0 };
			XSetForeground(blz.dpy, br.gc, def.normcolor.bg);
			XFillPolygon(blz.dpy, br.drawable, br.gc, pt, 3, Convex, CoordModeOrigin);
			XSetForeground(blz.dpy, br.gc, br.color.border);
			XDrawLines(blz.dpy, br.drawable, br.gc, pt, 2, CoordModeOrigin);
		}
	}
#endif

	XCopyArea(
		/* display */	blz.dpy,
		/* src */	pmap,
		/* dest */	f->client->framewin,
		/* gc */	f->client->gc,
		/* x, y */	0, 0,
		/* width */	f->rect.width,
		/* height */	f->rect.height,
		/* dest_x */	0,
		/* dest_y */	0
		);
	XSync(blz.dpy, False);
}

void
draw_frames() {
	Client *c;

	for(c=client; c; c=c->next)
		if(c->sel && c->sel->view == screen->sel)
			draw_frame(c->sel);
}

void
check_frame_constraints(XRectangle *rect) {
	int max_height;
	int barheight;

	barheight = screen->brect.height;
	max_height = screen->rect.height - barheight;

	if(rect->height > max_height)
		rect->height = max_height;
	if(rect->width > screen->rect.width)
		rect->width = screen->rect.width;
	if(rect->x + barheight > screen->rect.width)
		rect->x = screen->rect.width - barheight;
	if(rect->y + barheight > max_height)
		rect->y = max_height - barheight;
	if(r_east(rect) < barheight)
		rect->x = barheight - rect->width;
	if(r_south(rect) < barheight)
		rect->y = barheight - rect->height;
}
