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
	regex_t regex;
	char tag[MAX_TAGS][MAX_TAGLEN];
	unsigned int ntag;
	Bool is_valid;
} Rule;

static Rule *rule = nil;
static unsigned int rulesz = 0;
static unsigned int nrule = 0;

enum {
	IGNORE,
	REGEX,
	TAGS
};

void
update_rules()
{
	unsigned int i;
	int mode = IGNORE;
	char *p, *r=nil, *t=nil, regex[256], tags[256];

	if(!def.rules || !strlen(def.rules))
		return;

	for(i = 0; i < nrule; i++)
		if(rule[i].is_valid) {
			regfree(&rule[i].regex);
			rule[i].is_valid = False;
		}

	nrule = 0;
	for(p = def.rules; *p; p++)
		if(*p == '\n')
			nrule++;

	if(nrule > rulesz) {
		if(rule)
			free(rule);
		rule = cext_emallocz(sizeof(Rule) * nrule);
		rulesz = nrule;
	}

	i = 0;
	for(p = def.rules; *p; p++)
		switch(mode) {
		case IGNORE:
			if(*p == '/') {
				mode = REGEX;
				r = regex;
			}
			else if(*p == '>') {
				mode = TAGS;
				tags[0] = 0;
				t = tags;
			}
			break;
		case REGEX:
			if(*p == '/') {
				mode = IGNORE;
				*r = 0;
				rule[i].is_valid = !regcomp(&rule[i].regex, regex, 0);
				/* Is there a memory leak here if the rule is invalid? */
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
}


static void
match(Client *c, const char *prop)
{
	unsigned int i, j;
	regmatch_t tmpregm;

	c->ntag = 0;
	for(i = 0; i < nrule; i++) {
		Rule *r = &rule[i];
		if(r->is_valid && !regexec(&r->regex, prop, 1, &tmpregm, 0)) {
			for(j = 0; c->ntag < MAX_TAGS && j < r->ntag; j++) {
				if(!strncmp(r->tag[j], "~", 2))
					c->floating = True;
				else {
					cext_strlcpy(c->tag[c->ntag], r->tag[j], sizeof(c->tag[c->ntag]));
					c->ntag++;
				}
			}
		}
	}
}

void
match_tags(Client *c)
{
	if(!def.rules)
		return;
	match(c, c->name);
	match(c, c->classinst);
}
