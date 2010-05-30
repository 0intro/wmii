/* Copyright ©2006 Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include "dat.h"
#include "fns.h"

void
trim(char *str, const char *chars) {
	const char *cp;
	char *p, *q;
	char c;

	q = str;
	for(p=str; *p; p++) {
		for(cp=chars; (c = *cp); cp++)
			if(*p == c)
				break;
		if(c == '\0')
			*q++ = *p;
	}
	*q = '\0';
}

void
update_rules(Rule **rule, const char *data) {
#define putc(m, c) BLOCK(if(m.pos < m.end) *m.pos++ = c)
#define getc(m) (m.pos < m.end ? *m.pos++ : 0)
#define ungetc(m) BLOCK(if(m.pos > m.data) --m.pos)
	IxpMsg buf, outbuf, rebuf;
	char regex[256];
	char c;

	buf = ixp_message(data, strlen(data), MsgUnpack);
}

/* XXX: I hate this. --KM */
void
update_rules(Rule **rule, const char *data) {
	enum {
		COMMENT,
		IGNORE,
		REGEX,
		VALUE,
		WAIT,
	};
	int state, old_state;
	IxpMsg m;
	Ruleval **rvp;
	Ruleval *rv;
	Rule *rul;
	char regex[256];
	char *regex_end = regex + sizeof(regex) - 1;
	char *value_end = buffer + sizeof(buffer) - 1;
	char *r, *v;
	const char *p;
	char c;
	int len;

#define NEXT(next) BLOCK(		\
		old_state = state	\
		state = next;		\
		continue;		\
	)

	SET(r);
	SET(v);
	SET(old_state);

	if(!data || !strlen(data))
		return;
	while((rul = *rule)) {
		*rule = rul->next;
		while((rv = rul->values)) {
			rul->values = rv->next;
			free(rv);
		}
		free(rul->regex);
		free(rul);
	}
	state = IGNORE;
	for(p = data; (c = *p); p++)
		switch(state) {
		case COMMENT:
			if(c == '\n')
				state = old_state;
			break;
		case IGNORE:
			if(c == '#')
				goto comment;
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
			else if(c == '\\' && p[1] == '\\' && p[2] == '/')
				p++;
			else if(c == '\\' && r < regex_end)
				*r++ = *p++;
			else if(c == '/') {
				*r = 0;
				state = IGNORE;
				break;
			}
			if(r < regex_end)
				*r++ = c;
			break;
		case VALUE:
			if(c == '#')
				NEXT(COMMENT);
			if(c == '\n')
				NEXT(WAIT);
			if(v < value_end)
				*v++ = c;
			break;
		case WAIT:
			if(c == '#')
				NEXT(COMMENT);
			if(c == '/') {
				state = 
			break;
		default: /* can't happen */
			die("invalid state");
		accept:

			*v = 0;
			*rule = emallocz(sizeof **rule);
			(*rule)->regex = regcomp(regex);
			if((*rule)->regex) {
				(*rule)->value = strdup(buffer);
				trim((*rule)->value, " \t");

				rvp = &(*rule)->values;
				m = ixp_message(buffer, v - buffer, MsgUnpack);
				while((r = msg_getword(&m)))
					if((v = strchr(r, '='))) {
						len = strlen(r) + 1;
						*rvp = rv = emalloc(sizeof *rv + len);
						rvp = &rv->next;

						memcpy(&rv[1], r, len);
						tokenize(&rv->key, 2, (char*)&rv[1], '=');
					}


				rule = &(*rule)->next;
			}else
				free(*rule);

			r = regex;

			break;
		}
}
