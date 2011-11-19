// mgr preferences
static bool focusoncreate = true;

// monitor preferences
static bool bottomstack = false;
static double mastersize = 0.75;

// default applications
static const char *app_term[] = { "urxvt", NULL };

// key bindings
static Key keys[] = {
	{ XCB_MOD_MASK_4,			10, spawn,	{ .c = app_term } },
	{ XCB_MOD_MASK_4,			24, quit,	{ 0 } },
	{ XCB_MOD_MASK_1,			53, ckill,	{ 0 } },
	{ XCB_MOD_MASK_1,			54, cclose,	{ 0 } },
	{ XCB_MOD_MASK_1,			23, focus,	{ .i = +1 } },
	{ XCB_MOD_MASK_1|XCB_MOD_MASK_SHIFT,	23, focus,	{ .i = -1 } }
};
