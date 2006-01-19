/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/Xproto.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>

#include "wm.h"

static XRectangle initial_rect;
static int other_wm_running;
static int (*x_error_handler) (Display *, XErrorEvent *);

static void new_page(void *obj, char *arg);
static void _select_page(void *obj, char *arg);
static void _destroy_page(void *obj, char *arg);
static void quit(void *obj, char *arg);
static void _attach_client(void *obj, char *arg);
static void _detach_client(void *obj, char *arg);
static void _close_client(void *obj, char *arg);
static void pager(void *obj, char *arg);
static void detached_clients(void *obj, char *arg);

/* action table for /ctl namespace */
Action wm_acttbl[] = {
    {"new", new_page},
    {"destroy", _destroy_page},
    {"select", _select_page},
    {"attach", _attach_client},
    {"detach", _detach_client},
    {"close", _close_client},
    {"quit", quit},
    {"pager", pager},
    {"detclients", detached_clients},
    {0, 0}
};

char *version[] = {
    "wmiiwm - window manager improved 2 - " VERSION "\n"
        " (C)opyright MMIV-MMV Anselm R. Garbe\n", 0
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
draw_pager_layout(Layout *l, Draw * d)
{
    Frame *f;
    Frame *sel = sel_frame();
    for(f = l->def->frames(l); f; f = f->next) {
        if(f == sel) {
            d->bg = blitz_loadcolor(dpy, screen_num, def[WM_SEL_BG_COLOR]->content);
            d->fg = blitz_loadcolor(dpy, screen_num, def[WM_SEL_FG_COLOR]->content);
            d->border = blitz_loadcolor(dpy, screen_num, def[WM_SEL_BORDER_COLOR]->content);
        } else {
            d->bg = blitz_loadcolor(dpy, screen_num, def[WM_NORM_BG_COLOR]->content);
            d->fg = blitz_loadcolor(dpy, screen_num, def[WM_NORM_FG_COLOR]->content);
            d->border = blitz_loadcolor(dpy, screen_num, def[WM_NORM_BORDER_COLOR]->content);
        }
        d->data = f->sel->name;
        scale_rect(&rect, &initial_rect, &f->rect, &d->rect);
        blitz_drawlabel(dpy, d);
        XSync(dpy, False);      /* do not clear upwards */
    }
}

static void
draw_pager_page(Page * p, Draw * d)
{
    unsigned int idx = 1;
    char name[4];
    initial_rect = d->rect;
    Page *page;

    if(p == selpage) {
        d->bg = blitz_loadcolor(dpy, screen_num, def[WM_SEL_BG_COLOR]->content);
        d->fg = blitz_loadcolor(dpy, screen_num, def[WM_SEL_FG_COLOR]->content);
        d->border = blitz_loadcolor(dpy, screen_num, def[WM_SEL_BORDER_COLOR]->content);
    } else {
        d->bg = blitz_loadcolor(dpy, screen_num, def[WM_NORM_BG_COLOR]->content);
        d->fg = blitz_loadcolor(dpy, screen_num, def[WM_NORM_FG_COLOR]->content);
        d->border = blitz_loadcolor(dpy, screen_num, def[WM_NORM_BORDER_COLOR]->content);
    }
    for(page = pages; page && page != p; page = page->next)
        idx++;
    snprintf(name, sizeof(name), "%d", idx);
    d->data = name;
    blitz_drawlabel(dpy, d);
    XSync(dpy, False);
    if(p->managed)
        draw_pager_layout(p->managed, d);
    draw_pager_layout(p->floating, d);
}

static void
draw_pager()
{
    Draw d = { 0 };
    unsigned int ic, ir, tw, th, rows, cols;
    int dx;
    Page *p;

    blitz_getbasegeometry(npages, &cols, &rows);
    dx = (cols - 1) * GAP;      /* GAPpx space */
    tw = (rect.width - dx) / cols;
    th = ((double) tw / rect.width) * rect.height;
    d.drawable = transient;
    d.gc = transient_gc;
    d.font = font;
    p = pages;
    for(ir = 0; ir < rows; ir++) {
        for(ic = 0; ic < cols; ic++) {
            d.rect.x = ic * tw + (ic * GAP);
            d.rect.width = tw;
            if(rows == 1)
                d.rect.y = 0;
            else
                d.rect.y = ir * (rect.height - th) / (rows - 1);
            d.rect.height = th;
            draw_pager_page(p, &d);
            if(!p->next)
                return;
            p = p->next;
        }
    }
}

static Page *
xy_to_pager_page(int x, int y)
{
    unsigned int ic, ir, tw, th, rows, cols;
    int dx;
    XRectangle r;
    Page *p;

    blitz_getbasegeometry(npages, &cols, &rows);
    dx = (cols - 1) * GAP;      /* GAPpx space */
    tw = (rect.width - dx) / cols;
    th = ((double) tw / rect.width) * rect.height;

    p = pages;
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
                return p;
            if(!p->next)
                return nil;
            p = p->next;
        }
    }
    return nil;
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

    if(!pages)
        return;

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
                if(i < npages)
					focus_page(pageat(i));
            XUngrabKeyboard(dpy, CurrentTime);
            XUngrabPointer(dpy, CurrentTime /* ev.xbutton.time */ );
            return;
            break;
        case ButtonPress:
            XUnmapWindow(dpy, transient);
            if(ev.xbutton.button == Button1)
                focus_page(xy_to_pager_page(ev.xbutton.x, ev.xbutton.y));
            XUngrabKeyboard(dpy, CurrentTime);
            XUngrabPointer(dpy, CurrentTime /* ev.xbutton.time */ );
            return;
            break;
        }
    }
}

