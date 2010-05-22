/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <string.h>
#include "../x11.h"

void
changeprop_textlist(Window *w, char *prop, char *type, char *data[]) {
	char **p, *s, *t;
	int len, n;

	len = 0;
	for(p=data; *p; p++)
		len += strlen(*p) + 1;
	s = emalloc(len);
	t = s;
	for(p=data; *p; p++) {
		n = strlen(*p) + 1;
		memcpy(t, *p, n);
		t += n;
	}
	changeprop_char(w, prop, type, s, len);
	free(s);
}
