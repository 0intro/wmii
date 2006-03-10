/*
 * (C)opyright MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>
#include <stdlib.h>
#include "wm.h"

/* 
 * basic rule matching language
 *
 * /pattern/ -> tag [tag ...] 
 */

typedef struct {
	char pattern[256];
	char tags[256];
} Rule;

enum {
	IGNORE,
	PATTERN,
	TAGS
};

/* free the result */
static Rule *
parse(char *data, unsigned int *n)
{
	Rule *rules;
	unsigned int i;
	int mode = IGNORE;
	char *last = nil, *p, *pattern, *tags;

	if(!data || !strlen(data))
		return nil;

	*n = 0;
	for(last = p = data; *p; p++)
		if(*p == '\n')
			*n++;

	rules = cext_emallocz(sizeof(Rule) * (*n));

	i = 0;
	for(p = data; *p; p++) {
		switch(mode) {
			case IGNORE:
				if(*p == '/') {
					mode = PATTERN;
					pattern = rules[i].pattern;
				}
				else if(*p == '>' && last && *last == '-') {
					mode = TAGS;
					tags = rules[i].tags;
				}
				break;
			case PATTERN:
				if(*p == '/') {
					mode = IGNORE;
					*pattern = 0;
				}
				else {
					*pattern = *p;
					pattern++;
				}
				break;
			case TAGS:
				if(*p == ' ' && !strlen(tags))
					break;
				else if(*p == '\n') {
					*tags = 0;
					mode = IGNORE;
					i++;
				}
				else {
					*tags = *p;
					tags++;
				}
				break;
		}
		last = p;
	}

	return rules;
}

char *
match_tags(char *ruledef, Client *c)
{
	unsigned int n;
	Rule *rules = parse(ruledef, &n);



	free(rules);
	return nil;
}
