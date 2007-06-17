/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include <string.h>
#include <stdlib.h>
#include <X11/keysym.h>
#include <util.h>
#include "dat.h"
#include "fns.h"

void
init_lock_keys(void) {
	XModifierKeymap *modmap;
	KeyCode num_lock;
	static int masks[] = {
		ShiftMask, LockMask, ControlMask, Mod1Mask,
		Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask
	};
	int i;

	num_lock_mask = 0;
	modmap = XGetModifierMapping(display);
	num_lock = XKeysymToKeycode(display, XStringToKeysym("Num_Lock"));
	if(modmap && modmap->max_keypermod > 0) {
		int max = (sizeof(masks) / sizeof(int)) * modmap->max_keypermod;
		for(i = 0; i < max; i++)
			if(num_lock && (modmap->modifiermap[i] == num_lock))
				num_lock_mask = masks[i / modmap->max_keypermod];
	}
	XFreeModifiermap(modmap);
	valid_mask = 255 & ~(num_lock_mask | LockMask);
}

ulong
str2modmask(char *val) {
	ulong mod = 0;

	if (strstr(val, "Shift"))
		mod |= ShiftMask;
	if (strstr(val, "Control"))
		mod |= ControlMask;
	if (strstr(val, "Mod1"))
		mod |= Mod1Mask;
	if (strstr(val, "Mod2"))
		mod |= Mod2Mask;
	if (strstr(val, "Mod3"))
		mod |= Mod3Mask;
	if (strstr(val, "Mod4"))
		mod |= Mod4Mask;
	if (strstr(val, "Mod5"))
		mod |= Mod5Mask;
	return mod;
}

static void
grab_key(Key *k) {
	XGrabKey(display, k->key, k->mod, scr.root.w,
			True, GrabModeAsync, GrabModeAsync);
	if(num_lock_mask) {
		XGrabKey(display, k->key, k->mod | num_lock_mask, scr.root.w,
				True, GrabModeAsync, GrabModeAsync);
		XGrabKey(display, k->key, k->mod | num_lock_mask | LockMask, scr.root.w,
				True, GrabModeAsync, GrabModeAsync);
	}
	XSync(display, False);
}

static void
ungrab_key(Key *k) {
	XUngrabKey(display, k->key, k->mod, scr.root.w);
	if(num_lock_mask) {
		XUngrabKey(display, k->key, k->mod | num_lock_mask, scr.root.w);
		XUngrabKey(display, k->key, k->mod | num_lock_mask | LockMask, scr.root.w);
	}
	XSync(display, False);
}

static Key *
name2key(const char *name) {
	Key *k;
	for(k=key; k; k=k->lnext)
		if(!strncmp(k->name, name, sizeof(k->name))) break;
	return k;
}

static Key *
get_key(const char *name) {
	char buf[128];
	char *seq[8];
	char *kstr;
	uint i, toks;
	static ushort id = 1;
	Key *k = 0, *r = 0;

	if((k = name2key(name))) {
		ungrab_key(k);
		return k;
	}
	strncpy(buf, name, sizeof(buf));
	toks = tokenize(seq, 8, buf, ',');
	for(i = 0; i < toks; i++) {
		if(!k)
			r = k = emallocz(sizeof(Key));
		else {
			k->next = emallocz(sizeof(Key));
			k = k->next;
		}
		strncpy(k->name, name, sizeof(k->name));
		kstr = strrchr(seq[i], '-');
		if(kstr)
			kstr++;
		else
			kstr = seq[i];
		k->key = XKeysymToKeycode(display, XStringToKeysym(kstr));
		k->mod = str2modmask(seq[i]);
	}
	if(r) {
		r->id = id++;
		r->lnext = key;
		key = r;
	}

	return r;
}

static void
next_keystroke(ulong *mod, KeyCode *code) {
	XEvent e;
	KeySym sym;
	*mod = 0;

	do {
		XMaskEvent(display, KeyPressMask, &e);
		*mod |= e.xkey.state & valid_mask;
		*code = (KeyCode) e.xkey.keycode;
		sym = XKeycodeToKeysym(display, e.xkey.keycode, 0);
	} while(IsModifierKey(sym));
}

static void
emulate_key_press(ulong mod, KeyCode key) {
	XEvent e;
	XWindow client_win;
	int revert;

	XGetInputFocus(display, &client_win, &revert);
	e.xkey.type = KeyPress;
	e.xkey.time = CurrentTime;
	e.xkey.window = client_win;
	e.xkey.display = display;
	e.xkey.state = mod;
	e.xkey.keycode = key;
	XSendEvent(display, client_win, True, KeyPressMask, &e);
	e.xkey.type = KeyRelease;
	XSendEvent(display, client_win, True, KeyReleaseMask, &e);
	XSync(display, False);
}

static Key *
match_keys(Key *k, ulong mod, KeyCode keycode, Bool seq) {
	Key *ret = nil, *next;

	for(next = k->tnext; k; (k=next) && (next=k->tnext)) {
		if(seq)
			k = k->next;
		if(k && (k->mod == mod) && (k->key == keycode)) {
			k->tnext = ret;
			ret = k;
		}
	}
	return ret;
}

static void
kpress_seq(XWindow w, Key *done) {
	ulong mod;
	KeyCode key;
	Key *found;

	next_keystroke(&mod, &key);
	found = match_keys(done, mod, key, True);
	if((done->mod == mod) && (done->key == key))
		emulate_key_press(mod, key); /* double key */
	else {
		if(!found)
			XBell(display, 0);
		else if(!found->tnext && !found->next)
			write_event("Key %s\n", found->name);
		else
			kpress_seq(w, found);
	}
}

void
kpress(XWindow w, ulong mod, KeyCode keycode) {
	Key *k, *found;

	for(k=key; k; k=k->lnext)
		 k->tnext=k->lnext;
	found = match_keys(key, mod, keycode, False);
	if(!found) /* grabbed but not found */
		XBell(display, 0);
	else if(!found->tnext && !found->next)
		write_event("Key %s\n", found->name);
	else {
		XGrabKeyboard(display, w, True, GrabModeAsync, GrabModeAsync, CurrentTime);
		flushevents(FocusChangeMask, True);
		kpress_seq(w, found);
		XUngrabKeyboard(display, CurrentTime);
		XSync(display, False);
	}
}

void
update_keys(void) {
	Key *k, *n;
	char *l, *p;

	init_lock_keys();
	while((k = key)) {
		key = key->lnext;
		ungrab_key(k);
		while((n = k)) {
			k = k->next;
			free(n);
		}
	}
	for(l = p = def.keys; p && *p;) {
		if(*p == '\n') {
			*p = 0;
			if((k = get_key(l)))
				grab_key(k);
			*p = '\n';
			l = ++p;
		}
		else
			p++;
	}
	if(l < p && strlen(l)) {
		if((k = get_key(l)))
			grab_key(k);
	}
	XSync(display, False);
}
