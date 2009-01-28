/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2008 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#define EXTERN
#include "dat.h"
#include <X11/Xproto.h>
#include <X11/cursorfont.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <pwd.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include "fns.h"

static const char
	version[] = "wmii-"VERSION", ©2008 Kris Maglione\n";

static char*	address;
static char*	ns_path;
static int	sleeperfd;
static int	sock;
static int	exitsignal;

static struct sigaction	sa;
static struct passwd*	passwd;

static void
usage(void) {
	fatal("usage: wmii [-a <address>] [-r <wmiirc>] [-v]\n");
}

static int
errfmt(Fmt *f) {
	return fmtstrcpy(f, ixp_errbuf());
}

static void
scan_wins(void) {
	int i;
	uint num;
	XWindow *wins;
	XWindowAttributes wa;
	XWindow d1, d2;

	if(XQueryTree(display, scr.root.w, &d1, &d2, &wins, &num)) {
		for(i = 0; i < num; i++) {
			if(!XGetWindowAttributes(display, wins[i], &wa))
				continue;
			/* Skip transients. */
			if(wa.override_redirect || XGetTransientForHint(display, wins[i], &d1))
				continue;
			if(wa.map_state == IsViewable)
				client_create(wins[i], &wa);
		}
		/* Manage transients. */
		for(i = 0; i < num; i++) {
			if(!XGetWindowAttributes(display, wins[i], &wa))
				continue;
			if((XGetTransientForHint(display, wins[i], &d1))
			&& (wa.map_state == IsViewable))
				client_create(wins[i], &wa);
		}
	}
	if(wins)
		XFree(wins);
}

static void
init_ns(void) {
	char *s;

	if(address && strncmp(address, "unix!", 5) == 0) {
		ns_path = estrdup(&address[5]);
		s = strrchr(ns_path, '/');
		if(s != nil)
			*s = '\0';
		if(ns_path[0] != '/' || ns_path[0] == '\0')
			fatal("address %q is not an absolute path", address);
		setenv("NAMESPACE", ns_path, true);
	}else
		ns_path = ixp_namespace();

	if(ns_path == nil)
		fatal("Bad namespace path: %r\n");
}

static void
init_environment(void) {
	init_ns();

	if(address)
		setenv("WMII_ADDRESS", address, true);
	else
		address = smprint("unix!%s/wmii", ns_path);
}

static void
create_cursor(int ident, uint shape) {
	cursor[ident] = XCreateFontCursor(display, shape);
}

static void
init_cursors(void) {
	static char zchar[1];
	Pixmap pix;
	XColor black, dummy;

	create_cursor(CurNormal, XC_left_ptr);
	create_cursor(CurNECorner, XC_top_right_corner);
	create_cursor(CurNWCorner, XC_top_left_corner);
	create_cursor(CurSECorner, XC_bottom_right_corner);
	create_cursor(CurSWCorner, XC_bottom_left_corner);
	create_cursor(CurMove, XC_fleur);
	create_cursor(CurDHArrow, XC_sb_h_double_arrow);
	create_cursor(CurDVArrow, XC_sb_v_double_arrow);
	create_cursor(CurInput, XC_xterm);
	create_cursor(CurSizing, XC_sizing);
	create_cursor(CurIcon, XC_icon);
	create_cursor(CurTCross, XC_tcross);

	XAllocNamedColor(display, scr.colormap,
			"black", &black, &dummy);
	pix = XCreateBitmapFromData(
			display, scr.root.w,
			zchar, 1, 1);

	cursor[CurNone] = XCreatePixmapCursor(display,
			pix, pix,
			&black, &black,
			0, 0);

	XFreePixmap(display, pix);
}

/*
 * There's no way to check accesses to destroyed windows, thus
 * those cases are ignored (especially on UnmapNotifies).
 * Other types of errors call Xlib's default error handler, which
 * calls exit().
 */
