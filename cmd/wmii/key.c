/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <X11/keysym.h>
#include "fns.h"

void
init_lock_keys(void) {
	XModifierKeymap *modmap;
	KeyCode numlock;
	static int masks[] = {
		ShiftMask, LockMask, ControlMask,
		Mod1Mask, Mod2Mask, Mod3Mask,
		Mod4Mask, Mod5Mask
	};
	int i;

	numlock_mask = 0;
	modmap = XGetModifierMapping(display);
	numlock = keycode("Num_Lock");
	if(numlock)
	if(modmap && modmap->max_keypermod > 0) {
		int max;
		
		max = nelem(masks) * modmap->max_keypermod;
		for(i = 0; i < max; i++)
			if(modmap->modifiermap[i] == numlock)
				numlock_mask = masks[i / modmap->max_keypermod];
	}
	XFreeModifiermap(modmap);
	valid_mask = 255 & ~(numlock_mask | LockMask);
}

ulong
str2modmask(const char *val) {
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
grabkey(Key *k) {
	XGrabKey(display, k->key, k->mod, scr.root.w,
			true, GrabModeAsync, GrabModeAsync);
	if(numlock_mask) {
		XGrabKey(display, k->key, k->mod | numlock_mask, scr.root.w,
				true, GrabModeAsync, GrabModeAsync);
		XGrabKey(display, k->key, k->mod | numlock_mask | LockMask, scr.root.w,
				true, GrabModeAsync, GrabModeAsync);
	}
	sync();
}

static void
ungrabkey(Key *k) {
	XUngrabKey(display, k->key, k->mod, scr.root.w);
	if(numlock_mask) {
		XUngrabKey(display, k->key, k->mod | numlock_mask, scr.root.w);
		XUngrabKey(display, k->key, k->mod | numlock_mask | LockMask, scr.root.w);
	}
	sync();
}

static Key *
name2key(const char *name) {
	Key *k;

	for(k=key; k; k=k->lnext)
		if(!strncmp(k->name, name, sizeof k->name))
			return k;
	return nil;
}

static Key *
getkey(const char *name) {
	char buf[128];
	char *seq[8];
	char *kstr;
	uint i, toks;
	static ushort id = 1;
	Key *k, *r;

	r = nil;

	if((k = name2key(name))) {
		ungrabkey(k);
		return k;
	}
	utflcpy(buf, name, sizeof buf);
	toks = tokenize(seq, 8, buf, ',');
	for(i = 0; i < toks; i++) {
		if(!k)
			r = k = emallocz(sizeof *k);
		else {
			k->next = emallocz(sizeof *k);
			k = k->next;
		}
		utflcpy(k->name, name, sizeof k->name);
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
fake_keypress(ulong mod, KeyCode key) {
	XKeyEvent e;
	Client *c;

	c = screen->focus;
	if(c == nil || c->w.w == 0)
		return;

	e.time = CurrentTime;
	e.window = c->w.w;
	e.display = display;
	e.state = mod;
	e.keycode = key;

	e.type = KeyPress;
	sendevent(&c->w, true, KeyPressMask, (XEvent*)&e);
	e.type = KeyRelease;
	sendevent(&c->w, true, KeyReleaseMask, (XEvent*)&e);

	sync();
}

static Key *
match_keys(Key *k, ulong mod, KeyCode keycode, bool seq) {
	Key *ret, *next;
	volatile int i; /* shut up ken */

	ret = nil;
	for(next = k->tnext; k; i = (k=next) && (next=k->tnext)) {
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
	found = match_keys(done, mod, key, true);
	if((done->mod == mod) && (done->key == key))
		fake_keypress(mod, key); /* double key */
	else {
		if(!found)
			XBell(display, 0);
		else if(!found->tnext && !found->next)
			event("Key %s\n", found->name);
		else
			kpress_seq(w, found);
	}
}

void
kpress(XWindow w, ulong mod, KeyCode keycode) {
	Key *k, *found;

	for(k=key; k; k=k->lnext)
		 k->tnext=k->lnext;
	found = match_keys(key, mod, keycode, false);
	if(!found) /* grabbed but not found */
		XBell(display, 0);
	else if(!found->tnext && !found->next)
		event("Key %s\n", found->name);
	else {
		XGrabKeyboard(display, w, true, GrabModeAsync, GrabModeAsync, CurrentTime);
		flushevents(FocusChangeMask, true);
		kpress_seq(w, found);
		XUngrabKeyboard(display, CurrentTime);
		sync();
	}
}

void
update_keys(void) {
	Key *k, *n;
	char *l, *p;

	init_lock_keys();
	while((k = key)) {
		key = key->lnext;
		ungrabkey(k);
		while((n = k)) {
			k = k->next;
			free(n);
		}
	}
	for(l = p = def.keys; p && *p;) {
		if(*p == '\n') {
			*p = 0;
			if((k = getkey(l)))
				grabkey(k);
			*p = '\n';
			l = ++p;
		}
		else
			p++;
	}
	if(l < p && strlen(l)) {
		if((k = getkey(l)))
			grabkey(k);
	}
	sync();
}
