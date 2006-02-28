/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/cursorfont.h>
#include <X11/Xproto.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>

#include "wm.h"

static XRectangle initial_rect;
static int other_wm_running;
static int (*x_error_handler) (Display *, XErrorEvent *);

static char version[] = "wmiiwm - " VERSION ", (C)opyright MMIV-MMVI Anselm R. Garbe\n";

static void
usage()
{
    fprintf(stderr, "%s", "usage: wmiiwm -a <address> [-c] [-v]\n");
    exit(1);
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
	if(c == sel_client_of_page(c->area->page))
    	d->color = def.sel;
    else
    	d->color = def.norm;
    d->data = c->name;
    scale_rect(&rect, &initial_rect, &c->frame.rect, &d->rect);
    blitz_drawlabel(dpy, d);
    XSync(dpy, False);      /* do not clear upwards */
}

static void
draw_pager_page(size_t idx, Draw *d)
{
	int i, j;
    char name[4];
    initial_rect = d->rect;

    if(idx == sel)
        d->color = def.sel;
    else
        d->color = def.norm;
    snprintf(name, sizeof(name), "%d", idx + 1);
    d->data = name;
    blitz_drawlabel(dpy, d);
    XSync(dpy, False);

	for(i = 0; i < page[idx]->narea; i++) {
		Area *a = page[idx]->area[i];
		if(!a->nclient)
			continue;
		for(j = 0; j < a->nclient; j++)
			draw_pager_client(a->client[j], d);
	}
}

static void
draw_pager()
{
    Draw d = { 0 };
    unsigned int i, ic, ir, tw, th, rows, cols;
    int dx;

	for(i = 0; (i < pagesz) && page[i]; i++);
    blitz_getbasegeometry(i, &cols, &rows);
    dx = (cols - 1) * DEF_PAGER_GAP;      /* DEF_PAGER_GAPpx space */
    tw = (rect.width - dx) / cols;
    th = ((double) tw / rect.width) * rect.height;
    d.drawable = transient;
    d.gc = gc_transient;
    d.font = xfont;
	d.align = CENTER;
	i = 0;
    for(ir = 0; ir < rows; ir++) {
        for(ic = 0; ic < cols; ic++) {
            d.rect.x = ic * tw + (ic * DEF_PAGER_GAP);
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
    dx = (cols - 1) * DEF_PAGER_GAP;      /* DEF_PAGER_GAPpx space */
    tw = (rect.width - dx) / cols;
    th = ((double) tw / rect.width) * rect.height;

	i = 0;
    for(ir = 0; ir < rows; ir++) {
        for(ic = 0; ic < cols; ic++) {
            r.x = ic * tw + (ic * DEF_PAGER_GAP);
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

void
pager()
{
    XEvent ev;
    int i;
	Client *c;

    if(!npage)
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
                if(i < npage) {
					focus_page(page[i]);
					if((c = sel_client_of_page(page[i])))
						focus_client(c);
				}
            XUngrabKeyboard(dpy, CurrentTime);
            XUngrabPointer(dpy, CurrentTime /* ev.xbutton.time */ );
			XSync(dpy, False);
            return;
            break;
        case ButtonPress:
            XUnmapWindow(dpy, transient);
            if(ev.xbutton.button == Button1) {
				i = xy_to_pager_page(ev.xbutton.x, ev.xbutton.y);
                focus_page(page[i]);
				if((c = sel_client_of_page(page[i])))
					focus_client(c);
			}
            XUngrabKeyboard(dpy, CurrentTime);
            XUngrabPointer(dpy, CurrentTime /* ev.xbutton.time */ );
			XSync(dpy, False);
            return;
            break;
        }
    }
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
            }
        }
    }
    if(wins)
        XFree(wins);
}

static int
win_property(Window w, Atom a, Atom t, long l, unsigned char **prop)
{
    Atom real;
    int format;
    unsigned long res, extra;
    int status;

    status =
        XGetWindowProperty(dpy, w, a, 0L, l, False, t, &real, &format,
                           &res, &extra, prop);

    if(status != Success || *prop == 0) {
        return 0;
    }
    if(res == 0) {
        free((void *) *prop);
    }
    return res;
}

