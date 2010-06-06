/* Copyight ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#define IXP_NO_P9_
#define IXP_P9_STRUCTS
#include <dirent.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <sys/signal.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#include <ixp.h>
#include <stuff/util.h>
#include <bio.h>
#include <fmt.h>

static IxpClient* client;
static Biobuf*	outbuf;
static bool	binary;

static void
usage(void) {
	lprint(1,
	       "usage: %s [-a <address>] [-b] {create | ls [-dlp] | read | remove | write} <file>\n"
	       "       %s [-a <address>] xwrite <file> <data>\n"
	       "       %s -v\n", argv0, argv0, argv0);
	exit(1);
}

static int
errfmt(Fmt *f) {
	return fmtstrcpy(f, ixp_errbuf());
}

static bool
flush(IxpCFid *fid, char *in, int len, bool binary) {
	static mbstate_t state;
	static char buf[IXP_MAX_MSG];
	static char *out = buf, *outend = buf + sizeof buf;
	char *inend;
	wchar_t w;
	Rune r;
	int res;

	if(binary)
		return ixp_write(fid, in, len) == len;

	inend = in + len;
	do {
		if(in == nil || out + UTFmax > outend) {
			if(ixp_write(fid, buf, out - buf) != out - buf)
				return false;
			out = buf;
		}
		if(in == nil) {
			state = (mbstate_t){0};
			return true;
		}

		switch((res = mbrtowc(&w, in, inend - in, &state))) {
		case -1:
			return false;
		case 0:
		case -2:
			return true;
		default:
			in += res;
			r = w < Runemax ? w : Runesync;
			out += runetochar(out, &r);
		}
	} while(in < inend);
	return true;
}

static bool
unflush(int fd, char *in, int len, bool binary) {
	static mbstate_t state;
	static char buf[IXP_MAX_MSG], extra[UTFmax];
	static char *out = buf, *outend = buf + sizeof buf;
	static int nextra;
	char *start;
	Rune r;
	int res, n;

	if(binary)
		return write(fd, in, len) == len;

	if(in) {
		if((n = nextra)) {
			nextra = 0;
			while(len > 0 && n < UTFmax && !fullrune(extra, n)) {
				extra[n++] = *in++;
				len--;
			}
			unflush(fd, extra, n, binary);
		}
		n = utfnlen(in, len);
	}

	start = in;
	do {
		if(in == nil || out + MB_LEN_MAX > outend) {
			if(write(fd, buf, out - buf) != out - buf)
				return false;
			out = buf;
		}
		if(in == nil || n == 0) {
			state = (mbstate_t){0};
			return true;
		}

		in += chartorune(&r, in);
		n--;
		res = wcrtomb(out, r, &state);
		if(res == -1)
			*out++ = '?';
		else
			out += res;
	} while(n > 0);
	if(in < start + len) {
		nextra = min(sizeof extra, len - (in - start));
		memcpy(extra, in, nextra);
	}
	return true;
}

/* Utility Functions */
static void
write_data(IxpCFid *fid, char *name, bool binary) {
	char buf[IXP_MAX_MSG];
	int len;

	while((len = read(0, buf, fid->iounit)) > 0)
		if(!flush(fid, buf, len, binary))
			fatal("cannot write file %q\n", name);

	if(!binary)
		flush(fid, nil, 0, binary);
}

static int
comp_stat(const void *s1, const void *s2) {
	Stat *st1, *st2;

	st1 = (Stat*)s1;
	st2 = (Stat*)s2;
	return strcmp(st1->name, st2->name);
}

static void
setrwx(long m, char *s) {
	static char *modes[] = {
		"---", "--x", "-w-",
		"-wx", "r--", "r-x",
		"rw-", "rwx",
	};
	strncpy(s, modes[m], 3);
}

static char *
modestr(uint mode) {
	static char buf[16];

	buf[0]='-';
	if(mode & P9_DMDIR)
		buf[0]='d';
	buf[1]='-';
	setrwx((mode >> 6) & 7, &buf[2]);
	setrwx((mode >> 3) & 7, &buf[5]);
	setrwx((mode >> 0) & 7, &buf[8]);
	buf[11] = 0;
	return buf;
}

static char*
timestr(time_t val) {
	static char buf[32];

	strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", localtime(&val));
	return buf;
}

