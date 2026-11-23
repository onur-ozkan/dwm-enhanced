/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK (ButtonPressMask | ButtonReleaseMask)
#define CLEANMASK(mask) (mask & ~(numlockmask | LockMask) & (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask))
#define INTERSECT(x, y, w, h, m) (MAX(0, MIN((x) + (w), (m)->wx + (m)->ww) - MAX((x), (m)->wx)) * MAX(0, MIN((y) + (h), (m)->wy + (m)->wh) - MAX((y), (m)->wy)))
#define ISVISIBLE(C) ((C->tags & C->mon->tagset[C->mon->seltags]))
#define LENGTH(X) (sizeof X / sizeof X[0])
#define MOUSEMASK (BUTTONMASK | PointerMotionMask)
#define WIDTH(X) ((X)->w + 2 * (X)->bw)
#define HEIGHT(X) ((X)->h + 2 * (X)->bw)
#define BARWIDTH(M) MAX(1, (M)->ww - 2 * (sp + barborderpx))
#define BARHEIGHT (bh + 2 * barborderpx)
#define TAGMASK ((1 << LENGTH(tags)) - 1)
#define TEXTW(X) (drw_fontset_getwidth(drw, (X)) + lrpad)

/* enums */
enum
{
	CurNormal,
	CurResize,
	CurMove,
	CurLast
};								/* cursor */
enum
{
	SchemeNorm,
	SchemeSel,
	SchemeBlue,
	SchemeSwitcherWorkspace,
	SchemeSwitcherFilter,
	SchemeSwitcherBorder
};								/* color schemes */
enum
{
	NetSupported,
	NetWMName,
	NetWMState,
	NetWMCheck,
	NetWMFullscreen,
	NetActiveWindow,
	NetWMWindowType,
	NetWMWindowTypeDialog,
	NetClientList,
	NetLast
};								/* EWMH atoms */
enum
{
	WMProtocols,
	WMDelete,
	WMState,
	WMTakeFocus,
	WMLast
};								/* default atoms */
enum
{
	ClkTagBar,
	ClkLtSymbol,
	ClkStatusText,
	ClkWinTitle,
	ClkClientWin,
	ClkRootWin,
	ClkLast
};								/* clicks */

typedef union
{
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef struct
{
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg * arg);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client
{
	char name[256];
	char app[256];
	float mina, maxa;
	int x, y, w, h;
	int fsx, fsy, fsw, fsh;		/* geometry to restore after fullscreen */
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int bw, oldbw;
	unsigned int tags;
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
	int isworkspacefullscreen;
	Client *next;
	Client *snext;
	Monitor *mon;
	Window win;
};

typedef struct
{
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct
{
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

typedef struct Pertag Pertag;

struct Monitor
{
	char ltsymbol[16];
	float mfact;
	int nmaster;
	int num;
	int by;						/* bar geometry */
	int mx, my, mw, mh;			/* screen size */
	int wx, wy, ww, wh;			/* window area */
	int gappx;					/* gaps between windows */
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	int showbar;
	int topbar;
	int bardirty;
	int ltend, statusx;		/* boundaries from the last bar render */
	Client *clients;
	Client *sel;
	Client *stack;
	Monitor *next;
	Window barwin;
	const Layout *lt[2];
	Pertag *pertag;
};

typedef struct
{
	const char *class;
	const char *instance;
	const char *title;
	unsigned int tags;
	int isfloating;
	int monitor;
} Rule;

/* function declarations */
static void applypertag(void);
static void applyrules(Client * c);
static int applysizehints(Client * c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor * m);
static void arrangemon(Monitor * m);
static void attach(Client * c);
static void attachstack(Client * c);
static void buttonpress(XEvent * e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor * mon);
static void clientmessage(XEvent * e);
static void coalescemotion(XEvent * e);
static void configure(Client * c);
static void configurenotify(XEvent * e);
static void configurerequest(XEvent * e);
static Monitor *createmon(void);
static void destroynotify(XEvent * e);
static void detach(Client * c);
static void detachstack(Client * c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor * m);
static void drawbars(void);
static int drawstatusbar(Monitor * m, int bh, char *text);
static void expose(XEvent * e);
static void focus(Client * c);
static void focusin(XEvent * e);
static void focusmon(const Arg * arg);
static void focusstack(const Arg * arg);
static void flushbars(void);
static Atom *getatomprop(Client * c, Atom prop, unsigned long *count);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client * c, int focused);
static void grabkeys(void);
static int hasatomprop(Client * c, Atom prop, Atom atom);
static void keypress(XEvent * e);
static void killclient(const Arg * arg);
static void manage(Window w, XWindowAttributes * wa);
static void mappingnotify(XEvent * e);
static void maprequest(XEvent * e);
static void dragmouse(const Arg * arg);
static Client *nextclient(int direction, int tiled);
static Client *nexttiled(Client * c);
static void pop(Client *);
static void propertynotify(XEvent * e);
static void quit(const Arg * arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void renderbar(Monitor * m);
static void resize(Client * c, int x, int y, int w, int h, int interact);
static void resizeclient(Client * c, int x, int y, int w, int h);
static void restack(Monitor * m);
static void run(void);
static void runautostart(void);
static void scan(void);
static int sendevent(Client * c, Atom proto);
static void sendmon(Client * c, Monitor * m);
static void setclientstate(Client * c, long state);
static void setfocus(Client * c);
static void setfullscreen(Client * c, int fullscreen);
static void setgaps(const Arg * arg);
static void resetgaps(const Arg * arg);
static void setlayout(const Arg * arg);
static void setmfact(const Arg * arg);
static void setup(void);
static void seturgent(Client * c, int urg);
static void showhide(Client * c);
static void sigchld(int unused);
static void spawn(const Arg * arg);
static int statusparse(char *text, int x, int h, int render);
static void switcheropen(const Arg *arg);
static void tag(const Arg * arg);
static void tagmon(const Arg * arg);
static void tile(Monitor *);
static void togglebar(const Arg * arg);
static void togglefloating(const Arg * arg);
static void togglefullscr(const Arg * arg);
static void toggletag(const Arg * arg);
static void toggleview(const Arg * arg);
static void unfocus(Client * c, int setfocus);
static void unmanage(Client * c, int destroyed);
static void unmapnotify(XEvent * e);
static void updatebarpos(Monitor * m);
static void updatebars(void);
static void updateclientlist(void);
static void updateclass(Client *c);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client * c);
static void updatestatus(void);
static void updatetitle(Client * c);
static void updatewindowtype(Client * c);
static void updatewmhints(Client * c);
static void view(const Arg * arg);
static void viewclient(const Arg *arg, Client *c);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static int xerror(Display * dpy, XErrorEvent * ee);
static int xerrordummy(Display * dpy, XErrorEvent * ee);
static int xerrorstart(Display * dpy, XErrorEvent * ee);
static void zoom(const Arg * arg);

/* variables */
static const char autostartblocksh[] = "autostart_blocking.sh";
static const char autostartsh[] = "autostart.sh";
static const char broken[] = "broken";
static const char dwmdir[] = "dwm";
static const char localshare[] = ".local/share";
static char stext[1024];
static int screen;
static int sw, sh;				/* X display screen geometry width, height */
static int bh;					/* bar height */
static int lrpad;				/* sum of left and right padding for text */
static int vp;					/* vertical padding for bar */
static int sp;					/* side padding for bar */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent])(XEvent *) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify
};

