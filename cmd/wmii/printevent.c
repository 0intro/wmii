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

#include "dat.h"
#include <stdarg.h>
#include <bio.h>
//#include "fns.h"
#include "printevent.h"
#include <X11/Xproto.h>
#define Window XWindow
#include <X11/Intrinsic.h>

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

static char*
unmask(Pair * list, uint val)
{
	Pair  *p;
	char *s, *end;
	int n;

	buffer[0] = '\0';
	end = buffer + sizeof buffer;
	s = buffer;

	n = 0;
	s = utfecpy(s, end, "(");
	for (p = list; p->val; p++)
		if (val & p->key) {
			if(n++)
				s = utfecpy(s, end, "|");
			s = utfecpy(s, end, p->val);
		}
	utfecpy(s, end, ")");

	return buffer;
}

static char *
strhex(int key) {
	sprint(buffer, "0x%x", key);
	return buffer;
}

static char *
strdec(int key) {
	sprint(buffer, "%d", key);
	return buffer;
}

static char *
strign(int key) {
	USED(key);

	return "?";
}

/******************************************************************************/
/**** Miscellaneous routines to convert values to their string equivalents ****/
/******************************************************************************/

static void
TInt(Biobuf *b, va_list *ap) {
	Bprint(b, "%d", va_arg(*ap, int));
}

static void
TWindow(Biobuf *b, va_list *ap) {
	Window w;

	w = va_arg(*ap, Window);
	Bprint(b, "0x%ux", (uint)w);
}

static void
TData(Biobuf *b, va_list *ap) {
	long *l;
	int i;

	l = va_arg(*ap, long*);
	Bprint(b, "{");
	for (i = 0; i < 5; i++) {
		if(i > 0)
			Bprint(b, ", ");
		Bprint(b, "0x%08lx", l[i]);
	}
	Bprint(b, "}");
}

/* Returns the string equivalent of a timestamp */
static void
TTime(Biobuf *b, va_list *ap) {
	ldiv_t	d;
	ulong   msec;
	ulong   sec;
	ulong   min;
	ulong   hr;
	ulong   day;
	Time time;

	time = va_arg(*ap, Time);

	msec = time/1000;
	d = ldiv(msec, 60);
	msec = time-msec*1000;

	sec = d.rem;
	d = ldiv(d.quot, 60);
	min = d.rem;
	d = ldiv(d.quot, 24);
	hr = d.rem;
	day = d.quot;

#ifdef notdef
	sprintf(buffer, "%lu day%s %02lu:%02lu:%02lu.%03lu",
		day, day == 1 ? "" : "(s)", hr, min, sec, msec);
#endif

	Bprint(b, "%ludd_%ludh_%ludm_%lud.%03luds", day, hr, min, sec, msec);
}

