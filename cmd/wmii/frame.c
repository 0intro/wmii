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
	Frame *f;
	
	f = emallocz(sizeof *f);
	f->id = id++;
	f->client = c;
	f->view = v;

	if(c->sel) {
		f->revert = c->sel->revert;
		f->rect = c->sel->rect;
	}
	else{
		c->sel = f;
		f->rect = c->rect;
		f->rect.max.x += 2 * def.border;
		f->rect.max.y += frame_delta_h();
		f->revert = f->rect;
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

Rectangle
frame2client(Frame *f, Rectangle r) {
	if(f->area->floating) {
		r.max.x -= def.border * 2;
		r.max.y -= frame_delta_h();
	}else {
		r.max.x -= 2;
		r.max.y -= labelh(def.font) + 1;
	}
	r.max.x = max(r.min.x+1, r.max.x);
	r.max.y = max(r.min.y+1, r.max.y);
	return r;
}

Rectangle
client2frame(Frame *f, Rectangle r) {
	if(f->area->floating) {
		r.max.x += def.border * 2;
		r.max.y += frame_delta_h();
	}else {
		r.max.x += 2;
		r.max.y += labelh(def.font) + 1;
	}
	return r;
}

/* Handlers */
static void
bup_event(Window *w, XButtonEvent *e) {
	write_event("ClientClick 0x%x %d\n", (uint)w->w, e->button);
}

static void
bdown_event(Window *w, XButtonEvent *e) {
	Frame *f;
	Client *c;

	c = w->aux;
	f = c->sel;

	if((e->state & def.mod) == def.mod) {
		switch(e->button) {
		case Button1:
			do_mouse_resize(c, False, CENTER);
			focus(c, True);
			frame_to_top(f);
			focus(c, True);
			break;
		case Button3:
			do_mouse_resize(c, False, quadrant(f->rect, Pt(e->x_root, e->y_root)));
			frame_to_top(f);
			focus(c, True);
			break;
		default: break;
			XAllowEvents(display, ReplayPointer, e->time);
		}
	}else{
		if(e->button == Button1) {
			if(frame_to_top(f))
				restack_view(f->view);

			else if(ptinrect(Pt(e->x, e->y), f->grabbox))
				do_mouse_resize(c, True, CENTER);
			else if(f->area->floating)
				if(!e->subwindow && !ptinrect(Pt(e->x, e->y), f->titlebar))
					do_mouse_resize(c, False, quadrant(f->rect, Pt(e->x_root, e->y_root)));

			if(f->client != selclient())
				focus(c, True);
		}
		if(e->subwindow)
			XAllowEvents(display, ReplayPointer, e->time);
		else {
			/* Ungrab so a menu can receive events before the button is released */
			XUngrabPointer(display, e->time);
			XSync(display, False);

			write_event("ClientMouseDown 0x%x %d\n", f->client->win.w, e->button);
		}
	}
}

static void
enter_event(Window *w, XCrossingEvent *e) {
	Client *c;
	Frame *f;

	c = w->aux;
	f = c->sel;
	if(screen->focus != c) {
		if(verbose) fprintf(stderr, "enter_notify(f) => %s\n", f->client->name);
		if(f->area->floating || !f->collapsed)
			focus(f->client, False);
	}
	set_frame_cursor(f, Pt(e->x, e->y));
}

static void
expose_event(Window *w, XExposeEvent *e) {
	Client *c;
	
	c = w->aux;
	if(c->sel)
		draw_frame(c->sel);
	else
		fprintf(stderr, "Badness: Expose event on a client frame which shouldn't be visible: %x\n",
			(uint)c->win.w);
}

static void
motion_event(Window *w, XMotionEvent *e) {
	Client *c;
	
	c = w->aux;
	set_frame_cursor(c->sel, Pt(e->x, e->y));
}

Handlers framehandler = {
	.bup = bup_event,
	.bdown = bdown_event,
	.enter = enter_event,
	.expose = expose_event,
	.motion = motion_event,
};

void
resize_frame(Frame *f, Rectangle r) {
	Align stickycorner;
	Point pt;
	Client *c;

	c = f->client;
	stickycorner = get_sticky(f->rect, r);

	f->crect = r;
	apply_sizehints(c, &f->crect, f->area->floating, True, stickycorner);

	if(Dx(r) <= 0 || Dy(r) <= 0)
		fprintf(stderr, "Badness: Frame rect: %d,%d %dx%d\n",
			r.min.x, r.min.y, Dx(r), Dy(r));

	if(f->area->floating)
		f->rect = f->crect;
	else
		f->rect = r;

	f->crect = frame2client(f, f->crect);
	f->crect = rectsubpt(f->crect, f->crect.min);

	if(Dx(f->crect) < labelh(def.font)) {
		f->rect.max.x = f->rect.min.x + frame_delta_h();
		f->collapsed = True;
	}

	if(f->collapsed) {
		f->rect.max.y= f->rect.min.y + labelh(def.font);
		f->crect = f->rect;
	}

	pt.y = labelh(def.font);

	if(f->area->floating) {
		if(c->fullscreen) {
			f->crect = screen->rect;
			f->rect = client2frame(f, f->crect);
			pt.x = (Dx(f->rect) - Dx(f->crect)) / 2;
			f->rect = rectsubpt(f->rect, pt);
		}else
			f->rect = constrain(f->rect);
	}
	pt.x = (Dx(f->rect) - Dx(f->crect)) / 2;
	f->crect = rectaddpt(f->crect, pt);
}

void
set_frame_cursor(Frame *f, Point pt) {
	Rectangle r;
	Cursor cur;

	if(f->area->floating
	&& !ptinrect(pt, f->titlebar)
	&& !ptinrect(pt, f->crect)) {
	 	r = rectsubpt(f->rect, f->rect.min);
	 	cur = cursor_of_quad(quadrant(r, pt));
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

	return True;
}

void
swap_frames(Frame *fa, Frame *fb) {
	Rectangle trect;
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
	return def.border + labelh(def.font);
}

void
draw_frame(Frame *f) {
	Rectangle r, fr;
	CTuple *col;
	Frame *tf;

	if(f->view != screen->sel)
		return;

	if(f->client == screen->focus)
		col = &def.focuscolor;
	else
		col = &def.normcolor;
	if(!f->area->floating && f->area->mode == Colmax)
		for(tf = f->area->frame; tf; tf=tf->anext)
			if(tf->client == screen->focus) {
				col = &def.focuscolor;
				break;
			}

	/* background */
	fr = rectsubpt(f->rect, f->rect.min);
	r = fr;
	fill(screen->ibuf, r, col->bg);
	border(screen->ibuf, r, 1, col->border);

	r.max.y = r.min.y + labelh(def.font);
	border(screen->ibuf, r, 1, col->border);

	f->titlebar = insetrect(r, 3);
	f->titlebar.max.y += 3;

	/* grabbox */
	r.min = Pt(2, 2);
	r.max.x = r.min.x + def.font->height - 3;
	r.max.y -= 2;
	f->grabbox = r;

	if(f->client->urgent)
		fill(screen->ibuf, r, col->fg);
	border(screen->ibuf, r, 1, col->border);

	r.min.x = r.max.x;
	r.max.x = fr.max.x;
	r.min.y = 0;
	r.max.y = labelh(def.font);
	drawstring(screen->ibuf, def.font, r, WEST,
			f->client->name, col->fg);

	copyimage(f->client->framewin, fr, screen->ibuf, ZP);
	XSync(display, False);
}

void
draw_frames() {
	Client *c;

	for(c=client; c; c=c->next)
		if(c->sel && c->sel->view == screen->sel)
			draw_frame(c->sel);
}

Rectangle
constrain(Rectangle r) {
	Rectangle sr;
	int barheight;

	sr = screen->rect;
	barheight = Dy(screen->brect);
	sr.max.y -= barheight;

	if(Dx(r) > Dx(sr))
		r.max.x = r.min.x + Dx(sr);
	if(Dy(r) > Dy(sr))
		r.max.y = r.min.y + Dy(sr);
	if(r.min.x > sr.max.x - barheight)
		rectsubpt(r, Pt(sr.min.x - sr.max.x + barheight, 0));
	if(r.min.y > sr.max.y - barheight)
		rectsubpt(r, Pt(0, sr.min.y - sr.max.y + barheight));
	if(r.max.x < barheight)
		rectaddpt(r, Pt(barheight - sr.max.x, 0));
	if(r.max.y < barheight)
		rectaddpt(r, Pt(0, barheight - sr.max.y));
	return r;
}
