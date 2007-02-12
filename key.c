/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "wmii.h"
#include <string.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

void
init_lock_keys() {
	XModifierKeymap *modmap;
	KeyCode num_lock;
	static int masks[] = {
		ShiftMask, LockMask, ControlMask, Mod1Mask,
		Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask
	};
	int i;

	num_lock_mask = 0;
	modmap = XGetModifierMapping(blz.dpy);
	num_lock = XKeysymToKeycode(blz.dpy, XStringToKeysym("Num_Lock"));
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
mod_key_of_str(char *val) {
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
	XGrabKey(blz.dpy, k->key, k->mod, blz.root,
			True, GrabModeAsync, GrabModeAsync);
	if(num_lock_mask) {
		XGrabKey(blz.dpy, k->key, k->mod | num_lock_mask, blz.root,
				True, GrabModeAsync, GrabModeAsync);
		XGrabKey(blz.dpy, k->key, k->mod | num_lock_mask | LockMask, blz.root,
				True, GrabModeAsync, GrabModeAsync);
	}
	XSync(blz.dpy, False);
}

static void
ungrab_key(Key *k) {
	XUngrabKey(blz.dpy, k->key, k->mod, blz.root);
	if(num_lock_mask) {
		XUngrabKey(blz.dpy, k->key, k->mod | num_lock_mask, blz.root);
		XUngrabKey(blz.dpy, k->key, k->mod | num_lock_mask | LockMask, blz.root);
	}
	XSync(blz.dpy, False);
}

static Key *
name2key(const char *name) {
	Key *k;
	for(k=key; k && strncmp(k->name, name, sizeof(k->name)); k=k->lnext);
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
	toks = ixp_tokenize(seq, 8, buf, ',');
	for(i = 0; i < toks; i++) {
		if(!k)
			r = k = ixp_emallocz(sizeof(Key));
		else {
			k->next = ixp_emallocz(sizeof(Key));
			k = k->next;
		}
		strncpy(k->name, name, sizeof(k->name));
		kstr = strrchr(seq[i], '-');
		if(kstr)
			kstr++;
		else
			kstr = seq[i];
		k->key = XKeysymToKeycode(blz.dpy, XStringToKeysym(kstr));
		k->mod = mod_key_of_str(seq[i]);
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
		XMaskEvent(blz.dpy, KeyPressMask, &e);
		*mod |= e.xkey.state & valid_mask;
		*code = (KeyCode) e.xkey.keycode;
		sym = XKeycodeToKeysym(blz.dpy, e.xkey.keycode, 0);
	} while(IsModifierKey(sym));
}

static void
emulate_key_press(ulong mod, KeyCode key) {
	XEvent e;
	Window client_win;
	int revert;

	XGetInputFocus(blz.dpy, &client_win, &revert);
	e.xkey.type = KeyPress;
	e.xkey.time = CurrentTime;
	e.xkey.window = client_win;
	e.xkey.display = blz.dpy;
	e.xkey.state = mod;
	e.xkey.keycode = key;
	XSendEvent(blz.dpy, client_win, True, KeyPressMask, &e);
	e.xkey.type = KeyRelease;
	XSendEvent(blz.dpy, client_win, True, KeyReleaseMask, &e);
	XSync(blz.dpy, False);
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
kpress_seq(Window w, Key *done) {
	ulong mod;
	KeyCode key;
	Key *found;

	next_keystroke(&mod, &key);
	found = match_keys(done, mod, key, True);
	if((done->mod == mod) && (done->key == key))
		emulate_key_press(mod, key); /* double key */
	else {
		if(!found) {
			XBell(blz.dpy, 0);
		} /* grabbed but not found */
		else if(!found->tnext && !found->next)
			write_event("Key %s\n", found->name);
		else
			kpress_seq(w, found);
	}
}

void
kpress(Window w, ulong mod, KeyCode keycode) {
	Key *k, *found;

	for(k=key; k; k->tnext=k->lnext, k=k->lnext);
	found = match_keys(key, mod, keycode, False);
	if(!found) {
		XBell(blz.dpy, 0);
	} /* grabbed but not found */
	else if(!found->tnext && !found->next)
		write_event("Key %s\n", found->name);
	else {
		XGrabKeyboard(blz.dpy, w, True, GrabModeAsync, GrabModeAsync, CurrentTime);
		kpress_seq(w, found);
		XUngrabKeyboard(blz.dpy, CurrentTime);
		XSync(blz.dpy, False);
	}
}

void
update_keys() {
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
	XSync(blz.dpy, False);
}
