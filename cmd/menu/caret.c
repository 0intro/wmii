#include "dat.h"
#include <ctype.h>
#include <string.h>
#include "fns.h"

static int
iswordrune(Rune r) {
	if(isalpharune(r))
		return 1;
	return r < 0x80 && (r == '_' || isdigit(r));
}

static char*
prev_rune(char *start, char *p, Rune *r) {

	*r = 0;
	if(p == start)
		return p;
	while(p > start && (*(--p)&0xC0) == 0x80)
		;
	chartorune(r, p);
	return p;
}

static char*
next_rune(char *p, Rune *r) {
	int i;

	*r = 0;
	if(!*p)
		return p;
	i = chartorune(r, p);
	return p + i;
}

void
caret_set(int start, int end) {
	int len;

	len = input.end - input.string;
	start = max(0, min(len, start));

	input.pos = input.string + start;
	if(end < 0)
		input.pos_end = nil;
	else
		input.pos_end = input.string + max(start, end);
}

char*
caret_find(int dir, int type) {
	char *end;
	char *next, *p;
	Rune r;
	int res;

	p = input.pos;
	if(dir == FORWARD) {
		end = input.end;
		switch(type) {
		case LINE:
			return end;
		case WORD:
			chartorune(&r, p);
			res = iswordrune(r);
			while(next=next_rune(p, &r), r && iswordrune(r) == res && !isspacerune(r))
				p = next;
			while(next=next_rune(p, &r), r && isspacerune(r))
				p = next;
			return p;
		case CHAR:
			return next_rune(p, &r);
		}
	}
	else if(dir == BACKWARD) {
		end = input.string;
		switch(type) {
		case LINE:
			return end;
		case WORD:
			while(next=prev_rune(end, p, &r), r && isspacerune(r))
				p = next;
			prev_rune(end, p, &r);
			res = iswordrune(r);
			while(next=prev_rune(end, p, &r), r && iswordrune(r) == res && !isspacerune(r))
				p = next;
			return p;
		case CHAR:
			return prev_rune(end, p, &r);
		}
	}
	input.pos_end = nil;
	return input.pos;
}

void
caret_move(int dir, int type) {
	input.pos = caret_find(dir, type);
	input.pos_end = nil;
}

void
caret_delete(int dir, int type) {
	char *pos, *p;
	int n;

	if(input.pos_end)
		p = input.pos_end;
	else
		p = caret_find(dir, type);
	pos = input.pos;
	if(p == input.end)
		input.end = pos;
	else {
		if(p < pos) {
			pos = p;
			p = input.pos;
		}
		n = input.end - p;
		memmove(pos, p, n);
		input.pos = pos;
		input.end = pos + n;
	}
	*input.end = '\0';
	input.pos_end = nil;
}

void
caret_insert(char *s, bool clear) {
	int pos, end, len, size;

	if(s == nil)
		return;
	if(clear) {
		input.pos = input.string;
		input.end = input.string;
	}else if(input.pos_end)
		caret_delete(0, 0);

	len = strlen(s);
	pos = input.pos - input.string;
	end = input.end - input.string;

	size = input.size;
	if(input.size == 0)
		input.size = 1;
	while(input.size < end + len + 1)
		input.size <<= 2;
	if(input.size != size)
		input.string = erealloc(input.string, input.size);

	input.pos = input.string + pos;
	input.end = input.string + end + len;
	*input.end = '\0';
	memmove(input.pos + len, input.pos, end - pos);
	memmove(input.pos, s, len);
	input.pos += len;
	input.pos_end = nil;
}

