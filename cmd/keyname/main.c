/* Copyright ©2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stuff/util.h>
#include <stuff/x.h>
#include <fmt.h>
#include <locale.h>

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

	initdisplay();

	selectinput(&scr.root, KeyPressMask|KeyReleaseMask);
	sethandler(&scr.root, &handlers);
	if(!grabkeyboard(&scr.root))
		fatal("can't grab keyboard\n");

	event_loop();
	print("%s\n", keyname);

	XCloseDisplay(display);
	return 0;
}

static bool
kdown_event(Window *w, void *aux, XKeyEvent *ev) {
	Fmt f;
	char buf[32];
	char *key;
	KeySym ksym;
	int num;

	USED(aux);
	nkeys++;
	num = XLookupString(ev, buf, sizeof buf, &ksym, 0);
	key = XKeysymToString(ksym);

	fmtstrinit(&f);
	unmask(&f, ev->state, modkey_names, '-');
	if(f.nfmt)
		fmtrune(&f, '-');
	fmtstrcpy(&f, key);

	free(keyname);
	keyname = fmtstrflush(&f);
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

