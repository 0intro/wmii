/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "wmii.h"
#include <stdlib.h>
#include <string.h>

Frame *
create_frame(Client *c, View *v) {
	static ushort id = 1;
	Frame *f = ixp_emallocz(sizeof(Frame));

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
		f->revert.height = f->rect.height += def.border + labelh(&def.font);
	}
	f->collapsed = False;
	f->tile.blitz = &blz;
	f->tile.drawable = pmap;
	f->tile.gc = c->gc;
	f->tile.font = &def.font;
	f->tile.color = def.normcolor;
	f->tile.border = False;
	f->grabbox = f->titlebar = f->tile;
	f->titlebar.align = WEST;

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
	Area *a = f->area;

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
resize_frame(Frame *f, XRectangle *r) {
	BlitzAlign stickycorner;
	Client *c;

	c = f->client;
	stickycorner = get_sticky(&f->rect, r);

	f->rect = *r;
	if(f->area->floating)
		match_sizehints(c, &f->rect, f->area->floating, stickycorner);
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
	if(!a->floating && ((a->mode == Colstack)
		|| (a->mode == Colmax)))
		arrange_column(a, False);

	if(a != old_a)
		focus_area(f->area);
	else {
		update_frame_widget_colors(f);
		if(old)
			update_frame_widget_colors(old);
	}

	if(old_in_a)
		update_frame_widget_colors(old_in_a);

	if(v != screen->sel)
		return;

	if(a == old_a) {
		focus_client(f->client);
		draw_frame(f);
	}
	else if(old_in_a)
		draw_frame(old_in_a);

	if(f != old) {
		if(old)
			draw_frame(old);
		if(f->area == old_a)
			write_event("ClientFocus 0x%x\n", f->client->win);
	}

	if(restack)
		restack_view(v);
}

void
update_frame_widget_colors(Frame *f) {
	if(f->area->sel == f) {
		if(sel_screen && (f->client == sel_client()))
			f->grabbox.color = f->tile.color = f->titlebar.color = def.focuscolor;
		else
			f->grabbox.color = f->tile.color = f->titlebar.color = def.selcolor;
	}
	else
		f->grabbox.color = f->tile.color = f->titlebar.color = def.normcolor;
	if(f->client->urgent)
		f->grabbox.color.bg = f->grabbox.color.fg;
}

void
draw_frame(Frame *f) {
	if(def.border) {
		f->tile.border = 1;
		f->tile.rect = f->rect;
		f->tile.rect.x = f->tile.rect.y = 0;
	}
	f->grabbox.rect = f->tile.rect;
	f->grabbox.rect.x += 2;
	f->grabbox.rect.y += 2;
	f->grabbox.rect.height = labelh(&def.font) - 4;
	f->grabbox.rect.width = def.font.height - 3;
	f->grabbox.border = 1;
	f->titlebar.rect.x = f->grabbox.rect.x + f->grabbox.rect.width;
	f->titlebar.rect.y = f->tile.rect.y;
	f->titlebar.rect.width = f->rect.width - f->titlebar.rect.x;
	f->titlebar.rect.height = labelh(&def.font);
	f->titlebar.border = 0;
	draw_tile(&f->tile);
	draw_label(&f->titlebar, f->client->name);
	draw_border(&f->tile);
	/* XXX: Hack */
	f->titlebar.rect.x = 0;
	f->titlebar.rect.width = f->rect.width;
	f->titlebar.border = 1;
	draw_border(&f->titlebar);
	draw_tile(&f->grabbox);
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
		if(c->sel && c->sel->view == screen->sel) {
			update_frame_widget_colors(c->sel);
			draw_frame(c->sel);
		}
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
	if(rect->x + rect->width < barheight)
		rect->x = barheight - rect->width;
	if(rect->y + rect->height < barheight)
		rect->y = barheight - rect->height;
}
