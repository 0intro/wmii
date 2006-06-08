/*
 * (C)opyright MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include "wm.h"

/* basic rule matching language /regex/ -> value
 * regex might contain POSIX regex syntax defined in regex(3) */
enum {
	IGNORE,
	REGEX,
	VALUE
};

void
update_rules(Rule **rule, const char *data)
{
	int mode = IGNORE;
	Rule *rul;
	char *p, *r = nil, *v = nil, regex[256], value[256];

	if(!data || !strlen(data))
		return;

	while((rul = *rule)) {
		*rule = rul->next;
		regfree(&rul->regex);
		free(rul);
	}

	for(p = (char *)data; *p; p++)
		switch(mode) {
		case IGNORE:
			if(*p == '/') {
				mode = REGEX;
				r = regex;
			}
			else if(*p == '>') {
				mode = VALUE;
				value[0] = 0;
				v = value;
			}
			break;
		case REGEX:
			if(*p == '/') {
				mode = IGNORE;
				*r = 0;
			}
			else {
				*r = *p;
				r++;
			}
			break;
		case VALUE:
			if(*p == '\n' || *p == 0) {
				*rule = cext_emallocz(sizeof(Rule));
				*v = 0;
				cext_trim(value, " \t/");
				if(!regcomp(&(*rule)->regex, regex, 0)) {
					cext_strlcpy((*rule)->value, value, sizeof(rul->value));
					rule = &(*rule)->next;
				}
				else
					free(*rule);
				mode = IGNORE;
			}
			else {
				*v = *p;
				v++;
			}
			break;
		}
}
