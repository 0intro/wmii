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

IXPServer srv = {0};

static int other_wm_running;
static int (*x_error_handler) (Display *, XErrorEvent *);
static char version[] = "wmiiwm - " VERSION ", (C)opyright MMIV-MMVI Anselm R. Garbe\n";

static void
usage()
{
	fprintf(stderr, "%s", "usage: wmiiwm -a <address> [-c] [-v]\n");
	exit(1);
}

void
scan_wins()
{
	int i;
	unsigned int num;
	Window *wins;
	XWindowAttributes wa;
	Window d1, d2;

	if(XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for(i = 0; i < num; i++) {
			if(!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if(wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if(wa.map_state == IsViewable)
				manage_client(create_client(wins[i], &wa));
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

	status = XGetWindowProperty(dpy, w, a, 0L, l, False, t, &real, &format,
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

	res = win_property(w, wm_atom[WMProtocols], XA_ATOM, 20L,
			((unsigned char **) &protocols));
	if(res <= 0) {
		return protos;
	}
	for(i = 0; i < res; i++) {
		if(protocols[i] == wm_atom[WMDelete])
			protos |= WM_PROTOCOL_DELWIN;
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
	if(win_property(w, wm_atom[WMState], wm_atom[WMState], 2L,
				((unsigned char **) &prop)) > 0)
	{
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
	wm_atom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wm_atom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wm_atom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	net_atom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	net_atom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);

	XChangeProperty(dpy, root, net_atom[NetSupported], XA_ATOM, 32,
			PropModeReplace, (unsigned char *) net_atom, NetLast);
}

static void
init_cursors()
{
	cursor[CurNormal] = XCreateFontCursor(dpy, XC_left_ptr);
	cursor[CurResize] = XCreateFontCursor(dpy, XC_sizing);
	cursor[CurMove] = XCreateFontCursor(dpy, XC_fleur);
}

static void
init_screen()
{
	Window w;
	int ret;
	unsigned mask;
	XGCValues gcv;

	gcv.subwindow_mode = IncludeInferiors;
	gcv.function = GXxor;
	gcv.foreground = def.selcolor.bg;
	gcv.plane_mask = AllPlanes;
	gcv.graphics_exposures = False;
	xorgc = XCreateGC(dpy, root, GCForeground | GCGraphicsExposures |
						GCFunction | GCSubwindowMode | GCPlaneMask, &gcv);

	rect.x = rect.y = 0;
	rect.width = DisplayWidth(dpy, screen);
	rect.height = DisplayHeight(dpy, screen);
	def.snap = rect.height / 63;

	sel_screen = XQueryPointer(dpy, root, &w, &w, &ret, &ret, &ret, &ret, &mask);
}

/*
 * There's no way to check accesses to destroyed windows, thus
 * those cases are ignored (especially on UnmapNotify's).
 * Other types of errors call Xlib's default error handler, which
 * calls exit().
 */
int
wmii_error_handler(Display *dpy, XErrorEvent *error)
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
				&& error->error_code == BadMatch)
			|| (error->request_code == X_GrabKey
				&& error->error_code == BadMatch))
		return 0;
	fprintf(stderr, "wmiiwm: fatal error: Xrequest code=%d, Xerror code=%d\n",
			error->request_code, error->error_code);
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
	Client *c;
	for(c=client; c; c=c->next)
		reparent_client(c, root, c->sel->rect.x, c->sel->rect.y);
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
		fputs("wmiiwm: cannot open display\n", stderr);
		exit(1);
	}
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);

	/* check if another WM is already running */
	other_wm_running = 0;
	XSetErrorHandler(startup_error_handler);
	/* this causes an error if some other WM is running */
	XSelectInput(dpy, root, SubstructureRedirectMask | EnterWindowMask);
	XSync(dpy, False);

	if(other_wm_running) {
		fputs("wmiiwm: another window manager is already running\n", stderr);
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
	ixp_server_open_conn(&srv, i, &p9srv, serve_9pcon, nil);

	/* X server */
	ixp_server_open_conn(&srv, ConnectionNumber(dpy), nil, check_x_event, nil);
	init_x_event_handler();
	blitz_x11_init(dpy);

	view = nil;
	client = nil;
	sel = nil;
	lbar = nil;
	key = nil;

	def.colrules.string = nil;
	def.colrules.size = 0;
	def.tagrules.string = nil;
	def.tagrules.size = 0;
	def.keys = nil;
	def.keyssz = 0;
	def.font.fontstr = strdup(BLITZ_FONT);
	def.border = 2;
	def.colmode = Coldefault;
	cext_strlcpy(def.selcolor.colstr, BLITZ_SELCOLORS, sizeof(def.selcolor.colstr));
	blitz_loadcolor(&def.selcolor);
	cext_strlcpy(def.normcolor.colstr, BLITZ_NORMCOLORS, sizeof(def.normcolor.colstr));
	blitz_loadcolor(&def.normcolor);
	cext_strlcpy(def.grabmod, "Mod1", sizeof(def.grabmod));
	def.mod = Mod1Mask;

	init_atoms();
	init_cursors();
	blitz_loadfont(&def.font);
	init_lock_keys();
	init_screen();

	wa.event_mask = SubstructureRedirectMask | EnterWindowMask | LeaveWindowMask;
	wa.cursor = cursor[CurNormal];
	XChangeWindowAttributes(dpy, root, CWEventMask | CWCursor, &wa);

	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask | ButtonReleaseMask
		| SubstructureRedirectMask | SubstructureNotifyMask;

	brect = rect;
	brect.height = height_of_bar();
	brect.y = rect.height - brect.height;
	barwin = XCreateWindow(dpy, RootWindow(dpy, screen), brect.x, brect.y,
			brect.width, brect.height, 0, DefaultDepth(dpy, screen),
			CopyFromParent, DefaultVisual(dpy, screen),
			CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
	XSync(dpy, False);

	bargc = XCreateGC(dpy, barwin, 0, 0);

	barpmap = XCreatePixmap(dpy, barwin, brect.width, brect.height,
			DefaultDepth(dpy, screen));
	bartile = blitz_create_tile(barpmap, bargc);
	bartile->rect = brect;
	bartile->color = def.normcolor;

	XMapRaised(dpy, barwin);
	draw_bar();
	scan_wins();

	/* main event loop */
	errstr = ixp_server_loop(&srv);
	if(errstr)
		fprintf(stderr, "wmii: fatal: %s\n", errstr);

	blitz_destroy_tile(bartile);
	ixp_server_close(&srv);
	cleanup();
	XCloseDisplay(dpy);

	return errstr ? 1 : 0;
}
