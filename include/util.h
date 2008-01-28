/* Copyright ©2007-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#define nil	((void*)0)
#define nelem(ary) (sizeof(ary) / sizeof(*ary))

/* Types */
#undef uchar
#undef ushort
#undef uint
#undef ulong
#undef uvlong
#undef vlong
#define uchar	_x_uchar
#define ushort	_x_ushort
#define uint	_x_uint
#define ulong	_x_ulong
#define uvlong	_x_uvlong
#define vlong	_x_vlong

typedef unsigned char		uchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;
typedef unsigned long long	uvlong;
typedef long long	vlong;

#ifdef VARARGCK
# pragma varargck	argpos	_die	3
# pragma varargck	argpos	fatal	1
# pragma varargck	argpos	sxprint	1
#endif

#define strlcat wmii_strlcat
/* util.c */
void	_die(char*, int, char*, ...);
void*	emalloc(uint);
void*	emallocz(uint);
void*	erealloc(void*, uint);
char*	estrdup(const char*);
void	fatal(const char*, ...);
void*	freelater(void*);
int	max(int, int);
int	min(int, int);
uint	strlcat(char*, const char*, uint);
char*	sxprint(const char*, ...);
uint	tokenize(char **, uint, char*, char);
int	utflcpy(char*, const char*, int);
char*	vsxprint(const char*, va_list);

#define die(...) \
	_die(__FILE__, __LINE__, __VA_ARGS__)

char *argv0;
#undef ARGBEGIN
#undef ARGEND
#undef ARGF
#undef EARGF
#define ARGBEGIN \
		int _argtmp=0, _inargv; char *_argv=nil; \
		if(!argv0) argv0=*argv; argv++, argc--; \
		_inargv=1; USED(_inargv); \
		while(argc && argv[0][0] == '-') { \
			_argv=&argv[0][1]; argv++; argc--; \
			if(_argv[0] == '-' && _argv[1] == '\0') \
				break; \
			while(*_argv) switch(*_argv++)
#define ARGEND }_inargv=0;USED(_argtmp, _argv, _inargv)

#define EARGF(f) ((_inargv && *_argv) ? \
			(_argtmp=strlen(_argv), _argv+=_argtmp, _argv-_argtmp) \
			: ((argc > 0) ? \
				(--argc, ++argv, _used(argc), *(argv-1)) \
				: ((f), (char*)0)))
#define ARGF() EARGF(_used(0))

static inline void
_used(long a, ...) {
	if(a){}
}
#ifndef KENC
#  undef USED
#  undef SET
#  define USED(...) _used((long)__VA_ARGS__)
#  define SET(x) (x = 0)
/* # define SET(x) USED(&x) GCC 4 is 'too smart' for this. */
#endif
