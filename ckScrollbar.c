/* 
 * ckScrollbar.c --
 *
 *	This module implements a scrollbar widgets for the
 *	toolkit.  A scrollbar displays a slider and two arrows;
 *	mouse clicks on features within the scrollbar cause
 *	scrolling commands to be invoked.
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
 * A data structure of the following type is kept for each scrollbar
 * widget managed by this file:
 */

typedef struct {
    CkWindow *winPtr;		/* Window that embodies the scrollbar.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Tcl_Interp *interp;		/* Interpreter associated with scrollbar. */
    Tcl_Command widgetCmd;      /* Token for scrollbar's widget command. */
    Ck_Uid orientUid;		/* Orientation for window ("vertical" or
				 * "horizontal"). */
    int vertical;		/* Non-zero means vertical orientation
				 * requested, zero means horizontal. */
    char *command;		/* Command prefix to use when invoking
				 * scrolling commands.  NULL means don't
				 * invoke commands.  Malloc'ed. */
    int commandSize;		/* Number of non-NULL bytes in command. */

    /*
     * Information used when displaying widget:
     */

    int normalBg;		/* Used for drawing background. */
    int normalFg;		/* Used for drawing foreground. */
    int normalAttr;		/* Video attributes for normal mode. */
    int activeBg;		/* Background in active mode. */
    int activeFg;		/* Foreground in active mode. */
    int activeAttr;		/* Video attributes for active mode. */

    int sliderFirst;		/* Coordinate of top or left edge
				 * of slider area. */
    int sliderLast;             /* Coordinate just after bottom
                                 * or right edge of slider area. */
    /*
     * Information describing the application related to the scrollbar.
     * This information is provided by the application by invoking the
     * "set" widget command.
     */

    double firstFraction;	/* Position of first visible thing in window,
				 * specified as a fraction between 0 and
				 * 1.0. */
    double lastFraction;	/* Position of last visible thing in window,
				 * specified as a fraction between 0 and
				 * 1.0. */

    /*
     * Miscellaneous information:
     */

    char *takeFocus;		/* Value of -takefocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
    int flags;			/* Various flags;  see below for
				 * definitions. */
} Scrollbar;

/*
 * Flag bits for scrollbars:
 * 
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 * ACTIVATED:			1 means draw in activated mode,
 *				0 means draw in normal mode
 */

#define REDRAW_PENDING		1
#define ACTIVATED		2

/*
 * Legal values for identifying position in scrollbar. These
 * are the return values from the ScrollbarPosition procedure.
 */

#define OUTSIDE         0
#define TOP_ARROW       1
#define TOP_GAP         2
#define SLIDER          3
#define BOTTOM_GAP      4
#define BOTTOM_ARROW    5

/*
 * Minimum slider length, in pixels (designed to make sure that the slider
 * is always easy to grab with the mouse).
 */

#define MIN_SLIDER_LENGTH	1

/*
 * Information used for argv parsing.
 */

