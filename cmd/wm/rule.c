/*
 * (C)opyright MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include "wm.h"

/* basic rule matching language /regex/ -> value
 * regex might contain POSIX regex syntax defined in regex(3) */
enum {
	IGNORE,
	REGEX,
	TAGS
};

typedef struct {
	regex_t regex;
	char values[256];
} Rule;
VECTOR(RuleVector, Rule *);

static RuleVector rule;

static Vector *
vector_of_rules(RuleVector *rv)
{
	return (Vector *) rv;
}

static Bool
permit_tag(const char *tag)
{
	static char *exclude[] = { "sel", "status" };
	unsigned int i;
	for(i = 0; i < (sizeof(exclude) / sizeof(exclude[0])); i++)
		if(!strcmp(exclude[i], tag))
			return False;
	return True;
}

void
update_rules()
{
	unsigned int i;
	int mode = IGNORE;
	char *p, *r = nil, *v = nil, regex[256], values[256];

	if(!def.rules || !strlen(def.rules))
		return;

	while(rule.size) {
		Rule *r = rule.data[0];
		regfree(&r->regex);
		cext_vdetach(vector_of_rules(&rule), r);
		free(r);
	}

	for(p = def.rules; *p; p++)
		switch(mode) {
		case IGNORE:
			if(*p == '/') {
				mode = REGEX;
				r = regex;
			}
			else if(*p == '>') {
				mode = TAGS;
				values[0] = 0;
				v = values;
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
		case TAGS:
			if(*p == '\n' || *(p + 1) == 0) {
				Rule *rul = cext_emallocz(sizeof(Rule));
				*v = 0;
				cext_trim(values, " \t/");
				if(!regcomp(&rul->regex, regex, 0)) {
					cext_strlcpy(rul->values, values, sizeof(rul->values));
					cext_vattach(vector_of_rules(&rule), rul);
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
	for(i = 0; i < client.size; i++)
		apply_rules(client.data[i]);
	update_views();
}

void
apply_tags(Client *c, const char *tags)
{
	unsigned int i, j = 0, n;
	char buf[256];
	char *toks[16], *apply[16];

	cext_strlcpy(buf, tags, sizeof(buf));
	if(!(n = cext_tokenize(toks, 16, buf, '+')))
		return;

	for(i = 0; i < n; i++) {
		if(!strncmp(toks[i], "~", 2))
			c->floating = True;
		else if(!strncmp(toks[i], "!", 2)) {
			if(view.size)
				apply[j++] = view.data[sel]->name;
			else
				apply[j++] = "nil";
		}
		else if(permit_tag(toks[i]))
			apply[j++] = toks[i];
	}

	c->tags[0] = 0;
	for(i = 0; i < j; i++) {
		cext_strlcat(c->tags, apply[i], sizeof(c->tags) - strlen(c->tags) - 1);
		if(i + 1 < j)
			cext_strlcat(c->tags, "+", sizeof(c->tags) - strlen(c->tags) - 1);
	}

	if(!strlen(c->tags))
		apply_tags(c, "nil");
}

static void
match(Client *c, const char *prop)
{
	unsigned int i;
	regmatch_t tmpregm;

	for(i = 0; i < rule.size; i++) {
		Rule *r = rule.data[i];
		if(!regexec(&r->regex, prop, 1, &tmpregm, 0))
			if(!strlen(c->tags) || !strncmp(c->tags, "nil", 4))
				apply_tags(c, r->values);
	}
}

void
apply_rules(Client *c)
{
	if(!def.rules)
		goto Fallback;

	match(c, c->props);

Fallback:
	if(!strlen(c->tags))
		apply_tags(c, "nil");
}
