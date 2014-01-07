/* 
 * ckEntry.c --
 *
 *	This module implements entry widgets for the
 *	toolkit.  An entry displays a string and allows
 *	the string to be edited.
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
 * A data structure of the following type is kept for each entry
 * widget managed by this file:
 */

typedef struct {
    CkWindow *winPtr;		/* Window that embodies the entry. NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Tcl_Interp *interp;		/* Interpreter associated with entry. */
    Tcl_Command widgetCmd;      /* Token for entry's widget command. */
#if CK_USE_UTF
    int numBytes;		/* Number of bytes in string. */
#endif
    int numChars;		/* Number of non-NULL characters in
				 * string (may be 0). */
    char *string;		/* Pointer to storage for string;
				 * NULL-terminated;  malloc-ed. */
    char *textVarName;		/* Name of variable (malloc'ed) or NULL.
				 * If non-NULL, entry's string tracks the
				 * contents of this variable and vice versa. */
    Ck_Uid state;		/* Normal or disabled.  Entry is read-only
				 * when disabled. */

    /*
     * Information used when displaying widget:
     */

    int normalBg;               /* Normal background color. */
    int normalFg;               /* Normal foreground color. */
    int normalAttr;             /* Normal video attributes. */
    int selBg;                  /* Select background color. */
    int selFg;                  /* Select foreground color. */
    int selAttr;                /* Select video attributes. */
    Ck_Justify justify;		/* Justification to use for text within
				 * window. */
    int leftX;                  /* X position at which leftIndex is drawn
                                 * (varies depending on justify). */
    int leftIndex;		/* Index of left-most character visible in
				 * window. */
    int tabOrigin;		/* Origin for tabs (left edge of string[0]). */
    int insertPos;		/* Index of character before which next
				 * typed character will be inserted. */
    char *showChar;		/* Value of -show option.  If non-NULL, first
				 * character is used for displaying all
				 * characters in entry.  Malloc'ed. */
    char *displayString;	/* If non-NULL, points to string with same
				 * length as string but whose characters
				 * are all equal to showChar.  Malloc'ed. */
    int prefWidth;              /* Preferred width for window. */

    /*
     * Information about what's selected, if any.
     */

    int selectFirst;            /* Index of first selected character (-1 means
                                 * nothing selected. */
    int selectLast;             /* Index of last selected character (-1 means
                                 * nothing selected. */
    int selectAnchor;           /* Fixed end of selection (i.e. "select to"
                                 * operation will use this as one end of the
                                 * selection). */

    /*
     * Miscellaneous information:
     */

    char *takeFocus;		/* Value of -takefocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
    char *scrollCmd;		/* Command prefix for communicating with
				 * scrollbar(s).  Malloc'ed.  NULL means
				 * no command to issue. */
    int flags;			/* Miscellaneous flags;  see below for
				 * definitions. */
} Entry;

/*
 * Assigned bits of "flags" fields of Entry structures, and what those
 * bits mean:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler has
 *				already been queued to redisplay the entry.
 * GOT_FOCUS:			Non-zero means this window has the input
 *				focus.
 * UPDATE_SCROLLBAR:		Non-zero means scrollbar should be updated
 *				during next redisplay operation.
 */

#define REDRAW_PENDING		1
#define GOT_FOCUS		2
#define UPDATE_SCROLLBAR	4

/*
 * Information used for argv parsing.
 */