static void
print_stat(Stat *s, int lflag, char *file, int pflag) {
	char *slash;

	slash = "";
	if(pflag)
		slash = "/";
	else
		file = "";

	if(lflag)
		Blprint(outbuf, "%s %s %s %5llud %s %s%s%s\n",
			modestr(s->mode), s->uid, s->gid, s->length,
			timestr(s->mtime), file, slash, s->name);
	else {
		if((s->mode&P9_DMDIR) && strcmp(s->name, "/"))
			Blprint(outbuf, "%s%s%s/\n", file, slash, s->name);
		else
			Blprint(outbuf, "%s%s%s\n", file, slash, s->name);
	}
}

/* Service Functions */
static int
xwrite(int argc, char *argv[]) {
	IxpCFid *fid;
	char *file;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());
	fid = ixp_open(client, file, P9_OWRITE);
	if(fid == nil)
		fatal("Can't open file '%s': %r\n", file);

	write_data(fid, file, binary);
	ixp_close(fid);
	return 0;
}

static int
xawrite(int argc, char *argv[]) {
	IxpCFid *fid;
	char *file, *buf;
	int nbuf, i;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());
	fid = ixp_open(client, file, P9_OWRITE);
	if(fid == nil)
		fatal("Can't open file '%s': %r\n", file);

	nbuf = 1;
	for(i=0; i < argc; i++)
		nbuf += strlen(argv[i]) + (i > 0);
	buf = emalloc(nbuf);
	buf[0] = '\0';
	while(argc) {
		strcat(buf, ARGF());
		if(argc)
			strcat(buf, " ");
	}

	if(!(flush(fid, buf, nbuf, binary) && (binary || flush(fid, 0, 0, binary))))
		fatal("cannot write file '%s': %r\n", file);
	ixp_close(fid);
	free(buf);
	return 0;
}

static int
xcreate(int argc, char *argv[]) {
	IxpCFid *fid;
	char *file;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());
	fid = ixp_create(client, file, 0777, P9_OWRITE);
	if(fid == nil)
		fatal("Can't create file '%s': %r\n", file);

	if((fid->qid.type&P9_DMDIR) == 0)
		write_data(fid, file, binary);
	ixp_close(fid);
	return 0;
}

static int
xremove(int argc, char *argv[]) {
	char *file;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());
	do {
		if(!ixp_remove(client, file))
			lprint(2, "%s: Can't remove file '%s': %r\n", argv0, file);
	}while((file = ARGF()));
	return 0;
}

static int
xread(int argc, char *argv[]) {
	IxpCFid *fid;
	char *file, *buf;
	int count;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	if(argc == 0)
		usage();
	file = EARGF(usage());
	do {
		fid = ixp_open(client, file, P9_OREAD);
		if(fid == nil)
			fatal("Can't open file '%s': %r\n", file);

		buf = emalloc(fid->iounit);
		while((count = ixp_read(fid, buf, fid->iounit)) > 0)
			unflush(1, buf, count, binary);
		if(!binary)
			unflush(1, 0, 0, binary);
		ixp_close(fid);

		if(count == -1)
			lprint(2, "%s: cannot read file '%s': %r\n", argv0, file);
	} while((file = ARGF()));

	return 0;
}

static int
xls(int argc, char *argv[]) {
	IxpMsg m;
	Stat *stat;
	IxpCFid *fid;
	char *file;
	char *buf;
	int lflag, dflag, pflag;
	int count, nstat, mstat, i;

	lflag = dflag = pflag = 0;

	ARGBEGIN{
	case 'l':
		lflag++;
		break;
	case 'd':
		dflag++;
		break;
	case 'p':
		pflag++;
		break;
	default:
		usage();
	}ARGEND;

	count = 0;
	file = EARGF(usage());
	do {
		stat = ixp_stat(client, file);
		if(stat == nil)
			fatal("cannot stat file '%s': %r\n", file);

		i = strlen(file);
		if(file[i-1] == '/') {
			file[i-1] = '\0';
			if(!(stat->mode&P9_DMDIR))
				fatal("%s: not a directory", file);
		}
		if(dflag || (stat->mode&P9_DMDIR) == 0) {
			print_stat(stat, lflag, file, pflag);
			ixp_freestat(stat);
			continue;
		}
		ixp_freestat(stat);

		fid = ixp_open(client, file, P9_OREAD);
		if(fid == nil)
			fatal("Can't open file '%s': %r\n", file);

		nstat = 0;
		mstat = 16;
		stat = emalloc(mstat * sizeof *stat);
		buf = emalloc(fid->iounit);
		while((count = ixp_read(fid, buf, fid->iounit)) > 0) {
			m = ixp_message(buf, count, MsgUnpack);
			while(m.pos < m.end) {
				if(nstat == mstat) {
					mstat <<= 1;
					stat = erealloc(stat, mstat * sizeof *stat);
				}
				ixp_pstat(&m, &stat[nstat++]);
			}
		}
		ixp_close(fid);

		qsort(stat, nstat, sizeof *stat, comp_stat);
		for(i = 0; i < nstat; i++) {
			print_stat(&stat[i], lflag, file, pflag);
			ixp_freestat(&stat[i]);
		}
		free(stat);
	} while((file = ARGF()));

	if(count == -1)
		fatal("cannot read directory '%s': %r\n", file);
	return 0;
}

