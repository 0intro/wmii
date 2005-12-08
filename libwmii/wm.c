/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "blitz.h"

int property(Display * dpy, Window w, Atom a, Atom t, long l, unsigned char **prop)
{
	Atom real;
	int format;
	unsigned long res, extra;
	int status;

	status = XGetWindowProperty(dpy, w, a, 0L, l, False, t, &real, &format,
								&res, &extra, prop);

	if (status != Success || *prop == 0) {
		return 0;
	}
	if (res == 0) {
		free((void *) *prop);
	}
	return res;
}

void win_prop(Display * dpy, Window w, Atom a, char *res, int len)
{
	unsigned char *prop;

	if (property(dpy, w, a, XA_STRING, 100L, &prop)) {
		cext_strlcpy(res, (char *) prop, len);
		XFree(prop);
	}
	res[len - 1] = 0;
	XSync(dpy, False);
}

void send_message(Display * dpy, Window w, Atom a, long value)
{
	XEvent e;
	e.type = ClientMessage;
	e.xclient.window = w;
	e.xclient.message_type = a;
	e.xclient.format = 32;
	e.xclient.data.l[0] = value;
	e.xclient.data.l[1] = CurrentTime;

	XSendEvent(dpy, w, False, NoEventMask, &e);
	XSync(dpy, False);
}

#define NUM_MASKS      8
void
init_lock_modifiers(Display * dpy, unsigned int *valid_mask,
					unsigned int *num_lock_mask)
{
	XModifierKeymap *modmap;
	KeyCode num_lock;
	static int masks[NUM_MASKS] = {
		ShiftMask, LockMask, ControlMask, Mod1Mask,
		Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask
	};
	int i;

	*num_lock_mask = 0;
	modmap = XGetModifierMapping(dpy);
	num_lock = XKeysymToKeycode(dpy, XStringToKeysym("Num_Lock"));

	if (modmap && modmap->max_keypermod > 0) {
		int max = NUM_MASKS * modmap->max_keypermod;
		for (i = 0; i < max; i++) {
			if (num_lock && (modmap->modifiermap[i] == num_lock)) {
				*num_lock_mask = masks[i / modmap->max_keypermod];
			}
		}
	}
	XFreeModifiermap(modmap);

	*valid_mask = 255 & ~(*num_lock_mask | LockMask);
}
