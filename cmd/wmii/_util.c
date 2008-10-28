/* Copyright ©2008 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <bio.h>
#include "fns.h"

/* Blech. */
#define VECTOR(type, nam, c) \
void                                                                    \
vector_##c##init(Vector_##nam *v) {                                     \
	memset(v, 0, sizeof *v);                                        \
}                                                                       \
                                                                        \
void                                                                    \
vector_##c##free(Vector_##nam *v) {                                     \
	free(v->ary);                                                   \
	memset(v, 0, sizeof *v);                                        \
}                                                                       \
                                                                        \
void                                                                    \
vector_##c##push(Vector_##nam *v, type val) {                           \
	if(v->n == v->size) {                                           \
		if(v->size == 0)                                        \
			v->size = 2;                                    \
		v->size <<= 2;                                          \
		v->ary = erealloc(v->ary, v->size * sizeof *v->ary);    \
	}                                                               \
	v->ary[v->n++] = val;                                           \
}                                                                       \

VECTOR(long, long, l)
VECTOR(Rectangle, rect, r)
VECTOR(void*, ptr, p)

int
doublefork(void) {
	pid_t pid;
	int status;
	
	switch(pid=fork()) {
	case -1:
		fatal("Can't fork(): %r");
	case 0:
		switch(pid=fork()) {
		case -1:
			fatal("Can't fork(): %r");
		case 0:
			return 0;
		default:
			exit(0);
		}
	default:
		waitpid(pid, &status, 0);
		return pid;
	}
	/* NOTREACHED */
}

