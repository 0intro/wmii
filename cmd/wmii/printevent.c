/*
 * Original code posted to comp.sources.x
 * Modifications by Russ Cox <rsc@swtch.com>.
 * Further modifications by Kris Maglione <fbsdaemon@gmail.com>
 */

/*
 * Path: uunet!wyse!mikew From: mikew@wyse.wyse.com (Mike Wexler) Newsgroups:
 * comp.sources.x Subject: v02i056:  subroutine to print events in human
 * readable form, Part01/01 Message-ID: <1935@wyse.wyse.com> Date: 22 Dec 88
 * 19:28:25 GMT Organization: Wyse Technology, San Jose Lines: 1093 Approved:
 * mikew@wyse.com
 * 
 * Submitted-by: richsun!darkstar!ken Posting-number: Volume 2, Issue 56
 * Archive-name: showevent/part01
 * 
 * 
 * There are times during debugging when it would be real useful to be able to
 * print the fields of an event in a human readable form.  Too many times I
 * found myself scrounging around in section 8 of the Xlib manual looking for
 * the valid fields for the events I wanted to see, then adding printf's to
 * display the numeric values of the fields, and then scanning through X.h
 * trying to decode the cryptic detail and state fields.  After playing with
 * xev, I decided to write a couple of standard functions that I could keep
 * in a library and call on whenever I needed a little debugging verbosity.
 * The first function, GetType(), is useful for returning the string
 * representation of the type of an event.  The second function, ShowEvent(),
 * is used to display all the fields of an event in a readable format.  The
 * functions are not complicated, in fact, they are mind-numbingly boring -
 * but that's just the point nobody wants to spend the time writing functions
 * like this, they just want to have them when they need them.
 * 
 * A simple, sample program is included which does little else but to
 * demonstrate the use of these two functions.  These functions have saved me
 * many an hour during debugging and I hope you find some benefit to these.
 * If you have any comments, suggestions, improvements, or if you find any
 * blithering errors you can get it touch with me at the following location:
 * 
 * ken@richsun.UUCP
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/Intrinsic.h>
#include <X11/Xproto.h>
#include <util.h>
//#include "dat.h"
//#include "fns.h"
#include "printevent.h"

#define nil ((void*)0)

typedef struct Pair Pair;

struct Pair {
	int key;
	char *val;
};

static char* sep = " ";

static char *
search(Pair *lst, int key, char *(*def)(int)) {
	for(; lst->val; lst++)
		if(lst->key == key)
			return lst->val;
	return def(key);
}

static char buffer[512];

static char*
unmask(Pair * list, uint val)
{
	Pair  *p;
	char *s, *end;
	Boolean first = True;

	buffer[0] = '\0';
	end = buffer + sizeof buffer;
	s = buffer;

	s += strlcat(s, "(", end - s);

	for (p = list; p->val; p++)
		if (val & p->key) {
			if (!first)
				s += strlcat(s, "|", end - s);
			first = False;
			s += strlcat(s, p->val, end - s);
		}

	s += strlcat(s, ")", end - s);

	return buffer;
}

static char *
strhex(int key) {
	sprintf(buffer, "0x%x", key);
	return buffer;
}

static char *
strdec(int key) {
	sprintf(buffer, "%d", key);
	return buffer;
}

static char *
strign(int key) {
	return "?";
}

/******************************************************************************/
/**** Miscellaneous routines to convert values to their string equivalents ****/
/******************************************************************************/

static char    *
Self(char *str)
{
	strncpy(buffer, str, sizeof buffer);
	free(str);
	return buffer;
}

/* Returns the string equivalent of a timestamp */
static char    *
ServerTime(Time time)
{
	ulong		msec;
	ulong		sec;
	ulong		min;
	ulong		hr;
	ulong		day;

	msec = time % 1000;
	time /= 1000;
	sec = time % 60;
	time /= 60;
	min = time % 60;
	time /= 60;
	hr = time % 24;
	time /= 24;
	day = time;

	if (0)
		sprintf(buffer, "%lu day%s %02lu:%02lu:%02lu.%03lu",
			day, day == 1 ? "" : "(s)", hr, min, sec, msec);

	sprintf(buffer, "%lud%luh%lum%lu.%03lds", day, hr, min, sec, msec);
	return buffer;
}

/* Returns the string equivalent of a boolean parameter */
static char    *
TorF(int key)
{
	static Pair	list[] = {
		{True, "True"},
		{False, "False"},
		{0, nil},
	};

	return search(list, key, strign);
}

