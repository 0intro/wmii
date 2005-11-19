/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/keysym.h>

#include "wm.h"

static void     new_page(void *obj, char *cmd);
static void     _select_page(void *obj, char *cmd);
static void     _destroy_page(void *obj, char *cmd);
static void     quit(void *obj, char *cmd);
static void     _attach_client(void *obj, char *cmd);
static void     _detach_client(void *obj, char *cmd);
static void     _close_client(void *obj, char *cmd);
static void     pager(void *obj, char *cmd);
static void     icons(void *obj, char *cmd);

/* action table for /ctl namespace */
Action          core_acttbl[] = {
	{"new", new_page},
	{"destroy", _destroy_page},
	{"select", _select_page},
	{"attach", _attach_client},
	{"detach", _detach_client},
	{"close", _close_client},
	{"quit", quit},
	{"pager", pager},
	{"icons", icons},
	{0, 0}
};

void 
run_action(File * f, void *obj, Action * acttbl)
{
	int             i;
	size_t          len;

	if (!f->content)
		return;
	for (i = 0; acttbl[i].name; i++) {
		len = strlen(acttbl[i].name);
		if (!strncmp(acttbl[i].name, (char *) f->content, len)) {
			if (f->size > len)
				acttbl[i].func(obj, &((char *) f->content)[len + 1]);
			else
				acttbl[i].func(obj, 0);
			return;
			break;
		}
	}
	fprintf(stderr, "wmiiwm: unknown action '%s'\n"
		"     or invalid ctl device\n", (char *) f->content);
}

static void 
quit(void *obj, char *cmd)
{
	ixps->runlevel = SHUTDOWN;
}

void 
invoke_core_event(File * f)
{
	if (!f->content)
		return;
	spawn(dpy, f->content);
}

void 
focus_page(Page * p, int raise, int down)
{
	if (!pages)
		return;
	if (p != pages[sel]) {
		hide_page(pages[sel]);
		sel = index_item((void **) pages, p);
		show_page(pages[sel]);
		defaults[WM_SEL_PAGE]->content = p->files[P_PREFIX]->content;
		invoke_core_event(defaults[WM_EVENT_PAGE_UPDATE]);
	}
	if (down)
		focus_area(p->areas[p->sel], raise, 0, down);
}

unsigned int
tab_height(Frame *f)
{
	if (_strtonum(f->files[F_TAB]->content, 0, 1))
		return font->ascent + font->descent + 4;
	return 0;
}

unsigned int
border_width(Frame *f)
{
	if(_strtonum(f->files[F_BORDER]->content, 0, 1))
		return BORDER_WIDTH;
	return 0;
}

static void
scale_rect(XRectangle * from_dim, XRectangle * to_dim,
	   XRectangle * src, XRectangle * tgt)
{
	double          wfact = (double) to_dim->width / (double) from_dim->width;
	double          hfact = (double) to_dim->height / (double) from_dim->height;

	tgt->x = to_dim->x + (src->x * wfact);
	tgt->y = to_dim->y + (src->y * hfact);
	tgt->width = (src->width * wfact);
	tgt->height = (src->height * hfact);

	if (tgt->width < 1)
		tgt->width = 1;
	if (tgt->height < 1)
		tgt->height = 1;
}

