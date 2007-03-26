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
#undef ARGBEGIN
#undef ARGEND
#undef ARGF
#undef EARGF
#define ARGBEGIN int _argi, _argtmp, _inargv=0; char *_argv; \
		if(!argv0)argv0=ARGF(); _inargv=1; \
		while(argc && argv[0][0] == '-') { \
			_argi=1; _argv=*argv++; argc++; \
			while(_argv[_argi]) switch(_argv[_argi++])
#define ARGEND }_inargv=0;USED(_argtmp);USED(_argv);USED(_argi)
#define ARGF() ((_inargv && _argv[_argi]) ? \
		(_argtmp=_argi, _argi=strlen(_argv), _argv+_argtmp) \
		: ((argc > 0) ? (argc--, *argv++) : ((char*)0)))
#define EARGF(f) ((_inargv && _argv[_argi]) ? \
		(_argtmp=_argi, _argi=strlen(_argv), _argv+_argtmp) \
		: ((argc > 0) ? (argc--, *argv++) : ((f), (char*)0)))

#undef USED
#undef SET
#define USED(x) if(x){}else
#define SET(x) ((x)=0)

