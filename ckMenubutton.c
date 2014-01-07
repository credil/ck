/* 
 * ckMenubutton.c --
 *
 *	This module implements button-like widgets that are used
 *	to invoke pull-down menus.
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

/*
 * A data structure of the following type is kept for each
 * widget managed by this file:
 */

typedef struct {
    CkWindow *winPtr;		/* Window that embodies the widget.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Tcl_Interp *interp;		/* Interpreter associated with menubutton. */
    Tcl_Command widgetCmd;	/* Token for menubutton's widget command. */
    char *menuName;		/* Name of menu associated with widget.
				 * Malloc-ed. */

    /*
     * Information about what's displayed in the menu button:
     */

    char *text;			/* Text to display in button (malloc'ed)
				 * or NULL. */
    int numChars;		/* # of characters in text. */
    char *textVarName;		/* Name of variable (malloc'ed) or NULL.
				 * If non-NULL, button displays the contents
				 * of this variable. */

    /*
     * Information used when displaying widget:
     */

    Ck_Uid state;		/* State of button for display purposes:
				 * normal, active, or disabled. */
    int normalFg;		/* Foreground color in normal mode. */
    int normalBg;               /* Background color in normal mode. */
    int normalAttr;             /* Attributes in normal mode. */
    int activeFg;		/* Foreground color in active mode. */
    int activeBg;               /* Ditto, background color. */
    int activeAttr;		/* Attributes in active mode. */
    int disabledBg;             /* Background color when disabled. */
    int disabledFg;		/* Foreground color when disabled. */
    int disabledAttr;           /* Attributes when disabled. */
    int underlineFg;            /* Foreground color for underlined char. */
    int underlineAttr;          /* Attribute for underlined character. */
    int indicatorFg;		/* Foreground color for indicator. */
    int underline;              /* Index of underlined character, < 0 if
                                 * no underlining. */
    int width, height;		/* If > 0, these specify dimensions to request
				 * for window, in characters for text and in
				 * pixels for bitmaps.  In this case the actual
				 * size of the text string or bitmap is
				 * ignored in computing desired window size. */
    Ck_Anchor anchor;		/* Where text/bitmap should be displayed
				 * inside window region. */
    int indicatorOn;		/* Non-zero means display indicator;  0 means
				 * don't display. */

    /*
     * Miscellaneous information:
     */

    char *takeFocus;		/* Value of -takefocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
    int flags;			/* Various flags;  see below for
				 * definitions. */
} MenuButton;

/*
 * Flag bits for buttons:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 * POSTED:			Non-zero means that the menu associated
 *				with this button has been posted (typically
 *				because of an active button press).
 * GOT_FOCUS:			Non-zero means this button currently
 *				has the input focus.
 */

#define REDRAW_PENDING		1
#define POSTED			2
#define GOT_FOCUS		4

/*
 * Information used for parsing configuration specs:
 */