static Ck_ConfigSpec configSpecs[] = {
    {CK_CONFIG_ATTR, "-activeattributes", "activeAttributes", "Attributes",
	DEF_SCROLLBAR_ACTIVE_ATTR_COLOR, Ck_Offset(Scrollbar, activeAttr),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_ATTR, "-activeattributes", "activeAttributes", "Attributes",
	DEF_SCROLLBAR_ACTIVE_ATTR_MONO, Ck_Offset(Scrollbar, activeAttr),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-activebackground", "activeBackground", "Foreground",
	DEF_SCROLLBAR_ACTIVE_BG_COLOR, Ck_Offset(Scrollbar, activeBg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-activebackground", "activeBackground", "Foreground",
	DEF_SCROLLBAR_ACTIVE_BG_MONO, Ck_Offset(Scrollbar, activeBg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_SCROLLBAR_ACTIVE_FG_COLOR, Ck_Offset(Scrollbar, activeFg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_SCROLLBAR_ACTIVE_FG_MONO, Ck_Offset(Scrollbar, activeBg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_ATTR, "-attributes", "attributes", "Attributes",
	DEF_SCROLLBAR_ATTR, Ck_Offset(Scrollbar, normalAttr), 0},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_SCROLLBAR_BG_COLOR, Ck_Offset(Scrollbar, normalBg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_SCROLLBAR_BG_MONO, Ck_Offset(Scrollbar, normalBg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_STRING, "-command", "command", "Command",
	DEF_SCROLLBAR_COMMAND, Ck_Offset(Scrollbar, command),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_SCROLLBAR_FG_COLOR, Ck_Offset(Scrollbar, normalFg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_SCROLLBAR_FG_MONO, Ck_Offset(Scrollbar, normalFg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_UID, "-orient", "orient", "Orient",
	DEF_SCROLLBAR_ORIENT, Ck_Offset(Scrollbar, orientUid), 0},
    {CK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_SCROLLBAR_TAKE_FOCUS, Ck_Offset(Scrollbar, takeFocus),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		ComputeScrollbarGeometry _ANSI_ARGS_((
			    Scrollbar *scrollPtr));
static int		ConfigureScrollbar _ANSI_ARGS_((Tcl_Interp *interp,
			    Scrollbar *scrollPtr, int argc, char **argv,
			    int flags));
static void		DestroyScrollbar _ANSI_ARGS_((ClientData clientData));
static void		DisplayScrollbar _ANSI_ARGS_((ClientData clientData));
static void		EventuallyRedraw _ANSI_ARGS_((Scrollbar *scrollPtr));
static void		ScrollbarEventProc _ANSI_ARGS_((ClientData clientData,
			    CkEvent *eventPtr));
static void             ScrollbarCmdDeletedProc _ANSI_ARGS_((
                            ClientData clientData));
static int              ScrollbarPosition _ANSI_ARGS_((Scrollbar *scrollPtr,
			    int x, int y));
static int		ScrollbarWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *, int argc, char **argv));

/*
 *--------------------------------------------------------------
 *
 * Ck_ScrollbarCmd --
 *
 *	This procedure is invoked to process the "scrollbar" Tcl
 *	command.  See the user documentation for details on what
 *	it does.
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
Ck_ScrollbarCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *mainPtr = (CkWindow *) clientData;
    register Scrollbar *scrollPtr;
    CkWindow *new;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args:  should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    new = Ck_CreateWindowFromPath(interp, mainPtr, argv[1], 0);
    if (new == NULL) {
	return TCL_ERROR;
    }

    /*
     * Initialize fields that won't be initialized by ConfigureScrollbar,
     * or which ConfigureScrollbar expects to have reasonable values
     * (e.g. resource pointers).
     */

    scrollPtr = (Scrollbar *) ckalloc(sizeof (Scrollbar));
    scrollPtr->winPtr = new;
    scrollPtr->interp = interp;
    scrollPtr->widgetCmd = Tcl_CreateCommand(interp,
        scrollPtr->winPtr->pathName, ScrollbarWidgetCmd,
	    (ClientData) scrollPtr, ScrollbarCmdDeletedProc);
    scrollPtr->orientUid = NULL;
    scrollPtr->vertical = 0;
    scrollPtr->command = NULL;
    scrollPtr->commandSize = 0;
    scrollPtr->normalBg = 0;
    scrollPtr->normalFg = 0;
    scrollPtr->normalAttr = 0;
    scrollPtr->activeBg = 0;
    scrollPtr->activeFg = 0;
    scrollPtr->activeAttr = 0;
    scrollPtr->firstFraction = 0.0;
    scrollPtr->lastFraction = 0.0;
    scrollPtr->takeFocus = NULL;
    scrollPtr->flags = 0;

    Ck_SetClass(scrollPtr->winPtr, "Scrollbar");
    Ck_CreateEventHandler(scrollPtr->winPtr,
	    CK_EV_EXPOSE | CK_EV_MAP | CK_EV_DESTROY,
	    ScrollbarEventProc, (ClientData) scrollPtr);
    if (ConfigureScrollbar(interp, scrollPtr, argc-2, argv+2, 0) != TCL_OK) {
	goto error;
    }

    interp->result = scrollPtr->winPtr->pathName;
    return TCL_OK;

error:
    Ck_DestroyWindow(scrollPtr->winPtr);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * ScrollbarWidgetCmd --
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
ScrollbarWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about scrollbar
					 * widget. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    register Scrollbar *scrollPtr = (Scrollbar *) clientData;
    int result = TCL_OK;
    size_t length;
    int c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Ck_Preserve((ClientData) scrollPtr);
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'a') && (strncmp(argv[1], "activate", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " activate\"", (char *) NULL);
	    goto error;
	}
	if (!(scrollPtr->flags & ACTIVATED)) {
	    scrollPtr->flags |= ACTIVATED;
	    EventuallyRedraw(scrollPtr);
	}
    } else if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	&& (length >= 2)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "wrong # args: should be \"",
                    argv[0], " cget option\"",
                    (char *) NULL);
            goto error;
        }
        result = Ck_ConfigureValue(interp, scrollPtr->winPtr, configSpecs,
                (char *) scrollPtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 2)) {
	if (argc == 2) {
	    result = Ck_ConfigureInfo(interp, scrollPtr->winPtr, configSpecs,
		    (char *) scrollPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Ck_ConfigureInfo(interp, scrollPtr->winPtr, configSpecs,
		    (char *) scrollPtr, argv[2], 0);
	} else {
	    result = ConfigureScrollbar(interp, scrollPtr, argc-2, argv+2,
		    CK_CONFIG_ARGV_ONLY);
	}
   } else if ((c == 'd') && (strncmp(argv[1], "deactivate", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " deactivate\"", (char *) NULL);
	    goto error;
	}
	if (scrollPtr->flags & ACTIVATED) {
	    scrollPtr->flags &= ~ACTIVATED;
	    EventuallyRedraw(scrollPtr);
	}
    } else if ((c == 'f') && (strncmp(argv[1], "fraction", length) == 0)) {
	int x, y, pos, length;
	double fraction;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " fraction x y\"", (char *) NULL);
	    goto error;
	}
	if ((Tcl_GetInt(interp, argv[2], &x) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[3], &y) != TCL_OK)) {
	    goto error;
	}
	if (scrollPtr->vertical) {
	    pos = y - 1;
	    length = scrollPtr->winPtr->height - 1 - 2;
	} else {
	    pos = x - 1;
	    length = scrollPtr->winPtr->width - 1 - 2;
	}
	if (length == 0) {
	    fraction = 0.0;
	} else {
	    fraction = ((double) pos / (double) length);
	}
	if (fraction < 0) {
	    fraction = 0;
	} else if (fraction > 1.0) {
	    fraction = 1.0;
	}
	sprintf(interp->result, "%g", fraction);
    } else if ((c == 'g') && (strncmp(argv[1], "get", length) == 0)) {
	char first[TCL_DOUBLE_SPACE], last[TCL_DOUBLE_SPACE];
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " get\"", (char *) NULL);
	    goto error;
	}
	Tcl_PrintDouble(interp, scrollPtr->firstFraction, first);
	Tcl_PrintDouble(interp, scrollPtr->lastFraction, last);
	Tcl_AppendResult(interp, first, " ", last, (char *) NULL);
    } else if ((c == 'i') && (strncmp(argv[1], "identify", length) == 0)) {
        int x, y, thing;

        if (argc != 4) {
            Tcl_AppendResult(interp, "wrong # args: should be \"",
                    argv[0], " identify x y\"", (char *) NULL);
            goto error;
        }
        if ((Tcl_GetInt(interp, argv[2], &x) != TCL_OK)
	    || (Tcl_GetInt(interp, argv[3], &y) != TCL_OK)) {
            goto error;
        }
        thing = ScrollbarPosition(scrollPtr, x, y);
        switch (thing) {
	    case TOP_ARROW:     interp->result = "arrow1";      break;
	    case TOP_GAP:       interp->result = "trough1";     break;
	    case SLIDER:        interp->result = "slider";      break;
	    case BOTTOM_GAP:    interp->result = "trough2";     break;
	    case BOTTOM_ARROW:  interp->result = "arrow2";      break;
        }
    } else if ((c == 's') && (strncmp(argv[1], "set", length) == 0)) {
	if (argc == 4) {
	    double first, last;

	    if (Tcl_GetDouble(interp, argv[2], &first) != TCL_OK) {
		goto error;
	    }
	    if (Tcl_GetDouble(interp, argv[3], &last) != TCL_OK) {
		goto error;
	    }
	    if (first < 0) {
		scrollPtr->firstFraction = 0;
	    } else if (first > 1.0) {
		scrollPtr->firstFraction = 1.0;
	    } else {
		scrollPtr->firstFraction = first;
	    }
	    if (last < scrollPtr->firstFraction) {
		scrollPtr->lastFraction = scrollPtr->firstFraction;
	    } else if (last > 1.0) {
		scrollPtr->lastFraction = 1.0;
	    } else {
		scrollPtr->lastFraction = last;
	    }
	} else {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " set firstFraction lastFraction\"",
		    (char *) NULL);
	    goto error;
	}
	ComputeScrollbarGeometry(scrollPtr);
	EventuallyRedraw(scrollPtr);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be activate, cget, configure, deactivate, ",
		"fraction, get, or set", (char *) NULL);
	goto error;
    }
    Ck_Release((ClientData) scrollPtr);
    return result;

