/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * (C)opyright MMVI-MMVII Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
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
#include "wmii.h"

static Bool check_other_wm;
static int (*x_error_handler) (Display *, XErrorEvent *);
static char version[] = "wmiiwm - " VERSION ", (C)opyright MMVI-MMVII Kris Maglione\n";
static struct sigaction sa;

static void
usage() {
	fatal("usage: wmiiwm -a <address> [-r <wmiirc>] [-v]\n");
}

static void
scan_wins() {
	int i;
	uint num;
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
		for(i = 0; i < num; i++) {
			if(!XGetWindowAttributes(blz.dpy, wins[i], &wa))
				continue;
			if(XGetTransientForHint(blz.dpy, wins[i], &d1)
			&& wa.map_state == IsViewable)
				manage_client(create_client(wins[i], &wa));
		}
	}
	if(wins)
		XFree(wins);
}

static int
win_property(Window w, Atom a, Atom t, long l, uchar **prop) {
	Atom real;
	int format;
	ulong res, extra;
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
			((uchar **) &protocols));
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
			PropModeReplace, (uchar *) net_atom, NetLast);
}

static void
init_cursors() {
	Pixmap pix;
	XColor black, dummy;

	XAllocNamedColor(blz.dpy, DefaultColormap(blz.dpy, blz.screen), "black", &black, &dummy);
	pix = XCreateBitmapFromData(blz.dpy, blz.root, (char[]){0}, 1, 1);

	cursor[CurNormal] = XCreateFontCursor(blz.dpy, XC_left_ptr);
	cursor[CurNECorner] = XCreateFontCursor(blz.dpy, XC_top_right_corner);
	cursor[CurNWCorner] = XCreateFontCursor(blz.dpy, XC_top_left_corner);
	cursor[CurSECorner] = XCreateFontCursor(blz.dpy, XC_bottom_right_corner);
	cursor[CurSWCorner] = XCreateFontCursor(blz.dpy, XC_bottom_left_corner);
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
	gcv.foreground = def.normcolor.bg;
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
		fatal("wmiiwm: another window manager is already running\n");
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
	XSync(blz.dpy, False);
}

static void
cleanup_handler(int signal) {
	sa.sa_handler = SIG_DFL;
	sigaction(signal, &sa, nil);
	switch(signal) {
	case SIGINT:
		srv.running = False;
		break;
	default:
		cleanup();
		XCloseDisplay(blz.dpy);
		raise(signal);
		break;
	}
}

