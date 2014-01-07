/* 
 * ckMenu.c --
 *
 *	This module implements menus for the  toolkit.  The menus
 *	support normal button entries, plus check buttons, radio
 *	buttons, iconic forms of all of the above, and separator
 *	entries.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 * Copyright (c) 1995 Christian Werner
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "ckPort.h"
#include "ck.h"
#include "default.h"

#ifdef __WIN32__
#define DestroyMenu CkDestroyMenu
#endif

/*
 * One of the following data structures is kept for each entry of each
 * menu managed by this file:
 */

typedef struct MenuEntry {
    int type;			/* Type of menu entry;  see below for
				 * valid types. */
    struct Menu *menuPtr;	/* Menu with which this entry is associated. */
    char *label;		/* Main text label displayed in entry (NULL
				 * if no label).  Malloc'ed. */
    int labelLength;		/* Number of non-NULL characters in label. */
    int underline;		/* Index of character to underline. */
    char *accel;		/* Accelerator string displayed at right
				 * of menu entry.  NULL means no such
				 * accelerator.  Malloc'ed. */
    int accelLength;		/* Number of non-NULL characters in
				 * accelerator. */

    /*
     * Information related to displaying entry:
     */

    Ck_Uid state;		/* State of button for display purposes:
				 * normal, active, or disabled. */
    int y;			/* Y-coordinate of entry. */
    int indicatorOn;		/* True means draw indicator, false means
				 * don't draw it. */


    int normalBg;
    int normalFg;
    int normalAttr;
    int activeBg;
    int activeFg;
    int activeAttr;
    int disabledBg;
    int disabledFg;
    int disabledAttr;
    int underlineFg;
    int underlineAttr;
    int indicatorFg;

    /*
     * Information used to implement this entry's action:
     */

    char *command;		/* Command to invoke when entry is invoked.
				 * Malloc'ed. */
    char *name;			/* Name of variable (for check buttons and
				 * radio buttons) or menu (for cascade
				 * entries).  Malloc'ed.*/
    char *onValue;		/* Value to store in variable when selected
				 * (only for radio and check buttons).
				 * Malloc'ed. */
    char *offValue;		/* Value to store in variable when not
				 * selected (only for check buttons).
				 * Malloc'ed. */

    /*
     * Miscellaneous information:
     */

    int flags;			/* Various flags. See below for definitions. */
} MenuEntry;

/*
 * Flag values defined for menu entries:
 *
 * ENTRY_SELECTED:		Non-zero means this is a radio or check
 *				button and that it should be drawn in
 *				the "selected" state.
 * ENTRY_NEEDS_REDISPLAY:	Non-zero means the entry should be redisplayed.
 */

#define ENTRY_SELECTED		1
#define ENTRY_NEEDS_REDISPLAY	4

/*
 * Types defined for MenuEntries:
 */

#define COMMAND_ENTRY		0
#define SEPARATOR_ENTRY		1
#define CHECK_BUTTON_ENTRY	2
#define RADIO_BUTTON_ENTRY	3
#define CASCADE_ENTRY		4

/*
 * Mask bits for above types:
 */

#define COMMAND_MASK		CK_CONFIG_USER_BIT
#define SEPARATOR_MASK		(CK_CONFIG_USER_BIT << 1)
#define CHECK_BUTTON_MASK	(CK_CONFIG_USER_BIT << 2)
#define RADIO_BUTTON_MASK	(CK_CONFIG_USER_BIT << 3)
#define CASCADE_MASK		(CK_CONFIG_USER_BIT << 4)
#define ALL_MASK		(COMMAND_MASK | SEPARATOR_MASK \
	| CHECK_BUTTON_MASK | RADIO_BUTTON_MASK | CASCADE_MASK)

/*
 * Configuration specs for individual menu entries:
 */

