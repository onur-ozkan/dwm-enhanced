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
	unsigned long serial;
	unsigned int workspace;
	char app[256], title[256];
} SwitcherItem;

static struct
{
	Window win, initial;
	Monitor *mon;
	Drw *drw;
	Picture picture;
	Atom iconatom;
	SwitcherIcon *icons;
	SwitcherItem *items;
	size_t count, capacity, selected, offset;
	unsigned int tag, starttag, view, rows, width, height, rowheight, listy, footery, border;
	int released, hover, x, y;
	unsigned char modifiers[256], down[256];
} switcher;

static int switcherall = 1;	/* Start in All mode and remember subsequent choices. */

/* Cache even missing icons; navigation never reloads images. */
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

static void switcherinvalidate(Window win)
{
	SwitcherIcon **link, *icon;

	for (link = &switcher.icons; (icon = *link); link = &icon->next)
		if (icon->win == win)
		{
			*link = icon->next;
			switcherfreeicon(icon);
			return;
		}
}

static void switcherclose(void)
{
	SwitcherIcon *icon;

	if (switcher.win)
	{
		if (!switcher.hover)
		{
			XUngrabKeyboard(dpy, CurrentTime);
			XUngrabPointer(dpy, CurrentTime);
		}
		XDestroyWindow(dpy, switcher.win);
	}
	while ((icon = switcher.icons))
	{
		switcher.icons = icon->next;
		switcherfreeicon(icon);
	}
	if (switcher.picture)
		XRenderFreePicture(dpy, switcher.picture);
	if (switcher.drw)
	{
		switcher.drw->fonts = NULL;	/* Shared with the bar drawing context. */
		drw_free(switcher.drw);
	}
	free(switcher.items);
	memset(&switcher, 0, sizeof switcher);
}

static void switcherdraw(void)
{
	Drw *d = switcher.drw;
	SwitcherIcon *icon;
	SwitcherItem *item;
	Clr title[3];
	unsigned int i, x, w, side, y, h = switcher.rowheight;
	unsigned int pad = MIN(8, switcher.width / 4), appx, appw, titlex;
	int active;
	size_t index;
	char text[64];

	memcpy(title, scheme[SchemeSwitcherTitle], sizeof title);
	side = MIN(d->fonts->h, h);
	appx = MIN(switcher.width, pad + 8 + side + 10);
	appw = MIN(180, (switcher.width - appx) / 3);
	titlex = MIN(switcher.width, appx + appw + 16);
	drw_setscheme(d, scheme[SchemeNorm]);
	drw_rect(d, 0, 0, switcher.width, switcher.height, 1, 1);
	switcher.offset = MIN(switcher.offset,
		switcher.count > switcher.rows ? switcher.count - switcher.rows : 0);
	if (switcher.selected < switcher.offset)
		switcher.offset = switcher.selected;
	if (switcher.rows && switcher.count && switcher.selected >= switcher.offset + switcher.rows)
		switcher.offset = switcher.selected - switcher.rows + 1;
	for (i = 0; i < switcher.rows && switcher.offset + i < switcher.count; i++)
	{
		index = switcher.offset + i;
		item = &switcher.items[index];
		y = switcher.listy + i * h;
		active = index == switcher.selected;
		drw_setscheme(d, scheme[active ? SchemeSwitcherSel : SchemeNorm]);
		if (active)
		{
			drw_rect(d, pad, y, switcher.width - 2 * pad, h, 1, 1);
			drw_rect(d, pad, y + MIN(4, h / 4), MIN(2, switcher.width - pad),
				h - 2 * MIN(4, h / 4), 1, 0);
		}
		drw_text(d, appx, y, appw, h, 0, item->app, 0);
		icon = switchericon(item->win);
		x = pad + 8;
		if (x + side <= switcher.width - pad)
		{
			if (icon->picture)
				XRenderComposite(dpy, PictOpOver, icon->picture, None, switcher.picture,
					0, 0, 0, 0, x + (side - icon->w) / 2,
					y + (h - icon->h) / 2, icon->w, icon->h);
			else
			{
				drw_rect(d, x, y + (h - side) / 2, side, side, 0, 0);
				if (side > 4)
					drw_rect(d, x + 2, y + (h - side) / 2 + 2, side - 4, 2, 1, 0);
			}
		}
		title[ColBg] = scheme[active ? SchemeSwitcherSel : SchemeNorm][ColBg];
		drw_setscheme(d, title);
		drw_text(d, titlex, y, MAX(0, (int)switcher.width - (int)pad - (int)titlex),
			h, 0, item->title, 0);
	}
	drw_setscheme(d, scheme[SchemeNorm]);
	if (!switcher.hover)
	{
		snprintf(text, sizeof text, "%zu/%zu",
			switcher.count ? switcher.selected + 1 : 0, switcher.count);
		w = MIN(switcher.width - 2 * pad, drw_fontset_getwidth(d, text));
		x = switcher.width - pad - w;
		drw_text(d, x, switcher.footery, w, h, 0, text, 0);
		snprintf(text, sizeof text, "Workspace: %s", switcherall ? "All" : tags[switcher.tag]);
		drw_setscheme(d, scheme[SchemeSwitcherWorkspace]);
		drw_text(d, pad, switcher.footery, MAX(0, (int)x - 2 * (int)pad), h, 0, text, 0);
		drw_setscheme(d, scheme[SchemeNorm]);
	}
	drw_map(d, switcher.win, 0, 0, switcher.width, switcher.height);
}