static Atom wmatom[WMLast], netatom[NetLast];
static int running = 1;
static Cursor cursor[CurLast];
static Clr **scheme;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static Window root, wmcheckwin;


/* configuration, allows nested code to access above variables */
#include "config.h"
#include "switcher.c"

struct Pertag
{
	unsigned int curtag, prevtag;
	float mfacts[LENGTH(tags) + 1];
	unsigned int sellts[LENGTH(tags) + 1];
	const Layout *ltidxs[LENGTH(tags) + 1][2];
	int showbars[LENGTH(tags) + 1];
	int fullscreens[LENGTH(tags) + 1];
};

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags
{
	char limitexceeded[LENGTH(tags) > 31 ? -1 : 1];
};

/* function implementations */
void applyrules(Client * c)
{
	const char *class, *instance;
	unsigned int i;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->isfloating = 0;
	c->tags = 0;
	XGetClassHint(dpy, c->win, &ch);
	class = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name ? ch.res_name : broken;

	for (i = 0; i < LENGTH(rules); i++)
	{
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title)) && (!r->class || strstr(class, r->class))
			&& (!r->instance || strstr(instance, r->instance)))
		{
			c->isfloating = r->isfloating;
			c->tags |= r->tags;
			for (m = mons; m && m->num != r->monitor; m = m->next)
				;
			if (m)
				c->mon = m;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int applysizehints(Client * c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact)
	{
		if (*x > sw)
			*x = sw - WIDTH(c);
		if (*y > sh)
			*y = sh - HEIGHT(c);
		if (*x + *w + 2 * c->bw < 0)
			*x = 0;
		if (*y + *h + 2 * c->bw < 0)
			*y = 0;
	}
	else
	{
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}
	if (*h < bh)
		*h = bh;
	if (*w < bh)
		*w = bh;
	if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange)
	{
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if (!baseismin)
		{						/* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0)
		{
			if (c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if (c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}
		if (baseismin)
		{						/* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value */
		if (c->incw)
			*w -= *w % c->incw;
		if (c->inch)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void arrange(Monitor * m)
{
	if (m)
		showhide(m->stack);
	else
		for (m = mons; m; m = m->next)
			showhide(m->stack);
	if (m)
	{
		arrangemon(m);
		restack(m);
	}
	else
		for (m = mons; m; m = m->next)
			arrangemon(m);
}

void arrangemon(Monitor * m)
{
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
}

void attach(Client * c)
{
	c->next = c->mon->clients;
	c->mon->clients = c;
}

void attachstack(Client * c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void buttonpress(XEvent * e)
{
	unsigned int i, x, click, occ = 0;
	Arg arg = { 0 };
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;

	click = ClkRootWin;
	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != selmon &&
		(focusonwheel || (ev->button != Button4 && ev->button != Button5)))
	{
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	if (ev->window == selmon->barwin)
	{
		flushbars();
		i = x = 0;
		for (c = m->clients; c; c = c->next)
			occ |= c->tags == 255 ? 0 : c->tags;
		do
		{
			/* do not reserve space for vacant tags */
			if (!(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
				continue;
			x += TEXTW(tags[i]);
		}
		while (ev->x >= x && ++i < LENGTH(tags));
		if (i < LENGTH(tags))
		{
			click = ClkTagBar;
			arg.ui = 1 << i;
		}
		else if (ev->x < m->ltend)
			click = ClkLtSymbol;
		else if (ev->x >= m->statusx)
			click = ClkStatusText;
		else
			click = ClkWinTitle;
	}
	else if ((c = wintoclient(ev->window)))
	{
		if (focusonwheel || (ev->button != Button4 && ev->button != Button5))
			focus(c);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
			&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void cleanup(void)
{
	Arg a = {.ui = ~0 };
	Layout foo = { "", NULL };
	Monitor *m;
	size_t i;

	switcherclose();
	view(&a);
	selmon->lt[selmon->sellt] = &foo;
	for (m = mons; m; m = m->next)
	{
		while (m->stack)
			unmanage(m->stack, 0);
	}
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	while (mons)
		cleanupmon(mons);
	for (i = 0; i < CurLast; i++)
		XFreeCursor(dpy, cursor[i]);
	for (i = 0; i < LENGTH(colors) + 1; i++)
		free(scheme[i]);
	free(scheme);
	XDestroyWindow(dpy, wmcheckwin);
	drw_free(drw);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void cleanupmon(Monitor * mon)
{
	Monitor **link;

	for (link = &mons; *link != mon; link = &(*link)->next)
		;
	*link = mon->next;
	if (mon->barwin)
		XDestroyWindow(dpy, mon->barwin);
	free(mon->pertag);
	free(mon);
}

void clientmessage(XEvent * e)
{
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState])
	{
		if (cme->data.l[1] == netatom[NetWMFullscreen]
			|| cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1	/* _NET_WM_STATE_ADD */
							  || (cme->data.l[0] == 2	/* _NET_WM_STATE_TOGGLE */
								  && !c->isfullscreen)));
	}
	else if (cme->message_type == netatom[NetActiveWindow])
	{
		if (c != selmon->sel && !c->isurgent)
			seturgent(c, 1);
	}
}

void configure(Client * c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *) & ce);
}

void configurenotify(XEvent * e)
{
	Monitor *m;
	Client *c;
	XConfigureEvent *ev = &e->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == root)
	{
		switcherclose();
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if (updategeom() || dirty)
		{
			drw_resize(drw, sw, bh);
			updatebars();
			for (m = mons; m; m = m->next)
			{
				for (c = m->clients; c; c = c->next)
					if (c->isfullscreen)
						resizeclient(c, m->mx, m->my, m->mw, m->mh);
				XMoveResizeWindow(dpy, m->barwin, m->wx + sp, m->by + vp, BARWIDTH(m), bh);

			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void configurerequest(XEvent * e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = wintoclient(ev->window)))
	{
		if (c->isfloating || !c->mon->lt[c->mon->sellt]->arrange)
		{
			m = c->mon;
			if (ev->value_mask & CWX)
				c->x = m->mx + ev->x;
			if (ev->value_mask & CWY)
				c->y = m->my + ev->y;
			if (ev->value_mask & CWWidth)
				c->w = ev->width;
			if (ev->value_mask & CWHeight)
				c->h = ev->height;
			if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2);	/* center in x direction */
			if ((c->y + c->h) > m->my + m->mh && c->isfloating)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2);	/* center in y direction */
			if ((ev->value_mask & CWBorderWidth) ||
				((ev->value_mask & (CWX | CWY)) && !(ev->value_mask & (CWWidth | CWHeight))))
				configure(c);
			if (ISVISIBLE(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
		}
		else
			configure(c);
	}
	else
	{
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
}

Monitor *createmon(void)
{
	Monitor *m;
	unsigned int i;

	m = ecalloc(1, sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = 1;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->showbar = showbar;
	m->topbar = topbar;
	m->gappx = gappx;
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
	m->pertag = ecalloc(1, sizeof(Pertag));
	m->pertag->curtag = m->pertag->prevtag = 1;

	for (i = 0; i <= LENGTH(tags); i++)
	{
		m->pertag->mfacts[i] = m->mfact;

		m->pertag->ltidxs[i][0] = m->lt[0];
		m->pertag->ltidxs[i][1] = m->lt[1];
		m->pertag->sellts[i] = m->sellt;

		m->pertag->showbars[i] = m->showbar;
	}
	return m;
}

void setgaps(const Arg * arg)
{
	if ((arg->i == 0) || (selmon->gappx + arg->i < 0))
		selmon->gappx = 0;
	else
		selmon->gappx += arg->i;
	arrange(selmon);
}

void resetgaps(const Arg * arg)
{
	selmon->gappx = arg->i;
	arrange(selmon);
}

void destroynotify(XEvent * e)
{
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((c = wintoclient(ev->window)))
		unmanage(c, 1);
}

void detach(Client * c)
{
	Client **tc;

	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next)
		;
	*tc = c->next;
}

void detachstack(Client * c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext)
		;
	*tc = c->snext;

	if (c == c->mon->sel)
	{
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext)
			;
		c->mon->sel = t;
	}
}

/* The measuring pass validates everything before the drawing pass makes changes. */
int statusparse(char *text, int x, int h, int render)
{
	char *p, *end, *next, color[8];
	long value;
	int i, fields, command, first, w, values[4];

	while (*text)
	{
		p = strchr(text, '^');
		if (p)
			*p = '\0';
		w = TEXTW(text) - lrpad;
		if (render)
			drw_text(drw, x, 0, w, h, 0, text, 0);
		x += w;
		if (!p)
			break;
		*p++ = '^';
		end = strchr(p, '^');
		if (!end || p == end)
			return INT_MIN;
		first = 1;
		while (p < end)
		{
			command = *p++;
			if (command == 'c' || command == 'b')
			{
				if (end - p < 7 || *p != '#')
					return INT_MIN;
				for (i = 1; i < 7; i++)
					if (!isxdigit((unsigned char)p[i]))
						return INT_MIN;
				memcpy(color, p, 7);
				color[7] = '\0';
				if (render)
					drw_clr_create(drw, &drw->scheme[command == 'c' ? ColFg : ColBg], color);
				p += 7;
			}
			else if (command == 'r' || command == 'f')
			{
				fields = command == 'r' ? 4 : 1;
				for (i = 0; i < fields; i++)
				{
					if (p >= end || (!isdigit((unsigned char)*p) && *p != '-' && *p != '+'))
						return INT_MIN;
					errno = 0;
					value = strtol(p, &next, 10);
					if (next == p || next > end || errno == ERANGE ||
						value < SHRT_MIN || value > USHRT_MAX ||
						(command == 'r' && i >= 2 && value < 0))
						return INT_MIN;
					values[i] = value;
					p = next;
					if (i + 1 < fields && (p >= end || *p++ != ','))
						return INT_MIN;
				}
				/* Historically only a leading offset contributes to measured width. */
				if (command == 'f' && (render || first))
					x += values[0];
				else if (command == 'r' && render)
					drw_rect(drw, x + values[0], values[1], values[2], values[3], 1, 0);
			}
			else if (command != 'd')
				return INT_MIN;
			else if (render)
			{
				drw->scheme[ColFg] = scheme[SchemeNorm][ColFg];
				drw->scheme[ColBg] = scheme[SchemeNorm][ColBg];
			}
			first = 0;
		}
		text = end + 1;
	}
	return x;
}

int drawstatusbar(Monitor * m, int bh, char *stext)
{
	int w, x;
	char *text = ecalloc(strlen(stext) + 1, 1);

	strcpy(text, stext);
	w = statusparse(text, 0, bh, 0);
	if (w == INT_MIN)
	{
		w = TEXTW(stext) - lrpad;
		x = BARWIDTH(m) - w - 2 * sp;
		drw_setscheme(drw, scheme[SchemeNorm]);
		drw_rect(drw, x, 0, w + 2 * sp, bh, 1, 1);
		drw_text(drw, x + 1, 0, w, bh, 0, stext, 0);
	}
	else
	{
		w += 2 * sp;
		x = BARWIDTH(m) - w;
		drw_setscheme(drw, scheme[LENGTH(colors)]);
		drw->scheme[ColFg] = scheme[SchemeNorm][ColFg];
		drw->scheme[ColBg] = scheme[SchemeNorm][ColBg];
		drw_rect(drw, x, 0, w, bh, 1, 1);
		statusparse(text, x + 1, bh, 1);
	}
	drw_setscheme(drw, scheme[SchemeNorm]);
	free(text);
	return x;
}

Monitor *dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0)
	{
		if (!(m = selmon->next))
			m = mons;
	}
	else if (selmon == mons)
		for (m = mons; m->next; m = m->next);
	else
		for (m = mons; m->next != selmon; m = m->next);
	return m;
}

void drawbar(Monitor * m)
{
	/* Draw once after the current event has finished. */
	m->bardirty = 1;
}

void flushbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		if (m->bardirty)
		{
			m->bardirty = 0;
			if (m->showbar)
				renderbar(m);
		}
}

