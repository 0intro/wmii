
#define Net(x) ("_NET_" x)
#define	Action(x) ("_NET_WM_ACTION_" x)
#define	State(x) ("_NET_WM_STATE_" x)
#define	Type(x) ("_NET_WM_WINDOW_TYPE_" x)
#define NET(x) xatom(Net(x))
#define	ACTION(x) xatom(Action(x))
#define	STATE(x) xatom(State(x))
#define	TYPE(x) xatom(Type(x))

void	cleanup(Selection*);
Client*	client_find(Window*);
bool	client_hasmessage(Client*);
void	client_disown(Client*);
void	client_manage(XWindow);
void	client_message(Client*, long, int, ClientMessageData*);
void	client_opcode(Client*, long, long, long, long);
void	ewmh_setstrut(Window*, Rectangle[4]);
int	main(int, char*[]);
void	message(Selection*, XClientMessageEvent*);
void	message_cancel(Client*, long);
void	restrut(Window*, int);
Selection*	selection_create(char*, ulong, void (*)(Selection*, XSelectionRequestEvent*), void (*)(Selection*));
Selection*	selection_manage(char*, ulong, void (*)(Selection*, XClientMessageEvent*), void (*)(Selection*));
void	selection_release(Selection*);
void	tray_init(void);
void	tray_resize(Rectangle);
void	tray_update(void);
void	xembed_disown(XEmbed*);
XEmbed*	xembed_swallow(Window*, Window*, void (*)(XEmbed*));

#define Debug if(debug)
#define Dprint Debug print

