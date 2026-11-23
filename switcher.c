/* Included by dwm.c: popup navigation never changes dwm's active view. */
#include <X11/extensions/Xrender.h>

typedef struct SwitcherIcon SwitcherIcon;
struct SwitcherIcon
{
	Window win;
	Picture picture;
	unsigned int w, h;
	SwitcherIcon *next;
};

typedef struct
{
	Window win;
	char label[1024];
} SwitcherItem;

static struct
{
	Window win, initial;
	Monitor *mon;
	Drw *drw;
	Picture picture;
	Atom iconatom;
	SwitcherIcon *icons;
	XIM im;
	XIC ic;
	SwitcherItem *items;
	size_t count, capacity, selected, offset;
	unsigned int tag, starttag, view, rows, width, height, rowheight;
	int all, released;
	unsigned char modifiers[256], down[256];
	char query[256];
} switcher;

/* Cache even missing icons; navigation and filtering never reload images. */
static SwitcherIcon *switchericon(Window win)
{
	SwitcherIcon *icon;
	Atom type;
	int format;
	unsigned long n, remaining, *data = NULL, *best = NULL;
	unsigned long i, w, h, size, score = ULONG_MAX, candidate;
	unsigned int x, y, a, pixel, target = MIN(drw->fonts->h, switcher.rowheight);
	Pixmap pixmap;
	GC gc;
	XImage *image;
	XRenderPictFormat *argb;
	XTransform transform = {{{0}}};

	for (icon = switcher.icons; icon; icon = icon->next)
		if (icon->win == win)
			return icon;
	icon = ecalloc(1, sizeof *icon);
	icon->win = win;
	icon->next = switcher.icons;
	switcher.icons = icon;
	argb = XRenderFindStandardFormat(dpy, PictStandardARGB32);
	/* Bound transfer to 4 MiB; validate dimensions before multiplying. */
	if (!switcher.picture || !argb || XGetWindowProperty(dpy, win, switcher.iconatom,
			0, 1024 * 1024, False, XA_CARDINAL, &type, &format, &n, &remaining,
			(unsigned char **)&data) != Success)
		goto done;
	if (type != XA_CARDINAL || format != 32)
		goto done;
	for (i = 0; i + 2 <= n; i += 2 + w * h)
	{
		w = data[i];
		h = data[i + 1];
		if (!w || !h || w > (n - i - 2) / h)
			break;
		size = MAX(w, h);
		if (size > 1024)
			continue;
		/* Prefer the nearest size at least as large as the row icon. */
		candidate = size >= target ? size - target : 1024 + target - size;
		if (candidate < score)
		{
			best = &data[i];
			score = candidate;
		}
	}
	if (!best)
		goto done;
	w = best[0];
	h = best[1];
	icon->w = MAX(1, w * target / MAX(w, h));
	icon->h = MAX(1, h * target / MAX(w, h));
	image = XCreateImage(dpy, DefaultVisual(dpy, screen), 32, ZPixmap, 0, NULL,
						 w, h, 32, 0);
	if (!image)
		goto done;
	image->data = ecalloc(h, image->bytes_per_line);
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++)
		{
			pixel = best[2 + y * w + x];
			a = pixel >> 24;
			/* EWMH uses straight alpha; Render requires premultiplied channels. */
			XPutPixel(image, x, y, (a << 24)
				| (((pixel >> 16 & 255) * a / 255) << 16)
				| (((pixel >> 8 & 255) * a / 255) << 8)
				| ((pixel & 255) * a / 255));
		}
	pixmap = XCreatePixmap(dpy, root, w, h, 32);
	gc = XCreateGC(dpy, pixmap, 0, NULL);
	XPutImage(dpy, pixmap, gc, image, 0, 0, 0, 0, w, h);
	XFreeGC(dpy, gc);
	XDestroyImage(image);
	icon->picture = XRenderCreatePicture(dpy, pixmap, argb, 0, NULL);
	XFreePixmap(dpy, pixmap);
	transform.matrix[0][0] = XDoubleToFixed((double)w / icon->w);
	transform.matrix[1][1] = XDoubleToFixed((double)h / icon->h);
	transform.matrix[2][2] = XDoubleToFixed(1);
	XRenderSetPictureTransform(dpy, icon->picture, &transform);
	XRenderSetPictureFilter(dpy, icon->picture, FilterBilinear, NULL, 0);