void renderbar(Monitor * m)
{
	int x, w;
	int boxs = drw->fonts->h / 9;
	int boxw = drw->fonts->h / 6 + 2;
	unsigned int i, occ = 0, urg = 0;
	Client *c;


	/* draw status first so it can be overdrawn by tags later */
	m->statusx = m == selmon ? drawstatusbar(m, bh, stext) : BARWIDTH(m);

	for (c = m->clients; c; c = c->next)
	{
		occ |= c->tags == 255 ? 0 : c->tags;
		if (c->isurgent)
			urg |= c->tags;
	}
	x = 0;
	for (i = 0; i < LENGTH(tags); i++)
	{
		/* do not draw vacant tags */
		if (!(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
			continue;

		w = TEXTW(tags[i]);
		drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
		drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
		x += w;
	}
	w = TEXTW(m->ltsymbol);
	drw_setscheme(drw, scheme[SchemeNorm]);
	x = m->ltend = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);

	if ((w = m->statusx - x) > bh)
	{
		if (m->sel)
		{
			drw_setscheme(drw, scheme[m == selmon ? SchemeSel : SchemeNorm]);
			drw_text(drw, x, 0, w, bh, lrpad / 2, m->sel->name, 0);

			if (m->sel->isfloating)
				drw_rect(drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
		}
		else
		{
			drw_setscheme(drw, scheme[SchemeNorm]);
			drw_rect(drw, x, 0, w, bh, 1, 1);
		}
	}
	drw_map(drw, m->barwin, 0, 0, BARWIDTH(m), bh);
}

void drawbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbar(m);
}

void expose(XEvent * e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (m = wintomon(ev->window)))
		drawbar(m);
}