/* Returns the string equivalent of a property notify state */
static char    *
PropertyState(int key)
{
	static Pair	list[] = {
		{PropertyNewValue, "PropertyNewValue"},
		{PropertyDelete, "PropertyDelete"},
		{0, nil},
	};

	return search(list, key, strign);
}

/* Returns the string equivalent of a visibility notify state */
static char    *
VisibilityState(int key)
{
	static Pair	list[] = {
		{VisibilityUnobscured, "VisibilityUnobscured"},
		{VisibilityPartiallyObscured, "VisibilityPartiallyObscured"},
		{VisibilityFullyObscured, "VisibilityFullyObscured"},
		{0, nil},
	};

	return search(list, key, strign);
}

/* Returns the string equivalent of a mask of buttons and/or modifier keys */
static char    *
ButtonAndOrModifierState(uint state)
{
	static Pair	list[] = {
		{Button1Mask, "Button1Mask"},
		{Button2Mask, "Button2Mask"},
		{Button3Mask, "Button3Mask"},
		{Button4Mask, "Button4Mask"},
		{Button5Mask, "Button5Mask"},
		{ShiftMask, "ShiftMask"},
		{LockMask, "LockMask"},
		{ControlMask, "ControlMask"},
		{Mod1Mask, "Mod1Mask"},
		{Mod2Mask, "Mod2Mask"},
		{Mod3Mask, "Mod3Mask"},
		{Mod4Mask, "Mod4Mask"},
		{Mod5Mask, "Mod5Mask"},
		{0, nil},
	};

	return unmask(list, state);
}

/* Returns the string equivalent of a mask of configure window values */
static char    *
ConfigureValueMask(uint valuemask)
{
	static Pair	list[] = {
		{CWX, "CWX"},
		{CWY, "CWY"},
		{CWWidth, "CWWidth"},
		{CWHeight, "CWHeight"},
		{CWBorderWidth, "CWBorderWidth"},
		{CWSibling, "CWSibling"},
		{CWStackMode, "CWStackMode"},
		{0, nil},
	};

	return unmask(list, valuemask);
}

/* Returns the string equivalent of a motion hint */
#if 0
static char    *
IsHint(char key)
{
	static Pair	list[] = {
		{NotifyNormal, "NotifyNormal"},
		{NotifyHint, "NotifyHint"},
		{0, nil},
	};

	return search(list, key, strign);
}
#endif

/* Returns the string equivalent of an id or the value "None" */
static char    *
MaybeNone(int key)
{
	static Pair	list[] = {
		{None, "None"},
		{0, nil},
	};

	return search(list, key, strhex);
}

/* Returns the string equivalent of a colormap state */
static char    *
ColormapState(int key)
{
	static Pair	list[] = {
		{ColormapInstalled, "ColormapInstalled"},
		{ColormapUninstalled, "ColormapUninstalled"},
		{0, nil},
	};

	return search(list, key, strign);
}

/* Returns the string equivalent of a crossing detail */
static char    *
CrossingDetail(int key)
{
	static Pair	list[] = {
		{NotifyAncestor, "NotifyAncestor"},
		{NotifyInferior, "NotifyInferior"},
		{NotifyVirtual, "NotifyVirtual"},
		{NotifyNonlinear, "NotifyNonlinear"},
		{NotifyNonlinearVirtual, "NotifyNonlinearVirtual"},
		{0, nil},
	};

	return search(list, key, strign);
}

/* Returns the string equivalent of a focus change detail */
static char    *
FocusChangeDetail(int key)
{
	static Pair	list[] = {
		{NotifyAncestor, "NotifyAncestor"},
		{NotifyInferior, "NotifyInferior"},
		{NotifyVirtual, "NotifyVirtual"},
		{NotifyNonlinear, "NotifyNonlinear"},
		{NotifyNonlinearVirtual, "NotifyNonlinearVirtual"},
		{NotifyPointer, "NotifyPointer"},
		{NotifyPointerRoot, "NotifyPointerRoot"},
		{NotifyDetailNone, "NotifyDetailNone"},
		{0, nil},
	};

	return search(list, key, strign);
}

/* Returns the string equivalent of a configure detail */
static char    *
ConfigureDetail(int key)
{
	static Pair	list[] = {
		{Above, "Above"},
		{Below, "Below"},
		{TopIf, "TopIf"},
		{BottomIf, "BottomIf"},
		{Opposite, "Opposite"},
		{0, nil},
	};

	return search(list, key, strign);
}