done:
	if (data)
		XFree(data);
	return icon;
}

static void switcherfreeicon(SwitcherIcon *icon)
{
	if (icon->picture)
		XRenderFreePicture(dpy, icon->picture);
	free(icon);
}

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
	SwitcherIcon *icon;

	if (!switcher.win)
		return;
	XUngrabKeyboard(dpy, CurrentTime);
	XUngrabPointer(dpy, CurrentTime);
	if (switcher.ic)
		XDestroyIC(switcher.ic);
	if (switcher.im)
		XCloseIM(switcher.im);
	XDestroyWindow(dpy, switcher.win);
	while ((icon = switcher.icons))
	{
		switcher.icons = icon->next;
		switcherfreeicon(icon);
	}
	if (switcher.picture)
		XRenderFreePicture(dpy, switcher.picture);
	switcher.drw->fonts = NULL;	/* Shared with the bar drawing context. */
	drw_free(switcher.drw);
	free(switcher.items);
	memset(&switcher, 0, sizeof switcher);
}

static void switcherdraw(void)
{
	Drw *d = switcher.drw;
	SwitcherIcon *icon;
	unsigned int i, column = 0, columns = 0, x, w, side, y, h = switcher.rowheight;
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
		drw_setscheme(d, scheme[i == switcher.tag && !switcher.all
							   ? SchemeSwitcherWorkspace : SchemeNorm]);
		if (i == switcher.tag && !switcher.all)
		{
			side = MIN(h > 4 ? h - 4 : h, w);
			drw_text(d, x, (h - side) / 2, side, side,
				(side - MIN(side, drw_fontset_getwidth(d, tags[i]))) / 2, tags[i], 1);
		}
		else
			drw_text(d, x, 0, w, h, lrpad / 2, tags[i], 0);
	}
	drw_setscheme(d, scheme[SchemeSwitcherFilter]);
	snprintf(text, sizeof text, "Filter: %s|", switcher.query);
	drw_text(d, 0, h, switcher.width, h, lrpad / 2, text, 0);
	drw_setscheme(d, scheme[SchemeNorm]);
	snprintf(text, sizeof text, "Windows | [%c] All workspaces (Ctrl+Space)",
			 switcher.all ? 'x' : ' ');
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
		drw_text(d, 0, (3 + i) * h, switcher.width, h,
				 index < switcher.count ? lrpad + MIN(d->fonts->h, h) : lrpad / 2,
				 index < switcher.count ? switcher.items[index].label
				 : (!switcher.count && !i ? "No matching windows" : ""),
				 index == switcher.selected && index < switcher.count);
		if (index < switcher.count)
		{
			icon = switchericon(switcher.items[index].win);
			if (icon->picture)
				XRenderComposite(dpy, PictOpOver, icon->picture, None, switcher.picture,
					0, 0, 0, 0, lrpad / 2 + (MIN(d->fonts->h, h) - icon->w) / 2,
					(3 + i) * h + (h - icon->h) / 2, icon->w, icon->h);
			else
			{
				side = MIN(d->fonts->h, h);
				x = lrpad / 2;
				y = (3 + i) * h + (h - side) / 2;
				drw_rect(d, x, y, side, side, 0, index == switcher.selected);
				if (side > 4)
					drw_rect(d, x + 2, y + 2, side - 4, 2, 1,
						index == switcher.selected);
			}
		}
	}
	drw_setscheme(d, scheme[SchemeNorm]);
	snprintf(text, sizeof text,
			 "Tab: app   `: workspace   Shift: reverse   %zu/%zu",
			 switcher.count ? switcher.selected + 1 : 0, switcher.count);
	drw_text(d, 0, (3 + switcher.rows) * h, switcher.width, h, lrpad / 2, text, 0);
	drw_map(d, switcher.win, 0, 0, switcher.width, switcher.height);
}

