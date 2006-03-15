/*
 * (C)opyright MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include "wm.h"

/* 
 * basic rule matching language
 *
 * /regex/ -> tag [tag ...] 
 *
 * regex might contain POSIX regex syntax defined in regex(3)
 */

typedef struct {
	char regex[256];
	char tag[8][32];
	unsigned int ntag;
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
	char *p, *regex, *tags;

	if(!data || !strlen(data))
		return nil;

	*n = 0;
	for(p = data; *p; p++)
		if(*p == '\n')
			(*n)++;

	rules = cext_emallocz(sizeof(Rule) * (*n));

	i = 0;
	for(p = data; *p; p++)
		switch(mode) {
		case IGNORE:
			if(*p == '/') {
				mode = PATTERN;
				regex = rules[i].regex;
			}
			else if(*p == '>') {
				mode = TAGS;
				tags = rules[i].tag[0];
			}
			break;
		case PATTERN:
			if(*p == '/') {
				mode = IGNORE;
				*regex = 0;
			}
			else {
				*regex = *p;
				regex++;
			}
			break;
		case TAGS:
			if(*p == '\n' || *(p + 1) == 0) {
				*tags = 0;
				mode = IGNORE;
				i++;
			}
			else {
				if(*p == ' ' || *p == '\t') {
					if(*tags == 0)
						continue; /* skip prefixed whitespaces */
					*tags = 0;
					tags = rules[i].tag[++rules[i].ntag];
				}
				else
					*tags = *p;
				tags++;
			}
			break;
		}

	return rules;
}


static void
match(Rule *rule, unsigned int rulesz, Client *c, const char *prop)
{
	unsigned int i, j;
	regex_t regex;
	regmatch_t tmpregm;

	c->ntag = 0;
	for(i = 0; i < rulesz && c->ntag < 8; i++) {
		Rule r = rule[i];
		if(!regcomp(&regex, r.regex, 0)) {
			if(!regexec(&regex, prop, 1, &tmpregm, 0)) {
				for(j = 0; c->ntag < 8 && j < r.ntag; j++) {
					cext_strlcpy(c->tag[c->ntag], r.tag[j], sizeof(c->tag[c->ntag]));
					c->ntag++;
				}
				regfree(&regex);
			}
		}
	}
}

void
match_tags(Client *c)
{
	unsigned int n;
	Rule *rules;

	if(!def.rules)
		return;

   	rules = parse(def.rules, &n);
	match(rules, n, c, c->name);
	match(rules, n, c, c->classinst);
	free(rules);
}
