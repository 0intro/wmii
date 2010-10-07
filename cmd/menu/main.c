/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#define EXTERN
#include "dat.h"
#include <X11/Xproto.h>
#include <locale.h>
#include <strings.h>
#include <unistd.h>
#include <bio.h>
#include <stuff/clientutil.h>
#include "fns.h"
#define link _link

static const char version[] = "wimenu-"VERSION", "COPYRIGHT"\n";
static Biobuf*	cmplbuf;
static Biobuf*	inbuf;
static bool	alwaysprint;
static char*	cmdsep;
static int	screen_hint;

static void
usage(void) {
	fprint(2, "usage: %s -i [-a <address>] [-h <history>] [-p <prompt>] [-r <rows>] [-s <screen>]\n", argv0);
	fprint(2, "       See manual page for full usage details.\n");
	exit(1);
}

static int
errfmt(Fmt *f) {
	return fmtstrcpy(f, ixp_errbuf());
}

static inline void
splice(Item *i) {
	i->next->prev = i->prev;
	i->prev->next = i->next;
}
static inline void
link(Item *i, Item *j) {
	i->next = j;
	j->prev = i;
}

static Item*
populate_list(Biobuf *buf, bool hist) {
	Item ret;
	Item *i;
	char *p;
	bool stop;

	stop = !hist && !isatty(buf->fid);
	ret.next_link = nil;
	i = &ret;
	while((p = Brdstr(buf, '\n', true))) {
		if(stop && p[0] == '\0')
			break;
		i->next_link = emallocz(sizeof *i);
		i = i->next_link;
		i->string = p;
		i->retstring = p;
		if(cmdsep && (p = strstr(p, cmdsep))) {
			*p = '\0';
			i->retstring = p + strlen(cmdsep);
		}
		if(!hist) {
			i->len = strlen(i->string);
			i->width = textwidth_l(font, i->string, i->len) + itempad;
			match.maxwidth = max(i->width, match.maxwidth);
		}
	}

	return ret.next_link;
}

static void
check_competions(IxpConn *c) {
	char *s;

	s = Brdstr(cmplbuf, '\n', true);
	if(!s) {
		ixp_hangup(c);
		return;
	}
	input.filter_start = strtol(s, nil, 10);
	match.all = populate_list(cmplbuf, false);
	update_filter(false);
	menu_draw();
}

Item*
filter_list(Item *i, char *filter) {
	static Item exact;
	Item start, substr;
	Item *exactp, *startp, *substrp;
	Item **ip;
	char *p;
	int len;

	len = strlen(filter);
	exactp = &exact;
	startp = &start;
	substrp = &substr;
	for(; i; i=i->next_link)
		if((p = find(i->string, filter))) {
			ip = &substrp;
			if(p == i->string)
				if(strlen(p) == len)
					ip = &exactp;
				else
					ip = &startp;
			link(*ip, i);
			*ip = i;
		}

	link(substrp, &exact);
	link(startp,  &substr);
	link(exactp,  &start);
	splice(&substr);
	splice(&start);
	splice(&exact);
	return exact.next;
}

void
update_input(void) {
	if(alwaysprint) {
		write(1, input.string, input.pos - input.string);
		write(1, "\n", 1);
		write(1, input.pos, input.end - input.pos);
		write(1, "\n", 1);
	}
}

void
update_filter(bool print) {
	char *filter;

	filter = input.string + min(input.filter_start, input.pos - input.string);
	if(input.pos < input.end)
		filter = freelater(estrndup(filter, input.pos - filter));

	match.sel = nil;
	match.first = match.start = filter_list(match.all, filter);
	if(print)
		update_input();
}

enum { PointerScreen = -1 };

void
init_screens(void) {
	Rectangle *rects;
	Point p;
	int i, n;

	rects = xinerama_screens(&n);
	if(screen_hint >= 0 && screen_hint < n)
		i = screen_hint;
	else {
		/* Pick the screen with the pointer, for now. Later,
		 * try for the screen with the focused window first.
		 */
		p = querypointer(&scr.root);
		for(i=0; i < n; i++)
			if(rect_haspoint_p(rects[i], p))
				break;
		if(i == n)
			i = 0;
	}
	scr.rect = rects[i];
	menu_show();
}

ErrorCode ignored_xerrors[] = {
	{ 0, BadWindow },
	{ X_GetAtomName, BadAtom },
};

int
main(int argc, char *argv[]) {
	static char *address;
	static char *histfile;
	static char *keyfile;
	static bool nokeys;
	Item *item;
	int i;
	long ndump;

	setlocale(LC_ALL, "");
	fmtinstall('r', errfmt);
	quotefmtinstall();

	screen_hint = PointerScreen;

	find = strstr;
	compare = strncmp;

	ndump = -1;

	ARGBEGIN{
	case 'a':
		address = EARGF(usage());
		break;
	case 'c':
		alwaysprint = true;
		break;
	case 'h':
		histfile = EARGF(usage());
		break;
	case 'i':
		find = strcasestr;
		compare = strncasecmp;
		break;
	case 'K':
		nokeys = true;
	case 'k':
		keyfile = EARGF(usage());
		break;
	case 'n':
		ndump = strtol(EARGF(usage()), nil, 10);
		break;
	case 'p':
		menu.prompt = EARGF(usage());
		break;
	case 'r':
		menu.rows = strtol(EARGF(usage()), nil, 10);
		break;
	case 's':
		screen_hint = strtol(EARGF(usage()), nil, 10);
		break;
	case 'S':
		cmdsep = EARGF(usage());
		break;
	case 'v':
		lprint(1, "%s", version);
		return 0;
	default:
		usage();
	}ARGEND;

	if(argc)
		usage();

	initdisplay();

	xext_init();
	if(!isatty(0))
		menu_init();

	client_init(address);

	srv.preselect = event_preselect;
	ixp_listen(&srv, ConnectionNumber(display), nil, event_fdready, event_fdclosed);

	menu.ontop = !strcmp(readctl("/ctl", "bar "), "on top");
	client_readconfig(&cnorm, &csel, &font);

	itempad = (font->height & ~1) + font->pad.min.x + font->pad.max.x;

	cmplbuf = Bfdopen(0, OREAD);
	match.all = populate_list(cmplbuf, false);
	if(!isatty(cmplbuf->fid))
		ixp_listen(&srv, cmplbuf->fid, inbuf, check_competions, nil);

	caret_insert("", true);
	update_filter(false);

	if(!nokeys)
		parse_keys(binding_spec);
	if(keyfile) {
		i = open(keyfile, O_RDONLY);
		if(read(i, buffer, sizeof(buffer)) > 0)
			parse_keys(buffer);
	}

	histsel = &hist;
	link(&hist, &hist);
	if(histfile && (inbuf = Bopen(histfile, OREAD))) {
		item = filter_list(populate_list(inbuf, true), "");
		if(item->string) {
			link(item->prev, &hist);
			link(&hist, item);
		}
		Bterm(inbuf);
	}

	if(menu.win == nil)
		menu_init();

	init_screens();

	i = ixp_serverloop(&srv);
	if(i)
		fprint(2, "%s: error: %r\n", argv0);
	XCloseDisplay(display);

	if(ndump >= 0 && histfile && result == 0)
		history_dump(histfile, ndump);

	return result;
}

