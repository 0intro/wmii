/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

#include "wm.h"

static void select_client(void *obj, char *arg);
static void xexec(void *obj, char *arg);
static void swap_client(void *obj, char *arg);
static void xnew_column(void *obj, char *arg);

/* action table for /?/ namespace */
Action page_acttbl[] = {
    {"exec", xexec},
    {"swap", swap_client},
    {"newcol", xnew_column},
    {"select", select_client},
    {0, 0}
};

Page *
alloc_page()
{
    Page *p = cext_emallocz(sizeof(Page));
	Area *a = cext_emallocz(sizeof(Area));

	p->area = (Area **)cext_array_attach((void **)p->area, a, sizeof(Area *), &p->areasz);
	p->narea++;
	do_pend_fcall("NewPage\n");
	page = (Page **)cext_array_attach((void **)page, p, sizeof(Page *), &pagesz);
	npage++;
	focus_page(p);
    XChangeProperty(dpy, root, net_atoms[NET_NUMBER_OF_DESKTOPS], XA_CARDINAL,
			        32, PropModeReplace, (unsigned char *) &npage, 1);
    return p;
}

void
destroy_page(Page *p)
{
	unsigned int i;
	size_t naqueue = 0;

	for(i = 0; (i < aqsz) && aq[i]; i++)
		if(aq[i] == p)
			naqueue++;
	for(i = 0; i < naqueue; i++)
		cext_array_detach((void **)aq, p, &aqsz);

	for(i = 0; i < nclient; i++)
		if(client[i]->page == p)
			detach_client(client[i], False);

	for(i = 0; (i < npage) && (p != page[i]); i++);
	if(sel && (sel == i))
		sel--;
    free(p); 
	npage--;

	for(i = 0; i < npage; i++);
    XChangeProperty(dpy, root, net_atoms[NET_NUMBER_OF_DESKTOPS], XA_CARDINAL,
			        32, PropModeReplace, (unsigned char *) &i, 1);

    /* determine what to focus and do that */
    if(page[sel])
        focus_page(page[sel]);
    else {
		do_pend_fcall("NoPage\n");
        XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
    }
}

void
focus_page(Page *p)
{
	size_t i;
	Page *old = page ? page[sel] : nil;

	if(!page)
		return;

	for(i = 0; (i < npage) && (page[i] != p); i++);
	if(i == sel)
		return;

	sel = i;
	for(i = 0; i < nclient; i++) {
		Client *c = client[i];
		if(old && (c->page == old))
			XMoveWindow(dpy, c->frame.win, 2 * rect.width, 2 * rect.height);
		else if(c->page == p)
			XMoveWindow(dpy, c->frame.win, c->frame.rect.x, c->frame.rect.y);
	}
	do_pend_fcall("SelPage\n");
    XChangeProperty(dpy, root, net_atoms[NET_CURRENT_DESKTOP], XA_CARDINAL,
			        32, PropModeReplace, (unsigned char *) &sel, 1);
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
    if(wins) {
        XFree(wins);
    }
    *num = j;
    return result;
}

static void
xexec(void *obj, char *arg)
{
	aq = (Page **)cext_array_attach((void **)aq, obj,
				sizeof(Page *), &aqsz);
    wmii_spawn(dpy, arg);
}

static void
swap_client(void *obj, char *arg)
{
	Page *p = obj;
	Client *c = sel_client_of_page(p);
    Area *west = nil, *east = nil, *col = c->area;
    Client *north = nil, *south = nil;
	size_t i;

	if(!col || !arg)
		return;

	for(i = 1; i < p->narea && (p->area[i] != col); i++);
    west = i ? p->area[i - 1] : nil;
    east = (i < p->areasz) && p->area[i + 1] ? p->area[i + 1] : nil;

	for(i = 0; (i < col->nclient) && (col->client[i] != c); i++);
    north = i ? col->client[i - 1] : nil;
    south = (i + 1 < col->nclient) ? col->client[i + 1] : nil;

	if(!strncmp(arg, "north", 6) && north) {
		col->client[i] = col->client[i - 1]; 
		col->client[i - 1] = c;
		arrange_column(p, col);
	} else if(!strncmp(arg, "south", 6) && south) {
		col->client[i] = col->client[i + 1];
		col->client[i + 1] = c;
		arrange_column(p, col);
	}
	else if(!strncmp(arg, "west", 5) && west) {
		col->client[i] = west->client[west->sel];
		west->client[west->sel] = c;
		west->client[west->sel]->area = west;
		col->client[i]->area = col;
		arrange_column(p, col);
		arrange_column(p, west);
	} else if(!strncmp(arg, "east", 5) && east) {
		col->client[i] = west->client[west->sel];
		col->client[i]->area = col;
		east->client[east->sel] = c;
		east->client[east->sel]->area = east;
		arrange_column(p, col);
		arrange_column(p, east);
	}
	focus_client(c);
}

static void
xnew_column(void *obj, char *arg)
{
	new_column(obj);
}

static void
select_client(void *obj, char *arg)
{
	Page *p = obj;
	Client *c = sel_client_of_page(p);
	size_t i;

	if(!c || !arg)
		return;

	if(index_of_area(c->page, c->area) > 0)
		select_column(c, arg);
	else {
		Area *a = c->area;
		for(i = 0; (i < a->nclient) && (a->client[i] != c); i++);
		if(!strncmp(arg, "prev", 5)) {
			if(!i)
				i = a->nclient - 1;
			focus_client(a->client[i]);
		} else if(!strncmp(arg, "next", 5)) {
			if(i + 1 < a->nclient)
				focus_client(a->client[i + 1]);
			else
				focus_client(a->client[0]);
		}
		else {
			const char *errstr;
			i = cext_strtonum(arg, 0, a->nclient - 1, &errstr);
			if(errstr)
				return;
			focus_client(a->client[i]);
		}
	}
}

int
index_of_area(Page *p, Area *a)
{
	int i;
	for(i = 0; i < p->narea; i++)
		if(p->area[i] == a)
			return i;
	return -1;
}
