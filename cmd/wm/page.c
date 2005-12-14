/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

static void handle_after_write_page(IXPServer * s, File * f);

static void select_area(void *obj, char *arg);

/* action table for /?/ namespace */
Action page_acttbl[] = {
	{"select", select_area},
	{0, 0}
};

Page *alloc_page()
{
	Page *p = cext_emallocz(sizeof(Page));
	char buf[MAX_BUF], buf2[16];
	size_t id = cext_sizeof_container(pages);

	snprintf(buf2, sizeof(buf2), "%d", id);
	p->areas.list = p->areas.stack = 0;
	snprintf(buf, sizeof(buf), "/%d", id);
	p->file[P_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, sizeof(buf), "/%d/name", id);
	p->file[P_NAME] = wmii_create_ixpfile(ixps, buf, buf2);
	snprintf(buf, sizeof(buf), "/%d/layout/", id);
	p->file[P_AREA_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, sizeof(buf), "/%d/layout/sel", id);
	p->file[P_SEL_AREA] = ixp_create(ixps, buf);
	p->file[P_SEL_AREA]->bind = 1;	/* mount point */
	snprintf(buf, sizeof(buf), "/%d/ctl", id);
	p->file[P_CTL] = ixp_create(ixps, buf);
	p->file[P_CTL]->after_write = handle_after_write_page;
	alloc_area(p, "float");
	alloc_area(p, def[WM_LAYOUT]->content);
	cext_attach_item(pages, p);
	def[WM_SEL_PAGE]->content = p->file[P_PREFIX]->content;
	invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
	return p;
}

static void iter_destroy_area(void *area, void *aux)
{
	destroy_area((Area *)area);
}

void destroy_page(Page * p)
{
	cext_list_iterate(&p->areas, nil, iter_destroy_area);
	def[WM_SEL_PAGE]->content = 0;
	ixp_remove_file(ixps, p->file[P_PREFIX]);
	cext_detach_item(pages, p);
	free(p);
	if ((p = get_sel_page()))
		sel_page(p);
	else
		invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
}

void sel_page(Page * p)
{
	Page *sel = get_sel_page();
	if (!sel)
		return;
	if (p != sel) {
		hide_page(sel);
		cext_stack_top_item(pages, p);
		show_page(p);
	}
	def[WM_SEL_PAGE]->content = p->file[P_PREFIX]->content;
	invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
	sel_area(get_sel_area());
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

static void iter_hide_page(void *item, void *aux)
{
	hide_area((Area *)item);
}

void hide_page(Page * p)
{
	cext_list_iterate(&p->areas, nil, iter_hide_page);
}

static void iter_show_page(void *item, void *aux)
{
	show_area((Area *)item);
}

void show_page(Page * p)
{
	cext_list_iterate(&p->areas, nil, iter_show_page);
}

static void iter_after_write_page(void *item, void *aux)
{
	Page *p = (Page *)item;
	File *file = aux;
	if (file == p->file[P_CTL]) {
		run_action(file, p, page_acttbl);
		return;
	}
}

static void handle_after_write_page(IXPServer *s, File *f)
{
	cext_list_iterate(pages, f, iter_after_write_page);
}

Page *get_sel_page()
{
	return cext_stack_get_top_item(pages);
}

static void select_area(void *obj, char *arg)
{
	Page *p = obj;
	Area *a = cext_stack_get_top_item(&p->areas);

	if (!strncmp(arg, "prev", 5))
		a = cext_list_get_prev_item(&p->areas, a);
	else if (!strncmp(arg, "next", 5))
		a = cext_list_get_next_item(&p->areas, a);
	else 
		a = cext_list_get_item(&p->areas, blitz_strtonum(arg, 0, cext_sizeof_container(&p->areas) - 1));
	sel_area(a);
	invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
}
