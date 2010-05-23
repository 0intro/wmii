/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

typedef struct KMask KMask;

static struct KMask {
	int		mask;
	const char*	name;
} masks[] = {
	{ShiftMask,   "Shift"},
	{ControlMask, "Control"},
	{Mod1Mask,    "Mod1"},
	{Mod2Mask,    "Mod2"},
	{Mod3Mask,    "Mod3"},
	{Mod4Mask,    "Mod4"},
	{Mod5Mask,    "Mod5"},
	{0,}
};

bool
parsekey(char *str, int *mask, char **key) {
	static char *keys[16];
	KMask *m;
	int i, nkeys;

	*mask = 0;
	nkeys = tokenize(keys, nelem(keys), str, '-');
	for(i=0; i < nkeys; i++) {
		for(m=masks; m->mask; m++)
			if(!strcasecmp(m->name, keys[i])) {
				*mask |= m->mask;
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