static int
xnamespace(int argc, char *argv[]) {
	char *path;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	path = ixp_namespace();
	if(path == nil)
		fatal("can't find namespace: %r\n");
	Blprint(outbuf, "%s\n", path);
	return 0;
}

static int
xproglist(int argc, char *argv[]) {
	DIR *d;
	struct dirent *de;
	char *dir;

	quotefmtinstall();

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	while((dir = ARGF()))
		/* Don't use Blprint. wimenu expects UTF-8. */
		if((d = opendir(dir))) {
			while((de = readdir(d)))
				if(access(de->d_name, X_OK))
					Bprint(outbuf, "%q\n", de->d_name);
			closedir(d);
		}

	return 0; /* NOTREACHED */
}

static int
xsetsid(int argc, char *argv[]) {
	char *av0;
	bool dofork;

	av0 = nil;
	dofork = false;
	ARGBEGIN{
	case '0':
		av0 = EARGF(usage());
		break;
	case 'f':
		dofork = true;
		break;
	default:
		usage();
	}ARGEND;
	if(av0 == nil)
		av0 = argv[0];
	if(av0 == nil)
		return 1;

	setsid();
	if(dofork)
		switch(fork()) {
		case 0:  break;
		case -1: fatal("can't fork: %r\n");
		default: return 0;
		}

	execvp(av0, argv);
	fatal("setsid: can't exec: %r");
	return 1; /* NOTREACHED */
}

typedef struct exectab exectab;
struct exectab {
	char *cmd;
	int (*fn)(int, char**);
} fstab[] = {
	{"cat", xread},
	{"create", xcreate},
	{"ls", xls},
	{"read", xread},
	{"remove", xremove},
	{"rm", xremove},
	{"write", xwrite},
	{"xwrite", xawrite},
	{0, }
}, utiltab[] = {
	{"namespace", xnamespace},
	{"ns", xnamespace},
	{"proglist", xproglist},
	{"setsid", xsetsid},
	{0, }
};

int
main(int argc, char *argv[]) {
	char *address;
	exectab *tab;
	int ret;

	setlocale(LC_ALL, "");
	binary = utf8locale();

	quotefmtinstall();
	fmtinstall('r', errfmt);

	address = getenv("WMII_ADDRESS");

	ARGBEGIN{
	case 'b':
		binary = true;
		break;
	case 'v':
		lprint(1, "%s-" VERSION ", " COPYRIGHT "\n", argv0);
		exit(0);
	case 'a':
		address = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND;

	if(argc < 1)
		usage();

	outbuf = Bfdopen(1, OWRITE);

	for(tab=utiltab; tab->cmd; tab++)
		if(!strcmp(*argv, tab->cmd)) {
			ret = tab->fn(argc, argv);
			goto done;
		}

	if(address && *address)
		client = ixp_mount(address);
	else
		client = ixp_nsmount("wmii");
	if(client == nil)
		fatal("can't mount: %r\n");

	signal(SIGPIPE, SIG_DFL);

	for(tab=fstab; tab->cmd; tab++)
		if(strcmp(*argv, tab->cmd) == 0) break;
	if(tab->cmd == 0)
		usage();

	ret = tab->fn(argc, argv);

	ixp_unmount(client);
done:
	Bterm(outbuf);
	return ret;
}