static int switchergeometry(void)
{
	Monitor *m = switcher.mon;
	int border = switcher.hover ? 0 : 2, available = MAX(1, m->wh - 2 * border);
	int oldx = switcher.x, oldy = switcher.y, changed;
	int footer = !switcher.hover, pad;
	XRenderPictFormat *format;

	switcher.border = border;
	pad = MIN(8, MAX(0, (available - 1 - footer) / 3));
	switcher.rowheight = MIN(bh + 8, MAX(1, (available - (2 + footer) * pad) / (1 + footer)));
	switcher.listy = pad;
	switcher.width = MIN(MAX(switcherwidth, 1), (unsigned int)MAX(1, m->ww - 2 * border));
	switcher.rows = MIN(MAX(switcherrows, 1),
		MAX(0, (available - (2 + footer) * pad) / (int)switcher.rowheight - footer));
	if (switcher.hover)
		switcher.rows = MIN(switcher.rows, switcher.count);
	switcher.footery = pad + switcher.rows * switcher.rowheight + pad;
	switcher.height = switcher.footery + footer * (switcher.rowheight + pad);
	if (switcher.hover)
	{
		switcher.x = m->wx + sp + barborderpx
			+ (switcher.tag ? m->tagend[switcher.tag - 1] : 0);
		switcher.y = m->by + vp + (m->topbar ? BARHEIGHT : -(int)switcher.height - 2 * border);
		switcher.x = MAX(m->mx, MIN(switcher.x, m->mx + m->mw - (int)switcher.width - 2 * border));
		switcher.y = MAX(m->my, MIN(switcher.y, m->my + m->mh - (int)switcher.height - 2 * border));
	}
	else
	{
		switcher.x = m->wx + (m->ww - (int)switcher.width - 2 * border) / 2;
		switcher.y = m->wy + (m->wh - (int)switcher.height - 2 * border) / 2;
	}
	changed = oldx != switcher.x || oldy != switcher.y || !switcher.drw
		|| switcher.drw->w != switcher.width || switcher.drw->h != switcher.height;
	if (!switcher.drw)
	{
		switcher.drw = drw_create(dpy, screen, root, switcher.width, switcher.height);
		switcher.drw->fonts = drw->fonts;
	}
	else if (switcher.drw->w != switcher.width || switcher.drw->h != switcher.height)
	{
		if (switcher.picture)
			XRenderFreePicture(dpy, switcher.picture);
		switcher.picture = None;
		drw_resize(switcher.drw, switcher.width, switcher.height);
	}
	if (!switcher.picture && (format = XRenderFindVisualFormat(dpy, DefaultVisual(dpy, screen))))
		switcher.picture = XRenderCreatePicture(dpy, switcher.drw->drawable, format, 0, NULL);
	if (switcher.win && changed)
		XMoveResizeWindow(dpy, switcher.win, switcher.x, switcher.y, switcher.width, switcher.height);
	return changed;
}