static Ck_ConfigSpec configSpecs[] = {
    {CK_CONFIG_ATTR, "-activeattributes", "activeAttributes",
        "ActiveAttributes", DEF_MENUBUTTON_ACTIVE_ATTR_COLOR,
        Ck_Offset(MenuButton, activeAttr), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_ATTR, "-activeattributes", "activeAttributes",
        "ActiveAttributes", DEF_MENUBUTTON_ACTIVE_ATTR_MONO,
        Ck_Offset(MenuButton, activeAttr), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_ATTR, "-attributes", "attributes", "Attributes",
	DEF_MENUBUTTON_ATTR, Ck_Offset(MenuButton, normalAttr),	0},
    {CK_CONFIG_COLOR, "-activebackground", "activeBackground", "Foreground",
	DEF_MENUBUTTON_ACTIVE_BG_COLOR, Ck_Offset(MenuButton, activeBg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-activebackground", "activeBackground", "Foreground",
	DEF_MENUBUTTON_ACTIVE_BG_MONO, Ck_Offset(MenuButton, activeBg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_MENUBUTTON_ACTIVE_FG_COLOR, Ck_Offset(MenuButton, activeFg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_MENUBUTTON_ACTIVE_FG_MONO, Ck_Offset(MenuButton, activeFg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_ANCHOR, "-anchor", "anchor", "Anchor",
	DEF_MENUBUTTON_ANCHOR, Ck_Offset(MenuButton, anchor), 0},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_MENUBUTTON_BG_COLOR, Ck_Offset(MenuButton, normalBg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_MENUBUTTON_BG_MONO, Ck_Offset(MenuButton, normalBg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_ATTR, "-disabledattributes", "disabledAttributes",
        "DisabledAttributes", DEF_MENUBUTTON_DISABLED_ATTR,
        Ck_Offset(MenuButton, disabledAttr), 0},
    {CK_CONFIG_COLOR, "-disabledbackground", "disabledBackground",
	"DisabledBackground", DEF_MENUBUTTON_DISABLED_FG_COLOR,
	Ck_Offset(MenuButton, disabledBg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-disabledbackground", "disabledBackground",
	"DisabledBackground", DEF_MENUBUTTON_DISABLED_BG_MONO,
	Ck_Offset(MenuButton, disabledBg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
	"DisabledForeground", DEF_MENUBUTTON_DISABLED_BG_COLOR,
	Ck_Offset(MenuButton, disabledFg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
	"DisabledForeground", DEF_MENUBUTTON_DISABLED_FG_MONO,
	Ck_Offset(MenuButton, disabledFg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_MENUBUTTON_FG, Ck_Offset(MenuButton, normalFg), 0},
    {CK_CONFIG_COORD, "-height", "height", "Height",
	DEF_MENUBUTTON_HEIGHT, Ck_Offset(MenuButton, height), 0},
    {CK_CONFIG_COLOR, "-indicatorforeground", "indicatorForeground",
        "Foreground", DEF_MENUBUTTON_INDICATOR_FG_COLOR,
        Ck_Offset(MenuButton, indicatorFg), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-indicatorforeground", "indicatorForeground",
        "Foreground", DEF_MENUBUTTON_INDICATOR_FG_MONO,
        Ck_Offset(MenuButton, indicatorFg), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_BOOLEAN, "-indicatoron", "indicatorOn", "IndicatorOn",
	DEF_MENUBUTTON_INDICATOR, Ck_Offset(MenuButton, indicatorOn), 0},
    {CK_CONFIG_STRING, "-menu", "menu", "Menu",
	DEF_MENUBUTTON_MENU, Ck_Offset(MenuButton, menuName),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_UID, "-state", "state", "State",
	DEF_MENUBUTTON_STATE, Ck_Offset(MenuButton, state), 0},
    {CK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_MENUBUTTON_TAKE_FOCUS, Ck_Offset(MenuButton, takeFocus),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-text", "text", "Text",
	DEF_MENUBUTTON_TEXT, Ck_Offset(MenuButton, text), 0},
    {CK_CONFIG_STRING, "-textvariable", "textVariable", "Variable",
	DEF_MENUBUTTON_TEXT_VARIABLE, Ck_Offset(MenuButton, textVarName),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_INT, "-underline", "underline", "Underline",
	DEF_MENUBUTTON_UNDERLINE, Ck_Offset(MenuButton, underline), 0},
    {CK_CONFIG_ATTR, "-underlineattributes", "underlineAttributes",
        "UnderlineAttributes", DEF_MENUBUTTON_UNDERLINE_ATTR,
        Ck_Offset(MenuButton, underlineAttr), 0},
    {CK_CONFIG_COLOR, "-underlineforeground", "underlineForeground",
        "UnderlineForeground", DEF_MENUBUTTON_UNDERLINE_FG_COLOR,
        Ck_Offset(MenuButton, underlineFg), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-underlineforeground", "underlineForeground",
        "UnderlineForeground", DEF_MENUBUTTON_UNDERLINE_FG_MONO,
        Ck_Offset(MenuButton, underlineFg), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COORD, "-width", "width", "Width",
	DEF_MENUBUTTON_WIDTH, Ck_Offset(MenuButton, width), 0},
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		ComputeMenuButtonGeometry _ANSI_ARGS_((
			    MenuButton *mbPtr));
static void		MenuButtonCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static void		MenuButtonEventProc _ANSI_ARGS_((ClientData clientData,
			    CkEvent *eventPtr));
static char *		MenuButtonTextVarProc _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp,
			    char *name1, char *name2, int flags));
static int		MenuButtonWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static int		ConfigureMenuButton _ANSI_ARGS_((Tcl_Interp *interp,
			    MenuButton *mbPtr, int argc, char **argv,
			    int flags));
static void		DestroyMenuButton _ANSI_ARGS_((ClientData clientData));
static void		DisplayMenuButton _ANSI_ARGS_((ClientData clientData));

/*
 *--------------------------------------------------------------
 *
 * Ck_MenubuttonCmd --
 *
 *	This procedure is invoked to process the "menubutton"
 *	Tcl commands.  See the user documentation for details
 *      on what it does.
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
Ck_MenubuttonCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    register MenuButton *mbPtr;
    CkWindow *mainPtr = (CkWindow *) clientData;
    CkWindow *new;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args:  should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Create the new window.
     */

    new = Ck_CreateWindowFromPath(interp, mainPtr, argv[1], 0);
    if (new == NULL) {
	return TCL_ERROR;
    }

    /*
     * Initialize the data structure for the button.
     */

    mbPtr = (MenuButton *) ckalloc(sizeof(MenuButton));
    mbPtr->winPtr = new;
    mbPtr->interp = interp;
    mbPtr->widgetCmd = Tcl_CreateCommand(interp, mbPtr->winPtr->pathName,
	    MenuButtonWidgetCmd, (ClientData) mbPtr, MenuButtonCmdDeletedProc);
    mbPtr->menuName = NULL;
    mbPtr->text = NULL;
    mbPtr->numChars = 0;

    mbPtr->textVarName = NULL;
    mbPtr->state = ckNormalUid;
    mbPtr->normalBg = 0;
    mbPtr->normalFg = 0;
    mbPtr->normalAttr = 0;
    mbPtr->activeBg = 0;
    mbPtr->activeFg = 0;
    mbPtr->activeAttr = 0;
    mbPtr->disabledBg = 0;
    mbPtr->disabledFg = 0;
    mbPtr->disabledAttr = 0;
    mbPtr->underlineFg = 0;
    mbPtr->underlineAttr = 0;
    mbPtr->indicatorFg = 0;
    mbPtr->underline = -1;
    mbPtr->width = 0;
    mbPtr->height = 0;
    mbPtr->anchor = CK_ANCHOR_CENTER;
    mbPtr->indicatorOn = 0;
    mbPtr->takeFocus = NULL;
    mbPtr->flags = 0;

    Ck_SetClass(mbPtr->winPtr, "Menubutton");
    Ck_CreateEventHandler(mbPtr->winPtr,
	    CK_EV_EXPOSE | CK_EV_MAP | CK_EV_DESTROY,
	    MenuButtonEventProc, (ClientData) mbPtr);
    if (ConfigureMenuButton(interp, mbPtr, argc-2, argv+2, 0) != TCL_OK) {
	Ck_DestroyWindow(mbPtr->winPtr);
	return TCL_ERROR;
    }

    interp->result = mbPtr->winPtr->pathName;
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * MenuButtonWidgetCmd --
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
MenuButtonWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about button widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    register MenuButton *mbPtr = (MenuButton *) clientData;
    int result = TCL_OK;
    size_t length;
    int c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Ck_Preserve((ClientData) mbPtr);
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " cget option\"",
		    (char *) NULL);
	    goto error;
	}
	result = Ck_ConfigureValue(interp, mbPtr->winPtr, configSpecs,
		(char *) mbPtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 2)) {
	if (argc == 2) {
	    result = Ck_ConfigureInfo(interp, mbPtr->winPtr, configSpecs,
		    (char *) mbPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Ck_ConfigureInfo(interp, mbPtr->winPtr, configSpecs,
		    (char *) mbPtr, argv[2], 0);
	} else {
	    result = ConfigureMenuButton(interp, mbPtr, argc-2, argv+2,
		    CK_CONFIG_ARGV_ONLY);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\":  must be cget or configure",
		(char *) NULL);
	goto error;
    }
    Ck_Release((ClientData) mbPtr);
    return result;

    error:
    Ck_Release((ClientData) mbPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyMenuButton --
 *
 *	This procedure is invoked to recycle all of the resources
 *	associated with a button widget.  It is invoked as a
 *	when-idle handler in order to make sure that there is no
 *	other use of the button pending at the time of the deletion.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the widget is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyMenuButton(clientData)
    ClientData clientData;	/* Info about button widget. */
{
    register MenuButton *mbPtr = (MenuButton *) clientData;

    /*
     * Free up all the stuff that requires special handling, then
     * let Ck_FreeOptions handle all the standard option-related
     * stuff.
     */

    if (mbPtr->textVarName != NULL) {
	Tcl_UntraceVar(mbPtr->interp, mbPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MenuButtonTextVarProc, (ClientData) mbPtr);
    }
    Ck_FreeOptions(configSpecs, (char *) mbPtr, 0);
    ckfree((char *) mbPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureMenuButton --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a menubutton widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for mbPtr;  old resources get freed, if there
 *	were any.  The menubutton is redisplayed.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureMenuButton(interp, mbPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    register MenuButton *mbPtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    int result;

    /*
     * Eliminate any existing trace on variables monitored by the menubutton.
     */

    if (mbPtr->textVarName != NULL) {
	Tcl_UntraceVar(interp, mbPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MenuButtonTextVarProc, (ClientData) mbPtr);
    }

    result = Ck_ConfigureWidget(interp, mbPtr->winPtr, configSpecs,
	    argc, argv, (char *) mbPtr, flags);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * A few options need special processing, such as setting the
     * background from a 3-D border, or filling in complicated
     * defaults that couldn't be specified to Tk_ConfigureWidget.
     */

    if ((mbPtr->state != ckNormalUid) && (mbPtr->state != ckActiveUid)
	    && (mbPtr->state != ckDisabledUid)) {
	Tcl_AppendResult(interp, "bad state value \"", mbPtr->state,
	    "\":  must be normal, active, or disabled", (char *) NULL);
	mbPtr->state = ckNormalUid;
	return TCL_ERROR;
    }

    if (mbPtr->textVarName != NULL) {
	/*
	 * The menubutton displays a variable.  Set up a trace to watch
	 * for any changes in it.
	 */

	char *value;

	value = Tcl_GetVar(interp, mbPtr->textVarName, TCL_GLOBAL_ONLY);
	if (value == NULL) {
	    Tcl_SetVar(interp, mbPtr->textVarName, mbPtr->text,
		    TCL_GLOBAL_ONLY);
	} else {
	    if (mbPtr->text != NULL) {
		ckfree(mbPtr->text);
	    }
	    mbPtr->text = (char *) ckalloc((unsigned) (strlen(value) + 1));
	    strcpy(mbPtr->text, value);
	}
	Tcl_TraceVar(interp, mbPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MenuButtonTextVarProc, (ClientData) mbPtr);
    }

    ComputeMenuButtonGeometry(mbPtr);

    /*
     * Lastly, arrange for the button to be redisplayed.
     */

    if ((mbPtr->winPtr->flags & CK_MAPPED)
        && !(mbPtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplayMenuButton, (ClientData) mbPtr);
	mbPtr->flags |= REDRAW_PENDING;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayMenuButton --
 *
 *	This procedure is invoked to display a menubutton widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to display the menubutton in its
 *	current mode.
 *
 *----------------------------------------------------------------------
 */

static void
DisplayMenuButton(clientData)
    ClientData clientData;	/* Information about widget. */
{
    MenuButton *mbPtr = (MenuButton *) clientData;
    int x, y, fg, bg, attr, textWidth, charWidth;
    CkWindow *winPtr = mbPtr->winPtr;

    mbPtr->flags &= ~REDRAW_PENDING;
    if ((mbPtr->winPtr == NULL) || !(winPtr->flags & CK_MAPPED)) {
	return;
    }

    if (mbPtr->state == ckDisabledUid) {
        fg = mbPtr->disabledFg;
        bg = mbPtr->disabledBg;
        attr = mbPtr->disabledAttr;
    } else if (mbPtr->state == ckActiveUid) {
        fg = mbPtr->activeFg;
        bg = mbPtr->activeBg;
        attr = mbPtr->activeAttr;
    } else {
        fg = mbPtr->normalFg;
        bg = mbPtr->normalBg;
        attr = mbPtr->normalAttr;
    }

    /*
     * Display text for button.
     */

    if (mbPtr->text != NULL)
	CkMeasureChars(winPtr->mainPtr, mbPtr->text, mbPtr->numChars, 0,
	    winPtr->width, 0, CK_NEWLINES_NOT_SPECIAL | CK_IGNORE_TABS,
	    &textWidth, &charWidth);
    else
	textWidth = 0;

    switch (mbPtr->anchor) {
	case CK_ANCHOR_NW: case CK_ANCHOR_W: case CK_ANCHOR_SW:
	    x = 0;
	    break;
	case CK_ANCHOR_N: case CK_ANCHOR_CENTER: case CK_ANCHOR_S:
	    x = (winPtr->width - textWidth) / 2;
            if (mbPtr->indicatorOn)
                x--;
	    break;
	default:
	    x = winPtr->width - textWidth;
            if (mbPtr->indicatorOn)
                x -= 2;
	    break;
    }
    if (x + textWidth > winPtr->width)
	textWidth = winPtr->width - x;

    switch (mbPtr->anchor) {
	case CK_ANCHOR_NW: case CK_ANCHOR_N: case CK_ANCHOR_NE:
	    y = 0;
	    break;
	case CK_ANCHOR_W: case CK_ANCHOR_CENTER: case CK_ANCHOR_E:
	    y = (winPtr->height - 1) / 2;
	    break;
	default:
	    y = winPtr->height - 1;
            if (y < 0)
                y = 0;
	    break;
    }

    Ck_SetWindowAttr(winPtr, fg, bg, attr);
    Ck_ClearToBot(winPtr, 0, 0);
    if (mbPtr->text != NULL) {
	CkDisplayChars(winPtr->mainPtr, winPtr->window, mbPtr->text,
	    charWidth, x, y,
	    0, CK_NEWLINES_NOT_SPECIAL | CK_IGNORE_TABS);
	if (mbPtr->underline >= 0 && mbPtr->state == ckNormalUid) {
	    Ck_SetWindowAttr(winPtr, mbPtr->underlineFg, bg,
		mbPtr->underlineAttr);
	    CkUnderlineChars(winPtr->mainPtr, winPtr->window,
		mbPtr->text, charWidth, x, y,
		0, CK_NEWLINES_NOT_SPECIAL | CK_IGNORE_TABS,
		mbPtr->underline, mbPtr->underline);
	    Ck_SetWindowAttr(winPtr, fg, bg, attr);
        }
    }
    if (mbPtr->indicatorOn) {
	long gchar;

	x = textWidth + 2;
	if (x >= winPtr->width)
	    x = winPtr->width - 1;
	Ck_GetGChar(mbPtr->interp, "diamond", &gchar);
	Ck_SetWindowAttr(winPtr, mbPtr->indicatorFg, bg, attr);
        mvwaddch(winPtr->window, y, x, gchar);
    }
    Ck_SetWindowAttr(winPtr, fg, bg, attr);
    wmove(winPtr->window, y, x);
    Ck_EventuallyRefresh(winPtr);
}

/*
 *--------------------------------------------------------------
 *
 * MenuButtonEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on buttons.
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
MenuButtonEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    CkEvent *eventPtr;		/* Information about event. */
{
    MenuButton *mbPtr = (MenuButton *) clientData;

    if (eventPtr->type == CK_EV_EXPOSE) {
        if (mbPtr->winPtr != NULL && !(mbPtr->flags & REDRAW_PENDING)) {
	    Tk_DoWhenIdle(DisplayMenuButton, (ClientData) mbPtr);
	    mbPtr->flags |= REDRAW_PENDING;
        }
    } else if (eventPtr->type == CK_EV_DESTROY) {
	if (mbPtr->winPtr != NULL) {
	    mbPtr->winPtr = NULL;
	    Tcl_DeleteCommand(mbPtr->interp,
		    Tcl_GetCommandName(mbPtr->interp, mbPtr->widgetCmd));
	}
	if (mbPtr->flags & REDRAW_PENDING) {
	    Tk_CancelIdleCall(DisplayMenuButton, (ClientData) mbPtr);
	}
	Ck_EventuallyFree((ClientData) mbPtr,
	    (Ck_FreeProc *) DestroyMenuButton);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MenuButtonCmdDeletedProc --
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
MenuButtonCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    MenuButton *mbPtr = (MenuButton *) clientData;
    CkWindow *winPtr = mbPtr->winPtr;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case winPtr
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (winPtr != NULL) {
	mbPtr->winPtr = NULL;
	Ck_DestroyWindow(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeMenuButtonGeometry --
 *
 *	After changes in a menu button's text or bitmap, this procedure
 *	recomputes the menu button's geometry and passes this information
 *	along to the geometry manager for the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The menu button's window may change size.
 *
 *----------------------------------------------------------------------
 */

static void
ComputeMenuButtonGeometry(mbPtr)
    register MenuButton *mbPtr;		/* Widget record for menu button. */
{
    int width, height, dummy;
    CkWindow *winPtr = mbPtr->winPtr;

    mbPtr->numChars = mbPtr->text == NULL ? 0 : strlen(mbPtr->text);
    if (mbPtr->height > 0)
        height = mbPtr->height;
    else
        height = 1;
    if (mbPtr->width > 0)
        width = mbPtr->width;
    else
	CkMeasureChars(winPtr->mainPtr, mbPtr->text == NULL ? "" : mbPtr->text,
	    mbPtr->numChars, 0, 100000, 0,
	    CK_NEWLINES_NOT_SPECIAL | CK_IGNORE_TABS,
	    &width, &dummy);

    /*
     * When issuing the geometry request, add extra space for the indicator
     * if any.
     */

    if (mbPtr->indicatorOn)
        width += 2;

    Ck_GeometryRequest(mbPtr->winPtr, width, height);
}

/*
 *--------------------------------------------------------------
 *
 * MenuButtonTextVarProc --
 *
 *	This procedure is invoked when someone changes the variable
 *	whose contents are to be displayed in a menu button.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The text displayed in the menu button will change to match the
 *	variable.
 *
 *--------------------------------------------------------------
 */

static char *
MenuButtonTextVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about button. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    register MenuButton *mbPtr = (MenuButton *) clientData;
    char *value;

    /*
     * If the variable is unset, then immediately recreate it unless
     * the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    Tcl_SetVar(interp, mbPtr->textVarName, mbPtr->text,
		    TCL_GLOBAL_ONLY);
	    Tcl_TraceVar(interp, mbPtr->textVarName,
		    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    MenuButtonTextVarProc, clientData);
	}
	return (char *) NULL;
    }

    value = Tcl_GetVar(interp, mbPtr->textVarName, TCL_GLOBAL_ONLY);
    if (value == NULL) {
	value = "";
    }
    if (mbPtr->text != NULL) {
	ckfree(mbPtr->text);
    }
    mbPtr->text = (char *) ckalloc((unsigned) (strlen(value) + 1));
    strcpy(mbPtr->text, value);
    ComputeMenuButtonGeometry(mbPtr);

    if ((mbPtr->winPtr != NULL) && (mbPtr->winPtr->flags & CK_MAPPED)
	    && !(mbPtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplayMenuButton, (ClientData) mbPtr);
	mbPtr->flags |= REDRAW_PENDING;
    }
    return (char *) NULL;
}
