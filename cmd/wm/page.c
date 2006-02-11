/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

#include "wm.h"

/*
static void select_client(void *obj, char *arg);
static void xexec(void *obj, char *arg);
static void swap_client(void *obj, char *arg);
static void xnew_column(void *obj, char *arg);
*/

/* action table for /?/ namespace */
/*
Action page_acttbl[] = {
    {"exec", xexec},
    {"swap", swap_client},
    {"newcol", xnew_column},
    {"select", select_client},
    {0, 0}
};
*/

Page *
alloc_page()
{
	static unsigned short id = 1;
    Page *p = cext_emallocz(sizeof(Page));

	p->id = id++;
	alloc_area(p);
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

	for(i = 0; i < p->narea; i++)
		destroy_area(p->area[i]);
	free(p->area);

	if((sel + 1 == npage) && (sel - 1 >= 0))
		sel--;

	cext_array_detach((void **)page, p, &pagesz);
	npage--;
    free(p); 

	for(i = 0; i < npage; i++);
    XChangeProperty(dpy, root, net_atoms[NET_NUMBER_OF_DESKTOPS], XA_CARDINAL,
			        32, PropModeReplace, (unsigned char *) &i, 1);

    /* determine what to focus and do that */
    if(npage)
        focus_page(page[sel]);
    else
		do_pend_fcall("PN -\n");
}

int
page_to_index(Page *p)
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
	int i = page_to_index(p);

	if(!npage || (i == -1))
		return;
	sel = i;
	for(i = 0; i < nclient; i++) {
		c = client[i];
		if(old && (c->area && c->area->page == old))
			XMoveWindow(dpy, c->frame.win, 2 * rect.width, 2 * rect.height);
		else if(c->area && c->area->page == p) {
			XMoveWindow(dpy, c->frame.win, c->frame.rect.x, c->frame.rect.y);
			draw_client(c);
		}
	}
	if((c = sel_client_of_page(p)))
		focus_client(c);
	snprintf(buf, sizeof(buf), "PN %d\n", sel + 1);
	do_pend_fcall(buf);
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

/*
static void
xexec(void *obj, char *arg)
{
	aq = (Page **)cext_array_attach((void **)aq, obj,
				sizeof(Page *), &aqsz);
    wmii_spawn(dpy, arg);
}
*/

/*
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

	if(area_to_index(c->page, c->area) > 0)
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
*/

int
pid_to_index(unsigned short id)
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

    if(!npage || !arg)
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
		int idx = cext_strtonum(arg, 0, npage, &err);
		if(idx && (idx - 1 < npage))
			new = idx;
	}
    focus_page(page[new]);
}
