#include <stuff/base.h>
#include <stuff/x11.h>
#include <fmt.h>

extern void	init_screens(void);

#define Net(x) ("_NET_" x)
#define	Action(x) ("_NET_WM_ACTION_" x)
#define	State(x) ("_NET_WM_STATE_" x)
#define	Type(x) ("_NET_WM_WINDOW_TYPE_" x)
#define NET(x) xatom(Net(x))
#define	ACTION(x) xatom(Action(x))
#define	STATE(x) xatom(State(x))
#define	TYPE(x) xatom(Type(x))

/* printevent.c */
int	fmtevent(Fmt*);

int	fmtkey(Fmt*);

/* xext.c */
void	randr_event(XEvent*);
bool	render_argb_p(Visual*);
void	xext_event(XEvent*);
void	xext_init(void);
Rectangle*	xinerama_screens(int*);

void	client_readconfig(CTuple*, CTuple*, Font**);

#define event_handle(w, fn, ev) \
	_event_handle(w, offsetof(Handlers, fn), (XEvent*)ev)

void	_event_handle(Window*, ulong, XEvent*);

void	event_check(void);
void	event_dispatch(XEvent*);
uint	event_flush(long, bool dispatch);
uint	event_flushenter(void);
void	event_loop(void);
#ifdef IXP_API /* Evil. */
void	event_fdclosed(IxpConn*);
void	event_fdready(IxpConn*);
void	event_preselect(IxpServer*);
#endif
void	event_updatextime(void);

void	event_buttonpress(XButtonPressedEvent*);
void	event_buttonrelease(XButtonPressedEvent*);
void	event_clientmessage(XClientMessageEvent*);
void	event_configurenotify(XConfigureEvent*);
void	event_configurerequest(XConfigureRequestEvent*);
void	event_destroynotify(XDestroyWindowEvent*);
void	event_enternotify(XCrossingEvent*);
void	event_expose(XExposeEvent*);
void	event_focusin(XFocusChangeEvent*);
void	event_focusout(XFocusChangeEvent*);
void	event_keypress(XKeyEvent*);
void	event_keyrelease(XKeyEvent*);
void	event_leavenotify(XCrossingEvent*);
void	event_mapnotify(XMapEvent*);
void	event_mappingnotify(XMappingEvent*);
void	event_maprequest(XMapRequestEvent*);
void	event_motionnotify(XMotionEvent*);
void	event_propertynotify(XPropertyEvent*);
void	event_reparentnotify(XReparentEvent *ev);
void	event_selection(XSelectionEvent*);
void	event_selectionclear(XSelectionClearEvent*);
void	event_selectionrequest(XSelectionRequestEvent*);
void	event_unmapnotify(XUnmapEvent*);

extern long	event_xtime;
extern bool	event_looprunning;
extern void	(*event_debug)(XEvent*);

extern Visual*	render_visual;
extern bool	have_RandR;
extern bool	have_render;
extern bool	have_xinerama;