void focus(Client * c)
{
	Client *i;
	int fullscreen;

	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext)
			;
	if (selmon->sel && selmon->sel != c)
		unfocus(selmon->sel, 0);
	if (c)
	{
		if (c->mon != selmon)
			selmon = c->mon;
		if (c->isurgent)
			seturgent(c, 0);
		detachstack(c);
		attachstack(c);
		grabbuttons(c, 1);
		XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
	}
	else
	{
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	selmon->sel = c;
	fullscreen = selmon->pertag->fullscreens[selmon->pertag->curtag];
	if (c && fullscreen)
	{
		c->isworkspacefullscreen = 1;
		setfullscreen(c, 1);
	}
	for (i = selmon->clients; i; i = i->next)
		if (i->isworkspacefullscreen && (!fullscreen || !ISVISIBLE(i)))
		{
			i->isworkspacefullscreen = 0;
			setfullscreen(i, 0);
		}
	if (c)
		setfocus(c);
	drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void focusin(XEvent * e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

void focusmon(const Arg * arg)
{
	Monitor *m;

	if (!mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, 0);
	selmon = m;
	focus(NULL);
}

void focusstack(const Arg * arg)
{
	Client *c;

	if (!selmon->sel || selmon->sel->isfullscreen)
		return;
	if ((c = nextclient(arg->i, 0)))
	{
		focus(c);
		restack(selmon);
	}
}

/* Navigate the client list with wraparound, optionally skipping floating clients. */
Client *nextclient(int direction, int tiled)
{
	Client *c = NULL, *i;

	if (!selmon->sel)
		return NULL;
	if (direction > 0)
	{
		for (c = selmon->sel->next; c && (!ISVISIBLE(c) || (tiled && c->isfloating)); c = c->next)
			;
		if (!c)
			for (c = selmon->clients; c && (!ISVISIBLE(c) || (tiled && c->isfloating)); c = c->next)
				;
	}
	else
	{
		for (i = selmon->clients; i != selmon->sel; i = i->next)
			if (ISVISIBLE(i) && (!tiled || !i->isfloating))
				c = i;
		if (!c)
			for (; i; i = i->next)
				if (ISVISIBLE(i) && (!tiled || !i->isfloating))
					c = i;
	}
	return c;
}

/* The caller must XFree the returned atom list. */
Atom *getatomprop(Client * c, Atom prop, unsigned long *count)
{
	int format;
	unsigned long n, extra;
	unsigned char *p = NULL;
	Atom type;

	*count = 0;
	if (XGetWindowProperty(dpy, c->win, prop, 0L, 32L, False, XA_ATOM,
						   &type, &format, &n, &extra, &p) != Success)
		return NULL;
	if (type == XA_ATOM && format == 32 && extra)
	{
		XFree(p);
		p = NULL;
		if (XGetWindowProperty(dpy, c->win, prop, 0L, 32L + (extra + 3) / 4,
							   False, XA_ATOM, &type, &format, &n, &extra, &p) != Success)
			return NULL;
	}
	if (type != XA_ATOM || format != 32 || !n || extra || !p)
	{
		XFree(p);
		return NULL;
	}
	*count = n;
	return (Atom *)p;
}

int hasatomprop(Client * c, Atom prop, Atom atom)
{
	unsigned long i, n;
	Atom *atoms = getatomprop(c, prop, &n);
	int found = 0;

	for (i = 0; i < n; i++)
		if (atoms[i] == atom)
		{
			found = 1;
			break;
		}
	XFree(atoms);
	return found;
}

int getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
						   &real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

int gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else
	{
		if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list)
		{
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void grabbuttons(Client * c, int focused)
{
	unsigned int i, j;
	unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask | LockMask };

	XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
	if (!focused)
		XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
					BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
	for (i = 0; i < LENGTH(buttons); i++)
		if (buttons[i].click == ClkClientWin)
			for (j = 0; j < LENGTH(modifiers); j++)
				XGrabButton(dpy, buttons[i].button,
							buttons[i].mask | modifiers[j],
							c->win, False, BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
}

void grabkeys(void)
{
	unsigned int i, j;
	unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask | LockMask };
	KeyCode code;

	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	for (i = 0; i < LENGTH(keys); i++)
		if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
			for (j = 0; j < LENGTH(modifiers); j++)
				XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
						 True, GrabModeAsync, GrabModeAsync);
}

#ifdef XINERAMA
static int isuniquegeom(XineramaScreenInfo * unique, size_t n, XineramaScreenInfo * info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
			&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void keypress(XEvent * e)
{
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode) ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
			&& keys[i].func)
			keys[i].func(&(keys[i].arg));
}

