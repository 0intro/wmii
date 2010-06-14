/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

char *modkey_names[] = {
	"Shift",
	"",
	"Control",
	"Mod1",
	"Mod2",
	"Mod3",
	"Mod4",
	"Mod5",
	nil
};

bool
parsekey(char *str, int *mask, char **key) {
	static char *keys[16];
	int i, j, nkeys;

	*mask = 0;
	nkeys = tokenize(keys, nelem(keys), str, '-');
	for(i=0; i < nkeys; i++) {
		for(j=0; modkey_names[j]; j++)
			if(!strcasecmp(modkey_names[j], keys[i])) {
				*mask |= 1 << j;
				goto next;
			}
		break;
	next: continue;
	}
	if(key) {
		if(nkeys)
			*key = keys[i];
		return i == nkeys - 1;
	}
	else
		return i == nkeys;
}

int
numlockmask(void) {
	static int masks[] = {
		ShiftMask, LockMask, ControlMask, Mod1Mask,
		Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask
	};
	XModifierKeymap *modmap;
	KeyCode kcode;
	int i, max, numlock;

	numlock = 0;
	modmap = XGetModifierMapping(display);
	kcode = keycode("Num_Lock");
	if(kcode && modmap && modmap->max_keypermod > 0) {
		max = nelem(masks) * modmap->max_keypermod;
		for(i = 0; i < max && !numlock; i++)
			if(modmap->modifiermap[i] == kcode)
				numlock = masks[i / modmap->max_keypermod];
	}
	XFreeModifiermap(modmap);
	return numlock;
}

int
fmtkey(Fmt *f) {
	XKeyEvent *ev;
	char *key;
	int nfmt;

	ev = va_arg(f->args, XKeyEvent*);
	key = XKeysymToString(XKeycodeToKeysym(display, ev->keycode, 0));

	nfmt = f->nfmt;
	unmask(f, ev->state, modkey_names, '-');
	if(f->nfmt > nfmt)
		fmtrune(f, '-');
	return fmtstrcpy(f, key);
}