/* Returns the string equivalent of a grab mode */
static char    *
GrabMode(int key)
{
	static Pair	list[] = {
		{NotifyNormal, "NotifyNormal"},
		{NotifyGrab, "NotifyGrab"},
		{NotifyUngrab, "NotifyUngrab"},
		{NotifyWhileGrabbed, "NotifyWhileGrabbed"},
		{0, nil},
	};

	return search(list, key, strign);
}

/* Returns the string equivalent of a mapping request */
static char    *
MappingRequest(int key)
{
	static Pair	list[] = {
		{MappingModifier, "MappingModifier"},
		{MappingKeyboard, "MappingKeyboard"},
		{MappingPointer, "MappingPointer"},
		{0, nil},
	};

	return search(list, key, strign);
}

/* Returns the string equivalent of a stacking order place */
static char    *
Place(int key)
{
	static Pair	list[] = {
		{PlaceOnTop, "PlaceOnTop"},
		{PlaceOnBottom, "PlaceOnBottom"},
		{0, nil},
	};

	return search(list, key, strign);
}

/* Returns the string equivalent of a major code */
static char    *
MajorCode(int key)
{
	static Pair	list[] = {
		{X_CopyArea, "X_CopyArea"},
		{X_CopyPlane, "X_CopyPlane"},
		{0, nil},
	};

	return search(list, key, strhex);
}

static char    *
eventtype(int key)
{
	static Pair	list[] = {
		{ButtonPress, "ButtonPress"},
		{ButtonRelease, "ButtonRelease"},
		{CirculateNotify, "CirculateNotify"},
		{CirculateRequest, "CirculateRequest"},
		{ClientMessage, "ClientMessage"},
		{ColormapNotify, "ColormapNotify"},
		{ConfigureNotify, "ConfigureNotify"},
		{ConfigureRequest, "ConfigureRequest"},
		{CreateNotify, "CreateNotify"},
		{DestroyNotify, "DestroyNotify"},
		{EnterNotify, "EnterNotify"},
		{Expose, "Expose"},
		{FocusIn, "FocusIn"},
		{FocusOut, "FocusOut"},
		{GraphicsExpose, "GraphicsExpose"},
		{GravityNotify, "GravityNotify"},
		{KeyPress, "KeyPress"},
		{KeyRelease, "KeyRelease"},
		{KeymapNotify, "KeymapNotify"},
		{LeaveNotify, "LeaveNotify"},
		{MapNotify, "MapNotify"},
		{MapRequest, "MapRequest"},
		{MappingNotify, "MappingNotify"},
		{MotionNotify, "MotionNotify"},
		{NoExpose, "NoExpose"},
		{PropertyNotify, "PropertyNotify"},
		{ReparentNotify, "ReparentNotify"},
		{ResizeRequest, "ResizeRequest"},
		{SelectionClear, "SelectionClear"},
		{SelectionNotify, "SelectionNotify"},
		{SelectionRequest, "SelectionRequest"},
		{UnmapNotify, "UnmapNotify"},
		{VisibilityNotify, "VisibilityNotify"},
		{0, nil},
	};

	return search(list, key, strdec);
}
/* Returns the string equivalent the keycode contained in the key event */
static char*
Keycode(XKeyEvent * ev)
{
	KeySym		keysym_str;
	char           *keysym_name;

	XLookupString(ev, buffer, sizeof buffer, &keysym_str, NULL);

	if (keysym_str == NoSymbol)
		keysym_name = "NoSymbol";
	else
		keysym_name = XKeysymToString(keysym_str);
	if(keysym_name == nil)
		keysym_name = "(no name)";

	snprintf(buffer, sizeof buffer, "%u (keysym 0x%x \"%s\")",
		(int)ev->keycode, (int)keysym_str, keysym_name);
	return buffer;
}

/* Returns the string equivalent of an atom or "None" */
static char    *
AtomName(Atom atom)
{
	extern Display *display;
	char           *atom_name;

	if (atom == None)
		return "None";

	atom_name = XGetAtomName(display, atom);
	strncpy(buffer, atom_name, sizeof buffer);
	XFree(atom_name);

	return buffer;
}

#define _(m) #m, ev->m

enum {
	TEnd,
	TAtom,
	TBool,
	TColMap,
	TConfDetail,
	TConfMask,
	TFocus,
	TGrabMode,
	TInt,
	TIntNone,
	TMajor,
	TMapping,
	TModState,
	TPlace,
	TPropState,
	TString,
	TTime,
	TVis,
	TWindow,
	TXing,
};

