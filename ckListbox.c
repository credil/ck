/* 
 * ckListbox.c --
 *
 *	This module implements listbox widgets for the
 *	toolkit.  A listbox displays a collection of strings,
 *	one per line, and provides scrolling and selection.
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
 * One record of the following type is kept for each element
 * associated with a listbox widget:
 */

typedef struct Element {
    int textLength;		/* # non-NULL characters in text. */
    int textWidth;		/* Total width of element in screen
    				 * characters. */
    int selected;		/* 1 means this item is selected, 0 means
				 * it isn't. */
    struct Element *nextPtr;	/* Next in list of all elements of this
				 * listbox, or NULL for last element. */
    char text[4];		/* Characters of this element, NULL-
				 * terminated.  The actual space allocated
				 * here will be as large as needed (> 4,
				 * most likely).  Must be the last field
				 * of the record. */
} Element;

#define ElementSize(stringLength) \
	(sizeof(Element) - 3 + stringLength)

/*
 * A data structure of the following type is kept for each listbox
 * widget managed by this file:
 */

typedef struct {
    CkWindow *winPtr;		/* Window that embodies the listbox.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Tcl_Interp *interp;		/* Interpreter associated with listbox. */
    Tcl_Command widgetCmd;      /* Token for listbox's widget command. */
    int numElements;		/* Total number of elements in this listbox. */
    Element *firstPtr;		/* First in list of elements (NULL if no
				 * elements). */
    Element *lastPtr;		/* Last in list of elements (NULL if no
				 * elements). */

    /*
     * Information used when displaying widget:
     */

    int normalBg;               /* Normal background color. */
    int normalFg;               /* Normal foreground color. */
    int normalAttr;             /* Normal video attributes. */
    int selBg;                  /* Select background color. */
    int selFg;                  /* Select foreground color. */
    int selAttr;                /* Select video attributes. */
    int activeBg;               /* Active background color. */
    int activeFg;               /* Active foreground color. */
    int activeAttr;		/* Video attribute for active item. */
    int width;			/* Desired width of window, in characters. */
    int height;			/* Desired height of window, in lines. */
    int topIndex;		/* Index of top-most element visible in
				 * window. */
    int fullLines;		/* Number of lines that fit are completely
				 * visible in window.  There may be one
				 * additional line at the bottom that is
				 * partially visible. */

    /*
     * Information to support horizontal scrolling:
     */

    int maxWidth;		/* Width of widest string in listbox. */
    int xOffset;		/* The left edge of each string in the
				 * listbox is offset to the left by this
				 * many chars (0 means no offset, positive
				 * means there is an offset). */

    /*
     * Information about what's selected or active, if any.
     */

    Ck_Uid selectMode;		/* Selection style: single, browse, multiple,
				 * or extended.  This value isn't used in C
				 * code, but the Tcl bindings use it. */
    int numSelected;		/* Number of elements currently selected. */
    int selectAnchor;		/* Fixed end of selection (i.e. element
				 * at which selection was started.) */
    int active;			/* Index of "active" element (the one that
				 * has been selected by keyboard traversal).
				 * -1 means none. */

    /*
     * Miscellaneous information:
     */

    char *takeFocus;		/* Value of -takefocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
    char *yScrollCmd;		/* Command prefix for communicating with
				 * vertical scrollbar.  NULL means no command
				 * to issue.  Malloc'ed. */
    char *xScrollCmd;		/* Command prefix for communicating with
				 * horizontal scrollbar.  NULL means no command
				 * to issue.  Malloc'ed. */
    int flags;			/* Various flag bits:  see below for
				 * definitions. */
} Listbox;

/*
 * Flag bits for listboxes:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 * UPDATE_V_SCROLLBAR:		Non-zero means vertical scrollbar needs
 *				to be updated.
 * UPDATE_H_SCROLLBAR:		Non-zero means horizontal scrollbar needs
 *				to be updated.
 * GOT_FOCUS:			Non-zero means this widget currently
 *				has the input focus.
 */

#define REDRAW_PENDING		1
#define UPDATE_V_SCROLLBAR	2
#define UPDATE_H_SCROLLBAR	4
#define GOT_FOCUS		8

/*
 * Information used for argv parsing:
 */

