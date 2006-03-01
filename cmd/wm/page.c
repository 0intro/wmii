/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

#include "wm.h"

Page *
alloc_page()
{
	static unsigned short id = 1;
    Page *p = cext_emallocz(sizeof(Page));

	p->id = id++;
	alloc_area(p);
	alloc_area(p);
	page = (Page **)cext_array_attach((void **)page, p, sizeof(Page *), &pagesz);
	npage++;
	focus_page(p);
    XChangeProperty(dpy, root, net_atoms[NET_NUMBER_OF_DESKTOPS], XA_CARDINAL,
			        32, PropModeReplace, (unsigned char *) &npage, 1);
    return p;
}

char *
destroy_page(Page *p)
{
	unsigned int i;
	Page *old = p->revert;
	Client *c;

	for(i = 0; i < p->narea; i++)
		if(p->area[i]->nclient)
			return "page not empty";

	while(p->narea)
		destroy_area(p->area[0]);

	cext_array_detach((void **)page, p, &pagesz);
	npage--;

	for(i = 0; i < npage; i++) {
		if(page[i]->revert == p)
			page[i]->revert = nil;
		XChangeProperty(dpy, root, net_atoms[NET_NUMBER_OF_DESKTOPS], XA_CARDINAL,
				32, PropModeReplace, (unsigned char *) &i, 1);
	}

    free(p); 
    if(npage && old) {
        focus_page(old);
		if((c = sel_client_of_page(old)))
			focus_client(c);
	}
	else
		write_event("PN -\n");
	return nil;
}

int
page2index(Page *p)
{
	int i;
	for(i = 0; i < npage; i++)
		if(p == page[i])
			return i;
	return -1;
}

void
focus_page(Page *p)
{
	Page *old = page ? page[sel] : nil;
	char buf[16];
	Client *c;
	int i, pi = page2index(p);
	int px;

	if(!npage || (pi == -1))
		return;

	if(old && old != p)
		p->revert = old;
	sel = pi;
	px = sel * rect.width;
	/* gives all(!) clients proper geometry (for use of different pagers) */
	for(i = 0; i < nclient; i++) {
		c = client[i];
		if(c->area) {
			pi = page2index(c->area->page);
			XMoveWindow(dpy, c->frame.win, px - (pi * rect.width) + c->frame.rect.x, c->frame.rect.y);
			if(c->area->page == p)
				draw_client(c);
		}
	}
	snprintf(buf, sizeof(buf), "PN %d\n", sel + 1);
	write_event(buf);
    XChangeProperty(dpy, root, net_atoms[NET_CURRENT_DESKTOP], XA_CARDINAL,
			        32, PropModeReplace, (unsigned char *) &sel, 1);
	XSync(dpy, False);
}

XRectangle *
rectangles(unsigned int *num)
{
    XRectangle *result = 0;
    int i, j = 0;
    Window d1, d2;
    Window *wins;
    XWindowAttributes wa;
    XRectangle r;

    if(XQueryTree(dpy, root, &d1, &d2, &wins, num)) {
        result = cext_emallocz(*num * sizeof(XRectangle));
        for(i = 0; i < *num; i++) {
            if(!XGetWindowAttributes(dpy, wins[i], &wa))
                continue;
            if(wa.override_redirect && (wa.map_state == IsViewable)) {
                r.x = wa.x;
                r.y = wa.y;
                r.width = wa.width;
                r.height = wa.height;
                result[j++] = r;
            }
        }
    }
    if(wins)
        XFree(wins);
    *num = j;
    return result;
}

int
pid2index(unsigned short id)
{
	int i;
	for(i = 0; i < npage; i++)
		if(page[i]->id == id)
			return i;
	return -1;
}

void
select_page(char *arg)
{
	size_t new = sel;
	const char *err;
	Client *c;

    if(!npage)
        return;
    if(!strncmp(arg, "prev", 5)) {
		if(!new)
			new = npage;
		new--;
    } else if(!strncmp(arg, "next", 5)) {
		if(new < npage - 1)
			new++;
		else
			new = 0;
    } else {
		int idx = cext_strtonum(arg, 1, npage, &err);
		if(idx && (idx - 1 < npage))
			new = idx - 1;
	}
    focus_page(page[new]);
	if((c = sel_client_of_page(page[new])))
		focus_client(c);
}
