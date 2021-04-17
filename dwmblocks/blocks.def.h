//Modify this file to change what commands output to your statusbar, and recompile using the make command.
static const Block blocks[] = {
	/*Icon*/	/*Command*/		/*Update Interval*/	/*Update Signal*/
	{ " ", "./modules/kernel",		0,		0	},
	{ " ", "./modules/cpu_temp",	3,		0	},
//	{ " ", "./modules/date",		3600,	0	},
	{ " ", "./modules/memory",		3,		0	},
	{ " ", "./modules/brightness",	0,		11	},
	{ "", "./modules/volume",		0,		10	}, // this one have dynamic icon, so keep the icon column as blank value
	{ "",	"./modules/network",	1,	    0   },
	{ " ", "./modules/uptime",		60,		0	},
	{ "", "./modules/battery",		3,		0	}, // this one have dynamic icon, so keep the icon column as blank value
	{ " ", "./modules/clock",		3,		0	},
};

//sets delimeter between status commands. NULL character ('\0') means no delimeter.
static char delim[] = " | ";
static unsigned int delimLen = 5;