void killclient(const Arg * arg)
{
	if (!selmon->sel)
		return;
	if (!sendevent(selmon->sel, wmatom[WMDelete]))
	{
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, selmon->sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

void manage(Window w, XWindowAttributes * wa)
{
	Client *c, *t = NULL;
	Window trans = None;
	XWindowChanges wc;

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	/* geometry */
	c->x = wa->x;
	c->y = wa->y;
	c->w = wa->width;
	c->h = wa->height;
	c->bw = borderpx;
	c->oldbw = wa->border_width;

	updatetitle(c);
	updateclass(c);
	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans)))
	{
		c->mon = t->mon;
		c->tags = t->tags;
	}
	else
	{
		c->mon = selmon;
		applyrules(c);
	}

	if (c->x + WIDTH(c) > c->mon->mx + c->mon->mw)
		c->x = c->mon->mx + c->mon->mw - WIDTH(c);
	if (c->y + HEIGHT(c) > c->mon->my + c->mon->mh)
		c->y = c->mon->my + c->mon->mh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->mx);
	/* only fix client y-offset, if the client center might cover the bar */
	c->y =
		MAX(c->y,
			((c->mon->by == c->mon->my) && (c->x + (c->w / 2) >= c->mon->wx)
			 && (c->x + (c->w / 2) < c->mon->wx + c->mon->ww)) ? bh : c->mon->my);

	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
	configure(c);				/* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);
	c->x = c->mon->mx + (c->mon->mw - WIDTH(c)) / 2;
	c->y = c->mon->my + (c->mon->mh - HEIGHT(c)) / 2;
	XSelectInput(dpy, w, FocusChangeMask | PropertyChangeMask | StructureNotifyMask);
	grabbuttons(c, 0);
	if (!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed;
	if (c->isfloating)
		XRaiseWindow(dpy, c->win);
	attach(c);
	attachstack(c);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
					(unsigned char *)&(c->win), 1);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h);	/* some windows require
																		   this */
	setclientstate(c, NormalState);
	if (!switcher.win && c->mon == selmon)
		unfocus(selmon->sel, 0);
	if (!switcher.win)
		c->mon->sel = c;
	arrange(c->mon);
	XMapWindow(dpy, c->win);
	if (!switcher.win)
		focus(NULL);
	else if (c->mon == switcher.mon)
	{
		XRaiseWindow(dpy, switcher.win);
		switcherrefresh();
	}
}

void mappingnotify(XEvent * e)
{
	XMappingEvent *ev = &e->xmapping;
	Monitor *m;
	Client *c;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard || ev->request == MappingModifier)
	{
		updatenumlockmask();
		grabkeys();
		for (m = mons; m; m = m->next)
			for (c = m->clients; c; c = c->next)
				grabbuttons(c, c == selmon->sel);
	}
}

void maprequest(XEvent * e)
{
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	if (!XGetWindowAttributes(dpy, ev->window, &wa))
		return;
	if (wa.override_redirect)
		return;
	if (!wintoclient(ev->window))
		manage(ev->window, &wa);
}

void coalescemotion(XEvent * e)
{
	XEvent next;

	/* Skip stale positions without crossing a release or any other queued event. */
	while (XEventsQueued(dpy, QueuedAlready))
	{
		XPeekEvent(dpy, &next);
		if (next.type != MotionNotify || next.xmotion.window != e->xmotion.window)
			break;
		XNextEvent(dpy, e);
	}
}

void dragmouse(const Arg * arg)
{
	int x, y, ocx, ocy, ocw, och, nx, ny, nw, nh, px, py, resizing = arg->i;
	Client *c;
	Monitor *m;
	XEvent ev;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen)
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	ocw = c->w;
	och = c->h;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
					 None, cursor[resizing ? CurResize : CurMove], CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
	{
		XUngrabPointer(dpy, CurrentTime);
		return;
	}
	do
	{
		flushbars();
		XFlush(dpy);
		XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
		if (ev.type == MotionNotify)
			coalescemotion(&ev);
		switch (ev.type)
		{
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type] (&ev);
			break;
		case MotionNotify:
		case ButtonRelease:
			px = ev.type == MotionNotify ? ev.xmotion.x_root : ev.xbutton.x_root;
			py = ev.type == MotionNotify ? ev.xmotion.y_root : ev.xbutton.y_root;
			if (resizing)
			{
				nw = MAX(ocw + px - x, 1);
				nh = MAX(och + py - y, 1);
				if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
					&& c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh
					&& !c->isfloating && selmon->lt[selmon->sellt]->arrange
					&& (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
					togglefloating(NULL);
				nx = c->x;
				ny = c->y;
			}
			else
			{
				nx = ocx + (px - x);
				ny = ocy + (py - y);
				if (abs(selmon->wx - nx) < snap)
					nx = selmon->wx;
				else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
					nx = selmon->wx + selmon->ww - WIDTH(c);
				if (abs(selmon->wy - ny) < snap)
					ny = selmon->wy;
				else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
					ny = selmon->wy + selmon->wh - HEIGHT(c);
				if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
					&& (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
					togglefloating(NULL);
				nw = c->w;
				nh = c->h;
			}
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, nx, ny, nw, nh, 1);
			break;
		}
	}
	while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon)
	{
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

Client *nexttiled(Client * c)
{
	for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next)
		;
	return c;
}