static Ck_ConfigSpec configSpecs[] = {
    {CK_CONFIG_ATTR, "-attributes", "attributes", "Attributes",
        DEF_ENTRY_ATTR, Ck_Offset(Entry, normalAttr), 0},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_ENTRY_BG_COLOR, Ck_Offset(Entry, normalBg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_ENTRY_BG_MONO, Ck_Offset(Entry, normalBg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_ENTRY_FG, Ck_Offset(Entry, normalFg), 0},
    {CK_CONFIG_JUSTIFY, "-justify", "justify", "Justify",
	DEF_ENTRY_JUSTIFY, Ck_Offset(Entry, justify), 0},
    {CK_CONFIG_ATTR, "-selectattributes", "selectAttributes",
        "SelectAttributes", DEF_ENTRY_SELECT_ATTR_COLOR,
        Ck_Offset(Entry, selAttr), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_ATTR, "-selectattributes", "selectAttributes",
        "SelectAttributes", DEF_ENTRY_SELECT_ATTR_MONO,
        Ck_Offset(Entry, selAttr), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-selectbackground", "selectBackground", "Foreground",
	DEF_ENTRY_SELECT_BG_COLOR, Ck_Offset(Entry, selBg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-selectbackground", "selectBackground", "Foreground",
	DEF_ENTRY_SELECT_BG_MONO, Ck_Offset(Entry, selBg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_ENTRY_SELECT_FG_COLOR, Ck_Offset(Entry, selFg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_ENTRY_SELECT_FG_MONO, Ck_Offset(Entry, selFg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_STRING, "-show", "show", "Show",
	DEF_ENTRY_SHOW, Ck_Offset(Entry, showChar), CK_CONFIG_NULL_OK},
    {CK_CONFIG_UID, "-state", "state", "State",
        DEF_ENTRY_STATE, Ck_Offset(Entry, state), 0},
    {CK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_ENTRY_TAKE_FOCUS, Ck_Offset(Entry, takeFocus), CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-textvariable", "textVariable", "Variable",
	DEF_ENTRY_TEXT_VARIABLE, Ck_Offset(Entry, textVarName),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_INT, "-width", "width", "Width",
	DEF_ENTRY_WIDTH, Ck_Offset(Entry, prefWidth), 0},
    {CK_CONFIG_STRING, "-xscrollcommand", "xScrollCommand", "ScrollCommand",
	DEF_ENTRY_SCROLL_COMMAND, Ck_Offset(Entry, scrollCmd),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Flags for GetEntryIndex procedure:
 */

#define ZERO_OK			1
#define LAST_PLUS_ONE_OK	2

/*
 * Forward declarations for procedures defined later in this file:
 */

static int		ConfigureEntry _ANSI_ARGS_((Tcl_Interp *interp,
			    Entry *entryPtr, int argc, char **argv,
			    int flags));
static void		DeleteChars _ANSI_ARGS_((Entry *entryPtr, int index,
			    int count));
static void		DestroyEntry _ANSI_ARGS_((ClientData clientData));
static void		DisplayEntry _ANSI_ARGS_((ClientData clientData));
static void		EntryComputeGeometry _ANSI_ARGS_((Entry *entryPtr));
static void		EntryEventProc _ANSI_ARGS_((ClientData clientData,
			    CkEvent *eventPtr));
static void		EntryFocusProc _ANSI_ARGS_ ((Entry *entryPtr,
			    int gotFocus));
static void		EventuallyRedraw _ANSI_ARGS_((Entry *entryPtr));
static void             EntryCmdDeletedProc _ANSI_ARGS_((
                            ClientData clientData));
static void		EntrySetValue _ANSI_ARGS_((Entry *entryPtr,
			    char *value));
static void		EntrySelectTo _ANSI_ARGS_((
			    Entry *entryPtr, int index));
static char *		EntryTextVarProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));
static void		EntryUpdateScrollbar _ANSI_ARGS_((Entry *entryPtr));
static void		EntryVisibleRange _ANSI_ARGS_((Entry *entryPtr,
			    double *firstPtr, double *lastPtr));
static int		EntryWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static int		GetEntryIndex _ANSI_ARGS_((Tcl_Interp *interp,
			    Entry *entryPtr, char *string, int *indexPtr));
static void		InsertChars _ANSI_ARGS_((Entry *entryPtr, int index,
			    char *string));

/*
 *--------------------------------------------------------------
 *
 * Ck_EntryCmd --
 *
 *	This procedure is invoked to process the "entry" Tcl
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
Ck_EntryCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *mainPtr = (CkWindow *) clientData;
    register Entry *entryPtr;
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
     * Initialize the fields of the structure that won't be initialized
     * by ConfigureEntry, or that ConfigureEntry requires to be
     * initialized already (e.g. resource pointers).
     */

    entryPtr = (Entry *) ckalloc(sizeof (Entry));
    entryPtr->winPtr = new;
    entryPtr->interp = interp;
    entryPtr->widgetCmd = Tcl_CreateCommand(interp,
        entryPtr->winPtr->pathName, EntryWidgetCmd,
	    (ClientData) entryPtr, EntryCmdDeletedProc);
#if CK_USE_UTF
    entryPtr->numBytes = 0;
#endif
    entryPtr->numChars = 0;
    entryPtr->string = (char *) ckalloc(1);
    entryPtr->string[0] = '\0';
    entryPtr->textVarName = NULL;
    entryPtr->state = ckNormalUid;
    entryPtr->normalBg = 0;
    entryPtr->normalFg = 0;
    entryPtr->normalAttr = 0;
    entryPtr->selBg = 0;
    entryPtr->selFg = 0;
    entryPtr->selAttr = 0;
    entryPtr->justify = CK_JUSTIFY_LEFT;
    entryPtr->prefWidth = 0;
    entryPtr->leftIndex = 0;
    entryPtr->tabOrigin = 0;
    entryPtr->insertPos = 0;
    entryPtr->showChar = NULL;
    entryPtr->displayString = NULL;
    entryPtr->prefWidth = 1;
    entryPtr->selectFirst = -1;
    entryPtr->selectLast = -1;
    entryPtr->selectAnchor = 0;
    entryPtr->takeFocus = NULL;
    entryPtr->scrollCmd = NULL;
    entryPtr->flags = 0;

    Ck_SetClass(entryPtr->winPtr, "Entry");
    Ck_CreateEventHandler(entryPtr->winPtr,
	    CK_EV_EXPOSE | CK_EV_MAP | CK_EV_DESTROY,
	    EntryEventProc, (ClientData) entryPtr);
    if (ConfigureEntry(interp, entryPtr, argc-2, argv+2, 0) != TCL_OK) {
	goto error;
    }

    interp->result = entryPtr->winPtr->pathName;
    return TCL_OK;

    error:
    Ck_DestroyWindow(entryPtr->winPtr);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * EntryWidgetCmd --
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
EntryWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Information about entry widget. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    register Entry *entryPtr = (Entry *) clientData;
    int result = TCL_OK;
    size_t length;
    int c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Ck_Preserve((ClientData) entryPtr);
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
        result = Ck_ConfigureValue(interp, entryPtr->winPtr, configSpecs,
                (char *) entryPtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 2)) {
	if (argc == 2) {
	    result = Ck_ConfigureInfo(interp, entryPtr->winPtr, configSpecs,
		    (char *) entryPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Ck_ConfigureInfo(interp, entryPtr->winPtr, configSpecs,
		    (char *) entryPtr, argv[2], 0);
	} else {
	    result = ConfigureEntry(interp, entryPtr, argc-2, argv+2,
		    CK_CONFIG_ARGV_ONLY);
	}
    } else if ((c == 'd') && (strncmp(argv[1], "delete", length) == 0)) {
	int first, last;

	if ((argc < 3) || (argc > 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " delete firstIndex ?lastIndex?\"",
		    (char *) NULL);
	    goto error;
	}
	if (GetEntryIndex(interp, entryPtr, argv[2], &first) != TCL_OK) {
	    goto error;
	}
	if (argc == 3) {
	    last = first+1;
	} else {
	    if (GetEntryIndex(interp, entryPtr, argv[3], &last) != TCL_OK) {
		goto error;
	    }
	}
	if ((last >= first) && (entryPtr->state == ckNormalUid)) {
	    DeleteChars(entryPtr, first, last-first);
	}
    } else if ((c == 'g') && (strncmp(argv[1], "get", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " get\"", (char *) NULL);
	    goto error;
	}
	interp->result = entryPtr->string;
    } else if ((c == 'i') && (strncmp(argv[1], "icursor", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " icursor pos\"",
		    (char *) NULL);
	    goto error;
	}
	if (GetEntryIndex(interp, entryPtr, argv[2], &entryPtr->insertPos)
		!= TCL_OK) {
	    goto error;
	}
	EventuallyRedraw(entryPtr);
    } else if ((c == 'i') && (strncmp(argv[1], "index", length) == 0)
	    && (length >= 3)) {
	int index;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " index string\"", (char *) NULL);
	    goto error;
	}
	if (GetEntryIndex(interp, entryPtr, argv[2], &index) != TCL_OK) {
	    goto error;
	}
	sprintf(interp->result, "%d", index);
    } else if ((c == 'i') && (strncmp(argv[1], "insert", length) == 0)
	    && (length >= 3)) {
	int index;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " insert index text\"",
		    (char *) NULL);
	    goto error;
	}
	if (GetEntryIndex(interp, entryPtr, argv[2], &index) != TCL_OK) {
	    goto error;
	}
	if (entryPtr->state == ckNormalUid) {
	    InsertChars(entryPtr, index, argv[3]);
	}
    } else if ((c == 's') && (length >= 2)
	    && (strncmp(argv[1], "selection", length) == 0)) {
	int index, index2;

	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " select option ?index?\"", (char *) NULL);
	    goto error;
	}
	length = strlen(argv[2]);
	c = argv[2][0];
	if ((c == 'c') && (strncmp(argv[2], "clear", length) == 0)) {
	    if (argc != 3) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
			argv[0], " selection clear\"", (char *) NULL);
		goto error;
	    }
	    if (entryPtr->selectFirst != -1) {
		entryPtr->selectFirst = entryPtr->selectLast = -1;
		EventuallyRedraw(entryPtr);
	    }
	    goto done;
	} else if ((c == 'p') && (strncmp(argv[2], "present", length) == 0)) {
	    if (argc != 3) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
			argv[0], " selection present\"", (char *) NULL);
		goto error;
	    }
	    if (entryPtr->selectFirst == -1) {
		interp->result = "0";
	    } else {
		interp->result = "1";
	    }
	    goto done;
	}
	if (argc >= 4) {
	    if (GetEntryIndex(interp, entryPtr, argv[3], &index) != TCL_OK) {
		goto error;
	    }
	}
	if ((c == 'a') && (strncmp(argv[2], "adjust", length) == 0)) {
	    if (argc != 4) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
			argv[0], " selection adjust index\"",
			(char *) NULL);
		goto error;
	    }
	    if (entryPtr->selectFirst >= 0) {
		int half1, half2;

		half1 = (entryPtr->selectFirst + entryPtr->selectLast)/2;
		half2 = (entryPtr->selectFirst + entryPtr->selectLast + 1)/2;
		if (index < half1) {
		    entryPtr->selectAnchor = entryPtr->selectLast;
		} else if (index > half2) {
		    entryPtr->selectAnchor = entryPtr->selectFirst;
		} else {
		    /*
		     * We're at about the halfway point in the selection;
		     * just keep the existing anchor.
		     */
		}
	    }
	    EntrySelectTo(entryPtr, index);
	} else if ((c == 'f') && (strncmp(argv[2], "from", length) == 0)) {
	    if (argc != 4) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
			argv[0], " selection from index\"",
			(char *) NULL);
		goto error;
	    }
	    entryPtr->selectAnchor = index;
	} else if ((c == 'r') && (strncmp(argv[2], "range", length) == 0)) {
	    if (argc != 5) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
			argv[0], " selection range start end\"",
			(char *) NULL);
		goto error;
	    }
	    if (GetEntryIndex(interp, entryPtr, argv[4], &index2) != TCL_OK) {
		goto error;
	    }
	    if (index >= index2) {
		entryPtr->selectFirst = entryPtr->selectLast = -1;
	    } else {
		entryPtr->selectFirst = index;
		entryPtr->selectLast = index2;
	    }
	    EventuallyRedraw(entryPtr);
	} else if ((c == 't') && (strncmp(argv[2], "to", length) == 0)) {
	    if (argc != 4) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
			argv[0], " selection to index\"",
			(char *) NULL);
		goto error;
	    }
	    EntrySelectTo(entryPtr, index);
	} else {
	    Tcl_AppendResult(interp, "bad selection option \"", argv[2],
		    "\": must be adjust, clear, from, present, range, or to",
		    (char *) NULL);
	    goto error;
	}
    } else if ((c == 'x') && (strncmp(argv[1], "xview", length) == 0)) {
	int index, type, count, charsPerPage;
	double fraction, first, last;

	if (argc == 2) {
	    EntryVisibleRange(entryPtr, &first, &last);
	    sprintf(interp->result, "%g %g", first, last);
	    goto done;
	} else if (argc == 3) {
	    if (GetEntryIndex(interp, entryPtr, argv[2], &index) != TCL_OK) {
		goto error;
	    }
	} else {
	    type = Ck_GetScrollInfo(interp, argc, argv, &fraction, &count);
	    index = entryPtr->leftIndex;
	    switch (type) {
		case CK_SCROLL_ERROR:
		    goto error;
		case CK_SCROLL_MOVETO:
		    index = (int) (fraction * entryPtr->numChars);
		    break;
		case CK_SCROLL_PAGES:
		    charsPerPage = entryPtr->winPtr->width - 2;
		    if (charsPerPage < 1)
			charsPerPage = 1;
		    index += charsPerPage*count;
		    break;
		case CK_SCROLL_UNITS:
		    index += count;
		    break;
	    }
	}
	if (index >= entryPtr->numChars) {
	    index = entryPtr->numChars-1;
	}
	if (index < 0) {
	    index = 0;
	}
	entryPtr->leftIndex = index;
	entryPtr->flags |= UPDATE_SCROLLBAR;
	EntryComputeGeometry(entryPtr);
	EventuallyRedraw(entryPtr);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be cget, configure, delete, get, ",
		"icursor, index, insert, selection, or xview",
		(char *) NULL);
	goto error;
    }