static void 
draw_pager_page(Page * p, Draw * d)
{
	unsigned int    i, j;
	XRectangle      r = d->rect;
	char name[4];
	if (p == pages[sel]) {
		d->bg = blitz_loadcolor(dpy, screen_num, defaults[WM_SEL_BG_COLOR]->content);
		d->fg = blitz_loadcolor(dpy, screen_num, defaults[WM_SEL_FG_COLOR]->content);
		d->border = blitz_loadcolor(dpy, screen_num, defaults[WM_SEL_BORDER_COLOR]->content);
		d->font = font;
	} else {
		d->bg = blitz_loadcolor(dpy, screen_num, defaults[WM_NORM_BG_COLOR]->content);
		d->fg = blitz_loadcolor(dpy, screen_num, defaults[WM_NORM_FG_COLOR]->content);
		d->border = blitz_loadcolor(dpy, screen_num, defaults[WM_NORM_BORDER_COLOR]->content);
		d->font = font;
	}
	snprintf(name, sizeof(name), "%d", index_item((void **)pages, p));
	d->data = name;
	blitz_drawlabel(dpy, d);
	XSync(dpy, False);

	for (i = 0; p->areas[i]; i++) {
		for(j = 0; p->areas[i]->frames[j]; j++) {
			if (i == p->sel && j == p->areas[i]->sel) {
				d->bg = blitz_loadcolor(dpy, screen_num, defaults[WM_SEL_BG_COLOR]->content);
				d->fg = blitz_loadcolor(dpy, screen_num, defaults[WM_SEL_FG_COLOR]->content);
				d->border = blitz_loadcolor(dpy, screen_num, defaults[WM_SEL_BORDER_COLOR]->content);
				d->font = font;
			} else {
				d->bg = blitz_loadcolor(dpy, screen_num, defaults[WM_NORM_BG_COLOR]->content);
				d->fg = blitz_loadcolor(dpy, screen_num, defaults[WM_NORM_FG_COLOR]->content);
				d->border = blitz_loadcolor(dpy, screen_num, defaults[WM_NORM_BORDER_COLOR]->content);
				d->font = font;
			}
			d->data = p->areas[i]->frames[j]->clients[p->areas[i]->frames[j]->sel]->files[C_NAME]->content;
			scale_rect(&rect, &r, &p->areas[i]->rect, &d->rect);
			blitz_drawlabel(dpy, d);
			XSync(dpy, False); /* do not clear upwards */
		}
	}
}

static void 
draw_pager()
{
	unsigned int    ic, ir, tw, th, rows, cols, size;
	int             i = 0;
	int             dx;
	Draw            d = {0};

	blitz_getbasegeometry((void **) pages, &size, &cols, &rows);
	dx = (cols - 1) * GAP;	/* GAPpx space */
	tw = (rect.width - dx) / cols;
	th = ((double) tw / rect.width) * rect.height;
	d.drawable = transient;
	d.gc = transient_gc;
	for (ir = 0; ir < rows; ir++) {
		for (ic = 0; ic < cols; ic++) {
			d.rect.x = ic * tw + (ic * GAP);
			d.rect.width = tw;
			if (rows == 1)
				d.rect.y = 0;
			else
				d.rect.y = ir * (rect.height - th) / (rows - 1);
			d.rect.height = th;
			if (!pages[i])
				return;
			draw_pager_page(pages[i], &d);
			i++;
		}
	}
}

static Page    *
xy_to_pager_page(int x, int y)
{
	unsigned int    ic, ir, tw, th, rows, cols, size;
	int             i = 0;
	int             dx;
	XRectangle      r;

	if (!pages)
		return 0;
	blitz_getbasegeometry((void **) pages, &size, &cols, &rows);
	dx = (cols - 1) * GAP;	/* GAPpx space */
	tw = (rect.width - dx) / cols;
	th = ((double) tw / rect.width) * rect.height;

	for (ir = 0; ir < rows; ir++) {
		for (ic = 0; ic < cols; ic++) {
			r.x = ic * tw + (ic * GAP);
			r.width = tw;
			if (rows == 1)
				r.y = 0;
			else
				r.y = ir * (rect.height - th) / (rows - 1);
			r.height = th;
			if (!pages[i])
				return 0;
			if (blitz_ispointinrect(x, y, &r))
				return pages[i];
			i++;
		}
	}
	return 0;
}

static int 
handle_kpress(XKeyEvent * e)
{
	KeySym          ksym = XKeycodeToKeysym(dpy, e->keycode, 0);

	if (ksym >= XK_1 && ksym <= XK_9)
		return ksym - XK_1;
	else if (ksym == XK_0)
		return 9;
	else if (ksym >= XK_a && ksym <= XK_z)
		return 10 + ksym - XK_a;

	return -1;
}