static Ck_ConfigSpec configSpecs[] = {
    {CK_CONFIG_ATTR, "-activeattributes", "activeAttributes",
        "ActiveAttributes", DEF_LISTBOX_ACTIVE_ATTR_COLOR,
        Ck_Offset(Listbox, activeAttr), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_ATTR, "-activeattributes", "activeAttributes",
        "ActiveAttributes", DEF_LISTBOX_ACTIVE_ATTR_MONO,
        Ck_Offset(Listbox, activeAttr), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-activebackground", "activeBackground", "Foreground",
	DEF_LISTBOX_ACTIVE_BG_COLOR, Ck_Offset(Listbox, activeBg),
        CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-activebackground", "activeBackground", "Foreground",
	DEF_LISTBOX_ACTIVE_BG_MONO, Ck_Offset(Listbox, activeBg),
        CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_LISTBOX_ACTIVE_FG_COLOR, Ck_Offset(Listbox, activeFg),
        CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_LISTBOX_ACTIVE_FG_MONO, Ck_Offset(Listbox, activeFg),
        CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_ATTR, "-attributes", "attributes", "Attributes",
	DEF_LISTBOX_ATTR, Ck_Offset(Listbox, normalAttr), 0},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_LISTBOX_BG_COLOR, Ck_Offset(Listbox, normalBg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_LISTBOX_BG_MONO, Ck_Offset(Listbox, normalBg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_LISTBOX_FG, Ck_Offset(Listbox, normalFg), 0},
    {CK_CONFIG_INT, "-height", "height", "Height",
	DEF_LISTBOX_HEIGHT, Ck_Offset(Listbox, height), 0},
    {CK_CONFIG_ATTR, "-selectattributes", "selectAttributes",
        "SelectAttributes", DEF_LISTBOX_SELECT_ATTR_COLOR,
        Ck_Offset(Listbox, selAttr), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_ATTR, "-selectattributes", "selectAttributes",
        "SelectAttributes", DEF_LISTBOX_SELECT_ATTR_MONO,
        Ck_Offset(Listbox, selAttr), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-selectbackground", "selectBackground", "Foreground",
	DEF_LISTBOX_SELECT_BG_COLOR, Ck_Offset(Listbox, selBg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-selectbackground", "selectBackground", "Foreground",
	DEF_LISTBOX_SELECT_BG_MONO, Ck_Offset(Listbox, selBg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_LISTBOX_SELECT_FG_COLOR, Ck_Offset(Listbox, selFg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_LISTBOX_SELECT_FG_MONO, Ck_Offset(Listbox, selFg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_UID, "-selectmode", "selectMode", "SelectMode",
	DEF_LISTBOX_SELECT_MODE, Ck_Offset(Listbox, selectMode), 0},
    {CK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_LISTBOX_TAKE_FOCUS, Ck_Offset(Listbox, takeFocus),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_INT, "-width", "width", "Width",
	DEF_LISTBOX_WIDTH, Ck_Offset(Listbox, width), 0},
    {CK_CONFIG_STRING, "-xscrollcommand", "xScrollCommand", "ScrollCommand",
	DEF_LISTBOX_SCROLL_COMMAND, Ck_Offset(Listbox, xScrollCmd),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-yscrollcommand", "yScrollCommand", "ScrollCommand",
	DEF_LISTBOX_SCROLL_COMMAND, Ck_Offset(Listbox, yScrollCmd),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		ChangeListboxOffset _ANSI_ARGS_((Listbox *listPtr,
			    int offset));
static void		ChangeListboxView _ANSI_ARGS_((Listbox *listPtr,
			    int index));
static int		ConfigureListbox _ANSI_ARGS_((Tcl_Interp *interp,
			    Listbox *listPtr, int argc, char **argv,
			    int flags));
static void		DeleteEls _ANSI_ARGS_((Listbox *listPtr, int first,
			    int last));
static void		DestroyListbox _ANSI_ARGS_((ClientData clientData));
static void		DisplayListbox _ANSI_ARGS_((ClientData clientData));
static int		GetListboxIndex _ANSI_ARGS_((Tcl_Interp *interp,
			    Listbox *listPtr, char *string, int numElsOK,
			    int *indexPtr));
static void		InsertEls _ANSI_ARGS_((Listbox *listPtr, int index,
			    int argc, char **argv));
static void		ListboxCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static void		ListboxComputeGeometry _ANSI_ARGS_((Listbox *listPtr));
static void		ListboxEventProc _ANSI_ARGS_((ClientData clientData,
			    CkEvent *eventPtr));
static void		ListboxRedrawRange _ANSI_ARGS_((Listbox *listPtr,
			    int first, int last));
static void		ListboxSelect _ANSI_ARGS_((Listbox *listPtr,
			    int first, int last, int select));
static void		ListboxUpdateHScrollbar _ANSI_ARGS_((Listbox *listPtr));
static void		ListboxUpdateVScrollbar _ANSI_ARGS_((Listbox *listPtr));
static int		ListboxWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static int		NearestListboxElement _ANSI_ARGS_((Listbox *listPtr,
			    int y));

/*
 *--------------------------------------------------------------
 *
 * Ck_ListboxCmd --
 *
 *	This procedure is invoked to process the "listbox" Tcl
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
Ck_ListboxCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    register Listbox *listPtr;
    CkWindow *new;
    CkWindow *mainPtr = (CkWindow *) clientData;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    new = Ck_CreateWindowFromPath(interp, mainPtr, argv[1], 0);
    if (new == NULL) {
	return TCL_ERROR;
    }

    /*
     * Initialize the fields of the structure that won't be initialized
     * by ConfigureListbox, or that ConfigureListbox requires to be
     * initialized already (e.g. resource pointers).
     */

    listPtr = (Listbox *) ckalloc(sizeof(Listbox));
    listPtr->winPtr = new;
    listPtr->interp = interp;
    listPtr->widgetCmd = Tcl_CreateCommand(interp, listPtr->winPtr->pathName,
        ListboxWidgetCmd, (ClientData) listPtr, ListboxCmdDeletedProc);
    listPtr->numElements = 0;
    listPtr->firstPtr = NULL;
    listPtr->lastPtr = NULL;
    listPtr->normalBg = 0;
    listPtr->normalFg = 0;
    listPtr->normalAttr = 0;
    listPtr->selBg = 0;
    listPtr->selFg = 0;
    listPtr->selAttr = 0;
    listPtr->activeBg = 0;
    listPtr->activeFg = 0;
    listPtr->activeAttr = 0;
    listPtr->width = 0;
    listPtr->height = 0;
    listPtr->topIndex = 0;
    listPtr->fullLines = 1;
    listPtr->maxWidth = 0;
    listPtr->xOffset = 0;
    listPtr->selectMode = NULL;
    listPtr->numSelected = 0;
    listPtr->selectAnchor = 0;
    listPtr->active = 0;
    listPtr->takeFocus = NULL;
    listPtr->xScrollCmd = NULL;
    listPtr->yScrollCmd = NULL;
    listPtr->flags = 0;

    Ck_SetClass(listPtr->winPtr, "Listbox");
    Ck_CreateEventHandler(listPtr->winPtr,
	    CK_EV_EXPOSE | CK_EV_MAP | CK_EV_DESTROY |
	    CK_EV_FOCUSIN | CK_EV_FOCUSOUT,
	    ListboxEventProc, (ClientData) listPtr);
    if (ConfigureListbox(interp, listPtr, argc-2, argv+2, 0) != TCL_OK) {
	goto error;
    }

    interp->result = listPtr->winPtr->pathName;
    return TCL_OK;

    error:
    Ck_DestroyWindow(listPtr->winPtr);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * ListboxWidgetCmd --
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
ListboxWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Information about listbox widget. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    register Listbox *listPtr = (Listbox *) clientData;
    int result = TCL_OK;
    size_t length;
    int c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Ck_Preserve((ClientData) listPtr);
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'a') && (strncmp(argv[1], "activate", length) == 0)) {
	int index;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " activate index\"",
		    (char *) NULL);
	    goto error;
	}
	ListboxRedrawRange(listPtr, listPtr->active, listPtr->active);
	if (GetListboxIndex(interp, listPtr, argv[2], 0, &index)
		!= TCL_OK) {
	    goto error;
	}
	listPtr->active = index;
	ListboxRedrawRange(listPtr, listPtr->active, listPtr->active);
    } else if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	&& (length >= 2)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "wrong # args: should be \"",
                    argv[0], " cget option\"",
                    (char *) NULL);
            goto error;
        }
        result = Ck_ConfigureValue(interp, listPtr->winPtr, configSpecs,
                (char *) listPtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 2)) {
	if (argc == 2) {
	    result = Ck_ConfigureInfo(interp, listPtr->winPtr, configSpecs,
		    (char *) listPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Ck_ConfigureInfo(interp, listPtr->winPtr, configSpecs,
		    (char *) listPtr, argv[2], 0);
	} else {
	    result = ConfigureListbox(interp, listPtr, argc-2, argv+2,
		    CK_CONFIG_ARGV_ONLY);
	}
    } else if ((c == 'c') && (strncmp(argv[1], "curselection", length) == 0)
	    && (length >= 2)) {
	int i, count;
	char index[20];
	Element *elPtr;

	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " curselection\"",
		    (char *) NULL);
	    goto error;
	}
	count = 0;
	for (i = 0, elPtr = listPtr->firstPtr; elPtr != NULL;
		i++, elPtr = elPtr->nextPtr) {
	    if (elPtr->selected) {
		sprintf(index, "%d", i);
		Tcl_AppendElement(interp, index);
		count++;
	    }
	}
	if (count != listPtr->numSelected) {
	    panic("ListboxWidgetCmd: selection count incorrect");
	}
    } else if ((c == 'd') && (strncmp(argv[1], "delete", length) == 0)) {
	int first, last;

	if ((argc < 3) || (argc > 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " delete firstIndex ?lastIndex?\"",
		    (char *) NULL);
	    goto error;
	}
	if (GetListboxIndex(interp, listPtr, argv[2], 0, &first) != TCL_OK) {
	    goto error;
	}
	if (argc == 3) {
	    last = first;
	} else {
	    if (GetListboxIndex(interp, listPtr, argv[3], 0, &last) != TCL_OK) {
		goto error;
	    }
	}
	DeleteEls(listPtr, first, last);
    } else if ((c == 'g') && (strncmp(argv[1], "get", length) == 0)) {
	int first, last, i;
	Element *elPtr;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " get first ?last?\"", (char *) NULL);
	    goto error;
	}
	if (GetListboxIndex(interp, listPtr, argv[2], 0, &first) != TCL_OK) {
	    goto error;
	}
	if ((argc == 4) && (GetListboxIndex(interp, listPtr, argv[3],
		0, &last) != TCL_OK)) {
	    goto error;
	}
	for (elPtr = listPtr->firstPtr, i = 0; i < first;
		i++, elPtr = elPtr->nextPtr) {
	    /* Empty loop body. */
	}
	if (elPtr != NULL) {
	    if (argc == 3) {
		interp->result = elPtr->text;
	    } else {
		for (  ; i <= last; i++, elPtr = elPtr->nextPtr) {
		    Tcl_AppendElement(interp, elPtr->text);
		}
	    }
	}
    } else if ((c == 'i') && (strncmp(argv[1], "index", length) == 0)
	    && (length >= 3)) {
	int index;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " index index\"",
		    (char *) NULL);
	    goto error;
	}
	if (GetListboxIndex(interp, listPtr, argv[2], 1, &index)
		!= TCL_OK) {
	    goto error;
	}
	sprintf(interp->result, "%d", index);
    } else if ((c == 'i') && (strncmp(argv[1], "insert", length) == 0)
	    && (length >= 3)) {
	int index;

	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " insert index ?element element ...?\"",
		    (char *) NULL);
	    goto error;
	}
	if (GetListboxIndex(interp, listPtr, argv[2], 1, &index)
		!= TCL_OK) {
	    goto error;
	}
	InsertEls(listPtr, index, argc-3, argv+3);
    } else if ((c == 'n') && (strncmp(argv[1], "nearest", length) == 0)) {
	int index, y;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " nearest y\"", (char *) NULL);
	    goto error;
	}
	if (Tcl_GetInt(interp, argv[2], &y) != TCL_OK) {
	    goto error;
	}
	index = NearestListboxElement(listPtr, y);
	sprintf(interp->result, "%d", index);
    } else if ((c == 's') && (strncmp(argv[1], "see", length) == 0)
	    && (length >= 3)) {
	int index, diff;
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " see index\"",
		    (char *) NULL);
	    goto error;
	}
	if (GetListboxIndex(interp, listPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	diff = listPtr->topIndex-index;
	if (diff > 0) {
	    if (diff <= (listPtr->fullLines/3)) {
		ChangeListboxView(listPtr, index);
	    } else {
		ChangeListboxView(listPtr, index - (listPtr->fullLines-1)/2);
	    }
	} else {
	    diff = index - (listPtr->topIndex + listPtr->fullLines - 1);
	    if (diff > 0) {
		if (diff <= (listPtr->fullLines/3)) {
		    ChangeListboxView(listPtr, listPtr->topIndex + diff);
		} else {
		    ChangeListboxView(listPtr,
			    index - (listPtr->fullLines-1)/2);
		}
	    }
	}
    } else if ((c == 's') && (length >= 3)
	    && (strncmp(argv[1], "selection", length) == 0)) {
	int first, last;

	if ((argc != 4) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " selection option index ?index?\"",
		    (char *) NULL);
	    goto error;
	}
	if (GetListboxIndex(interp, listPtr, argv[3], 0, &first) != TCL_OK) {
	    goto error;
	}
	if (argc == 5) {
	    if (GetListboxIndex(interp, listPtr, argv[4], 0, &last) != TCL_OK) {
		goto error;
	    }
	} else {
	    last = first;
	}
	length = strlen(argv[2]);
	c = argv[2][0];
	if ((c == 'a') && (strncmp(argv[2], "anchor", length) == 0)) {
	    if (argc != 4) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " selection anchor index\"", (char *) NULL);
		goto error;
	    }
	    listPtr->selectAnchor = first;
	} else if ((c == 'c') && (strncmp(argv[2], "clear", length) == 0)) {
	    ListboxSelect(listPtr, first, last, 0);
	} else if ((c == 'i') && (strncmp(argv[2], "includes", length) == 0)) {
	    int i;
	    Element *elPtr;
    
	    if (argc != 4) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
			argv[0], " selection includes index\"", (char *) NULL);
		goto error;
	    }
	    for (elPtr = listPtr->firstPtr, i = 0; i < first;
		    i++, elPtr = elPtr->nextPtr) {
		/* Empty loop body. */
	    }
	    if ((elPtr != NULL) && (elPtr->selected)) {
		interp->result = "1";
	    } else {
		interp->result = "0";
	    }
	} else if ((c == 's') && (strncmp(argv[2], "set", length) == 0)) {
	    ListboxSelect(listPtr, first, last, 1);
	} else {
	    Tcl_AppendResult(interp, "bad selection option \"", argv[2],
		    "\": must be anchor, clear, includes, or set",
		    (char *) NULL);
	    goto error;
	}
    } else if ((c == 's') && (length >= 2)
	    && (strncmp(argv[1], "size", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " size\"", (char *) NULL);
	    goto error;
	}
	sprintf(interp->result, "%d", listPtr->numElements);
    } else if ((c == 'x') && (strncmp(argv[1], "xview", length) == 0)) {
	int index, count, type, windowWidth;
	int offset = 0;		/* Initialized to stop gcc warnings. */
	double fraction, fraction2;

	windowWidth = listPtr->winPtr->width;
	if (argc == 2) {
	    if (listPtr->maxWidth == 0) {
		interp->result = "0 1";
	    } else {
		fraction = listPtr->xOffset/((double) listPtr->maxWidth);
		fraction2 = (listPtr->xOffset + windowWidth)
			/((double) listPtr->maxWidth);
		if (fraction2 > 1.0) {
		    fraction2 = 1.0;
		}
		sprintf(interp->result, "%g %g", fraction, fraction2);
	    }
	} else if (argc == 3) {
	    if (Tcl_GetInt(interp, argv[2], &index) != TCL_OK) {
		goto error;
	    }
	    ChangeListboxOffset(listPtr, index);
	} else {
	    type = Ck_GetScrollInfo(interp, argc, argv, &fraction, &count);
	    switch (type) {
		case CK_SCROLL_ERROR:
		    goto error;
		case CK_SCROLL_MOVETO:
		    offset = (int) fraction*listPtr->maxWidth;
		    break;
		case CK_SCROLL_PAGES:
		    offset = listPtr->xOffset + count * windowWidth;
		    break;
		case CK_SCROLL_UNITS:
		    offset = listPtr->xOffset + count;
		    break;
	    }
	    ChangeListboxOffset(listPtr, offset);
	}
    } else if ((c == 'y') && (strncmp(argv[1], "yview", length) == 0)) {
	int index, count, type;
	double fraction, fraction2;

	if (argc == 2) {
	    if (listPtr->numElements == 0) {
		interp->result = "0 1";
	    } else {
		fraction = listPtr->topIndex/((double) listPtr->numElements);
		fraction2 = (listPtr->topIndex+listPtr->fullLines)
			/((double) listPtr->numElements);
		if (fraction2 > 1.0) {
		    fraction2 = 1.0;
		}
		sprintf(interp->result, "%g %g", fraction, fraction2);
	    }
	} else if (argc == 3) {
	    if (GetListboxIndex(interp, listPtr, argv[2], 0, &index)
		    != TCL_OK) {
		goto error;
	    }
	    ChangeListboxView(listPtr, index);
	} else {
	    type = Ck_GetScrollInfo(interp, argc, argv, &fraction, &count);
	    switch (type) {
		case CK_SCROLL_ERROR:
		    goto error;
		case CK_SCROLL_MOVETO:
		    index = (int) (listPtr->numElements * fraction);
		    break;
		case CK_SCROLL_PAGES:
		    if (listPtr->fullLines > 2) {
			index = listPtr->topIndex
				+ count * (listPtr->fullLines - 2);
		    } else {
			index = listPtr->topIndex + count;
		    }
		    break;
		case CK_SCROLL_UNITS:
		    index = listPtr->topIndex + count;
		    break;
	    }
	    ChangeListboxView(listPtr, index);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be activate, cget, configure, ",
		"curselection, delete, get, index, insert, nearest, ",
		"see, selection, size, ",
		"xview, or yview", (char *) NULL);
	goto error;
    }
    Ck_Release((ClientData) listPtr);
    return result;

    error:
    Ck_Release((ClientData) listPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyListbox --
 *
 *	This procedure is invoked by Ck_EventuallyFree or Ck_Release
 *	to clean up the internal structure of a listbox at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the listbox is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyListbox(clientData)
    ClientData clientData;	/* Info about listbox widget. */
{
    register Listbox *listPtr = (Listbox *) clientData;
    register Element *elPtr, *nextPtr;

    /*
     * Free up all of the list elements.
     */

    for (elPtr = listPtr->firstPtr; elPtr != NULL; ) {
	nextPtr = elPtr->nextPtr;
	ckfree((char *) elPtr);
	elPtr = nextPtr;
    }

    Ck_FreeOptions(configSpecs, (char *) listPtr, 0);
    ckfree((char *) listPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ListboxCmdDeletedProc --
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
ListboxCmdDeletedProc(clientData)
    ClientData clientData;      /* Pointer to widget record for widget. */
{
    Listbox *listPtr = (Listbox *) clientData;
    CkWindow *winPtr = listPtr->winPtr;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case winPtr
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (winPtr != NULL) {
        listPtr->winPtr = NULL;
        Ck_DestroyWindow(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureListbox --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the option database, in order to configure (or reconfigure)
 *	a listbox widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as colors, border width,
 *	etc. get set for listPtr;  old resources get freed,
 *	if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureListbox(interp, listPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    register Listbox *listPtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Ck_ConfigureWidget. */
{
    if (Ck_ConfigureWidget(interp, listPtr->winPtr, configSpecs,
	    argc, argv, (char *) listPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Register the desired geometry for the window and arrange for
     * the window to be redisplayed.
     */

    ListboxComputeGeometry(listPtr);
    listPtr->flags |= UPDATE_V_SCROLLBAR|UPDATE_H_SCROLLBAR;
    ListboxRedrawRange(listPtr, 0, listPtr->numElements-1);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DisplayListbox --
 *
 *	This procedure redraws the contents of a listbox window.
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
DisplayListbox(clientData)
    ClientData clientData;	/* Information about window. */
{
    Listbox *listPtr = (Listbox *) clientData;
    CkWindow *winPtr = listPtr->winPtr;
    Element *elPtr;
    int i, limit, y, width, cursorY;

    listPtr->flags &= ~REDRAW_PENDING;
    if (listPtr->flags & UPDATE_V_SCROLLBAR) {
	ListboxUpdateVScrollbar(listPtr);
    }
    if (listPtr->flags & UPDATE_H_SCROLLBAR) {
	ListboxUpdateHScrollbar(listPtr);
    }
    listPtr->flags &= ~(REDRAW_PENDING|UPDATE_V_SCROLLBAR|UPDATE_H_SCROLLBAR);
    if ((listPtr->winPtr == NULL) || !(winPtr->flags & CK_MAPPED)) {
	return;
    }

    Ck_SetWindowAttr(winPtr, listPtr->normalFg, listPtr->normalBg,
        listPtr->normalAttr);
    Ck_ClearToBot(winPtr, 0, 0);

    /*
     * Iterate through all of the elements of the listbox, displaying each
     * in turn.  Selected elements use a different fg/bg/attr.
     */

    limit = listPtr->topIndex + listPtr->fullLines;
    if (limit > listPtr->numElements) {
	limit = listPtr->numElements;
    }
    width = listPtr->xOffset + winPtr->width;
    for (elPtr = listPtr->firstPtr, i = 0, y = cursorY = 0;
            (elPtr != NULL) && (i < limit);
	    elPtr = elPtr->nextPtr, i++) {
	if (i < listPtr->topIndex) {
	    continue;
	}
	if (i == listPtr->active && (listPtr->flags & GOT_FOCUS)) {
	    cursorY = y;
	    Ck_SetWindowAttr(winPtr, listPtr->activeFg, listPtr->activeBg,
        	listPtr->activeAttr |
        	(elPtr->selected ? listPtr->selAttr : 0));
	} else if (elPtr->selected) {
	    Ck_SetWindowAttr(winPtr, listPtr->selFg, listPtr->selBg,
        	listPtr->selAttr);
        } else {
	    Ck_SetWindowAttr(winPtr, listPtr->normalFg, listPtr->normalBg,
        	listPtr->normalAttr);
        }
#if CK_USE_UTF
	if (listPtr->xOffset < elPtr->textWidth) {
	    char *p = Tcl_UtfAtIndex(elPtr->text, listPtr->xOffset);

	    CkDisplayChars(winPtr->mainPtr, winPtr->window, p,
		strlen(p), 0, y, 0,
		CK_NEWLINES_NOT_SPECIAL | CK_IGNORE_TABS | CK_FILL_UNTIL_EOL);
	}
#else
	CkDisplayChars(winPtr->mainPtr,
	    winPtr->window, &elPtr->text[listPtr->xOffset],
	    elPtr->textLength - listPtr->xOffset, 0, y, 0,
	    CK_NEWLINES_NOT_SPECIAL | CK_IGNORE_TABS | CK_FILL_UNTIL_EOL);
#endif
	y++;
    }
    wmove(winPtr->window, cursorY, 0);
    Ck_EventuallyRefresh(winPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ListboxComputeGeometry --
 *
 *	This procedure is invoked to recompute geometry information
 *	such as the sizes of the elements and the overall dimensions
 *	desired for the listbox.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Geometry information is updated and a new requested size is
 *	registered for the widget.  Internal border and gridding
 *	information is also set.
 *
 *----------------------------------------------------------------------
 */

static void
ListboxComputeGeometry(listPtr)
    Listbox *listPtr;		/* Listbox whose geometry is to be
				 * recomputed. */
{
    int width, height;

    width = listPtr->width;
    if (width <= 0) {
	width = listPtr->maxWidth;
	if (width < 1) {
	    width = 1;
	}
    }
    height = listPtr->height;
    if (listPtr->height <= 0) {
	height = listPtr->numElements;
	if (height < 1) {
	    height = 1;
	}
    }
    Ck_GeometryRequest(listPtr->winPtr, width, height);
}

/*
 *----------------------------------------------------------------------
 *
 * InsertEls --
 *
 *	Add new elements to a listbox widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	New information gets added to listPtr;  it will be redisplayed
 *	soon, but not immediately.
 *
 *----------------------------------------------------------------------
 */

static void
InsertEls(listPtr, index, argc, argv)
    register Listbox *listPtr;	/* Listbox that is to get the new
				 * elements. */
    int index;			/* Add the new elements before this
				 * element. */
    int argc;			/* Number of new elements to add. */
    char **argv;		/* New elements (one per entry). */
{
    register Element *prevPtr, *newPtr;
    int length, i, oldMaxWidth;

    /*
     * Find the element before which the new ones will be inserted.
     */

    if (index <= 0) {
	index = 0;
    }
    if (index > listPtr->numElements) {
	index = listPtr->numElements;
    }
    if (index == 0) {
	prevPtr = NULL;
    } else if (index == listPtr->numElements) {
          prevPtr = listPtr->lastPtr;
    } else {
	for (prevPtr = listPtr->firstPtr, i = index - 1; i > 0; i--) {
	    prevPtr = prevPtr->nextPtr;
	}
    }

    /*
     * For each new element, create a record, initialize it, and link
     * it into the list of elements.
     */

    oldMaxWidth = listPtr->maxWidth;
    for (i = argc ; i > 0; i--, argv++, prevPtr = newPtr) {
	length = strlen(*argv);
	newPtr = (Element *) ckalloc(ElementSize(length));
	newPtr->textLength = length;
	strcpy(newPtr->text, *argv);
#if CK_USE_UTF
	newPtr->textWidth = Tcl_NumUtfChars(*argv, length);
#else
	newPtr->textWidth = newPtr->textLength;
#endif
	if (newPtr->textWidth > listPtr->maxWidth) {
	    listPtr->maxWidth = newPtr->textWidth;
	}
	newPtr->selected = 0;
	if (prevPtr == NULL) {
	    newPtr->nextPtr = listPtr->firstPtr;
	    listPtr->firstPtr = newPtr;
	} else {
	    newPtr->nextPtr = prevPtr->nextPtr;
	    prevPtr->nextPtr = newPtr;
	}
    }
    if ((prevPtr != NULL) && (prevPtr->nextPtr == NULL)) {
	listPtr->lastPtr = prevPtr;
    }
    listPtr->numElements += argc;

    /*
     * Update the selection and other indexes to account for the
     * renumbering that has just occurred.  Then arrange for the new
     * information to be displayed.
     */

    if (index <= listPtr->selectAnchor) {
	listPtr->selectAnchor += argc;
    }
    if (index < listPtr->topIndex) {
	listPtr->topIndex += argc;
    }
    if (index <= listPtr->active) {
	listPtr->active += argc;
	if ((listPtr->active >= listPtr->numElements)
		&& (listPtr->numElements > 0)) {
	    listPtr->active = listPtr->numElements-1;
	}
    }
    listPtr->flags |= UPDATE_V_SCROLLBAR;
    if (listPtr->maxWidth != oldMaxWidth) {
	listPtr->flags |= UPDATE_H_SCROLLBAR;
    }
    ListboxComputeGeometry(listPtr);
    ListboxRedrawRange(listPtr, index, listPtr->numElements-1);
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteEls --
 *
 *	Remove one or more elements from a listbox widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory gets freed, the listbox gets modified and (eventually)
 *	redisplayed.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteEls(listPtr, first, last)
    register Listbox *listPtr;	/* Listbox widget to modify. */
    int first;			/* Index of first element to delete. */
    int last;			/* Index of last element to delete. */
{
    register Element *prevPtr, *elPtr;
    int count, i, widthChanged;

    /*
     * Adjust the range to fit within the existing elements of the
     * listbox, and make sure there's something to delete.
     */

    if (first < 0) {
	first = 0;
    }
    if (last >= listPtr->numElements) {
	last = listPtr->numElements-1;
    }
    count = last + 1 - first;
    if (count <= 0) {
	return;
    }

    /*
     * Find the element just before the ones to delete.
     */

    if (first == 0) {
	prevPtr = NULL;
    } else {
	for (i = first-1, prevPtr = listPtr->firstPtr; i > 0; i--) {
	    prevPtr = prevPtr->nextPtr;
	}
    }

    /*
     * Delete the requested number of elements.
     */

    widthChanged = 0;
    for (i = count; i > 0; i--) {
	if (prevPtr == NULL) {
	    elPtr = listPtr->firstPtr;
	    listPtr->firstPtr = elPtr->nextPtr;
	    if (listPtr->firstPtr == NULL) {
		listPtr->lastPtr = NULL;
	    }
	} else {
	    elPtr = prevPtr->nextPtr;
	    prevPtr->nextPtr = elPtr->nextPtr;
	    if (prevPtr->nextPtr == NULL) {
		listPtr->lastPtr = prevPtr;
	    }
	}
	if (elPtr->textWidth == listPtr->maxWidth) {
	    widthChanged = 1;
	}
	if (elPtr->selected) {
	    listPtr->numSelected -= 1;
	}
	ckfree((char *) elPtr);
    }
    listPtr->numElements -= count;

    /*
     * Update the selection and viewing information to reflect the change
     * in the element numbering, and redisplay to slide information up over
     * the elements that were deleted.
     */

    if (first <= listPtr->selectAnchor) {
	listPtr->selectAnchor -= count;
	if (listPtr->selectAnchor < first) {
	    listPtr->selectAnchor = first;
	}
    }
    if (first <= listPtr->topIndex) {
	listPtr->topIndex -= count;
	if (listPtr->topIndex < first) {
	    listPtr->topIndex = first;
	}
    }
    if (listPtr->topIndex > (listPtr->numElements - listPtr->fullLines)) {
	listPtr->topIndex = listPtr->numElements - listPtr->fullLines;
	if (listPtr->topIndex < 0) {
	    listPtr->topIndex = 0;
	}
    }
    if (listPtr->active > last) {
	listPtr->active -= count;
    } else if (listPtr->active >= first) {
	listPtr->active = first;
	if ((listPtr->active >= listPtr->numElements)
		&& (listPtr->numElements > 0)) {
	    listPtr->active = listPtr->numElements-1;
	}
    }
    listPtr->flags |= UPDATE_V_SCROLLBAR;
    ListboxComputeGeometry(listPtr);
    if (widthChanged) {
	int maxWidth = 0;

	for (elPtr = listPtr->firstPtr; elPtr != NULL; elPtr = elPtr->nextPtr)
	    if (elPtr->textWidth > maxWidth)
		maxWidth = elPtr->textWidth;
	if (maxWidth != listPtr->maxWidth) {
	    listPtr->maxWidth = maxWidth;
	    listPtr->flags |= UPDATE_H_SCROLLBAR;
	    if (listPtr->xOffset + listPtr->width >= listPtr->maxWidth)
		listPtr->xOffset = listPtr->maxWidth - listPtr->width;
	    if (listPtr->xOffset < 0)
		listPtr->xOffset = 0;
	}
    }
    ListboxRedrawRange(listPtr, first, listPtr->numElements-1);
}

/*
 *--------------------------------------------------------------
 *
 * ListboxEventProc --
 *
 *	This procedure is invoked by the dispatcher for various
 *	events on listboxes.
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
ListboxEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    CkEvent *eventPtr;		/* Information about event. */
{
    Listbox *listPtr = (Listbox *) clientData;

    if (eventPtr->type == CK_EV_DESTROY) {
        if (listPtr->winPtr != NULL) {
            listPtr->winPtr = NULL;
            Tcl_DeleteCommand(listPtr->interp,
                Tcl_GetCommandName(listPtr->interp, listPtr->widgetCmd));
        }
	if (listPtr->flags & REDRAW_PENDING) {
	    Tk_CancelIdleCall(DisplayListbox, (ClientData) listPtr);
	}
	Ck_EventuallyFree((ClientData) listPtr,
	    (Ck_FreeProc *) DestroyListbox);
    } else if (eventPtr->type == CK_EV_EXPOSE) {
	listPtr->fullLines = listPtr->winPtr->height;
	listPtr->flags |= UPDATE_V_SCROLLBAR|UPDATE_H_SCROLLBAR;
	ChangeListboxView(listPtr, listPtr->topIndex);
	ChangeListboxOffset(listPtr, listPtr->xOffset);

	/*
	 * Redraw the whole listbox.  It's hard to tell what needs
	 * to be redrawn (e.g. if the listbox has shrunk then we
	 * may only need to redraw the borders), so just redraw
	 * everything for safety.
	 */

	ListboxRedrawRange(listPtr, 0, listPtr->numElements-1);
    } else if (eventPtr->type == CK_EV_FOCUSIN) {
	listPtr->flags |= GOT_FOCUS;
	ListboxRedrawRange(listPtr, listPtr->active, listPtr->active);
    } else if (eventPtr->type == CK_EV_FOCUSOUT) {
	listPtr->flags &= ~GOT_FOCUS;
	ListboxRedrawRange(listPtr, listPtr->active, listPtr->active);
    }
}

/*
 *--------------------------------------------------------------
 *
 * GetListboxIndex --
 *
 *	Parse an index into a listbox and return either its value
 *	or an error.
 *
 * Results:
 *	A standard Tcl result.  If all went well, then *indexPtr is
 *	filled in with the index (into listPtr) corresponding to
 *	string.  Otherwise an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
GetListboxIndex(interp, listPtr, string, numElsOK, indexPtr)
    Tcl_Interp *interp;		/* For error messages. */
    Listbox *listPtr;		/* Listbox for which the index is being
				 * specified. */
    char *string;		/* Specifies an element in the listbox. */
    int numElsOK;		/* 0 means the return value must be less
				 * less than the number of entries in
				 * the listbox;  1 means it may also be
				 * equal to the number of entries. */
    int *indexPtr;		/* Where to store converted index. */
{
    int c;
    size_t length;

    length = strlen(string);
    c = string[0];
    if ((c == 'a') && (strncmp(string, "active", length) == 0)
	    && (length >= 2)) {
	*indexPtr = listPtr->active;
    } else if ((c == 'a') && (strncmp(string, "anchor", length) == 0)
	    && (length >= 2)) {
	*indexPtr = listPtr->selectAnchor;
    } else if ((c == 'e') && (strncmp(string, "end", length) == 0)) {
	*indexPtr = listPtr->numElements;
    } else if (c == '@') {
	int x, y;
	char *p, *end;

	p = string+1;
	x = strtol(p, &end, 0);
	if ((end == p) || (*end != ',')) {
	    goto badIndex;
	}
	p = end+1;
	y = strtol(p, &end, 0);
	if ((end == p) || (*end != 0)) {
	    goto badIndex;
	}
	*indexPtr = NearestListboxElement(listPtr, y);
    } else {
	if (Tcl_GetInt(interp, string, indexPtr) != TCL_OK) {
	    Tcl_ResetResult(interp);
	    goto badIndex;
	}
    }
    if (numElsOK) {
	if (*indexPtr > listPtr->numElements) {
	    *indexPtr = listPtr->numElements;
	}
    } else if (*indexPtr >= listPtr->numElements) {
	*indexPtr = listPtr->numElements-1;
    }
    if (*indexPtr < 0) {
	*indexPtr = 0;
    }
    return TCL_OK;

    badIndex:
    Tcl_AppendResult(interp, "bad listbox index \"", string,
	    "\":  must be active, anchor, end, @x,y, or a number",
	    (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * ChangeListboxView --
 *
 *	Change the view on a listbox widget so that a given element
 *	is displayed at the top.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	What's displayed on the screen is changed.  If there is a
 *	scrollbar associated with this widget, then the scrollbar
 *	is instructed to change its display too.
 *
 *----------------------------------------------------------------------
 */

static void
ChangeListboxView(listPtr, index)
    register Listbox *listPtr;		/* Information about widget. */
    int index;				/* Index of element in listPtr
					 * that should now appear at the
					 * top of the listbox. */
{
    if (index >= (listPtr->numElements - listPtr->fullLines)) {
	index = listPtr->numElements - listPtr->fullLines;
    }
    if (index < 0) {
	index = 0;
    }
    if (listPtr->topIndex != index) {
	listPtr->topIndex = index;
	if (!(listPtr->flags & REDRAW_PENDING)) {
	    Tk_DoWhenIdle(DisplayListbox, (ClientData) listPtr);
	    listPtr->flags |= REDRAW_PENDING;
	}
	listPtr->flags |= UPDATE_V_SCROLLBAR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ChangListboxOffset --
 *
 *	Change the horizontal offset for a listbox.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The listbox may be redrawn to reflect its new horizontal
 *	offset.
 *
 *----------------------------------------------------------------------
 */

static void
ChangeListboxOffset(listPtr, offset)
    register Listbox *listPtr;		/* Information about widget. */
    int offset;				/* Desired new "xOffset" for
					 * listbox. */
{
    int maxOffset;

    /*
     * Make sure that the new offset is within the allowable range, and
     * round it off to an even multiple of xScrollUnit.
     */

    maxOffset = listPtr->maxWidth - listPtr->winPtr->width;
    if (offset > maxOffset) {
	offset = maxOffset;
    }
    if (offset < 0) {
	offset = 0;
    }
    listPtr->xOffset = offset;
    listPtr->flags |= UPDATE_H_SCROLLBAR;
    ListboxRedrawRange(listPtr, 0, listPtr->numElements);
}

/*
 *----------------------------------------------------------------------
 *
 * NearestListboxElement --
 *
 *	Given a y-coordinate inside a listbox, compute the index of
 *	the element under that y-coordinate (or closest to that
 *	y-coordinate).
 *
 * Results:
 *	The return value is an index of an element of listPtr.  If
 *	listPtr has no elements, then 0 is always returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
NearestListboxElement(listPtr, y)
    register Listbox *listPtr;		/* Information about widget. */
    int y;				/* Y-coordinate in listPtr's window. */
{
    int index;

    index = y;
    if (index >= listPtr->fullLines) {
	index = listPtr->fullLines - 1;
    }
    if (index < 0) {
	index = 0;
    }
    index += listPtr->topIndex;
    if (index >= listPtr->numElements) {
	index = listPtr->numElements-1;
    }
    return index;
}

/*
 *----------------------------------------------------------------------
 *
 * ListboxSelect --
 *
 *	Select or deselect one or more elements in a listbox..
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All of the elements in the range between first and last are
 *	marked as either selected or deselected, depending on the
 *	"select" argument.  Any items whose state changes are redisplayed.
 *	The selection is claimed from X when the number of selected
 *	elements changes from zero to non-zero.
 *
 *----------------------------------------------------------------------
 */

static void
ListboxSelect(listPtr, first, last, select)
    register Listbox *listPtr;		/* Information about widget. */
    int first;				/* Index of first element to
					 * select or deselect. */
    int last;				/* Index of last element to
					 * select or deselect. */
    int select;				/* 1 means select items, 0 means
					 * deselect them. */
{
    int i, firstRedisplay, lastRedisplay, increment, oldCount;
    Element *elPtr;

    if (last < first) {
	i = first;
	first = last;
	last = i;
    }
    if (first >= listPtr->numElements) {
	return;
    }
    oldCount = listPtr->numSelected;
    firstRedisplay = -1;
    increment = select ? 1 : -1;
    for (i = 0, elPtr = listPtr->firstPtr; i < first;
	    i++, elPtr = elPtr->nextPtr) {
	/* Empty loop body. */
    }
    for ( ; i <= last; i++, elPtr = elPtr->nextPtr) {
	if (elPtr->selected == select) {
	    continue;
	}
	listPtr->numSelected += increment;
	elPtr->selected = select;
	if (firstRedisplay < 0) {
	    firstRedisplay = i;
	}
	lastRedisplay = i;
    }
    if (firstRedisplay >= 0) {
	ListboxRedrawRange(listPtr, first, last);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ListboxRedrawRange --
 *
 *	Ensure that a given range of elements is eventually redrawn on
 *	the display (if those elements in fact appear on the display).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets redisplayed.
 *
 *----------------------------------------------------------------------
 */

static void
ListboxRedrawRange(listPtr, first, last)
    register Listbox *listPtr;		/* Information about widget. */
    int first;				/* Index of first element in list
					 * that needs to be redrawn. */
    int last;				/* Index of last element in list
					 * that needs to be redrawn.  May
					 * be less than first;
					 * these just bracket a range. */
{
    if ((listPtr->winPtr == NULL) || !(listPtr->winPtr->flags & CK_MAPPED)
	    || (listPtr->flags & REDRAW_PENDING)) {
	return;
    }
    Tk_DoWhenIdle(DisplayListbox, (ClientData) listPtr);
    listPtr->flags |= REDRAW_PENDING;
}

/*
 *----------------------------------------------------------------------
 *
 * ListboxUpdateVScrollbar --
 *
 *	This procedure is invoked whenever information has changed in
 *	a listbox in a way that would invalidate a vertical scrollbar
 *	display.  If there is an associated scrollbar, then this command
 *	updates it by invoking a Tcl command.
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
ListboxUpdateVScrollbar(listPtr)
    register Listbox *listPtr;		/* Information about widget. */
{
    char string[100];
    double first, last;
    int result;

    if (listPtr->yScrollCmd == NULL) {
	return;
    }
    if (listPtr->numElements == 0) {
	first = 0.0;
	last = 1.0;
    } else {
	first = listPtr->topIndex/((double) listPtr->numElements);
	last = (listPtr->topIndex+listPtr->fullLines)
		/((double) listPtr->numElements);
	if (last > 1.0) {
	    last = 1.0;
	}
    }
    sprintf(string, " %g %g", first, last);
    result = Tcl_VarEval(listPtr->interp, listPtr->yScrollCmd, string,
	    (char *) NULL);
    if (result != TCL_OK) {
	Tcl_AddErrorInfo(listPtr->interp,
		"\n    (vertical scrolling command executed by listbox)");
	Tk_BackgroundError(listPtr->interp);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ListboxUpdateHScrollbar --
 *
 *	This procedure is invoked whenever information has changed in
 *	a listbox in a way that would invalidate a horizontal scrollbar
 *	display.  If there is an associated horizontal scrollbar, then
 *	this command updates it by invoking a Tcl command.
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
ListboxUpdateHScrollbar(listPtr)
    register Listbox *listPtr;		/* Information about widget. */
{
    char string[60];
    int result, windowWidth;
    double first, last;

    if (listPtr->xScrollCmd == NULL) {
	return;
    }
    windowWidth = listPtr->winPtr->width;
    if (listPtr->maxWidth == 0) {
	first = 0;
	last = 1.0;
    } else {
	first = listPtr->xOffset/((double) listPtr->maxWidth);
	last = (listPtr->xOffset + windowWidth)
		/((double) listPtr->maxWidth);
	if (last > 1.0) {
	    last = 1.0;
	}
    }
    sprintf(string, " %g %g", first, last);
    result = Tcl_VarEval(listPtr->interp, listPtr->xScrollCmd, string,
	    (char *) NULL);
    if (result != TCL_OK) {
	Tcl_AddErrorInfo(listPtr->interp,
		"\n    (horizontal scrolling command executed by listbox)");
	Tk_BackgroundError(listPtr->interp);
    }
}