error:
    Ck_Release((ClientData) scrollPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyScrollbar --
 *
 *	This procedure is invoked by Ck_EventuallyFree or Ck_Release
 *	to clean up the internal structure of a scrollbar at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the scrollbar is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyScrollbar(clientData)
    ClientData clientData;	/* Info about scrollbar widget. */
{
    register Scrollbar *scrollPtr = (Scrollbar *) clientData;

    /*
     * Free up all the stuff that requires special handling, then
     * let Ck_FreeOptions handle all the standard option-related
     * stuff.
     */

    Ck_FreeOptions(configSpecs, (char *) scrollPtr, 0);
    ckfree((char *) scrollPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ScrollbarCmdDeletedProc --
 *
 *      This procedure is invoked when a widget command is deleted.  If
 *      the widget isn't already in the process of being destroyed,
 *      this command destroys it.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The widget is destroyed.
 *
 *----------------------------------------------------------------------
 */

static void
ScrollbarCmdDeletedProc(clientData)
    ClientData clientData;      /* Pointer to widget record for widget. */
{
    Scrollbar *scrollPtr = (Scrollbar *) clientData;
    CkWindow *winPtr = scrollPtr->winPtr;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case winPtr
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (winPtr != NULL) {
        scrollPtr->winPtr = NULL;
        Ck_DestroyWindow(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureScrollbar --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the option database, in order to configure (or
 *	reconfigure) a scrollbar widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as colors, border width,
 *	etc. get set for scrollPtr;  old resources get freed,
 *	if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureScrollbar(interp, scrollPtr, argc, argv, flags)
    Tcl_Interp *interp;			/* Used for error reporting. */
    register Scrollbar *scrollPtr;	/* Information about widget;  may or
					 * may not already have values for
					 * some fields. */
    int argc;				/* Number of valid entries in argv. */
    char **argv;			/* Arguments. */
    int flags;				/* Flags to pass to
					 * Ck_ConfigureWidget. */
{
    size_t length;

    if (Ck_ConfigureWidget(interp, scrollPtr->winPtr, configSpecs,
	    argc, argv, (char *) scrollPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * A few options need special processing, such as parsing the
     * orientation or setting the background from a 3-D border.
     */

    length = strlen(scrollPtr->orientUid);
    if (strncmp(scrollPtr->orientUid, "vertical", length) == 0) {
	scrollPtr->vertical = 1;
    } else if (strncmp(scrollPtr->orientUid, "horizontal", length) == 0) {
	scrollPtr->vertical = 0;
    } else {
	Tcl_AppendResult(interp, "bad orientation \"", scrollPtr->orientUid,
		"\": must be vertical or horizontal", (char *) NULL);
	return TCL_ERROR;
    }

    if (scrollPtr->command != NULL) {
	scrollPtr->commandSize = strlen(scrollPtr->command);
    } else {
	scrollPtr->commandSize = 0;
    }

    /*
     * Register the desired geometry for the window (leave enough space
     * for the two arrows plus a minimum-size slider, plus border around
     * the whole window, if any).  Then arrange for the window to be
     * redisplayed.
     */

    ComputeScrollbarGeometry(scrollPtr);
    EventuallyRedraw(scrollPtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DisplayScrollbar --
 *
 *	This procedure redraws the contents of a scrollbar window.
 *	It is invoked as a do-when-idle handler, so it only runs
 *	when there's nothing else for the application to do.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information appears on the screen.
 *
 *--------------------------------------------------------------
 */

static void
DisplayScrollbar(clientData)
    ClientData clientData;	/* Information about window. */
{
    register Scrollbar *scrollPtr = (Scrollbar *) clientData;
    register CkWindow *winPtr = scrollPtr->winPtr;
    int width, i;
    long gchar;

    if ((scrollPtr->winPtr == NULL) || !(winPtr->flags & CK_MAPPED)) {
	goto done;
    }

    width = (scrollPtr->vertical ? winPtr->height : winPtr->width);

    if (scrollPtr->flags & ACTIVATED) {
	Ck_SetWindowAttr(winPtr, scrollPtr->activeFg,
	    scrollPtr->activeBg, scrollPtr->activeAttr);
    } else {
	Ck_SetWindowAttr(winPtr, scrollPtr->normalFg,
	    scrollPtr->normalBg, scrollPtr->normalAttr);
    }

    /*
     * Fill space left with blanks.
     */

    if (scrollPtr->vertical) {
    	for (i = 0; i < width; i++) {
    	    wmove(winPtr->window, i, 0);
	    waddch(winPtr->window, ' ');
	}
    } else {
	wmove(winPtr->window, 0, 0);
    	for (i = 0; i < width; i++) {
	    waddch(winPtr->window, ' ');
	}
    }

    /*
     * Display the slider.
     */

    Ck_GetGChar(scrollPtr->interp, "ckboard", &gchar);
    if (scrollPtr->vertical) {
    	for (i = scrollPtr->sliderFirst; i < scrollPtr->sliderLast; i++) {
	    mvwaddch(winPtr->window, i, 0, gchar);
	}
    } else {
	wmove(winPtr->window, 0, scrollPtr->sliderFirst);
    	for (i = scrollPtr->sliderFirst; i < scrollPtr->sliderLast; i++) {
	    waddch(winPtr->window, gchar);
	}
    }

    /*
     * Display top or left arrow.
     */

    Ck_GetGChar(scrollPtr->interp, scrollPtr->vertical ? "uarrow" : "larrow",
	&gchar);
    mvwaddch(winPtr->window, 0, 0, gchar);

    /*
     * Display the bottom or right arrow.
     */

    Ck_GetGChar(scrollPtr->interp, scrollPtr->vertical ? "darrow" : "rarrow",
	&gchar);
    scrollPtr->vertical ? wmove(winPtr->window, width - 1, 0) :
	wmove(winPtr->window, 0, width - 1);
    waddch(winPtr->window, gchar);

    Ck_EventuallyRefresh(winPtr);

done:
    scrollPtr->flags &= ~REDRAW_PENDING;
}

/*
 *--------------------------------------------------------------
 *
 * ScrollbarEventProc --
 *
 *	This procedure is invoked by the dispatcher for various
 *	events on scrollbars.
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
ScrollbarEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    CkEvent *eventPtr;		/* Information about event. */
{
    Scrollbar *scrollPtr = (Scrollbar *) clientData;

    if (eventPtr->type == CK_EV_EXPOSE) {
	ComputeScrollbarGeometry(scrollPtr);
	EventuallyRedraw(scrollPtr);
    } else if (eventPtr->type == CK_EV_DESTROY) {
        if (scrollPtr->winPtr != NULL) {
            scrollPtr->winPtr = NULL;
            Tcl_DeleteCommand(scrollPtr->interp,
                Tcl_GetCommandName(scrollPtr->interp, scrollPtr->widgetCmd));
        }
	if (scrollPtr->flags & REDRAW_PENDING) {
	    Tk_CancelIdleCall(DisplayScrollbar, (ClientData) scrollPtr);
	}
	Ck_EventuallyFree((ClientData) scrollPtr,
	    (Ck_FreeProc *) DestroyScrollbar);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeScrollbarGeometry --
 *
 *	After changes in a scrollbar's size or configuration, this
 *	procedure recomputes various geometry information used in
 *	displaying the scrollbar.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The scrollbar will be displayed differently.
 *
 *----------------------------------------------------------------------
 */

static void
ComputeScrollbarGeometry(scrollPtr)
    register Scrollbar *scrollPtr;	/* Scrollbar whose geometry may
					 * have changed. */
{
    int fieldLength;

    fieldLength = (scrollPtr->vertical ? scrollPtr->winPtr->height
	    : scrollPtr->winPtr->width) - 2;
    if (fieldLength < 0) {
	fieldLength = 0;
    }
    scrollPtr->sliderFirst = (int) (fieldLength * scrollPtr->firstFraction);
    scrollPtr->sliderLast = (int) (fieldLength * scrollPtr->lastFraction);

    /*
     * Adjust the slider so that some piece of it is always
     * displayed in the scrollbar and so that it has at least
     * a minimal width (so it can be grabbed with the mouse).
     */

    if (scrollPtr->sliderFirst > fieldLength) {
	scrollPtr->sliderFirst = fieldLength;
    }
    if (scrollPtr->sliderFirst < 0) {
	scrollPtr->sliderFirst = 0;
    }
    if (scrollPtr->sliderLast < (scrollPtr->sliderFirst
	    + MIN_SLIDER_LENGTH)) {
	scrollPtr->sliderLast = scrollPtr->sliderFirst + MIN_SLIDER_LENGTH;
    }
    if (scrollPtr->sliderLast > fieldLength) {
	scrollPtr->sliderLast = fieldLength;
    }
    scrollPtr->sliderFirst += 1;
    scrollPtr->sliderLast += 1;

    /*
     * Register the desired geometry for the window (leave enough space
     * for the two arrows plus a minimum-size slider, plus border around
     * the whole window, if any).  Then arrange for the window to be
     * redisplayed.
     */

    if (scrollPtr->vertical) {
	Ck_GeometryRequest(scrollPtr->winPtr, 1, 2 + MIN_SLIDER_LENGTH);
    } else {
	Ck_GeometryRequest(scrollPtr->winPtr, 2 + MIN_SLIDER_LENGTH, 1);
    }
}

/*
 *--------------------------------------------------------------
 *
 * ScrollbarPosition --
 *
 *	Determine the scrollbar element corresponding to a
 *	given position.
 *
 * Results:
 *	One of TOP_ARROW, TOP_GAP, etc., indicating which element
 *	of the scrollbar covers the position given by (x, y).  If
 *	(x,y) is outside the scrollbar entirely, then OUTSIDE is
 *	returned.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
ScrollbarPosition(scrollPtr, x, y)
    register Scrollbar *scrollPtr;	/* Scrollbar widget record. */
    int x, y;				/* Coordinates within scrollPtr's
					 * window. */
{
    int length, width, tmp;

    if (scrollPtr->vertical) {
	length = scrollPtr->winPtr->height;
	width = scrollPtr->winPtr->width;
    } else {
	tmp = x;
	x = y;
	y = tmp;
	length = scrollPtr->winPtr->width;
	width = scrollPtr->winPtr->height;
    }

    if (x < 0 || x >= width || y < 0 || y >= length)
	return OUTSIDE;

    if (y == 0)
	return TOP_ARROW;
    if (y < scrollPtr->sliderFirst)
	return TOP_GAP;
    if (y < scrollPtr->sliderLast)
	return SLIDER;
    if (y == length - 1)
	return BOTTOM_ARROW;
    return BOTTOM_GAP;
}

/*
 *--------------------------------------------------------------
 *
 * EventuallyRedraw --
 *
 *	Arrange for one or more of the fields of a scrollbar
 *	to be redrawn.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
EventuallyRedraw(scrollPtr)
    register Scrollbar *scrollPtr;	/* Information about widget. */
{
    if ((scrollPtr->winPtr == NULL) ||
	!(scrollPtr->winPtr->flags & CK_MAPPED)) {
	return;
    }
    if ((scrollPtr->flags & REDRAW_PENDING) == 0) {
	Tk_DoWhenIdle(DisplayScrollbar, (ClientData) scrollPtr);
	scrollPtr->flags |= REDRAW_PENDING;
    }
}