static void switcherrefresh(void)
{
	Client *c;
	SwitcherItem *item;
	SwitcherIcon **link, *icon;
	Window selected = switcher.selected < switcher.count
		? switcher.items[switcher.selected].win : None;
	size_t capacity = 0, previous = switcher.selected;
	unsigned int i, visible = switcherworkspaces();
	char workspaces[256];

	for (link = &switcher.icons; (icon = *link); )
		if (!(c = wintoclient(icon->win)) || c->mon != switcher.mon)
		{
			*link = icon->next;
			switcherfreeicon(icon);
		}
		else
			link = &icon->next;
	if (!switcher.items)
		selected = switcher.initial;
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
			for (i = 0; switcher.all && i < LENGTH(tags); i++)
				if (c->tags & (1U << i))
				{
					if (*workspaces)
						strncat(workspaces, ",", sizeof workspaces - strlen(workspaces) - 1);
					strncat(workspaces, tags[i], sizeof workspaces - strlen(workspaces) - 1);
				}
			item = &switcher.items[switcher.count++];
			item->win = c->win;
			if (switcher.all)
				snprintf(item->label, sizeof item->label, "[%s] %s — %s", workspaces,
						 c->app, c->name);
			else
				snprintf(item->label, sizeof item->label, "%s — %s", c->app, c->name);
		}
	}
	switcher.selected = switcher.count ? MIN(previous, switcher.count - 1) : 0;
	for (i = 0; i < switcher.count; i++)
		if (switcher.items[i].win == selected)
			switcher.selected = i;
	if (!switcher.count)
		switcher.offset = 0;
	switcherdraw();
}

static void switcheractivate(void)
{
	Client *c = NULL;
	Monitor *mon = switcher.mon;
	Arg tag;

	if (switcher.selected < switcher.count)
		c = wintoclient(switcher.items[switcher.selected].win);
	if (switcher.tag == switcher.starttag && (!c || c->win == switcher.initial))
	{
		switcherclose();
		return;
	}
	tag.ui = 1U << switcher.tag;
	if (c && switcher.tag == switcher.starttag && (c->tags & switcher.view))
		tag.ui = switcher.view;
	else if (c && !(c->tags & tag.ui))
		tag.ui = c->tags & (~c->tags + 1U);
	switcherclose();
	selmon = mon;
	viewclient(&tag, c);
	if (c)
		XRaiseWindow(dpy, c->win);
}

static void switcherreset(void)
{
	switcher.count = switcher.selected = switcher.offset = 0;
	switcherrefresh();
}

static void switcherworkspace(int direction)
{
	unsigned int visible, tag = switcher.tag;

	visible = switcherworkspaces();
	do
		switcher.tag = (switcher.tag + LENGTH(tags) + direction) % LENGTH(tags);
	while (!(visible & (1U << switcher.tag)));
	if (switcher.tag == tag && !switcher.all)
		return;
	switcher.all = 0;
	switcherreset();
}

