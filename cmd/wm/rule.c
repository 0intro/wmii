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

VECTOR(PropVector, char *);

static RuleVector rule;

static Vector *
vector_of_rules(RuleVector *rv)
{
	return (Vector *) rv;
}

static Vector *
vector_of_props(PropVector *pv)
{
	return (Vector *) pv;
}

Bool
permit_tags(const char *tags)
{
	static char *exclude[] = { "sel", "status" };
	char buf[256];
	char *toks[16];
	unsigned int i, j, n;

	cext_strlcpy(buf, tags, sizeof(buf));
	if(!(n = cext_tokenize(toks, 16, buf, '+')))
		return False;
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
				cext_trim(tags, " \t/");
				if(permit_tags(tags)) {
					Rule *rul = cext_emallocz(sizeof(Rule));
					rul->is_valid = !regcomp(&rul->regex, regex, 0);
					cext_strlcpy(rul->tags, tags, sizeof(rul->tags));
					cext_vattach(vector_of_rules(&rule), rul);
				}
				else
					fprintf(stderr, "wmiiwm: ignoring rule with tags '%s', restricted tag name\n",
							tags);
				mode = IGNORE;
			}
			else {
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
match(Client *c, PropVector prop)
{
	unsigned int i,j;
	regmatch_t tmpregm;

	for(i = 0; i < rule.size; i++) {
		Rule *r = rule.data[i];
		for(j=0; j < prop.size; j++)
			if(r->is_valid && !regexec(&r->regex, prop.data[j], 1, &tmpregm, 0)) {
				if(!strncmp(r->tags, "~", 2))
					c->floating = True;
				else if(!strlen(c->tags) || !strncmp(c->tags, "nil", 4)) {
					if(!strncmp(r->tags, "!", 2)) {
						if(view.size)
							cext_strlcpy(c->tags, view.data[sel]->name, sizeof(c->tags));
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
	PropVector prop = {0};

	if(!def.rules)
		goto Fallback;

	cext_vattach(vector_of_props(&prop), c->classinst);
	cext_vattach(vector_of_props(&prop), c->name);

	match(c, prop);

	while(prop.size)
		cext_vdetach(vector_of_props(&prop), prop.data[0]);

Fallback:
	if(!strlen(c->tags))
		cext_strlcpy(c->tags, "nil", sizeof(c->tags));
}