void pop(Client * c)
{
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

void propertynotify(XEvent * e)
{
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if ((ev->window == root) && (ev->atom == XA_WM_NAME))
		updatestatus();
	else if (ev->state == PropertyDelete && ev->atom != XA_WM_CLASS
			 && ev->atom != XA_WM_NAME && ev->atom != netatom[NetWMName])
		return;					/* ignore */
	else if ((c = wintoclient(ev->window)))
	{
		switch (ev->atom)
		{
		default:
			break;
		case XA_WM_CLASS:
			updateclass(c);
			if (switcher.win && c->mon == switcher.mon)
				switcherrefresh();
			break;
		case XA_WM_TRANSIENT_FOR:
			if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
				(c->isfloating = (wintoclient(trans)) != NULL))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			updatesizehints(c);
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			drawbars();
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName])
		{
			updatetitle(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
			if (switcher.win && c->mon == switcher.mon)
				switcherrefresh();
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

void quit(const Arg * arg)
{
	running = 0;
}

Monitor *recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area)
		{
			area = a;
			r = m;
		}
	return r;
}

void resize(Client * c, int x, int y, int w, int h, int interact)
{
	if (applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}

void resizeclient(Client * c, int x, int y, int w, int h)
{
	XWindowChanges wc;

	c->x = wc.x = x;
	c->y = wc.y = y;
	c->w = wc.width = w;
	c->h = wc.height = h;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
	configure(c);
}

void restack(Monitor * m)
{
	Client *c;
	XWindowChanges wc;

	drawbar(m);
	if (!m->sel)
		return;
	if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
		XRaiseWindow(dpy, m->sel->win);
	if (m->lt[m->sellt]->arrange)
	{
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for (c = m->stack; c; c = c->snext)
			if (!c->isfloating && ISVISIBLE(c))
			{
				XConfigureWindow(dpy, c->win, CWSibling | CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}
}

void run(void)
{
	XEvent ev;
	/* main event loop */
	while (running)
	{
		flushbars();
		XFlush(dpy);
		switcherfinish();
		XNextEvent(dpy, &ev);
		if (switcherevent(&ev))
			continue;
		if (ev.type < LASTEvent && handler[ev.type])
			handler[ev.type] (&ev);	/* call handler */
	}
}

void runautostart(void)
{
	const char *scripts[] = {autostartblocksh, autostartsh};
	const char *home = getenv("HOME"), *xdgdatahome = getenv("XDG_DATA_HOME");
	char *path;
	size_t size, prefix, i;
	int status;
	struct stat sb;

	if (!home)
		return;
	/* Reserve room for either directory, the longest script name, and " &". */
	size = MAX(strlen(home) + strlen(localshare) + strlen(dwmdir) + 3,
			   (xdgdatahome ? strlen(xdgdatahome) : 0) + strlen(dwmdir) + 2);
	size += MAX(strlen(autostartblocksh), strlen(autostartsh)) + 3;
	path = ecalloc(size, 1);
	if (xdgdatahome && *xdgdatahome)
		snprintf(path, size, "%s/%s", xdgdatahome, dwmdir);
	else
		snprintf(path, size, "%s/%s/%s", home, localshare, dwmdir);
	if (stat(path, &sb) != 0 || !S_ISDIR(sb.st_mode))
		snprintf(path, size, "%s/.%s", home, dwmdir);
	prefix = strlen(path);
	for (i = 0; i < LENGTH(scripts); i++)
	{
		snprintf(path + prefix, size - prefix, "/%s", scripts[i]);
		if (access(path, X_OK) != 0)
			continue;
		if (i)
			strcat(path, " &");
		status = system(path);
		(void)status;
	}
	free(path);
}


void scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num))
	{
		for (i = 0; i < num; i++)
		{
			if (!XGetWindowAttributes(dpy, wins[i], &wa) || wa.override_redirect
				|| XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++)
		{						/* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
				&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

void sendmon(Client * c, Monitor * m)
{
	int dx = m->mx - c->mon->mx;
	int dy = m->my - c->mon->my;

	if (c->mon == m)
		return;
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	switcherinvalidate(c->win);
	c->mon = m;
	c->tags = m->tagset[m->seltags];	/* assign tags of target monitor */
	if (c->isfullscreen)
	{
		c->fsw = MAX(1, MIN(c->fsw, m->ww - 2 * borderpx));
		c->fsh = MAX(1, MIN(c->fsh, m->wh - 2 * borderpx));
		c->fsx = MAX(m->wx, MIN(c->fsx + dx, m->wx + m->ww - c->fsw - 2 * borderpx));
		c->fsy = MAX(m->wy, MIN(c->fsy + dy, m->wy + m->wh - c->fsh - 2 * borderpx));
		resizeclient(c, m->mx, m->my, m->mw, m->mh);
	}
	attach(c);
	attachstack(c);
	focus(NULL);
	arrange(NULL);
}

void setclientstate(Client * c, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
					PropModeReplace, (unsigned char *)data, 2);
}

int sendevent(Client * c, Atom proto)
{
	int n;
	Atom *protocols;
	int exists = 0;
	XEvent ev;

	if (XGetWMProtocols(dpy, c->win, &protocols, &n))
	{
		while (!exists && n--)
			exists = protocols[n] == proto;
		XFree(protocols);
	}
	if (exists)
	{
		ev.type = ClientMessage;
		ev.xclient.window = c->win;
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, c->win, False, NoEventMask, &ev);
	}
	return exists;
}

void setfocus(Client * c)
{
	if (!c->neverfocus)
	{
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow],
						XA_WINDOW, 32, PropModeReplace, (unsigned char *)&(c->win), 1);
	}
	sendevent(c, wmatom[WMTakeFocus]);
}

void setfullscreen(Client * c, int fullscreen)
{
	unsigned long i, n, count = 0;
	Atom *states, *updated;

	fullscreen = !!fullscreen;
	if (fullscreen == c->isfullscreen)
		return;
	states = getatomprop(c, netatom[NetWMState], &n);
	updated = ecalloc(n + 1, sizeof(Atom));
	for (i = 0; i < n; i++)
		if (states[i] != netatom[NetWMFullscreen])
			updated[count++] = states[i];
	if (fullscreen)
		updated[count++] = netatom[NetWMFullscreen];
	XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
					PropModeReplace, (unsigned char *)updated, count);
	XFree(states);
	free(updated);

	if (fullscreen)
	{
		c->fsx = c->x;
		c->fsy = c->y;
		c->fsw = c->w;
		c->fsh = c->h;
		c->isfullscreen = 1;
		c->oldstate = c->isfloating;
		c->bw = 0;
		c->isfloating = 1;
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->win);
		if (switcher.win)
			XRaiseWindow(dpy, switcher.win);
	}
	else
	{
		c->isfullscreen = 0;
		c->isfloating = c->oldstate;
		c->bw = borderpx;
		resizeclient(c, c->fsx, c->fsy, c->fsw, c->fsh);
		arrange(c->mon);
	}
}

void setlayout(const Arg * arg)
{
	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag] ^= 1;
	if (arg && arg->v)
		selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt] =
			(Layout *) arg->v;
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void setmfact(const Arg * arg)
{
	float f;

	if (!arg || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
	if (f < 0.05 || f > 0.95)
		return;
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = f;
	arrange(selmon);
}

void setup(void)
{
	int i;
	XSetWindowAttributes wa;
	Atom utf8string;

	/* clean up any zombies immediately */
	sigchld(0);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	drw = drw_create(dpy, screen, root, sw, 1);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h;
	bh = drw->fonts->h + 6;
	drw_resize(drw, sw, bh);
	sp = sidepad;
	vp = (topbar == 1) ? vertpad : -vertpad;
	updategeom();

	/* init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	/* init cursors */
	cursor[CurNormal] = XCreateFontCursor(dpy, XC_left_ptr);
	cursor[CurResize] = XCreateFontCursor(dpy, XC_sizing);
	cursor[CurMove] = XCreateFontCursor(dpy, XC_fleur);
	/* init appearance */
	scheme = ecalloc(LENGTH(colors) + 1, sizeof(Clr *));
	scheme[LENGTH(colors)] = drw_scm_create(drw, colors[0], 3);
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], 3);
	/* init bars */
	updatebars();
	updatestatus();

	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
					PropModeReplace, (unsigned char *)&wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
					PropModeReplace, (unsigned char *)"dwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
					PropModeReplace, (unsigned char *)&wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
					PropModeReplace, (unsigned char *)netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select events */
	wa.cursor = cursor[CurNormal];
	wa.event_mask =
		SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask |
		StructureNotifyMask | PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask | CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	updatenumlockmask();
	grabkeys();
	focus(NULL);
}

