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

enum {
	IGNORE,
	REGEX,
	TAGS
};

typedef struct {
	regex_t regex;
	char tags[256];
	Bool is_valid;
} Rule;
VECTOR(RuleVector, Rule *);

static RuleVector rule;

static Vector *
rule2vector(RuleVector *rv)
{
	return (Vector *) rv;
}

Bool
permit_tags(const char *tags)
{
	static char *exclude[]
		= { "bar", "client", "ctl", "def", "event", "view" };
	char buf[256];
	char *toks[16];
	unsigned int i, j, n;

	cext_strlcpy(buf, tags, sizeof(buf));
	n = cext_tokenize(toks, 16, buf, '+');
	for(i = 0; i < (sizeof(exclude)/sizeof(exclude[0])); i++)
		for(j = 0; j < n; j++) {
			if(!strncmp(exclude[i], toks[j], strlen(toks[j])) &&
				!strncmp(exclude[i], toks[j], strlen(exclude[i])))
				return False;
		}
	return True;
}

void
update_rules()
{
	unsigned int i;
	int mode = IGNORE;
	char *p, *r = nil, *t = nil, regex[256], tags[256];

	if(!def.rules || !strlen(def.rules))
		return;

	while(rule.size) {
		Rule *r = rule.data[0];
		if(r->is_valid)
			regfree(&r->regex);
		cext_vdetach(rule2vector(&rule), r);
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
				tags[0] = 0;
				t = tags;
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
				*t = 0;
				if(permit_tags(tags)) {
					Rule *rul = cext_emallocz(sizeof(Rule));
					rul->is_valid = !regcomp(&rul->regex, regex, 0);
					cext_strlcpy(rul->tags, tags, sizeof(rul->tags));
					cext_vattach(rule2vector(&rule), rul);
				}
				else
					fprintf(stderr, "wmiiwm: ignoring rule with tags '%s', restricted tag name\n",
							tags);
				mode = IGNORE;
			}
			else {
				if((*p == ' ' || *p == '\t') && (tags[0] == 0))
					continue; /* skip prefixed whitespaces */
				*t = *p;
				t++;
			}
			break;
		}
	for(i = 0; i < client.size; i++)
		apply_rules(client.data[i]);
	update_views();
}


static void
match(Client *c, const char *prop)
{
	unsigned int i;
	regmatch_t tmpregm;

	for(i = 0; i < rule.size; i++) {
		Rule *r = rule.data[i];
		if(r->is_valid && !regexec(&r->regex, prop, 1, &tmpregm, 0)) {
			if(!strncmp(r->tags, "~", 2))
				c->floating = True;
			else if(!c->view.size || !strncmp(c->tags, "nil", 4)) {
				if(!strncmp(r->tags, "!", 2)) {
					if(view.size) {
						c->tags[0] = 0;
						unsigned int j;
						for(j = 0; j < c->view.size; j++) {
							cext_strlcat(c->tags, c->view.data[j]->name, sizeof(c->tags));
							if(j + 1 < c->view.size)
								cext_strlcat(c->tags, "+", sizeof(c->tags));
						}
					}
					else
						cext_strlcpy(c->tags, "nil", sizeof(c->tags));
				}
				else
					cext_strlcpy(c->tags, r->tags, sizeof(c->tags));
			}
		}
	}
}

void
apply_rules(Client *c)
{
	if(!def.rules)
		goto Fallback;
	match(c, c->name);
	match(c, c->classinst);

Fallback:
	if(!strlen(c->tags) || (!view.size && !strncmp(c->tags, "*", 2)))
		cext_strlcpy(c->tags, "nil", sizeof(c->tags));
}