static void
map_detached_clients()
{
    unsigned int ic, ir, tw, th, rows, cols;
    int dx, dy;
    Client *c = detached;
    XRectangle cr;

    blitz_getbasegeometry(ndetached, &cols, &rows);
	if(!cols)
		cols = 1;
	if(!rows)
		rows = 1;
    dx = (cols - 1) * GAP;      /* GAPpx space */
    dy = (rows - 1) * GAP;      /* GAPpx space */
    tw = (rect.width - dx) / cols;
    th = (rect.height - dy) / rows;

    for(ir = 0; ir < rows; ir++) {
        for(ic = 0; ic < cols; ic++) {
            if(!c)
                return;
            cr.x = ic * tw + (ic * GAP);
            cr.y = ir * th + (ir * GAP);
            cr.width = tw;
            cr.height = th;
            XMoveResizeWindow(dpy, c->win, cr.x, cr.y, cr.width, cr.height);
            configure_client(c);
            map_client(c);
			grab_client(c, AnyModifier, Button1);
            XRaiseWindow(dpy, c->win);
            XSync(dpy, False);
            c = c->next;
        }
    }
}

static void
detached_clients(void *obj, char *arg)
{
    XEvent ev;
    int n;
    Client *c = detached;

    if(!c)
        return;
    XClearWindow(dpy, transient);
    XMapRaised(dpy, transient);
    map_detached_clients();
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
            for(c = detached; c; c = c->next)
                unmap_client(c);
            if((n = handle_kpress(&ev.xkey)) != -1) {
                if((c = clientat(detached, n))) {
                    detach_detached(c);
                    attach_client(c);
                }
            }
            XUngrabKeyboard(dpy, CurrentTime);
            return;
            break;
        case ButtonPress:
            XUnmapWindow(dpy, transient);
            for(c = detached; c; c = c->next)
                unmap_client(c);
            if((ev.xbutton.button == Button1)
               && (c = win_to_client(ev.xbutton.window))) {
                detach_detached(c);
                attach_client(c);
            }
            XUngrabKeyboard(dpy, CurrentTime);
            return;
            break;
        }
    }
}

static void
_close_client(void *obj, char *arg)
{
    Client *c = sel_client();
    if(c)
        close_client(c);
}

static void
_attach_client(void *obj, char *arg)
{
    Client *c = detached;
    if(c) {
        detach_detached(c);
        attach_client(c);
    }
}

