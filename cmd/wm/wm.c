/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <X11/cursorxfont.h>
#include <X11/Xproto.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>

#include "wm.h"

static XRectangle initial_rect;
static int other_wm_running;
static int (*x_error_handler) (Display *, XErrorEvent *);

static void new_page(void *obj, char *arg);
static void xselect_page(void *obj, char *arg);
static void xdestroy_page(void *obj, char *arg);
static void quit(void *obj, char *arg);
static void xattach_client(void *obj, char *arg);
static void xdetach_client(void *obj, char *arg);
static void xclose_client(void *obj, char *arg);
static void pager(void *obj, char *arg);
static void detached_client(void *obj, char *arg);

/* action table for /ctl namespace */
Action wm_acttbl[] = {
    {"new", new_page},
    {"destroy", xdestroy_page},
    {"select", xselect_page},
    {"attach", xattach_client},
    {"detach", xdetach_client},
    {"close", xclose_client},
    {"quit", quit},
    {"pager", pager},
    {"detclient", detached_client},
    {0, 0}
};

char *version[] = {
    "wmiiwm - window manager improved 2 - " VERSION "\n"
        " (C)opyright MMIV-MMVI Anselm R. Garbe\n", 0
};

static void
usage()
{
    fprintf(stderr, "%s",
            "usage: wmiiwm -s <socket file> [-c] [-v]\n"
            "      -s    socket file\n"
            "      -c    checks if another WM is already running\n"
            "      -v    version info\n");
    exit(1);
}

