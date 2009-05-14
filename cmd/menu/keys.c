#include "dat.h"
#include <ctype.h>
#include <strings.h>
#include <unistd.h>
#include "fns.h"

typedef struct Key Key;
typedef struct KMask KMask;

struct Key {
	Key*	next;
	long	mask;
	char*	key;
	char**	action;
};

static Key* bindings;

static struct KMask {
	int		mask;
	const char*	name;
} masks[] = {
	{ControlMask, "Control"},
	{Mod1Mask,    "Mod1"},
	{Mod2Mask,    "Mod2"},
	{Mod3Mask,    "Mod3"},
	{Mod4Mask,    "Mod4"},
	{ShiftMask,   "Shift"},
	{0,}
};

/*
 * To do: Find my red black tree implementation.
 */
void
parse_keys(char *spec) {
	static char *lines[1024];
	static char *words[16];
	static char *keys[16];
	Key *k;
	KMask *m;
	char *p, *line;
	long mask;
	int i, j, nlines, nwords, nkeys;

	nlines = tokenize(lines, nelem(lines), spec, '\n');
	for(i=0; i < nlines; i++) {
		line = lines[i];
		p = strchr(line, '#');
		if(p)
			*p = '\0';
		nwords = stokenize(words, nelem(words) - 1, line, " \t");
		words[nwords] = nil;
		if(!words[0])
			continue;
		mask = 0;
		nkeys = tokenize(keys, nelem(keys), words[0], '-');
		for(j=0; j < nkeys; j++) {
			for(m=masks; m->mask; m++)
				if(!strcasecmp(m->name, keys[j])) {
					mask |= m->mask;
					goto next;
				}
			break;
		next: continue;
		}
		if(j == nkeys - 1) {
			k = emallocz(sizeof *k);
			k->key    = keys[j];
			k->mask   = mask;
			k->action = strlistdup(words + 1);
			k->next   = bindings;
			bindings = k;
		}
	}
}

char**
find_key(char *key, long mask) {
	Key *k;

	/* Horrible hack. */
	if(!strcmp(key, "ISO_Left_Tab"))
		key = "Tab";

	mask &= ~(numlock | LockMask);
	for(k=bindings; k; k=k->next)
		if(!strcasecmp(k->key, key) && k->mask == mask)
			return k->action;
	return nil;
}

	/* sed 's/"([^"]+)"/L\1/g' | tr 'a-z' 'A-Z' */
	/* awk '{$1=""; print}' keys.txt | perl -e '$_=lc join "", <>; print join "\n", m/(\w+)/g;' | sort -u | sed 's:.*:	"&",:' */
char *symtab[] = {
	"accept",
	"backward",
	"char",
	"complete",
	"first",
	"forward",
	"history",
	"kill",
	"last",
	"line",
	"literal",
	"next",
	"nextpage",
	"prev",
	"prevpage",
	"reject",
	"word",
};

static int
_bsearch(char *s, char **tab, int ntab) {
	int i, n, m, cmp;

	if(s == nil)
		return -1;

	n = ntab;
	i = 0;
	while(n) {
		m = n/2;
		cmp = strcasecmp(s, tab[i+m]);
		if(cmp == 0)
			return i+m;
		if(cmp < 0 || m == 0)
			n = m;
		else {
			i += m;
			n = n-m;
		}
	}
	return -1;
}

int
getsym(char *s) {
	return _bsearch(s, symtab, nelem(symtab));
}