static void 
pager(void *obj, char *cmd)
{
	XEvent          ev;
	int             i;

	if (!pages)
		return;

	XClearWindow(dpy, transient);
	XMapRaised(dpy, transient);
	draw_pager();
	while (XGrabKeyboard(dpy, transient, True, GrabModeAsync, GrabModeAsync, CurrentTime) != GrabSuccess)
		usleep(1000);

	for (;;) {
		while (!XCheckWindowEvent(dpy, transient, ButtonPressMask | KeyPressMask, &ev)) {
			usleep(20000);
			continue;
		}

		switch (ev.type) {
		case KeyPress:
			XUnmapWindow(dpy, transient);
			if ((i = handle_kpress(&ev.xkey)) != -1)
				if (i < count_items((void **) pages))
					focus_page(pages[i], 0, 1);
			XUngrabKeyboard(dpy, CurrentTime);
			return;
			break;
		case ButtonPress:
			XUnmapWindow(dpy, transient);
			if (ev.xbutton.button == Button1) {
				Page           *p = xy_to_pager_page(ev.xbutton.x, ev.xbutton.y);
				if (p)
					focus_page(p, 0, 1);
			}
			return;
			break;
		}
	}
}

static void 
draw_icons()
{
	unsigned int    i, ic, ir, tw, th, rows, cols, size;
	int             dx, dy;

	if (!detached)
		return;
	blitz_getbasegeometry((void **) detached, &size, &cols, &rows);
	dx = (cols - 1) * GAP;	/* GAPpx space */
	dy = (rows - 1) * GAP;	/* GAPpx space */
	tw = (rect.width - dx) / cols;
	th = (rect.height - dy) / rows;

	XClearWindow(dpy, transient);
	XMapRaised(dpy, transient);
	i = 0;
	for (ir = 0; ir < rows; ir++) {
		for (ic = 0; ic < cols; ic++) {
			Client         *c = detached[i++];
			XRectangle      cr;
			if (!c)
				return;
			cr.x = ic * tw + (ic * GAP);
			cr.y = ir * th + (ir * GAP);
			cr.width = tw;
			cr.height = th;
			XMoveResizeWindow(dpy, c->win, cr.x, cr.y, cr.width,
					  cr.height);
			configure_client(c);
			show_client(c);
			XRaiseWindow(dpy, c->win);
			grab_client(c, AnyModifier, AnyButton);
			XSync(dpy, False);
		}
	}
}


static void 
icons(void *obj, char *cmd)
{
	XEvent          ev;
	int             i, n;
	Client         *c;

	if (!detached)
		return;

	XClearWindow(dpy, transient);
	XMapRaised(dpy, transient);
	draw_icons();
	while (XGrabKeyboard(dpy, transient, True, GrabModeAsync, GrabModeAsync, CurrentTime) != GrabSuccess)
		usleep(1000);

	for (;;) {
		while (!XCheckMaskEvent(dpy, ButtonPressMask | KeyPressMask, &ev)) {
			usleep(20000);
			continue;
		}
		switch (ev.type) {
		case KeyPress:
			XUnmapWindow(dpy, transient);
			if ((n = handle_kpress(&ev.xkey)) != -1) {
				for (i = 0; detached && detached[i]; i++)
					hide_client(detached[i]);
				if (n - 1 < i) {
					c = detached[n];
					detached = (Client **) detach_item((void **) detached, c,
							  sizeof(Client *));
					attach_client(c);
				}
			} else {
				for (i = 0; detached && detached[i]; i++)
					hide_client(detached[i]);
			}
			XUngrabKeyboard(dpy, CurrentTime);
			return;
			break;
		case ButtonPress:
			if (ev.xbutton.button == Button1) {
				XUnmapWindow(dpy, transient);
				for (i = 0; detached && detached[i]; i++)
					hide_client(detached[i]);
				if ((c = win_to_client(ev.xbutton.window))) {
					detached =
						(Client **) detach_item((void **) detached, c,
							  sizeof(Client *));
					attach_client(c);
				}
				XUngrabKeyboard(dpy, CurrentTime);
			}
			return;
			break;
		}
	}
}

static void 
_close_client(void *obj, char *cmd)
{
	Frame *f = pages ? SELFRAME(pages[sel]) : 0;
    if (f->clients[f->sel])
		close_client(f->clients[f->sel]);
}

