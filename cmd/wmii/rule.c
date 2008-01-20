/* Copyright ©2006 Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include "dat.h"
#include <assert.h>
#include "fns.h"

void
trim(char *str, const char *chars) {
	const char *cp;
	char *p, *q;
	char c;

	q = str;
	for(p = str; *p; p++) {
		for(cp = chars; (c = *cp); cp++)
			if(*p == c)
				goto nextchar;
		*q++ = *p;
	nextchar:
		continue;
	}
	*q = '\0';
}

/* XXX: I hate this. --KM */
void
update_rules(Rule **rule, const char *data) {
	/* basic rule matching language /regex/ -> value
	 * regex might contain POSIX regex syntax defined in regex(3) */
	enum {
		IGNORE,
		REGEX,
		VALUE,
		COMMENT,
	};
	int state;
	Rule *rul;
	char regex[256], value[256];
	char *regex_end = regex + sizeof(regex) - 1;
	char *value_end = value + sizeof(value) - 1;
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
	state = IGNORE;
	for(p = data; (c = *p); p++)
		switch(state) {
		case IGNORE:
			if(c == '#')
				state = COMMENT;
			else if(c == '/') {
				r = regex;
				state = REGEX;
			}
			else if(c == '>') {
				value[0] = 0;
				v = value;
				state = VALUE;
			}
			break;
		case REGEX:
			if(c == '\\' && p[1] == '/')
				p++;
			else if(c == '/') {
				*r = 0;
				state = IGNORE;
			}
			if(r < regex_end)
				*r++ = c;
			break;
		case VALUE:
			if(c == '\n' || c == '#' || c == 0) {
				*v = 0;
				trim(value, " \t/");
				*rule = emallocz(sizeof *rule);
				(*rule)->regex = regcomp(regex);
				if((*rule)->regex) {
					utflcpy((*rule)->value, value, sizeof(rul->value));
					rule = &(*rule)->next;
				}else
					free(*rule);
				state = IGNORE;
				if(c == '#')
					state = COMMENT;
			}
			else if(v < value_end)
				*v++ = c;
			break;
		default: /* can't happen */
			die("invalid state");
		}
}
