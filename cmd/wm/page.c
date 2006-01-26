/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

#include "wm.h"

static void handle_after_write_page(IXPServer * s, File * file);

static void select_client(void *obj, char *arg);
static void toggle_layout(void *obj, char *arg);
static void xexec(void *obj, char *arg);
static void swap_client(void *obj, char *arg);
static void xnew_column(void *obj, char *arg);

/* action table for /?/ namespace */
Action page_acttbl[] = {
    {"toggle", toggle_layout},
    {"exec", xexec},
    {"swap", swap_client},
    {"newcol", xnew_column},
    {"select", select_client},
    {0, 0}
};

Page **
attach_page_to_array(Page *p, Page **array, size_t *size)
{
	size_t i;
	if(!array) {
		*size = 2;
		array = cext_emallocz(sizeof(Page *) * (*size));
	}
	for(i = 0; (i < (*size)) && array[i]; i++);
	if(i >= (*size)) {
		Page **tmp = array;
		(*size) *= 2;
		array = cext_emallocz(sizeof(Page *) * (*size));
		for(i = 0; tmp[i]; i++)
			array[i] = tmp[i];
		free(tmp);
	}
	array[i] = p;
	return array;
}

void
detach_page_from_array(Page *p, Page **array)
{
	size_t i;
	for(i = 0; array[i] != p; i++);
	for(; array[i + 1]; i++)
		array[i] = array[i + 1];
	array[i] = nil;
}

Page *
alloc_page()
{
    Page *p = cext_emallocz(sizeof(Page));
    char buf[MAX_BUF], buf2[16];
	static int id = 1;
	size_t np;

    snprintf(buf2, sizeof(buf2), "%d", id);
    snprintf(buf, sizeof(buf), "/%d", id);
    p->file[P_PREFIX] = ixp_create(ixps, buf);
    snprintf(buf, sizeof(buf), "/%d/name", id);
    p->file[P_NAME] = wmii_create_ixpfile(ixps, buf, buf2);
    snprintf(buf, sizeof(buf), "/%d/client", id);
    p->file[P_CLIENT_PREFIX] = ixp_create(ixps, buf);
    snprintf(buf, sizeof(buf), "/%d/client/sel", id);
    p->file[P_SEL_PREFIX] = ixp_create(ixps, buf);
    p->file[P_SEL_PREFIX]->bind = 1;    /* mount point */
    snprintf(buf, sizeof(buf), "/%d/ctl", id);
    p->file[P_CTL] = ixp_create(ixps, buf);
    p->file[P_CTL]->after_write = handle_after_write_page;
    def[WM_SEL_PAGE]->content = p->file[P_PREFIX]->content;
    invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
	id++;
	p->rect_column = rect;
	page = attach_page_to_array(p, page, &pagesz);
	for(np = 0; (np < pagesz) && page[np]; np++);
	focus_page(p);
    XChangeProperty(dpy, root, net_atoms[NET_NUMBER_OF_DESKTOPS], XA_CARDINAL,
			        32, PropModeReplace, (unsigned char *) &np, 1);
	p->is_column = True;
    return p;
}

void
destroy_page(Page *p)
{
	unsigned int i;
	size_t naqueue = 0;

	for(i = 0; (i < aqueuesz) && aqueue[i]; i++)
		if(aqueue[i] == p)
			naqueue++;
	for(i = 0; i < naqueue; i++)
		detach_page_from_array(p, aqueue);

	for(i = 0; (i < clientsz) && client[i]; i++)
		if(client[i]->page == p)
			detach_client(client[i], False);

	for(i = 0; (i < pagesz) && page[i] && (p != page[i]); i++);
	if(sel_page && (sel_page == i))
		sel_page--;

    def[WM_SEL_PAGE]->content = 0;
    ixp_remove_file(ixps, p->file[P_PREFIX]);
    free(p); 

	for(i = 0; (i < pagesz) && page[i]; i++);
    XChangeProperty(dpy, root, net_atoms[NET_NUMBER_OF_DESKTOPS], XA_CARDINAL,
			        32, PropModeReplace, (unsigned char *) &i, 1);

    /* determine what to focus and do that */
    if(page[sel_page])
        focus_page(page[sel_page]);
    else {
        invoke_wm_event(def[WM_EVENT_CLIENT_UPDATE]);
        invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
        XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
    }
}

