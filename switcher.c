/* Included by dwm.c: popup navigation never changes dwm's active view. */
typedef struct
{
	Window win;
	char label[1024];
} SwitcherItem;

static struct
{
	Window win;
	Monitor *mon;
	Drw *drw;
	XIM im;
	XIC ic;
	SwitcherItem *items;
	size_t count, capacity, selected, offset;
	unsigned int tag, rows, width, height, rowheight;
	int all;
	char query[256];
} switcher;

static unsigned int switcherworkspaces(void)
{
	Client *c;
	unsigned int visible = switcher.mon->tagset[switcher.mon->seltags];

	for (c = switcher.mon->clients; c; c = c->next)
		visible |= c->tags;
	return visible & TAGMASK;
}

static int switchermatch(const char *text, const char *query)
{
	size_t i;

	if (!*query)
		return 1;
	for (; *text; text++)
	{
		for (i = 0; query[i] && text[i]
			 && tolower((unsigned char)text[i]) == tolower((unsigned char)query[i]); i++);
		if (!query[i])
			return 1;
	}
	return 0;
}

static void switcherclose(void)
{
	if (!switcher.win)
		return;
	XUngrabKeyboard(dpy, CurrentTime);
	XUngrabPointer(dpy, CurrentTime);
	if (switcher.ic)
		XDestroyIC(switcher.ic);
	if (switcher.im)
		XCloseIM(switcher.im);
	XDestroyWindow(dpy, switcher.win);
	switcher.drw->fonts = NULL;	/* Shared with the bar drawing context. */
	drw_free(switcher.drw);
	free(switcher.items);
	memset(&switcher, 0, sizeof switcher);
}

static void switcherdraw(void)
{
	Drw *d = switcher.drw;
	unsigned int i, column = 0, columns = 0, x, w, h = switcher.rowheight;
	unsigned int visible = switcherworkspaces();
	size_t index;
	char text[1024];

	for (i = 0; i < LENGTH(tags); i++)
		if (visible & (1U << i))
			columns++;
	drw_setscheme(d, scheme[SchemeNorm]);
	drw_rect(d, 0, 0, switcher.width, switcher.height, 1, 1);
	for (i = 0; i < LENGTH(tags); i++)
	{
		if (!(visible & (1U << i)))
			continue;
		x = column * switcher.width / columns;
		w = (++column) * switcher.width / columns - x;
		drw_setscheme(d, scheme[i == switcher.tag && !switcher.all ? SchemeBlue : SchemeNorm]);
		drw_text(d, x, 0, w, h, lrpad / 2, tags[i], i == switcher.tag && !switcher.all);
	}
	drw_setscheme(d, scheme[SchemeSel]);
	snprintf(text, sizeof text, "Filter: %s|", switcher.query);
	drw_text(d, 0, h, switcher.width, h, lrpad / 2, text, 0);
	drw_setscheme(d, scheme[SchemeNorm]);
	snprintf(text, sizeof text, "[%c] All workspaces (Ctrl+Space)", switcher.all ? 'x' : ' ');
	drw_text(d, 0, 2 * h, switcher.width, h, lrpad / 2, text, 0);
	if (switcher.selected < switcher.offset)
		switcher.offset = switcher.selected;
	if (switcher.selected >= switcher.offset + switcher.rows)
		switcher.offset = switcher.selected - switcher.rows + 1;
	for (i = 0; i < switcher.rows; i++)
	{
		index = switcher.offset + i;
		drw_setscheme(d, scheme[index == switcher.selected && index < switcher.count
								? SchemeBlue : SchemeNorm]);
		drw_text(d, 0, (3 + i) * h, switcher.width, h, lrpad / 2,
				 index < switcher.count ? switcher.items[index].label
				 : (!switcher.count && !i ? "No matching windows" : ""),
				 index == switcher.selected && index < switcher.count);
	}
	drw_setscheme(d, scheme[SchemeNorm]);
	snprintf(text, sizeof text,
			 "←→ Workspace   ↑↓ App   %zu/%zu",
			 switcher.count ? switcher.selected + 1 : 0, switcher.count);
	drw_text(d, 0, (3 + switcher.rows) * h, switcher.width, h, lrpad / 2, text, 0);
	drw_map(d, switcher.win, 0, 0, switcher.width, switcher.height);
}

