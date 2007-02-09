/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "wmii.h"
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
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

Bool check_other_wm;
static int (*x_error_handler) (Display *, XErrorEvent *);
static char version[] = "wmiiwm - " VERSION ", (C)opyright MMIV-MMVI Anselm R. Garbe\n";

static void
usage() {
	ixp_eprint("usage: wmiiwm -a <address> [-r <wmiirc>] [-v]\n");
}

static void
sigchld_handler(int sig) {
	int ret;
	/* We only spawn one child */
	wait(&ret);
}

static void
scan_wins() {
	int i;
	unsigned int num;
	Window *wins;
	XWindowAttributes wa;
	Window d1, d2;

	if(XQueryTree(blz.dpy, blz.root, &d1, &d2, &wins, &num)) {
		for(i = 0; i < num; i++) {
			if(!XGetWindowAttributes(blz.dpy, wins[i], &wa))
				continue;
			if(wa.override_redirect || XGetTransientForHint(blz.dpy, wins[i], &d1))
				continue;
			if(wa.map_state == IsViewable)
				manage_client(create_client(wins[i], &wa));
		}
	}
	if(wins)
		XFree(wins);
}

static int
win_property(Window w, Atom a, Atom t, long l, unsigned char **prop) {
	Atom real;
	int format;
	unsigned long res, extra;
	int status;

	status = XGetWindowProperty(blz.dpy, w, a, 0L, l, False, t, &real, &format,
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
win_proto(Window w) {
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


static void
init_atoms() {
	wm_atom[WMState] = XInternAtom(blz.dpy, "WM_STATE", False);
	wm_atom[WMProtocols] = XInternAtom(blz.dpy, "WM_PROTOCOLS", False);
	wm_atom[WMDelete] = XInternAtom(blz.dpy, "WM_DELETE_WINDOW", False);
	net_atom[NetSupported] = XInternAtom(blz.dpy, "_NET_SUPPORTED", False);
	net_atom[NetWMName] = XInternAtom(blz.dpy, "_NET_WM_NAME", False);
	tags_atom = XInternAtom(blz.dpy, "_WIN_TAGS", False);
	XChangeProperty(blz.dpy, blz.root, net_atom[NetSupported], XA_ATOM, 32,
			PropModeReplace, (unsigned char *) net_atom, NetLast);
}

static void
init_cursors() {
	Pixmap pix;
	XColor black, dummy;

	XAllocNamedColor(blz.dpy, DefaultColormap(blz.dpy, blz.screen), "black", &black, &dummy);
	pix = XCreateBitmapFromData(blz.dpy, blz.root, (char[]){0}, 1, 1);

	cursor[CurNormal] = XCreateFontCursor(blz.dpy, XC_left_ptr);
	cursor[CurResize] = XCreateFontCursor(blz.dpy, XC_sizing);
	cursor[CurMove] = XCreateFontCursor(blz.dpy, XC_fleur);
	cursor[CurInput] = XCreateFontCursor(blz.dpy, XC_xterm);
	cursor[CurInvisible] = XCreatePixmapCursor(blz.dpy, pix, pix, &black, &black, 0, 0);

	XFreePixmap(blz.dpy, pix);
}

static void
init_screen(WMScreen *screen) {
	Window w;
	int ret;
	unsigned mask;
	XGCValues gcv;

	gcv.subwindow_mode = IncludeInferiors;
	gcv.function = GXxor;
	gcv.foreground = def.selcolor.bg;
	gcv.plane_mask = AllPlanes;
	gcv.graphics_exposures = False;
	xorgc = XCreateGC(blz.dpy, blz.root, GCForeground | GCGraphicsExposures |
						GCFunction | GCSubwindowMode | GCPlaneMask, &gcv);
	screen->rect.x = screen->rect.y = 0;
	screen->rect.width = DisplayWidth(blz.dpy, blz.screen);
	screen->rect.height = DisplayHeight(blz.dpy, blz.screen);
	def.snap = screen->rect.height / 63;
	sel_screen = XQueryPointer(blz.dpy, blz.root, &w, &w, &ret, &ret, &ret, &ret, &mask);
}

/*
 * There's no way to check accesses to destroyed windows, thus
 * those cases are ignored (especially on UnmapNotify's).
 * Other types of errors call Xlib's default error handler, which
 * calls exit().
 */
int
wmii_error_handler(Display *dpy, XErrorEvent *error) {
	if(check_other_wm)
		ixp_eprint("wmiiwm: another window manager is already running\n");
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
				&& error->error_code == BadAccess))
		return 0;
	fprintf(stderr, "wmiiwm: fatal error: Xrequest code=%d, Xerror code=%d\n",
			error->request_code, error->error_code);
	return x_error_handler(blz.dpy, error); /* calls exit() */
}

static void
cleanup() {
	Client *c;

	for(c=client; c; c=c->next)
		reparent_client(c, blz.root, c->sel->rect.x, c->sel->rect.y);
	XSetInputFocus(blz.dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XSync(blz.dpy, False);
}

int
main(int argc, char *argv[]) {
	int i;
	char *address = nil, *wmiirc = nil, *namespace, *errstr;
	WMScreen *s;
	struct passwd *passwd;
	XSetWindowAttributes wa;

	/* command line args */
	for(i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
		switch (argv[i][1]) {
		case 'v':
			fprintf(stdout, "%s", version);
			exit(0);
			break;
		case 'a':
			if(i + 1 < argc)
				address = argv[++i];
			else
				usage();
			break;
		case 'r':
			if(i + 1 < argc)
				wmiirc = argv[++i];
			else
				usage();
			break;
		default:
			usage();
			break;
		}
	}

	if(!address)
		usage();

	setlocale(LC_CTYPE, "");
	starting = True;

	blz.dpy = XOpenDisplay(0);
	if(!blz.dpy)
		ixp_eprint("wmiiwm: cannot open dpy\n");
	blz.screen = DefaultScreen(blz.dpy);
	blz.root = RootWindow(blz.dpy, blz.screen);

	check_other_wm = True;
	x_error_handler = XSetErrorHandler(wmii_error_handler);
	XSelectInput(blz.dpy, blz.root, SubstructureRedirectMask | EnterWindowMask);
	XSync(blz.dpy, False);
	check_other_wm = False;

	/* Check namespace permissions */
	if(!strncmp(address, "unix!", 5)) {
		struct stat st;
		namespace = ixp_estrdup(&address[5]);
		for(i = strlen(namespace) - 1; i >= 0; i--)
			if(namespace[i] == '/') break;
		namespace[i+1] = '\0';
		if(stat(namespace, &st))
			ixp_eprint("wmiiwm: can't stat namespace directory \"%s\": %s\n",
					namespace, strerror(errno));
		if(getuid() != st.st_uid)
			ixp_eprint("wmiiwm: namespace directory \"%s\" exists, "
				"but is not owned by you",
				namespace);
		if(st.st_mode & 077)
			ixp_eprint("wmiiwm: namespace directory \"%s\" exists, "
				"but has group or world permissions",
				namespace);
		free(namespace);
	}

	errstr = nil;
	i = ixp_create_sock(address, &errstr);
	if(i < 0)
		ixp_eprint("wmiiwm: fatal: %s\n", errstr);

	/* start wmiirc */
	if(wmiirc) {
		int name_len = strlen(wmiirc) + 6;
		char execstr[name_len];
		signal(SIGCHLD, sigchld_handler);
		switch(fork()) {
		case 0:
			if(setsid() == -1)
				ixp_eprint("wmiiwm: can't setsid: %s\n", strerror(errno));
			close(i);
			close(ConnectionNumber(blz.dpy));
			snprintf(execstr, name_len, "exec %s", wmiirc);
			execl("/bin/sh", "sh", "-c", execstr, nil);
			ixp_eprint("wmiiwm: can't exec \"%s\": %s\n", wmiirc, strerror(errno));
		case -1:
			perror("wmiiwm: cannot fork wmiirc");
		default:
			break;
		}
	}

	/* IXP server */
	ixp_server_open_conn(&srv, i, &p9srv, serve_9pcon, nil);
	/* X server */
	ixp_server_open_conn(&srv, ConnectionNumber(blz.dpy), nil, check_x_event, nil);

	view = nil;
	client = nil;
	key = nil;
	passwd = getpwuid(getuid());
	user = ixp_estrdup(passwd->pw_name);
	def.colrules.string = nil;
	def.colrules.size = 0;
	def.tagrules.string = nil;
	def.tagrules.size = 0;
	def.keys = nil;
	def.keyssz = 0;
	def.font.fontstr = ixp_estrdup(BLITZ_FONT);
	def.border = 1;
	def.colmode = Coldefault;
	strncpy(def.selcolor.colstr, BLITZ_SELCOLORS, sizeof(def.selcolor.colstr));
	loadcolor(&blz, &def.selcolor);
	strncpy(def.normcolor.colstr, BLITZ_NORMCOLORS, sizeof(def.normcolor.colstr));
	loadcolor(&blz, &def.normcolor);
	strncpy(def.grabmod, "Mod1", sizeof(def.grabmod));
	def.mod = Mod1Mask;
	init_atoms();
	init_cursors();
	loadfont(&blz, &def.font);
	init_lock_keys();
	num_screens = 1;
	screens = ixp_emallocz(num_screens * sizeof(*screens));
	for(i = 0; i < num_screens; i++) {
		s = &screens[i];
		s->lbar = nil;
		s->rbar = nil;
		s->sel = nil;
		init_screen(s);
		pmap = XCreatePixmap(blz.dpy, blz.root, s->rect.width, s->rect.height,
				DefaultDepth(blz.dpy, blz.screen));
		wa.event_mask = SubstructureRedirectMask | EnterWindowMask | LeaveWindowMask;
		wa.cursor = cursor[CurNormal];
		XChangeWindowAttributes(blz.dpy, blz.root, CWEventMask | CWCursor, &wa);
		wa.override_redirect = 1;
		wa.background_pixmap = ParentRelative;
		wa.event_mask = ExposureMask | ButtonReleaseMask
			| SubstructureRedirectMask | SubstructureNotifyMask;
		s->brect = s->rect;
		s->brect.height = labelh(&def.font);
		s->brect.y = s->rect.height - s->brect.height;
		s->barwin = XCreateWindow(blz.dpy, RootWindow(blz.dpy, blz.screen),
				s->brect.x, s->brect.y,
				s->brect.width, s->brect.height, 0,
				DefaultDepth(blz.dpy, blz.screen),
				CopyFromParent, DefaultVisual(blz.dpy, blz.screen),
				CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
		XSync(blz.dpy, False);
		s->bbrush.blitz = &blz;
		s->bbrush.gc = XCreateGC(blz.dpy, s->barwin, 0, 0);
		s->bbrush.drawable = pmap;
		s->bbrush.rect = s->brect;
		s->bbrush.rect.x = 0;
		s->bbrush.rect.y = 0;
		s->bbrush.color = def.normcolor;
		s->bbrush.font = &def.font;
		s->bbrush.border = 1;
		draw_bar(s);
		XMapRaised(blz.dpy, s->barwin);
	}

	screen = &screens[0];

	scan_wins();
	update_views();
	starting = False;

	/* main event loop */
	errstr = ixp_server_loop(&srv);
	if(errstr)
		fprintf(stderr, "wmiiwm: fatal: %s\n", errstr);

	cleanup();
	XCloseDisplay(blz.dpy);
	ixp_server_close(&srv);
	if(errstr)
		return 1;
	return 0;
}