void
closeexec(int fd) {
	if(fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
		fatal("can't set %d close on exec: %r", fd);
}

int
spawn3(int fd[3], const char *file, char *argv[]) {
	/* Some ideas from Russ Cox's libthread port. */
	int p[2];
	int pid;

	if(pipe(p) < 0)
		return -1;
	closeexec(p[1]);

	switch(pid = doublefork()) {
	case 0:
		dup2(fd[0], 0);
		dup2(fd[1], 1);
		dup2(fd[2], 2);

		execvp(file, argv);
		write(p[1], &errno, sizeof errno);
		exit(1);
		break;
	default:
		close(p[1]);
		if(read(p[0], &errno, sizeof errno) == sizeof errno)
			pid = -1;
		close(p[0]);
		break;
	case -1: /* can't happen */
		break;
	}

	close(fd[0]);
	/* These could fail if any of these was also a previous fd. */
	close(fd[1]);
	close(fd[2]);
	return pid;
}

int
spawn3l(int fd[3], const char *file, ...) {
	va_list ap;
	char **argv;
	int i, n;

	va_start(ap, file);
	for(n=0; va_arg(ap, char*); n++)
		;
	va_end(ap);

	argv = emalloc((n+1) * sizeof *argv);
	va_start(ap, file);
	quotefmtinstall();
	for(i=0; i <= n; i++)
		argv[i] = va_arg(ap, char*);
	va_end(ap);

	i = spawn3(fd, file, argv);
	free(argv);
	return i;
}

#ifdef __linux__
# define PROGTXT "exe"
#else
# define PROGTXT "file"
#endif

static void
_backtrace(int pid, char *btarg) {
	char *proc, *spid, *gdbcmd;
	int fd[3], p[2];
	int status, cmdfd;

	if(pipe(p) < 0)
		goto done;
	closeexec(p[0]);

	gdbcmd = estrdup("/tmp/gdbcmd.XXXXXX");
	cmdfd = mkstemp(gdbcmd);
	if(cmdfd < 0)
		goto done;

	fprint(cmdfd, "bt %s\n", btarg);
	fprint(cmdfd, "detach\n");
	close(cmdfd);

	fd[0] = open("/dev/null", O_RDONLY);
	fd[1] = p[1];
	fd[2] = dup(2);

	proc = sxprint("/proc/%d/" PROGTXT, pid);
	spid = sxprint("%d", pid);
	if(spawn3l(fd, "gdb", "gdb", "-batch", "-x", gdbcmd, proc, spid, nil) < 0) {
		unlink(gdbcmd);
		goto done;
	}

	Biobuf bp;
	char *s;

	Binit(&bp, p[0], OREAD);
	while((s = Brdstr(&bp, '\n', 1))) {
		Dprint(DStack, "%s\n", s);
		free(s);
	}
	unlink(gdbcmd);

done:
	free(gdbcmd);
	kill(pid, SIGKILL);
	waitpid(pid, &status, 0);
}

void
backtrace(char *btarg) {
	int pid;

	/* Fork so we can backtrace the child. Keep this stack
	 * frame minimal, so the trace is fairly clean.
	 */
	Debug(DStack)
		switch(pid = fork()) {
		case -1:
			return;
		case 0:
			kill(getpid(), SIGSTOP);
			_exit(0);
		default:
			_backtrace(pid, btarg);
			break;
		}

}

void
reinit(Regex *r, char *regx) {

	refree(r);

	if(regx[0] != '\0') {
		r->regex = estrdup(regx);
		r->regc = regcomp(regx);
	}
}

void
refree(Regex *r) {

	free(r->regex);
	free(r->regc);
	r->regex = nil;
	r->regc = nil;
}

void
uniq(char **toks) {
	char **p, **q;

	q = toks;
	if(*q == nil)
		return;
	for(p=q+1; *p; p++)
		if(strcmp(*q, *p))
			*++q = *p;
	*++q = nil;
}

char**
comm(int cols, char **toka, char **tokb) {
	Vector_ptr vec;
	char **ret;
	int cmp, len;

	len = 0;
	vector_pinit(&vec);
	while(*toka || *tokb) {
		if(!*toka)
			cmp = 1;
		else if(!*tokb)
			cmp = -1;
		else
			cmp = strcmp(*toka, *tokb);
		if(cmp < 0) {
			if(cols & CLeft) {
				vector_ppush(&vec, *toka);
				len += strlen(*toka) + 1;
			}
			toka++;
		}else if(cmp > 0) {
			if(cols & CRight) {
				vector_ppush(&vec, *tokb);
				len += strlen(*tokb) + 1;
			}
			tokb++;
		}else {
			if(cols & CCenter) {
				vector_ppush(&vec, *toka);
				len += strlen(*toka) + 1;
			}
			toka++;
			tokb++;
		}
	}
	vector_ppush(&vec, nil);
	ret = strlistdup((char**)vec.ary);
	free(vec.ary);
	return ret;
}

void
grep(char **list, Reprog *re, int flags) {
	char **p, **q;
	int res;

	q = list;
	for(p=q; *p; p++) {
		res = 0;
		if(re)
			res = regexec(re, *p, nil, 0);
		if(res && !(flags & GInvert)
		|| !res && (flags & GInvert))
			*q++ = *p;
	}
	*q = nil;
}

char*
join(char **list, char *sep) {
	Fmt f;
	char **p;

	if(fmtstrinit(&f) < 0)
		abort();

	for(p=list; *p; p++) {
		if(p != list)
			fmtstrcpy(&f, sep);
		fmtstrcpy(&f, *p);
	}

	return fmtstrflush(&f);
}

int
strlcatprint(char *buf, int len, const char *fmt, ...) {
	va_list ap;
	int buflen;
	int ret;

	va_start(ap, fmt);
	buflen = strlen(buf);
	ret = vsnprint(buf+buflen, len-buflen, fmt, ap);
	va_end(ap);
	return ret;
}

int
unquote(char *buf, char *toks[], int ntoks) {
	char *s, *t;
	bool inquote;
	int n;

	n = 0;
	s = buf;
	while(*s && n < ntoks) {
		while(*s && utfrune(" \t\r\n", *s))
			s++;
		inquote = false;
		toks[n] = s;
		t = s;
		while(*s && (inquote || !utfrune(" \t\r\n", *s))) {
			if(*s == '\'') {
				if(inquote && s[1] == '\'')
					*t++ = *s++;
				else
					inquote = !inquote;
			}
			else
				*t++ = *s;
			s++;
		}
		if(*s)
			s++;
		*t = '\0';
		if(s != toks[n])
			n++;
	}
	return n;
}