static void switcherrefresh(void)
{
	Client *c;
	SwitcherItem *item;
	Window selected = switcher.selected < switcher.count
		? switcher.items[switcher.selected].win : None;
	size_t capacity = 0, previous = switcher.selected;
	int initial = !switcher.items;
	unsigned int i, visible = switcherworkspaces();
	char workspaces[256];

	if (!(visible & (1U << switcher.tag)))
		for (switcher.tag = 0; !(visible & (1U << switcher.tag)); switcher.tag++);
	for (c = switcher.mon->stack; c; c = c->snext)
		capacity++;
	if (capacity > switcher.capacity)
	{
		free(switcher.items);
		switcher.capacity = MAX(capacity, 2 * switcher.capacity);
		switcher.items = ecalloc(switcher.capacity, sizeof *switcher.items);
	}
	switcher.count = 0;
	for (c = switcher.mon->stack; c; c = c->snext)
	{
		if (!switcher.all && !(c->tags & (1U << switcher.tag)))
			continue;
		if (switchermatch(c->name, switcher.query) || switchermatch(c->app, switcher.query))
		{
			workspaces[0] = '\0';
			for (i = 0; i < LENGTH(tags); i++)
				if (c->tags & (1U << i))
				{
					if (*workspaces)
						strncat(workspaces, ",", sizeof workspaces - strlen(workspaces) - 1);
					strncat(workspaces, tags[i], sizeof workspaces - strlen(workspaces) - 1);
				}
			item = &switcher.items[switcher.count++];
			item->win = c->win;
			snprintf(item->label, sizeof item->label, "[%s] %s — %s", workspaces, c->app,
					 c->name);
		}
	}
	switcher.selected = switcher.count ? MIN(previous, switcher.count - 1) : 0;
	for (i = 0; i < switcher.count; i++)
		if (switcher.items[i].win == selected)
			switcher.selected = i;
	if (initial && switcher.count > 1 && switcher.mon->sel
		&& switcher.items[0].win == switcher.mon->sel->win)
		switcher.selected = 1;
	if (!switcher.count)
		switcher.offset = 0;
	switcherdraw();
}

static void switcheractivate(void)
{
	Client *c;
	Arg tag;
	Window win;

	if (!switcher.count)
		return;
	win = switcher.items[switcher.selected].win;
	c = wintoclient(win);
	if (!c)
	{
		switcherrefresh();
		return;
	}
	tag.ui = 1U << switcher.tag;
	if (!(c->tags & tag.ui))
		tag.ui = c->tags & (~c->tags + 1U);
	viewclient(&tag, c);
	XRaiseWindow(dpy, c->win);
	switcherclose();
}

static void switcherkey(XKeyEvent *event)
{
	KeySym key = NoSymbol;
	Status status;
	char text[64];
	int n;
	unsigned int visible, tag = switcher.tag;
	size_t len = strlen(switcher.query);

	if (switcher.ic)
	{
		n = Xutf8LookupString(switcher.ic, event, text, sizeof text, &key, &status);
		if (status == XBufferOverflow)
			return;
	}
	else
		n = XLookupString(event, text, sizeof text, &key, NULL);
	switch (key)
	{
	case XK_Escape:
		switcherclose();
		return;
	case XK_Return:
	case XK_KP_Enter:
		switcheractivate();
		return;
	case XK_Left:
	case XK_Right:
		visible = switcherworkspaces();
		do
			switcher.tag = (switcher.tag + LENGTH(tags) + (key == XK_Left ? -1 : 1)) % LENGTH(tags);
		while (!(visible & (1U << switcher.tag)));
		if (switcher.tag == tag && !switcher.all)
			return;
		switcher.all = 0;
		switcher.selected = switcher.offset = 0;
		switcherrefresh();
		return;
	case XK_Up:
	case XK_Down:
	case XK_Tab:
	case XK_ISO_Left_Tab:
		if (switcher.count > 1)
		{
			if (key == XK_Up || key == XK_ISO_Left_Tab
				|| (key == XK_Tab && (event->state & ShiftMask)))
				switcher.selected = (switcher.selected + switcher.count - 1) % switcher.count;
			else
				switcher.selected = (switcher.selected + 1) % switcher.count;
			switcherdraw();
		}
		return;
	case XK_space:
		if (event->state & ControlMask)
		{
			switcher.all = !switcher.all;
			switcher.selected = switcher.offset = 0;
			switcherrefresh();
			return;
		}
		break;
	case XK_BackSpace:
		if (!len)
			return;
		do
			len--;
		while (len && ((unsigned char)switcher.query[len] & 0xc0) == 0x80);
		switcher.query[len] = '\0';
		n = 0;
		break;
	}
	if (key != XK_BackSpace && (event->state & (ControlMask | Mod1Mask | Mod4Mask)))
		return;
	if (n > 0 && (unsigned char)text[0] >= 0x20 && (unsigned char)text[0] != 0x7f
		&& len + n < sizeof switcher.query)
	{
		memcpy(switcher.query + len, text, n);
		switcher.query[len + n] = '\0';
	}
	else if (key != XK_BackSpace)
		return;
	switcher.selected = switcher.offset = 0;
	switcherrefresh();
}

