/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

static void handle_after_write_page(IXPServer *s, File *file);

static void toggle_area(void *obj, char *arg);

/* action table for /?/ namespace */
Action page_acttbl[] = {
	{"toggle", toggle_area},
	{0, 0}
};

Page *alloc_page()
{
	Page *p, *new = cext_emallocz(sizeof(Page));
	char buf[MAX_BUF], buf2[16];

	snprintf(buf2, sizeof(buf2), "%d", npages);
	snprintf(buf, sizeof(buf), "/%d", npages);
	new->file[P_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, sizeof(buf), "/%d/name", npages);
	new->file[P_NAME] = wmii_create_ixpfile(ixps, buf, buf2);
	snprintf(buf, sizeof(buf), "/%d/layout/", npages);
	new->file[P_AREA_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, sizeof(buf), "/%d/layout/sel", npages);
	new->file[P_SEL_AREA] = ixp_create(ixps, buf);
	new->file[P_SEL_AREA]->bind = 1;	/* mount point */
	snprintf(buf, sizeof(buf), "/%d/ctl", npages);
	new->file[P_CTL] = ixp_create(ixps, buf);
	new->file[P_CTL]->after_write = handle_after_write_page;
	new->floating = alloc_area(new, "float");
	new->sel = new->managed = alloc_area(new, def[WM_LAYOUT]->content);
	fprintf(stderr, "%s", "after allocating areas\n");
	for (p = pages; p && p->next; p = p->next);
	if (!p)
		pages = new;
	else {
		new->prev = p;
		p->next = new;
	}
	selpage = new;
	fprintf(stderr, "%s", "after attaching page\n");
	def[WM_SEL_PAGE]->content = new->file[P_PREFIX]->content;
	invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
	return new;
}

void destroy_page(Page *p)
{
	destroy_area(p->floating);
	destroy_area(p->managed);
	def[WM_SEL_PAGE]->content = 0;
	ixp_remove_file(ixps, p->file[P_PREFIX]);
	if (p == selpage) {
		if (p->prev)
			selpage = p->prev;
		else
			selpage = nil;
	}
		
	if (p == pages) {
		if (p->next)
			p->next->prev = nil;
		pages = p->next;
	}
	else {
		p->prev->next = p->next;
		if (p->next)
			p->next->prev = p->prev;
	}

	free(p);
	if (!selpage)
		selpage = pages;

	if (selpage)
		focus_page(selpage);
}

void focus_page(Page *p)
{
	if (selpage != p)
		hide_page(selpage);
	selpage = p;
	show_page(p);
	def[WM_SEL_PAGE]->content = p->file[P_PREFIX]->content;
	invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
	focus_area(sel_area());
}

XRectangle *rectangles(unsigned int *num)
{
	XRectangle *result = 0;
	int i, j = 0;
	Window d1, d2;
	Window *wins;
	XWindowAttributes wa;
	XRectangle r;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, num)) {
		result = cext_emallocz(*num * sizeof(XRectangle));
		for (i = 0; i < *num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (wa.override_redirect && (wa.map_state == IsViewable)) {
				r.x = wa.x;
				r.y = wa.y;
				r.width = wa.width;
				r.height = wa.height;
				result[j++] = r;
			}
		}
	}
	if (wins) {
		XFree(wins);
	}
	*num = j;
	return result;
}

void hide_page(Page *p)
{
	hide_area(p->managed);
	hide_area(p->floating);
}

void show_page(Page *p)
{
	show_area(p->managed, False);
	show_area(p->floating, False);
}

static void handle_after_write_page(IXPServer *s, File *file)
{
	Page *p;
	for (p = pages; p; p = p->next) {
		if (file == p->file[P_CTL]) {
			run_action(file, p, page_acttbl);
			return;
		}
	}
}

static void toggle_area(void *obj, char *arg)
{
	Page *p = obj;

	if (p->sel == p->managed)
		p->sel = p->floating;
	else
		p->sel = p->managed;

	focus_area(p->sel);	
	invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
}

Page *pageat(unsigned int idx)
{
	unsigned int i = 0;
	Page *p = pages;
	for (; p && i != idx; p = p->next);
	return p;
}
