/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "wmii.h"
#include <stdlib.h>
#include <string.h>

Frame *
create_frame(Client *c, View *v) {
	static unsigned short id = 1;
	Frame *f = ixp_emallocz(sizeof(Frame));

	f->id = id++;
	f->client = c;
	f->view = v;
	if(c->frame) {
		f->revert = c->sel->revert;
		f->rect = c->sel->rect;
	}
	else{
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
	f->tile.border = True;
	f->grabbox = f->titlebar = f->tile;
	f->titlebar.align = WEST;

	return f;
}

void
remove_frame(Frame *f) {
	Area *a = f->area;
	Frame **ft = &a->frame;

	for(; *ft && *ft != f; ft=&(*ft)->anext);
	*ft = f->anext;
}

void
insert_frame(Frame *pos, Frame *f, Bool before) {
	Area *a = f->area;

	if(before) {
		Frame *ft;
		for(ft=a->frame; ft && ft->anext != pos; ft=ft->anext);
		pos=ft;
	}
	Frame **p = pos ? &pos->anext : &a->frame;
	f->anext = *p;
	*p = f;
}

void
update_frame_widget_colors(Frame *f) {
	if(sel_screen && (f->client == sel_client()))
		f->tile.color = f->titlebar.color = def.selcolor;
	else
		f->tile.color = f->titlebar.color = def.normcolor;
	if(f->area->sel == f)
		f->grabbox.color = def.selcolor;
	else
		f->grabbox.color = def.normcolor;
}

void
draw_frame(Frame *f) {
	if(def.border) {
		f->tile.rect = f->rect;
		f->tile.rect.x = f->tile.rect.y = 0;
	}
	f->grabbox.rect = f->tile.rect;
	f->grabbox.rect.height = labelh(&def.font);
	f->grabbox.rect.width = def.font.height;
	f->titlebar.rect = f->grabbox.rect;
	f->titlebar.rect.x = f->grabbox.rect.x + f->grabbox.rect.width;
	f->titlebar.rect.width = f->rect.width -  f->titlebar.rect.x;
	draw_tile(&f->tile);
	draw_tile(&f->grabbox);
	draw_label(&f->titlebar, f->client->name);
	XCopyArea(blz.dpy, pmap, f->client->framewin, f->client->gc,
			0, 0, f->rect.width, f->rect.height, 0, 0);
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