static int switchercompare(const void *left, const void *right)
{
	const SwitcherItem *a = left, *b = right;

	if (a->workspace != b->workspace)
		return a->workspace < b->workspace ? -1 : 1;
	return (a->serial > b->serial) - (a->serial < b->serial);
}

static int switcherlabel(SwitcherItem *item, const Client *c)
{
	char app[sizeof item->app];
	unsigned int workspace;

	if (switcherall && !switcher.hover)
	{
		for (workspace = 0; !(item->workspace & (1U << workspace)); workspace++);
		snprintf(app, sizeof app, "[%s] ", tags[workspace]);
		strncat(app, c->app, sizeof app - strlen(app) - 1);
	}
	else
		snprintf(app, sizeof app, "%s", c->app);
	if (!strcmp(item->app, app) && !strcmp(item->title, c->name))
		return 0;
	strcpy(item->app, app);
	strcpy(item->title, c->name);
	return 1;
}

static void switcherrefresh(void)
{
	Client *c;
	SwitcherItem *item;
	Window selected = switcher.selected < switcher.count
		? switcher.items[switcher.selected].win : None;
	size_t capacity = 0, previous = switcher.selected;
	size_t i;
	int all = switcherall && !switcher.hover;

	if (!switcher.win && !switcher.hover)
		selected = switcher.initial;
	for (c = switcher.mon->clients; c; c = c->next)
		if (all || (c->tags & (1U << switcher.tag)))
			capacity++;
	if (switcher.hover && !capacity)
	{
		switcherclose();
		return;
	}
	if (capacity > switcher.capacity)
	{
		free(switcher.items);
		switcher.capacity = MAX(capacity, 2 * switcher.capacity);
		switcher.items = ecalloc(switcher.capacity, sizeof *switcher.items);
	}
	switcher.count = 0;
	for (c = switcher.mon->clients; c; c = c->next)
	{
		if (!all && !(c->tags & (1U << switcher.tag)))
			continue;
		item = &switcher.items[switcher.count++];
		item->win = c->win;
		item->serial = c->serial;
		item->workspace = all ? c->tags & (~c->tags + 1U) : 1U << switcher.tag;
		switcherlabel(item, c);
	}
	if (switcher.count > 1)
		qsort(switcher.items, switcher.count, sizeof *switcher.items, switchercompare);
	switcher.selected = switcher.count ? MIN(previous, switcher.count - 1) : 0;
	for (i = 0; i < switcher.count; i++)
		if (switcher.items[i].win == selected)
			switcher.selected = i;
	if (!switcher.count)
		switcher.offset = 0;
	switchergeometry();
	if (switcher.win)
		switcherdraw();
}

static void switcherupdate(const Client *c)
{
	size_t i;

	if (!switcher.win || c->mon != switcher.mon)
		return;
	for (i = 0; i < switcher.count; i++)
		if (switcher.items[i].win == c->win)
		{
			if (switcherlabel(&switcher.items[i], c) && i >= switcher.offset
				&& i < switcher.offset + switcher.rows)
				switcherdraw();
			return;
		}
}

