/* Copyright ©2010 Kris Maglione <maglione.k at Gmail>
 * Copyright ©2002 by Lucent Technologies.
 * See LICENSE file for license details.
 */
#include "fmtdef.h"

static bool
pad(Fmt *f, int len) {
	if(f->flags & FmtWidth) {
		if(f->runes)
			return __rfmtpad(f, f->width - len) >= 0;
		return __fmtpad(f, f->width - len) >= 0;
	}
	return true;
}

int
localefmt(Fmt *f) {
	mbstate_t state;
	Rune *rp, *rend;
	char *sp, *send, *str, *end;
	Rune r;
	wchar_t w;
	int res, count, rlen;

	str = va_arg(f->args, char*);

	if(utf8locale()) {
		/* We handle precision in bytes, fmtstrcpy in characters */
		if(f->flags & FmtPrec)
			f->prec = utfnlen(str, f->prec);
		return fmtstrcpy(f, str);
	}

	end = 0;
	if(f->flags & FmtPrec)
		end = str + f->prec;

	if(!(f->flags & FmtLeft) && !pad(f, localelen(str, end)))
		return -1;

	sp = f->to;
	send = f->stop;
	rp = (Rune*)f->to;
	rend = (Rune*)f->stop;

	count = 0;
	for(state = (mbstate_t){0}; ; str += res) {
		switch((res = mbrtowc(&w, str, end ? end - str : MB_LEN_MAX, &state))) {
		case 0:
		case -2:
			break;
		case -1:
			w = Runesync;
			res = 1;
			/* Fallthrough. */
		default:
			count++;
			if(w > Runemax)
				w = Runesync;
			r = w;
			// print("%d %C\n", res, r);
			if(f->runes) {
				if(rp >= rend) {
					// print("flush\n");
					rp = (Rune*)__fmtflush(f, rp, sizeof *rp);
					rend = (Rune*)f->stop;
					if(rp == nil)
						return -1;
				}
				*rp++ = r;
			}else {
				if(sp + UTFmax > send && sp + (rlen = runelen(r)) > send) {
					// print("flush %d\n", rlen);
					sp = __fmtflush(f, sp, rlen);
					send = f->stop;
					if(sp == nil)
						return -1;
				}
				if(r < Runeself)
					*sp++ = (char)r;
				else
					sp += runetochar(sp, &r);
			}
			continue;
		}
		if(f->runes) {
			f->nfmt += rp - (Rune*)f->to;
			f->to = (char*)rp;
		}else {
			f->nfmt += sp - (char*)f->to;
			f->to = sp;
		}
		break;
	}

	if((f->flags & FmtLeft) && !pad(f, count))
		return -1;

	return 0;
}

void
localefmtinstall(void) {
	fmtinstall('L', localefmt);
}

