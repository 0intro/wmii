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
	char tag[MAX_TAGS][MAX_TAGLEN];
	unsigned int ntag;
} Rule;

enum {
	IGNORE,
	REGEX,
	TAGS
};

static Rule *
parse(char *data, unsigned int *n)
{
	static Rule *rule = nil;
	static unsigned int rulesz = 0;
	unsigned int i;
	int mode = IGNORE;
	char *p, *r, *t, regex[256], tags[256];

	if(!data || !strlen(data))
		return nil;

	*n = 0;
	for(p = data; *p; p++)
		if(*p == '\n')
			(*n)++;

	if(*n > rulesz) {
		if(rule)
			free(rule);
		rule = cext_emallocz(sizeof(Rule) * (*n));
		rulesz = *n;
	}

	i = 0;
	for(p = data; *p; p++)
		switch(mode) {
		case IGNORE:
			if(*p == '/') {
				mode = REGEX;
				r = regex;
			}
			else if(*p == '>') {
				mode = TAGS;
				t = tags;
			}
			break;
		case REGEX:
			if(*p == '/') {
				mode = IGNORE;
				*r = 0;
				cext_strlcpy(rule[i].regex, regex, sizeof(rule[i].regex));
			}
			else {
				*r = *p;
				r++;
			}
			break;
		case TAGS:
			if(*p == '\n' || *(p + 1) == 0) {
				*t = 0;
				rule[i].ntag = str2tags(rule[i].tag, tags);
				mode = IGNORE;
				i++;
			}
			else {
				if((*p == ' ' || *p == '\t') && (tags[0] == 0))
					continue; /* skip prefixed whitespaces */
				*t = *p;
				t++;
			}
			break;
		}

	return rule;
}


static void
match(Rule *rule, unsigned int rulez, Client *c, const char *prop)
{
	unsigned int i, j;
	regex_t regex;
	regmatch_t tmpregm;

	c->ntag = 0;
	for(i = 0; i < rulez && c->ntag < MAX_TAGS; i++) {
		Rule r = rule[i];
		if(!regcomp(&regex, r.regex, 0)) {
			if(!regexec(&regex, prop, 1, &tmpregm, 0)) {
				for(j = 0; c->ntag < MAX_TAGS && j < r.ntag; j++) {
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
	Rule *rule;

	if(!def.rules)
		return;

   	rule = parse(def.rules, &n);
	match(rule, n, c, c->name);
	match(rule, n, c, c->classinst);
}
