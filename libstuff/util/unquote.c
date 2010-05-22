/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "util.h"

int
unquote(char *buf, char *toks[], int ntoks) {
	char *s, *t;
	bool inquote;
	int n;

	n = 0;
	s = buf;
	while(*s && n < ntoks) {
		while(*s && utfrune(" \t\r\n", *s))
			s++;
		inquote = false;
		toks[n] = s;
		t = s;
		while(*s && (inquote || !utfrune(" \t\r\n", *s))) {
			if(*s == '\'') {
				if(inquote && s[1] == '\'')
					*t++ = *s++;
				else
					inquote = !inquote;
			}
			else
				*t++ = *s;
			s++;
		}
		if(*s)
			s++;
		*t = '\0';
		if(s != toks[n])
			n++;
	}
	return n;
}
