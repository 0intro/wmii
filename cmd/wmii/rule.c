/* Copyright ©2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */

#include "dat.h"
#include "fns.h"

void
update_rules(Rule **rule, char *data) {
#define putc(m, c) BLOCK(if((m)->pos < (m)->end) *(m)->pos++ = c;)
#define getc(m) ((m)->pos < (m)->end ? *(m)->pos++ : 0)
#define ungetc(m) BLOCK(if((m)->pos > (m)->data) --(m)->pos)

	IxpMsg buf, valuebuf, rebuf;
	Reprog *re;
	Rule *r;
	Ruleval **rvp;
	Ruleval *rv;
	char *w;
	char regexp[256];
	char c;
	int len;

	while((r = *rule)) {
		*rule = r->next;
		while((rv = r->values)) {
			r->values = rv->next;
			free(rv);
		}
		free(r->regex);
		free(r->value);
		free(r);
	}

	if(!data || !data[0])
		return;

	buf = ixp_message(data, strlen(data), MsgUnpack);

begin:
	msg_eatrunes(&buf, isspacerune, true);
	if(getc(&buf) == '/')
		goto regexp;
	/* Regexp not at begining of the line. Rest of the line is junk. */
	while((c = getc(&buf)))
		if(c == '\n')
			goto begin;
	goto done;

regexp:
	rebuf = ixp_message(regexp, sizeof regexp - 1, MsgPack);
	while((c = getc(&buf)))
		if(c == '/')
			goto value;
		else if(c != '\\')
			putc(&rebuf, c);
		else if(buf.pos[1] == '/' || buf.pos[1] == '\\' && buf.pos[2] == '/')
			putc(&rebuf, getc(&buf));
		else {
			putc(&rebuf, c);
			putc(&rebuf, getc(&buf));
		}
	goto done;

value:
	valuebuf = ixp_message(buffer, sizeof buffer - 1, MsgPack);
	while((c = getc(&buf))) {
		if(c == '\n') {
			putc(&valuebuf, ' ');
			msg_eatrunes(&buf, isspacerune, true);
			if((c = getc(&buf)) == '/') {
				ungetc(&buf);
				break;
			}
		}
		putc(&valuebuf, c);
	}

	putc(&rebuf, '\0');
	re = regcomp(regexp);
	if(!re)
		goto begin;
	r = emallocz(sizeof *r);
	*rule = r;
	rule = &r->next;
	r->regex = re;

	valuebuf.end = valuebuf.pos;
	valuebuf.pos = valuebuf.data;
	rvp = &r->values;
	while((w = msg_getword(&valuebuf))) {
		free(r->value);
		r->value = estrdup(w);
		if(strchr(w, '=')) {
			len = strlen(w) + 1;
			*rvp = rv = emallocz(sizeof *rv + len);
			rvp = &rv->next;

			memcpy(&rv[1], w, len);
			tokenize(&rv->key, 2, (char*)&rv[1], '=');
		}
	}
	goto begin;

done:
	return;
}

