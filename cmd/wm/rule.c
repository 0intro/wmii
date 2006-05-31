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

static Vector *
vector_of_rules(RuleVector *rv)
{
	return (Vector *) rv;
}

void
update_rules(RuleVector *rule, const char *data)
{
	int mode = IGNORE;
	char *p, *r = nil, *v = nil, regex[256], value[256];

	if(!data || !strlen(data))
		return;

	while(rule->size) {
		Rule *rul = rule->data[0];
		regfree(&rul->regex);
		cext_vdetach(vector_of_rules(rule), rul);
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
				Rule *rul = cext_emallocz(sizeof(Rule));
				*v = 0;
				cext_trim(value, " \t/");
				if(!regcomp(&rul->regex, regex, 0)) {
					cext_strlcpy(rul->value, value, sizeof(rul->value));
					cext_vattach(vector_of_rules(rule), rul);
				}
				else
					free(rul);
				mode = IGNORE;
			}
			else {
				*v = *p;
				v++;
			}
			break;
		}
}
