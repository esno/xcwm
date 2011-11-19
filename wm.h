// don't forget MIT license
#ifndef _WM_H_
#define _WM_H_

#include <stdbool.h>
#include <xcb/xcb.h>

typedef struct Monitor Monitor;
typedef struct Client Client;

struct Mgr {
	xcb_connection_t *connection;
	xcb_screen_t *screen;
	Monitor *mstack;
	bool running;
	struct {
		bool focusoncreate;
	} pref;
};

struct Monitor {
	Monitor *next;
	Monitor *prev;
	Client *cstack;
	struct {
		bool bottomstack;
		double mastersize;
		int x, y, w, h;
	} pref;
};

struct Client {
	xcb_window_t window;
	Monitor *monitor;
	Client *next;
	Client *prev;
	struct {
		bool focus;
	} pref;
};

typedef union {
	int i;
	const char **c;
} Arg;

typedef struct {
	unsigned int mod;
	unsigned int keysym;
	void (*func) (const Arg *);
	const Arg arg;
} Key;

#endif /* _WM_H_ */
