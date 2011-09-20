typedef struct Selection	Selection;

struct Selection {
	Window*	owner;
	char*	selection;
	ulong	time_start;
	ulong	time_end;
	void	(*cleanup)(Selection*);
	void	(*message)(Selection*, XClientMessageEvent*);
	void	(*request)(Selection*, XSelectionRequestEvent*);
	long	timer;
	ulong	oldowner;
};

Selection*	selection_create(char*, ulong, void (*)(Selection*, XSelectionRequestEvent*), void (*)(Selection*));
Selection*	selection_manage(char*, ulong, void (*)(Selection*, XClientMessageEvent*), void (*)(Selection*), bool);
void		selection_release(Selection*);