static Ck_ConfigSpec entryConfigSpecs[] = {
    {CK_CONFIG_ATTR, "-activeattributes", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_ACTIVE_ATTR, Ck_Offset(MenuEntry, activeAttr),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|CK_CONFIG_DONT_SET_DEFAULT},
    {CK_CONFIG_COLOR, "-activebackground", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_ACTIVE_BG, Ck_Offset(MenuEntry, activeBg),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|CK_CONFIG_DONT_SET_DEFAULT},
    {CK_CONFIG_COLOR, "-activeforeground", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_ACTIVE_FG, Ck_Offset(MenuEntry, activeFg),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|CK_CONFIG_DONT_SET_DEFAULT},
    {CK_CONFIG_STRING, "-accelerator", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_ACCELERATOR, Ck_Offset(MenuEntry, accel),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|CK_CONFIG_NULL_OK},
    {CK_CONFIG_ATTR, "-attributes", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_ATTR, Ck_Offset(MenuEntry, normalAttr),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|CK_CONFIG_DONT_SET_DEFAULT},
    {CK_CONFIG_COLOR, "-background", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_BG, Ck_Offset(MenuEntry, normalBg),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|CK_CONFIG_DONT_SET_DEFAULT},
    {CK_CONFIG_STRING, "-command", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_COMMAND, Ck_Offset(MenuEntry, command),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|CK_CONFIG_NULL_OK},
    {CK_CONFIG_COLOR, "-foreground", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_FG, Ck_Offset(MenuEntry, normalFg),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|CK_CONFIG_NULL_OK},
    {CK_CONFIG_BOOLEAN, "-indicatoron", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_INDICATOR, Ck_Offset(MenuEntry, indicatorOn),
	CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CK_CONFIG_DONT_SET_DEFAULT},
    {CK_CONFIG_STRING, "-label", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_LABEL, Ck_Offset(MenuEntry, label),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK},
    {CK_CONFIG_STRING, "-menu", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_MENU, Ck_Offset(MenuEntry, name),
	CASCADE_MASK|CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-offvalue", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_OFF_VALUE, Ck_Offset(MenuEntry, offValue),
	CHECK_BUTTON_MASK},
    {CK_CONFIG_STRING, "-onvalue", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_ON_VALUE, Ck_Offset(MenuEntry, onValue),
	CHECK_BUTTON_MASK},
    {CK_CONFIG_COLOR, "-selectcolor", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_SELECT, Ck_Offset(MenuEntry, indicatorFg),
	CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CK_CONFIG_NULL_OK},
    {CK_CONFIG_UID, "-state", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_STATE, Ck_Offset(MenuEntry, state),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|CK_CONFIG_DONT_SET_DEFAULT},
    {CK_CONFIG_STRING, "-value", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_VALUE, Ck_Offset(MenuEntry, onValue),
	RADIO_BUTTON_MASK|CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-variable", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_CHECK_VARIABLE, Ck_Offset(MenuEntry, name),
	CHECK_BUTTON_MASK|CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-variable", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_RADIO_VARIABLE, Ck_Offset(MenuEntry, name),
	RADIO_BUTTON_MASK},
    {CK_CONFIG_INT, "-underline", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_UNDERLINE, Ck_Offset(MenuEntry, underline),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|CK_CONFIG_DONT_SET_DEFAULT},
    {CK_CONFIG_ATTR, "-underlineattributes", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_UNDERLINE, Ck_Offset(MenuEntry, underlineAttr),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|CK_CONFIG_DONT_SET_DEFAULT},
    {CK_CONFIG_COLOR, "-underlineforeground", (char *) NULL, (char *) NULL,
	DEF_MENU_ENTRY_UNDERLINE, Ck_Offset(MenuEntry, underlineFg),
	COMMAND_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CASCADE_MASK
	|CK_CONFIG_DONT_SET_DEFAULT},
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * A data structure of the following type is kept for each
 * menu managed by this file:
 */

typedef struct Menu {
    CkWindow *winPtr;		/* Window that embodies the pane.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Tcl_Interp *interp;		/* Interpreter associated with menu. */
    Tcl_Command widgetCmd;	/* Token for menu's widget command. */
    MenuEntry **entries;	/* Array of pointers to all the entries
				 * in the menu.  NULL means no entries. */
    int numEntries;		/* Number of elements in entries. */
    int active;			/* Index of active entry.  -1 means
				 * nothing active. */

    /*
     * Information used when displaying widget:
     */

    int normalBg;
    int normalFg;
    int normalAttr;
    int activeBg;
    int activeFg;
    int activeAttr;
    int disabledBg;
    int disabledFg;
    int disabledAttr;
    int underlineFg;
    int underlineAttr;
    int indicatorFg;
    CkBorder *borderPtr;
    int labelWidth;		/* Number of chars to allow for displaying
				 * labels in menu entries. */
    int indicatorSpace;         /* Number of chars for displaying
				 * indicators. */

    /*
     * Miscellaneous information:
     */

    char *takeFocus;		/* Value of -takefocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
    char *postCommand;		/* Command to execute just before posting
				 * this menu, or NULL.  Malloc-ed. */
    MenuEntry *postedCascade;	/* Points to menu entry for cascaded
				 * submenu that is currently posted, or
				 * NULL if no submenu posted. */
    int flags;			/* Various flags;  see below for
				 * definitions. */
} Menu;

/*
 * Flag bits for menus:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 * RESIZE_PENDING:		Non-zero means a call to ComputeMenuGeometry
 *				has already been scheduled.
 */

#define REDRAW_PENDING		1
#define RESIZE_PENDING		2

/*
 * Configuration specs valid for the menu as a whole:
 */

static Ck_ConfigSpec configSpecs[] = {
    {CK_CONFIG_ATTR, "-activeattributes", "activeAttributes",
        "ActiveAttributes", DEF_MENU_ACTIVE_ATTR_COLOR,
        Ck_Offset(Menu, activeAttr), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_ATTR, "-activeattributes", "activeAttributes",
        "ActiveAttributes", DEF_MENU_ACTIVE_ATTR_MONO,
        Ck_Offset(Menu, activeAttr), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-activebackground", "activeBackground", "Foreground",
	DEF_MENU_ACTIVE_BG_COLOR, Ck_Offset(Menu, activeBg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-activebackground", "activeBackground", "Foreground",
	DEF_MENU_ACTIVE_BG_MONO, Ck_Offset(Menu, activeBg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_MENU_ACTIVE_FG_COLOR, Ck_Offset(Menu, activeFg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_MENU_ACTIVE_FG_MONO, Ck_Offset(Menu, activeFg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_ATTR, "-attributes", "attributes", "Attributes",
        DEF_MENU_ATTR, Ck_Offset(Menu, normalAttr), 0},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_MENU_BG_COLOR, Ck_Offset(Menu, normalBg), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_MENU_BG_MONO, Ck_Offset(Menu, normalBg), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_BORDER, "-border", "border", "Border",
        DEF_MENU_BORDER, Ck_Offset(Menu, borderPtr), CK_CONFIG_NULL_OK},
    {CK_CONFIG_ATTR, "-disabledattributes", "disabledAttributes",
        "DisabledAttributes", DEF_MENU_DISABLED_ATTR,
        Ck_Offset(Menu, disabledAttr), 0},
    {CK_CONFIG_COLOR, "-disabledbackground", "disabledBackground",
	"Foreground", DEF_MENU_DISABLED_BG_COLOR,
	Ck_Offset(Menu, disabledBg), CK_CONFIG_COLOR_ONLY|CK_CONFIG_NULL_OK},
    {CK_CONFIG_COLOR, "-disabledbackground", "disabledBackground",
	"Foreground", DEF_MENU_DISABLED_BG_MONO,
	Ck_Offset(Menu, disabledBg), CK_CONFIG_MONO_ONLY|CK_CONFIG_NULL_OK},
    {CK_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
	"DisabledForeground", DEF_MENU_DISABLED_FG_COLOR,
	Ck_Offset(Menu, disabledFg), CK_CONFIG_COLOR_ONLY|CK_CONFIG_NULL_OK},
    {CK_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
	"DisabledForeground", DEF_MENU_DISABLED_FG_MONO,
	Ck_Offset(Menu, disabledFg), CK_CONFIG_MONO_ONLY|CK_CONFIG_NULL_OK},
    {CK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_MENU_FG, Ck_Offset(Menu, normalFg), 0},
    {CK_CONFIG_STRING, "-postcommand", "postCommand", "Command",
	DEF_MENU_POST_COMMAND, Ck_Offset(Menu, postCommand),
        CK_CONFIG_NULL_OK},
    {CK_CONFIG_COLOR, "-selectcolor", "selectColor", "Background",
	DEF_MENU_SELECT_COLOR, Ck_Offset(Menu, indicatorFg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-selectcolor", "selectColor", "Background",
	DEF_MENU_SELECT_MONO, Ck_Offset(Menu, indicatorFg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_MENU_TAKE_FOCUS, Ck_Offset(Menu, takeFocus), CK_CONFIG_NULL_OK},
    {CK_CONFIG_ATTR, "-underlineattributes", "underlineAttributes",
        "UnderlineAttributes", DEF_MENU_UNDERLINE_ATTR,
        Ck_Offset(Menu, underlineAttr), CK_CONFIG_NULL_OK},
    {CK_CONFIG_COLOR, "-underlineforeground", "underlineForeground",
	"UnderlineForeground", DEF_MENU_UNDERLINE_FG_COLOR,
	Ck_Offset(Menu, underlineFg), CK_CONFIG_COLOR_ONLY|CK_CONFIG_NULL_OK},
    {CK_CONFIG_COLOR, "-underlineforeground", "underlineForeground",
	"UnderlineForeground", DEF_MENU_UNDERLINE_FG_MONO,
	Ck_Offset(Menu, underlineFg), CK_CONFIG_MONO_ONLY|CK_CONFIG_NULL_OK},
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static int		ActivateMenuEntry _ANSI_ARGS_((Menu *menuPtr,
			    int index));
static void		ComputeMenuGeometry _ANSI_ARGS_((
			    ClientData clientData));
static int		ConfigureMenu _ANSI_ARGS_((Tcl_Interp *interp,
			    Menu *menuPtr, int argc, char **argv,
			    int flags));
static int		ConfigureMenuEntry _ANSI_ARGS_((Tcl_Interp *interp,
			    Menu *menuPtr, MenuEntry *mePtr, int index,
			    int argc, char **argv, int flags));
static void		DestroyMenu _ANSI_ARGS_((ClientData clientData));
static void		DestroyMenuEntry _ANSI_ARGS_((ClientData clientData));
static void		DisplayMenu _ANSI_ARGS_((ClientData clientData));
static void		EventuallyRedrawMenu _ANSI_ARGS_((Menu *menuPtr,
			    MenuEntry *mePtr));
static int		GetMenuIndex _ANSI_ARGS_((Tcl_Interp *interp,
			    Menu *menuPtr, char *string, int lastOK,
			    int *indexPtr));
static int		MenuAddOrInsert _ANSI_ARGS_((Tcl_Interp *interp,
			    Menu *menuPtr, char *indexString, int argc,
			    char **argv));
static void		MenuCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static void		MenuEventProc _ANSI_ARGS_((ClientData clientData,
			    CkEvent *eventPtr));
static MenuEntry *	MenuNewEntry _ANSI_ARGS_((Menu *menuPtr, int index,
			    int type));
static char *		MenuVarProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));
static int		MenuWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static int		PostSubmenu _ANSI_ARGS_((Tcl_Interp *interp,
			    Menu *menuPtr, MenuEntry *mePtr));

/*
 *--------------------------------------------------------------
 *
 * Ck_MenuCmd --
 *
 *	This procedure is invoked to process the "menu" Tcl
 *	command.  See the user documentation for details on
 *	what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
Ck_MenuCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *mainPtr = (CkWindow *) clientData;
    CkWindow *new;
    register Menu *menuPtr;
 
    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Create the new window.
     */

    new = Ck_CreateWindowFromPath(interp, mainPtr, argv[1], 1);
    if (new == NULL) {
	return TCL_ERROR;
    }

    /*
     * Initialize the data structure for the menu.
     */

    menuPtr = (Menu *) ckalloc(sizeof(Menu));
    menuPtr->winPtr = new;
    menuPtr->interp = interp;
    menuPtr->widgetCmd = Tcl_CreateCommand(interp,
	    menuPtr->winPtr->pathName, MenuWidgetCmd,
	    (ClientData) menuPtr, MenuCmdDeletedProc);
    menuPtr->entries = NULL;
    menuPtr->numEntries = 0;
    menuPtr->active = -1;
    menuPtr->normalBg = 0;
    menuPtr->normalFg = 0;
    menuPtr->normalAttr = 0;
    menuPtr->activeBg = 0;
    menuPtr->activeFg = 0;
    menuPtr->activeAttr = 0;
    menuPtr->disabledBg = 0;
    menuPtr->disabledFg = 0;
    menuPtr->disabledAttr = 0;
    menuPtr->underlineFg = 0;
    menuPtr->underlineAttr = 0;
    menuPtr->indicatorFg = 0;
    menuPtr->borderPtr = NULL;
    menuPtr->labelWidth = 0;
    menuPtr->takeFocus = NULL;
    menuPtr->postCommand = NULL;
    menuPtr->postedCascade = NULL;
    menuPtr->flags = 0;

    Ck_SetClass(new, "Menu");
    Ck_CreateEventHandler(menuPtr->winPtr,
            CK_EV_MAP | CK_EV_EXPOSE | CK_EV_DESTROY,
	    MenuEventProc, (ClientData) menuPtr);
    if (ConfigureMenu(interp, menuPtr, argc-2, argv+2, 0) != TCL_OK) {
	goto error;
    }

    interp->result = menuPtr->winPtr->pathName;
    return TCL_OK;

    error:
    Ck_DestroyWindow(menuPtr->winPtr);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * MenuWidgetCmd --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a widget managed by this module.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

static int
MenuWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about menu widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    register Menu *menuPtr = (Menu *) clientData;
    register MenuEntry *mePtr;
    int result = TCL_OK;
    size_t length;
    int c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Ck_Preserve((ClientData) menuPtr);
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'a') && (strncmp(argv[1], "activate", length) == 0)
	    && (length >= 2)) {
	int index;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " activate index\"", (char *) NULL);
	    goto error;
	}
	if (GetMenuIndex(interp, menuPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	if (menuPtr->active == index) {
	    goto done;
	}
	if (index >= 0) {
	    if ((menuPtr->entries[index]->type == SEPARATOR_ENTRY)
		    || (menuPtr->entries[index]->state == ckDisabledUid)) {
		index = -1;
	    }
	}
	result = ActivateMenuEntry(menuPtr, index);
    } else if ((c == 'a') && (strncmp(argv[1], "add", length) == 0)
	    && (length >= 2)) {
	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " add type ?options?\"", (char *) NULL);
	    goto error;
	}
	if (MenuAddOrInsert(interp, menuPtr, (char *) NULL,
		argc-2, argv+2) != TCL_OK) {
	    goto error;
	}
    } else if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " cget option\"",
		    (char *) NULL);
	    goto error;
	}
	result = Ck_ConfigureValue(interp, menuPtr->winPtr, configSpecs,
		(char *) menuPtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 2)) {
	if (argc == 2) {
	    result = Ck_ConfigureInfo(interp, menuPtr->winPtr, configSpecs,
		    (char *) menuPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Ck_ConfigureInfo(interp, menuPtr->winPtr, configSpecs,
		    (char *) menuPtr, argv[2], 0);
	} else {
	    result = ConfigureMenu(interp, menuPtr, argc-2, argv+2,
		    CK_CONFIG_ARGV_ONLY);
	}
    } else if ((c == 'd') && (strncmp(argv[1], "delete", length) == 0)) {
	int first, last, i, numDeleted;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " delete first ?last?\"", (char *) NULL);
	    goto error;
	}
	if (GetMenuIndex(interp, menuPtr, argv[2], 0, &first) != TCL_OK) {
	    goto error;
	}
	if (argc == 3) {
	    last = first;
	} else {
	    if (GetMenuIndex(interp, menuPtr, argv[3], 0, &last) != TCL_OK) {
	        goto error;
	    }
	}
	if ((first < 0) || (last < first)) {
	    goto done;
	}
	numDeleted = last + 1 - first;
	for (i = first; i <= last; i++) {
	    Ck_EventuallyFree((ClientData) menuPtr->entries[i],
	        (Ck_FreeProc *) DestroyMenuEntry);
	}
	for (i = last+1; i < menuPtr->numEntries; i++) {
	    menuPtr->entries[i-numDeleted] = menuPtr->entries[i];
	}
	menuPtr->numEntries -= numDeleted;
	if ((menuPtr->active >= first) && (menuPtr->active <= last)) {
	    menuPtr->active = -1;
	} else if (menuPtr->active > last) {
	    menuPtr->active -= numDeleted;
	}
	if (!(menuPtr->flags & RESIZE_PENDING)) {
	    menuPtr->flags |= RESIZE_PENDING;
	    Tk_DoWhenIdle(ComputeMenuGeometry, (ClientData) menuPtr);
	}
    } else if ((c == 'e') && (length >= 7)
	    && (strncmp(argv[1], "entrycget", length) == 0)) {
	int index;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " entrycget index option\"",
		    (char *) NULL);
	    goto error;
	}
	if (GetMenuIndex(interp, menuPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	if (index < 0) {
	    goto done;
	}
	mePtr = menuPtr->entries[index];
	Ck_Preserve((ClientData) mePtr);
	result = Ck_ConfigureValue(interp, menuPtr->winPtr, entryConfigSpecs,
		(char *) mePtr, argv[3], COMMAND_MASK << mePtr->type);
	Ck_Release((ClientData) mePtr);
    } else if ((c == 'e') && (length >= 7)
	    && (strncmp(argv[1], "entryconfigure", length) == 0)) {
	int index;

	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " entryconfigure index ?option value ...?\"",
		    (char *) NULL);
	    goto error;
	}
	if (GetMenuIndex(interp, menuPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	if (index < 0) {
	    goto done;
	}
	mePtr = menuPtr->entries[index];
	Ck_Preserve((ClientData) mePtr);
	if (argc == 3) {
	    result = Ck_ConfigureInfo(interp, menuPtr->winPtr,
		    entryConfigSpecs, (char *) mePtr, (char *) NULL,
		    COMMAND_MASK << mePtr->type);
	} else if (argc == 4) {
	    result = Ck_ConfigureInfo(interp, menuPtr->winPtr,
		    entryConfigSpecs,
		    (char *) mePtr, argv[3], COMMAND_MASK << mePtr->type);
	} else {
	    result = ConfigureMenuEntry(interp, menuPtr, mePtr, index, argc-3,
		    argv+3, CK_CONFIG_ARGV_ONLY | COMMAND_MASK << mePtr->type);
	}
	Ck_Release((ClientData) mePtr);
    } else if ((c == 'i') && (strncmp(argv[1], "index", length) == 0)
	    && (length >= 3)) {
	int index;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " index string\"", (char *) NULL);
	    goto error;
	}
	if (GetMenuIndex(interp, menuPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	if (index < 0) {
	    interp->result = "none";
	} else {
	    sprintf(interp->result, "%d", index);
	}
    } else if ((c == 'i') && (strncmp(argv[1], "insert", length) == 0)
	    && (length >= 3)) {
	if (argc < 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " insert index type ?options?\"", (char *) NULL);
	    goto error;
	}
	if (MenuAddOrInsert(interp, menuPtr, argv[2],
		argc-3, argv+3) != TCL_OK) {
	    goto error;
	}
    } else if ((c == 'i') && (strncmp(argv[1], "invoke", length) == 0)
	    && (length >= 3)) {
	int index;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " invoke index\"", (char *) NULL);
	    goto error;
	}
	if (GetMenuIndex(interp, menuPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	if (index < 0) {
	    goto done;
	}
	mePtr = menuPtr->entries[index];
	if (mePtr->state == ckDisabledUid) {
	    goto done;
	}
	Ck_Preserve((ClientData) mePtr);
	if (mePtr->type == CHECK_BUTTON_ENTRY) {
	    if (mePtr->flags & ENTRY_SELECTED) {
		if (Tcl_SetVar(interp, mePtr->name, mePtr->offValue,
			TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		    result = TCL_ERROR;
		}
	    } else {
		if (Tcl_SetVar(interp, mePtr->name, mePtr->onValue,
			TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		    result = TCL_ERROR;
		}
	    }
	} else if (mePtr->type == RADIO_BUTTON_ENTRY) {
	    if (Tcl_SetVar(interp, mePtr->name, mePtr->onValue,
		    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		result = TCL_ERROR;
	    }
	}
	if ((result == TCL_OK) && (mePtr->command != NULL)) {
	    result = CkCopyAndGlobalEval(interp, mePtr->command);
	}
	if ((result == TCL_OK) && (mePtr->type == CASCADE_ENTRY)) {
	    result = PostSubmenu(menuPtr->interp, menuPtr, mePtr);
	}
	Ck_Release((ClientData) mePtr);
    } else if ((c == 'p') && (strncmp(argv[1], "post", length) == 0)
	    && (length == 4)) {
	int x, y, tmp;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " post x y\"", (char *) NULL);
	    goto error;
	}
	if ((Tcl_GetInt(interp, argv[2], &x) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[3], &y) != TCL_OK)) {
	    goto error;
	}

	/*
	 * De-activate any active element.
	 */

	ActivateMenuEntry(menuPtr, -1);

	/*
	 * If there is a command for the menu, execute it.  This
	 * may change the size of the menu, so be sure to recompute
	 * the menu's geometry if needed.
	 */

	if (menuPtr->postCommand != NULL) {
	    result = CkCopyAndGlobalEval(menuPtr->interp,
		    menuPtr->postCommand);
	    if (result != TCL_OK) {
		return result;
	    }
	    if (menuPtr->flags & RESIZE_PENDING) {
		Tk_CancelIdleCall(ComputeMenuGeometry, (ClientData) menuPtr);
		ComputeMenuGeometry((ClientData) menuPtr);
	    }
	}
	if (menuPtr->borderPtr != NULL)
	    x -= 1;
	tmp = menuPtr->winPtr->mainPtr->maxWidth - menuPtr->winPtr->reqWidth;
	if (x > tmp) {
	    x = tmp;
	}
	if (x < 0) {
	    x = 0;
	}
	tmp = menuPtr->winPtr->mainPtr->maxHeight - menuPtr->winPtr->reqHeight;
	if (y > tmp) {
	    y = tmp;
	}
	if (y < 0) {
	    y = 0;
	}
	if (x != menuPtr->winPtr->x || y != menuPtr->winPtr->y) {
	    Ck_MoveWindow(menuPtr->winPtr, x, y);
	}
        if (menuPtr->winPtr->reqWidth != menuPtr->winPtr->width ||
	    menuPtr->winPtr->reqHeight != menuPtr->winPtr->reqHeight) {
	    Ck_ResizeWindow(menuPtr->winPtr,
		 menuPtr->winPtr->reqWidth, menuPtr->winPtr->reqHeight);
	}
	if (!(menuPtr->winPtr->flags & CK_MAPPED)) {
	    Ck_MapWindow(menuPtr->winPtr);
	}
    } else if ((c == 'p') && (strncmp(argv[1], "postcascade", length) == 0)
	    && (length > 4)) {
	int index;
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " postcascade index\"", (char *) NULL);
	    goto error;
	}
	if (GetMenuIndex(interp, menuPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	if ((index < 0) || (menuPtr->entries[index]->type != CASCADE_ENTRY)) {
	    result = PostSubmenu(interp, menuPtr, (MenuEntry *) NULL);
	} else {
	    result = PostSubmenu(interp, menuPtr, menuPtr->entries[index]);
	}
    } else if ((c == 't') && (strncmp(argv[1], "type", length) == 0)) {
	int index;
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " type index\"", (char *) NULL);
	    goto error;
	}
	if (GetMenuIndex(interp, menuPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	if (index < 0) {
	    goto done;
	}
	mePtr = menuPtr->entries[index];
	switch (mePtr->type) {
	    case COMMAND_ENTRY:
		interp->result = "command";
		break;
	    case SEPARATOR_ENTRY:
		interp->result = "separator";
		break;
	    case CHECK_BUTTON_ENTRY:
		interp->result = "checkbutton";
		break;
	    case RADIO_BUTTON_ENTRY:
		interp->result = "radiobutton";
		break;
	    case CASCADE_ENTRY:
		interp->result = "cascade";
		break;
	}
    } else if ((c == 'u') && (strncmp(argv[1], "unpost", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " unpost\"", (char *) NULL);
	    goto error;
	}
	Ck_UnmapWindow(menuPtr->winPtr);
	if (result == TCL_OK) {
	    result = PostSubmenu(interp, menuPtr, (MenuEntry *) NULL);
	}
    } else if ((c == 'y') && (strncmp(argv[1], "yposition", length) == 0)) {
	int index;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " yposition index\"", (char *) NULL);
	    goto error;
	}
	if (GetMenuIndex(interp, menuPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	if (index < 0) {
	    interp->result = "0";
	} else {
	    sprintf(interp->result, "%d", menuPtr->entries[index]->y);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be activate, add, cget, configure, delete, ",
		"entrycget, entryconfigure, index, insert, invoke, ",
		"post, postcascade, type, unpost, or yposition",
		(char *) NULL);
	goto error;
    }
    done:
    Ck_Release((ClientData) menuPtr);
    return result;

    error:
    Ck_Release((ClientData) menuPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyMenu --
 *
 *	This procedure is invoked by Ck_EventuallyFree or Ck_Release
 *	to clean up the internal structure of a menu at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the menu is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyMenu(clientData)
    ClientData clientData;	/* Info about menu widget. */
{
    register Menu *menuPtr = (Menu *) clientData;
    int i;

    /*
     * Free up all the stuff that requires special handling, then
     * let Ck_FreeOptions handle all the standard option-related
     * stuff.
     */

    for (i = 0; i < menuPtr->numEntries; i++) {
	DestroyMenuEntry((ClientData) menuPtr->entries[i]);
    }
    if (menuPtr->entries != NULL) {
	ckfree((char *) menuPtr->entries);
    }
    Ck_FreeOptions(configSpecs, (char *) menuPtr, 0);
    ckfree((char *) menuPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyMenuEntry --
 *
 *	This procedure is invoked by Ck_EventuallyFree or Ck_Release
 *	to clean up the internal structure of a menu entry at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the menu entry is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyMenuEntry(clientData)
    ClientData clientData;		/* Pointer to entry to be freed. */
{
    register MenuEntry *mePtr = (MenuEntry *) clientData;
    Menu *menuPtr = mePtr->menuPtr;

    /*
     * Free up all the stuff that requires special handling, then
     * let Ck_FreeOptions handle all the standard option-related
     * stuff.
     */

    if (menuPtr->postedCascade == mePtr) {
	/*
	 * Ignore errors while unposting the menu, since it's possible
	 * that the menu has already been deleted and the unpost will
	 * generate an error.
	 */

	PostSubmenu(menuPtr->interp, menuPtr, (MenuEntry *) NULL);
    }
    if (mePtr->name != NULL) {
	Tcl_UntraceVar(menuPtr->interp, mePtr->name,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MenuVarProc, (ClientData) mePtr);
    }
    Ck_FreeOptions(entryConfigSpecs, (char *) mePtr,
        (COMMAND_MASK << mePtr->type));
    ckfree((char *) mePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureMenu --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the option database, in order to configure (or reconfigure)
 *      a menu widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as colors, font, etc. get set
 *	for menuPtr;  old resources get freed, if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureMenu(interp, menuPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    register Menu *menuPtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    int i;

    if (Ck_ConfigureWidget(interp, menuPtr->winPtr, configSpecs,
	    argc, argv, (char *) menuPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * After reconfiguring a menu, we need to reconfigure all of the
     * entries in the menu, since some of the things in the children
     * (such as graphics contexts) may have to change to reflect changes
     * in the parent.
     */

    for (i = 0; i < menuPtr->numEntries; i++) {
	MenuEntry *mePtr;

	mePtr = menuPtr->entries[i];
	ConfigureMenuEntry(interp, menuPtr, mePtr, i, 0, (char **) NULL,
		CK_CONFIG_ARGV_ONLY | COMMAND_MASK << mePtr->type);
    }

    Ck_SetInternalBorder(menuPtr->winPtr, menuPtr->borderPtr != NULL);

    if (!(menuPtr->flags & RESIZE_PENDING)) {
	menuPtr->flags |= RESIZE_PENDING;
	Tk_DoWhenIdle(ComputeMenuGeometry, (ClientData) menuPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureMenuEntry --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the option database, in order to configure (or reconfigure)
 *      one entry in a menu.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information such as label and accelerator get
 *	set for mePtr;  old resources get freed, if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureMenuEntry(interp, menuPtr, mePtr, index, argc, argv, flags)
    Tcl_Interp *interp;			/* Used for error reporting. */
    Menu *menuPtr;			/* Information about whole menu. */
    register MenuEntry *mePtr;		/* Information about menu entry;  may
					 * or may not already have values for
					 * some fields. */
    int index;				/* Index of mePtr within menuPtr's
					 * entries. */
    int argc;				/* Number of valid entries in argv. */
    char **argv;			/* Arguments. */
    int flags;				/* Additional flags to pass to
					 * Tk_ConfigureWidget. */
{
    /*
     * If this entry is a cascade and the cascade is posted, then unpost
     * it before reconfiguring the entry (otherwise the reconfigure might
     * change the name of the cascaded entry, leaving a posted menu
     * high and dry).
     */

    if (menuPtr->postedCascade == mePtr) {
	if (PostSubmenu(menuPtr->interp, menuPtr, (MenuEntry *) NULL)
		!= TCL_OK) {
	    Tk_BackgroundError(menuPtr->interp);
	}
    }

    /*
     * If this entry is a check button or radio button, then remove
     * its old trace procedure.
     */

    if ((mePtr->name != NULL) &&
	    ((mePtr->type == CHECK_BUTTON_ENTRY)
	    || (mePtr->type == RADIO_BUTTON_ENTRY))) {
	Tcl_UntraceVar(menuPtr->interp, mePtr->name,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MenuVarProc, (ClientData) mePtr);
    }

    if (Ck_ConfigureWidget(interp, menuPtr->winPtr, entryConfigSpecs,
	    argc, argv, (char *) mePtr,
	    flags | (COMMAND_MASK << mePtr->type)) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * The code below handles special configuration stuff not taken
     * care of by Ck_ConfigureWidget, such as special processing for
     * defaults, sizing strings, graphics contexts, etc.
     */

    if (mePtr->label == NULL) {
	mePtr->labelLength = 0;
    } else {
	mePtr->labelLength = strlen(mePtr->label);
    }
    if (mePtr->accel == NULL) {
	mePtr->accelLength = 0;
    } else {
	mePtr->accelLength = strlen(mePtr->accel);
    }

    if (mePtr->state == ckActiveUid) {
	if (index != menuPtr->active) {
	    ActivateMenuEntry(menuPtr, index);
	}
    } else {
	if (index == menuPtr->active) {
	    ActivateMenuEntry(menuPtr, -1);
	}
	if ((mePtr->state != ckNormalUid) && (mePtr->state != ckDisabledUid)) {
	    Tcl_AppendResult(interp, "bad state value \"", mePtr->state,
		    "\":  must be normal, active, or disabled", (char *) NULL);
	    mePtr->state = ckNormalUid;
	    return TCL_ERROR;
	}
    }

    if ((mePtr->type == CHECK_BUTTON_ENTRY)
	    || (mePtr->type == RADIO_BUTTON_ENTRY)) {
	char *value;

	if (mePtr->name == NULL) {
	    mePtr->name = (char *) ckalloc(mePtr->labelLength + 1);
	    strcpy(mePtr->name, (mePtr->label == NULL) ? "" : mePtr->label);
	}
	if (mePtr->onValue == NULL) {
	    mePtr->onValue = (char *) ckalloc(mePtr->labelLength + 1);
	    strcpy(mePtr->onValue, (mePtr->label == NULL) ? "" : mePtr->label);
	}

	/*
	 * Select the entry if the associated variable has the
	 * appropriate value, initialize the variable if it doesn't
	 * exist, then set a trace on the variable to monitor future
	 * changes to its value.
	 */

	value = Tcl_GetVar(interp, mePtr->name, TCL_GLOBAL_ONLY);
	mePtr->flags &= ~ENTRY_SELECTED;
	if (value != NULL) {
	    if (strcmp(value, mePtr->onValue) == 0) {
		mePtr->flags |= ENTRY_SELECTED;
	    }
	} else {
	    Tcl_SetVar(interp, mePtr->name,
		    (mePtr->type == CHECK_BUTTON_ENTRY) ? mePtr->offValue : "",
		    TCL_GLOBAL_ONLY);
	}
	Tcl_TraceVar(interp, mePtr->name,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MenuVarProc, (ClientData) mePtr);
    }

    if (!(menuPtr->flags & RESIZE_PENDING)) {
	menuPtr->flags |= RESIZE_PENDING;
	Tk_DoWhenIdle(ComputeMenuGeometry, (ClientData) menuPtr);
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ComputeMenuGeometry --
 *
 *	This procedure is invoked to recompute the size and
 *	layout of a menu.  It is called as a when-idle handler so
 *	that it only gets done once, even if a group of changes is
 *	made to the menu.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Fields of menu entries are changed to reflect their
 *	current positions, and the size of the menu window
 *	itself may be changed.
 *
 *--------------------------------------------------------------
 */

static void
ComputeMenuGeometry(clientData)
    ClientData clientData;		/* Structure describing menu. */
{
    Menu *menuPtr = (Menu *) clientData;
    CkWindow *winPtr = menuPtr->winPtr;
    register MenuEntry *mePtr;
    int maxLabelWidth, maxIndicatorWidth, maxAccelWidth;
    int width, height, indicatorSpace, dummy;
    int i, y;

    if (menuPtr->winPtr == NULL) {
	return;
    }

    maxLabelWidth = maxIndicatorWidth = maxAccelWidth = 0;
    y = 0;

    for (i = 0; i < menuPtr->numEntries; i++) {
	mePtr = menuPtr->entries[i];
	indicatorSpace = 0;

	if (mePtr->label != NULL) {
	    CkMeasureChars(winPtr->mainPtr,
	        mePtr->label, mePtr->labelLength, 0,
		100000, 0, CK_NEWLINES_NOT_SPECIAL, &width, &dummy);
	} else {
	    width = 0;
	}
	if (mePtr->indicatorOn && (mePtr->type == CHECK_BUTTON_ENTRY ||
	    mePtr->type == RADIO_BUTTON_ENTRY)) {
	    indicatorSpace = 4;
	}
	if (width > maxLabelWidth) {
	    maxLabelWidth = width;
	}
	if (mePtr->type == CASCADE_ENTRY) {
	    width = 2;
	} else if (mePtr->accel != NULL) {
	    CkMeasureChars(winPtr->mainPtr,
		mePtr->accel, mePtr->accelLength, 0,
		100000, 0, CK_NEWLINES_NOT_SPECIAL, &width, &dummy);
	} else {
	    width = 0;
	}
	if (width > maxAccelWidth) {
	    maxAccelWidth = width;
	}
	if (indicatorSpace > maxIndicatorWidth) {
	    maxIndicatorWidth = indicatorSpace;
	}
	mePtr->y = y;
	y++;
    }

    /*
     * Got all the sizes.  Update fields in the menu structure, then
     * resize the window if necessary.  Leave margins on either side
     * of the indicator (or just one margin if there is no indicator).
     * Leave another margin on the right side of the label, plus yet
     * another margin to the right of the accelerator (if there is one).
     */

    menuPtr->indicatorSpace = maxIndicatorWidth;
    menuPtr->labelWidth = maxLabelWidth;
    width = menuPtr->indicatorSpace + menuPtr->labelWidth + maxAccelWidth;
    height = y;

    if (width <= 0) {
	width = 1;
    }
    if (height <= 0) {
	height = 1;
    }

    if (menuPtr->borderPtr != NULL) {
	width += 2;
	height += 2;
    }
    if (width != menuPtr->winPtr->reqWidth ||
	height != menuPtr->winPtr->reqHeight) {
	Ck_GeometryRequest(menuPtr->winPtr, width, height);
    } else {
	/*
	 * Must always force a redisplay here if the window is mapped
	 * (even if the size didn't change, something else might have
	 * changed in the menu, such as a label or accelerator).  The
	 * resize will force a redisplay above.
	 */

	EventuallyRedrawMenu(menuPtr, (MenuEntry *) NULL);
    }
    menuPtr->flags &= ~RESIZE_PENDING;
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayMenu --
 *
 *	This procedure is invoked to display a menu widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to display the menu in its
 *	current mode.
 *
 *----------------------------------------------------------------------
 */

static void
DisplayMenu(clientData)
    ClientData clientData;	/* Information about widget. */
{
    register Menu *menuPtr = (Menu *) clientData;
    register MenuEntry *mePtr;
    register CkWindow *winPtr = menuPtr->winPtr;
    int index, leftEdge, x, y, cursorX, cursorY;
    int fg, nFg, aFg, dFg;
    int bg, nBg, aBg, dBg;
    int attr, nAt, aAt, dAt;

    menuPtr->flags &= ~REDRAW_PENDING;
    if (menuPtr->winPtr == NULL || !(winPtr->flags & CK_MAPPED))
	return;

    x = cursorX = menuPtr->borderPtr != NULL ? 1 : 0;
    y = cursorY = menuPtr->borderPtr != NULL ? 1 : 0;

    /*
     * Loop through all of the entries, drawing them one at a time.
     */

    leftEdge = menuPtr->indicatorSpace + x;

    for (index = 0; index < menuPtr->numEntries; index++, y++) {
	mePtr = menuPtr->entries[index];
	if (mePtr->state == ckActiveUid) {
	    cursorY = y;
	    if (mePtr->type == CASCADE_ENTRY)
		cursorX = winPtr->width - x - 1;
	    else if (mePtr->type == CHECK_BUTTON_ENTRY ||
		mePtr->type == RADIO_BUTTON_ENTRY)
		cursorX = x + 1;
	}
	if (!(mePtr->flags & ENTRY_NEEDS_REDISPLAY)) {
	    continue;
	}
	mePtr->flags &= ~ENTRY_NEEDS_REDISPLAY;

	/*
	 * Colors.
	 */

        nBg = mePtr->normalBg < 0 ? menuPtr->normalBg : mePtr->normalBg;
        aBg = mePtr->activeBg < 0 ? menuPtr->activeBg : mePtr->activeBg;
        dBg = mePtr->disabledBg < 0 ? menuPtr->disabledBg : mePtr->disabledBg;
        nFg = mePtr->normalFg < 0 ? menuPtr->normalFg : mePtr->normalFg;
        aFg = mePtr->activeFg < 0 ? menuPtr->activeFg : mePtr->activeFg;
        dFg = mePtr->disabledFg < 0 ? menuPtr->disabledFg : mePtr->disabledFg;
        nAt = mePtr->normalAttr < 0 ? menuPtr->normalAttr : mePtr->normalAttr;
        aAt = mePtr->activeAttr < 0 ? menuPtr->activeAttr : mePtr->activeAttr;
        dAt = mePtr->disabledAttr < 0 ? menuPtr->disabledAttr : 
            mePtr->disabledAttr;

	if (mePtr->state == ckActiveUid) {
	    bg = aBg; fg = aFg; attr = aAt;
	} else if (mePtr->state == ckDisabledUid) {
	    bg = dBg; fg = dFg; attr = dAt;
	} else {
	    bg = nBg; fg = nFg; attr = nAt;
	}

	Ck_SetWindowAttr(winPtr, fg, bg, attr);
	Ck_ClearToEol(winPtr, x, y);

	if (mePtr->label != NULL) {
	    CkDisplayChars(winPtr->mainPtr, winPtr->window,
		mePtr->label, mePtr->labelLength,
		leftEdge, y, leftEdge,
		CK_NEWLINES_NOT_SPECIAL | CK_IGNORE_TABS);
	    if (mePtr->underline >= 0 && mePtr->state == ckNormalUid) {
		Ck_SetWindowAttr(winPtr, mePtr->underlineFg < 0 ?
		    menuPtr->underlineFg : mePtr->underlineFg, bg,
		    mePtr->underlineAttr < 0 ? menuPtr->underlineAttr :
		    mePtr->underlineAttr);
		CkUnderlineChars(winPtr->mainPtr, winPtr->window, mePtr->label,
		    mePtr->labelLength, leftEdge, y, leftEdge,
		    CK_NEWLINES_NOT_SPECIAL | CK_IGNORE_TABS,
		    mePtr->underline, mePtr->underline);
		Ck_SetWindowAttr(winPtr, fg, bg, attr);
	    }
	}

	/*
	 * Draw accelerator or cascade arrow.
	 */

	if (mePtr->type == CASCADE_ENTRY) {
	    long gchar;

	    Ck_GetGChar(menuPtr->interp, "rarrow", &gchar);
	    mvwaddch(winPtr->window, y, winPtr->width - x - 1, gchar);
	} else if (mePtr->accel != NULL) {
	    CkDisplayChars(winPtr->mainPtr, winPtr->window,
		mePtr->accel, mePtr->accelLength,
		leftEdge + menuPtr->labelWidth, y,
	        leftEdge + menuPtr->labelWidth,
		CK_NEWLINES_NOT_SPECIAL | CK_IGNORE_TABS);
	}

	/*
	 * Draw check-button/radio-button indicators.
	 */

	if (mePtr->indicatorOn && (mePtr->type == CHECK_BUTTON_ENTRY ||
	    mePtr->type == RADIO_BUTTON_ENTRY)) {
	    wmove(winPtr->window, y, x);
	    Ck_SetWindowAttr(winPtr, nFg, nBg, nAt);
	    waddstr(winPtr->window, mePtr->type == CHECK_BUTTON_ENTRY ?
		"[ ]" : "( )");
	    if (mePtr->flags & ENTRY_SELECTED) {
		long gchar;

		Ck_GetGChar(menuPtr->interp,
		    mePtr->type == CHECK_BUTTON_ENTRY ? "diamond" : "bullet",
		    &gchar);
		Ck_SetWindowAttr(winPtr, mePtr->indicatorFg < 0 ?
		    menuPtr->indicatorFg : mePtr->indicatorFg, nBg, nAt);
		mvwaddch(winPtr->window, y, x + 1, gchar);
	    }
	}

	/*
	 * Draw separator.
	 */

	if (mePtr->type == SEPARATOR_ENTRY) {
	    int i;
	    long gchar;

	    wmove(winPtr->window, y, x);
	    Ck_SetWindowAttr(winPtr, nFg, nBg, nAt);
	    Ck_GetGChar(menuPtr->interp, "hline", &gchar);
	    for (i = x; i < winPtr->width - x; i++)
		waddch(winPtr->window, gchar);
	}
    }
    if (menuPtr->borderPtr != NULL) {
	Ck_SetWindowAttr(winPtr, menuPtr->normalFg, menuPtr->normalBg,
	    menuPtr->normalAttr);
        Ck_DrawBorder(winPtr, menuPtr->borderPtr, 0, 0,
	    winPtr->width, winPtr->height);
    }
    wmove(winPtr->window, cursorY, cursorX);
    Ck_EventuallyRefresh(winPtr);
}

/*
 *--------------------------------------------------------------
 *
 * GetMenuIndex --
 *
 *	Parse a textual index into a menu and return the numerical
 *	index of the indicated entry.
 *
 * Results:
 *	A standard Tcl result.  If all went well, then *indexPtr is
 *	filled in with the entry index corresponding to string
 *	(ranges from -1 to the number of entries in the menu minus
 *	one).  Otherwise an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
GetMenuIndex(interp, menuPtr, string, lastOK, indexPtr)
    Tcl_Interp *interp;		/* For error messages. */
    Menu *menuPtr;		/* Menu for which the index is being
				 * specified. */
    char *string;		/* Specification of an entry in menu.  See
				 * manual entry for valid .*/
    int lastOK;			/* Non-zero means its OK to return index
				 * just *after* last entry. */
    int *indexPtr;		/* Where to store converted relief. */
{
    int i;

    if ((string[0] == 'a') && (strcmp(string, "active") == 0)) {
	*indexPtr = menuPtr->active;
	return TCL_OK;
    }

    if (((string[0] == 'l') && (strcmp(string, "last") == 0))
	    || ((string[0] == 'e') && (strcmp(string, "end") == 0))) {
	*indexPtr = menuPtr->numEntries - ((lastOK) ? 0 : 1);
	return TCL_OK;
    }

    if ((string[0] == 'n') && (strcmp(string, "none") == 0)) {
	*indexPtr = -1;
	return TCL_OK;
    }

    if (string[0] == '@') {
	if (Tcl_GetInt(interp, string+1, &i) == TCL_OK) {
	    if (menuPtr->borderPtr != NULL)
		i -= 1;
	    if (i >= menuPtr->numEntries)
		i = -1;
	    if (i < 0)
		i = -1;
	    *indexPtr = i;
	    return TCL_OK;
	} else {
	    Tcl_SetResult(interp, (char *) NULL, TCL_STATIC);
	}
    }

    if (isdigit((unsigned char) string[0])) {
	if (Tcl_GetInt(interp, string,  &i) == TCL_OK) {
	    if (i >= menuPtr->numEntries) {
		if (lastOK) {
		    i = menuPtr->numEntries;
		} else {
		    i = menuPtr->numEntries-1;
		}
	    } else if (i < 0) {
		i = -1;
	    }
	    *indexPtr = i;
	    return TCL_OK;
	}
	Tcl_SetResult(interp, (char *) NULL, TCL_STATIC);
    }

    for (i = 0; i < menuPtr->numEntries; i++) {
	char *label;

	label = menuPtr->entries[i]->label;
	if ((label != NULL)
		&& (Tcl_StringMatch(menuPtr->entries[i]->label, string))) {
	    *indexPtr = i;
	    return TCL_OK;
	}
    }

    Tcl_AppendResult(interp, "bad menu entry index \"",
	    string, "\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * MenuEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on menus.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets exposed, it is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
MenuEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    CkEvent *eventPtr;		/* Information about event. */
{
    Menu *menuPtr = (Menu *) clientData;
    if (eventPtr->type == CK_EV_EXPOSE || eventPtr->type == CK_EV_MAP) {
	EventuallyRedrawMenu(menuPtr, (MenuEntry *) NULL);
    } else if (eventPtr->type == CK_EV_DESTROY) {
	if (menuPtr->winPtr != NULL) {
	    menuPtr->winPtr = NULL;
	    Tcl_DeleteCommand(menuPtr->interp,
		    Tcl_GetCommandName(menuPtr->interp, menuPtr->widgetCmd));
	}
	if (menuPtr->flags & REDRAW_PENDING) {
	    Tk_CancelIdleCall(DisplayMenu, (ClientData) menuPtr);
	}
	if (menuPtr->flags & RESIZE_PENDING) {
	    Tk_CancelIdleCall(ComputeMenuGeometry, (ClientData) menuPtr);
	}
	Ck_EventuallyFree((ClientData) menuPtr, (Ck_FreeProc *) DestroyMenu);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MenuCmdDeletedProc --
 *
 *	This procedure is invoked when a widget command is deleted.  If
 *	the widget isn't already in the process of being destroyed,
 *	this command destroys it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget is destroyed.
 *
 *----------------------------------------------------------------------
 */

static void
MenuCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    Menu *menuPtr = (Menu *) clientData;
    CkWindow *winPtr = menuPtr->winPtr;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (winPtr != NULL) {
	menuPtr->winPtr = NULL;
	Ck_DestroyWindow(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MenuNewEntry --
 *
 *	This procedure allocates and initializes a new menu entry.
 *
 * Results:
 *	The return value is a pointer to a new menu entry structure,
 *	which has been malloc-ed, initialized, and entered into the
 *	entry array for the  menu.
 *
 * Side effects:
 *	Storage gets allocated.
 *
 *----------------------------------------------------------------------
 */

static MenuEntry *
MenuNewEntry(menuPtr, index, type)
    Menu *menuPtr;		/* Menu that will hold the new entry. */
    int index;			/* Where in the menu the new entry is to
				 * go. */
    int type;			/* The type of the new entry. */
{
    MenuEntry *mePtr;
    MenuEntry **newEntries;
    int i;

    /*
     * Create a new array of entries with an empty slot for the
     * new entry.
     */

    newEntries = (MenuEntry **) ckalloc((unsigned)
	    ((menuPtr->numEntries+1)*sizeof(MenuEntry *)));
    for (i = 0; i < index; i++) {
	newEntries[i] = menuPtr->entries[i];
    }
    for (  ; i < menuPtr->numEntries; i++) {
	newEntries[i+1] = menuPtr->entries[i];
    }
    if (menuPtr->numEntries != 0) {
	ckfree((char *) menuPtr->entries);
    }
    menuPtr->entries = newEntries;
    menuPtr->numEntries++;
    menuPtr->entries[index] = mePtr = (MenuEntry *) ckalloc(sizeof(MenuEntry));
    mePtr->type = type;
    mePtr->menuPtr = menuPtr;
    mePtr->label = NULL;
    mePtr->labelLength = 0;
    mePtr->underline = -1;
    mePtr->accel = NULL;
    mePtr->accelLength = 0;
    mePtr->state = ckNormalUid;
    mePtr->y = 0;
    mePtr->indicatorOn = 1;
    mePtr->normalBg = -1;
    mePtr->normalFg = -1;
    mePtr->normalAttr = -1;
    mePtr->activeBg = -1;
    mePtr->activeFg = -1;
    mePtr->activeAttr = -1;
    mePtr->disabledBg = -1;
    mePtr->disabledFg = -1;
    mePtr->disabledAttr = -1;
    mePtr->underlineFg = -1;
    mePtr->underlineAttr = -1;
    mePtr->indicatorFg = -1;
    mePtr->command = NULL;
    mePtr->name = NULL;
    mePtr->onValue = NULL;
    mePtr->offValue = NULL;
    mePtr->flags = 0;
    return mePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * MenuAddOrInsert --
 *
 *	This procedure does all of the work of the "add" and "insert"
 *	widget commands, allowing the code for these to be shared.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *	A new menu entry is created in menuPtr.
 *
 *----------------------------------------------------------------------
 */

static int
MenuAddOrInsert(interp, menuPtr, indexString, argc, argv)
    Tcl_Interp *interp;			/* Used for error reporting. */
    Menu *menuPtr;			/* Widget in which to create new
					 * entry. */
    char *indexString;			/* String describing index at which
					 * to insert.  NULL means insert at
					 * end. */
    int argc;				/* Number of elements in argv. */
    char **argv;			/* Arguments to command:  first arg
					 * is type of entry, others are
					 * config options. */
{
    int c, type, i, index;
    size_t length;
    MenuEntry *mePtr;

    if (indexString != NULL) {
	if (GetMenuIndex(interp, menuPtr, indexString, 1, &index) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	index = menuPtr->numEntries;
    }
    if (index < 0) {
	Tcl_AppendResult(interp, "bad index \"", indexString, "\"",
		 (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Figure out the type of the new entry.
     */

    c = argv[0][0];
    length = strlen(argv[0]);
    if ((c == 'c') && (strncmp(argv[0], "cascade", length) == 0)
	    && (length >= 2)) {
	type = CASCADE_ENTRY;
    } else if ((c == 'c') && (strncmp(argv[0], "checkbutton", length) == 0)
	    && (length >= 2)) {
	type = CHECK_BUTTON_ENTRY;
    } else if ((c == 'c') && (strncmp(argv[0], "command", length) == 0)
	    && (length >= 2)) {
	type = COMMAND_ENTRY;
    } else if ((c == 'r')
	    && (strncmp(argv[0], "radiobutton", length) == 0)) {
	type = RADIO_BUTTON_ENTRY;
    } else if ((c == 's')
	    && (strncmp(argv[0], "separator", length) == 0)) {
	type = SEPARATOR_ENTRY;
    } else {
	Tcl_AppendResult(interp, "bad menu entry type \"",
		argv[0], "\":  must be cascade, checkbutton, ",
		"command, radiobutton, or separator", (char *) NULL);
	return TCL_ERROR;
    }
    mePtr = MenuNewEntry(menuPtr, index, type);
    if (ConfigureMenuEntry(interp, menuPtr, mePtr, index,
	    argc-1, argv+1, 0) != TCL_OK) {
	DestroyMenuEntry((ClientData) mePtr);
	for (i = index+1; i < menuPtr->numEntries; i++) {
	    menuPtr->entries[i-1] = menuPtr->entries[i];
	}
	menuPtr->numEntries--;
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * MenuVarProc --
 *
 *	This procedure is invoked when someone changes the
 *	state variable associated with a radiobutton or checkbutton
 *	menu entry.  The entry's selected state is set to match
 *	the value of the variable.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The menu entry may become selected or deselected.
 *
 *--------------------------------------------------------------
 */

static char *
MenuVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about menu entry. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* First part of variable's name. */
    char *name2;		/* Second part of variable's name. */
    int flags;			/* Describes what just happened. */
{
    MenuEntry *mePtr = (MenuEntry *) clientData;
    Menu *menuPtr;
    char *value;

    menuPtr = mePtr->menuPtr;

    /*
     * If the variable is being unset, then re-establish the
     * trace unless the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	mePtr->flags &= ~ENTRY_SELECTED;
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    Tcl_TraceVar(interp, mePtr->name,
		    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    MenuVarProc, clientData);
	}
	EventuallyRedrawMenu(menuPtr, (MenuEntry *) NULL);
	return (char *) NULL;
    }

    /*
     * Use the value of the variable to update the selected status of
     * the menu entry.
     */

    value = Tcl_GetVar(interp, mePtr->name, TCL_GLOBAL_ONLY);
    if (value == NULL) {
	value = "";
    }
    if (strcmp(value, mePtr->onValue) == 0) {
	if (mePtr->flags & ENTRY_SELECTED) {
	    return (char *) NULL;
	}
	mePtr->flags |= ENTRY_SELECTED;
    } else if (mePtr->flags & ENTRY_SELECTED) {
	mePtr->flags &= ~ENTRY_SELECTED;
    } else {
	return (char *) NULL;
    }
    EventuallyRedrawMenu(menuPtr, (MenuEntry *) NULL);
    return (char *) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * EventuallyRedrawMenu --
 *
 *	Arrange for an entry of a menu, or the whole menu, to be
 *	redisplayed at some point in the future.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A when-idle hander is scheduled to do the redisplay, if there
 *	isn't one already scheduled.
 *
 *----------------------------------------------------------------------
 */

static void
EventuallyRedrawMenu(menuPtr, mePtr)
    register Menu *menuPtr;	/* Information about menu to redraw. */
    register MenuEntry *mePtr;	/* Entry to redraw.  NULL means redraw
				 * all the entries in the menu. */
{
    int i;

    if (menuPtr->winPtr == NULL) {
	return;
    }
    if (mePtr != NULL) {
	mePtr->flags |= ENTRY_NEEDS_REDISPLAY;
    } else {
	for (i = 0; i < menuPtr->numEntries; i++) {
	    menuPtr->entries[i]->flags |= ENTRY_NEEDS_REDISPLAY;
	}
    }
    if ((menuPtr->winPtr == NULL) || !(menuPtr->winPtr->flags & CK_MAPPED)
	    || (menuPtr->flags & REDRAW_PENDING)) {
	return;
    }
    Tk_DoWhenIdle(DisplayMenu, (ClientData) menuPtr);
    menuPtr->flags |= REDRAW_PENDING;
}

/*
 *--------------------------------------------------------------
 *
 * PostSubmenu --
 *
 *	This procedure arranges for a particular submenu (i.e. the
 *	menu corresponding to a given cascade entry) to be
 *	posted.
 *
 * Results:
 *	A standard Tcl return result.  Errors may occur in the
 *	Tcl commands generated to post and unpost submenus.
 *
 * Side effects:
 *	If there is already a submenu posted, it is unposted.
 *	The new submenu is then posted.
 *
 *--------------------------------------------------------------
 */

static int
PostSubmenu(interp, menuPtr, mePtr)
    Tcl_Interp *interp;		/* Used for invoking sub-commands and
				 * reporting errors. */
    register Menu *menuPtr;	/* Information about menu as a whole. */
    register MenuEntry *mePtr;	/* Info about submenu that is to be
				 * posted.  NULL means make sure that
				 * no submenu is posted. */
{
    char string[30];
    int result, x, y;
    CkWindow *winPtr;

    if (mePtr == menuPtr->postedCascade) {
	return TCL_OK;
    }

    if (menuPtr->postedCascade != NULL) {
	/*
	 * Note: when unposting a submenu, we have to redraw the entire
	 * parent menu.  This is because of a combination of the following
	 * things:
	 * (a) the submenu partially overlaps the parent.
	 * (b) the submenu specifies "save under", which causes the X
	 *     server to make a copy of the information under it when it
	 *     is posted.  When the submenu is unposted, the X server
	 *     copies this data back and doesn't generate any Expose
	 *     events for the parent.
	 * (c) the parent may have redisplayed itself after the submenu
	 *     was posted, in which case the saved information is no
	 *     longer correct.
	 * The simplest solution is just force a complete redisplay of
	 * the parent.
	 */

	EventuallyRedrawMenu(menuPtr, (MenuEntry *) NULL);
	result = Tcl_VarEval(interp, menuPtr->postedCascade->name,
		" unpost", (char *) NULL);
	menuPtr->postedCascade = NULL;
	if (result != TCL_OK) {
	    return result;
	}
    }

    if ((mePtr != NULL) && (mePtr->name != NULL)
	    && (menuPtr->winPtr->flags & CK_MAPPED)) {
	/*
	 * Make sure that the cascaded submenu is a child of the
	 * parent menu.
	 */

	winPtr = Ck_NameToWindow(interp, mePtr->name, menuPtr->winPtr);
	if (winPtr == NULL) {
	    return TCL_ERROR;
	}
	if (winPtr->parentPtr != menuPtr->winPtr) {
	    Tcl_AppendResult(interp, "cascaded sub-menu ",
		    winPtr->pathName, " must be a child of ",
		    menuPtr->winPtr->pathName, (char *) NULL);
	    return TCL_ERROR;
	}

	/*
	 * Position the cascade with its upper left corner slightly
	 * below and to the left of the upper right corner of the
	 * menu entry (this is an attempt to match Motif behavior).
	 */
	x = menuPtr->winPtr->x;
	y = menuPtr->winPtr->y;
	x += menuPtr->winPtr->width;
	y += mePtr->y;
	sprintf(string, "%d %d", x, y);
	result = Tcl_VarEval(interp, mePtr->name, " post ", string,
		(char *) NULL);
	if (result != TCL_OK) {
	    return result;
	}
	menuPtr->postedCascade = mePtr;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ActivateMenuEntry --
 *
 *	This procedure is invoked to make a particular menu entry
 *	the active one, deactivating any other entry that might
 *	currently be active.
 *
 * Results:
 *	The return value is a standard Tcl result (errors can occur
 *	while posting and unposting submenus).
 *
 * Side effects:
 *	Menu entries get redisplayed, and the active entry changes.
 *	Submenus may get posted and unposted.
 *
 *----------------------------------------------------------------------
 */

static int
ActivateMenuEntry(menuPtr, index)
    register Menu *menuPtr;		/* Menu in which to activate. */
    int index;				/* Index of entry to activate, or
					 * -1 to deactivate all entries. */
{
    register MenuEntry *mePtr;
    int result = TCL_OK;

    if (menuPtr->active >= 0) {
	mePtr = menuPtr->entries[menuPtr->active];

	/*
	 * Don't change the state unless it's currently active (state
	 * might already have been changed to disabled).
	 */

	if (mePtr->state == ckActiveUid) {
	    mePtr->state = ckNormalUid;
	}
	EventuallyRedrawMenu(menuPtr, menuPtr->entries[menuPtr->active]);
    }
    menuPtr->active = index;
    if (index >= 0) {
	mePtr = menuPtr->entries[index];
	mePtr->state = ckActiveUid;
	EventuallyRedrawMenu(menuPtr, mePtr);
    }
    return result;
}