ErrorCode ignored_xerrors[] = {
	{ 0, BadWindow },
	{ X_SetInputFocus, BadMatch },
	{ X_PolyText8, BadDrawable },
	{ X_PolyFillRectangle, BadDrawable },
	{ X_PolySegment, BadDrawable },
	{ X_ConfigureWindow, BadMatch },
	{ X_GrabKey, BadAccess },
	{ X_GetAtomName, BadAtom },
};

void
init_screens(void) {
	Rectangle *rects;
	View *v;
	int i, n, m;

#ifdef notdef
	d.x = Dx(scr.rect) - Dx(screen->r);
	d.y = Dy(scr.rect) - Dy(screen->r);
	for(v=view; v; v=v->next) {
		v->r.max.x += d.x;
		v->r.max.y += d.y;
	}
#endif

	/* Reallocate screens, zero any new ones. */
	rects = xinerama_screens(&n);
	m = max(n, nscreens);
	screens = erealloc(screens, (m + 1) * sizeof *screens);
	screens[m] = nil;
	for(v=view; v; v=v->next) {
		v->areas = erealloc(v->areas, m * sizeof *v->areas);
		v->r = erealloc(v->r, m * sizeof *v->r);
	}

	for(i=nscreens; i < m; i++) {
		screens[i] = emallocz(sizeof *screens[i]);
		for(v=view; v; v=v->next)
			view_init(v, i);
	}

	nscreens = m;

	/* Reallocate buffers. */
	freeimage(ibuf);
	freeimage(ibuf32);
	ibuf = allocimage(Dx(scr.rect), Dy(scr.rect), scr.depth);
	ibuf32 = nil; /* Probably shouldn't do this until it's needed. */
	if(render_visual)
		ibuf32 = allocimage(Dx(scr.rect), Dy(scr.rect), 32);
	disp.ibuf = ibuf;
	disp.ibuf32 = ibuf32;

	/* Resize and initialize screens. */
	for(i=0; i < nscreens; i++) {
		screen = screens[i];
		screen->idx = i;

		screen->showing = i < n;
		if(screen->showing)
			screen->r = rects[i];
		else
			screen->r = rectsetorigin(screen->r, scr.rect.max);
		def.snap = Dy(screen->r) / 63;
		bar_init(screens[i]);
	}
	screen = screens[0];
	if(selview)
		view_update(selview);
}

static void
cleanup(void) {
	while(client)
		client_destroy(client);
	ixp_server_close(&srv);
	close(sleeperfd);
}

static void
cleanup_handler(int signal) {
	sa.sa_handler = SIG_DFL;
	sigaction(signal, &sa, nil);

	srv.running = false;

	switch(signal) {
	case SIGTERM:
		sa.sa_handler = cleanup_handler;
		sigaction(SIGALRM, &sa, nil);
		alarm(1);
	default:
		exitsignal = signal;
		break;
	case SIGALRM:
		raise(SIGTERM);
	case SIGINT:
		break;
	}
}

static void
init_traps(void) {
	char buf[1];
	int fd[2];

	if(pipe(fd) != 0)
		fatal("Can't pipe(): %r");

	if(doublefork() == 0) {
		close(fd[1]);
		close(ConnectionNumber(display));
		setsid();

		display = XOpenDisplay(nil);
		if(!display)
			fatal("Can't open display");

		/* Wait for parent to exit */
		read(fd[0], buf, 1);

		setfocus(pointerwin, RevertToPointerRoot);
		XCloseDisplay(display);
		exit(0);
	}

	close(fd[0]);
	sleeperfd = fd[1];

	sa.sa_flags = 0;
	sa.sa_handler = cleanup_handler;
	sigaction(SIGINT, &sa, nil);
	sigaction(SIGTERM, &sa, nil);
	sigaction(SIGQUIT, &sa, nil);
	sigaction(SIGHUP, &sa, nil);
	sigaction(SIGUSR1, &sa, nil);
	sigaction(SIGUSR2, &sa, nil);
}

