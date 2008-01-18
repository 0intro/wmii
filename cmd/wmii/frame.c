/* Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <math.h>
#include "fns.h"

uint
frame_idx(Frame *f) {
	Frame *fp;
	uint i;

	fp = f->area->frame;
	for(i = 1; fp != f; fp = fp->anext)
		i++;
	return i;
}

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
		f->r = c->sel->r;
	}
	else{
		f->r = client2frame(f, gravclient(c, ZR));
		f->revert = f->r;
		c->sel = f;
	}
	f->collapsed = False;

	return f;
}

void
remove_frame(Frame *f) {
	Area *a;

	a = f->area;
	if(f->aprev)
		f->aprev->anext = f->anext;
	if(f->anext)
		f->anext->aprev = f->aprev;
	if(f == a->frame)
		a->frame = f->anext;

	if(a->floating) {
		if(f->sprev)
			f->sprev->snext = f->snext;
		if(f->snext)
			f->snext->sprev = f->sprev;
		if(f == a->stack)
			a->stack = f->snext;
	}
	f->anext = f->aprev = f->snext = f->sprev = nil;
}

void
insert_frame(Frame *pos, Frame *f) {
	Area *a;

	a = f->area;

	if(pos) {
		f->aprev = pos;
		f->anext = pos->anext;
	}else {
		f->anext = f->area->frame;
		f->area->frame = f;
	}
	if(f->aprev)
		f->aprev->anext = f;
	if(f->anext)
		f->anext->aprev = f;

	if(a->floating) {
		f->snext = a->stack;
		a->stack = f;
		if(f->snext)
			f->snext->sprev = f;
	}
}

Bool
frame_to_top(Frame *f) {
	Area *a;

	a = f->area;
	if(!a->floating || f == a->stack)
		return False;

	if(f->sprev)
		f->sprev->snext = f->snext;
	if(f->snext)
		f->snext->sprev = f->sprev;

	f->snext = a->stack;
	a->stack = f;
	f->sprev = nil;
	if(f->snext)
		f->snext->sprev = f;

	return True;
}

/* Handlers */
static void
bup_event(Window *w, XButtonEvent *e) {
	write_event("ClientClick %C %d\n", w->aux, e->button);
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
			do_mouse_resize(c, False, quadrant(f->r, Pt(e->x_root, e->y_root)));
			frame_to_top(f);
			focus(c, True);
			break;
		default:
			XAllowEvents(display, ReplayPointer, e->time);
			break;
		}
		if(e->button != Button1)
			XUngrabPointer(display, e->time);
	}else{
		if(e->button == Button1) {
			if(frame_to_top(f))
				restack_view(f->view);
			else if(ptinrect(Pt(e->x, e->y), f->grabbox))
				do_mouse_resize(c, True, CENTER);
			else if(f->area->floating)
				if(!e->subwindow && !ptinrect(Pt(e->x, e->y), f->titlebar))
					do_mouse_resize(c, False, quadrant(f->r, Pt(e->x_root, e->y_root)));

			if(f->client != selclient())
				focus(c, True);
		}
		if(e->subwindow)
			XAllowEvents(display, ReplayPointer, e->time);
		else {
			/* Ungrab so a menu can receive events before the button is released */
			XUngrabPointer(display, e->time);
			XSync(display, False);

			write_event("ClientMouseDown %C %d\n", f->client, e->button);
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
		Dprint("enter_notify(f) => %s\n", f->client->name);
		if(f->area->floating || !f->collapsed)
			focus(f->client, False);
	}
	set_frame_cursor(f, Pt(e->x, e->y));
}

