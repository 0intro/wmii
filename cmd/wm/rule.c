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
				tags = rules[i].tags;
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
				*tags = *p;
				tags++;
			}
			break;
		}

	return rules;
}


static char *
match(Rule *rule, unsigned int nrule, char *prop)
{
	unsigned int i;
	regex_t regex;
	regmatch_t tmpregm;
	static char result[256];

	result[0] = 0;	
	for(i = 0; i < nrule; i++) {
		Rule r = rule[i];
		if(!regcomp(&regex, r.regex, 0)) {
			if(!regexec(&regex, prop, 1, &tmpregm, 0))
				cext_strlcat(result, r.tags, sizeof(result));
			regfree(&regex);
		}
	}
	return result;
}

void
match_tags(Client *c)
{
	unsigned int n;
	Rule *rules;
	char *tags;

	if(!def.rules)
		return;

   	rules = parse(def.rules, &n);
	c->tags[0] = 0;
	tags = match(rules, n, c->name);
	fprintf(stderr, "match_tags tags=%s c->name=%s\n", tags, c->name);
	if(strlen(tags))
		cext_strlcat(c->tags, tags, sizeof(c->tags));
	tags = match(rules, n, c->classinst);
	fprintf(stderr, "match_tags tags=%s c->name=%s\n", tags, c->name);
	if(strlen(tags))
		cext_strlcat(c->tags, tags, sizeof(c->tags));

	free(rules);
}