static void switchericonchanged(Window win)
{
	size_t i;

	switcherinvalidate(win);
	for (i = switcher.offset; i < switcher.count && i < switcher.offset + switcher.rows; i++)
		if (switcher.items[i].win == win)
		{
			switcherdraw();
			break;
		}
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
	if (selmon != mon)
		unfocus(selmon->sel, 1);
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

static void switcherkey(XKeyEvent *event)
{
	KeySym key = XLookupKeysym(event, 0);

	if (key >= XK_1 && key <= XK_9 && key - XK_1 < LENGTH(tags))
	{
		switcher.tag = key - XK_1;
		/* An explicit number commits one workspace, even from a combined view. */
		switcher.starttag = LENGTH(tags);
		switcherall = 0;
		switcherreset();
		return;
	}
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
	case XK_0:
		switcherall = !switcherall;
		switcherreset();
		break;
	}
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
	if (switcher.win && !switcher.hover && switcher.released && !XPending(dpy))
		switcheractivate();
}

static size_t switcheritemat(int y)
{
	if (y < (int)switcher.listy || y >= (int)(switcher.listy + switcher.rows * switcher.rowheight))
		return switcher.count;
	return MIN(switcher.count, switcher.offset + (y - switcher.listy) / switcher.rowheight);
}

static int switcherevent(XEvent *event)
{
	XEvent next;
	unsigned int code;
	size_t index;

	if (!switcher.win)
		return 0;
	if (switcher.hover)
	{
		if (event->type == KeyPress ||
			(event->type == ButtonPress && event->xbutton.window != switcher.win))
		{
			switcherclose();
			return 0;
		}
		if (event->type == ButtonPress)
		{
			if (event->xbutton.button == Button1 && event->xbutton.x >= 0
				&& event->xbutton.x < (int)switcher.width)
			{
				index = switcheritemat(event->xbutton.y);
				if (index < switcher.count)
				{
					switcher.selected = index;
					switcheractivate();
				}
			}
			else if (event->xbutton.button == Button4 || event->xbutton.button == Button5)
			{
				if (event->xbutton.button == Button4 && switcher.offset)
					switcher.offset--;
				else if (event->xbutton.button == Button5 && switcher.offset + switcher.rows < switcher.count)
					switcher.offset++;
				else
					return 1;
				index = switcheritemat(event->xbutton.y);
				switcher.selected = index < switcher.count ? index
					: MAX(switcher.offset, MIN(switcher.selected, switcher.offset + switcher.rows - 1));
				switcherdraw();
			}
			return 1;
		}
		if (event->type == KeyRelease || event->type == ButtonRelease)
			return event->type == ButtonRelease && event->xbutton.window == switcher.win;
	}
	else if (event->type == KeyPress || event->type == KeyRelease)
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
		if (event->type == KeyPress)
			switcherkey(&event->xkey);
		return 1;
	}
	if (!switcher.hover && (event->type == ButtonPress || event->type == ButtonRelease))
		return 1;
	if (event->type == Expose && event->xexpose.window == switcher.win)
	{
		if (!event->xexpose.count)
			drw_map(switcher.drw, switcher.win, 0, 0, switcher.width, switcher.height);
		return 1;
	}
	return 0;
}

