/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

#include <cext.h>

static Page zero_page = { 0 };

static void select_frame(void *obj, char *cmd);
static void handle_after_write_page(IXPServer * s, File * f);

/* action table for /page/?/ namespace */
Action page_acttbl[] = {
	{"select", select_frame},
	{0, 0}
};

Page *alloc_page(char *autodestroy)
{
	Page *p = emalloc(sizeof(Page));
	char buf[MAX_BUF];
	int id = count_items((void **) pages) + 1;

	*p = zero_page;
	snprintf(buf, sizeof(buf), "/page/%d", id);
	p->files[P_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, sizeof(buf), "/page/%d/area", id);
	p->files[P_AREA_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, sizeof(buf), "/page/%d/area/sel", id);
	p->files[P_SEL_AREA] = ixp_create(ixps, buf);
	p->files[P_SEL_AREA]->bind = 1;	/* mount point */
	snprintf(buf, sizeof(buf), "/page/%d/ctl", id);
	p->files[P_CTL] = ixp_create(ixps, buf);
	p->files[P_CTL]->after_write = handle_after_write_page;
	snprintf(buf, sizeof(buf), "/page/%d/auto-destroy", id);
	p->files[P_AUTO_DESTROY] = wmii_create_ixpfile(ixps, buf, autodestroy);
	pages = (Page **) attach_item_end((void **) pages, p, sizeof(Page *));
	sel = index_item((void **) pages, p);
	defaults[WM_SEL_PAGE]->content = p->files[P_PREFIX]->content;
	invoke_wm_event(defaults[WM_EVENT_PAGE_UPDATE]);
	return p;
}

void destroy_page(Page * p)
{
	unsigned int i;
	for (i = 0; p->areas[i]; i++)
		destroy_area(p->areas[i]);
	free_page(p);
	if (pages) {
		show_page(pages[sel]);
		defaults[WM_SEL_PAGE]->content =
			pages[sel]->files[P_PREFIX]->content;
		focus_page(pages[sel], 0, 1);
		invoke_wm_event(defaults[WM_EVENT_PAGE_UPDATE]);
	}
}

void free_page(Page * p)
{
	pages = (Page **) detach_item((void **) pages, p, sizeof(Page *));
	if (pages) {
		if (sel - 1 >= 0)
			sel--;
		else
			sel = 0;
	}
	defaults[WM_SEL_PAGE]->content = 0;
	ixp_remove_file(ixps, p->files[P_PREFIX]);
	if (ixps->errstr)
		fprintf(stderr, "wmiiwm: free_page(): %s\n", ixps->errstr);
	free(p);
}

void focus_page(Page * p, int raise, int down)
{
	if (!pages)
		return;
	if (p != pages[sel]) {
		hide_page(pages[sel]);
		sel = index_item((void **) pages, p);
		show_page(pages[sel]);
		defaults[WM_SEL_PAGE]->content = p->files[P_PREFIX]->content;
		invoke_wm_event(defaults[WM_EVENT_PAGE_UPDATE]);
	}
	if (down)
		focus_area(p->areas[p->sel], raise, 0, down);
}

void draw_page(Page * p)
{
	int i;
	if (!p)
		return;
	for (i = 0; p->areas && p->areas[i]; i++)
		draw_area(p->areas[i]);
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
		result = emalloc(*num * sizeof(XRectangle));
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
	f = old = pages ? SELFRAME(pages[sel]) : 0;
	if (!f || !cmd)
		return;
	if (!strncmp(cmd, "prev", 5)) {
		i = index_prev_item((void **) f->area->frames, f);
		f = f->area->frames[i];
	} else if (!strncmp(cmd, "next", 5)) {
		i = index_next_item((void **) f->area->frames, f);
		f = f->area->frames[i];
	}
	if (old != f) {
		focus_frame(f, 1, 1, 1);
		center_pointer(f);
		draw_frame(old);
		draw_frame(f);
	}
}

void hide_page(Page * p)
{

	int i;
	for (i = 0; p->areas && p->areas[i]; i++)
		hide_area(p->areas[i]);
}

void show_page(Page * p)
{
	int i;
	for (i = 0; p->areas && p->areas[i]; i++)
		show_area(p->areas[i]);
}

