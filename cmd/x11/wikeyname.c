/* Copyright ©2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stuff/util.h>
#include <stuff/x.h>
#include <fmt.h>
#include <locale.h>
#include <unistd.h>

static const char version[] = "wikeyname-"VERSION", "COPYRIGHT"\n";

static Handlers handlers;
static char*	keyname;
static int	nkeys;

static void
usage(void) {
	fprint(2, "usage: wikeyname\n");
	exit(1);
}

int
main(int argc, char *argv[]) {

	setlocale(LC_CTYPE, "");

	ARGBEGIN{
	default: usage();
	}ARGEND;

	if(argc)
		usage();

	fmtinstall('K', fmtkey);
	initdisplay();

	selectinput(&scr.root, KeyPressMask|KeyReleaseMask);
	sethandler(&scr.root, &handlers);
	if(!grabkeyboard(&scr.root))
		fatal("can't grab keyboard\n");

	if(isatty(1))
		lprint(2, "Please press a key...\n");
	event_loop();
	lprint(1, "%s\n", keyname);

	XCloseDisplay(display);
	return 0;
}

static bool
kdown_event(Window *w, void *aux, XKeyEvent *ev) {

	USED(w, aux);
	nkeys++;
	free(keyname);
	keyname = smprint("%K", ev);
	return false;
}

static bool
kup_event(Window *w, void *aux, XKeyEvent *ev) {

	USED(w, aux, ev);
	if(keyname != nil && --nkeys <= 0)
		event_looprunning = false;
	return false;
}


static Handlers handlers = {
	.kup = kup_event,
	.kdown = kdown_event,
};

