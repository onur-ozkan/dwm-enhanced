/* Exercise the internal event handlers against an isolated X server. */
#include <assert.h>

#define main dwm_main
#include "../dwm.c"
#undef main
#include "../drw.c"

static unsigned int matches, draws;

FcPattern *__real_XftFontMatch(Display *, int, const FcPattern *, XftResult *);
void __real_XftDrawStringUtf8(XftDraw *, const XftColor *, XftFont *, int, int,
							const FcChar8 *, int);

FcPattern *__wrap_XftFontMatch(Display *display, int scr, const FcPattern *pattern,
							 XftResult *result)
{
	matches++;
	return __real_XftFontMatch(display, scr, pattern, result);
}

void __wrap_XftDrawStringUtf8(XftDraw *draw, const XftColor *color, XftFont *font,
							int x, int y, const FcChar8 *text, int len)
{
	int chars, width;

	assert(FcUtf8Len(text, len, &chars, &width));
	draws++;
	__real_XftDrawStringUtf8(draw, color, font, x, y, text, len);
}

static void test_tags(void)
{
	Arg all = {.ui = TAGMASK}, one = {.ui = 1}, forward = {.i = 1}, back = {.i = -1};
	unsigned int i;

	assert(!selmon->sel);
	movestack(&forward);
	movestack(&back);
	for (i = 0; i < LENGTH(tags); i++)
	{
		view(&all);
		assert(selmon->pertag->curtag == 0);
		one.ui = 1U << i;
		toggleview(&one);
		assert(selmon->tagset[selmon->seltags] == (TAGMASK ^ one.ui));
		assert(selmon->pertag->curtag > 0);
		assert(selmon->tagset[selmon->seltags] & (1U << (selmon->pertag->curtag - 1)));
		toggleview(&one);
		assert(selmon->pertag->curtag == 0);
	}
	one.ui = 1;
	view(&one);
}

static void test_status(void)
{
	const char *valid[] = {"plain", "^c#123aBc^color^d^", "^b#010203^bg^d^",
		"^f10^offset", "^f-10^offset", "^r0,2,10,4^", "^c#abcdefb#123456r0,0,1,1f2d^"};
	const char *invalid[] = {"^", "hello ^x", "^^", "^c#fff^", "^c#gggggg^",
		"^b#123456", "^r1,2^", "^r1,2,3^", "^r0,0,-1,2^", "^f^", "^f-^",
		"^f999999999999999999999999999^", "^r0,0,1,2", "^f1 ^"};
	char truncated[1024];
	unsigned int i;

	for (i = 0; i < LENGTH(valid); i++)
	{
		assert(statusvalid(valid[i]));
		drawstatusbar(selmon, bh, (char *)valid[i]);
	}
	for (i = 0; i < LENGTH(invalid); i++)
	{
		assert(!statusvalid(invalid[i]));
		drawstatusbar(selmon, bh, (char *)invalid[i]);
	}
	memset(truncated, 'x', sizeof truncated);
	memcpy(truncated + sizeof truncated - 5, "^c#f", 5);
	assert(!statusvalid(truncated));
	drawstatusbar(selmon, bh, truncated);
}

static Window test_window(void)
{
	Window w = XCreateSimpleWindow(dpy, root, 100, 100, 260, 180, 0, 0, 0);

	XMapWindow(dpy, w);
	return w;
}

static void test_properties(void)
{
	Client c = {.mon = selmon, .x = 100, .y = 100, .w = 260, .h = 180, .isfloating = 1};
	Atom *readback, states[40], above = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
	unsigned long n;
	unsigned int i;
	unsigned char byte = 1;

	c.win = test_window();
	for (i = 0; i < LENGTH(states); i++)
		states[i] = above;
	states[39] = netatom[NetWMFullscreen];
	XChangeProperty(dpy, c.win, netatom[NetWMState], XA_ATOM, 32, PropModeReplace,
					(unsigned char *)states, LENGTH(states));
	assert(hasatomprop(&c, netatom[NetWMState], netatom[NetWMFullscreen]));
	readback = getatomprop(&c, netatom[NetWMState], &n);
	assert(n == LENGTH(states));
	XFree(readback);
	setfullscreen(&c, 1);
	assert(c.isfullscreen && hasatomprop(&c, netatom[NetWMState], above));
	resizeclient(&c, 0, 0, 800, 600);
	setfullscreen(&c, 0);
	assert(c.x == 100 && c.y == 100 && c.w == 260 && c.h == 180);
	assert(!hasatomprop(&c, netatom[NetWMState], netatom[NetWMFullscreen]));
	assert(hasatomprop(&c, netatom[NetWMState], above));
	for (i = 0; i < 3; i++)
	{
		XChangeProperty(dpy, c.win, netatom[NetWMState], i == 2 ? XA_STRING : XA_ATOM,
						i == 0 ? 32 : 8, PropModeReplace, &byte, i == 0 ? 0 : 1);
		readback = getatomprop(&c, netatom[NetWMState], &n);
		assert(!readback && n == 0);
	}
	XDestroyWindow(dpy, c.win);
}