static void
init_traps() {
	char buf[1];
	int fd[2];

	if(pipe(fd) != 0)
		fatal("Can't pipe(): %s\n", strerror(errno));

	switch(fork()) {
	case -1:
		fatal("Can't fork(): %s\n", strerror(errno));
	default:
		close(fd[0]);
		sa.sa_flags = 0;
		sa.sa_handler = cleanup_handler;
		sigaction(SIGINT, &sa, nil);
		sigaction(SIGTERM, &sa, nil);
		sigaction(SIGQUIT, &sa, nil);
		sigaction(SIGHUP, &sa, nil);
		break;
	case 0:
		close(fd[1]);
		close(ConnectionNumber(blz.dpy));
		setsid();
		blz.dpy = XOpenDisplay(0);
		if(!blz.dpy)
			fatal("wmiiwm: cannot open display\n");
		read(fd[0], buf, 1);
		XSetInputFocus(blz.dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
		XCloseDisplay(blz.dpy);
		exit(0);
	}
}

void
check_9pcon(IXPConn *c) {
	serve_9pcon(c);
	check_x_event(nil);
}

int
main(int argc, char *argv[]) {
	char *wmiirc, *errstr, *tmp;
	char address[1024], ns_path[1024];
	struct passwd *passwd;
	WMScreen *s;
	int sock, i;
	pid_t pid;
	XSetWindowAttributes wa;

	passwd = getpwuid(getuid());
	user = estrdup(passwd->pw_name);
	wmiirc = "wmiistartrc";

	address[0] = '\0';
	if((tmp = getenv("WMII_ADDRESS")) && strlen(tmp) > 0)
		strncpy(address, tmp, sizeof(address));

	/* command line args */
	for(i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
		switch (argv[i][1]) {
		case 'v':
			fprintf(stdout, "%s", version);
			exit(0);
			break;
		case 'V':
			verbose = True;
			break;
		case 'a':
			if(i + 1 < argc)
				strncpy(address, argv[++i], sizeof(address));
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

	if(strncmp(address, "unix!", 5) == 0) {
		tmp = &address[5];
		i = strrchr(tmp, '/') - tmp;
		if(i < 0)
			fatal("wmiiwm: Bad address\n");
		strncpy(ns_path, tmp, min(sizeof(ns_path), i));
	}else if((tmp = getenv("WMII_NS_PATH")) && strlen(tmp) > 0)
		strncpy(ns_path, tmp, sizeof(ns_path));
	else
		snprintf(ns_path, sizeof(ns_path), "/tmp/ns.%s.%s", user, getenv("DISPLAY"));

	if(strlen(address) == 0)
		snprintf(address, sizeof(address), "unix!%s/wmii", ns_path);

	setenv("WMII_NS_DIR", ns_path, True);
	setenv("WMII_ADDRESS", address, True);

	setlocale(LC_CTYPE, "");
	starting = True;

	blz.dpy = XOpenDisplay(0);
	if(!blz.dpy)
		fatal("wmiiwm: cannot open dpy\n");
	blz.screen = DefaultScreen(blz.dpy);
	blz.root = RootWindow(blz.dpy, blz.screen);

	check_other_wm = True;
	x_error_handler = XSetErrorHandler(wmii_error_handler);
	XSelectInput(blz.dpy, blz.root, SubstructureRedirectMask | EnterWindowMask);
	XSync(blz.dpy, False);
	check_other_wm = False;

	init_traps();

	/* Make sure that the namespace directory exists */
	switch(pid = fork()) {
	case -1:
		fatal("wmiiwm: Can't fork: %s\n", strerror(errno));
		break; /* Not reached */
	case 0:
		execlp("mkdir", "mkdir", "-m", "0700", "-p", ns_path, nil);
		fatal("wmiiwm: Can't exec mkdir: %s\n", strerror(errno));
		break; /* Not reached */
	default:
		if(waitpid(pid, &i, WUNTRACED) == -1)
			fprintf(stderr, "wmiiwm: warning: wait for mkdir returned -1: %s\n",
				strerror(errno));
		else if(WEXITSTATUS(i) != 0)
			fatal("wmiiwm: Can't create namespace dir \"%s\" (mkdir returned %d)\n",
				ns_path, i);
		break;
	}

	/* Check namespace permissions */
	if(!strncmp(address, "unix!", 5)) {
		struct stat st;

		if(stat(ns_path, &st))
			fatal("wmiiwm: can't stat ns_path directory \"%s\": %s\n",
					ns_path, strerror(errno));
		if(getuid() != st.st_uid)
			fatal("wmiiwm: ns_path directory \"%s\" exists, "
					"but is not owned by you",
				ns_path);
		if(st.st_mode & 077)
			fatal("wmiiwm: ns_path directory \"%s\" exists, "
					"but has group or world permissions",
				ns_path);
	}

	errstr = nil;
	sock = ixp_create_sock(address, &errstr);
	if(sock < 0)
		fatal("wmiiwm: fatal: %s (%s)\n", errstr, address);

	/* start wmiirc */
	if(wmiirc) {
		/* Double fork hack */
		switch(pid = fork()) {
		case -1:
			perror("wmiiwm: cannot fork wmiirc");
			break; /* Not reached */
		case 0:
			switch(fork()) {
			case -1:
				perror("wmiiwm: cannot fork wmiirc");
				break; /* Not reached */
			case 0:
				if(setsid() == -1)
					fatal("wmiiwm: can't setsid: %s\n", strerror(errno));
				close(sock);
				close(ConnectionNumber(blz.dpy));

				/* Run through the user's shell as a login shell */
				tmp = malloc(sizeof(char*) * (strlen(passwd->pw_shell) + 2));
				/* Can't overflow */
				sprintf(tmp, "-%s", passwd->pw_shell);
				execl(passwd->pw_shell, tmp, "-c", wmiirc, nil);

				fatal("wmiiwm: can't exec \"%s\": %s\n", wmiirc, strerror(errno));
				break; /* Not reached */
			default:
				exit(0);
				break; /* Not reached */
			}
		default:
			waitpid(pid, &i, 0);
			if(i != 0)
				exit(1); /* Error already printed */
			break;
		}
	}

	ixp_server_open_conn(&srv, sock, &p9srv, check_9pcon, nil);
	ixp_server_open_conn(&srv, ConnectionNumber(blz.dpy), nil, check_x_event, nil);

	view = nil;
	client = nil;
	key = nil;

	memset(&def, 0, sizeof(def));
	def.font.fontstr = estrdup(BLITZ_FONT);
	def.border = 1;
	def.colmode = Coldefault;

	def.mod = Mod1Mask;
	strncpy(def.grabmod, "Mod1", sizeof(def.grabmod));

	strncpy(def.focuscolor.colstr, BLITZ_FOCUSCOLORS, sizeof(def.focuscolor.colstr));
	strncpy(def.normcolor.colstr, BLITZ_NORMCOLORS, sizeof(def.normcolor.colstr));
	loadcolor(&blz, &def.focuscolor);
	loadcolor(&blz, &def.normcolor);

	init_atoms();
	init_cursors();
	loadfont(&blz, &def.font);
	init_lock_keys();

	num_screens = 1;
	screens = emallocz(num_screens * sizeof(*screens));
	for(sock = 0; sock < num_screens; sock++) {
		s = &screens[sock];
		s->lbar = nil;
		s->rbar = nil;
		s->sel = nil;
		init_screen(s);
		pmap = XCreatePixmap(
			/* display */	blz.dpy,
			/* drawable */	blz.root,
			/* width */	s->rect.width,
			/* height */	s->rect.height,
			/* depth */	DefaultDepth(blz.dpy, blz.screen)
			);
		wa.event_mask = 
			  SubstructureRedirectMask
			| EnterWindowMask
			| LeaveWindowMask
			| FocusChangeMask;
		wa.cursor = cursor[CurNormal];
		XChangeWindowAttributes(blz.dpy, blz.root, CWEventMask | CWCursor, &wa);
		wa.override_redirect = 1;
		wa.background_pixmap = ParentRelative;
		wa.event_mask =
			  ExposureMask
			| ButtonReleaseMask
			| FocusChangeMask
			| SubstructureRedirectMask
			| SubstructureNotifyMask;
		s->brect = s->rect;
		s->brect.height = labelh(&def.font);
		s->brect.y = s->rect.height - s->brect.height;
		s->barwin = XCreateWindow(
			/* display */	blz.dpy,
			/* parent */	RootWindow(blz.dpy, blz.screen),
			/* x */		s->brect.x,
			/* y */		s->brect.y,
			/* width */	s->brect.width,
			/* height */	s->brect.height,
			/*border_width*/0,
			/* depth */	DefaultDepth(blz.dpy, blz.screen),
			/* class */	CopyFromParent,
			/* visual */	DefaultVisual(blz.dpy, blz.screen),
			/* valuemask */	CWOverrideRedirect | CWBackPixmap | CWEventMask,
			/* attrubutes */&wa
			);
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
	screen->focus = nil;
	XSetInputFocus(blz.dpy, screen->barwin, RevertToParent, CurrentTime);

	scan_wins();
	starting = False;
	update_views();
	if(view)
		write_event("FocusTag %s\n", screen->sel->name);

	check_x_event(nil);
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
