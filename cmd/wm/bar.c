/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>

#include "wm.h"

Label *
new_label()
{
	static unsigned int id = 1;
	Label *l = cext_emallocz(sizeof(Label));
	l->id = id++;
	cext_strlcpy(l->colstr, def.selcolor, sizeof(l->colstr));
	l->color = def.sel;
	label = (Label **)cext_array_attach((void **)label, l, sizeof(Label *), &labelsz);
	nlabel++;
	return l;
}

void
detach_label(Label *l)
{
	cext_array_detach((void **)label, l, &labelsz);
	nlabel--;
}

void
update_bar_geometry()
{
	size_t i, j;
    brect = rect;
    brect.height = xfont->ascent + xfont->descent + 4;
    brect.y = rect.height - brect.height;
    XMoveResizeWindow(dpy, winbar, brect.x, brect.y, brect.width, brect.height);
    XSync(dpy, False);
    XFreePixmap(dpy, pmapbar);
    pmapbar = XCreatePixmap(dpy, winbar, brect.width, brect.height, DefaultDepth(dpy, screen));
    XSync(dpy, False);
	draw_bar();
	for(i = 0; i < npage; i++)
		for(j = 1; j < page[i]->narea; j++) {
			update_area_geometry(page[i]->area[j]);
			arrange_column(page[i]->area[j]);
		}
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

	if(!nlabel) { /* /default only */
		d.color = def.sel;
		blitz_drawlabel(dpy, &d);
	}
	else {
		for(i = 0; i < nlabel; i++) {
			Label *l = label[i];
			l->rect.x = l->rect.y = 0;
			l->rect.height = brect.height;
			if(i == iexpand)
		   		continue;
			l->rect.width = brect.height;
			if(strlen(l->data)) {
				if(!strncmp(l->data, "%m:", 3))
					l->rect.width = brect.height / 2;
				else
					l->rect.width += XTextWidth(xfont, l->data, strlen(l->data));
			}
			w += l->rect.width;
		}

		if(w >= brect.width) {
			/* failsafe mode, give all labels same width */
			w = brect.width / nlabel;
			for(i = 0; i < nlabel; i++) {
				label[i]->rect.x = i * w;
				label[i]->rect.width = w;
			}
			i--;
			label[i]->rect.width = brect.width - label[i]->rect.x;
		}
		else {
			label[iexpand]->rect.width = brect.width - w;
			for(i = 1; i < nlabel; i++)
				label[i]->rect.x = label[i - 1]->rect.x + label[i - 1]->rect.width;
		}

		for(i = 0; i < nlabel; i++) {
			d.color = label[i]->color;
			d.rect = label[i]->rect;
			d.data = label[i]->data;
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
lid_to_index(unsigned short id)
{
	int i;
	for(i = 0; i < nlabel; i++)
		if(label[i]->id == id)
			return i;
	return -1;
}
