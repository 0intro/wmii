/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

static void select_client(void *obj, char *cmd);
static void handle_after_write_frame(IXPServer * s, File * f);
static void handle_before_read_frame(IXPServer * s, File * f);

/* action table for /frame/?/ namespace */
Action frame_acttbl[] = {
	{"select", select_client},
	{0, 0}
};

Frame *alloc_frame(XRectangle * r)
{
	XSetWindowAttributes wa;
	static int id = 0;
	char buf[MAX_BUF];
	Frame *f = (Frame *) cext_emallocz(sizeof(Frame));
	int bw, th;

	f->rect = *r;
	f->cursor = normal_cursor;

	snprintf(buf, MAX_BUF, "/detached/frame/%d", id);
	f->file[F_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/client", id);
	f->file[F_CLIENT_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/client/sel", id);
	f->file[F_SEL_CLIENT] = ixp_create(ixps, buf);
	f->file[F_SEL_CLIENT]->bind = 1;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/ctl", id);
	f->file[F_CTL] = ixp_create(ixps, buf);
	f->file[F_CTL]->after_write = handle_after_write_frame;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/geometry", id);
	f->file[F_GEOMETRY] = ixp_create(ixps, buf);
	f->file[F_GEOMETRY]->before_read = handle_before_read_frame;
	f->file[F_GEOMETRY]->after_write = handle_after_write_frame;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/border", id);
	f->file[F_BORDER] = wmii_create_ixpfile(ixps, buf, def[WM_BORDER]->content);
	f->file[F_BORDER]->after_write = handle_after_write_frame;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/tab", id);
	f->file[F_TAB] = wmii_create_ixpfile(ixps, buf, def[WM_TAB]->content);
	f->file[F_TAB]->after_write = handle_after_write_frame;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/handleinc", id);
	f->file[F_HANDLE_INC] = wmii_create_ixpfile(ixps, buf, def[WM_HANDLE_INC]->content);
	f->file[F_HANDLE_INC]->after_write = handle_after_write_frame;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/locked", id);
	f->file[F_LOCKED] = wmii_create_ixpfile(ixps, buf, def[WM_LOCKED]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/sstyle/bgcolor", id);
	f->file[F_SEL_BG_COLOR] = wmii_create_ixpfile(ixps, buf, def[WM_SEL_BG_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/sstyle/fgcolor", id);
	f->file[F_SEL_FG_COLOR] = wmii_create_ixpfile(ixps, buf, def[WM_SEL_FG_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/sstyle/bordercolor", id);
	f->file[F_SEL_BORDER_COLOR] = wmii_create_ixpfile(ixps, buf, def[WM_SEL_BORDER_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/nstyle/bgcolor", id);
	f->file[F_NORM_BG_COLOR] = wmii_create_ixpfile(ixps, buf, def[WM_NORM_BG_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/nstyle/fgcolor", id);
	f->file[F_NORM_FG_COLOR] = wmii_create_ixpfile(ixps, buf, def[WM_NORM_FG_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/nstyle/bordercolor", id);
	f->file[F_NORM_BORDER_COLOR] = wmii_create_ixpfile(ixps, buf, def[WM_NORM_BORDER_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/event/b2press", id);
	f->file[F_EVENT_B2PRESS] = wmii_create_ixpfile(ixps, buf, def[WM_EVENT_B2PRESS]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/event/b3press", id);
	f->file[F_EVENT_B3PRESS] = wmii_create_ixpfile(ixps, buf, def[WM_EVENT_B3PRESS]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/event/b4press", id);
	f->file[F_EVENT_B4PRESS] = wmii_create_ixpfile(ixps, buf, def[WM_EVENT_B4PRESS]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/event/b5press", id);
	f->file[F_EVENT_B5PRESS] = wmii_create_ixpfile(ixps, buf, def[WM_EVENT_B5PRESS]->content);
	id++;

	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask
		| PointerMotionMask | SubstructureRedirectMask
		| SubstructureNotifyMask;

    bw 	= border_width(f);
	th = tab_height(f);
	f->rect.width += 2 * bw;
	f->rect.height += bw + (th ? th : bw);
	f->win = XCreateWindow(dpy, root, f->rect.x, f->rect.y, f->rect.width,
							f->rect.height, 0, DefaultDepth(dpy, screen_num),
							CopyFromParent, DefaultVisual(dpy, screen_num),
							CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
	XDefineCursor(dpy, f->win, f->cursor);
	f->gc = XCreateGC(dpy, f->win, 0, 0);
	XSync(dpy, False);
	cext_attach_item(&frames, f);
	return f;
}

void sel_frame(Frame * f, Bool raise)
{
	Area *a = f->area;
	sel_client(cext_stack_get_top_item(&f->clients));
	cext_stack_top_item(a->layout->get_frames(a), f);
	a->file[A_SEL_FRAME]->content = f->file[F_PREFIX]->content;
	if (raise)
		XRaiseWindow(dpy, f->win);
}

static int comp_frame_win(void *pattern, void *frame)
{
	Window w = *(Window *)pattern;
	Frame *f = frame;

	return w == f->win;
}

Frame *win_to_frame(Window w)
{
	return cext_find_item(&frames, &w, comp_frame_win);
}

void destroy_frame(Frame * f)
{
	cext_detach_item(&frames, f);
	XFreeGC(dpy, f->gc);
	XDestroyWindow(dpy, f->win);
	ixp_remove_file(ixps, f->file[F_PREFIX]);
	free(f);
}

unsigned int tab_height(Frame * f)
{
	if (_strtonum(f->file[F_TAB]->content, 1, 1))
		return font->ascent + font->descent + 4;
	return 0;
}

unsigned int border_width(Frame * f)
{
	if (_strtonum(f->file[F_BORDER]->content, 0, 1))
		return BORDER_WIDTH;
	return 0;
}

typedef struct {
	unsigned int tabh;
	unsigned int bw;
} Twouint;

static void iter_resize_client(void *item, void *aux)
{
	Client *c = item;
	Twouint *v = aux;
	c->rect.x = v->bw;
	c->rect.y = v->tabh ? v->tabh : v->bw;
	c->rect.width = c->frame->rect.width - 2 * v->bw;
	c->rect.height = c->frame->rect.height - v->bw - (v->tabh ? v->tabh : v->bw);
	XMoveResizeWindow(dpy, c->win, c->rect.x, c->rect.y, c->rect.width, c->rect.height);
	configure_client(c);
}

static void resize_clients(Frame * f, int tabh, int bw)
{
	Twouint aux = { tabh, bw };
	cext_list_iterate(&f->clients, &aux, iter_resize_client);
}

static void check_dimensions(Frame * f, unsigned int tabh, unsigned int bw)
{
	Client *c = get_sel_client();
	if (!c)
		return;

	if (c->size.flags & PMinSize) {
		if (f->rect.width - 2 * bw < c->size.min_width) {
			f->rect.width = c->size.min_width + 2 * bw;
		}
		if (f->rect.height - bw - (tabh ? tabh : bw) < c->size.min_height) {
			f->rect.height = c->size.min_height + bw + (tabh ? tabh : bw);
		}
	}
	if (c->size.flags & PMaxSize) {
		if (f->rect.width - 2 * bw > c->size.max_width) {
			f->rect.width = c->size.max_width + 2 * bw;
		}
		if (f->rect.height - bw - (tabh ? tabh : bw) > c->size.max_height) {
			f->rect.height = c->size.max_height + bw + (tabh ? tabh : bw);
		}
	}
}

static void resize_incremental(Frame * f, unsigned int tabh, unsigned int bw)
{
	Client *c = get_sel_client();
	if (!c)
		return;
	/* increment stuff, see chapter 4.1.2.3 of the ICCCM Manual */
	if (c->size.flags & PResizeInc) {
		int base_width = 0, base_height = 0;

		if (c->size.flags & PBaseSize) {
			base_width = c->size.base_width;
			base_height = c->size.base_height;
		} else if (c->size.flags & PMinSize) {
			/* base_{width,height} default to min_{width,height} */
			base_width = c->size.min_width;
			base_height = c->size.min_height;
		}
		/* client_width = base_width + i * c->size.width_inc for an integer i */
		f->rect.width -=
			(f->rect.width - 2 * bw - base_width) % c->size.width_inc;
		f->rect.height -=
			(f->rect.height - bw - (tabh ? tabh : bw) -
			 base_height) % c->size.height_inc;
	}
}

void resize_frame(Frame * f, XRectangle * r, XPoint * pt)
{
	Area *a = f->area;
	unsigned int tabh = tab_height(f);
	unsigned int bw = border_width(f);

	a->layout->resize(f, r, pt);

	/* resize if client requests special size */
	check_dimensions(f, tabh, bw);

	if (f->file[F_HANDLE_INC]->content && ((char *) f->file[F_HANDLE_INC]->content)[0] == '1')
		resize_incremental(f, tabh, bw);

	XMoveResizeWindow(dpy, f->win, f->rect.x, f->rect.y, f->rect.width, f->rect.height);
	resize_clients(f, (tabh ? tabh : bw), bw);
}

/**
 * Assumes following file:
 *
 * ./sel-style/text-font	   "<value>"
 * ./sel-style/text-size	   "<int>"
 * ./sel-style/text-color	  "#RRGGBBAA"
 * ./sel-style/bg-color		"#RRGGBBAA"
 * ./sel-style/border-color	"#RRGGBBAA [#RRGGBBAA [#RRGGBBAA [#RRGGBBAA]]]"
 * ./norm-style/text-font	   "<value>"
 * ./norm-style/text-size	   "<int>"
 * ./norm-style/text-color	  "#RRGGBBAA"
 * ./norm-style/bg-color		"#RRGGBBAA"
 * ./norm-style/border-color	"#RRGGBBAA [#RRGGBBAA [#RRGGBBAA [#RRGGBBAA]]]"
 */
void draw_frame(void *frame, void *aux)
{
	Frame *f = frame;
	Draw d = { 0 };
	int bw = border_width(f);
	XRectangle notch;
	if (bw) {
		notch.x = bw;
		notch.y = bw;
		notch.width = f->rect.width - 2 * bw;
		notch.height = f->rect.height - 2 * bw;
		d.drawable = f->win;
		d.font = font;
		d.gc = f->gc;

		/* define ground plate (i = 0) */
		if (f == get_sel_frame()) {
			d.bg = blitz_loadcolor(dpy, screen_num, f->file[F_SEL_BG_COLOR]->content);
			d.fg = blitz_loadcolor(dpy, screen_num, f->file[F_SEL_FG_COLOR]->content);
			d.border = blitz_loadcolor(dpy, screen_num, f->file[F_SEL_BORDER_COLOR]->content);
		} else {
			d.bg = blitz_loadcolor(dpy, screen_num, f->file[F_NORM_BG_COLOR]->content);
			d.fg = blitz_loadcolor(dpy, screen_num, f->file[F_NORM_FG_COLOR]->content);
			d.border = blitz_loadcolor(dpy, screen_num, f->file[F_NORM_BORDER_COLOR]->content);
		}
		d.rect = f->rect;
		d.rect.x = d.rect.y = 0;
		d.notch = &notch;

		blitz_drawlabel(dpy, &d);
	}
	draw_clients(f);
	XSync(dpy, False);
}

void handle_frame_buttonpress(XButtonEvent *e, Frame *f)
{
	Align align;
	size_t size = cext_sizeof(&f->clients);
	int bindex, cindex = e->x / f->rect.width / size;
	Client *c = cext_list_get_item(&f->clients, cindex);
	cext_stack_top_item(&f->clients, c);
	sel_frame(f, cext_list_get_item_index(&f->area->page->areas, f->area) == 0);
	if (e->button == Button1) {
		align = cursor_to_align(f->cursor);
		if (align == CENTER)
			mouse_move(f);
		else
			mouse_resize(f, align);
		return;
	}
	bindex = F_EVENT_B2PRESS - 2 + e->button;
	/* frame mouse handling */
	if (f->file[bindex]->content)
		spawn(dpy, f->file[bindex]->content);
	draw_frame(f, nil);
}

void attach_client_to_frame(Frame *f, Client *c)
{
	wmii_move_ixpfile(c->file[C_PREFIX], f->file[F_CLIENT_PREFIX]);
	f->file[F_SEL_CLIENT]->content = c->file[C_PREFIX]->content;
	cext_attach_item(&f->clients, c);
	c->frame = f;
	reparent_client(c, f->win, border_width(f), tab_height(f));
	resize_frame(f, &f->rect, 0);
	show_client(c);
	sel_client(c);
}

void detach_client_from_frame(Frame *f, Client *c)
{
	Client *client;
	c->frame = nil;
	f->file[F_SEL_CLIENT]->content = nil;
	wmii_move_ixpfile(c->file[C_PREFIX], def[WM_DETACHED_CLIENT]);
	cext_detach_item(&f->clients, c);
	if (!c->destroyed) {
		hide_client(c);
		cext_attach_item(&detached, c);
		reparent_client(c, root, border_width(f), tab_height(f));
	}
	if ((client = cext_stack_get_top_item(&f->clients))) {
		sel_client(client);
		draw_frame(f, nil);
	}
}

static void select_client(void *obj, char *cmd)
{
	Frame *f = obj;
	size_t size = cext_sizeof(&f->clients);
	if (!f || !cmd || size == 1)
		return;
	if (!strncmp(cmd, "prev", 5))
		cext_stack_top_item(&f->clients, cext_stack_get_up_item(&f->clients, cext_stack_get_top_item(&f->clients)));
	else if (!strncmp(cmd, "next", 5))
		cext_stack_top_item(&f->clients, cext_stack_get_down_item(&f->clients, cext_stack_get_top_item(&f->clients)));
	sel_client(cext_stack_get_top_item(&f->clients));
	draw_frame(f, nil);
}

static void iter_before_read_frame(void *item, void *aux)
{
	Frame *f = item;
	File *file = aux;
	if (file == f->file[F_GEOMETRY]) {
		char buf[64];
		snprintf(buf, 64, "%d,%d,%d,%d", f->rect.x, f->rect.y, f->rect.width, f->rect.height);
		if (file->content)
			free(file->content);
		file->content = cext_estrdup(buf);
		file->size = strlen(buf);
	}
}

static void handle_before_read_frame(IXPServer *s, File *f)
{
	cext_list_iterate(&frames, f, iter_before_read_frame);
}

static void iter_after_write_frame(void *item, void *aux)
{
	Frame *f = item;
	File *file = aux;
	if (file == f->file[F_CTL]) {
		run_action(file, f, frame_acttbl);
		return;
	}
	if (file == f->file[F_TAB] || file == f->file[F_BORDER] || file == f->file[F_HANDLE_INC]) {
		f->area->layout->arrange(f->area);
		draw_page(f->area->page);
		return;
	} else if (file == f->file[F_GEOMETRY]) {
		char *size = f->file[F_GEOMETRY]->content;
		if (size && strrchr(size, ',')) {
			XRectangle frect;
			blitz_strtorect(&rect, &frect, size);
			resize_frame(f, &frect, 0);
			draw_page(f->area->page);
		}
		return;
	}
}

static void handle_after_write_frame(IXPServer * s, File * f)
{
	cext_list_iterate(&frames, f, iter_after_write_frame);
}

Frame *get_sel_frame_of_area(Area *a)
{
	return cext_stack_get_top_item(a->layout->get_frames(a));
}

Frame *get_sel_frame()
{
	Page *p = get_sel_page();
	if (!p)
		return nil;
	return get_sel_frame_of_area(get_sel_area(p));
}
