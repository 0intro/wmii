/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "wm.h"

void
init_lock_keys()
{
	XModifierKeymap *modmap;
	KeyCode num_lock;
	static int masks[] = {
		ShiftMask, LockMask, ControlMask, Mod1Mask,
		Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask
	};
	int i;

	num_lock_mask = 0;
	modmap = XGetModifierMapping(blz.display);
	num_lock = XKeysymToKeycode(blz.display, XStringToKeysym("Num_Lock"));

	if(modmap && modmap->max_keypermod > 0) {
		int max = (sizeof(masks) / sizeof(int)) * modmap->max_keypermod;
		for(i = 0; i < max; i++)
			if(num_lock && (modmap->modifiermap[i] == num_lock))
				num_lock_mask = masks[i / modmap->max_keypermod];
	}
	XFreeModifiermap(modmap);

	valid_mask = 255 & ~(num_lock_mask | LockMask);
}

unsigned long
mod_key_of_str(char *val)
{
	unsigned long mod = 0;
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
grab_key(Key *k)
{
	XGrabKey(blz.display, k->key, k->mod, blz.root,
			True, GrabModeAsync, GrabModeAsync);
	if(num_lock_mask) {
		XGrabKey(blz.display, k->key, k->mod | num_lock_mask, blz.root,
				True, GrabModeAsync, GrabModeAsync);
		XGrabKey(blz.display, k->key, k->mod | num_lock_mask | LockMask, blz.root,
				True, GrabModeAsync, GrabModeAsync);
	}
	XSync(blz.display, False);
}

static void
ungrab_key(Key *k)
{
	XUngrabKey(blz.display, k->key, k->mod, blz.root);
	if(num_lock_mask) {
		XUngrabKey(blz.display, k->key, k->mod | num_lock_mask, blz.root);
		XUngrabKey(blz.display, k->key, k->mod | num_lock_mask | LockMask, blz.root);
	}
	XSync(blz.display, False);
}

static Key *
name2key(const char *name)
{
	Key *k;
	for(k=key; k && strncmp(k->name, name, sizeof(k->name)); k=k->lnext);
	return k;
}

static Key *
get_key(const char *name)
{
	char buf[128];
	char *seq[8];
	char *kstr;
	unsigned int i, toks;
	static unsigned short id = 1;
	Key *k = 0, *r = 0;

	if((k = name2key(name))) {
		ungrab_key(k);
		return k;
	}

	cext_strlcpy(buf, name, sizeof(buf));
	toks = cext_tokenize(seq, 8, buf, ',');

	for(i = 0; i < toks; i++) {
		if(!k)
			r = k = cext_emallocz(sizeof(Key));
		else {
			k->next = cext_emallocz(sizeof(Key));
			k = k->next;
		}
		cext_strlcpy(k->name, name, sizeof(k->name));
		kstr = strrchr(seq[i], '-');
		if(kstr)
			kstr++;
		else
			kstr = seq[i];
		k->key = XKeysymToKeycode(blz.display, XStringToKeysym(kstr));
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
next_keystroke(unsigned long *mod, KeyCode *keyCode)
{
	XEvent e;
	KeySym sym;
	*mod = 0;
	do {
		XMaskEvent(blz.display, KeyPressMask, &e);
		*mod |= e.xkey.state & valid_mask;
		*keyCode = (KeyCode) e.xkey.keycode;
		sym = XKeycodeToKeysym(blz.display, e.xkey.keycode, 0);
	} while(IsModifierKey(sym));
}

static void
emulate_key_press(unsigned long mod, KeyCode key)
{
	XEvent e;
	Window client_win;
	int revert;

	XGetInputFocus(blz.display, &client_win, &revert);

	e.xkey.type = KeyPress;
	e.xkey.time = CurrentTime;
	e.xkey.window = client_win;
	e.xkey.display = blz.display;
	e.xkey.state = mod;
	e.xkey.keycode = key;
	XSendEvent(blz.display, client_win, True, KeyPressMask, &e);
	e.xkey.type = KeyRelease;
	XSendEvent(blz.display, client_win, True, KeyReleaseMask, &e);
	XSync(blz.display, False);
}

static Key *
match_keys(Key *k, unsigned long mod, KeyCode keycode, Bool seq)
{
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
handle_key_seq(Window w, Key *done)
{
	unsigned long mod;
	KeyCode key;
	Key *found;
	char buf[128];

	next_keystroke(&mod, &key);

	found = match_keys(done, mod, key, True);
	if((done->mod == mod) && (done->key == key))
		emulate_key_press(mod, key); /* double key */
	else {
		if(!found) {
			XBell(blz.display, 0);
		} /* grabbed but not found */
		else if(!found->tnext && !found->next) {
			snprintf(buf, sizeof(buf), "Key %s\n", found->name);
			write_event(buf);
		}
		else
			handle_key_seq(w, found);
	}
}

void
handle_key(Window w, unsigned long mod, KeyCode keycode)
{
	Key *k;
	char buf[128];

	for(k=key; k; k->tnext=k->lnext, k=k->lnext);
	Key *found = match_keys(key, mod, keycode, False);

	if(!found) {
		XBell(blz.display, 0);
	} /* grabbed but not found */
	else if(!found->tnext && !found->next) {
		snprintf(buf, sizeof(buf), "Key %s\n", found->name);
		write_event(buf);
	}
	else {
		XGrabKeyboard(blz.display, w, True, GrabModeAsync, GrabModeAsync, CurrentTime);
		handle_key_seq(w, found);
		XUngrabKeyboard(blz.display, CurrentTime);
		XSync(blz.display, False);
	}
}

void
update_keys()
{
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

	XSync(blz.display, False);
}