void seturgent(Client * c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

void showhide(Client * c)
{
	if (!c)
		return;
	if (ISVISIBLE(c))
	{
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
			resize(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->snext);
	}
	else
	{
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void sigchld(int unused)
{
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		die("can't install SIGCHLD handler:");
	while (0 < waitpid(-1, NULL, WNOHANG))
		;
}

void spawn(const Arg * arg)
{
	if (fork() == 0)
	{
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[0]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
}

void tag(const Arg * arg)
{
	if (selmon->sel && arg->ui & TAGMASK)
	{
		selmon->sel->tags = arg->ui & TAGMASK;
		focus(NULL);
		arrange(selmon);
		if (viewontag && ((arg->ui & TAGMASK) != TAGMASK))
			view(arg);
	}
}

void tagmon(const Arg * arg)
{
	if (!selmon->sel || !mons->next)
		return;
	sendmon(selmon->sel, dirtomon(arg->i));
}

void tile(Monitor * m)
{
	unsigned int i, n, h, mw, my, ty;
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
		;
	if (n == 0)
		return;

	if (n > m->nmaster)
		mw = m->nmaster ? m->ww * m->mfact : 0;
	else
		mw = m->ww - m->gappx;
	for (i = 0, my = ty = m->gappx, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < m->nmaster)
		{
			h = (m->wh - my) / (MIN(n, m->nmaster) - i) - m->gappx;
			resize(c, m->wx + m->gappx, m->wy + my, mw - (2 * c->bw) - m->gappx, h - (2 * c->bw),
				   0);
			if (my + HEIGHT(c) < m->wh)
				my += HEIGHT(c) + m->gappx;
		}
		else
		{
			h = (m->wh - ty) / (n - i) - m->gappx;
			resize(c, m->wx + mw + m->gappx, m->wy + ty, m->ww - mw - (2 * c->bw) - 2 * m->gappx,
				   h - (2 * c->bw), 0);
			if (ty + HEIGHT(c) < m->wh)
				ty += HEIGHT(c) + m->gappx;
		}
}

void togglebar(const Arg * arg)
{
	selmon->showbar = selmon->pertag->showbars[selmon->pertag->curtag] = !selmon->showbar;
	updatebarpos(selmon);
	XMoveResizeWindow(dpy, selmon->barwin, selmon->wx + sp, selmon->by + vp, BARWIDTH(selmon),
					  bh);
	arrange(selmon);
}

void togglefloating(const Arg * arg)
{
	if (!selmon->sel)
		return;
	if (selmon->sel->isfullscreen)	/* no support for fullscreen windows */
		return;
	selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
	if (selmon->sel->isfloating)
		resize(selmon->sel, selmon->sel->x, selmon->sel->y, selmon->sel->w, selmon->sel->h, 0);
	arrange(selmon);
}

void togglefullscr(const Arg * arg)
{
	if (selmon->sel)
	{
		selmon->pertag->fullscreens[selmon->pertag->curtag] ^= 1;
		focus(selmon->sel);
	}
}

void toggletag(const Arg * arg)
{
	unsigned int newtags;

	if (!selmon->sel)
		return;
	newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
	if (newtags)
	{
		selmon->sel->tags = newtags;
		focus(NULL);
		arrange(selmon);
	}
}

void toggleview(const Arg * arg)
{
	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);
	int i;

	if (newtagset)
	{
		selmon->tagset[selmon->seltags] = newtagset;

		if (newtagset == TAGMASK)
		{
			selmon->pertag->prevtag = selmon->pertag->curtag;
			selmon->pertag->curtag = 0;
		}

		/* test if the user did not select the same tag */
		else if (!selmon->pertag->curtag ||
				 !(newtagset & 1U << (selmon->pertag->curtag - 1)))
		{
			selmon->pertag->prevtag = selmon->pertag->curtag;
			for (i = 0; !(newtagset & 1 << i); i++);
			selmon->pertag->curtag = i + 1;
		}

		applypertag();
		focus(NULL);
		arrange(selmon);
	}
}

void unfocus(Client * c, int setfocus)
{
	if (!c)
		return;
	grabbuttons(c, 0);
	XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
	if (setfocus)
	{
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void unmanage(Client * c, int destroyed)
{
	Monitor *m = c->mon;
	XWindowChanges wc;

	detach(c);
	detachstack(c);
	switcherinvalidate(c->win);
	if (!destroyed)
	{
		wc.border_width = c->oldbw;
		XGrabServer(dpy);		/* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc);	/* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	if (m->sel == c)
		m->sel = NULL;
	free(c);
	if (!switcher.win || !selmon->sel)
		focus(NULL);
	updateclientlist();
	arrange(m);
	if (switcher.win && m == switcher.mon)
		switcherrefresh();
}

void unmapnotify(XEvent * e)
{
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = wintoclient(ev->window)))
	{
		if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, 0);
	}
}

void updatebars(void)
{
	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.border_pixel = scheme[SchemeNorm][ColBorder].pixel,
		.event_mask = ButtonPressMask | ExposureMask
	};
	XClassHint ch = { "dwm", "dwm" };
	for (m = mons; m; m = m->next)
	{
		if (m->barwin)
			continue;
		m->barwin =
			XCreateWindow(dpy, root, m->wx + sp, m->by + vp, BARWIDTH(m), bh, barborderpx,
						  DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen),
						  CWOverrideRedirect | CWBackPixmap | CWBorderPixel | CWEventMask, &wa);
		XDefineCursor(dpy, m->barwin, cursor[CurNormal]);
		XMapRaised(dpy, m->barwin);
		XSetClassHint(dpy, m->barwin, &ch);
	}
}

void updatebarpos(Monitor * m)
{
	m->wy = m->my;
	m->wh = m->mh;
	if (m->showbar)
	{
		m->wh = m->wh - vertpad - BARHEIGHT;
		m->by = m->topbar ? m->wy : m->wy + m->wh + vertpad;
		m->wy = m->topbar ? m->wy + BARHEIGHT + vp : m->wy;
	}
	else
		m->by = -BARHEIGHT - vp;
}

void updateclientlist()
{
	Client *c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty(dpy, root, netatom[NetClientList],
							XA_WINDOW, 32, PropModeAppend, (unsigned char *)&(c->win), 1);
}

int updategeom(void)
{
	int dirty = 0;

#ifdef XINERAMA
	if (XineramaIsActive(dpy))
	{
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);

		for (n = 0, m = mons; m; m = m->next, n++)
			;
		/* only consider unique geometries as separate screens */
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(info, j, &info[i]))
				info[j++] = info[i];
		nn = j;
		for (i = n; i < nn; i++)
		{
			for (m = mons; m && m->next; m = m->next)
				;
			if (m)
				m->next = createmon();
			else
				mons = createmon();
		}
		for (i = nn; i < n; i++)
		{
			dirty = 1;
			for (m = mons; m->next; m = m->next)
				;
			while ((c = m->clients))
			{
				m->clients = c->next;
				detachstack(c);
				switcherinvalidate(c->win);
				c->mon = mons;
				attach(c);
				attachstack(c);
			}
			if (m == selmon)
				selmon = mons;
			cleanupmon(m);
		}
		/* Refresh surviving geometries after either adding or removing monitors. */
		for (i = 0, m = mons; m; m = m->next, i++)
			if (i >= n || info[i].x_org != m->mx || info[i].y_org != m->my
				|| info[i].width != m->mw || info[i].height != m->mh)
			{
				dirty = 1;
				m->num = i;
				m->mx = m->wx = info[i].x_org;
				m->my = m->wy = info[i].y_org;
				m->mw = m->ww = info[i].width;
				m->mh = m->wh = info[i].height;
				updatebarpos(m);
			}
		XFree(info);
	}
	else
#endif /* XINERAMA */
	{							/* default monitor setup */
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh)
		{
			dirty = 1;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
		}
	}
	if (dirty)
	{
		selmon = mons;
		selmon = wintomon(root);
	}
	return dirty;
}

void updatenumlockmask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j] ==
				XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void updatesizehints(Client * c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize)
	{
		c->basew = size.base_width;
		c->baseh = size.base_height;
	}
	else if (size.flags & PMinSize)
	{
		c->basew = size.min_width;
		c->baseh = size.min_height;
	}
	else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc)
	{
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	}
	else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize)
	{
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	}
	else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize)
	{
		c->minw = size.min_width;
		c->minh = size.min_height;
	}
	else if (size.flags & PBaseSize)
	{
		c->minw = size.base_width;
		c->minh = size.base_height;
	}
	else
		c->minw = c->minh = 0;
	if (size.flags & PAspect)
	{
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	}
	else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
}

