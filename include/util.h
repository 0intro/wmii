#define nil	((void*)0)
#define nelem(ary) (sizeof(ary) / sizeof(*ary))

/* Types */
#undef uchar
#undef ushort
#undef uint
#undef ulong
#undef uvlong
#undef vlong
#ifndef KENC
# define uchar	_wmiiuchar
# define ushort	_wmiiushort
# define uint	_wmiiuint
# define ulong	_wmiiulong
# define vlong	_wmiivlong
# define uvlong	_wmiiuvlong
#endif
typedef unsigned char		uchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;
typedef unsigned long long	uvlong;
typedef long long		vlong;

#define strlcat wmii_strlcat
/* util.c */
uint tokenize(char *res[], uint reslen, char *str, char delim);
char *estrdup(const char *str);
void *erealloc(void *ptr, uint size);
void *emallocz(uint size);
void *emalloc(uint size);
void fatal(const char *fmt, ...);
int max(int a, int b);
int min(int a, int b);
char *str_nil(char *s);
uint strlcat(char *dst, const char *src, unsigned int siz);

char *argv0;
void *__p;
int __i;
#undef ARGBEGIN
#undef ARGEND
#undef ARGF
#undef EARGF
#define ARGBEGIN \
		int _argi=0, _argtmp=0, _inargv=0; char *_argv=nil; \
		if(!argv0) argv0=ARGF(); \
		_inargv=1; USED(_inargv); \
		while(argc && argv[0][0] == '-') { \
			_argi=1; _argv=*argv++; argc--; \
			while(_argv[_argi]) switch(_argv[_argi++])
#define ARGEND }_inargv=0;USED(_argtmp);USED(_argv);USED(_argi);USED(_inargv)

#define ARGF() ((_inargv && _argv[_argi]) ? \
			(_argtmp=_argi, _argi=strlen(_argv), __i=_argi,_argv+_argtmp) \
			: ((argc > 0) ? (--argc, ++argv, __i=argc, __p=argv, (*argv-1)) : ((char*)0)))

#define EARGF(f) ((_inargv && _argv[_argi]) ? \
			(_argtmp=_argi, _argi=strlen(_argv), __i=_argi, _argv+_argtmp) \
			: ((argc > 0) ? (--argc, ++argv, __i=argc, __p=argv, (*argv-1)) : ((f), (char*)0)))

#ifndef KENC
# undef USED
# undef SET
# define USED(x) if(x){}else
# define SET(x) ((x)=0)
#endif

