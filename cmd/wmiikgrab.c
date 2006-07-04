/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cext.h>

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
emulate_key_press(unsigned long mod, KeyCode key)
{
	XEvent e;
	Window win;
	int revert;

	XGetInputFocus(dpy, &win, &revert);

	e.xkey.type = KeyPress;
	e.xkey.time = CurrentTime;
	e.xkey.window = win;
	e.xkey.display = dpy;
	e.xkey.state = mod;
	e.xkey.keycode = key;
	XSendEvent(dpy, win, True, KeyPressMask, &e);
	e.xkey.type = KeyRelease;
	XSendEvent(dpy, win, True, KeyReleaseMask, &e);
	XSync(dpy, False);
}

static void
next_keystroke(unsigned long *mod, KeyCode *code)
{
	XEvent e;
	KeySym sym;
	*mod = 0;
	do {
		XMaskEvent(dpy, KeyPressMask, &e);
		*mod |= e.xkey.state;
		*code = (KeyCode) e.xkey.keycode;
		sym = XKeycodeToKeysym(dpy, e.xkey.keycode, 0);
	} while(IsModifierKey(sym));
}

static void
print_key(unsigned long mod, KeyCode code)
{
	char buf[256];

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

	cext_strlcat(buf,
			XKeysymToString(XKeycodeToKeysym(dpy, code, 0)), sizeof(buf));

	fprintf(stdout, "EventType=Key;EventValue='%s'\n", buf);
}

int
main(int argc, char **argv)
{
	unsigned long mod;
	KeyCode code;

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

	XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime);
	for(;;) {
		next_keystroke(&mod, &code);
		print_key(mod, code);
		emulate_key_press(mod, code);
	}
	XUngrabKeyboard(dpy, CurrentTime);
	XSync(dpy, False);

	return 0;
}
