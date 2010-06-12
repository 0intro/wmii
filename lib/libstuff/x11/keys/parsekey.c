/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

typedef struct KMask KMask;

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