static void 
_attach_client(void *obj, char *cmd)
{
	if (detached) {
		Client         *c = detached[0];
		detached =
			(Client **) detach_item((void **) detached, c,
						sizeof(Client *));
		attach_client(c);
	}
}

static void 
_detach_client(void *obj, char *cmd)
{
	Frame          *f;
	if (!pages)
		return;
	f = SELFRAME(pages[sel]);
	if (!f)
		return;
	detach_client_from_frame(f->clients[f->sel], 0, 0);
}

static void 
_select_page(void *obj, char *cmd)
{
	if (!pages || !cmd)
		return;
	if (!strncmp(cmd, "prev", 5))
		sel = index_prev_item((void **) pages, pages[sel]);
	else if (!strncmp(cmd, "next", 5))
		sel = index_next_item((void **) pages, pages[sel]);
	else
		sel = _strtonum(cmd, 0, count_items((void **)pages));
	focus_page(pages[sel], 0, 1);
}

void 
destroy_page(Page * p)
{
	unsigned int     i;
	for (i = 0; p->areas[i]; i++)
		destroy_area(p->areas[i]);
	free_page(p);
	if (pages) {
		show_page(pages[sel]);
		defaults[WM_SEL_PAGE]->content = pages[sel]->files[P_PREFIX]->content;
		focus_page(pages[sel], 0, 1);
		invoke_core_event(defaults[WM_EVENT_PAGE_UPDATE]);
	}
}

static void 
_destroy_page(void *obj, char *cmd)
{
	if (!pages)
		return;
	destroy_page(pages[sel]);
}

static void 
new_page(void *obj, char *cmd)
{
	if (pages)
		hide_page(pages[sel]);
	alloc_page("0");
}

Client         *
win_to_client(Window w)
{
	int             i;

	for (i = 0; clients && clients[i]; i++)
		if (clients[i]->win == w)
			return clients[i];
	return 0;
}

void 
scan_wins()
{
	int             i;
	unsigned int    num;
	Window         *wins;
	XWindowAttributes wa;
	Window          d1, d2;
	Client         *c;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (wa.override_redirect
			    || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable) {
				c = alloc_client(wins[i]);
				_init_client(c, &wa);
				attach_client(c);
			}
		}
	}
	if (wins)
		XFree(wins);
}

void           *
get_func(void *acttbl[][2], int rows, char *fname)
{
	int             i;
	for (i = 0; i < rows; i++) {
		if (!strncmp((char *) acttbl[i][0], fname, MAX_BUF)) {
			return acttbl[i][1];
		}
	}
	return 0;
}

int 
win_proto(Window w)
{
	Atom           *protocols;
	long            res;
	int             protos = 0;
	int             i;

	res = property(dpy, w, wm_protocols, XA_ATOM,
		       20L, ((unsigned char **) &protocols));
	if (res <= 0) {
		return protos;
	}
	for (i = 0; i < res; i++) {
		if (protocols[i] == wm_delete) {
			protos |= PROTO_DEL;
		}
	}
	free((char *) protocols);
	return protos;
}

int 
win_state(Window w)
{
	/* state hints */
	XWMHints       *hints = XGetWMHints(dpy, w);
	int             res;

	long           *prop = 0;
	if (property(dpy, w, wm_state, wm_state,
		     2L, ((unsigned char **) &prop)) > 0) {
		res = (int) *prop;
		free((long *) prop);
	} else {
		res = hints ? hints->initial_state : NormalState;
	}

	if (hints) {
		free(hints);
	}
	return res;
}

void 
handle_after_write(IXPServer * s, File * f)
{
	if (f == defaults[WM_CTL])
		run_action(f, 0, core_acttbl);
	else if (f == defaults[WM_TRANS_COLOR]) {
		unsigned long   col[1];
		col[0] = xorcolor.pixel;
		XFreeColors(dpy, DefaultColormap(dpy, screen_num), col, 1, 0);
		XAllocNamedColor(dpy, DefaultColormap(dpy, screen_num),
				 defaults[WM_TRANS_COLOR]->content,
				 &xorcolor, &xorcolor);
		XSetForeground(dpy, xorgc, xorcolor.pixel);
	}
	check_event(0);
}
