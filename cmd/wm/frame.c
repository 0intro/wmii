/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "wm.h"

Frame *
create_frame(Client *c, View *v)
{
	static unsigned short id = 1;
	Frame *f = cext_emallocz(sizeof(Frame));

	f->id = id++;
	f->client = c;
	f->view = v;

	if(c->frame) {
		f->revert = c->sel->revert;
		f->rect = c->sel->rect;
	}else{
		f->revert = f->rect = c->rect;
		f->revert.width = f->rect.width += 2 * def.border;
		f->revert.height = f->rect.height += def.border + height_of_bar();
	}
	f->collapsed = False;

	f->tile.blitz = &blz;
	f->tile.drawable = pmap;
	f->tile.gc = c->gc;
	f->tile.font = &def.font;
	f->tile.color = def.normcolor;
	f->tile.border = True;
	f->titlebar = f->posbar = f->tile;
	f->titlebar.align = WEST;
	f->posbar.align = CENTER;

	f->tagbar.blitz = &blz;
	f->tagbar.drawable = pmap;
	f->tagbar.gc = c->gc;
	f->tagbar.font = &def.font;
	f->tagbar.color = def.normcolor;

	return f;
}

void
remove_frame(Frame *f)
{
	Area *a = f->area;
	Frame **ft = &a->frame;

	for(; *ft && *ft != f; ft=&(*ft)->anext);
	cext_assert(*ft == f);
	*ft = f->anext;
}

void
insert_frame(Frame *pos, Frame *f, Bool before)
{
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
update_frame_widget_colors(Frame *f)
{
 	if(sel_screen && (f->client == sel_client()))
		f->tagbar.color = f->tile.color = f->titlebar.color = def.selcolor;
	else
		f->tagbar.color = f->tile.color = f->titlebar.color = def.normcolor;

	if(f->area->sel == f)
		f->posbar.color = def.selcolor;
	else
		f->posbar.color = def.normcolor;
}

void
map_frame(Frame *f, XRectangle *r)
{
	XCopyArea(blz.display, pmap, f->client->framewin, f->client->gc,
			r->x, r->y, r->width, r->height, r->x, r->y);
	XSync(blz.display, False);
}

void
draw_frame(Frame *f)
{
	Frame *p;
	unsigned int fidx, size, w;
	XRectangle r = f->rect;

	for(fidx=0, p=f->area->frame; p && p != f; p=p->anext, fidx++);
	for(size=fidx; p; p=p->anext, size++);

	if(def.border) {
		f->tile.rect = f->rect;
		f->tile.rect.x = f->tile.rect.y = 0;
	}

	f->posbar.rect = f->tile.rect;
	f->posbar.rect.height = height_of_bar();

	snprintf(buffer, BUFFER_SIZE, "%s%d/%d",
		f->area->floating ? "~" : "", fidx + 1, size);

	w = f->posbar.rect.width =
		f->posbar.rect.height + blitz_textwidth(&def.font, buffer);

	f->posbar.rect.x = f->rect.width - f->posbar.rect.width; 
	
	/* tag bar */
	f->tagbar.rect = f->posbar.rect;
	f->tagbar.rect.x = 0;
	f->tagbar.rect.width = def.testtags ?
		f->tagbar.rect.height + blitz_textwidth(&def.font, def.testtags) :
		f->tagbar.rect.height + blitz_textwidth(&def.font, f->client->tags);

	if(f->tagbar.rect.width > f->rect.width / 3)
		f->tagbar.rect.width = f->rect.width / 3;

	f->titlebar.rect = f->tagbar.rect;
	f->titlebar.rect.x = f->tagbar.rect.x + f->tagbar.rect.width;
	f->titlebar.rect.width = f->rect.width - (f->tagbar.rect.width + f->posbar.rect.width);

	blitz_draw_tile(&f->tile);
	f->tagbar.text = def.testtags ? def.testtags : f->client->tags;
	blitz_draw_input(&f->tagbar);
	blitz_draw_label(&f->titlebar, f->client->name);
	blitz_draw_label(&f->posbar, buffer);
	r.x = r.y = 0;
	map_frame(f, &r);
}

void
draw_frames()
{
	Client *c;
	for(c=client; c; c=c->next)
		if(c->sel && (c->sel->area->view == sel)) {
			update_frame_widget_colors(c->sel);
			draw_frame(c->sel);
		}
}

