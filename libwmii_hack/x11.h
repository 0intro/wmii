#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

static void	changeprop_char(Display*, Window, char*, char*, char[], int);
static void	changeprop_long(Display*, Window, char*, char*, long[], int);
/* static void	changeprop_short(Display*, Window, char*, char*, short[], int); */
static void	changeprop_string(Display*, Window, char*, char*);
static void	changeprop_textlist(Display*, Window, char*, char*, char*[]);
static void	changeproperty(Display*, Window, char*, char*, int width, uchar*, int);
/* static void	delproperty(Display*, Window, char*); */
/* static void	freestringlist(char**); */
static ulong	getprop_long(Display*, Window, char*, char*, ulong, long**, ulong);
/* static char*	getprop_string(Display*, Window, char*); */
/* static int	getprop_textlist(Display*, Window, char*, char**[]); */
/* static ulong	getproperty(Display*, Window, char*, char*, Atom*, ulong, uchar**, ulong); */
static Atom	xatom(Display*, char*);