static void
expose_event(Window *w, XExposeEvent *e) {
	Client *c;

	USED(e);

	c = w->aux;
	if(c->sel)
		draw_frame(c->sel);
	else
		fprint(2, "Badness: Expose event on a client frame which shouldn't be visible: %C\n",
			c);
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

Rectangle
frame2client(Frame *f, Rectangle r) {
	if(f == nil || f->area == nil || f->area->floating) {
		r.max.x -= def.border * 2;
		r.max.y -= frame_delta_h();
		if(f) {
			if(f->client->borderless) {
				r.max.x += 2 * def.border;
				r.max.y += def.border;
			}
			if(f->client->titleless)
				r.max.y += labelh(def.font);
		}
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
	if(f == nil || f->area == nil ||  f->area->floating) {
		r.max.x += def.border * 2;
		r.max.y += frame_delta_h();
		if(f) {
			if(f->client->borderless) {
				r.max.x -= 2 * def.border;
				r.max.y -= def.border;
			}
			if(f->client->titleless)
				r.max.y -= labelh(def.font);
		}
	}else {
		r.max.x += 2;
		r.max.y += labelh(def.font) + 1;
	}
	return r;
}

void
resize_frame(Frame *f, Rectangle r) {
	Align stickycorner;
	Point pt;
	Client *c;

	c = f->client;
	stickycorner = get_sticky(f->r, r);

	f->crect = frame_hints(f, r, stickycorner);

	if(Dx(r) <= 0 || Dy(r) <= 0)
		fprint(2, "Badness: Frame rect: %R\n", r);

	if(f->area->floating)
		f->r = f->crect;
	else
		f->r = r;

	f->crect = frame2client(f, f->crect);
	f->crect = rectsubpt(f->crect, f->crect.min);

	if(!f->area->floating && f->area->mode == Coldefault) {
		if(Dy(f->r) < 2 * labelh(def.font))
			f->collapsed = True;
		else
			f->collapsed = False;
	}

	if(Dx(f->crect) < labelh(def.font)) {
		f->r.max.x = f->r.min.x + frame_delta_h();
		f->collapsed = True;
	}

	if(f->collapsed) {
		f->r.max.y= f->r.min.y + labelh(def.font);
		f->crect = f->r;
	}

	pt = ZP;
	if(!f->client->borderless || !f->area->floating)
		pt.y += 1;
	if(!f->client->titleless || !f->area->floating)
		pt.y += labelh(def.font) - 1;

	if(f->area->floating) {
		if(c->fullscreen) {
			f->crect = screen->r;
			f->r = client2frame(f, f->crect);
			pt.x = (Dx(f->r) - Dx(f->crect)) / 2;
			f->r = rectsubpt(f->r, pt);
		}else
			f->r = constrain(f->r);
	}
	pt.x = (Dx(f->r) - Dx(f->crect)) / 2;
	f->crect = rectaddpt(f->crect, pt);
}

void
set_frame_cursor(Frame *f, Point pt) {
	Rectangle r;
	Cursor cur;

	if(f->area->floating
	&& !ptinrect(pt, f->titlebar)
	&& !ptinrect(pt, f->crect)) {
	 	r = rectsubpt(f->r, f->r.min);
	 	cur = cursor_of_quad(quadrant(r, pt));
		set_cursor(f->client, cur);
	} else
		set_cursor(f->client, cursor[CurNormal]);
}

void
swap_frames(Frame *fa, Frame *fb) {
	Frame **fp;
	Client *c;

	if(fa == fb) return;

	for(fp = &fa->client->frame; *fp; fp = &fp[0]->cnext)
		if(*fp == fa) break;
	fp[0] = fp[0]->cnext;

	for(fp = &fb->client->frame; *fp; fp = &fp[0]->cnext)
		if(*fp == fb) break;
	fp[0] = fp[0]->cnext;

	c = fa->client;
	fa->client = fb->client;
	fb->client = c;
	fb->cnext = c->frame;
	c->frame = fb;

	c = fa->client;
	fa->cnext = c->frame;
	c->frame = fa;

	if(c->sel && c->sel->view == screen->sel)
		focus_view(screen, c->sel->view);
}

void
focus_frame(Frame *f, Bool restack) {
	View *v;
	Area *a, *old_a;

	a = f->area;
	v = f->view;
	old_a = v->sel;

	a->sel = f;

	if(a != old_a)
		focus_area(f->area);

	if(v != screen->sel)
		return;

	focus_client(f->client);

	if(!a->floating && ((a->mode == Colstack) || (a->mode == Colmax)))
		arrange_column(a, False);

	if(restack)
		restack_view(v);
}

int
frame_delta_h(void) {
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
	fr = f->client->framewin->r;
	fr = rectsubpt(fr, fr.min);

	/* background */
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

	XSetWindowBackgroundPixmap(display, f->client->framewin->w, None);
	copyimage(f->client->framewin, fr, screen->ibuf, ZP);
	XSync(display, False);
}

void
draw_frames(void) {
	Client *c;

	for(c=client; c; c=c->next)
		if(c->sel && c->sel->view == screen->sel)
			draw_frame(c->sel);
}

Rectangle
constrain(Rectangle r) {
	Rectangle sr;
	Point p;

	sr = screen->r;
	sr.max.y = screen->brect.min.y;

	if(Dx(r) > Dx(sr))
		r.max.x = r.min.x + Dx(sr);
	if(Dy(r) > Dy(sr))
		r.max.y = r.min.y + Dy(sr);

	sr = insetrect(sr, Dy(screen->brect));
	p = ZP;
	p.x -= min(r.max.x - sr.min.x, 0);
	p.x -= max(r.min.x - sr.max.x, 0);
	p.y -= min(r.max.y - sr.min.y, 0);
	p.y -= max(r.min.y - sr.max.y, 0);
	return rectaddpt(r, p);
}