void
focus_page(Page *p)
{
	size_t i;
	Page *old = page ? page[sel_page] : nil;

	if(!page)
		return;

	for(i = 0; (i < pagesz) && page[i] && (page[i] != p); i++);
	if(i == sel_page)
		return;

	sel_page = i;
	for(i = 0; (i < clientsz) && client[i]; i++) {
		Client *c = client[i];
		if(old && (c->page == old))
			XMoveWindow(dpy, c->frame.win, 2 * rect.width, 2 * rect.height);
		else if(c->page == p)
			XMoveWindow(dpy, c->frame.win, c->frame.rect.x, c->frame.rect.y);
	}
    def[WM_SEL_PAGE]->content = p->file[P_PREFIX]->content;
    invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
    XChangeProperty(dpy, root, net_atoms[NET_CURRENT_DESKTOP], XA_CARDINAL,
			        32, PropModeReplace, (unsigned char *) &sel_page, 1);
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
handle_after_write_page(IXPServer *s, File *file)
{
	size_t i;

	for(i = 0; (i < pagesz) && page[i]; i++) {
        if(file == page[i]->file[P_CTL]) {
            run_action(file, page[i], page_acttbl);
            return;
        }
    }
}

static void
xexec(void *obj, char *arg)
{
	aqueue = attach_page_to_array(obj, aqueue, &aqueuesz);
    wmii_spawn(dpy, arg);
}

static void
toggle_layout(void *obj, char *arg)
{
    Page *p = obj;

	p->is_column = !p->is_column;
	if(p->is_column) {
		Column *col = p->column[p->sel_column];
		if(col && col->clientsz && col->client[col->sel])
			focus_client(col->client[col->sel]);
	}
	else if(p->floating && p->floatingsz && p->floating[p->sel_float])
		focus_client(p->floating[p->sel_float]);
}

static void
swap_client(void *obj, char *arg)
{
	Page *p = obj;
	Client *c = sel_client_of_page(p);
    Column *west = nil, *east = nil, *col = c->column;
    Client *north = nil, *south = nil;
	size_t i;

	if(!col || !arg)
		return;

	for(i = 0; (i < p->columnsz) && p->column[i] && (p->column[i] != col); i++);
    west = i ? p->column[i - 1] : nil;
    east = (i < p->columnsz) && p->column[i + 1] ? p->column[i + 1] : nil;

	for(i = 0; (i < col->clientsz) && col->client[i] && (col->client[i] != c); i++);
    north = i ? col->client[i - 1] : nil;
    south = (i < col->clientsz) && col->client[i + 1] ? col->client[i + 1] : nil;

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
		col->client[i]->column = col;
		west->client[west->sel] = c;
		west->client[west->sel]->column = west;
		arrange_column(p, col);
		arrange_column(p, west);
	} else if(!strncmp(arg, "east", 5) && east) {
		col->client[i] = west->client[west->sel];
		col->client[i]->column = col;
		east->client[east->sel] = c;
		east->client[east->sel]->column = east;
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

	if(c->column)
		select_column(c, arg);
	else {
		for(i = 0; (i < p->floatingsz) && p->floating[i] && (p->floating[i] != c); i++);
		if(!strncmp(arg, "prev", 5)) {
			if(!i)
				for(i = 0; (i < p->floatingsz) && p->floating[i]; i++);
			focus_client(p->floating[i - 1]);
		} else if(!strncmp(arg, "next", 5)) {
			if(p->floating[i + 1])
				focus_client(p->floating[i + 1]);
			else
				focus_client(p->floating[0]);
		}
		else {
			const char *errstr;
			for(i = 0; (i < p->floatingsz) && p->floating[i]; i++);
			i = cext_strtonum(arg, 0, i - 1, &errstr);
			if(errstr)
				return;
			focus_client(p->floating[i]);
		}
	}
}

