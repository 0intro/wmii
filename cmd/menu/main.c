/* Copyright ©2006-2009 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#define IXP_NO_P9_
#define IXP_P9_STRUCTS
#define EXTERN
#include "dat.h"
#include <X11/Xproto.h>
#include <locale.h>
#include <strings.h>
#include <unistd.h>
#include <bio.h>
#include <clientutil.h>
#include "fns.h"
#define link _link

static const char version[] = "wimenu-"VERSION", ©2009 Kris Maglione\n";
static Biobuf*	cmplbuf;
static Biobuf*	inbuf;
static bool	alwaysprint;

static void
usage(void) {
	fatal("usage: wimenu -i [-h <history>] [-a <address>] [-p <prompt>] [-s <screen>]\n");
}

static int
errfmt(Fmt *f) {
	return fmtstrcpy(f, ixp_errbuf());
}

/* Stubs. */
void
debug(int flag, const char *fmt, ...) {
	va_list ap;

	USED(flag);
	va_start(ap, fmt);
	vfprint(2, fmt, ap);
	va_end(ap);
}

void dprint(long, char*, ...);
void dprint(long mask, char *fmt, ...) {
	va_list ap;

	USED(mask);
	va_start(ap, fmt);
	vfprint(2, fmt, ap);
	va_end(ap);
}

static void
splice(Item *i) {
	i->next->prev = i->prev;
	i->prev->next = i->next;
}
static void
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
	i = &ret;
	while((p = Brdstr(buf, '\n', true))) {
		if(stop && p[0] == '\0')
			break;
		link(i, emallocz(sizeof *i));
		i->next_link = i->next;
		i = i->next;
		i->string = p;
		i->retstring = p;
		if(!hist) {
			i->len = strlen(p);
			i->width = textwidth_l(font, p, i->len);
			if(i->width > maxwidth)
				maxwidth = i->width;
		}
	}

	link(i, &ret);
	splice(&ret);
	return ret.next != &ret ? ret.next : nil;
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
	items = populate_list(cmplbuf, false);
	update_filter();
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
update_filter(void) {
	char *filter;

	filter = input.string + min(input.filter_start, input.pos - input.string);
	if(input.pos < input.end)
		filter = freelater(estrndup(filter, input.pos - filter));

	matchidx = nil;
	matchfirst = matchstart = filter_list(items, filter);
	if(alwaysprint) {
		write(1, input.string, input.pos - input.string);
		write(1, "", 1);
		write(1, input.pos, input.end - input.pos + 1);
	}
}

/*
 * There's no way to check accesses to destroyed windows, thus
 * those cases are ignored (especially on UnmapNotifies).
 * Other types of errors call Xlib's default error handler, which
 * calls exit().
 */
ErrorCode ignored_xerrors[] = {
	{ 0, BadWindow },
	{ X_SetInputFocus, BadMatch },
	{ X_PolyText8, BadDrawable },
	{ X_PolyFillRectangle, BadDrawable },
	{ X_PolySegment, BadDrawable },
	{ X_ConfigureWindow, BadMatch },
	{ X_GrabKey, BadAccess },
	{ X_GetAtomName, BadAtom },
};

static void
end(IxpConn *c) {

	USED(c);
	srv.running = 0;
}

static void
preselect(IxpServer *s) {
	
	USED(s);
	check_x_event(nil);
}

enum { PointerScreen = -1 };

void
init_screens(int screen_hint) {
	Rectangle *rects;
	Point p;
	int i, n;

	rects = xinerama_screens(&n);
	if (screen_hint >= 0 && screen_hint < n)
		/* We were given a valid screen index, use that. */
		i = screen_hint;
	else {
		/* Pick the screen with the pointer, for now. Later,
		 * try for the screen with the focused window first.
		 */
		p = querypointer(&scr.root);
		for(i=0; i < n; i++)
			if(rect_haspoint_p(p, rects[i]))
				break;
		if(i == n)
			i = 0;
	}
	scr.rect = rects[i];
	menu_show();
}

int
main(int argc, char *argv[]) {
	Item *item;
	static char *address;
	static char *histfile;
	static char *keyfile;
	static bool nokeys;
	int i;
	long ndump;
	int screen;

	quotefmtinstall();
	fmtinstall('r', errfmt);
	address = getenv("WMII_ADDRESS");
	screen = PointerScreen;

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
		prompt = EARGF(usage());
		break;
	case 's':
		screen = strtol(EARGF(usage()), nil, 10);
		break;
	default:
		usage();
	}ARGEND;

	if(argc)
		usage();

	setlocale(LC_CTYPE, "");

	initdisplay();

	xext_init();
	if(!isatty(0))
		menu_init();

	client_init(address);

	srv.preselect = preselect;
	ixp_listen(&srv, ConnectionNumber(display), nil, check_x_event, end);

	ontop = !strcmp(readctl("bar on "), "top");
	loadcolor(&cnorm, readctl("normcolors "));
	loadcolor(&csel, readctl("focuscolors "));
	font = loadfont(readctl("font "));
	if(!font)
		fatal("Can't load font %q", readctl("font "));

	cmplbuf = Bfdopen(0, OREAD);
	items = populate_list(cmplbuf, false);
	if(!isatty(cmplbuf->fid))
		ixp_listen(&srv, cmplbuf->fid, inbuf, check_competions, nil);

	caret_insert("", true);
	update_filter();

	if(!nokeys)
		parse_keys(binding_spec);
	if(keyfile) {
		i = open(keyfile, O_RDONLY);
		if(read(i, buffer, sizeof(buffer)) > 0)
			parse_keys(buffer);
	}

	histidx = &hist;
	link(&hist, &hist);
	if(histfile) {
		inbuf = Bopen(histfile, OREAD);
		if(inbuf) {
			item = populate_list(inbuf, true);
			if(item) {
				link(item->prev, &hist);
				link(&hist, item);
			}
			Bterm(inbuf);
		}
	}

	if(barwin == nil)
		menu_init();

	init_screens(screen);

	i = ixp_serverloop(&srv);
	if(i)
		fprint(2, "%s: error: %r\n", argv0);
	XCloseDisplay(display);

	if(ndump >= 0 && histfile && result == 0)
		history_dump(histfile, ndump);

	return result;
}