void updatestatus(void)
{
	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
		strcpy(stext, "dwm-" VERSION);
	drawbar(selmon);
}

void updateclass(Client *c)
{
	XClassHint hint = {NULL, NULL};

	XGetClassHint(dpy, c->win, &hint);
	snprintf(c->app, sizeof c->app, "%s",
			 hint.res_class ? hint.res_class : (hint.res_name ? hint.res_name : "Window"));
	if (hint.res_name)
		XFree(hint.res_name);
	if (hint.res_class)
		XFree(hint.res_class);
}

void updatetitle(Client * c)
{
	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0')		/* hack to mark broken clients */
		strcpy(c->name, broken);
}

void updatewindowtype(Client * c)
{
	if (hasatomprop(c, netatom[NetWMState], netatom[NetWMFullscreen]))
		setfullscreen(c, 1);
	if (hasatomprop(c, netatom[NetWMWindowType], netatom[NetWMWindowTypeDialog]))
		c->isfloating = 1;
}

void updatewmhints(Client * c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->win)))
	{
		if (c == selmon->sel && wmh->flags & XUrgencyHint)
		{
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		}
		else
			c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = 0;
		XFree(wmh);
	}
}

void view(const Arg * arg)
{
	viewclient(arg, NULL);
}

void viewclient(const Arg *arg, Client *c)
{
	int i;
	unsigned int tmptag;

	if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
	{
		if (c)
		{
			focus(c);
			restack(selmon);
		}
		return;
	}
	selmon->seltags ^= 1;		/* toggle sel tagset */
	if (arg->ui & TAGMASK)
	{
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;

		selmon->pertag->prevtag = selmon->pertag->curtag;

		if ((arg->ui & TAGMASK) == TAGMASK)
			selmon->pertag->curtag = 0;
		else
		{
			for (i = 0; !(arg->ui & 1 << i); i++);
			selmon->pertag->curtag = i + 1;
		}
	}
	else
	{
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}

	applypertag();
	focus(c);
	arrange(selmon);
}

void applypertag(void)
{
	Pertag *p = selmon->pertag;
	unsigned int tag = p->curtag;

	selmon->mfact = p->mfacts[tag];
	selmon->sellt = p->sellts[tag];
	selmon->lt[0] = p->ltidxs[tag][0];
	selmon->lt[1] = p->ltidxs[tag][1];
	if (selmon->showbar != p->showbars[tag])
		togglebar(NULL);
}

Client *wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;
	return NULL;
}

Monitor *wintomon(Window w)
{
	int x, y;
	Client *c;
	Monitor *m;

	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for (m = mons; m; m = m->next)
		if (w == m->barwin)
			return m;
	if ((c = wintoclient(w)))
		return c->mon;
	return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int xerror(Display * dpy, XErrorEvent * ee)
{
	if (ee->error_code == BadWindow
		|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
		|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
		|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
		|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
		|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
		|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
		|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
		|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
			ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee);	/* may call exit */
}

int xerrordummy(Display * dpy, XErrorEvent * ee)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int xerrorstart(Display * dpy, XErrorEvent * ee)
{
	die("dwm: another window manager is already running");
	return -1;
}

void zoom(const Arg * arg)
{
	Client *c = selmon->sel;

	if (!selmon->lt[selmon->sellt]->arrange || (selmon->sel && selmon->sel->isfloating))
		return;
	if (c == nexttiled(selmon->clients))
		if (!c || !(c = nexttiled(c->next)))
			return;
	pop(c);
}

int main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp("-v", argv[1]))
		die("dwm-" VERSION);
	else if (argc != 1)
		die("usage: dwm [-v]");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("dwm: cannot open display");
	checkotherwm();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
	scan();
	runautostart();
	run();
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