void
run_action(File * f, void *obj, Action * acttbl)
{
    int i;
    size_t len;

    if(!f->content)
        return;
    for(i = 0; acttbl[i].name; i++) {
        len = strlen(acttbl[i].name);
        if(!strncmp(acttbl[i].name, (char *) f->content, len)) {
            if(f->size > len)
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
quit(void *obj, char *arg)
{
    ixps->runlevel = SHUTDOWN;
}

void
invoke_wm_event(File * f)
{
    if(!f->content)
        return;
    wmii_spawn(dpy, f->content);
}

static void
scale_rect(XRectangle * from_dim, XRectangle * to_dim,
           XRectangle * src, XRectangle * tgt)
{
    double wfact = (double) to_dim->width / (double) from_dim->width;
    double hfact = (double) to_dim->height / (double) from_dim->height;

    tgt->x = to_dim->x + (src->x * wfact);
    tgt->y = to_dim->y + (src->y * hfact);
    tgt->width = (src->width * wfact);
    tgt->height = (src->height * hfact);

    if(tgt->width < 1)
        tgt->width = 1;
    if(tgt->height < 1)
        tgt->height = 1;
}

static void
draw_pager_client(Client *c, Draw *d)
{
	if(c == sel_client()) {
    	d->bg = blitz_loadcolor(dpy, screen, def[WM_SEL_BG_COLOR]->content);
        d->fg = blitz_loadcolor(dpy, screen, def[WM_SEL_FG_COLOR]->content);
        d->border = blitz_loadcolor(dpy, screen, def[WM_SEL_BORDER_COLOR]->content);
    } else {
    	d->bg = blitz_loadcolor(dpy, screen, def[WM_NORM_BG_COLOR]->content);
        d->fg = blitz_loadcolor(dpy, screen, def[WM_NORM_FG_COLOR]->content);
        d->border = blitz_loadcolor(dpy, screen, def[WM_NORM_BORDER_COLOR]->content);
    }
    d->data = c->name;
    scale_rect(&rect, &initial_rect, &c->frame.rect, &d->rect);
    blitz_drawlabel(dpy, d);
    XSync(dpy, False);      /* do not clear upwards */
}

static void
draw_pager_page(size_t idx, Draw *d)
{
	size_t i, j;
    char name[4];
    initial_rect = d->rect;

    if(idx == sel_page) {
        d->bg = blitz_loadcolor(dpy, screen, def[WM_SEL_BG_COLOR]->content);
        d->fg = blitz_loadcolor(dpy, screen, def[WM_SEL_FG_COLOR]->content);
        d->border = blitz_loadcolor(dpy, screen, def[WM_SEL_BORDER_COLOR]->content);
    } else {
        d->bg = blitz_loadcolor(dpy, screen, def[WM_NORM_BG_COLOR]->content);
        d->fg = blitz_loadcolor(dpy, screen, def[WM_NORM_FG_COLOR]->content);
        d->border = blitz_loadcolor(dpy, screen, def[WM_NORM_BORDER_COLOR]->content);
    }
    snprintf(name, sizeof(name), "%d", idx + 1);
    d->data = name;
    blitz_drawlabel(dpy, d);
    XSync(dpy, False);

	for(i = 0; (i < page[idx]->areasz) && page[idx]->area[i]; i++) {
		Area *col = page[idx]->area[i];
		for(j = 0; (i < col->clientsz) && col->client[j]; j++)
			draw_pager_client(col->client[j], d);
	}
	for(i = 0; (i < page[idx]->floatingsz) && page[idx]->floating[i]; i++)
		draw_pager_client(page[idx]->floating[i], d);
}

static void
draw_pager()
{
    Draw d = { 0 };
    unsigned int i, ic, ir, tw, th, rows, cols;
    int dx;

	for(i = 0; (i < pagesz) && page[i]; i++);
    blitz_getbasegeometry(i, &cols, &rows);
    dx = (cols - 1) * GAP;      /* GAPpx space */
    tw = (rect.width - dx) / cols;
    th = ((double) tw / rect.width) * rect.height;
    d.drawable = transient;
    d.gc = gc_transient;
    d.xfont = xfont;
	i = 0;
    for(ir = 0; ir < rows; ir++) {
        for(ic = 0; ic < cols; ic++) {
            d.rect.x = ic * tw + (ic * GAP);
            d.rect.width = tw;
            if(rows == 1)
                d.rect.y = 0;
            else
                d.rect.y = ir * (rect.height - th) / (rows - 1);
            d.rect.height = th;
            draw_pager_page(i++, &d);
            if(!page[i])
                return;
        }
    }
}

static int
xy_to_pager_page(int x, int y)
{
    unsigned int i, ic, ir, tw, th, rows, cols;
    int dx;
    XRectangle r;

	for(i = 0; (i < pagesz) && page[i]; i++);
    blitz_getbasegeometry(i, &cols, &rows);
    dx = (cols - 1) * GAP;      /* GAPpx space */
    tw = (rect.width - dx) / cols;
    th = ((double) tw / rect.width) * rect.height;

	i = 0;
    for(ir = 0; ir < rows; ir++) {
        for(ic = 0; ic < cols; ic++) {
            r.x = ic * tw + (ic * GAP);
            r.width = tw;
            if(rows == 1)
                r.y = 0;
            else
                r.y = ir * (rect.height - th) / (rows - 1);
            r.height = th;
            if(blitz_ispointinrect(x, y, &r))
                return i;
			i++;
            if(!page[i])
                return -1;
        }
    }
    return -1;
}

static int
handle_kpress(XKeyEvent * e)
{
    KeySym ksym = XKeycodeToKeysym(dpy, e->keycode, 0);

    if(ksym >= XK_1 && ksym <= XK_9)
        return ksym - XK_1;
	else if(ksym == XK_0)
		return 9;
    else if(ksym >= XK_a && ksym <= XK_z)
        return 10 + ksym - XK_a;

    return -1;
}

static void
pager(void *obj, char *arg)
{
    XEvent ev;
    int i;
	size_t j;

    if(!page)
        return;

	for(j = 0; (j < pagesz) && page[j]; j++);

    XClearWindow(dpy, transient);
    XMapRaised(dpy, transient);
    draw_pager();

    while(XGrabKeyboard
          (dpy, transient, True, GrabModeAsync, GrabModeAsync,
           CurrentTime) != GrabSuccess)
        usleep(20000);

    while(XGrabPointer
          (dpy, transient, False, ButtonPressMask, GrabModeAsync,
           GrabModeAsync, None, normal_cursor, CurrentTime) != GrabSuccess)
        usleep(20000);

    for(;;) {
        while(!XCheckWindowEvent
              (dpy, transient, ButtonPressMask | KeyPressMask, &ev)) {
            usleep(20000);
            continue;
        }

        switch (ev.type) {
        case KeyPress:
            XUnmapWindow(dpy, transient);
            if((i = handle_kpress(&ev.xkey)) != -1)
                if(i < j)
					focus_page(page[i]);
            XUngrabKeyboard(dpy, CurrentTime);
            XUngrabPointer(dpy, CurrentTime /* ev.xbutton.time */ );
            return;
            break;
        case ButtonPress:
            XUnmapWindow(dpy, transient);
            if(ev.xbutton.button == Button1)
                focus_page(page[xy_to_pager_page(ev.xbutton.x, ev.xbutton.y)]);
            XUngrabKeyboard(dpy, CurrentTime);
            XUngrabPointer(dpy, CurrentTime /* ev.xbutton.time */ );
            return;
            break;
        }
    }
}

static void
map_detached_client()
{
    unsigned int i, ic, ir, tw, th, rows, cols;
    int dx, dy;
    XRectangle cr;

	for(i = 0; detached && detached[i]; i++);
    blitz_getbasegeometry(i, &cols, &rows);
	if(!cols)
		cols = 1;
	if(!rows)
		rows = 1;
    dx = (cols - 1) * GAP;      /* GAPpx space */
    dy = (rows - 1) * GAP;      /* GAPpx space */
    tw = (rect.width - dx) / cols;
    th = (rect.height - dy) / rows;

	i = 0;
    for(ir = 0; ir < rows; ir++) {
        for(ic = 0; ic < cols; ic++) {
			if(!detached[i++])
                return;
            cr.x = ic * tw + (ic * GAP);
            cr.y = ir * th + (ir * GAP);
            cr.width = tw;
            cr.height = th;
            XMoveResizeWindow(dpy, detached[i]->win, cr.x, cr.y, cr.width, cr.height);
            configure_client(detached[i]);
            map_client(detached[i]);
			grab_client(detached[i], AnyModifier, Button1);
            XRaiseWindow(dpy, detached[i]->win);
            XSync(dpy, False);
        }
    }
}

static void
detached_client(void *obj, char *arg)
{
    XEvent ev;
    int n;
	size_t i, nc;
	Client *c;

	for(nc = 0; detached && detached[nc]; nc++);
    if(!nc)
        return;
    XClearWindow(dpy, transient);
    XMapRaised(dpy, transient);
    map_detached_client();
    while(XGrabKeyboard
          (dpy, transient, True, GrabModeAsync, GrabModeAsync,
           CurrentTime) != GrabSuccess)
        usleep(20000);

    for(;;) {
        while(!XCheckMaskEvent(dpy, ButtonPressMask | KeyPressMask, &ev)) {
            usleep(20000);
            continue;
        }
        switch (ev.type) {
        case KeyPress:
            XUnmapWindow(dpy, transient);
            for(i = 0; detached[i]; i++)
                unmap_client(detached[i]);
            if((n = handle_kpress(&ev.xkey)) != -1) {
                if(n < nc) {
					c = detached[n];
					cext_array_detach((void **)detached, c, &detachedsz);
                    attach_client(c);
                }
            }
            XUngrabKeyboard(dpy, CurrentTime);
            return;
            break;
        case ButtonPress:
            XUnmapWindow(dpy, transient);
            for(i = 0; detached[i]; i++)
                unmap_client(detached[i]);
            if((ev.xbutton.button == Button1)
               && (c = win_to_client(ev.xbutton.window))) {
				cext_array_detach((void **)detached, c, &detachedsz);
                attach_client(c);
            }
            XUngrabKeyboard(dpy, CurrentTime);
            return;
            break;
        }
    }
}

static void
xclose_client(void *obj, char *arg)
{
    Client *c = sel_client();
    if(c)
        close_client(c);
}

static void
xattach_client(void *obj, char *arg)
{
    Client *c = detached ? detached[0] : nil;
    if(c) {
		cext_array_detach((void **)detached, c, &detachedsz);
        attach_client(c);
    }
}

static void
xdetach_client(void *obj, char *arg)
{
    Client *c = sel_client();
    if(c)
        detach_client(c, False);
}

static void
xselect_page(void *obj, char *arg)
{
	size_t np, new = sel_page;

	for(np = 0; (np < pagesz) && page[np]; np++);
    if(!np || !arg)
        return;
    if(!strncmp(arg, "prev", 5)) {
		if(new > 0)
			for(new = 0; page[new]; new++);
		new--;
    } else if(!strncmp(arg, "next", 5)) {
		if(page[new + 1])
			new++;
		else
			new = 0;
    } else {
		int idx = blitz_strtonum(arg, 0, np);
		if(idx < np)
			new = idx;
	}
    focus_page(page[new]);
}

static void
xdestroy_page(void *obj, char *arg)
{
    destroy_page(page[sel_page]);
}

static void
new_page(void *obj, char *arg)
{
    alloc_page();
}

Client *
win_to_client(Window w)
{
	size_t i;

	for(i = 0; (i < clientsz) && client[i]; i++)
		if(client[i]->win == w)
			return client[i];
	return nil;
}

void
scan_wins()
{
    int i;
    unsigned int num;
    Window *wins;
    XWindowAttributes wa;
    Window d1, d2;
    Client *c;

    if(XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
        for(i = 0; i < num; i++) {
            if(!XGetWindowAttributes(dpy, wins[i], &wa))
                continue;
            if(wa.override_redirect
               || XGetTransientForHint(dpy, wins[i], &d1))
                continue;
            if(wa.map_state == IsViewable) {
                c = alloc_client(wins[i], &wa);
                attach_client(c);
                c->ignore_unmap++;      /* was viewable already */
            }
        }
    }
    if(wins)
        XFree(wins);
}

void *
get_func(void *acttbl[][2], int rows, char *fname)
{
    int i;
    for(i = 0; i < rows; i++) {
        if(!strncmp((char *) acttbl[i][0], fname, MAX_BUF)) {
            return acttbl[i][1];
        }
    }
    return 0;
}

int
win_proto(Window w)
{
    Atom *protocols;
    long res;
    int protos = 0;
    int i;

    res =
        wmii_property(dpy, w, wm_protocols, XA_ATOM, 20L,
                      ((unsigned char **) &protocols));
    if(res <= 0) {
        return protos;
    }
    for(i = 0; i < res; i++) {
        if(protocols[i] == wm_delete) {
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
    XWMHints *hints = XGetWMHints(dpy, w);
    int res;

    long *prop = 0;
    if(wmii_property
       (dpy, w, wm_state, wm_state, 2L, ((unsigned char **) &prop)) > 0) {
        res = (int) *prop;
        free((long *) prop);
    } else {
        res = hints ? hints->initial_state : NormalState;
    }

    if(hints) {
        free(hints);
    }
    return res;
}

void
handle_after_write(IXPServer * s, File * f)
{
    if(f == def[WM_CTL])
        run_action(f, 0, wm_acttbl);
    else if(f == def[WM_TRANS_COLOR]) {
        unsigned long col[1];
        col[0] = color_xor.pixel;
        XFreeColors(dpy, DefaultColormap(dpy, screen), col, 1, 0);
        XAllocNamedColor(dpy, DefaultColormap(dpy, screen),
                         def[WM_TRANS_COLOR]->content,
                         &color_xor, &color_xor);
        XSetForeground(dpy, gc_xor, color_xor.pixel);
    } else if(f == def[WM_FONT]) {
        XFreeFont(dpy, xfont);
        xfont = blitz_getxfont(dpy, def[WM_FONT]->content);
    }
    check_event(0);
}

static void
init_atoms()
{
    wm_state = XInternAtom(dpy, "WM_STATE", False);
    wm_change_state = XInternAtom(dpy, "WM_CHANGE_STATE", False);
    wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    motif_wm_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
    net_atoms[NET_NUMBER_OF_DESKTOPS] = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
    net_atoms[NET_CURRENT_DESKTOP] = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    net_atoms[NET_WM_DESKTOP] = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
    XChangeProperty(dpy, root, XInternAtom(dpy, "_NET_SUPPORTED", False), XA_ATOM, 32, PropModeReplace, (unsigned char *) net_atoms, NET_ATOM_COUNT);
}

static void
init_cursors()
{
    normal_cursor = XCreateFontCursor(dpy, XC_left_ptr);
    resize_cursor = XCreateFontCursor(dpy, XC_sizing);
    move_cursor = XCreateFontCursor(dpy, XC_fleur);
    drag_cursor = XCreateFontCursor(dpy, XC_cross);
    w_cursor = XCreateFontCursor(dpy, XC_left_side);
    e_cursor = XCreateFontCursor(dpy, XC_right_side);
    n_cursor = XCreateFontCursor(dpy, XC_top_side);
    s_cursor = XCreateFontCursor(dpy, XC_bottom_side);
    nw_cursor = XCreateFontCursor(dpy, XC_top_left_corner);
    ne_cursor = XCreateFontCursor(dpy, XC_top_right_corner);
    sw_cursor = XCreateFontCursor(dpy, XC_bottom_left_corner);
    se_cursor = XCreateFontCursor(dpy, XC_bottom_right_corner);
}

static void
init_default()
{
    def[WM_TRANS_COLOR] =
        wmii_create_ixpfile(ixps, "/default/transcolor",
                            BLITZ_SEL_FG_COLOR);
    def[WM_TRANS_COLOR]->after_write = handle_after_write;
    def[WM_COLUMN_GEOMETRY] = wmii_create_ixpfile(ixps, "/default/geometry", BLITZ_SEL_FG_COLOR);
    def[WM_SEL_BG_COLOR] =
        wmii_create_ixpfile(ixps, "/default/sstyle/bgcolor",
                            BLITZ_SEL_BG_COLOR);
    def[WM_SEL_FG_COLOR] =
        wmii_create_ixpfile(ixps, "/default/sstyle/fgcolor",
                            BLITZ_SEL_FG_COLOR);
    def[WM_SEL_BORDER_COLOR] =
        wmii_create_ixpfile(ixps, "/default/sstyle/bordercolor",
                            BLITZ_SEL_BORDER_COLOR);
    def[WM_NORM_BG_COLOR] =
        wmii_create_ixpfile(ixps, "/default/nstyle/bgcolor",
                            BLITZ_NORM_BG_COLOR);
    def[WM_NORM_FG_COLOR] =
        wmii_create_ixpfile(ixps, "/default/nstyle/fgcolor",
                            BLITZ_NORM_FG_COLOR);
    def[WM_NORM_BORDER_COLOR] = wmii_create_ixpfile(ixps, "/default/nstyle/bordercolor",
                            BLITZ_NORM_BORDER_COLOR);
    def[WM_FONT] = wmii_create_ixpfile(ixps, "/default/xfont", BLITZ_FONT);
    def[WM_FONT]->after_write = handle_after_write;
    def[WM_SNAP_VALUE] = wmii_create_ixpfile(ixps, "/default/snapvalue", "20"); /* 0..1000 */
    def[WM_BORDER] = wmii_create_ixpfile(ixps, "/default/border", "1");
    def[WM_TAB] = wmii_create_ixpfile(ixps, "/default/tab", "1");
    def[WM_HANDLE_INC] = wmii_create_ixpfile(ixps, "/default/handleinc", "1");
    def[WM_SEL_PAGE] = ixp_create(ixps, "/sel");
    def[WM_EVENT_PAGE_UPDATE] = ixp_create(ixps, "/event/pageupdate");
    def[WM_EVENT_CLIENT_UPDATE] = ixp_create(ixps, "/event/clientupdate");
    def[WM_EVENT_B1PRESS] = ixp_create(ixps, "/default/event/b1press");
    def[WM_EVENT_B2PRESS] = ixp_create(ixps, "/default/event/b2press");
    def[WM_EVENT_B3PRESS] = ixp_create(ixps, "/default/event/b3press");
    def[WM_EVENT_B4PRESS] = ixp_create(ixps, "/default/event/b4press");
    def[WM_EVENT_B5PRESS] = ixp_create(ixps, "/default/event/b5press");
    def[WM_DETACHED_PREFIX] = ixp_create(ixps, "/detached");
}

static void
init_screen()
{
    XGCValues gcv;
    XSetWindowAttributes wa;

    XAllocNamedColor(dpy, DefaultColormap(dpy, screen),
                     def[WM_TRANS_COLOR]->content, &color_xor, &color_xor);
    gcv.subwindow_mode = IncludeInferiors;
    gcv.function = GXxor;
    gcv.foreground = color_xor.pixel;
    gcv.line_width = 4;
    gcv.plane_mask = AllPlanes;
    gcv.graphics_exposures = False;
    gc_xor = XCreateGC(dpy, root, GCForeground | GCGraphicsExposures
                      | GCFunction | GCSubwindowMode | GCLineWidth
                      | GCPlaneMask, &gcv);
    rect.x = rect.y = 0;
    rect.width = DisplayWidth(dpy, screen);
    rect.height = DisplayHeight(dpy, screen);

    wa.override_redirect = 1;
    wa.background_pixmap = ParentRelative;
    wa.event_mask = ExposureMask | ButtonPressMask;
    transient =
        XCreateWindow(dpy, root, 0, 0, rect.width, rect.height, 0,
                      DefaultDepth(dpy, screen), CopyFromParent,
                      DefaultVisual(dpy, screen),
                      CWOverrideRedirect | CWBackPixmap | CWEventMask,
                      &wa);

    XSync(dpy, False);
    gc_transient = XCreateGC(dpy, transient, 0, 0);
    XDefineCursor(dpy, transient, normal_cursor);
    XDefineCursor(dpy, root, normal_cursor);
}

/*
 * There's no way to check accesses to destroyed windows, thus
 * those cases are ignored (especially on UnmapNotify's).
 * Other types of errors call Xlib's default error handler, which
 * calls exit().
 */
static int
wmii_error_handler(Display * dpy, XErrorEvent * error)
{
    if(error->error_code == BadWindow
       || (error->request_code == X_SetInputFocus
           && error->error_code == BadMatch)
       || (error->request_code == X_PolyText8
           && error->error_code == BadDrawable)
       || (error->request_code == X_PolyFillRectangle
           && error->error_code == BadDrawable)
       || (error->request_code == X_PolySegment
           && error->error_code == BadDrawable)
       || (error->request_code == X_ConfigureWindow
           && error->error_code == BadMatch))
        return 0;
    fprintf(stderr, "%s", "wmiiwm: fatal error");
    return x_error_handler(dpy, error); /* calls exit() */
}

/*
 * Startup Error handler to check if another window manager
 * is already running.
 */
static int
startup_error_handler(Display * dpy, XErrorEvent * error)
{
    other_wm_running = 1;
    return -1;
}

static void
cleanup()
{
	size_t i;
	Client *c;
	for(i = 0; client && client[i]; i++) {
		c = client[i];
		reparent_client(c, root, c->frame.rect.x + c->rect.x, c->frame.rect.y + c->rect.y);
	}
    XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XSync(dpy, False);
}

int
main(int argc, char *argv[])
{
    int i;
    int checkwm = 0;
    char *sockfile = 0;

    /* command line args */
    if(argc > 1) {
        for(i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
            switch (argv[i][1]) {
            case 'v':
                fprintf(stdout, "%s", version[0]);
                exit(0);
                break;
            case 'c':
                checkwm = 1;
                break;
            case 's':
                if(i + 1 < argc)
                    sockfile = argv[++i];
                else
                    usage();
                break;
            default:
                usage();
                break;
            }
        }
    }
    dpy = XOpenDisplay(0);
    if(!dpy) {
        fprintf(stderr, "%s", "wmiiwm: cannot open display\n");
        exit(1);
    }
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);

    /* check if another WM is already running */
    other_wm_running = 0;
    XSetErrorHandler(startup_error_handler);
    /* this causes an error if some other WM is running */
    XSelectInput(dpy, root, ROOT_MASK);
    XSync(dpy, False);

    if(other_wm_running) {
        fprintf(stderr,
                "wmiiwm: another window manager is already running\n");
        exit(1);
    }
    if(checkwm) {
        XCloseDisplay(dpy);
        exit(0);
    }
    XSetErrorHandler(0);
    x_error_handler = XSetErrorHandler(wmii_error_handler);

    ixps = wmii_setup_server(sockfile);

    init_event_hander();

    if(!(def[WM_CTL] = ixp_create(ixps, "/ctl"))) {
        perror("wmiiwm: cannot connect IXP server");
        exit(1);
    }
    def[WM_CTL]->after_write = handle_after_write;

	aqueuesz = detachedsz = pagesz = clientsz = sel_page = 0;
    page = nil;
	client = detached = nil;
	aqueue = nil;

    init_atoms();
    init_cursors();
    init_default();
    xfont = blitz_getxfont(dpy, def[WM_FONT]->content);
    wmii_init_lock_modifiers(dpy, &valid_mask, &num_lock_mask);
    init_screen();
    scan_wins();

    /* main event loop */
    run_server_with_fd_support(ixps, ConnectionNumber(dpy), check_event,
                               0);
    cleanup();
/*	deinit_server(ixps);*/
    XCloseDisplay(dpy);

    return 0;
}
