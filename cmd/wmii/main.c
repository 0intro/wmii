/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/cursorfont.h>
#include <errno.h>
#include <locale.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <util.h>
#include "dat.h"
#include "fns.h"

static const char
	version[] = "wmii-"VERSION", ©2007 Kris Maglione\n";

static int (*x_error_handler) (Display *, XErrorEvent *);
static char *address, *ns_path;
static Bool check_other_wm;
static struct sigaction sa;
static struct passwd *passwd;
static int sleeperfd, sock;

static void
usage() {
	fatal("usage: wmii [-a <address>] [-r <wmiirc>] [-v]\n");
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

int
win_proto(Window w) {
	Atom *protocols;
	Atom real;
	ulong nitems, extra;
	int i, format, status, protos;

	status = XGetWindowProperty(
		/* display */	blz.dpy,
		/* window */	w,
		/* property */	atom[WMProtocols],
		/* offset */	0L,
		/* length */	20L,
		/* delete */	False,
		/* req_type */	XA_ATOM,
		/* type_ret */	&real,
		/* format_ret */&format,
		/* nitems_ret */&nitems,
		/* extra_bytes */&extra,
		/* prop_return */(uchar**)&protocols
	);

	if(status != Success || protocols == 0) {
		return 0;
	}

	if(nitems == 0) {
		free(protocols);
		return 0;
	}

	protos = 0;
	for(i = 0; i < nitems; i++) {
		if(protocols[i] == atom[WMDelete])
			protos |= WM_PROTOCOL_DELWIN;
	}

	free(protocols);
	return protos;
}

static char*
ns_display() {
	char *s, *disp;

	disp = getenv("DISPLAY");
	if(disp == nil)
		fatal("DISPLAY is unset");

	disp = estrdup(disp);
	s = &disp[strlen(disp) - 2];
	if(strcmp(s, ".0") == 0)
		*s = '\0';

	s = emalloc(strlen(disp) + strlen(user) + strlen("/tmp/ns..") + 1);
	sprintf(s, "/tmp/ns.%s.%s", user, disp);

	free(disp);
	return s;
}

static void
rmkdir(char *path, int mode) {
	char *p, *q;
	int ret;
	char c;

	q = path + strlen(ns_path);
	for(p = &path[1]; p <= q; p++) {
		c = *p;
		if((c == '/') || (c == '\0')) {
			*p = '\0';
			ret = mkdir(path, mode);
			if((ret == -1) && (errno != EEXIST))
				fatal("Can't create ns_path '%s':", path);
			*p = c;
		}
	}
}

static void
init_ns() {
	struct stat st;
	char *s;

	if(address && strncmp(address, "unix!", 5) == 0) {
		ns_path = estrdup(&address[5]);
		s = strrchr(ns_path, '/');
		if(s != nil)
			s = '\0';
	}
	else if((s = getenv("NAMESPACE")))
		ns_path = s;
	else
		ns_path = ns_display();

	if((ns_path[0] != '/') || (strlen(ns_path) == 0))
		fatal("Bad ns_path");

	rmkdir(ns_path, 0700);

	if(stat(ns_path, &st))
		fatal("Can't stat ns_path '%s':", ns_path);
	if(getuid() != st.st_uid)
		fatal("ns_path '%s' exists but is not owned by you",
			ns_path);
	if(st.st_mode & 077)
		fatal("ns_path '%s' exists, but has group or world permissions",
			ns_path);
}

static void
init_environment() {
	init_ns();

	if(address == nil) {
		address = emalloc(strlen(ns_path) + strlen("unix!/wmii") + 1);
		sprintf(address, "unix!%s/wmii", ns_path);
	}

	setenv("WMII_NS_DIR", ns_path, True);
	setenv("WMII_ADDRESS", address, True);
}

static void
intern_atom(int ident, char *name) {
	atom[ident] = XInternAtom(blz.dpy, name, False);
}

static void
init_atoms() {
	intern_atom(WMState, "WM_STATE");
	intern_atom(WMProtocols, "WM_PROTOCOLS");
	intern_atom(WMDelete, "WM_DELETE_WINDOW");
	intern_atom(NetSupported, "_NET_SUPPORTED");
	intern_atom(NetWMName, "_NET_WM_NAME");
	intern_atom(TagsAtom, "_WIN_TAGS");

	XChangeProperty(blz.dpy, blz.root, atom[NetSupported], XA_ATOM, 32,
			PropModeReplace, (uchar *)&atom[NetSupported], 2);
}

static void
create_cursor(int ident, uint shape) {
	cursor[ident] = XCreateFontCursor(blz.dpy, shape);
}

static void
init_cursors() {
	Pixmap pix;
	XColor black, dummy;

	create_cursor(CurNormal, XC_left_ptr);
	create_cursor(CurNECorner, XC_top_right_corner);
	create_cursor(CurNWCorner, XC_top_left_corner);
	create_cursor(CurSECorner, XC_bottom_right_corner);
	create_cursor(CurSWCorner, XC_bottom_left_corner);
	create_cursor(CurMove, XC_fleur);
	create_cursor(CurInput, XC_xterm);

	XAllocNamedColor(blz.dpy, DefaultColormap(blz.dpy, blz.screen),
			"black", &black,
			&dummy);
	pix = XCreateBitmapFromData(blz.dpy, blz.root,
			(char[]){0}, 1, 1);

	cursor[CurInvisible] = XCreatePixmapCursor(blz.dpy,
			pix, pix,
			&black, &black,
			0, 0);

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
	xorgc = XCreateGC(blz.dpy, blz.root,
			  GCForeground
			| GCGraphicsExposures
			| GCFunction
			| GCSubwindowMode
			| GCPlaneMask,
			&gcv);

	screen->rect.x = screen->rect.y = 0;
	screen->rect.width = DisplayWidth(blz.dpy, blz.screen);
	screen->rect.height = DisplayHeight(blz.dpy, blz.screen);
	def.snap = screen->rect.height / 63;

	sel_screen = XQueryPointer(blz.dpy, blz.root,
			&w, &w,
			&ret, &ret, &ret, &ret,
			&mask);
}

struct {
	uchar rcode, ecode;
} itab[] = {
	{ 0, BadWindow },
	{ X_SetInputFocus, BadMatch },
	{ X_PolyText8, BadDrawable },
	{ X_PolyFillRectangle, BadDrawable },
	{ X_PolySegment, BadDrawable },
	{ X_ConfigureWindow, BadMatch },
	{ X_GrabKey, BadAccess },
};

/*
 * There's no way to check accesses to destroyed windows, thus
 * those cases are ignored (especially on UnmapNotify's).
 * Other types of errors call Xlib's default error handler, which
 * calls exit().
 */
int
wmii_error_handler(Display *dpy, XErrorEvent *error) {
	int i;

	if(check_other_wm)
		fatal("another window manager is already running");

	for(i = 0; i < nelem(itab); i++)
		if((itab[i].rcode == 0 || itab[i].rcode == error->request_code)
		&&(itab[i].ecode == 0 || itab[i].ecode == error->error_code))
			return 0;

	fprintf(stderr, "%s: fatal error: Xrequest code=%d, Xerror code=%d\n",
			argv0, error->request_code, error->error_code);
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
		fatal("Can't pipe():");

	switch(fork()) {
	case -1:
		fatal("Can't fork():");
		break; /* not reached */
	case 0:
		close(fd[1]);
		close(ConnectionNumber(blz.dpy));
		setsid();

		blz.dpy = XOpenDisplay(0);
		if(!blz.dpy)
			fatal("Can't open display");

		/* Wait for parent to exit */
		read(fd[0], buf, 1);

		XSetInputFocus(blz.dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
		XCloseDisplay(blz.dpy);
		exit(0);
	default:
		break;
	}

	close(fd[0]);
	sleeperfd = fd[1];

	sa.sa_flags = 0;
	sa.sa_handler = cleanup_handler;
	sigaction(SIGINT, &sa, nil);
	sigaction(SIGTERM, &sa, nil);
	sigaction(SIGQUIT, &sa, nil);
	sigaction(SIGHUP, &sa, nil);
}

static void
spawn_command(const char *cmd) {
	char *shell, *p;
	pid_t pid;
	int status;

	/* Double fork hack */
	switch(pid = fork()) {
	case -1:
		fatal("Can't fork:");
		break; /* Not reached */
	case 0:
		switch(fork()) {
		case -1:
			fatal("Can't fork:");
			break; /* Not reached */
		case 0:
			if(setsid() == -1)
				fatal("Can't setsid:");
			close(sock);
			close(ConnectionNumber(blz.dpy));

			shell = passwd->pw_shell;
			if(shell[0] != '/')
				fatal("Shell is not an absolute path: %s", shell);

			/* Run through the user's shell as a login shell */
			p = malloc((strlen(shell) + 2));
			sprintf(p, "-%s", strrchr(shell, '/') + 1);

			execl(shell, p, "-c", cmd, nil);
			fatal("Can't exec '%s':", cmd);
			break; /* Not reached */
		default:
			exit(0);
			break; /* Not reached */
		}
	default:
		waitpid(pid, &status, 0);
		/* if(status != 0)
			exit(1); */
		break;
	}
}

void
check_9pcon(IxpConn *c) {
	serve_9pcon(c);
	check_x_event(nil);
}

int
main(int argc, char *argv[]) {
	char *wmiirc;
	WMScreen *s;
	XSetWindowAttributes wa;
	int i;

	passwd = getpwuid(getuid());
	user = estrdup(passwd->pw_name);
	wmiirc = "wmiistartrc";

	/* command line args */
	ARGBEGIN{
	case 'v':
		printf("%s", version);
		exit(0);
	case 'V':
		verbose = True;
		break;
	case 'a':
		address = EARGF(usage());
		break;
	case 'r':
		wmiirc = EARGF(usage());
		break;
	default:
		usage();
		break;
	}ARGEND;

	setlocale(LC_CTYPE, "");
	starting = True;

	blz.dpy = XOpenDisplay(0);
	if(!blz.dpy)
		fatal("Can't open display");
	blz.screen = DefaultScreen(blz.dpy);
	blz.root = RootWindow(blz.dpy, blz.screen);

	check_other_wm = True;
	x_error_handler = XSetErrorHandler(wmii_error_handler);
	XSelectInput(blz.dpy, blz.root, SubstructureRedirectMask | EnterWindowMask);
	XSync(blz.dpy, False);
	check_other_wm = False;

	init_environment();
	init_traps();

	errstr = nil;
	sock = ixp_announce(address);
	if(sock < 0)
		fatal("Can't create socket '%s': %s", address, errstr);

	if(wmiirc)
		spawn_command(wmiirc);

	ixp_listen(&srv, sock, &p9srv, check_9pcon, nil);
	ixp_listen(&srv, ConnectionNumber(blz.dpy), nil, check_x_event, nil);

	view = nil;
	client = nil;
	key = nil;

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
	for(i = 0; i < num_screens; i++) {
		s = &screens[i];
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
	select_view("nil");
	update_views();
	write_event("FocusTag %s\n", screen->sel->name);

	check_x_event(nil);
	errstr = ixp_serverloop(&srv);
	if(errstr)
		fprintf(stderr, "%s: error: %s\n", argv0, errstr);

	cleanup();
	XCloseDisplay(blz.dpy);
	ixp_server_close(&srv);
	close(sleeperfd);

	if(execstr)
		execl("/bin/sh", "sh", "-c", execstr, nil);

	if(errstr)
		return 1;
	return 0;
}
