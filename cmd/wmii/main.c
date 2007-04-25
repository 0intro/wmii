/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
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

static int (*xlib_errorhandler) (Display*, XErrorEvent*);
static char *address, *ns_path;
static Bool check_other_wm;
static struct sigaction sa;
static struct passwd *passwd;
static int sleeperfd, sock, exitsignal;

static void
usage() {
	fatal("usage: wmii [-a <address>] [-r <wmiirc>] [-v]\n");
}

static void
scan_wins() {
	int i;
	uint num;
	XWindow *wins;
	XWindowAttributes wa;
	XWindow d1, d2;

	if(XQueryTree(display, scr.root.w, &d1, &d2, &wins, &num)) {
		for(i = 0; i < num; i++) {
			if(!XGetWindowAttributes(display, wins[i], &wa))
				continue;
			if(wa.override_redirect || XGetTransientForHint(display, wins[i], &d1))
				continue;
			if(wa.map_state == IsViewable)
				create_client(wins[i], &wa);
		}
		for(i = 0; i < num; i++) {
			if(!XGetWindowAttributes(display, wins[i], &wa))
				continue;
			if((XGetTransientForHint(display, wins[i], &d1))
			&& (wa.map_state == IsViewable))
				create_client(wins[i], &wa);
		}
	}
	if(wins)
		XFree(wins);
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
		fatal("ns_path '%s' exists but is not owned by you", ns_path);
	if(st.st_mode & 077)
		fatal("ns_path '%s' exists, but has group or world permissions", ns_path);
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
init_atoms() {
	Atom net[] = { xatom("_NET_SUPPORTED"), xatom("_NET_WM_NAME") };

	changeprop(&scr.root, "_NET_SUPPORTED", "ATOM", net, nelem(net));
}

static void
create_cursor(int ident, uint shape) {
	cursor[ident] = XCreateFontCursor(display, shape);
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
	create_cursor(CurDHArrow, XC_sb_h_double_arrow);
	create_cursor(CurInput, XC_xterm);
	create_cursor(CurSizing, XC_sizing);
	create_cursor(CurIcon, XC_icon);

	XAllocNamedColor(display, scr.colormap,
			"black", &black, &dummy);
	pix = XCreateBitmapFromData(
			display, scr.root.w,
			(char[]){0}, 1, 1);

	cursor[CurInvisible] = XCreatePixmapCursor(display,
			pix, pix,
			&black, &black,
			0, 0);

	XFreePixmap(display, pix);
}

static void
init_screen(WMScreen *screen) {
	XWindow w;
	int ret;
	unsigned mask;
	XGCValues gcv;

	gcv.subwindow_mode = IncludeInferiors;
	gcv.function = GXxor;
	gcv.foreground = def.normcolor.bg;
	gcv.plane_mask = AllPlanes;
	gcv.graphics_exposures = False;

	xor.type = WImage;
	xor.image = scr.root.w;
	xor.gc = XCreateGC(display, scr.root.w,
			  GCForeground
			| GCGraphicsExposures
			| GCFunction
			| GCSubwindowMode
			| GCPlaneMask,
			&gcv);

	screen->r = scr.rect;
	def.snap = Dy(scr.rect) / 63;

	sel_screen = XQueryPointer(display, scr.root.w,
			&w, &w,
			&ret, &ret, &ret, &ret,
			&mask);
}

static void
cleanup() {
	Point p;
	Client *c;

	for(c=client; c; c=c->next) {
		p = ZP;
		if(c->sel)
			p = c->sel->r.min;
		reparent_client(c, &scr.root, p);
		if(c->sel && c->sel->view != screen->sel)
			unmap_client(c, IconicState);
	}
	XSync(display, False);
	XCloseDisplay(display);
	ixp_server_close(&srv);
	close(sleeperfd);
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
static int
errorhandler(Display *dpy, XErrorEvent *error) {
	static Bool dead;
	int i;

	if(check_other_wm)
		fatal("another window manager is already running");

	for(i = 0; i < nelem(itab); i++)
		if((itab[i].rcode == 0 || itab[i].rcode == error->request_code)
		&& (itab[i].ecode == 0 || itab[i].ecode == error->error_code))
			return 0;

	fprintf(stderr, "%s: fatal error: Xrequest code=%d, Xerror code=%d\n",
			argv0, error->request_code, error->error_code);
	if(!dead++)
		cleanup();
	return xlib_errorhandler(display, error); /* calls exit() */
}

static void
cleanup_handler(int signal) {
	sa.sa_handler = SIG_DFL;
	sigaction(signal, &sa, nil);

	srv.running = False;

	switch(signal) {
	default:
		exitsignal = signal;
		break;
	case SIGINT:
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
		close(ConnectionNumber(display));
		setsid();

		display = XOpenDisplay(0);
		if(!display)
			fatal("Can't open display");

		/* Wait for parent to exit */
		read(fd[0], buf, 1);

		XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
		XCloseDisplay(display);
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
			close(ConnectionNumber(display));

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
	char *wmiirc, *str;
	WMScreen *s;
	WinAttr wa;
	int i;
	ulong col;

	wmiirc = "wmiistartrc";

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

	if(argc)
		usage();

	setlocale(LC_CTYPE, "");
	starting = True;

	initdisplay();

	xlib_errorhandler = XSetErrorHandler(errorhandler);

	check_other_wm = True;
	XSelectInput(display, scr.root.w,
			  SubstructureRedirectMask
			| EnterWindowMask);
	XSync(display, False);

	check_other_wm = False;

	passwd = getpwuid(getuid());
	user = estrdup(passwd->pw_name);

	init_environment();

	sock = ixp_announce(address);
	if(sock < 0)
		fatal("Can't create socket '%s': %s", address, errstr);

	if(wmiirc)
		spawn_command(wmiirc);

	init_traps();
	init_atoms();
	init_cursors();
	init_lock_keys();

	ixp_listen(&srv, sock, &p9srv, check_9pcon, nil);
	ixp_listen(&srv, ConnectionNumber(display), nil, check_x_event, nil);

	def.font = loadfont(FONT);
	def.border = 1;
	def.colmode = Coldefault;

	def.mod = Mod1Mask;
	strncpy(def.grabmod, "Mod1", sizeof(def.grabmod));

	loadcolor(&def.focuscolor, FOCUSCOLORS);
	loadcolor(&def.normcolor, NORMCOLORS);

	num_screens = 1;
	screens = emallocz(num_screens * sizeof(*screens));
	screen = &screens[0];
	for(i = 0; i < num_screens; i++) {
		s = &screens[i];
		init_screen(s);

		s->ibuf = allocimage(Dx(s->r), Dy(s->r), scr.depth);

		wa.event_mask = 
				  SubstructureRedirectMask
				| SubstructureNotifyMask
				| EnterWindowMask
				| LeaveWindowMask
				| FocusChangeMask;
		wa.cursor = cursor[CurNormal];
		setwinattr(&scr.root, &wa,
				  CWEventMask
				| CWCursor);
		initbar(s);
	}

	str = "This app is broken. Disable its transparency feature.";
	i = textwidth(def.font, str) + labelh(def.font);
	broken = allocimage(i, labelh(def.font), scr.depth);

	namedcolor("#ff0000", &col);
	fill(broken, broken->r, scr.black);
	drawstring(broken, def.font, broken->r, EAST, str, col);

	screen->focus = nil;
	setfocus(screen->barwin, RevertToParent);

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

	if(exitsignal)
		raise(exitsignal);
	fprintf(stderr, "execstr: %s\n", execstr);
	if(execstr)
		execl("/bin/sh", "sh", "-c", execstr, nil);
	if(errstr)
		return 1;
	return 0;
}
