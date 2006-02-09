/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>

#include "wm.h"

Item *
new_item()
{
	static unsigned int id = 1;
	Item *it = cext_emallocz(sizeof(Item));
	it->id = id++;
	cext_strlcpy(it->colstr, def.selcolor, sizeof(it->colstr));
	it->color = def.sel;
	item = (Item **)cext_array_attach((void **)item, it, sizeof(Item *), &itemsz);
	nitem++;
	return it;
}

void
detach_item(Item *it)
{
	cext_array_detach((void **)item, it, &itemsz);
	nitem--;
}

void
draw_bar()
{
	size_t i;
	unsigned int w = 0;
	Draw d = { 0 };

	d.align = WEST;
    d.gc = gcbar;
    d.drawable = pmapbar;
    d.rect = brect;
    d.rect.y = 0;
	d.font = xfont;

	if(!nitem) { /* /default only */
		d.color = def.sel;
		blitz_drawlabel(dpy, &d);
	}
	else {
		for(i = 0; i < nitem; i++) {
			Item *it = item[i];
			it->rect.x = it->rect.y = 0;
			it->rect.height = brect.height;
			if(i == iexpand)
		   		continue;
			it->rect.width = brect.height;
			if(strlen(it->data)) {
				if(!strncmp(it->data, "%m:", 3))
					it->rect.width = brect.height / 2;
				else
					it->rect.width += XTextWidth(xfont, it->data, strlen(it->data));
			}
			w += it->rect.width;
		}

		if(w >= brect.width) {
			/* failsafe mode, give all labels same width */
			w = brect.width / nitem;
			for(i = 0; i < nitem; i++) {
				item[i]->rect.x = i * w;
				item[i]->rect.width = w;
			}
			i--;
			item[i]->rect.width = brect.width - item[i]->rect.x;
		}
		else {
			item[iexpand]->rect.width = brect.width - w;
			for(i = 1; i < nitem; i++)
				item[i]->rect.x = item[i - 1]->rect.x + item[i - 1]->rect.width;
		}

		for(i = 0; i < nitem; i++) {
			d.color = item[i]->color;
			d.rect = item[i]->rect;
			d.data = item[i]->data;
			if(d.data && !strncmp(d.data, "%m:", 3))
				blitz_drawmeter(dpy, &d);
			else
				blitz_drawlabel(dpy, &d);
		}
	}
    XCopyArea(dpy, pmapbar, winbar, gcbar, 0, 0, brect.width, brect.height, 0, 0);
    XSync(dpy, False);
}

int
index_of_bar_id(unsigned short id)
{
	int i;
	for(i = 0; i < nitem; i++)
		if(item[i]->id == id)
			return i;
	return -1;
}