Layout *get_layout(char *name)
{
	int i = 0;
	size_t len;
	if (!name)
		return 0;
	len = strlen(name);
	for (i = 0; layouts[i]; i++) {
		if (!strncmp(name, layouts[i]->name, len))
			return layouts[i];
	}
	return 0;
}

static void handle_after_write_page(IXPServer * s, File * f)
{
	int i;

	for (i = 0; pages && pages[i]; i++) {
		Page *p = pages[i];
		if (p->files[P_CTL] == f) {
			run_action(f, p, page_acttbl);
			return;
		}
		/*
		   else if (p->files[P_MANAGED_SIZE] == f) {
		   / resize stuff /
		   blitz_strtorect(dpy, &rect, &p->managed_rect,
		   p->files[P_MANAGED_SIZE]->content);
		   if (!p->managed_rect.width)
		   p->managed_rect.width = 10;
		   if (!p->managed_rect.height)
		   p->managed_rect.height = 10;
		   if (p->layout)
		   p->layout->arrange(p);
		   draw_page(p);
		   return;
		   } else if (p->files[P_MANAGED_LAYOUT] == f) {
		   int             had_valid_layout = p->layout ? 1 : 0;
		   if (p->layout)
		   p->layout->deinit(p);
		   p->layout = get_layout(p->files[P_MANAGED_LAYOUT]->content);
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
		   / make all managed clients floating /
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
		   invoke_wm_event(wm_files[CORE_EVENT_PAGE_UPDATE]);
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
		wmii_move_ixpfile(f->files[F_PREFIX], p->files[P_MANAGED_PREFIX]);
		p->files[P_MANAGED_SELECTED]->content =
			f->files[F_PREFIX]->content;
		if (p == pages[sel_page])
			for (i = 0; p->floating && p->floating[i]; i++)
				XRaiseWindow(dpy, p->floating[i]->win);
	} else {
		p->floating = (Frame **) attach_item_end((void **) p->floating, f,
							 sizeof(Frame *));
		p->floating_stack =
			(Frame **) attach_item_begin((void **) p->floating_stack, f,
						     sizeof(Frame *));
		wmii_move_ixpfile(f->files[F_PREFIX], p->files[P_FLOATING_PREFIX]);
		p->files[P_FLOATING_SELECTED]->content =
			f->files[F_PREFIX]->content;
		p->files[P_MODE]->content = p->files[P_FLOATING_PREFIX]->content;
	}
	f->page = p;
	focus_frame(f, 1, 0, 1);
	if (is_managed_frame(f) && p->layout)
		p->layout->manage(f);
	center_pointer(f);
	if (old)
		draw_frame(old);
	draw_frame(f);
}

void 
detach_frame_from_page(Frame * f, int ignore_focus_and_destroy)
{
	Page           *p = f->page;
	wmii_move_ixpfile(f->files[F_PREFIX], wm_files[CORE_DETACHED_FRAME]);
	if (is_managed_frame(f)) {
		p->managed = (Frame **) detach_item((void **) p->managed, f,
						    sizeof(Frame *));
		p->managed_stack =
			(Frame **) detach_item((void **) p->managed_stack, f,
					       sizeof(Frame *));
		p->files[P_MANAGED_SELECTED]->content = 0;
	} else {
		p->floating = (Frame **) detach_item((void **) p->floating, f,
						     sizeof(Frame *));
		p->floating_stack =
			(Frame **) detach_item((void **) p->floating_stack, f,
					       sizeof(Frame *));
		p->files[P_FLOATING_SELECTED]->content = 0;
	}
	XUnmapWindow(dpy, f->win);
	if (is_managed_mode(p) && p->layout)
		p->layout->unmanage(f);
	f->page = 0;
	if (!ignore_focus_and_destroy) {
		Frame          *fr;
		if (!p->managed && !p->floating
		    && _strtonum(p->files[P_AUTO_DESTROY]->content, 0, 1)) {
			destroy_page(p);
			return;
		}
		focus_page(p, 0, 1);
		fr = get_selected(p);
		if (fr) {
			center_pointer(fr);
			draw_frame(fr);
		}
	}
}
*/
