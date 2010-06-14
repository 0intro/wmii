#include "dat.h"
#include <ctype.h>
#include <strings.h>
#include <unistd.h>
#include "fns.h"

typedef struct Key Key;

struct Key {
	Key*	next;
	long	mask;
	char*	key;
	char**	action;
};

static Key* bindings;

/*
 * To do: Find my red black tree implementation.
 */
void
parse_keys(char *spec) {
	static char *lines[1024];
	static char *words[16];
	Key *k;
	char *p, *line;
	int mask;
	int i, nlines, nwords;

	if(!numlock)
		numlock = numlockmask();

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
		if(parsekey(words[0], &mask, &p)) {
			k = emallocz(sizeof *k);
			k->key    = p;
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

	mask &= ~(numlock | LockMask) & ((1<<8) - 1);
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

