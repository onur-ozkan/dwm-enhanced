/* See LICENSE file for copyright and license details. */

#include <X11/XF86keysym.h>
#include "movestack.h"

/* appearance */
#define GAP_SIZE 5        						/* default gap size between windows*/
static const unsigned int gappx     = GAP_SIZE;
static const unsigned int snap      = 32;       /* snap pixel */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const int focusonwheel       = 0;
static const char *fonts[]          = { "RobotoMono Nerd Font:size=12" };
static const char dmenufont[]       = "Roboto Mono:size=12";
static const char col_soft_black[]  = "#222222";
static const char col_gray[]  = "#616161";
static const char col_dirty_white[] = "#eeeeee";
static const char col_red[]         = "#d75f5f";
static const char col_dark_yellow[] = "#f7ca88";
static const char col_white[]       = "#f5f5f5";
static const char col_green[]       = "#87af5f";
static const char col_blue[]        = "#87afd7";

static const char *colors[][3]      = {
	/*                          fg                   bg                 border                    */
	[SchemeNorm]    =	 { col_dirty_white,     col_soft_black,     col_gray        },   // Normal
	[SchemeSel]     =	 { col_dark_yellow,     col_soft_black,     col_soft_black  },   // Selected
	[SchemeRed] =	     { col_red,             col_soft_black,		col_red         },
	[SchemeBlue] =	     { col_blue,            col_soft_black,     col_blue        },
	[SchemeGreen] =	     { col_green,           col_soft_black,     col_green       },
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
	{ "Gimp",     NULL,       NULL,       0,            1,           -1 },
	{ "Firefox",  NULL,       NULL,       1 << 8,       0,           -1 },
};

/* layout(s) */
static const float mfact     = 0.5; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "﬿ ",     tile            },    /* first entry is default */
	{ " ",      NULL            },    /* no layout function means floating behavior */
	{ " ",      monocle         },
	{ "ﳶ ",      bstack          },
	{ "יִ ",      centeredmaster   },
	{ "ﱖ ",	     gaplessgrid     },
};

/* key definitions */
#define MODKEY Mod1Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_soft_black, "-nf", col_dirty_white, "-sb", col_soft_black, "-sf", col_dark_yellow, NULL };
static const char *termcmd[]  = { "st", NULL };

// static const char *toggle_mic[] = { "/usr/bin/amixer", "set", "Capture", "toggle", NULL }

static Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_p,      spawn,          {.v = dmenucmd } },
	{ MODKEY|ShiftMask,             XK_Return, spawn,          {.v = termcmd } },
	{ MODKEY,                       XK_b,      togglebar,      {0} },
	{ MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_k,      focusstack,     {.i = -1 } },
	{ MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },
	{ MODKEY,                       XK_d,      incnmaster,     {.i = -1 } },
	{ MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },
	{ MODKEY,                       XK_l,      setmfact,       {.f = +0.05} },
	{ MODKEY,                       XK_Return, zoom,           {0} },
	{ MODKEY,                       XK_Tab,    view,           {0} },
	{ MODKEY|ShiftMask,             XK_e,      killclient,     {0} },
	{ MODKEY|ShiftMask,             XK_t,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY|ShiftMask,             XK_f,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY|ShiftMask,             XK_m,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY|ShiftMask,             XK_b,      setlayout,      {.v = &layouts[3]} },
	{ MODKEY|ShiftMask,             XK_g,      setlayout,      {.v = &layouts[5]} },
	{ MODKEY|ShiftMask,             XK_c,      setlayout,      {.v = &layouts[4]} },
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
	{ MODKEY,                       XK_minus,  setgaps,        {.i = -1 } },
	{ MODKEY,                       XK_equal,  setgaps,        {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_equal,  resetgaps,      {.i = GAP_SIZE  } },
	{ MODKEY|ShiftMask,             XK_j,      movestack,      {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_k,      movestack,      {.i = -1 } },
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)
	{ MODKEY|ShiftMask,             XK_q,      quit,           {0} },
	/* AUDIO */
	{ 0,                       XF86XK_AudioRaiseVolume, spawn, SHCMD("/usr/bin/amixer -q set Master 5%+ unmute; kill -44 $(pidof dwmblocks)") },
	{ 0,                       XF86XK_AudioLowerVolume, spawn, SHCMD("/usr/bin/amixer -q set Master 5%- unmute; kill -44 $(pidof dwmblocks)") },
	{ 0,                       XF86XK_AudioMute, spawn, SHCMD("/usr/bin/amixer -q set Master toggle; kill -44 $(pidof dwmblocks)") },
	/* BRIGHTNESS */
	{ MODKEY,				   XK_F7, spawn, SHCMD("/usr/bin/brightnessctl s 5%- && kill -45 $(pidof dwmblocks)") },
	{ MODKEY,				   XK_F8, spawn, SHCMD("/usr/bin/brightnessctl s +5% && kill -45 $(pidof dwmblocks)") },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};

