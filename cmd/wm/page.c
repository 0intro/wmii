/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

static void select_frame(void *obj, char *cmd);
static void handle_after_write_page(IXPServer * s, File * f);

/* action table for /?/ namespace */
Action page_acttbl[] = {
	{"select", select_frame},
	{0, 0}
};

Page *alloc_page()
{
	Page *p = cext_emalloc(sizeof(Page));
	char buf[MAX_BUF], buf2[16];
	size_t id = cext_sizeof(&pages);

	snprintf(buf2, sizeof(buf2), "%d", id);
	p->areas.list = p->areas.stack = 0;
	snprintf(buf, sizeof(buf), "/%d", id);
	p->file[P_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, sizeof(buf), "/%d/name", id);
	p->file[P_NAME] = wmii_create_ixpfile(ixps, buf, buf2);
	snprintf(buf, sizeof(buf), "/%d/a", id);
	p->file[P_AREA_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, sizeof(buf), "/%d/a/sel", id);
	p->file[P_SEL_AREA] = ixp_create(ixps, buf);
	p->file[P_SEL_AREA]->bind = 1;	/* mount point */
	snprintf(buf, sizeof(buf), "/%d/ctl", id);
	p->file[P_CTL] = ixp_create(ixps, buf);
	p->file[P_CTL]->after_write = handle_after_write_page;
	alloc_area(p, &rect, "float");
	cext_attach_item(&pages, p);
	def[WM_SEL_PAGE]->content = p->file[P_PREFIX]->content;
	return p;
}

static void iter_destroy_page(void *item, void *aux)
{
	destroy_area((Area *)item);
}

void destroy_page(Page * p)
{
	cext_iterate(&p->areas, nil, iter_destroy_page);
	def[WM_SEL_PAGE]->content = 0;
	ixp_remove_file(ixps, p->file[P_PREFIX]);
	if (ixps->errstr)
		fprintf(stderr, "wmiiwm: free_page(): %s\n", ixps->errstr);
	free(p);
	if ((p = get_sel_page())) {
		show_page(p);
		sel_page(p);
	}
}

void sel_page(Page * p)
{
	Page *sel = get_sel_page();
	if (!sel)
		return;
	if (p != sel) {
		hide_page(sel);
		cext_top_item(&pages, p);
		show_page(p);
	}
	def[WM_SEL_PAGE]->content = p->file[P_PREFIX]->content;
	invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
	sel_area(get_sel_area());
}

static void iter_draw_page(void *item, void *aux)
{
	draw_area((Area *)item);
}

void draw_page(Page * p)
{
	cext_iterate(&p->areas, nil, iter_draw_page);
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
		result = cext_emalloc(*num * sizeof(XRectangle));
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

static void center_pointer(Frame * f)
{

	Window dummy;
	int wex, wey, ex, ey, i;
	unsigned int dmask;
	if (!f)
		return;
	XQueryPointer(dpy, f->win, &dummy, &dummy, &i, &i, &wex, &wey, &dmask);
	XTranslateCoordinates(dpy, f->win, root, wex, wey, &ex, &ey, &dummy);
	if (blitz_ispointinrect(ex, ey, &f->rect))
		return;
	/* suppress EnterNotify's while mouse warping */
	XSelectInput(dpy, root, ROOT_MASK & ~StructureNotifyMask);
	XWarpPointer(dpy, None, f->win, 0, 0, 0, 0, f->rect.width / 2,
				 f->rect.height / 2);
	XSync(dpy, False);
	XSelectInput(dpy, root, ROOT_MASK);

}

static void select_frame(void *obj, char *cmd)
{
	Area *a;
	Frame *f, *old;
	f = old = get_sel_frame();
	if (!f || !cmd)
		return;
	a = f->area;
	if (!strncmp(cmd, "prev", 5))
		cext_top_item(&a->frames, cext_get_up_item(&a->frames, f));
	else if (!strncmp(cmd, "next", 5))
		cext_top_item(&a->frames, cext_get_down_item(&a->frames, f));
	if (old != f) {
		sel_frame(f, cext_get_item_index(&a->page->areas, a) == 0);
		center_pointer(f);
		draw_frame(old, nil);
		draw_frame(f, nil);
	}
}

static void iter_hide_page(void *item, void *aux)
{
	hide_area((Area *)item);
}

void hide_page(Page * p)
{
	cext_iterate(&p->areas, nil, iter_hide_page);
}

static void iter_show_page(void *item, void *aux)
{
	show_area((Area *)item);
}

void show_page(Page * p)
{
	cext_iterate(&p->areas, nil, iter_show_page);
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
	cext_iterate(&pages, f, iter_after_write_page);
}

Page *get_sel_page()
{
	return cext_get_top_item(&pages);
}