static void switchershow(Monitor *m, int hover, unsigned int tag)
{
	XSetWindowAttributes attributes = {.override_redirect = True,.event_mask = ExposureMask,
		.border_pixel = scheme[SchemeSwitcherBorder][ColBorder].pixel
	};
	XModifierKeymap *modifiers;
	char down[32];
	unsigned int i, j, code;

	switcher.mon = m;
	switcher.hover = hover;
	switcher.tag = tag;
	switcher.initial = m->sel ? m->sel->win : None;
	switcher.view = m->tagset[m->seltags];
	switcher.starttag = hover ? LENGTH(tags) : tag;
	switcher.iconatom = XInternAtom(dpy, "_NET_WM_ICON", False);
	switcherrefresh();
	if (!switcher.drw)
		return;
	if (hover)
		attributes.event_mask |= PointerMotionMask | EnterWindowMask | LeaveWindowMask | BUTTONMASK;
	switcher.win = XCreateWindow(dpy, root, switcher.x, switcher.y,
								 switcher.width, switcher.height, switcher.border, CopyFromParent,
								 CopyFromParent, CopyFromParent,
								 CWOverrideRedirect | CWEventMask | CWBorderPixel, &attributes);
	XStoreName(dpy, switcher.win, "dwm window switcher");
	XMapRaised(dpy, switcher.win);
	if (hover)
	{
		switcherdraw();
		return;
	}
	if (XGrabKeyboard(dpy, switcher.win, False, GrabModeAsync, GrabModeAsync, CurrentTime) !=
		GrabSuccess)
	{
		switcherclose();
		return;
	}
	if (XGrabPointer(dpy, switcher.win, False, ButtonPressMask | ButtonReleaseMask,
					 GrabModeAsync, GrabModeAsync, None, cursor[CurNormal],
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
	switcherdraw();
}

static void switcheropen(const Arg *arg)
{
	unsigned int active, tag;

	(void)arg;
	if (switcher.win && !switcher.hover)
		return;
	switcherclose();
	active = selmon->tagset[selmon->seltags];
	if (selmon->sel && (selmon->sel->tags & active))
		active &= selmon->sel->tags;
	for (tag = 0; !(active & (1U << tag)); tag++);
	switchershow(selmon, 0, tag);
}

static void hovernotify(XEvent *event)
{
	Monitor *m;
	Client *c;
	Window child, rootret;
	int x, y, wx, wy, bx, by, tag;
	unsigned int mask;
	size_t index;

	if (switcher.win && !switcher.hover)
		return;
	if (event && event->type == MotionNotify)
	{
		coalescemotion(event);
		/* Unchanged hits need neither a server round trip nor a redraw. */
		if (switcher.win && event->xmotion.window == switcher.win
			&& event->xmotion.x >= 0 && event->xmotion.x < (int)switcher.width
			&& event->xmotion.y >= 0 && event->xmotion.y < (int)switcher.height)
		{
			index = switcheritemat(event->xmotion.y);
			if (index >= switcher.count || index == switcher.selected)
				return;
		}
		else
			for (m = mons; m; m = m->next)
				if (event->xmotion.window == m->barwin && event->xmotion.y >= 0
					&& event->xmotion.y < bh && event->xmotion.x >= 0
					&& event->xmotion.x < BARWIDTH(m))
				{
					tag = bartag(m, event->xmotion.x);
					if (!switcher.win)
					{
						if (tag < 0)
							return;
						for (c = m->clients; c && !(c->tags & (1U << tag)); c = c->next);
						if (!c)
							return;
					}
					else if (switcher.mon == m && switcher.tag == (unsigned int)tag)
						return;
					break;
				}
	}
	/* Query the current position so queued crossing events cannot close a popup
	 * that the pointer has already entered from the bar. */
	if (!XQueryPointer(dpy, root, &rootret, &child, &x, &y, &wx, &wy, &mask))
	{
		switcherclose();
		return;
	}
	if (switcher.win && child == switcher.win)
	{
		index = switcheritemat(y - switcher.y - switcher.border);
		if (index < switcher.count && index != switcher.selected)
		{
			switcher.selected = index;
			switcherdraw();
		}
		return;
	}
	for (m = mons; m; m = m->next)
	{
		bx = m->wx + sp + barborderpx;
		by = m->by + vp + barborderpx;
		if (child != m->barwin || !m->showbar || y < by || y >= by + bh
			|| x < bx || x >= bx + BARWIDTH(m))
			continue;
		if ((tag = bartag(m, x - bx)) < 0)
			break;
		if (switcher.win && switcher.mon != m)
			switcherclose();
		if (!switcher.win)
			switchershow(m, 1, tag);
		else if (switcher.tag != (unsigned int)tag)
		{
			switcher.tag = tag;
			switcherreset();
		}
		else if (switchergeometry())
			switcherdraw();
		return;
	}
	switcherclose();
}
