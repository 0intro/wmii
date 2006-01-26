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

static void toggle_layout(void *obj, char *arg);
static void xexec(void *obj, char *arg);

/* action table for /?/ namespace */
Action page_acttbl[] = {
    {"toggle", toggle_layout},
    {"exec", xexec},
    {0, 0}
};

void
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
    Page *p, *new = cext_emallocz(sizeof(Page));
    char buf[MAX_BUF], buf2[16];
	static int id = 1;
	size_t np;

    snprintf(buf2, sizeof(buf2), "%d", id);
    snprintf(buf, sizeof(buf), "/%d", id);
    new->file[P_PREFIX] = ixp_create(ixps, buf);
    snprintf(buf, sizeof(buf), "/%d/name", id);
    new->file[P_NAME] = wmii_create_ixpfile(ixps, buf, buf2);
    snprintf(buf, sizeof(buf), "/%d/floating/", id);
    new->file[P_FLOATING_PREFIX] = ixp_create(ixps, buf);
    snprintf(buf, sizeof(buf), "/%d/column/", id);
    new->file[P_COLUMN_PREFIX] = ixp_create(ixps, buf);
    snprintf(buf, sizeof(buf), "/%d/sel/", id);
    new->file[P_SEL_PREFIX] = ixp_create(ixps, buf);
    new->file[P_SEL_PREFIX]->bind = 1;    /* mount point */
    snprintf(buf, sizeof(buf), "/%d/floating/sel", id);
    new->file[P_SEL_FLOATING_CLIENT] = ixp_create(ixps, buf);
    new->file[P_SEL_FLOATING_CLIENT]->bind = 1; 
    snprintf(buf, sizeof(buf), "/%d/column/sel", id);
    new->file[P_SEL_COLUMN_CLIENT] = ixp_create(ixps, buf);
    new->file[P_SEL_COLUMN_CLIENT]->bind = 1; 
    snprintf(buf, sizeof(buf), "/%d/ctl", id);
    new->file[P_CTL] = ixp_create(ixps, buf);
    new->file[P_CTL]->after_write = handle_after_write_page;
    def[WM_SEL_PAGE]->content = new->file[P_PREFIX]->content;
    invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
	id++;
	p->rect_column = rect;
	attach_page_to_array(p, page, &pagesz);
	for(np = 0; (np < pagesz) && page[np]; np++);
	focus_page(new);
    XChangeProperty(dpy, root, net_atoms[NET_NUMBER_OF_DESKTOPS], XA_CARDINAL,
			        32, PropModeReplace, (unsigned char *) &np, 1);
    return new;
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
	unsigned int i, j;
	Page *old = page ? page[sel_page] : nil;

	if(!page)
		return;

	for(i = 0; (i < pagesz) && page[i]; i++);

	if(i == sel_page)
		return;

	sel_page = i;
	for(j = 0; (j < clientsz) && client[j]; j++) {
		if(client[j]->page == old)
			XMoveWindow(dpy, client[j]->frame.win, 2 * rect.width, 2 * rect.height);
		else if(client[j]->page == p)
			XMoveWindow(dpy, client[j]->frame.win,
						client[j]->frame.rect.x, client[j]->frame.rect.y);
	}
    def[WM_SEL_PAGE]->content = p->file[P_PREFIX]->content;
	if(p->is_column)
		p->file[P_SEL_PREFIX]->content = p->file[P_COLUMN_PREFIX]->content;
	else
		p->file[P_SEL_PREFIX]->content = p->file[P_FLOATING_PREFIX]->content;

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
	attach_page_to_array(obj, aqueue, &aqueuesz);
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