done:
    Ck_Release((ClientData) entryPtr);
    return result;

error:
    Ck_Release((ClientData) entryPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyEntry --
 *
 *	This procedure is invoked by Ck_EventuallyFree or Ck_Release
 *	to clean up the internal structure of an entry at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the entry is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyEntry(clientData)
    ClientData clientData;			/* Info about entry widget. */
{
    Entry *entryPtr = (Entry *) clientData;

    /*
     * Free up all the stuff that requires special handling, then
     * let Ck_FreeOptions handle all the standard option-related
     * stuff.
     */

    ckfree(entryPtr->string);
    if (entryPtr->textVarName != NULL) {
	Tcl_UntraceVar(entryPtr->interp, entryPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		EntryTextVarProc, (ClientData) entryPtr);
    }
    if (entryPtr->displayString != NULL) {
	ckfree(entryPtr->displayString);
    }
    Ck_FreeOptions(configSpecs, (char *) entryPtr, 0);
    ckfree((char *) entryPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * EntryCmdDeletedProc --
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
EntryCmdDeletedProc(clientData)
    ClientData clientData;      /* Pointer to widget record for widget. */
{
    Entry *entryPtr = (Entry *) clientData;
    CkWindow *winPtr = entryPtr->winPtr;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case winPtr
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (winPtr != NULL) {
        entryPtr->winPtr = NULL;
        Ck_DestroyWindow(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureEntry --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	an entry widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as colors, border width,
 *	etc. get set for entryPtr;  old resources get freed,
 *	if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureEntry(interp, entryPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    register Entry *entryPtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    /*
     * Eliminate any existing trace on a variable monitored by the entry.
     */

    if (entryPtr->textVarName != NULL) {
	Tcl_UntraceVar(interp, entryPtr->textVarName, 
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		EntryTextVarProc, (ClientData) entryPtr);
    }

    if (Ck_ConfigureWidget(interp, entryPtr->winPtr, configSpecs,
	    argc, argv, (char *) entryPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * If the entry is tied to the value of a variable, then set up
     * a trace on the variable's value, create the variable if it doesn't
     * exist, and set the entry's value from the variable's value.
     */

    if (entryPtr->textVarName != NULL) {
	char *value;

	value = Tcl_GetVar(interp, entryPtr->textVarName, TCL_GLOBAL_ONLY);
	if (value == NULL) {
	    Tcl_SetVar(interp, entryPtr->textVarName, entryPtr->string,
		    TCL_GLOBAL_ONLY);
	} else {
	    EntrySetValue(entryPtr, value);
	}
	Tcl_TraceVar(interp, entryPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		EntryTextVarProc, (ClientData) entryPtr);
    }

    /*
     * A few other options also need special processing, such as parsing
     * the geometry and setting the colors.
     */

    if ((entryPtr->state != ckNormalUid)
	    && (entryPtr->state != ckDisabledUid)) {
	Tcl_AppendResult(interp, "bad state value \"", entryPtr->state,
		"\":  must be normal or disabled", (char *) NULL);
	entryPtr->state = ckNormalUid;
	return TCL_ERROR;
    }

    /*
     * Recompute the window's geometry and arrange for it to be
     * redisplayed.
     */

    EntryComputeGeometry(entryPtr);
    entryPtr->flags |= UPDATE_SCROLLBAR;
    EventuallyRedraw(entryPtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DisplayEntry --
 *
 *	This procedure redraws the contents of an entry window.
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
DisplayEntry(clientData)
    ClientData clientData;	/* Information about window. */
{
    register Entry *entryPtr = (Entry *) clientData;
    CkWindow *winPtr = entryPtr->winPtr;
    int y, startX, leftIndex, selectFirst, selectLast, insertPos, dummy;
    char *displayString;

    entryPtr->flags &= ~REDRAW_PENDING;
    if ((entryPtr->winPtr == NULL) || !(winPtr->flags & CK_MAPPED))
	return;

    /*
     * Update the scrollbar if that's needed.
     */

    if (entryPtr->flags & UPDATE_SCROLLBAR) {
	EntryUpdateScrollbar(entryPtr);
    }

    /*
     * Compute x-coordinate of the pixel just after last visible
     * one, plus vertical position of baseline of text.
     */

    y = winPtr->height / 2;

    if (entryPtr->displayString == NULL) {
	displayString = entryPtr->string;
    } else {
	displayString = entryPtr->displayString;
    }

    Ck_SetWindowAttr(winPtr, entryPtr->normalFg, entryPtr->normalBg,
        entryPtr->normalAttr);
    Ck_ClearToBot(winPtr, 0, 0);

#if CK_USE_UTF
    leftIndex = Tcl_UtfAtIndex(displayString, entryPtr->leftIndex) -
	displayString;
    selectFirst = Tcl_UtfAtIndex(displayString, entryPtr->selectFirst) -
	displayString;
    selectLast = Tcl_UtfAtIndex(displayString, entryPtr->selectLast) -
	displayString;
    insertPos = Tcl_UtfAtIndex(displayString, entryPtr->insertPos) -
	displayString;
#else
    leftIndex = entryPtr->leftIndex;
    selectFirst = entryPtr->selectFirst;
    selectLast = entryPtr->selectLast;
    insertPos = entryPtr->insertPos;
#endif

    CkDisplayChars(winPtr->mainPtr, winPtr->window,
	displayString + leftIndex,
	strlen(displayString) - leftIndex,
       	entryPtr->leftX, y, entryPtr->tabOrigin,
	CK_NEWLINES_NOT_SPECIAL);

    if (entryPtr->selectLast >= entryPtr->leftIndex) {
	if (entryPtr->selectFirst < entryPtr->leftIndex) {
	    startX = 0;
        } else {
	    CkMeasureChars(winPtr->mainPtr,
		displayString + leftIndex,
	        selectFirst - leftIndex, entryPtr->leftX,
	        winPtr->width, entryPtr->tabOrigin, CK_NEWLINES_NOT_SPECIAL,
	        &startX, &dummy);
	}
	if (startX < winPtr->width) {
	    Ck_SetWindowAttr(winPtr, entryPtr->selFg, entryPtr->selBg,
		entryPtr->selAttr);
	    wmove(winPtr->window, y, startX + entryPtr->leftX);
	    CkDisplayChars(winPtr->mainPtr, winPtr->window,
		displayString + selectFirst,
		selectLast - selectFirst,
		entryPtr->leftX + startX, y, entryPtr->tabOrigin,
	        CK_NEWLINES_NOT_SPECIAL);
	    Ck_SetWindowAttr(winPtr, entryPtr->normalFg, entryPtr->normalBg,
		entryPtr->normalAttr);
	}
    }

    CkMeasureChars(winPtr->mainPtr, displayString + leftIndex,
    	insertPos - leftIndex, entryPtr->leftX,
    	winPtr->width, entryPtr->tabOrigin, CK_NEWLINES_NOT_SPECIAL,
	&startX, &dummy);

    if (startX >= 0 && startX < winPtr->width) {
        wmove(winPtr->window, y, startX);
	if (entryPtr->state == ckNormalUid)
	    Ck_SetHWCursor(winPtr, 1);
	else
	    Ck_SetHWCursor(winPtr, 0);
    } else {
	wmove(winPtr->window, y, 0);
        Ck_SetHWCursor(winPtr, 0);
    }

    Ck_EventuallyRefresh(winPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * EntryComputeGeometry --
 *
 *	This procedure is invoked to recompute information about where
 *	in its window an entry's string will be displayed.  It also
 *	computes the requested size for the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The tabOrigin fields are recomputed for entryPtr,
 *	and leftIndex may be adjusted.  Ck_GeometryRequest is called
 *	to register the desired dimensions for the window.
 *
 *----------------------------------------------------------------------
 */

static void
EntryComputeGeometry(entryPtr)
    Entry *entryPtr;			/* Widget record for entry. */
{
    int totalLength, overflow, maxOffScreen;
    int width, i, rightX, dummy;
    char *p, *displayString;
    CkWindow *winPtr = entryPtr->winPtr;

    /*
     * If we're displaying a special character instead of the value of
     * the entry, recompute the displayString.
     */

    if (entryPtr->displayString != NULL) {
	ckfree(entryPtr->displayString);
	entryPtr->displayString = NULL;
    }
    if (entryPtr->showChar != NULL) {
#if CK_USE_UTF
	int ulen;

	entryPtr->displayString = (char *) ckalloc(entryPtr->numChars * 3 + 1);
	ulen = Tcl_UtfNext(entryPtr->showChar) - entryPtr->showChar;
	for (p = entryPtr->displayString, i = entryPtr->numChars; i > 0;
		i--) {
	    memcpy(p, entryPtr->showChar, ulen);
	    p += ulen;
	}
#else
	entryPtr->displayString = (char *) ckalloc(entryPtr->numChars + 1);
	for (p = entryPtr->displayString, i = entryPtr->numChars; i > 0;
		i--, p++) {
	    *p = entryPtr->showChar[0];
	}
#endif
	*p = 0;
	displayString = entryPtr->displayString;
    } else {
	displayString = entryPtr->string;
    }

    /*
     * Recompute where the leftmost character on the display will
     * be drawn (entryPtr->leftX) and adjust leftIndex if necessary
     * so that we don't let characters hang off the edge of the
     * window unless the entire window is full.
     */

    CkMeasureChars(winPtr->mainPtr, displayString, strlen(displayString),
	0, INT_MAX, 0,
	CK_NEWLINES_NOT_SPECIAL, &totalLength, &dummy);
    if (entryPtr->insertPos == entryPtr->numChars)
	totalLength += 1;
    overflow = totalLength - entryPtr->winPtr->width;
    if (overflow < 0) {
	entryPtr->leftIndex = 0;
	if (entryPtr->justify == CK_JUSTIFY_LEFT) {
	    entryPtr->leftX = 0;
	} else if (entryPtr->justify == CK_JUSTIFY_RIGHT) {
	    entryPtr->leftX = entryPtr->winPtr->width - totalLength;
	} else {
	    entryPtr->leftX = (entryPtr->winPtr->width - totalLength) / 2;
	}
	entryPtr->tabOrigin = entryPtr->leftX;
    } else {
	int leftIndex;

	/*
	 * The whole string can't fit in the window.  Compute the
	 * maximum number of characters that may be off-screen to
	 * the left without leaving empty space on the right of the
	 * window, then don't let leftIndex be any greater than that.
	 */

        maxOffScreen = CkMeasureChars(winPtr->mainPtr,
	    displayString, strlen(displayString),
            0, overflow, 0, CK_NEWLINES_NOT_SPECIAL|CK_PARTIAL_OK, &rightX,
	    &dummy);
	if (rightX < overflow) {
	    maxOffScreen += 1;
	}
	if (entryPtr->leftIndex > maxOffScreen) {
	    entryPtr->leftIndex = maxOffScreen;
	}
#if CK_USE_UTF
	leftIndex = Tcl_UtfAtIndex(displayString, entryPtr->leftIndex) -
	    displayString;
#else
	leftIndex = entryPtr->leftIndex;
#endif
	CkMeasureChars(winPtr->mainPtr, displayString, leftIndex,
	    0, INT_MAX, 0,
	    CK_NEWLINES_NOT_SPECIAL|CK_PARTIAL_OK, &rightX, &dummy);
	entryPtr->leftX = 0;
	entryPtr->tabOrigin = entryPtr->leftX - rightX;
    }

    if (entryPtr->prefWidth > 0) {
	width = entryPtr->prefWidth;
    } else if (totalLength == 0) {
	width = 1;
    } else {
	width = totalLength;
    }
    Ck_GeometryRequest(entryPtr->winPtr, width, 1);
}

/*
 *----------------------------------------------------------------------
 *
 * InsertChars --
 *
 *	Add new characters to an entry widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	New information gets added to entryPtr;  it will be redisplayed
 *	soon, but not necessarily immediately.
 *
 *----------------------------------------------------------------------
 */

static void
InsertChars(entryPtr, index, string)
    register Entry *entryPtr;	/* Entry that is to get the new
				 * elements. */
    int index;			/* Add the new elements before this
				 * element. */
    char *string;		/* New characters to add (NULL-terminated
				 * string). */
{
    int length, clength;
    char *new;
#if CK_USE_UTF
    int inspos;
#endif

    length = strlen(string);
    if (length == 0) {
	return;
    }
#if CK_USE_UTF
    clength = Tcl_NumUtfChars(string, -1);
    new = (char *) ckalloc((unsigned) (entryPtr->numBytes + length + 1));
    inspos = Tcl_UtfAtIndex(entryPtr->string, index) - entryPtr->string;
    strncpy(new, entryPtr->string, (size_t) inspos);
    strcpy(new+inspos, string);
    strcpy(new+inspos+length, entryPtr->string+inspos);
    ckfree(entryPtr->string);
    entryPtr->string = new;
    entryPtr->numChars += clength;
    entryPtr->numBytes += length;
#else
    clength = length;
    new = (char *) ckalloc((unsigned) (entryPtr->numChars + length + 1));
    strncpy(new, entryPtr->string, (size_t) index);
    strcpy(new+index, string);
    strcpy(new+index+length, entryPtr->string+index);
    ckfree(entryPtr->string);
    entryPtr->string = new;
    entryPtr->numChars += length;
#endif

    /*
     * Inserting characters invalidates all indexes into the string.
     * Touch up the indexes so that they still refer to the same
     * characters (at new positions).  When updating the selection
     * end-points, don't include the new text in the selection unless
     * it was completely surrounded by the selection.
     */

    if (entryPtr->selectFirst >= index) {
	entryPtr->selectFirst += clength;
    }
    if (entryPtr->selectLast > index) {
	entryPtr->selectLast += clength;
    }
    if ((entryPtr->selectAnchor > index) || (entryPtr->selectFirst >= index)) {
	entryPtr->selectAnchor += clength;
    }
    if (entryPtr->leftIndex > index) {
	entryPtr->leftIndex += clength;
    }
    if (entryPtr->insertPos >= index) {
	entryPtr->insertPos += clength;
    }

    if (entryPtr->textVarName != NULL) {
	Tcl_SetVar(entryPtr->interp, entryPtr->textVarName, entryPtr->string,
		TCL_GLOBAL_ONLY);
    }
    entryPtr->flags |= UPDATE_SCROLLBAR;
    EntryComputeGeometry(entryPtr);
    EventuallyRedraw(entryPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteChars --
 *
 *	Remove one or more characters from an entry widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory gets freed, the entry gets modified and (eventually)
 *	redisplayed.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteChars(entryPtr, index, count)
    register Entry *entryPtr;	/* Entry widget to modify. */
    int index;			/* Index of first character to delete. */
    int count;			/* How many characters to delete. */
{
    char *new;
#if CK_USE_UTF
    int delpos, delcount;
#endif

    if ((index + count) > entryPtr->numChars) {
	count = entryPtr->numChars - index;
    }
    if (count <= 0) {
	return;
    }

#if CK_USE_UTF
    delpos = Tcl_UtfAtIndex(entryPtr->string, index) - entryPtr->string;
    delcount = Tcl_UtfAtIndex(entryPtr->string + delpos, count) -
	   	   (entryPtr->string + delpos);
    new = (char *) ckalloc((unsigned) (entryPtr->numBytes + 1 - delcount));
    strncpy(new, entryPtr->string, (size_t) delpos);
    strcpy(new+delpos, entryPtr->string+delpos+delcount);
    entryPtr->numChars = Tcl_NumUtfChars(new, -1);
    entryPtr->numBytes = strlen(new);
#else
    new = (char *) ckalloc((unsigned) (entryPtr->numChars + 1 - count));
    strncpy(new, entryPtr->string, (size_t) index);
    strcpy(new+index, entryPtr->string+index+count);
    entryPtr->numChars -= count;
#endif
    ckfree(entryPtr->string);
    entryPtr->string = new;

    /*
     * Deleting characters results in the remaining characters being
     * renumbered.  Update the various indexes into the string to reflect
     * this change.
     */

    if (entryPtr->selectFirst >= index) {
	if (entryPtr->selectFirst >= (index+count)) {
	    entryPtr->selectFirst -= count;
	} else {
	    entryPtr->selectFirst = index;
	}
    }
    if (entryPtr->selectLast >= index) {
	if (entryPtr->selectLast >= (index+count)) {
	    entryPtr->selectLast -= count;
	} else {
	    entryPtr->selectLast = index;
	}
    }
    if (entryPtr->selectLast <= entryPtr->selectFirst) {
	entryPtr->selectFirst = entryPtr->selectLast = -1;
    }
    if (entryPtr->selectAnchor >= index) {
	if (entryPtr->selectAnchor >= (index+count)) {
	    entryPtr->selectAnchor -= count;
	} else {
	    entryPtr->selectAnchor = index;
	}
    }
    if (entryPtr->leftIndex > index) {
	if (entryPtr->leftIndex >= (index+count)) {
	    entryPtr->leftIndex -= count;
	} else {
	    entryPtr->leftIndex = index;
	}
    }
    if (entryPtr->insertPos >= index) {
	if (entryPtr->insertPos >= (index+count)) {
	    entryPtr->insertPos -= count;
	} else {
	    entryPtr->insertPos = index;
	}
    }

    if (entryPtr->textVarName != NULL) {
	Tcl_SetVar(entryPtr->interp, entryPtr->textVarName, entryPtr->string,
		TCL_GLOBAL_ONLY);
    }
    entryPtr->flags |= UPDATE_SCROLLBAR;
    EntryComputeGeometry(entryPtr);
    EventuallyRedraw(entryPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * EntrySetValue --
 *
 *	Replace the contents of a text entry with a given value.  This
 *	procedure is invoked when updating the entry from the entry's
 *	associated variable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The string displayed in the entry will change.  Any selection
 *	in the entry is lost and the insertion point gets set to the
 *	end of the entry.  Note: this procedure does *not* update the
 *	entry's associated variable, since that could result in an
 *	infinite loop.
 *
 *----------------------------------------------------------------------
 */

static void
EntrySetValue(entryPtr, value)
    register Entry *entryPtr;		/* Entry whose value is to be
					 * changed. */
    char *value;			/* New text to display in entry. */
{
    ckfree(entryPtr->string);
#if CK_USE_UTF
    entryPtr->numBytes = strlen(value);
    entryPtr->numChars = Tcl_NumUtfChars(value, -1);
    entryPtr->string = (char *) ckalloc((unsigned) (entryPtr->numBytes + 1));
#else
    entryPtr->numChars = strlen(value);
    entryPtr->string = (char *) ckalloc((unsigned) (entryPtr->numChars + 1));
#endif
    strcpy(entryPtr->string, value);
    entryPtr->selectFirst = entryPtr->selectLast = -1;
    entryPtr->leftIndex = 0;
    entryPtr->insertPos = entryPtr->numChars;

    entryPtr->flags |= UPDATE_SCROLLBAR;
    EntryComputeGeometry(entryPtr);
    EventuallyRedraw(entryPtr);
}

/*
 *--------------------------------------------------------------
 *
 * EntryEventProc --
 *
 *	This procedure is invoked by the dispatcher for various
 *	events on entryes.
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
EntryEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    CkEvent *eventPtr;		/* Information about event. */
{
    Entry *entryPtr = (Entry *) clientData;

    if (eventPtr->type == CK_EV_EXPOSE) {
	Ck_Preserve((ClientData) entryPtr);
	entryPtr->flags |= UPDATE_SCROLLBAR;
	EntryComputeGeometry(entryPtr);
	EventuallyRedraw(entryPtr);
	Ck_Release((ClientData) entryPtr);
    } else if (eventPtr->type == CK_EV_DESTROY) {
        if (entryPtr->winPtr != NULL) {
            entryPtr->winPtr = NULL;
            Tcl_DeleteCommand(entryPtr->interp,
                    Tcl_GetCommandName(entryPtr->interp, entryPtr->widgetCmd));
        }
	if (entryPtr->flags & REDRAW_PENDING) {
	    Tk_CancelIdleCall(DisplayEntry, (ClientData) entryPtr);
	}
	Ck_EventuallyFree((ClientData) entryPtr, (Ck_FreeProc *) DestroyEntry);
    } else if (eventPtr->type == CK_EV_FOCUSIN) {
	EntryFocusProc(entryPtr, 1);
    } else if (eventPtr->type == CK_EV_FOCUSOUT) {
	EntryFocusProc(entryPtr, 0);
    }
}

/*
 *--------------------------------------------------------------
 *
 * GetEntryIndex --
 *
 *	Parse an index into an entry and return either its value
 *	or an error.
 *
 * Results:
 *	A standard Tcl result.  If all went well, then *indexPtr is
 *	filled in with the index (into entryPtr) corresponding to
 *	string.  The index value is guaranteed to lie between 0 and
 *	the number of characters in the string, inclusive.  If an
 *	error occurs then an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
GetEntryIndex(interp, entryPtr, string, indexPtr)
    Tcl_Interp *interp;		/* For error messages. */
    Entry *entryPtr;		/* Entry for which the index is being
				 * specified. */
    char *string;		/* Specifies character in entryPtr. */
    int *indexPtr;		/* Where to store converted index. */
{
    size_t length;
    int dummy;
    CkWindow *winPtr = entryPtr->winPtr;

    length = strlen(string);

    if (string[0] == 'a') {
	if (strncmp(string, "anchor", length) == 0) {
	    *indexPtr = entryPtr->selectAnchor;
	} else {
	    badIndex:

	    /*
	     * Some of the paths here leave messages in interp->result,
	     * so we have to clear it out before storing our own message.
	     */

	    Tcl_SetResult(interp, (char *) NULL, TCL_STATIC);
	    Tcl_AppendResult(interp, "bad entry index \"", string,
		    "\"", (char *) NULL);
	    return TCL_ERROR;
	}
    } else if (string[0] == 'e') {
	if (strncmp(string, "end", length) == 0) {
	    *indexPtr = entryPtr->numChars;
	} else {
	    goto badIndex;
	}
    } else if (string[0] == 'i') {
	if (strncmp(string, "insert", length) == 0) {
	    *indexPtr = entryPtr->insertPos;
	} else {
	    goto badIndex;
	}
    } else if (string[0] == 's') {
	if (entryPtr->selectFirst == -1) {
	    interp->result = "selection isn't in entry";
	    return TCL_ERROR;
	}
	if (length < 5) {
	    goto badIndex;
	}
	if (strncmp(string, "sel.first", length) == 0) {
	    *indexPtr = entryPtr->selectFirst;
	} else if (strncmp(string, "sel.last", length) == 0) {
	    *indexPtr = entryPtr->selectLast;
	} else {
	    goto badIndex;
	}
    } else if (string[0] == '@') {
        int x, roundUp;

        if (Tcl_GetInt(interp, string+1, &x) != TCL_OK) {
            goto badIndex;
        }
        if (x < 0) {
            x = 0;
        }
        roundUp = 0;
        if (x >= entryPtr->winPtr->width) {
            x = entryPtr->winPtr->width - 1;
            roundUp = 1;
        }
        if (entryPtr->numChars == 0) {
            *indexPtr = 0;
        } else {
	    char *string = (entryPtr->displayString == NULL) ?
		entryPtr->string : entryPtr->displayString;

            *indexPtr = CkMeasureChars(winPtr->mainPtr, string, strlen(string),
	       	entryPtr->tabOrigin, x,
                entryPtr->tabOrigin, CK_NEWLINES_NOT_SPECIAL, &dummy, &dummy);
#if 0
            *indexPtr = entryPtr->leftX + entryPtr->leftIndex + x;
#endif
        }
        if (*indexPtr >= entryPtr->numChars)
            *indexPtr = entryPtr->numChars;
    } else {
	if (Tcl_GetInt(interp, string, indexPtr) != TCL_OK) {
	    goto badIndex;
	}
	if (*indexPtr < 0){
	    *indexPtr = 0;
	} else if (*indexPtr > entryPtr->numChars) {
	    *indexPtr = entryPtr->numChars;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * EntrySelectTo --
 *
 *	Modify the selection by moving its un-anchored end.  This could
 *	make the selection either larger or smaller.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */

static void
EntrySelectTo(entryPtr, index)
    register Entry *entryPtr;		/* Information about widget. */
    int index;				/* Index of element that is to
					 * become the "other" end of the
					 * selection. */
{
    int newFirst, newLast;

    /*
     * Pick new starting and ending points for the selection.
     */

    if (entryPtr->selectAnchor > entryPtr->numChars) {
	entryPtr->selectAnchor = entryPtr->numChars;
    }
    if (entryPtr->selectAnchor <= index) {
	newFirst = entryPtr->selectAnchor;
	newLast = index;
    } else {
	newFirst = index;
	newLast = entryPtr->selectAnchor;
	if (newLast < 0) {
	    newFirst = newLast = -1;
	}
    }
    if ((entryPtr->selectFirst == newFirst)
	    && (entryPtr->selectLast == newLast)) {
	return;
    }
    entryPtr->selectFirst = newFirst;
    entryPtr->selectLast = newLast;
    EventuallyRedraw(entryPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * EventuallyRedraw --
 *
 *	Ensure that an entry is eventually redrawn on the display.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets redisplayed.  Right now we don't do selective
 *	redisplays:  the whole window will be redrawn.  This doesn't
 *	seem to hurt performance noticeably, but if it does then this
 *	could be changed.
 *
 *----------------------------------------------------------------------
 */

static void
EventuallyRedraw(entryPtr)
    Entry *entryPtr;		/* Information about widget. */
{
    if ((entryPtr->winPtr == NULL) || !(entryPtr->winPtr->flags & CK_MAPPED)) {
	return;
    }

    /*
     * Right now we don't do selective redisplays:  the whole window
     * will be redrawn.  This doesn't seem to hurt performance noticeably,
     * but if it does then this could be changed.
     */

    if (!(entryPtr->flags & REDRAW_PENDING)) {
	entryPtr->flags |= REDRAW_PENDING;
	Tk_DoWhenIdle(DisplayEntry, (ClientData) entryPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * EntryVisibleRange --
 *
 *	Return information about the range of the entry that is
 *	currently visible.
 *
 * Results:
 *	*firstPtr and *lastPtr are modified to hold fractions between
 *	0 and 1 identifying the range of characters visible in the
 *	entry.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
EntryVisibleRange(entryPtr, firstPtr, lastPtr)
    Entry *entryPtr;			/* Information about widget. */
    double *firstPtr;			/* Return position of first visible
					 * character in widget. */
    double *lastPtr;			/* Return position of char just after
					 * last visible one. */
{
    char *displayString;
    int charsInWindow, endX, dummy;
    CkWindow *winPtr = entryPtr->winPtr;

    if (entryPtr->displayString == NULL) {
	displayString = entryPtr->string;
    } else {
	displayString = entryPtr->displayString;
    }
    if (entryPtr->numChars == 0) {
	*firstPtr = 0.0;
	*lastPtr = 1.0;
    } else {
	int leftIndex, total;

#if CK_USE_UTF
	leftIndex = Tcl_UtfAtIndex(displayString, entryPtr->leftIndex) -
	    displayString;
	total = entryPtr->numBytes - leftIndex;
#else
	leftIndex = entryPtr->leftIndex;
	total = entryPtr->numChars - leftIndex;
#endif
	charsInWindow = CkMeasureChars(winPtr->mainPtr,
	    displayString + leftIndex, total, 0,
	    entryPtr->winPtr->width, 0,
	    CK_AT_LEAST_ONE|CK_NEWLINES_NOT_SPECIAL, &endX, &dummy);
	*firstPtr = ((double) leftIndex)/entryPtr->numChars;
	*lastPtr = ((double) (leftIndex + charsInWindow))
		/entryPtr->numChars;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * EntryUpdateScrollbar --
 *
 *	This procedure is invoked whenever information has changed in
 *	an entry in a way that would invalidate a scrollbar display.
 *	If there is an associated scrollbar, then this procedure updates
 *	it by invoking a Tcl command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A Tcl command is invoked, and an additional command may be
 *	invoked to process errors in the command.
 *
 *----------------------------------------------------------------------
 */

static void
EntryUpdateScrollbar(entryPtr)
    Entry *entryPtr;			/* Information about widget. */
{
    char args[100];
    int code;
    double first, last;

    if (entryPtr->scrollCmd == NULL) {
	return;
    }

    EntryVisibleRange(entryPtr, &first, &last);
    sprintf(args, " %g %g", first, last);
    code = Tcl_VarEval(entryPtr->interp, entryPtr->scrollCmd, args,
	    (char *) NULL);
    if (code != TCL_OK) {
	Tcl_AddErrorInfo(entryPtr->interp,
		"\n    (horizontal scrolling command executed by entry)");
	Tk_BackgroundError(entryPtr->interp);
    }
    Tcl_SetResult(entryPtr->interp, (char *) NULL, TCL_STATIC);
}

/*
 *----------------------------------------------------------------------
 *
 * EntryFocusProc --
 *
 *	This procedure is called whenever the entry gets or loses the
 *	input focus.  It's also called whenever the window is reconfigured
 *	while it has the focus.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cursor gets turned on or off.
 *
 *----------------------------------------------------------------------
 */

static void
EntryFocusProc(entryPtr, gotFocus)
    Entry *entryPtr;     	/* Entry that got or lost focus. */
    int gotFocus;		/* 1 means window is getting focus, 0 means
				 * it's losing it. */
{
    if (gotFocus)
	entryPtr->flags |= GOT_FOCUS;
    else
	entryPtr->flags &= ~GOT_FOCUS;
}

/*
 *--------------------------------------------------------------
 *
 * EntryTextVarProc --
 *
 *	This procedure is invoked when someone changes the variable
 *	whose contents are to be displayed in an entry.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The text displayed in the entry will change to match the
 *	variable.
 *
 *--------------------------------------------------------------
 */

static char *
EntryTextVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about button. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Not used. */
    char *name2;		/* Not used. */
    int flags;			/* Information about what happened. */
{
    register Entry *entryPtr = (Entry *) clientData;
    char *value;

    /*
     * If the variable is unset, then immediately recreate it unless
     * the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    Tcl_SetVar(interp, entryPtr->textVarName, entryPtr->string,
		    TCL_GLOBAL_ONLY);
	    Tcl_TraceVar(interp, entryPtr->textVarName,
		    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    EntryTextVarProc, clientData);
	}
	return (char *) NULL;
    }

    /*
     * Update the entry's text with the value of the variable, unless
     * the entry already has that value (this happens when the variable
     * changes value because we changed it because someone typed in
     * the entry).
     */

    value = Tcl_GetVar(interp, entryPtr->textVarName, TCL_GLOBAL_ONLY);
    if (value == NULL) {
	value = "";
    }
    if (strcmp(value, entryPtr->string) != 0) {
	EntrySetValue(entryPtr, value);
    }
    return (char *) NULL;
}