typedef struct TypeTab TypeTab;

struct TypeTab {
	int size;
	char *(*fn)();
} ttab[] = {
	[TEnd] = {0, nil},
	[TAtom] = {sizeof(Atom), AtomName},
	[TBool] = {sizeof(Bool), TorF},
	[TColMap] = {sizeof(int), ColormapState},
	[TConfDetail] = {sizeof(int), ConfigureDetail},
	[TConfMask] = {sizeof(int), ConfigureValueMask},
	[TFocus] = {sizeof(int), FocusChangeDetail},
	[TGrabMode] = {sizeof(int), GrabMode},
	[TIntNone] = {sizeof(int), MaybeNone},
	[TInt] = {sizeof(int), strdec},
	[TMajor] = {sizeof(int), MajorCode},
	[TMapping] = {sizeof(int), MappingRequest},
	[TModState] = {sizeof(int), ButtonAndOrModifierState},
	[TPlace] = {sizeof(int), Place},
	[TPropState] = {sizeof(int), PropertyState},
	[TString] = {sizeof(char*), Self},
	[TTime] = {sizeof(Time), ServerTime},
	[TVis] = {sizeof(int), VisibilityState},
	[TWindow] = {sizeof(Window), strhex},
	[TXing] = {sizeof(int), CrossingDetail},
};

static void
pevent(void *ev, ...) {
	static char buf[4096];
	static char *bend = buf + sizeof(buf);
	va_list ap;
	TypeTab *t;
	char *p, *key, *val;
	int n, type, valint;

	va_start(ap, ev);

	p = buf;
	*p = '\0';
	n = 0;
	for(;;) {
		type = va_arg(ap, int);
		if(type == TEnd)
			break;
		t = &ttab[type];

		key = va_arg(ap, char*);
		switch(t->size) {
		default:
			break; /* Can't continue */
		case sizeof(int):
			valint = va_arg(ap, int);
			val = t->fn(valint);
			break;
		}

		if(n++ != 0)
			p += strlcat(p, sep, bend-p);
		p += snprintf(p, bend-p, "%s=%s", key, val);

		if(p >= bend)
			break;	
	}
	fprintf(stderr, "%s\n", buf);

	va_end(ap);
}

/******************************************************************************/
/**** Routines to print out readable values for the field of various events ***/
/******************************************************************************/

static void
VerbMotion(XMotionEvent *ev)
{
	pevent(ev,
		TWindow, _(window),
		TWindow, _(root),
		TWindow, _(subwindow),
		TTime, _(time),
		TInt, _(x), TInt, _(y),
		TInt, _(x_root), TInt, _(y_root),
		TModState, _(state),
		TBool, _(same_screen),
		TEnd
	);
    //fprintf(stderr, "is_hint=%s%s", IsHint(ev->is_hint), sep);
}

static void
VerbButton(XButtonEvent *ev)
{
	pevent(ev,
		TWindow, _(window),
		TWindow, _(root),
		TWindow, _(subwindow),
		TTime, _(time),
		TInt, _(x), TInt, _(y),
		TInt, _(x_root), TInt, _(y_root),
		TModState, _(state),
		TModState, _(button),
		TBool, _(same_screen),
		TEnd
	);
}

static void
VerbColormap(XColormapEvent *ev)
{
	pevent(ev,
		TWindow, _(window),
		TIntNone, _(colormap),
		TBool, _(new),
		TColMap, _(state),
		TEnd
	);
}

static void
VerbCrossing(XCrossingEvent *ev)
{
	pevent(ev,
		TWindow, _(window),
		TWindow, _(root),
		TWindow, _(subwindow),
		TTime, _(time),
		TInt, _(x), TInt, _(y),
		TInt, _(x_root), TInt, _(y_root),
		TGrabMode, _(mode),
		TXing, _(detail),
		TBool, _(same_screen),
		TBool, _(focus),
		TModState, _(state),
		TEnd
	);
}

static void
VerbExpose(XExposeEvent *ev)
{
	pevent(ev,
		TWindow, _(window),
		TInt, _(x), TInt, _(y),
		TInt, _(width), TInt, _(height),
		TInt, _(count),
		TEnd
	);
}

static void
VerbGraphicsExpose(XGraphicsExposeEvent *ev)
{
	pevent(ev,
		TWindow, _(drawable),
		TInt, _(x), TInt, _(y),
		TInt, _(width), TInt, _(height),
		TMajor, _(major_code),
		TInt, _(minor_code),
		TEnd
	);
}