int
win_proto(Window w)
{
    Atom *protocols;
    long res;
    int protos = 0;
    int i;

    res = win_property(w, wm_protocols, XA_ATOM, 20L,
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
    if(win_property(w, wm_state, wm_state, 2L,
					((unsigned char **) &prop)) > 0) {
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
init_screen()
{
    XGCValues gcv;
    XSetWindowAttributes wa;

    gcv.subwindow_mode = IncludeInferiors;
    gcv.function = GXxor;
    gcv.foreground = def.sel.bg;
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
		reparent_client(c, root, c->frame.rect.x, c->frame.rect.y);
	}
    XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XSync(dpy, False);
}


int
main(int argc, char *argv[])
{
    int i;
    int checkwm = 0;
    char *address = nil, *errstr;
    XSetWindowAttributes wa;

    /* command line args */
    if(argc > 1) {
        for(i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
            switch (argv[i][1]) {
            case 'v':
                fprintf(stdout, "%s", version);
                exit(0);
                break;
            case 'c':
                checkwm = 1;
                break;
            case 'a':
                if(i + 1 < argc)
                    address = argv[++i];
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
	/* above -c is checked */
    if(!address)
		usage();

    XSetErrorHandler(0);
    x_error_handler = XSetErrorHandler(wmii_error_handler);
	errstr = nil;
	i = ixp_create_sock(address, &errstr);
	if(i < 0) {
        fprintf(stderr, "wmii: fatal: %s\n", errstr);
		exit(1);
	}

	/* IXP server */
	ixp_server_open_conn(&srv, i, new_ixp_conn, ixp_server_close_conn);
    root_qid.type = IXP_QTDIR;
    root_qid.version = 0;
    root_qid.path = mkqpath(Droot, 0, 0, 0);

	/* X server */
	ixp_server_open_conn(&srv, ConnectionNumber(dpy), check_x_event, nil);
    init_x_event_handler();

	npage = nclient = pagesz = clientsz = sel = 0;
    page = nil;
	client = nil;

	key = nil;
	keysz = nkey = 0;
	label = nil;
	nlabel = labelsz = iexpand = 0;

	def.font = strdup(BLITZ_FONT);
	def.border = DEF_BORDER;
	def.snap = DEF_SNAP;
	def.inc = True;
	cext_strlcpy(def.selcolor, BLITZ_SELCOLORS, sizeof(def.selcolor));
	blitz_loadcolor(dpy, screen, def.selcolor, &def.sel);
	cext_strlcpy(def.normcolor, BLITZ_NORMCOLORS, sizeof(def.normcolor));
	blitz_loadcolor(dpy, screen, def.normcolor, &def.norm);

    init_atoms();
    init_cursors();
    xfont = blitz_getfont(dpy, def.font);
    init_lock_modifiers();
    init_screen();
    scan_wins();

    wa.override_redirect = 1;
    wa.background_pixmap = ParentRelative;
    wa.event_mask = ExposureMask | ButtonPressMask
					| SubstructureRedirectMask | SubstructureNotifyMask;
    brect = rect;
    brect.height = xfont->ascent + xfont->descent + 4;
    brect.y = rect.height - brect.height;
    winbar = XCreateWindow(dpy, RootWindow(dpy, screen), brect.x, brect.y,
                        brect.width, brect.height, 0, DefaultDepth(dpy, screen),
                        CopyFromParent, DefaultVisual(dpy, screen),
                        CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);

    XDefineCursor(dpy, winbar, XCreateFontCursor(dpy, XC_left_ptr));
    XSync(dpy, False);

    gcbar = XCreateGC(dpy, winbar, 0, 0);

    pmapbar = XCreatePixmap(dpy, winbar, brect.width, brect.height,
                      	 	DefaultDepth(dpy, screen));
	XMapRaised(dpy, winbar);
	draw_bar();

    /* main event loop */
	errstr = ixp_server_loop(&srv);
	if(errstr)
    	fprintf(stderr, "wmii: fatal: %s\n", errstr);

	ixp_server_close(&srv);
    cleanup();
    XCloseDisplay(dpy);

    return errstr ? 1 : 0;
}