static void test_monitors(void)
{
	Monitor *first = selmon, *other = createmon();
	Client *c = ecalloc(1, sizeof(Client));
	XEvent ev = {0};
	int restorew, restoreh;

	assert(!first->next);
	first->next = other;
	other->mx = other->wx = sw / 2;
	other->my = other->wy = 0;
	other->mw = other->ww = sw / 2;
	other->mh = other->wh = sh / 2;
	updatebarpos(other);
	c->win = test_window();
	c->mon = other;
	c->tags = 1;
	c->x = other->wx + 20;
	c->y = other->wy + 20;
	c->w = 260;
	c->h = 180;
	attach(c);
	attachstack(c);
	other->sel = c;
	first->lt[first->sellt] = &layouts[1];
	other->lt[other->sellt] = &layouts[0];
	ev.xconfigurerequest.window = c->win;
	ev.xconfigurerequest.value_mask = CWWidth;
	ev.xconfigurerequest.width = 300;
	configurerequest(&ev);
	assert(c->w == 260);
	first->lt[first->sellt] = &layouts[0];
	other->lt[other->sellt] = &layouts[1];
	configurerequest(&ev);
	assert(c->w == 300);
	c->isfloating = 1;
	restorew = c->w;
	restoreh = c->h;
	setfullscreen(c, 1);
	sendmon(c, first);
	assert(c->mon == first);
	assert(c->x == first->mx && c->y == first->my && c->w == first->mw && c->h == first->mh);
	sendmon(c, other);
	assert(c->x == other->mx && c->y == other->my && c->w == other->mw && c->h == other->mh);
	setfullscreen(c, 0);
	assert(c->w == restorew && c->h == restoreh);
	assert(c->x >= other->wx && c->y >= other->wy);
	assert(c->x + WIDTH(c) <= other->wx + other->ww);
	assert(c->y + HEIGHT(c) <= other->wy + other->wh);
	XDestroyWindow(dpy, c->win);
	unmanage(c, 1);
	first->next = NULL;
	free(other->pertag);
	free(other);
}

static void test_geometry(void)
{
	Window r;
	int x, y;
	unsigned int w, h, border, depth;
	XWindowAttributes wa;

	assert(XGetGeometry(dpy, drw->drawable, &r, &x, &y, &w, &h, &border, &depth));
	assert(w == (unsigned int)sw && h == (unsigned int)bh);
	XGetWindowAttributes(dpy, root, &wa);
	assert(!(wa.your_event_mask & (PointerMotionMask | EnterWindowMask | LeaveWindowMask)));
#ifndef XINERAMA
	sw -= 100;
	sh -= 100;
	assert(updategeom());
	assert(selmon->ww == sw && selmon->wh == sh - bh - vertpad);
	assert(selmon->wy == bh + vp);
	sw += 100;
	sh += 100;
	assert(updategeom());
#endif
}

static void test_redraw_and_mapping(void)
{
	XEvent ev = {0};
	unsigned int once, savedmask = numlockmask;

	flushbars();
	draws = 0;
	drawbar(selmon);
	flushbars();
	once = draws;
	assert(once > 0);
	draws = 0;
	drawbar(selmon);
	drawbar(selmon);
	flushbars();
	assert(draws == once);
	draws = 0;
	selmon->showbar = 0;
	drawbar(selmon);
	flushbars();
	assert(draws == 0);
	selmon->showbar = 1;
	drawbar(selmon);
	flushbars();
	assert(draws == once);
	numlockmask = ~0U;
	ev.xmapping.display = dpy;
	ev.xmapping.request = MappingModifier;
	mappingnotify(&ev);
	assert(numlockmask == savedmask);
}

static void test_fonts(void)
{
	const char missing[] = "\xf4\x8f\xbf\xbf";
	char text[513];
	unsigned int i, calls, width, nextwidth, limit, fullwidth;
	size_t len;

	drw_setfontset(drw, drw->fonts);
	matches = 0;
	drw_fontset_getwidth(drw, missing);
	calls = matches;
	assert(calls > 0 && drw->nomatchcount == 1);
	for (i = 0; i < 10; i++)
	{
		drw_fontset_getwidth(drw, missing);
		drw_text(drw, 0, 0, 100, bh, 0, missing, 0);
	}
	assert(matches == calls);
	drw_setfontset(drw, drw->fonts);
	drw_fontset_getwidth(drw, missing);
	assert(matches > calls);
	for (i = 0; i < 256; i++)
		memcpy(text + i * 2, "\xc3\xa9", 2);
	text[512] = '\0';
	drw_font_getexts(drw->fonts, text, 512, &fullwidth, NULL);
	for (limit = 0; limit <= fullwidth; limit += 7)
	{
		len = textfit(drw->fonts, text, 512, limit, &width);
		assert(len % 2 == 0 && width <= limit);
		if (len < 512)
		{
			drw_font_getexts(drw->fonts, text, len + 2, &nextwidth, NULL);
			assert(nextwidth > limit);
		}
		assert(drw_text(drw, 10, 0, limit, bh, 0, text, 0) == (int)(10 + limit));
	}
	for (i = 0; i < 32; i++)
		drw_text(drw, 0, 0, i, bh, 0, "Latin \xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80 end", 0);
}