static void switcherkey(XKeyEvent *event)
{
	KeySym key = NoSymbol;
	Status status;
	char text[64];
	int n;
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
	case XK_Tab:
	case XK_ISO_Left_Tab:
		if (switcher.count > 1)
		{
			switcher.selected = (switcher.selected + switcher.count
				+ (key == XK_ISO_Left_Tab || (event->state & ShiftMask) ? -1 : 1))
				% switcher.count;
			switcherdraw();
		}
		return;
	case XK_grave:
	case XK_asciitilde:
		switcherworkspace(event->state & ShiftMask ? -1 : 1);
		return;
	case XK_space:
		if (event->state & ControlMask)
		{
			switcher.all = !switcher.all;
			switcherreset();
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
	switcherreset();
}

static int switchermodheld(void)
{
	unsigned int i;

	for (i = 1; i < LENGTH(switcher.down); i++)
		if (switcher.modifiers[i] && switcher.down[i])
			return 1;
	return 0;
}

static void switcherfinish(void)
{
	/* Drain input queued before the grab before committing a fast Alt release. */
	if (switcher.win && switcher.released && !XPending(dpy))
		switcheractivate();
}

static int switcherevent(XEvent *event)
{
	XEvent input, next;
	unsigned int code;

	if (!switcher.win)
		return 0;
	if (event->type == KeyPress || event->type == KeyRelease)
	{
		code = event->xkey.keycode;
		if (code >= LENGTH(switcher.down))
			return 1;
		/* Legacy autorepeat emits a synthetic release/press pair. */
		if (event->type == KeyRelease && XEventsQueued(dpy, QueuedAfterReading))
		{
			XPeekEvent(dpy, &next);
			if (next.type == KeyPress && next.xkey.keycode == code
				&& next.xkey.time == event->xkey.time)
				return 1;
		}
		if (event->type == KeyRelease)
		{
			switcher.down[code] = 0;
			if (switcher.modifiers[code] && !switchermodheld())
			{
				switcheractivate();
				return 1;
			}
		}
		else
		{
			if (switcher.modifiers[code])
				switcher.released = 0;
			switcher.down[code] = 1;
		}
		/* Alt belongs to the popup gesture, not the text being composed. */
		input = *event;
		input.xkey.state &= ~MODKEY;
		if (!XFilterEvent(&input, switcher.win) && input.type == KeyPress)
			switcherkey(&input.xkey);
		return 1;
	}
	if (event->type == ButtonPress || event->type == ButtonRelease)
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
		.border_pixel = scheme[SchemeSwitcherBorder][ColBorder].pixel
	};
	XModifierKeymap *modifiers;
	char down[32];
	unsigned int active, i, j, code;
	int border = 2;
	XRenderPictFormat *format;

	(void)arg;
	if (switcher.win)
		return;
	switcher.mon = selmon;
	switcher.initial = selmon->sel ? selmon->sel->win : None;
	switcher.view = active = selmon->tagset[selmon->seltags];
	if (selmon->sel && (selmon->sel->tags & active))
		active &= selmon->sel->tags;
	for (switcher.tag = 0; !(active & (1U << switcher.tag)); switcher.tag++);
	switcher.starttag = switcher.tag;
	switcher.rowheight = MIN(bh + 8, MAX(1, (selmon->wh - 2 * border) / 5));
	switcher.width = MIN(MAX(switcherwidth, 1), (unsigned int)MAX(1, selmon->ww - 2 * border));
	switcher.rows = MIN(MAX(switcherrows, 1),
						MAX(1, (selmon->wh - 2 * border) / (int)switcher.rowheight - 4));
	switcher.height = (switcher.rows + 4) * switcher.rowheight;
	switcher.drw = drw_create(dpy, screen, root, switcher.width, switcher.height);
	switcher.drw->fonts = drw->fonts;
	switcher.iconatom = XInternAtom(dpy, "_NET_WM_ICON", False);
	format = XRenderFindVisualFormat(dpy, DefaultVisual(dpy, screen));
	if (format)
		switcher.picture = XRenderCreatePicture(dpy, switcher.drw->drawable, format, 0, NULL);
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
	modifiers = XGetModifierMapping(dpy);
	if (modifiers)
	{
		for (i = 0; i < 8; i++)
			if (MODKEY & (1U << i))
				for (j = 0; j < modifiers->max_keypermod; j++)
				{
					code = modifiers->modifiermap[i * modifiers->max_keypermod + j];
					if (code)
						switcher.modifiers[code] = 1;
				}
		XFreeModifiermap(modifiers);
	}
	XQueryKeymap(dpy, down);
	for (i = 1; i < LENGTH(switcher.down); i++)
		switcher.down[i] = !!(down[i / 8] & (1U << (i % 8)));
	switcher.released = !switchermodheld();
	switcher.im = XOpenIM(dpy, NULL, NULL, NULL);
	if (switcher.im)
		switcher.ic = XCreateIC(switcher.im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
								XNClientWindow, switcher.win, XNFocusWindow, switcher.win, NULL);
	if (switcher.ic)
		XSetICFocus(switcher.ic);
	switcherrefresh();
}
