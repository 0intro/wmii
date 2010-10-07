#include "dat.h"
#include <assert.h>
#include <bio.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "fns.h"

static void
splice(Item *i) {
	if(i->next != nil)
		i->next->prev = i->prev;
	if(i->prev != nil)
		i->prev->next = i->next;
}

char*
history_search(int dir, char *string, int n) {
	Item *i;

	if(dir == FORWARD) {
		if(histsel == &hist)
			return hist.string;
		for(i=histsel->next; i != hist.next; i=i->next)
			if(!i->string || !compare(i->string, string, n)) {
				histsel = i;
				return i->string;
			}
		return string;
	}
	assert(dir == BACKWARD);

	if(histsel == &hist) {
		free(hist.string);
		hist.string = estrdup(input.string);
	}

	for(i=histsel->prev; i != &hist; i=i->prev)
		if(!compare(i->string, string, n)) {
			histsel = i;
			return i->string;
		}
	return string;
}

void
history_dump(const char *path, int max) {
	static char *items[20];
	static char *tmp;
	Biobuf b;
	Item *h, *first;
	int i, n, fd;

	SET(first);
	if(fork() != 0)
		return;

	tmp = smprint("%s.XXXXXX", path);
	fd = mkstemp(tmp);
	if(fd < 0) {
		fprint(2, "%s: Can't create temporary history file %q: %r\n", argv0, path);
		_exit(1);
	}

	hist.string = input.string;
	n = 0;
	hist.next->prev = nil;
	for(h=&hist; h; h=h->prev) {
		for(i=0; i < nelem(items); i++)
			if(items[i] && !strcmp(h->string, items[i])) {
				splice(h);
				goto next;
			}
		items[n++ % nelem(items)] = h->string;
		first = h;
		if(!max || n >= max)
			break;
	next:
		continue;
	}

	Binit(&b, fd, OWRITE);
	hist.next = nil;
	for(h=first; h; h=h->next)
		if(Bprint(&b, "%s\n", h->string) < 0) {
			unlink(tmp);
			fprint(2, "%s: Can't write temporary history file %q: %r\n", argv0, path);
			_exit(1);
		}
	Bterm(&b);
	rename(tmp, path);
	_exit(0);
}

