/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#define EXTERN
#include "dat.h"
#include <X11/Xproto.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <stuff/clientutil.h>
#include <sys/signal.h>
#include "fns.h"

static const char version[] = "witray-"VERSION", "COPYRIGHT"\n";

static int	exitsignal;
static struct sigaction	sa;

static void
usage(void) {
	fprint(2, "usage: %s [-a <address>] [-NESW] [-HVn] [-p <padding>] [-s <iconsize>] [-t tags]\n"
	          "       %s -v\n", argv0, argv0);
	exit(1);
}

static int
errfmt(Fmt *f) {
	return fmtstrcpy(f, ixp_errbuf());
}

static void
cleanup_handler(int signal) {
	sa.sa_handler = SIG_DFL;
	sigaction(signal, &sa, nil);

	selection_release(tray.selection);
	srv.running = false;

	switch(signal) {
	case SIGINT:
	case SIGTERM:
		sa.sa_handler = cleanup_handler;
		sigaction(SIGALRM, &sa, nil);
		alarm(1);
	default:
		exitsignal = signal;
		break;
	case SIGALRM:
		raise(SIGTERM);
	}
}

static void
init_traps(void) {

	sa.sa_flags = 0;
	sa.sa_handler = cleanup_handler;
	sigaction(SIGINT, &sa, nil);
	sigaction(SIGTERM, &sa, nil);
	sigaction(SIGQUIT, &sa, nil);
	sigaction(SIGHUP, &sa, nil);
	sigaction(SIGUSR1, &sa, nil);
	sigaction(SIGUSR2, &sa, nil);
}

void
cleanup(Selection *s) {
	USED(s);

	while(tray.clients)
		client_disown(tray.clients);
	tray.selection = nil;
	srv.running = false;
}

void
message(Selection *s, XClientMessageEvent *ev) {
	Window *w;
	Client *c;

	USED(s);

	Dprint("message(%A) 0x%lx\n", ev->message_type, ev->window);
	Dprint("\t0x%lx, 0x%lx, 0x%ulx, 0x%ulx, 0x%ulx\n",
	       ev->data.l[0], ev->data.l[1], ev->data.l[2], ev->data.l[3], ev->data.l[4]);

	w = findwin(ev->window);
	if(w == nil)
		return;

	if(w == tray.selection->owner) {
		if(ev->message_type == NET("SYSTEM_TRAY_OPCODE") && ev->format == 32)
			if(ev->data.l[1] == TrayRequestDock)
				client_manage(ev->data.l[2]);
		return;
	}

	if((c = client_find(w))) {
		if(ev->message_type == NET("SYSTEM_TRAY_OPCODE") && ev->format == 32)
			client_opcode(w->aux, ev->data.l[1], ev->data.l[2], ev->data.l[3], ev->data.l[4]);
		else
			client_message(w->aux, ev->message_type, ev->format, (ClientMessageData*)&ev->data);
		return;
	}
}

ErrorCode ignored_xerrors[] = {
	{ 0, BadWindow },
	{ X_GetAtomName, BadAtom },
};

int
main(int argc, char *argv[]) {
	static char* address;
	bool steal;

	program_args = argv;

	setlocale(LC_CTYPE, "");
	fmtinstall('r', errfmt);
	fmtinstall('E', fmtevent);

	steal = true;
	tray.orientation = OHorizontal;
	tray.tags = "/./";
	tray.padding = 1;

	ARGBEGIN{
	case 'N':
		tray.edge = (tray.edge & ~South) | North;
		break;
	case 'S':
		tray.edge = (tray.edge & ~North) | South;
		break;
	case 'E':
		tray.edge = (tray.edge & ~West) | East;
		break;
	case 'W':
		tray.edge = (tray.edge & ~East) | West;
		break;
	case 'H':
		tray.orientation = OHorizontal;
		break;
	case 'V':
		tray.orientation = OVertical;
		break;
	case 'n':
		steal = false;
		break;
	case 'p':
		if(!getulong(EARGF(usage()), &tray.padding))
			usage();
		tray.padding = max(1, min(10, (int)tray.padding));
		break;
	case 's':
		if(!getulong(EARGF(usage()), &tray.iconsize))
			usage();
		tray.iconsize = max(1, (int)tray.iconsize);
		break;
	case 't':
		tray.tags = EARGF(usage());
		break;
	case 'a':
		address = EARGF(usage());
		break;
	case 'D':
		debug++;
		break;
	case 'v':
		lprint(1, "%s", version);
		return 0;
	default:
		usage();
	}ARGEND;

	if(argc)
		usage();

	init_traps();
	initdisplay();

	srv.preselect = event_preselect;
	ixp_listen(&srv, ConnectionNumber(display), nil, event_fdready, event_fdclosed);

	event_updatextime();
	tray.selection = selection_manage(sxprint(Net("SYSTEM_TRAY_S%d"), scr.screen),
					  event_xtime, message, cleanup, steal);
	if(tray.selection == nil)
		fatal("Another system tray is already running.");
	if(tray.selection->oldowner)
		lprint(1, "%s: Replacing currently running system tray.\n", argv0);

	xext_init();
	tray_init();

	client_init(address);

	if(tray.edge == 0)
		tray.edge = West | (!strcmp(readctl("/ctl", "bar on "), "top") ? North : South);

	client_readconfig(&tray.normcolors, &tray.selcolors, &tray.font);

	if(tray.iconsize == 0) /* Default to wmii's bar size. */
		tray.iconsize = labelh(tray.font) - 2 * tray.padding;

	srv.running = true;
	ixp_serverloop(&srv);

	if(tray.selection)
		selection_release(tray.selection);

	XCloseDisplay(display);

	if(exitsignal)
		raise(exitsignal);
	return 0;
}

