/* Copyright ©2006 Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include "dat.h"
#include <assert.h>
#include "fns.h"

/* basic rule matching language /regex/ -> value
 * regex might contain POSIX regex syntax defined in regex(3) */
enum {
	IGNORE,
	REGEX,
	VALUE
};

void
trim(char *str, const char *chars) {
	const char *cp;
	char *p, *q;
	char c;

	for(cp = chars; (c = *cp); cp++) {
		q = str;
		for(p = q; *p; p++)
			if(*p != c)
				*q++ = *p;
		*q = '\0';
	}
}

void
update_rules(Rule **rule, const char *data) {
	int state = IGNORE;
	Rule *rul;
	char regex[256], value[256];
	char *r, *v;
	const char *p;
	
	SET(r);
	SET(v);

	if(!data || !strlen(data))
		return;
	while((rul = *rule)) {
		*rule = rul->next;
		free(rul->regex);
		free(rul);
	}
	for(p = data; *p; p++)
		switch(state) {
		case IGNORE:
			if(*p == '/') {
				r = regex;
				state = REGEX;
			}
			else if(*p == '>') {
				value[0] = 0;
				v = value;
				state = VALUE;
			}
			break;
		case REGEX:
			if(*p == '/') {
				*r = 0;
				state = IGNORE;
			}
			else
				*r++ = *p;
			break;
		case VALUE:
			if(*p == '\n' || *p == 0) {
				*rule = emallocz(sizeof(Rule));
				*v = 0;
				trim(value, " \t/");
				(*rule)->regex = regcomp(regex);
				if((*rule)->regex) {
					utflcpy((*rule)->value, value, sizeof(rul->value));
					rule = &(*rule)->next;
				}
				else free(*rule);
				state = IGNORE;
			}
			else
				*v++ = *p;
			break;
		default: /* can't happen */
			die("invalid state");
		}
}