static void
VerbNoExpose(XNoExposeEvent *ev)
{
	pevent(ev,
		TWindow, _(drawable),
		TMajor, _(major_code),
		TInt, _(minor_code),
		TEnd
	);
}

static void
VerbFocus(XFocusChangeEvent *ev)
{
	pevent(ev,
		TWindow, _(window),
		TGrabMode, _(mode),
		TFocus, _(detail),
		TEnd
	);
}

static void
VerbKeymap(XKeymapEvent * ev)
{
	int		i;

	fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
	fprintf(stderr, "key_vector=");
	for (i = 0; i < 32; i++)
		fprintf(stderr, "%02x", ev->key_vector[i]);
	fprintf(stderr, "\n");
}

static void
VerbKey(XKeyEvent *ev)
{
	pevent(ev,
		TWindow, _(window),
		TWindow, _(root),
		TWindow, _(subwindow),
		TTime, _(time),
		TInt, _(x), TInt, _(y),
		TInt, _(x_root), TInt, _(y_root),
		TModState, _(state),
		TString, "keycode", estrdup(Keycode(ev)),
		TBool, _(same_screen),
		TEnd
	);
}

static void
VerbProperty(XPropertyEvent *ev)
{
	pevent(ev,
		TWindow, _(window),
		TAtom, _(atom),
		TTime, _(time),
		TPropState, _(state),
		TEnd
	);
}

static void
VerbResizeRequest(XResizeRequestEvent *ev)
{
	pevent(ev,
		TWindow, _(window),
		TInt, _(width), TInt, _(height),
		TEnd
	);
}

static void
VerbCirculate(XCirculateEvent *ev)
{
	pevent(ev,
		TWindow, _(event),
		TWindow, _(window),
		TPlace, _(place),
		TEnd
	);
}

static void
VerbConfigure(XConfigureEvent *ev)
{
	pevent(ev,
		TWindow, _(event),
		TWindow, _(window),
		TInt, _(x), TInt, _(y),
		TInt, _(width), TInt, _(height),
		TInt, _(border_width),
		TIntNone, _(above),
		TBool, _(override_redirect),
		TEnd
	);
}

static void
VerbCreateWindow(XCreateWindowEvent *ev)
{
	pevent(ev,
		TWindow, _(parent),
		TWindow, _(window),
		TInt, _(x), TInt, _(y),
		TInt, _(width), TInt, _(height),
		TInt, _(border_width),
		TBool, _(override_redirect),
		TEnd
	);
}

static void
VerbDestroyWindow(XDestroyWindowEvent *ev)
{
	pevent(ev,
		TWindow, _(event),
		TWindow, _(window),
		TEnd
	);
}

static void
VerbGravity(XGravityEvent *ev)
{
	pevent(ev,
		TWindow, _(event),
		TWindow, _(window),
		TInt, _(x), TInt, _(y),
		TEnd
	);
}

static void
VerbMap(XMapEvent *ev)
{
	pevent(ev,
		TWindow, _(event),
		TWindow, _(window),
		TBool, _(override_redirect),
		TEnd
	);
}

static void
VerbReparent(XReparentEvent *ev)
{
	pevent(ev,
		TWindow, _(event),
		TWindow, _(window),
		TWindow, _(parent),
		TInt, _(x), TInt, _(y),
		TBool, _(override_redirect),
		TEnd
	);
}

static void
VerbUnmap(XUnmapEvent *ev)
{
	pevent(ev,
		TWindow, _(event),
		TWindow, _(window),
		TBool, _(from_configure),
		TEnd
	);
}

static void
VerbCirculateRequest(XCirculateRequestEvent *ev)
{
	pevent(ev,
		TWindow, _(parent),
		TWindow, _(window),
		TPlace, _(place),
		TEnd
	);
}

static void
VerbConfigureRequest(XConfigureRequestEvent *ev)
{
	pevent(ev,
		TWindow, _(parent),
		TWindow, _(window),
		TInt, _(x), TInt, _(y),
		TInt, _(width), TInt, _(height),
		TInt, _(border_width),
		TIntNone, _(above),
		TConfDetail, _(detail),
		TConfMask, _(value_mask),
		TEnd
	);
}

static void
VerbMapRequest(XMapRequestEvent *ev)
{
	pevent(ev,
		TWindow, _(parent),
		TWindow, _(window),
		TEnd
	);
}