static void
_detach_client(void *obj, char *arg)
{
    Client *c = sel_client();
    if(c)
        detach_client(c, False);
}

static void
_select_page(void *obj, char *arg)
{
    Page *p = selpage;
    if(!p || !arg)
        return;
    if(!strncmp(arg, "prev", 5)) {
        if(p->prev)
            p = p->prev;
        else
            for(p = pages; p && p->next; p = p->next);
    } else if(!strncmp(arg, "next", 5)) {
        if(p->next)
            p = p->next;
        else
            p = pages;
    } else {
		int idx = blitz_strtonum(arg, 0, pageid);
		if(idx)
			idx--;
		else
			idx = 9;
        p = pageat(idx);
	}
    focus_page(p);
}

static void
_destroy_page(void *obj, char *arg)
{
    destroy_page(selpage);
    focus_page(selpage);
}

static void
new_page(void *obj, char *arg)
{
    focus_page(alloc_page());
}

Client *
win_to_client(Window w)
{
    Page *p;
    Client *c;
    for(c = detached; c; c = c->next)
        if(c->win == w)
            return c;
    for(p = pages; p; p = p->next) {
        Frame *f;
		if(p->managed->def)
			for(f = p->managed->def->frames(p->managed); f; f = f->next) {
				for(c = f->clients; c; c = c->next)
					if(c->win == w)
						return c;
			}
        for(f = p->floating->def->frames(p->floating); f; f = f->next) {
            for(c = f->clients; c; c = c->next)
                if(c->win == w)
                    return c;
        }
    }
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
                c = alloc_client(wins[i]);
                init_client(c, &wa);
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

static void
update_pages()
{
    Page *p;

    for(p = pages; p; p = p->next) {
        p->floating->def->arrange(p->floating);
        p->managed->def->arrange(p->managed);
    }
}

void
handle_before_read(IXPServer * s, File * f)
{
    char buf[64];
    if(f == def[WM_MANAGED_GEOMETRY]) {
        snprintf(buf, 64, "%d %d %d %d", layout_rect.x, layout_rect.y,
                 layout_rect.width, layout_rect.height);
        if(f->content)
            free(f->content);
        f->content = cext_estrdup(buf);
        f->size = strlen(buf);
    }
}

void
handle_after_write(IXPServer * s, File * f)
{
    if(f == def[WM_CTL])
        run_action(f, 0, wm_acttbl);
    else if(f == def[WM_TRANS_COLOR]) {
        unsigned long col[1];
        col[0] = xorcolor.pixel;
        XFreeColors(dpy, DefaultColormap(dpy, screen_num), col, 1, 0);
        XAllocNamedColor(dpy, DefaultColormap(dpy, screen_num),
                         def[WM_TRANS_COLOR]->content,
                         &xorcolor, &xorcolor);
        XSetForeground(dpy, xorgc, xorcolor.pixel);
    } else if(f == def[WM_FONT]) {
        XFreeFont(dpy, font);
        font = blitz_getfont(dpy, def[WM_FONT]->content);
    } else if(f == def[WM_MANAGED_GEOMETRY]) {
        char *geom = def[WM_MANAGED_GEOMETRY]->content;
        if(geom && strrchr(geom, ' ')) {
            layout_rect = rect;
            blitz_strtorect(&rect, &layout_rect, geom);
            update_pages();
        }
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
    def[WM_MANAGED_GEOMETRY] =
        wmii_create_ixpfile(ixps, "/default/geometry",
                            BLITZ_SEL_FG_COLOR);
    def[WM_MANAGED_GEOMETRY]->after_write = handle_after_write;
    def[WM_MANAGED_GEOMETRY]->before_read = handle_before_read;
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
    def[WM_NORM_BORDER_COLOR] =
        wmii_create_ixpfile(ixps, "/default/nstyle/bordercolor",
                            BLITZ_NORM_BORDER_COLOR);
    def[WM_FONT] = wmii_create_ixpfile(ixps, "/default/font", BLITZ_FONT);
    def[WM_FONT]->after_write = handle_after_write;
    def[WM_SNAP_VALUE] = wmii_create_ixpfile(ixps, "/default/snapvalue", "20"); /* 0..1000 */
    def[WM_BORDER] = wmii_create_ixpfile(ixps, "/default/border", "1");
    def[WM_TAB] = wmii_create_ixpfile(ixps, "/default/tab", "1");
    def[WM_HANDLE_INC] =
        wmii_create_ixpfile(ixps, "/default/handleinc", "1");
    def[WM_LAYOUT] = wmii_create_ixpfile(ixps, "/default/layout", LAYOUT);
    def[WM_SEL_PAGE] = ixp_create(ixps, "/sel");
    def[WM_EVENT_PAGE_UPDATE] = ixp_create(ixps, "/event/pageupdate");
    def[WM_EVENT_CLIENT_UPDATE] = ixp_create(ixps, "/event/clientupdate");
    def[WM_EVENT_B1PRESS] = ixp_create(ixps, "/default/event/b1press");
    def[WM_EVENT_B2PRESS] = ixp_create(ixps, "/default/event/b2press");
    def[WM_EVENT_B3PRESS] = ixp_create(ixps, "/default/event/b3press");
    def[WM_EVENT_B4PRESS] = ixp_create(ixps, "/default/event/b4press");
    def[WM_EVENT_B5PRESS] = ixp_create(ixps, "/default/event/b5press");
}

static void
init_screen()
{
    XGCValues gcv;
    XSetWindowAttributes wa;

    XAllocNamedColor(dpy, DefaultColormap(dpy, screen_num),
                     def[WM_TRANS_COLOR]->content, &xorcolor, &xorcolor);
    gcv.subwindow_mode = IncludeInferiors;
    gcv.function = GXxor;
    gcv.foreground = xorcolor.pixel;
    gcv.line_width = 4;
    gcv.plane_mask = AllPlanes;
    gcv.graphics_exposures = False;
    xorgc = XCreateGC(dpy, root, GCForeground | GCGraphicsExposures
                      | GCFunction | GCSubwindowMode | GCLineWidth
                      | GCPlaneMask, &gcv);
    rect.x = rect.y = 0;
    rect.width = DisplayWidth(dpy, screen_num);
    rect.height = DisplayHeight(dpy, screen_num);
    layout_rect = rect;

    wa.override_redirect = 1;
    wa.background_pixmap = ParentRelative;
    wa.event_mask = ExposureMask | ButtonPressMask;
    transient =
        XCreateWindow(dpy, root, 0, 0, rect.width, rect.height, 0,
                      DefaultDepth(dpy, screen_num), CopyFromParent,
                      DefaultVisual(dpy, screen_num),
                      CWOverrideRedirect | CWBackPixmap | CWEventMask,
                      &wa);

    XSync(dpy, False);
    transient_gc = XCreateGC(dpy, transient, 0, 0);
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
    Page *p;
    Client *c;
    for(p = pages; p; p = p->next) {
        Frame *f;
        for(f = p->managed->def->frames(p->managed); f; f = f->next) {
            while((c = f->clients))
                detach_client_from_frame(c, False);
        }
        for(f = p->floating->def->frames(p->floating); f; f = f->next) {
            while((c = f->clients))
                detach_client_from_frame(c, False);
        }
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
    screen_num = DefaultScreen(dpy);
    root = RootWindow(dpy, screen_num);

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

    pages = selpage = nil;
    ndetached = npages = 0;
	pageid = 1;
    detached = nil;
    layouts = nil;
	attachqueue = nil;

    init_atoms();
    init_cursors();
    init_default();
    font = blitz_getfont(dpy, def[WM_FONT]->content);
    wmii_init_lock_modifiers(dpy, &valid_mask, &num_lock_mask);
    init_screen();
    init_layouts();
    scan_wins();

    /* main event loop */
    run_server_with_fd_support(ixps, ConnectionNumber(dpy), check_event,
                               0);
    cleanup();
/*	deinit_server(ixps);*/
    XCloseDisplay(dpy);

    return 0;
}