static void
spawn_command(const char *cmd) {
	char *shell, *p;

	if(doublefork() == 0) {
		if(setsid() == -1)
			fatal("Can't setsid: %r");

		shell = passwd->pw_shell;
		if(shell[0] != '/')
			fatal("Shell is not an absolute path: %s", shell);
		/* Run through the user's shell as a login shell */
		p = smprint("-%s", strrchr(shell, '/') + 1);

		close(0);
		open("/dev/null", O_RDONLY);

		execl(shell, p, "-c", cmd, nil);
		fatal("Can't exec '%s': %r", cmd);
		/* Not reached */
	}
}

static void
check_preselect(IxpServer *s) {
	USED(s);

	check_x_event(nil);
}

static void
closedisplay(IxpConn *c) {
	USED(c);

	XCloseDisplay(display);
}

int
main(int argc, char *argv[]) {
	IxpMsg m;
	char **oargv;
	char *wmiirc, *s;
	int i;

	quotefmtinstall();
	fmtinstall('r', errfmt);
	fmtinstall('a', afmt);
	fmtinstall('C', Cfmt);
extern int fmtevent(Fmt*);
	fmtinstall('E', fmtevent);

	wmiirc = "wmiistartrc";

	oargv = argv;
	ARGBEGIN{
	case 'a':
		address = EARGF(usage());
		break;
	case 'r':
		wmiirc = EARGF(usage());
		break;
	case 'v':
		print("%s", version);
		exit(0);
	case 'D':
		s = EARGF(usage());
		m = ixp_message(s, strlen(s), 0);
		msg_debug(&m);
		break;
	default:
		usage();
		break;
	}ARGEND;

	if(argc)
		usage();

	setlocale(LC_CTYPE, "");
	starting = true;

	initdisplay();

	traperrors(true);
	selectinput(&scr.root, EnterWindowMask
			     | SubstructureRedirectMask);
	if(traperrors(false))
		fatal("another window manager is already running");

	passwd = getpwuid(getuid());
	user = estrdup(passwd->pw_name);

	init_environment();

	sock = ixp_announce(address);
	if(sock < 0)
		fatal("Can't create socket '%s': %r", address);
	closeexec(ConnectionNumber(display));
	closeexec(sock);

	if(wmiirc)
		spawn_command(wmiirc);

	init_traps();
	init_cursors();
	init_lock_keys();
	ewmh_init();
	xext_init();

	srv.preselect = check_preselect;
	ixp_listen(&srv, sock, &p9srv, serve_9pcon, nil);
	ixp_listen(&srv, ConnectionNumber(display), nil, check_x_event, closedisplay);

	def.border = 1;
	def.colmode = Colstack;
	def.font = loadfont(FONT);
	def.incmode = ISqueeze;

	def.mod = Mod1Mask;
	strcpy(def.grabmod, "Mod1");

	loadcolor(&def.focuscolor, FOCUSCOLORS);
	loadcolor(&def.normcolor, NORMCOLORS);

	disp.sel = pointerscreen();

	init_screens();
	root_init();

	disp.focus = nil;
	setfocus(screen->barwin, RevertToParent);
	view_select("1");

	scan_wins();
	starting = false;

	view_update_all();
	ewmh_updateviews();

	event("FocusTag %s\n", selview->name);

	i = ixp_serverloop(&srv);
	if(i)
		fprint(2, "%s: error: %r\n", argv0);

	cleanup();

	if(exitsignal)
		raise(exitsignal);
	if(execstr) {
		char *toks[32];
		int n;

		n = unquote(strdup(execstr), toks, nelem(toks)-1);
		toks[n] = nil;
		execvp(toks[0], toks);
		fprint(2, "%s: failed to exec %q: %r\n", argv0, execstr);
		execvp(argv0, oargv);
		fatal("failed to exec myself");
	}
	return i;
}

