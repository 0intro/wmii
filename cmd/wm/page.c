/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

static Page zero_page = { 0 };

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
	int id = count_items((void **) page) + 1;

	snprintf(buf2, sizeof(buf2), "%d", id);
	*p = zero_page;
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
	page = (Page **) attach_item_end((void **) page, p, sizeof(Page *));
	sel = index_item((void **) page, p);
	def[WM_SEL_PAGE]->content = p->file[P_PREFIX]->content;
	return p;
}

void destroy_page(Page * p)
{
	unsigned int i;
	for (i = 0; p->area[i]; i++)
		destroy_area(p->area[i]);
	free_page(p);
	if (page) {
		show_page(page[sel]);
		def[WM_SEL_PAGE]->content = page[sel]->file[P_PREFIX]->content;
		sel_page(page[sel]);
	}
}

void free_page(Page * p)
{
	page = (Page **) detach_item((void **) page, p, sizeof(Page *));
	if (page) {
		if (sel - 1 >= 0)
			sel--;
		else
			sel = 0;
	}
	def[WM_SEL_PAGE]->content = 0;
	ixp_remove_file(ixps, p->file[P_PREFIX]);
	if (ixps->errstr)
		fprintf(stderr, "wmiiwm: free_page(): %s\n", ixps->errstr);
	free(p);
}

void sel_page(Page * p)
{
	if (!page)
		return;
	if (p != page[sel]) {
		hide_page(page[sel]);
		sel = index_item((void **) page, p);
		show_page(page[sel]);
		def[WM_SEL_PAGE]->content = p->file[P_PREFIX]->content;
	}
	invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
	sel_area(p->area[p->sel], !p->sel);
}

void draw_page(Page * p)
{
	int i;
	if (!p)
		return;
	for (i = 0; p->area && p->area[i]; i++)
		draw_area(p->area[i]);
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
	int i;
	Frame *f, *old;
	f = old = page ? SELFRAME(page[sel]) : 0;
	if (!f || !cmd)
		return;
	if (!strncmp(cmd, "prev", 5)) {
		i = index_prev_item((void **) f->area->frame, f);
		f = f->area->frame[i];
	} else if (!strncmp(cmd, "next", 5)) {
		i = index_next_item((void **) f->area->frame, f);
		f = f->area->frame[i];
	}
	if (old != f) {
		sel_frame(f, 1);
		center_pointer(f);
		draw_frame(old);
		draw_frame(f);
	}
}

void hide_page(Page * p)
{

	int i;
	for (i = 0; p->area && p->area[i]; i++)
		hide_area(p->area[i]);
}

void show_page(Page * p)
{
	int i;
	for (i = 0; p->area && p->area[i]; i++)
		show_area(p->area[i]);
}

static void handle_after_write_page(IXPServer * s, File * f)
{
	int i;

	for (i = 0; page && page[i]; i++) {
		Page *p = page[i];
		if (p->file[P_CTL] == f) {
			run_action(f, p, page_acttbl);
			return;
		}
		/*
		   else if (p->file[P_MANAGED_SIZE] == f) {
		   / resize stuff /
		   blitz_strtorect(dpy, &rect, &p->managed_rect,
		   p->file[P_MANAGED_SIZE]->content);
		   if (!p->managed_rect.width)
		   p->managed_rect.width = 10;
		   if (!p->managed_rect.height)
		   p->managed_rect.height = 10;
		   if (p->layout)
		   p->layout->arrange(p);
		   draw_page(p);
		   return;
		   } else if (p->file[P_MANAGED_LAYOUT] == f) {
		   int             had_valid_layout = p->layout ? 1 : 0;
		   if (p->layout)
		   p->layout->deinit(p);
		   p->layout = get_layout(p->file[P_MANAGED_LAYOUT]->content);
		   if (p->layout) {
		   p->layout->init(p);
		   p->layout->arrange(p);
		   if (!had_valid_layout) {
		   int             j;
		   Frame         **tmp = 0;
		   for (j = 0; p->floating && p->floating[j]; j++) {
		   if (!p->floating[j]->floating)
		   tmp =
		   (Frame **) attach_item_begin((void **) tmp,
		   p->
		   floating[j],
		   sizeof(Frame
		   *));
		   }
		   for (j = 0; tmp && tmp[j]; j++)
		   toggle_frame(tmp[j]);
		   free(tmp);
		   }
		   }
		   if (!p->layout) {
		   / make all managed client floating /
		   int             j;
		   Frame         **tmp = 0;
		   while (p->managed) {
		   tmp = (Frame **) attach_item_begin((void **) tmp,
		   p->managed[0],
		   sizeof(Frame *));
		   detach_frame_from_page(p->managed[0], 1);
		   }
		   for (j = 0; tmp && tmp[j]; j++) {
		   attach_Frameo_page(p, tmp[j], 0);
		   resize_frame(tmp[j], rect_of_frame(tmp[j]), 0, 1);
		   }
		   free(tmp);
		   }
		   draw_page(p);
		   invoke_wm_event(wm_file[CORE_EVENT_PAGE_UPDATE]);
		   return;
		   }
		 */
	}
}

