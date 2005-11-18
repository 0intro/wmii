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

#include "wm.h"

static int      other_wm_running;
static int      (*x_error_handler) (Display *, XErrorEvent *);

char           *version[] = {
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

static void 
init_atoms()
{
	wm_state = XInternAtom(dpy, "WM_STATE", False);
	wm_change_state = XInternAtom(dpy, "WM_CHANGE_STATE", False);
	wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	motif_wm_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
	net_wm_desktop = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
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
init_defaults()
{
	defaults[WM_DETACHED_FRAME] = ixp_create(ixps, "/detached/frame");
	defaults[WM_DETACHED_CLIENT] = ixp_create(ixps, "/detached/client");
	defaults[WM_TRANS_COLOR] = wmii_create_ixpfile(ixps, "/default/transcolor", BLITZ_SEL_FG_COLOR);
	defaults[WM_TRANS_COLOR]->after_write = handle_after_write;
	defaults[WM_SEL_BG_COLOR] = wmii_create_ixpfile(ixps, "/default/selstyle/bgcolor", BLITZ_SEL_BG_COLOR);
	defaults[WM_SEL_FG_COLOR] = wmii_create_ixpfile(ixps, "/default/selstyle/fgcolor", BLITZ_SEL_FG_COLOR);
	defaults[WM_SEL_BORDER_COLOR] = wmii_create_ixpfile(ixps, "/default/selstyle/fgcolor", BLITZ_SEL_BORDER_COLOR);
	defaults[WM_NORM_BG_COLOR] = wmii_create_ixpfile(ixps, "/default/normstyle/bgcolor", BLITZ_NORM_BG_COLOR);
	defaults[WM_NORM_FG_COLOR] = wmii_create_ixpfile(ixps, "/default/normstyle/fgcolor", BLITZ_NORM_FG_COLOR);
	defaults[WM_NORM_BORDER_COLOR] = wmii_create_ixpfile(ixps, "/default/normstyle/fgcolor", BLITZ_NORM_BORDER_COLOR);
	defaults[WM_FONT] = wmii_create_ixpfile(ixps, "/default/font", BLITZ_FONT);
	defaults[WM_FONT]->after_write = handle_after_write;
	defaults[WM_PAGE_SIZE] = wmii_create_ixpfile(ixps, "/default/pagesize", "0,0,east,south-16");
	defaults[WM_SNAP_VALUE] = wmii_create_ixpfile(ixps, "/default/snapvalue", "20");	/* 0..1000 */
	defaults[WM_BORDER] = wmii_create_ixpfile(ixps, "/default/border", "1");
	defaults[WM_TAB] = wmii_create_ixpfile(ixps, "/default/tab", "1");
	defaults[WM_HANDLE_INC] = wmii_create_ixpfile(ixps, "/default/handleinc", "1");
	defaults[WM_LOCKED] = wmii_create_ixpfile(ixps, "/default/locked", "1");
	defaults[WM_LAYOUT] = wmii_create_ixpfile(ixps, "/default/layout", LAYOUT);
	defaults[WM_SEL_PAGE] = ixp_create(ixps, "/page/sel");
	defaults[WM_EVENT_PAGE_UPDATE] = ixp_create(ixps, "/default/event/pageupdate");
	defaults[WM_EVENT_CLIENT_UPDATE] = ixp_create(ixps, "/default/event/clientupdate");
	defaults[WM_EVENT_B1PRESS] = ixp_create(ixps, "/defaults/event/b1press");
	defaults[WM_EVENT_B2PRESS] = ixp_create(ixps, "/defaults/event/b2press");
	defaults[WM_EVENT_B3PRESS] = ixp_create(ixps, "/defaults/event/b3press");
	defaults[WM_EVENT_B4PRESS] = ixp_create(ixps, "/defaults/event/b4press");
	defaults[WM_EVENT_B5PRESS] = ixp_create(ixps, "/defaults/event/b5press");
}

static void 
init_screen()
{
	XGCValues       gcv;
	XSetWindowAttributes wa;

	XAllocNamedColor(dpy, DefaultColormap(dpy, screen_num),
			 defaults[WM_TRANS_COLOR]->content,
			 &xorcolor, &xorcolor);
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

	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask | ButtonPressMask | PointerMotionMask
		| SubstructureRedirectMask | SubstructureNotifyMask;
	transient = XCreateWindow(dpy, root, 0, 0, rect.width, rect.height,
				  0, DefaultDepth(dpy, screen_num),
				  CopyFromParent, DefaultVisual(dpy,
								screen_num),
				  CWOverrideRedirect | CWBackPixmap |
				  CWEventMask, &wa);

	XSync(dpy, False);
	transient_gc = XCreateGC(dpy, transient, 0, 0);
	XDefineCursor(dpy, transient, normal_cursor);
	XDefineCursor(dpy, root, normal_cursor);
	XSelectInput(dpy, root, ROOT_MASK);
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
	if (error->error_code == BadWindow
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
	return x_error_handler(dpy, error);	/* calls exit() */
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
	int             i;
	XWindowChanges  wc;

	for (i = 0; clients && clients[i]; i++) {
		Client         *c = clients[i];
		Frame          *f = c->frame;
		if (f) {
			gravitate(c, tab_height(f), border_width(f), 1);
			XReparentWindow(dpy, c->win, root,
					rect_of_frame(f)->x + c->rect.x,
					rect_of_frame(f)->y + c->rect.y);
			wc.border_width = c->border;
			XConfigureWindow(dpy, c->win, CWBorderWidth, &wc);
		}
	}
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
}

static void 
run()
{
	/* init */
	init_event_hander();

	if (!(defaults[WM_CTL] = ixp_create(ixps, "/ctl"))) {
		perror("wmiiwm: cannot connect IXP server");
		exit(1);
	}
	defaults[WM_CTL]->after_write = handle_after_write;

	clients = 0;
	frames = 0;
	detached = 0;
	pages = 0;
	layouts = 0;
	sel = 0;
	sel_client = 0;

	init_atoms();
	init_cursors();
	init_defaults();
	font = blitz_getfont(dpy, defaults[WM_FONT]->content);
	init_lock_modifiers(dpy, &valid_mask, &num_lock_mask);
	init_screen();
	init_layouts();
	scan_wins();

	/* main event loop */
	run_server_with_fd_support(ixps, ConnectionNumber(dpy), check_event,
				   0);
	cleanup();
	deinit_server(ixps);
	XCloseDisplay(dpy);
}

int 
main(int argc, char *argv[])
{
	int             i;
	int             checkwm = 0;
	char           *sockfile = 0;

	/* command line args */
	if (argc > 1) {
		for (i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
			switch (argv[i][1]) {
			case 'v':
				fprintf(stdout, "%s", version[0]);
				exit(0);
				break;
			case 'c':
				checkwm = 1;
				break;
			case 's':
				if (i + 1 < argc)
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
	if (!dpy) {
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
	if (other_wm_running) {
		fprintf(stderr,
		     "wmiiwm: another window manager is already running\n");
		exit(1);
	}
	if (checkwm) {
		XCloseDisplay(dpy);
		exit(0);
	}
	XSetErrorHandler(0);
	x_error_handler = XSetErrorHandler(wmii_error_handler);

	ixps = wmii_setup_server(sockfile);

	run();

	return 0;
}
