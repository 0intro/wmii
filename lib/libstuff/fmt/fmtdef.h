#include <stuff/util.h>
#include <langinfo.h>
#include <limits.h>
#include <string.h>
#include <wchar.h>

extern void* __fmtflush(Fmt *f, void *t, int len);
extern int __fmtpad(Fmt *f, int n);
extern int __rfmtpad(Fmt *f, int n);