/*
void 
attach_frame_to_page(Page * p, Frame * f, int managed)
{
	Frame          *old = get_selected(p);
	XSelectInput(dpy, root, ROOT_MASK & ~StructureNotifyMask);
	XMapRaised(dpy, f->win);
	if (!f->floating && managed && p->layout) {
		int             i;
		p->managed = (Frame **) attach_item_end((void **) p->managed, f,
							sizeof(Frame *));
		p->managed_stack =
			(Frame **) attach_item_begin((void **) p->managed_stack, f,
						     sizeof(Frame *));
		wmii_move_ixpfile(f->file[F_PREFIX], p->file[P_MANAGED_PREFIX]);
		p->file[P_MANAGED_SELECTED]->content =
			f->file[F_PREFIX]->content;
		if (p == page[sel_page])
			for (i = 0; p->floating && p->floating[i]; i++)
				XRaiseWindow(dpy, p->floating[i]->win);
	} else {
		p->floating = (Frame **) attach_item_end((void **) p->floating, f,
							 sizeof(Frame *));
		p->floating_stack =
			(Frame **) attach_item_begin((void **) p->floating_stack, f,
						     sizeof(Frame *));
		wmii_move_ixpfile(f->file[F_PREFIX], p->file[P_FLOATING_PREFIX]);
		p->file[P_FLOATING_SELECTED]->content =
			f->file[F_PREFIX]->content;
		p->file[P_MODE]->content = p->file[P_FLOATING_PREFIX]->content;
	}
	f->page = p;
	sel_frame(f, 1, 0, 1);
	if (is_managed_frame(f) && p->layout)
		p->layout->manage(f);
	center_pointer(f);
	if (old)
		draw_frame(old);
	draw_frame(f);
}

void 
detach_frame_from_page(Frame * f, int ignore_sel_and_destroy)
{
	Page           *p = f->page;
	wmii_move_ixpfile(f->file[F_PREFIX], wm_file[CORE_DETACHED_FRAME]);
	if (is_managed_frame(f)) {
		p->managed = (Frame **) detach_item((void **) p->managed, f,
						    sizeof(Frame *));
		p->managed_stack =
			(Frame **) detach_item((void **) p->managed_stack, f,
					       sizeof(Frame *));
		p->file[P_MANAGED_SELECTED]->content = 0;
	} else {
		p->floating = (Frame **) detach_item((void **) p->floating, f,
						     sizeof(Frame *));
		p->floating_stack =
			(Frame **) detach_item((void **) p->floating_stack, f,
					       sizeof(Frame *));
		p->file[P_FLOATING_SELECTED]->content = 0;
	}
	XUnmapWindow(dpy, f->win);
	if (is_managed_mode(p) && p->layout)
		p->layout->unmanage(f);
	f->page = 0;
	if (!ignore_sel_and_destroy) {
		Frame          *fr;
		if (!p->managed && !p->floating
		    && _strtonum(p->file[P_AUTO_DESTROY]->content, 0, 1)) {
			destroy_page(p);
			return;
		}
		sel_page(p, 0, 1);
		fr = get_selected(p);
		if (fr) {
			center_pointer(fr);
			draw_frame(fr);
		}
	}
}
*/

Page *get_sel_page()
{
	return cext_get_top_item(&page);
}
