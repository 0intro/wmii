/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cext.h>

#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

static char version[] = "wmiikgrab - " VERSION ", (C)opyright MMIV-MMVI Anselm R. Garbe\n";
static Window root;
static Display *dpy;

static void
usage()
{
	fprintf(stderr, "%s\n", "usage: wmiikgrab [-v]\n");
	exit(1);
}

static void
next_keystroke(unsigned long *mod, KeyCode *code)
{
	XEvent e;

	while(XGrabKeyboard(dpy, root, True,
				GrabModeAsync, GrabModeAsync, CurrentTime) != Success);
	*mod = 0;
	XMaskEvent(dpy, KeyPressMask, &e);
	*mod |= e.xkey.state;/* & valid_mask;*/
	*code = (KeyCode) e.xkey.keycode;
	XUngrabKeyboard(dpy, CurrentTime);
}

static void
print_key(unsigned long mod, KeyCode code)
{
	char buf[256], *k;

	buf[0] = 0;
	if(mod & ShiftMask)
		cext_strlcat(buf, "Shift-", sizeof(buf));
	if(mod & ControlMask)
		cext_strlcat(buf, "Control-", sizeof(buf));
	if(mod & Mod1Mask)
		cext_strlcat(buf, "Mod1-", sizeof(buf));
	if(mod & Mod2Mask)
		cext_strlcat(buf, "Mod2-", sizeof(buf));
	if(mod & Mod3Mask)
		cext_strlcat(buf, "Mod3-", sizeof(buf));
	if(mod & Mod4Mask)
		cext_strlcat(buf, "Mod4-", sizeof(buf));
	if(mod & Mod5Mask)
		cext_strlcat(buf, "Mod5-", sizeof(buf));

	if((k = XKeysymToString(XKeycodeToKeysym(dpy, code, 0))))
	cext_strlcat(buf, k, sizeof(buf));

	fprintf(stdout, "EventType=Key;EventValue='%s'\n", buf);
}

int
main(int argc, char **argv)
{
	unsigned long mod = 0;
	KeyCode code = 0;

	/* command line args */
	if(argc > 1) {
		if(!strncmp(argv[1], "-v", 3)) {
			fprintf(stdout, "%s", version);
			exit(0);
		} else
			usage();
	}
	dpy = XOpenDisplay(0);
	if(!dpy) {
		fprintf(stderr, "%s", "wmiikgrab: cannot open display\n");
		exit(1);
	}
	root = DefaultRootWindow(dpy);

	for(;;) {
		next_keystroke(&mod, &code);
		print_key(mod, code);
		XTestFakeKeyEvent(dpy, code, True, CurrentTime);
	}

	return 0;
}