static int switcherevent(XEvent *event)
{
	if (!switcher.win)
		return 0;
	if (event->type == KeyPress)
	{
		if (!XFilterEvent(event, switcher.win))
			switcherkey(&event->xkey);
		return 1;
	}
	if (event->type == KeyRelease || event->type == ButtonPress || event->type == ButtonRelease)
		return 1;
	if (event->type == Expose && event->xexpose.window == switcher.win)
	{
		if (!event->xexpose.count)
			drw_map(switcher.drw, switcher.win, 0, 0, switcher.width, switcher.height);
		return 1;
	}
	return 0;
}

static void switcheropen(const Arg *arg)
{
	XSetWindowAttributes attributes = {.override_redirect = True,.event_mask = ExposureMask,
		.border_pixel = scheme[SchemeBlue][ColFg].pixel
	};
	unsigned int active;
	int border = 2;

	(void)arg;
	if (switcher.win)
		return;
	switcher.mon = selmon;
	active = selmon->tagset[selmon->seltags];
	for (switcher.tag = 0; !(active & (1U << switcher.tag)); switcher.tag++);
	switcher.rowheight = MIN(bh + 8, MAX(1, (selmon->wh - 2 * border) / 5));
	switcher.width = MIN(MAX(switcherwidth, 1), (unsigned int)MAX(1, selmon->ww - 2 * border));
	switcher.rows = MIN(MAX(switcherrows, 1),
						MAX(1, (selmon->wh - 2 * border) / (int)switcher.rowheight - 4));
	switcher.height = (switcher.rows + 4) * switcher.rowheight;
	switcher.drw = drw_create(dpy, screen, root, switcher.width, switcher.height);
	switcher.drw->fonts = drw->fonts;
	switcher.win = XCreateWindow(dpy, root,
								 selmon->wx + (selmon->ww - (int)switcher.width - 2 * border) / 2,
								 selmon->wy + (selmon->wh - (int)switcher.height - 2 * border) / 2,
								 switcher.width, switcher.height, border, CopyFromParent,
								 CopyFromParent, CopyFromParent,
								 CWOverrideRedirect | CWEventMask | CWBorderPixel, &attributes);
	XStoreName(dpy, switcher.win, "dwm window switcher");
	XMapRaised(dpy, switcher.win);
	if (XGrabKeyboard(dpy, switcher.win, False, GrabModeAsync, GrabModeAsync, CurrentTime) !=
		GrabSuccess)
	{
		switcherclose();
		return;
	}
	if (XGrabPointer(dpy, switcher.win, False, ButtonPressMask | ButtonReleaseMask,
					 GrabModeAsync, GrabModeAsync, None, cursor[CurNormal]->cursor,
					 CurrentTime) != GrabSuccess)
	{
		switcherclose();
		return;
	}
	switcher.im = XOpenIM(dpy, NULL, NULL, NULL);
	if (switcher.im)
		switcher.ic = XCreateIC(switcher.im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
								XNClientWindow, switcher.win, XNFocusWindow, switcher.win, NULL);
	if (switcher.ic)
		XSetICFocus(switcher.ic);
	switcherrefresh();
}