static void sendkey(KeySym key, unsigned int modifiers)
{
	XEvent ev = {0};

	ev.xkey.type = KeyPress;
	ev.xkey.display = dpy;
	ev.xkey.window = ev.xkey.root = root;
	ev.xkey.keycode = XKeysymToKeycode(dpy, key);
	ev.xkey.state = modifiers;
	XSendEvent(dpy, root, False, SubstructureRedirectMask, &ev);
	XFlush(dpy);
}

/* Run the real blocking event loop; drive it through a separate connection. */
static void test_event_loop(void)
{
	int ready[2], i, status;
	pid_t pid, result = 0;
	Window bar, w;
	XWindowAttributes wa;
	XEvent ev = {0};
	Client c = {0};
	Atom above;

	assert(signal(SIGCHLD, SIG_DFL) != SIG_ERR);
	assert(pipe(ready) == 0);
	pid = fork();
	assert(pid >= 0);
	if (pid == 0)
	{
		close(ready[0]);
		alarm(20);
		XSetErrorHandler(NULL);
		mons = selmon = NULL;
		dpy = XOpenDisplay(NULL);
		assert(dpy);
		checkotherwm();
		setup();
		flushbars();
		XSync(dpy, False);
		assert(write(ready[1], &selmon->barwin, sizeof(Window)) == sizeof(Window));
		close(ready[1]);
		run();
		cleanup();
		XCloseDisplay(dpy);
		_exit(0);
	}
	close(ready[1]);
	assert(read(ready[0], &bar, sizeof bar) == sizeof bar);
	close(ready[0]);
	dpy = XOpenDisplay(NULL);
	assert(dpy);
	sendkey(XK_j, MODKEY | ShiftMask);
	sendkey(XK_0, MODKEY);
	sendkey(XK_1, MODKEY | ControlMask);
	sendkey(XK_1, MODKEY);
	w = XCreateSimpleWindow(dpy, root, 100, 100, 260, 180, 0, 0, 0);
	above = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
	XChangeProperty(dpy, w, netatom[NetWMState], XA_ATOM, 32, PropModeReplace,
					(unsigned char *)&above, 1);
	XMapWindow(dpy, w);
	XFlush(dpy);
	for (i = 0; i < 200 && getstate(w) != NormalState; i++)
		usleep(10000);
	assert(i < 200);
	XGetWindowAttributes(dpy, w, &wa);
	assert(!(wa.all_event_masks & EnterWindowMask));
	c.win = w;
	for (status = 1; status >= 0; status--)
	{
		ev.xclient.type = ClientMessage;
		ev.xclient.window = w;
		ev.xclient.message_type = netatom[NetWMState];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = status;
		ev.xclient.data.l[1] = netatom[NetWMFullscreen];
		XSendEvent(dpy, root, False, SubstructureRedirectMask, &ev);
		XFlush(dpy);
		for (i = 0; i < 200; i++)
		{
			XGetWindowAttributes(dpy, w, &wa);
			if ((status && wa.width == sw && wa.height == sh) || (!status && wa.height < sh))
				break;
			usleep(10000);
		}
		assert(i < 200 && hasatomprop(&c, netatom[NetWMState], above));
	}
	for (status = 0; status < 2; status++)
	{
		sendkey(XK_b, MODKEY);
		for (i = 0; i < 200; i++)
		{
			XGetWindowAttributes(dpy, bar, &wa);
			if ((status == 0 && wa.y < 0) || (status == 1 && wa.y >= 0))
				break;
			usleep(10000);
		}
		assert(i < 200);
	}
	XStoreName(dpy, root, "CPU ^");
	XDestroyWindow(dpy, w);
	sendkey(XK_F12, MODKEY | ShiftMask);
	for (i = 0; i < 200 && (result = waitpid(pid, &status, WNOHANG)) == 0; i++)
		usleep(10000);
	if (!result)
	{
		kill(pid, SIGKILL);
		waitpid(pid, &status, 0);
	}
	assert(result == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
	XCloseDisplay(dpy);
}

int main(void)
{
	assert(setlocale(LC_CTYPE, ""));
	dpy = XOpenDisplay(NULL);
	assert(dpy);
	checkotherwm();
	setup();
	test_tags();
	test_status();
	test_properties();
	test_monitors();
	test_geometry();
	test_redraw_and_mapping();
	test_fonts();
	XSync(dpy, False);
	cleanup();
	XCloseDisplay(dpy);
	test_event_loop();
	puts("All regression checks passed");
	return 0;
}
