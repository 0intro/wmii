/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

#include <cext.h>

static Frame    zero_frame = {0};

static void     mouse();
static void     select_client(void *obj, char *cmd);
static void     handle_after_write_frame(IXPServer * s, File * f);
static void     handle_before_read_frame(IXPServer * s, File * f);

/* action table for /frame/?/ namespace */
Action          frame_acttbl[] = {
	{"select", select_client},
	{0, 0}
};

Frame          *
alloc_frame(XRectangle * r, int add_frame_border, int floating)
{
	XSetWindowAttributes wa;
	static int      id = 0;
	char            buf[MAX_BUF];
	Frame          *f = (Frame *) emalloc(sizeof(Frame));

	*f = zero_frame;
	f->rect = *r;
	f->cursor = normal_cursor;

	snprintf(buf, MAX_BUF, "/detached/frame/%d", id);
	f->files[F_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/client", id);
	f->files[F_CLIENT_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/client/sel", id);
	f->files[F_SEL_CLIENT] = ixp_create(ixps, buf);
	f->files[F_SEL_CLIENT]->bind = 1;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/ctl", id);
	f->files[F_CTL] = ixp_create(ixps, buf);
	f->files[F_CTL]->after_write = handle_after_write_frame;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/size", id);
	f->files[F_SIZE] = ixp_create(ixps, buf);
	f->files[F_SIZE]->before_read = handle_before_read_frame;
	f->files[F_SIZE]->after_write = handle_after_write_frame;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/border", id);
	f->files[F_BORDER] = wmii_create_ixpfile(ixps, buf, defaults[WM_BORDER]->content);
	f->files[F_BORDER]->after_write = handle_after_write_frame;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/tab", id);
	f->files[F_TAB] = wmii_create_ixpfile(ixps, buf, defaults[WM_TAB]->content);
	f->files[F_TAB]->after_write = handle_after_write_frame;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/handleinc", id);
	f->files[F_HANDLE_INC] = wmii_create_ixpfile(ixps, buf, defaults[WM_HANDLE_INC]->content);
	f->files[F_HANDLE_INC]->after_write = handle_after_write_frame;
	snprintf(buf, MAX_BUF, "/detached/frame/%d/locked", id);
	f->files[F_LOCKED] = wmii_create_ixpfile(ixps, buf, defaults[WM_LOCKED]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/selstyle/bgcolor", id);
	f->files[F_SEL_BG_COLOR] = wmii_create_ixpfile(ixps, buf, defaults[WM_SEL_BG_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/selstyle/fgcolor", id);
	f->files[F_SEL_FG_COLOR] = wmii_create_ixpfile(ixps, buf, defaults[WM_SEL_FG_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/selstyle/bordercolor", id);
	f->files[F_SEL_BORDER_COLOR] = wmii_create_ixpfile(ixps, buf, defaults[WM_SEL_BORDER_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/normstyle/bgcolor", id);
	f->files[F_NORM_BG_COLOR] = wmii_create_ixpfile(ixps, buf, defaults[WM_NORM_BG_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/normstyle/fgcolor", id);
	f->files[F_NORM_FG_COLOR] = wmii_create_ixpfile(ixps, buf, defaults[WM_NORM_FG_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/normstyle/bordercolor", id);
	f->files[F_NORM_BORDER_COLOR] = wmii_create_ixpfile(ixps, buf, defaults[WM_NORM_BORDER_COLOR]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/event/b2press", id);
	f->files[F_EVENT_B2PRESS] = wmii_create_ixpfile(ixps, buf, defaults[WM_EVENT_B2PRESS]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/event/b3press", id);
	f->files[F_EVENT_B3PRESS] = wmii_create_ixpfile(ixps, buf, defaults[WM_EVENT_B3PRESS]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/event/b4press", id);
	f->files[F_EVENT_B4PRESS] = wmii_create_ixpfile(ixps, buf, defaults[WM_EVENT_B4PRESS]->content);
	snprintf(buf, MAX_BUF, "/detached/frame/%d/event/b5press", id);
	f->files[F_EVENT_B5PRESS] = wmii_create_ixpfile(ixps, buf, defaults[WM_EVENT_B5PRESS]->content);
	id++;

	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask
		| PointerMotionMask | SubstructureRedirectMask
		| SubstructureNotifyMask;

	if (add_frame_border) {
		int             bw = border_width(f);
		int             th = tab_height(f);
		f->rect.width += 2 * bw;
		f->rect.height += bw + (th ? th : bw);
	}
	f->win =
		XCreateWindow(dpy, root, f->rect.x, f->rect.y, f->rect.width, f->rect.height, 0,
			      DefaultDepth(dpy, screen_num), CopyFromParent,
			      DefaultVisual(dpy, screen_num),
			    CWOverrideRedirect | CWBackPixmap | CWEventMask,
			      &wa);

	XDefineCursor(dpy, f->win, f->cursor);
	f->gc = XCreateGC(dpy, f->win, 0, 0);
	XSync(dpy, False);
	frames =
		(Frame **) attach_item_end((void **) frames, f, sizeof(Frame *));
	return f;
}

void
focus_frame(Frame * f, int raise, int up, int down)
{
	Area *a = f->area;
	if (down && f->clients)
		focus_client(f->clients[f->sel], raise, 0);
	a->sel = index_item((void **)a->frames, f);
	a->files[A_SEL_FRAME]->content = f->files[F_PREFIX]->content;
	if (raise && a->page->sel == 0) /* only floating windows are raised */
		XRaiseWindow(dpy, f->win);
	if (up)
		focus_area(a, raise, up, 0);
}

Frame          *
win_to_frame(Window w)
{
	int             i;

	for (i = 0; frames && frames[i]; i++)
		if (frames[i]->win == w)
			return frames[i];
	return 0;
}

void 
free_frame(Frame * f)
{
	frames = (Frame **) detach_item((void **) frames, f, sizeof(Frame *));
	XFreeGC(dpy, f->gc);
	XDestroyWindow(dpy, f->win);
	ixp_remove_file(ixps, f->files[F_PREFIX]);
	if (ixps->errstr)
		fprintf(stderr, "wmiiwm: free_frame(): %s\n", ixps->errstr);
	free(f);
}

void 
focus_client(Client * c, int raise, int up)
{
	Frame          *f = 0;
	/* focus client */
	if (c) {
		f = c->frame;
		for (f->sel = 0; f->clients && f->clients[f->sel] != c; f->sel++);
		f->files[F_SEL_CLIENT]->content = c->files[C_PREFIX]->content;
		if (raise)
			XRaiseWindow(dpy, c->win);
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
	} else
		XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	invoke_core_event(defaults[WM_EVENT_CLIENT_UPDATE]);
	if (up && f)
		focus_frame(f, raise, up, 0);
}

static void 
resize_clients(Frame * f, int tabh, int bw)
{
	int             i;
	for (i = 0; f->clients && f->clients[i]; i++) {
		Client         *c = f->clients[i];
		c->rect.x = bw;
		c->rect.y = tabh ? tabh : bw;
		c->rect.width = f->rect.width - 2 * bw;
		c->rect.height = f->rect.height - bw - (tabh ? tabh : bw);
		XMoveResizeWindow(dpy, c->win, c->rect.x, c->rect.y,
				  c->rect.width, c->rect.height);
		configure_client(c);
	}
}

static void 
check_dimensions(Frame * f, unsigned int tabh, unsigned int bw)
{
	Client         *c = f->clients ? f->clients[f->sel] : 0;
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

static void
resize_incremental(Frame * f, unsigned int tabh, unsigned int bw)
{
	Client         *c = f->clients ? f->clients[f->sel] : 0;
	if (!c)
		return;
	/* increment stuff, see chapter 4.1.2.3 of the ICCCM Manual */
	if (c->size.flags & PResizeInc) {
		int             base_width = 0, base_height = 0;

		if (c->size.flags & PBaseSize) {
			base_width = c->size.base_width;
			base_height = c->size.base_height;
		} else if (c->size.flags & PMinSize) {
			/* base_{width,height} defaults to min_{width,height} */
			base_width = c->size.min_width;
			base_height = c->size.min_height;
		}
		/* client_width = base_width + i * c->size.width_inc for an integer i */
		f->rect.width -= (f->rect.width - 2 * bw - base_width) % c->size.width_inc;
		f->rect.height -= (f->rect.height - bw - (tabh ? tabh : bw) - base_height) % c->size.height_inc;
	}
}

void
resize_frame(Frame * f, XRectangle * r, XPoint * pt, int ignore_layout)
{
	unsigned int    tabh = tab_height(f);
	unsigned int    bw = border_width(f);
	/* do layout stuff if necessary */
	if (!ignore_layout) {
		Area        *a = f->area;
		if (a && a->layout) {
			a->layout->resize(f, r, pt);
		} else
			f->rect = *r;
	} else
		f->rect = *r;

	/* resize if client requests special size */
	check_dimensions(f, tabh, bw);

	if (f->files[F_HANDLE_INC]->content
	    && ((char *) f->files[F_HANDLE_INC]->content)[0] == '1')
		resize_incremental(f, tabh, bw);

	XMoveResizeWindow(dpy, f->win, f->rect.x, f->rect.y, f->rect.width, f->rect.height);
	resize_clients(f, (tabh ? tabh : bw), bw);
}


void 
draw_tab(Frame * f, char *text, int x, int y, int w, int h, int sel)
{
	Draw            d = {0};
	d.drawable = f->win;
	d.gc = f->gc;
	d.rect.x = x;
	d.rect.y = y;
	d.rect.width = w;
	d.rect.height = h;
	d.data = text;
	if (sel) {
		d.bg = blitz_loadcolor(dpy, screen_num, f->files[F_SEL_BG_COLOR]->content);
		d.fg = blitz_loadcolor(dpy, screen_num, f->files[F_SEL_FG_COLOR]->content);
		d.border = blitz_loadcolor(dpy, screen_num, f->files[F_SEL_BORDER_COLOR]->content);
		d.font = font;
	} else {
		d.bg = blitz_loadcolor(dpy, screen_num, f->files[F_NORM_BG_COLOR]->content);
		d.fg = blitz_loadcolor(dpy, screen_num, f->files[F_NORM_FG_COLOR]->content);
		d.border = blitz_loadcolor(dpy, screen_num, f->files[F_NORM_BORDER_COLOR]->content);
		d.font = font;
	}
	blitz_drawlabel(dpy, &d);
	XSync(dpy, False);
	XFreeFont(dpy, font);
}


/**
 * Assumes following files:
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
void 
draw_frame(Frame * f)
{
	Draw            d = {0};
	int             bw = border_width(f);
	XRectangle      notch;
	if (bw) {
		notch.x = bw;
		notch.y = bw;
		notch.width = f->rect.width - 2 * bw;
		notch.height = f->rect.height - 2 * bw;
		d.drawable = f->win;
		d.gc = f->gc;

		/* define ground plate (i = 0) */
		if (ISSELFRAME(f)) {
			d.bg = blitz_loadcolor(dpy, screen_num, f->files[F_SEL_BG_COLOR]->content);
			d.fg = blitz_loadcolor(dpy, screen_num, f->files[F_SEL_FG_COLOR]->content);
			d.border = blitz_loadcolor(dpy, screen_num, f->files[F_SEL_BORDER_COLOR]->content);
		} else {
			d.bg = blitz_loadcolor(dpy, screen_num, f->files[F_NORM_BG_COLOR]->content);
			d.fg = blitz_loadcolor(dpy, screen_num, f->files[F_NORM_FG_COLOR]->content);
			d.border = blitz_loadcolor(dpy, screen_num, f->files[F_NORM_BORDER_COLOR]->content);
		}
		d.rect = f->rect;
		d.rect.x = d.rect.y = 0;
		d.notch = &notch;

		blitz_drawlabel(dpy, &d);
	}
	draw_clients(f);
	XSync(dpy, False);
}

void 
handle_frame_buttonpress(XButtonEvent * e, Frame * f)
{
	int             bindex;
	int             size = count_items((void **) f->clients);
	int             cindex = e->x / f->rect.width / size;
	if (!f->area->page->sel)
		XRaiseWindow(dpy, f->win);
	if (cindex != f->sel) {
		focus_client(f->clients[cindex], 1, 0);
		draw_frame(f);
		return;
	}
	if (e->button == Button1) {
		mouse();
		return;
	}
	bindex = F_EVENT_B2PRESS - 2 + e->button;
	/* frame mouse handling */
	if (f->files[bindex]->content)
		spawn(dpy, f->files[bindex]->content);
	draw_frame(f);
}

void 
attach_Cliento_frame(Frame * f, Client * c)
{
	int             size = count_items((void **) f->clients);
	wmii_move_ixpfile(c->files[C_PREFIX], f->files[F_CLIENT_PREFIX]);
	f->files[F_SEL_CLIENT]->content = c->files[C_PREFIX]->content;
	f->clients = (Client **) attach_item_end((void **) f->clients, c, sizeof(Client *));
	f->sel = size;
	c->frame = f;
	reparent_client(c, f->win, border_width(f), tab_height(f));
	resize_frame(f, &f->rect, 0, 1);
	show_client(c);
	focus_client(c, 1, 1);
}

void 
attach_client(Client * c)
{
	Page           *p = 0;
	Frame          *f = 0;
	if (!pages)
		alloc_page("0");
	/* transient stuff */
	if (c && c->trans && !f) {
		Client         *t = win_to_client(c->trans);
		if (t && t->frame) {
			focus_client(t, 1, 1);
			f = alloc_frame(&c->rect, 1, 1);
		}
	}
	p = pages[sel];

	if (!f) {
		/* check if we shall manage it */
		if (!manage_class_instance(c))
			f = alloc_frame(&c->rect, 1, 1);
	}
	if (!f) {
		/* check for tabbing? */
		f = SELFRAME(p);
		if (f && (((char *) f->files[F_LOCKED]->content)[0] == '1'))
			f = 0;
	}
	if (!f)
		f = alloc_frame(&c->rect, 1, 0);

	if (!f->area)
		attach_frame_to_area(p->areas[p->sel], f);
	attach_client_to_frame(f, c);
	draw_frame(f);
	invoke_core_event(defaults[WM_EVENT_PAGE_UPDATE]);
}

void 
detach_client_from_frame(Client * c, int unmapped, int destroyed)
{
	Frame          *f = c->frame;
	wmii_move_ixpfile(c->files[C_PREFIX], defaults[WM_DETACHED_CLIENT]);
	c->frame = 0;
	f->clients =
		(Client **) detach_item((void **) f->clients, c, sizeof(Client *));
	if (f->sel)
		f->sel--;
	else
		f->sel = 0;
	if (!destroyed) {
		if (!unmapped) {
			hide_client(c);
			detached =
				(Client **) attach_item_begin((void **) detached, c,
							  sizeof(Client *));
		}
		reparent_client(c, root, border_width(f), tab_height(f));
	}
	if (f->clients) {
		focus_client(f->clients[f->sel], 1, 1);
		draw_frame(f);
	} else {
		detach_frame_from_area(f, 0);
		free_frame(f);
		if (pages)
			focus_page(pages[sel], 0, 1);
	}
	invoke_core_event(defaults[WM_EVENT_PAGE_UPDATE]);
}

static void 
mouse()
{
	Frame          *f;
	Align           align;

	if (!pages)
		return;
	if (!(f = SELFRAME(pages[sel])))
		return;
	align = cursor_to_align(f->cursor);
	if (align == CENTER)
		mouse_move(f);
	else
		mouse_resize(f, align);
}

static void 
select_client(void *obj, char *cmd)
{
	Frame          *f = obj;
	int             size = count_items((void **) f->clients);
	if (!f || !cmd || size == 1)
		return;
	if (!strncmp(cmd, "prev", 5)) {
		if (f->sel > 0)
			f->sel--;
		else
			f->sel = size - 1;
	} else if (!strncmp(cmd, "next", 5)) {
		if (f->sel + 1 == size)
			f->sel = 0;
		else
			f->sel++;
	}
	focus_client(f->clients[f->sel], 1, 0);
	draw_frame(f);
}

static void 
handle_before_read_frame(IXPServer * s, File * f)
{
	int             i = 0;

	for (i = 0; frames && frames[i]; i++) {
		if (f == frames[i]->files[F_SIZE]) {
			char            buf[64];
			snprintf(buf, 64, "%d,%d,%d,%d", frames[i]->rect.x, frames[i]->rect.y,
					frames[i]->rect.width, frames[i]->rect.height);
			if (f->content)
				free(f->content);
			f->content = estrdup(buf);
			f->size = strlen(buf);
			return;
		}
	}
}

static void 
handle_after_write_frame(IXPServer * s, File * f)
{
	int             i;

	for (i = 0; frames && frames[i]; i++) {
		if (f == frames[i]->files[F_CTL]) {
			run_action(f, frames[i], frame_acttbl);
			return;
		}
		if (f == frames[i]->files[F_TAB]
		    || f == frames[i]->files[F_BORDER]
		    || f == frames[i]->files[F_HANDLE_INC]) {
			if (frames[i]->area) {
				frames[i]->area->layout->arrange(frames[i]->area);
				draw_page(frames[i]->area->page);
			}
			return;
		} else if (f == frames[i]->files[F_SIZE]) {
			char           *size = frames[i]->files[F_SIZE]->content;
			if (size && strrchr(size, ',')) {
				XRectangle      frect;
				blitz_strtorect(dpy, &rect, &frect, size);
				resize_frame(frames[i], &frect, 0, 0);
				draw_page(frames[i]->area->page);
			}
			return;
		}
	}
}