/* Returns the string equivalent of a boolean parameter */
static void
TBool(Biobuf *b, va_list *ap) {
	static Pair list[] = {
		{True, "True"},
		{False, "False"},
		{0, nil},
	};
	Bool key;

	key = va_arg(*ap, Bool);
	Bprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a property notify state */
static void
TPropState(Biobuf *b, va_list *ap) {
	static Pair list[] = {
		{PropertyNewValue, "PropertyNewValue"},
		{PropertyDelete, "PropertyDelete"},
		{0, nil},
	};
	uint key;

	key = va_arg(*ap, uint);
	Bprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a visibility notify state */
static void
TVis(Biobuf *b, va_list *ap) {
	static Pair list[] = {
		{VisibilityUnobscured, "VisibilityUnobscured"},
		{VisibilityPartiallyObscured, "VisibilityPartiallyObscured"},
		{VisibilityFullyObscured, "VisibilityFullyObscured"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	Bprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a mask of buttons and/or modifier keys */
static void
TModState(Biobuf *b, va_list *ap) {
	static Pair list[] = {
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
	uint state;

	state = va_arg(*ap, uint);
	Bprint(b, "%s", unmask(list, state));
}

/* Returns the string equivalent of a mask of configure window values */
static void
TConfMask(Biobuf *b, va_list *ap) {
	static Pair list[] = {
		{CWX, "CWX"},
		{CWY, "CWY"},
		{CWWidth, "CWWidth"},
		{CWHeight, "CWHeight"},
		{CWBorderWidth, "CWBorderWidth"},
		{CWSibling, "CWSibling"},
		{CWStackMode, "CWStackMode"},
		{0, nil},
	};
	uint valuemask;

	valuemask = va_arg(*ap, uint);
	Bprint(b, "%s", unmask(list, valuemask));
}

/* Returns the string equivalent of a motion hint */
#if 0
static void
IsHint(Biobuf *b, va_list *ap) {
	static Pair list[] = {
		{NotifyNormal, "NotifyNormal"},
		{NotifyHint, "NotifyHint"},
		{0, nil},
	};
	char key;

	key = va_arg(*ap, char);
	Bprint(b, "%s", search(list, key, strign));
}
#endif

/* Returns the string equivalent of an id or the value "None" */
static void
TIntNone(Biobuf *b, va_list *ap) {
	static Pair list[] = {
		{None, "None"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	Bprint(b, "%s", search(list, key, strhex));
}

/* Returns the string equivalent of a colormap state */
static void
TColMap(Biobuf *b, va_list *ap) {
	static Pair list[] = {
		{ColormapInstalled, "ColormapInstalled"},
		{ColormapUninstalled, "ColormapUninstalled"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	Bprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a crossing detail */
static void
TXing(Biobuf *b, va_list *ap) {
	static Pair list[] = {
		{NotifyAncestor, "NotifyAncestor"},
		{NotifyInferior, "NotifyInferior"},
		{NotifyVirtual, "NotifyVirtual"},
		{NotifyNonlinear, "NotifyNonlinear"},
		{NotifyNonlinearVirtual, "NotifyNonlinearVirtual"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	Bprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a focus change detail */
static void
TFocus(Biobuf *b, va_list *ap) {
	static Pair list[] = {
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
	int key;

	key = va_arg(*ap, int);
	Bprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a configure detail */
static void
TConfDetail(Biobuf *b, va_list *ap) {
	static Pair list[] = {
		{Above, "Above"},
		{Below, "Below"},
		{TopIf, "TopIf"},
		{BottomIf, "BottomIf"},
		{Opposite, "Opposite"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	Bprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a grab mode */
static void
TGrabMode(Biobuf *b, va_list *ap) {
	static Pair list[] = {
		{NotifyNormal, "NotifyNormal"},
		{NotifyGrab, "NotifyGrab"},
		{NotifyUngrab, "NotifyUngrab"},
		{NotifyWhileGrabbed, "NotifyWhileGrabbed"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	Bprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a mapping request */
static void
TMapping(Biobuf *b, va_list *ap) {
	static Pair list[] = {
		{MappingModifier, "MappingModifier"},
		{MappingKeyboard, "MappingKeyboard"},
		{MappingPointer, "MappingPointer"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	Bprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a stacking order place */
static void
TPlace(Biobuf *b, va_list *ap) {
	static Pair list[] = {
		{PlaceOnTop, "PlaceOnTop"},
		{PlaceOnBottom, "PlaceOnBottom"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	Bprint(b, "%s", search(list, key, strign));
}

/* Returns the string equivalent of a major code */
static void
TMajor(Biobuf *b, va_list *ap) {
	static Pair list[] = {
		{X_CopyArea, "X_CopyArea"},
		{X_CopyPlane, "X_CopyPlane"},
		{0, nil},
	};
	int key;

	key = va_arg(*ap, int);
	Bprint(b, "%s", search(list, key, strhex));
}

static char*
eventtype(int key) {
	static Pair list[] = {
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
static void
TKeycode(Biobuf *b, va_list *ap) {
	KeySym keysym_str;
	XKeyEvent *ev;
	char *keysym_name;

	ev = va_arg(*ap, XKeyEvent*);

	XLookupString(ev, buffer, sizeof buffer, &keysym_str, nil);

	if (keysym_str == NoSymbol)
		keysym_name = "NoSymbol";
	else
		keysym_name = XKeysymToString(keysym_str);
	if(keysym_name == nil)
		keysym_name = "(no name)";

	Bprint(b, "%ud (keysym 0x%x \"%s\")", (int)ev->keycode,
			(int)keysym_str, keysym_name);
}

/* Returns the string equivalent of an atom or "None" */
static void
TAtom(Biobuf *b, va_list *ap) {
	char *atom_name;
	Atom atom;

	atom = va_arg(*ap, Atom);
	atom_name = XGetAtomName(display, atom);
	Bprint(b, "%s", atom_name);
	XFree(atom_name);
}

#define _(m) #m, ev->m
#define TEnd nil
typedef void (*Tfn)(Biobuf*, va_list*);

static void
pevent(void *ev, ...) {
	static Biobuf *b;
	va_list ap;
	Tfn fn;
	char *key;
	int n;

	if(b == nil)
		b = Bfdopen(2, O_WRONLY);

	n = 0;
	va_start(ap, ev);
	for(;;) {
		fn = va_arg(ap, Tfn);
		if(fn == TEnd)
			break;

		if(n++ != 0)
			Bprint(b, "%s", sep);

		key = va_arg(ap, char*);
		Bprint(b, "%s=", key);
		fn(b, &ap);
	}
	va_end(ap);

	Bprint(b, "\n");
	Bflush(b);
}

/******************************************************************************/
/**** Routines to print out readable values for the field of various events ***/
/******************************************************************************/

static void
VerbMotion(XEvent *e) {
	XMotionEvent *ev = &e->xmotion;

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
VerbButton(XEvent *e) {
	XButtonEvent *ev = &e->xbutton;

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
VerbColormap(XEvent *e) {
	XColormapEvent *ev = &e->xcolormap;

	pevent(ev,
		TWindow, _(window),
		TIntNone, _(colormap),
		TBool, _(new),
		TColMap, _(state),
		TEnd
	);
}

static void
VerbCrossing(XEvent *e) {
	XCrossingEvent *ev = &e->xcrossing;

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
VerbExpose(XEvent *e) {
	XExposeEvent *ev = &e->xexpose;

	pevent(ev,
		TWindow, _(window),
		TInt, _(x), TInt, _(y),
		TInt, _(width), TInt, _(height),
		TInt, _(count),
		TEnd
	);
}

static void
VerbGraphicsExpose(XEvent *e) {
	XGraphicsExposeEvent *ev = &e->xgraphicsexpose;

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
VerbNoExpose(XEvent *e) {
	XNoExposeEvent *ev = &e->xnoexpose;

	pevent(ev,
		TWindow, _(drawable),
		TMajor, _(major_code),
		TInt, _(minor_code),
		TEnd
	);
}

static void
VerbFocus(XEvent *e) {
	XFocusChangeEvent *ev = &e->xfocus;

	pevent(ev,
		TWindow, _(window),
		TGrabMode, _(mode),
		TFocus, _(detail),
		TEnd
	);
}

static void
VerbKeymap(XEvent *e) {
	XKeymapEvent *ev = &e->xkeymap;
	int i;

	fprint(2, "window=0x%x%s", (int)ev->window, sep);
	fprint(2, "key_vector=");
	for (i = 0; i < 32; i++)
		fprint(2, "%02x", ev->key_vector[i]);
	fprint(2, "\n");
}

static void
VerbKey(XEvent *e) {
	XKeyEvent *ev = &e->xkey;

	pevent(ev,
		TWindow, _(window),
		TWindow, _(root),
		TWindow, _(subwindow),
		TTime, _(time),
		TInt, _(x), TInt, _(y),
		TInt, _(x_root), TInt, _(y_root),
		TModState, _(state),
		TKeycode, "keycode", ev,
		TBool, _(same_screen),
		TEnd
	);
}

static void
VerbProperty(XEvent *e) {
	XPropertyEvent *ev = &e->xproperty;

	pevent(ev,
		TWindow, _(window),
		TAtom, _(atom),
		TTime, _(time),
		TPropState, _(state),
		TEnd
	);
}

static void
VerbResizeRequest(XEvent *e) {
	XResizeRequestEvent *ev = &e->xresizerequest;

	pevent(ev,
		TWindow, _(window),
		TInt, _(width), TInt, _(height),
		TEnd
	);
}

static void
VerbCirculate(XEvent *e) {
	XCirculateEvent *ev = &e->xcirculate;

	pevent(ev,
		TWindow, _(event),
		TWindow, _(window),
		TPlace, _(place),
		TEnd
	);
}

static void
VerbConfigure(XEvent *e) {
	XConfigureEvent *ev = &e->xconfigure;

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
VerbCreateWindow(XEvent *e) {
	XCreateWindowEvent *ev = &e->xcreatewindow;

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
VerbDestroyWindow(XEvent *e) {
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	pevent(ev,
		TWindow, _(event),
		TWindow, _(window),
		TEnd
	);
}

static void
VerbGravity(XEvent *e) {
	XGravityEvent *ev = &e->xgravity;

	pevent(ev,
		TWindow, _(event),
		TWindow, _(window),
		TInt, _(x), TInt, _(y),
		TEnd
	);
}

static void
VerbMap(XEvent *e) {
	XMapEvent *ev = &e->xmap;

	pevent(ev,
		TWindow, _(event),
		TWindow, _(window),
		TBool, _(override_redirect),
		TEnd
	);
}

static void
VerbReparent(XEvent *e) {
	XReparentEvent *ev = &e->xreparent;

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
VerbUnmap(XEvent *e) {
	XUnmapEvent *ev = &e->xunmap;

	pevent(ev,
		TWindow, _(event),
		TWindow, _(window),
		TBool, _(from_configure),
		TEnd
	);
}

static void
VerbCirculateRequest(XEvent *e) {
	XCirculateRequestEvent *ev = &e->xcirculaterequest;

	pevent(ev,
		TWindow, _(parent),
		TWindow, _(window),
		TPlace, _(place),
		TEnd
	);
}

static void
VerbConfigureRequest(XEvent *e) {
	XConfigureRequestEvent *ev = &e->xconfigurerequest;

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
VerbMapRequest(XEvent *e) {
	XMapRequestEvent *ev = &e->xmaprequest;

	pevent(ev,
		TWindow, _(parent),
		TWindow, _(window),
		TEnd
	);
}

static void
VerbClient(XEvent *e) {
	XClientMessageEvent *ev = &e->xclient;

	pevent(ev,
		TWindow, _(window),
		TAtom, _(message_type),
		TInt, _(format),
		TData, "data (as longs)", &ev->data,
		TEnd
	);
}

static void
VerbMapping(XEvent *e) {
	XMappingEvent *ev = &e->xmapping;

	pevent(ev,
		TWindow, _(window),
		TMapping, _(request),
		TWindow, _(first_keycode),
		TWindow, _(count),
		TEnd
	);
}

static void
VerbSelectionClear(XEvent *e) {
	XSelectionClearEvent *ev = &e->xselectionclear;

	pevent(ev,
		TWindow, _(window),
		TAtom, _(selection),
		TTime, _(time),
		TEnd
	);
}

static void
VerbSelection(XEvent *e) {
	XSelectionEvent *ev = &e->xselection;

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
VerbSelectionRequest(XEvent *e) {
	XSelectionRequestEvent *ev = &e->xselectionrequest;

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
VerbVisibility(XEvent *e) {
	XVisibilityEvent *ev = &e->xvisibility;

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
	void (*fn)(XEvent*);
};

void 
printevent(XEvent *e) {
	XAnyEvent *ev = &e->xany;

	fprint(2, "%3ld %-20s ", ev->serial, eventtype(e->xany.type));
	if(ev->send_event)
		fprint(2, "(sendevent) ");
	/*
		fprintf(stderr, "type=%s%s", eventtype(e->xany.type), sep);
		fprintf(stderr, "serial=%lu%s", ev->serial, sep);
		fprintf(stderr, "send_event=%s%s", TorF(ev->send_event), sep);
		fprintf(stderr, "display=0x%p%s", ev->display, sep);
	*/
	static Handler fns[] = {
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
		if (p->key == ev->type) {
			p->fn(e);
			break;
		}
}