static void
VerbClient(XClientMessageEvent * ev)
{
	int		i;

	fprintf(stderr, "window=0x%x%s", (int)ev->window, sep);
	fprintf(stderr, "message_type=%s%s", AtomName(ev->message_type), sep);
	fprintf(stderr, "format=%d\n", ev->format);
	fprintf(stderr, "data (shown as longs)=");
	for (i = 0; i < 5; i++)
		fprintf(stderr, " 0x%08lx", ev->data.l[i]);
	fprintf(stderr, "\n");
}

static void
VerbMapping(XMappingEvent *ev)
{
	pevent(ev,
		TWindow, _(window),
		TMapping, _(request),
		TWindow, _(first_keycode),
		TWindow, _(count),
		TEnd
	);
}

static void
VerbSelectionClear(XSelectionClearEvent *ev)
{
	pevent(ev,
		TWindow, _(window),
		TAtom, _(selection),
		TTime, _(time),
		TEnd
	);
}

static void
VerbSelection(XSelectionEvent *ev)
{
	pevent(ev,
		TWindow, _(requestor),
		TAtom, _(selection),
		TAtom, _(target),
		TAtom, _(property),
		TTime, _(time),
		TEnd
	);
}

static void
VerbSelectionRequest(XSelectionRequestEvent *ev)
{
	pevent(ev,
		TWindow, _(owner),
		TWindow, _(requestor),
		TAtom, _(selection),
		TAtom, _(target),
		TAtom, _(property),
		TTime, _(time),
		TEnd
	);
}

static void
VerbVisibility(XVisibilityEvent *ev)
{
	pevent(ev,
		TWindow, _(window),
		TVis, _(state),
		TEnd
	);
}

/******************************************************************************/
/**************** Print the values of all fields for any event ****************/
/******************************************************************************/

typedef struct Handler Handler;

struct Handler {
	int key;
	void (*fn)();
};

void 
printevent(XEvent * e)
{
	extern Display *display;
	XAnyEvent      *ev = (void *)e;
	char           *name;

	if (ev->window) {
		XFetchName(display, ev->window, &name);
		if (name)
			fprintf(stderr, "\ttitle=%s\n", name);
		XFree(name);
	}

	fprintf(stderr, "%3ld %-20s ", ev->serial, eventtype(e->xany.type));
	if (ev->send_event)
		fprintf(stderr, "(sendevent) ");
	if (0) {
		fprintf(stderr, "type=%s%s", eventtype(e->xany.type), sep);
		fprintf(stderr, "serial=%lu%s", ev->serial, sep);
		fprintf(stderr, "send_event=%s%s", TorF(ev->send_event), sep);
		fprintf(stderr, "display=0x%p%s", ev->display, sep);
	}
	static Handler	fns[] = {
		{MotionNotify, VerbMotion},
		{ButtonPress, VerbButton},
		{ButtonRelease, VerbButton},
		{ColormapNotify, VerbColormap},
		{EnterNotify, VerbCrossing},
		{LeaveNotify, VerbCrossing},
		{Expose, VerbExpose},
		{GraphicsExpose, VerbGraphicsExpose},
		{NoExpose, VerbNoExpose},
		{FocusIn, VerbFocus},
		{FocusOut, VerbFocus},
		{KeymapNotify, VerbKeymap},
		{KeyPress, VerbKey},
		{KeyRelease, VerbKey},
		{PropertyNotify, VerbProperty},
		{ResizeRequest, VerbResizeRequest},
		{CirculateNotify, VerbCirculate},
		{ConfigureNotify, VerbConfigure},
		{CreateNotify, VerbCreateWindow},
		{DestroyNotify, VerbDestroyWindow},
		{GravityNotify, VerbGravity},
		{MapNotify, VerbMap},
		{ReparentNotify, VerbReparent},
		{UnmapNotify, VerbUnmap},
		{CirculateRequest, VerbCirculateRequest},
		{ConfigureRequest, VerbConfigureRequest},
		{MapRequest, VerbMapRequest},
		{ClientMessage, VerbClient},
		{MappingNotify, VerbMapping},
		{SelectionClear, VerbSelectionClear},
		{SelectionNotify, VerbSelection},
		{SelectionRequest, VerbSelectionRequest},
		{VisibilityNotify, VerbVisibility},
		{0, nil},
	};
	Handler *p;

	for (p = fns; p->fn; p++)
		if (p->key == ev->type)
			break;
	if (p->fn)
		p->fn(ev);
}
