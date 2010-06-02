
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

