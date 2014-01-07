/* 
 * ckText.c --
 *
 *	This module provides a big chunk of the implementation of
 *	multi-line editable text widgets for ck.  Among other things,
 *	it provides the Tcl command interfaces to text widgets and
 *	the display code.  The B-tree representation of text is
 *	implemented elsewhere.
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 * Copyright (c) 1995 Christian Werner
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "ckPort.h"
#include "ck.h"
#include "ckText.h"
#include "default.h"

/*
 * Information used to parse text configuration options:
 */

static Ck_ConfigSpec configSpecs[] = {
    {CK_CONFIG_ATTR, "-attributes", "attributes", "Attributes",
	DEF_TEXT_ATTR, Ck_Offset(CkText, attr), 0},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_TEXT_BG_COLOR, Ck_Offset(CkText, bg), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_TEXT_BG_MONO, Ck_Offset(CkText, bg), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_TEXT_FG, Ck_Offset(CkText, fg), 0},
    {CK_CONFIG_COORD, "-height", "height", "Height",
	DEF_TEXT_HEIGHT, Ck_Offset(CkText, height), 0},
    {CK_CONFIG_ATTR, "-selectattributes", "selectAttributes",
        "SelectAttributes", DEF_TEXT_SELECT_ATTR_COLOR,
        Ck_Offset(CkText, selAttr), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_ATTR, "-selectattributes", "selectAttributes",
        "SelectAttributes", DEF_TEXT_SELECT_ATTR_MONO,
        Ck_Offset(CkText, selAttr), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-selectbackground", "selectBackground", "Foreground",
	DEF_TEXT_SELECT_BG_COLOR, Ck_Offset(CkText, selBg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-selectbackground", "selectBackground", "Foreground",
	DEF_TEXT_SELECT_BG_MONO, Ck_Offset(CkText, selBg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_TEXT_SELECT_FG_COLOR, Ck_Offset(CkText, selFg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_TEXT_SELECT_FG_MONO, Ck_Offset(CkText, selFg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_UID, "-state", "state", "State",
	DEF_TEXT_STATE, Ck_Offset(CkText, state), 0},
    {CK_CONFIG_STRING, "-tabs", "tabs", "Tabs",
	DEF_TEXT_TABS, Ck_Offset(CkText, tabOptionString), CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_TEXT_TAKE_FOCUS, Ck_Offset(CkText, takeFocus),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_COORD, "-width", "width", "Width",
	DEF_TEXT_WIDTH, Ck_Offset(CkText, width), 0},
    {CK_CONFIG_UID, "-wrap", "wrap", "Wrap",
	DEF_TEXT_WRAP, Ck_Offset(CkText, wrapMode), 0},
    {CK_CONFIG_STRING, "-xscrollcommand", "xScrollCommand", "ScrollCommand",
	DEF_TEXT_XSCROLL_COMMAND, Ck_Offset(CkText, xScrollCmd),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-yscrollcommand", "yScrollCommand", "ScrollCommand",
	DEF_TEXT_YSCROLL_COMMAND, Ck_Offset(CkText, yScrollCmd),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Ck_Uid's used to represent text states:
 */

Ck_Uid ckTextCharUid = NULL;
Ck_Uid ckTextDisabledUid = NULL;
Ck_Uid ckTextNoneUid = NULL;
Ck_Uid ckTextNormalUid = NULL;
Ck_Uid ckTextWordUid = NULL;

/*
 * Boolean variable indicating whether or not special debugging code
 * should be executed.
 */

int ckTextDebug = 0;

/*
 * Forward declarations for procedures defined later in this file:
 */

static int		ConfigureText _ANSI_ARGS_((Tcl_Interp *interp,
			    CkText *textPtr, int argc, char **argv, int flags));
static int		DeleteChars _ANSI_ARGS_((CkText *textPtr,
			    char *index1String, char *index2String));
static void		DestroyText _ANSI_ARGS_((ClientData clientData));
static void		InsertChars _ANSI_ARGS_((CkText *textPtr,
			    CkTextIndex *indexPtr, char *string));
static void		TextCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static void		TextEventProc _ANSI_ARGS_((ClientData clientData,
			    CkEvent *eventPtr));
static int		TextSearchCmd _ANSI_ARGS_((CkText *textPtr,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TextWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));

/*
 *--------------------------------------------------------------
 *
 * Ck_TextCmd --
 *
 *	This procedure is invoked to process the "text" Tcl command.
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

int
Ck_TextCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *mainPtr = (CkWindow *) clientData;
    CkWindow *new;
    register CkText *textPtr;
    CkTextIndex startIndex;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Perform once-only initialization:
     */

    if (ckTextNormalUid == NULL) {
	ckTextCharUid = Ck_GetUid("char");
	ckTextDisabledUid = Ck_GetUid("disabled");
	ckTextNoneUid = Ck_GetUid("none");
	ckTextNormalUid = Ck_GetUid("normal");
	ckTextWordUid = Ck_GetUid("word");
    }

    /*
     * Create the window.
     */

    new = Ck_CreateWindowFromPath(interp, mainPtr, argv[1], 0);
    if (new == NULL) {
	return TCL_ERROR;
    }

    textPtr = (CkText *) ckalloc(sizeof(CkText));
    textPtr->winPtr = new;
    textPtr->interp = interp;
    textPtr->widgetCmd = Tcl_CreateCommand(interp,
	new->pathName, TextWidgetCmd, (ClientData) textPtr,
        TextCmdDeletedProc);
    textPtr->tree = CkBTreeCreate();
    Tcl_InitHashTable(&textPtr->tagTable, TCL_STRING_KEYS);
    textPtr->numTags = 0;
    Tcl_InitHashTable(&textPtr->markTable, TCL_STRING_KEYS);
    Tcl_InitHashTable(&textPtr->windowTable, TCL_STRING_KEYS);
    textPtr->state = ckTextNormalUid;
    textPtr->bg = 0;
    textPtr->fg = 0;
    textPtr->attr = 0;
    textPtr->tabOptionString = NULL;
    textPtr->tabArrayPtr = NULL;
    textPtr->wrapMode = ckTextCharUid;
    textPtr->width = 0;
    textPtr->height = 0;
    textPtr->prevWidth = new->width;
    textPtr->prevHeight = new->height;
    CkTextCreateDInfo(textPtr);
#if CK_USE_UTF
    CkTextMakeByteIndex(textPtr->tree, 0, 0, &startIndex);
#else
    CkTextMakeIndex(textPtr->tree, 0, 0, &startIndex);
#endif
    CkTextSetYView(textPtr, &startIndex, 0);
    textPtr->selTagPtr = NULL;
    textPtr->selBg = 0;
    textPtr->selFg = 0;
    textPtr->selAttr = 0;
    textPtr->abortSelections = 0;
    textPtr->insertMarkPtr = NULL;
    textPtr->bindingTable = NULL;
    textPtr->currentMarkPtr = NULL;
    textPtr->pickEvent.type = -1;
    textPtr->numCurTags = 0;
    textPtr->curTagArrayPtr = NULL;
    textPtr->takeFocus = NULL;
    textPtr->xScrollCmd = NULL;
    textPtr->yScrollCmd = NULL;
    textPtr->flags = 0;

    /*
     * Create the "sel" tag and the "current" and "insert" marks.
     */

    textPtr->selTagPtr = CkTextCreateTag(textPtr, "sel");
    textPtr->currentMarkPtr = CkTextSetMark(textPtr, "current", &startIndex);
    textPtr->insertMarkPtr = CkTextSetMark(textPtr, "insert", &startIndex);

    Ck_SetClass(new, "Text");
    Ck_CreateEventHandler(textPtr->winPtr,
            CK_EV_EXPOSE | CK_EV_DESTROY | CK_EV_MAP | CK_EV_FOCUSIN |
	    CK_EV_FOCUSOUT,
	    TextEventProc, (ClientData) textPtr);
    Ck_CreateEventHandler(textPtr->winPtr, CK_EV_KEYPRESS,
	    CkTextBindProc, (ClientData) textPtr);
    if (ConfigureText(interp, textPtr, argc-2, argv+2, 0) != TCL_OK) {
	Ck_DestroyWindow(textPtr->winPtr);
	return TCL_ERROR;
    }
    interp->result = textPtr->winPtr->pathName;

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TextWidgetCmd --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a text widget.  See the user
 *	documentation for details on what it does.
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
TextWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about text widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    register CkText *textPtr = (CkText *) clientData;
    int result = TCL_OK;
    size_t length;
    int c;
    CkTextIndex index1, index2;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Ck_Preserve((ClientData) textPtr);
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'b') && (strncmp(argv[1], "bbox", length) == 0)) {
	int x, y, width, height;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " bbox index\"", (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}
	if (CkTextGetIndex(interp, textPtr, argv[2], &index1) != TCL_OK) {
	    result = TCL_ERROR;
	    goto done;
	}
	if (CkTextCharBbox(textPtr, &index1, &x, &y, &width, &height) == 0) {
	    sprintf(interp->result, "%d %d %d %d", x, y, width, height);
	}
    } else if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " cget option\"",
		    (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}
	result = Ck_ConfigureValue(interp, textPtr->winPtr, configSpecs,
		(char *) textPtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "compare", length) == 0)
	    && (length >= 3)) {
	int relation, value;
	char *p;

	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " compare index1 op index2\"", (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}
	if ((CkTextGetIndex(interp, textPtr, argv[2], &index1) != TCL_OK)
		|| (CkTextGetIndex(interp, textPtr, argv[4], &index2)
		!= TCL_OK)) {
	    result = TCL_ERROR;
	    goto done;
	}
	relation = CkTextIndexCmp(&index1, &index2);
	p = argv[3];
	if (p[0] == '<') {
		value = (relation < 0);
	    if ((p[1] == '=') && (p[2] == 0)) {
		value = (relation <= 0);
	    } else if (p[1] != 0) {
		compareError:
		Tcl_AppendResult(interp, "bad comparison operator \"",
			argv[3], "\": must be <, <=, ==, >=, >, or !=",
			(char *) NULL);
		result = TCL_ERROR;
		goto done;
	    }
	} else if (p[0] == '>') {
		value = (relation > 0);
	    if ((p[1] == '=') && (p[2] == 0)) {
		value = (relation >= 0);
	    } else if (p[1] != 0) {
		goto compareError;
	    }
	} else if ((p[0] == '=') && (p[1] == '=') && (p[2] == 0)) {
	    value = (relation == 0);
	} else if ((p[0] == '!') && (p[1] == '=') && (p[2] == 0)) {
	    value = (relation != 0);
	} else {
	    goto compareError;
	}
	interp->result = (value) ? "1" : "0";
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 3)) {
	if (argc == 2) {
	    result = Ck_ConfigureInfo(interp, textPtr->winPtr, configSpecs,
		    (char *) textPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Ck_ConfigureInfo(interp, textPtr->winPtr, configSpecs,
		    (char *) textPtr, argv[2], 0);
	} else {
	    result = ConfigureText(interp, textPtr, argc-2, argv+2,
		    CK_CONFIG_ARGV_ONLY);
	}
    } else if ((c == 'd') && (strncmp(argv[1], "debug", length) == 0)
	    && (length >= 3)) {
	if (argc > 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " debug boolean\"", (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}
	if (argc == 2) {
	    interp->result = (ckBTreeDebug) ? "1" : "0";
	} else {
	    if (Tcl_GetBoolean(interp, argv[2], &ckBTreeDebug) != TCL_OK) {
		result = TCL_ERROR;
		goto done;
	    }
	    ckTextDebug = ckBTreeDebug;
	}
    } else if ((c == 'd') && (strncmp(argv[1], "delete", length) == 0)
	    && (length >= 3)) {
	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " delete index1 ?index2?\"", (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}
	if (textPtr->state == ckTextNormalUid) {
	    result = DeleteChars(textPtr, argv[2],
		    (argc == 4) ? argv[3] : (char *) NULL);
	}
    } else if ((c == 'd') && (strncmp(argv[1], "dlineinfo", length) == 0)
	    && (length >= 2)) {
	int x, y, width, height, base;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " dlineinfo index\"", (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}
	if (CkTextGetIndex(interp, textPtr, argv[2], &index1) != TCL_OK) {
	    result = TCL_ERROR;
	    goto done;
	}
	if (CkTextDLineInfo(textPtr, &index1, &x, &y, &width, &height, &base)
		== 0) {
	    sprintf(interp->result, "%d %d %d %d %d", x, y, width,
		    height, base);
	}
    } else if ((c == 'g') && (strncmp(argv[1], "get", length) == 0)) {
	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " get index1 ?index2?\"", (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}
	if (CkTextGetIndex(interp, textPtr, argv[2], &index1) != TCL_OK) {
	    result = TCL_ERROR;
	    goto done;
	}
	if (argc == 3) {
	    index2 = index1;
	    CkTextIndexForwChars(&index2, 1, &index2);
	} else if (CkTextGetIndex(interp, textPtr, argv[3], &index2)
		!= TCL_OK) {
	    result = TCL_ERROR;
	    goto done;
	}
	if (CkTextIndexCmp(&index1, &index2) >= 0) {
	    goto done;
	}
	while (1) {
	    int offset, last, savedChar;
	    CkTextSegment *segPtr;

	    segPtr = CkTextIndexToSeg(&index1, &offset);
	    last = segPtr->size;
	    if (index1.linePtr == index2.linePtr) {
		int last2;

		if (index2.charIndex == index1.charIndex) {
		    break;
		}
		last2 = index2.charIndex - index1.charIndex + offset;
		if (last2 < last) {
		    last = last2;
		}
	    }
	    if (segPtr->typePtr == &ckTextCharType) {
		savedChar = segPtr->body.chars[last];
		segPtr->body.chars[last] = 0;
		Tcl_AppendResult(interp, segPtr->body.chars + offset,
			(char *) NULL);
		segPtr->body.chars[last] = savedChar;
	    }
	    CkTextIndexForwChars(&index1, last-offset, &index1);
	}
    } else if ((c == 'i') && (strncmp(argv[1], "index", length) == 0)
	    && (length >= 3)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " index index\"",
		    (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}
	if (CkTextGetIndex(interp, textPtr, argv[2], &index1) != TCL_OK) {
	    result = TCL_ERROR;
	    goto done;
	}
	CkTextPrintIndex(&index1, interp->result);
    } else if ((c == 'i') && (strncmp(argv[1], "insert", length) == 0)
	    && (length >= 3)) {
	int i, j, numTags;
	char **tagNames;
	CkTextTag **oldTagArrayPtr;

	if (argc < 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0],
		    " insert index chars ?tagList chars tagList ...?\"",
		    (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}
	if (CkTextGetIndex(interp, textPtr, argv[2], &index1) != TCL_OK) {
	    result = TCL_ERROR;
	    goto done;
	}
	if (textPtr->state == ckTextNormalUid) {
	    for (j = 3;  j < argc; j += 2) {
		InsertChars(textPtr, &index1, argv[j]);
		if (argc > (j+1)) {
		    CkTextIndexForwChars(&index1, (int) strlen(argv[j]),
			    &index2);
		    oldTagArrayPtr = CkBTreeGetTags(&index1, &numTags);
		    if (oldTagArrayPtr != NULL) {
			for (i = 0; i < numTags; i++) {
			    CkBTreeTag(&index1, &index2, oldTagArrayPtr[i], 0);
			}
			ckfree((char *) oldTagArrayPtr);
		    }
		    if (Tcl_SplitList(interp, argv[j+1], &numTags, &tagNames)
			    != TCL_OK) {
			result = TCL_ERROR;
			goto done;
		    }
		    for (i = 0; i < numTags; i++) {
			CkBTreeTag(&index1, &index2,
				CkTextCreateTag(textPtr, tagNames[i]), 1);
		    }
		    ckfree((char *) tagNames);
		    index1 = index2;
		}
	    }
	}
    } else if ((c == 'm') && (strncmp(argv[1], "mark", length) == 0)) {
	result = CkTextMarkCmd(textPtr, interp, argc, argv);
    } else if ((c == 's') && (strcmp(argv[1], "search") == 0)
	    && (length >= 3)) {
	result = TextSearchCmd(textPtr, interp, argc, argv);
    } else if ((c == 's') && (strcmp(argv[1], "see") == 0) && (length >= 3)) {
	result = CkTextSeeCmd(textPtr, interp, argc, argv);
    } else if ((c == 't') && (strcmp(argv[1], "tag") == 0)) {
	result = CkTextTagCmd(textPtr, interp, argc, argv);
#if 0
    } else if ((c == 'w') && (strncmp(argv[1], "window", length) == 0)) {
	result = CkTextWindowCmd(textPtr, interp, argc, argv);
#endif
    } else if ((c == 'x') && (strncmp(argv[1], "xview", length) == 0)) {
	result = CkTextXviewCmd(textPtr, interp, argc, argv);
    } else if ((c == 'y') && (strncmp(argv[1], "yview", length) == 0)
	    && (length >= 2)) {
	result = CkTextYviewCmd(textPtr, interp, argc, argv);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\":  must be bbox, cget, compare, configure, debug, delete, ",
		"dlineinfo, get, index, insert, mark, scan, search, see, ",
		"tag, window, xview, or yview",
		(char *) NULL);
	result = TCL_ERROR;
    }

    done:
    Ck_Release((ClientData) textPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyText --
 *
 *	This procedure is invoked by Ck_EventuallyFree or Ck_Release
 *	to clean up the internal structure of a text at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the text is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyText(clientData)
    ClientData clientData;	/* Info about text widget. */
{
    register CkText *textPtr = (CkText *) clientData;
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    CkTextTag *tagPtr;

    /*
     * Free up all the stuff that requires special handling, then
     * let Ck_FreeOptions handle all the standard option-related
     * stuff.  Special note:  free up display-related information
     * before deleting the B-tree, since display-related stuff
     * may refer to stuff in the B-tree.
     */

    CkTextFreeDInfo(textPtr);
    CkBTreeDestroy(textPtr->tree);
    for (hPtr = Tcl_FirstHashEntry(&textPtr->tagTable, &search);
	    hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	tagPtr = (CkTextTag *) Tcl_GetHashValue(hPtr);
	CkTextFreeTag(textPtr, tagPtr);
    }
    Tcl_DeleteHashTable(&textPtr->tagTable);
    for (hPtr = Tcl_FirstHashEntry(&textPtr->markTable, &search);
	    hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	ckfree((char *) Tcl_GetHashValue(hPtr));
    }
    Tcl_DeleteHashTable(&textPtr->markTable);
    if (textPtr->tabArrayPtr != NULL) {
	ckfree((char *) textPtr->tabArrayPtr);
    }
    if (textPtr->bindingTable != NULL) {
	Ck_DeleteBindingTable(textPtr->bindingTable);
    }

    /*
     * NOTE: do NOT free up selBorder, selBdString, or selFgColorPtr:
     * they are duplicates of information in the "sel" tag, which was
     * freed up as part of deleting the tags above.
     */

    Ck_FreeOptions(configSpecs, (char *) textPtr, 0);
    ckfree((char *) textPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureText --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Ck option database, in order to configure (or
 *	reconfigure) a text widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for textPtr;  old resources get freed, if there
 *	were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureText(interp, textPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    register CkText *textPtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Ck_ConfigureWidget. */
{
    if (Ck_ConfigureWidget(interp, textPtr->winPtr, configSpecs,
	    argc, argv, (char *) textPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * A few other options also need special processing, such as parsing
     * the geometry and setting the background from a 3-D border.
     */

    if ((textPtr->state != ckTextNormalUid)
	    && (textPtr->state != ckTextDisabledUid)) {
	Tcl_AppendResult(interp, "bad state value \"", textPtr->state,
		"\":  must be normal or disabled", (char *) NULL);
	textPtr->state = ckTextNormalUid;
	return TCL_ERROR;
    }

    if ((textPtr->wrapMode != ckTextCharUid)
	    && (textPtr->wrapMode != ckTextNoneUid)
	    && (textPtr->wrapMode != ckTextWordUid)) {
	Tcl_AppendResult(interp, "bad wrap mode \"", textPtr->wrapMode,
		"\":  must be char, none, or word", (char *) NULL);
	textPtr->wrapMode = ckTextCharUid;
	return TCL_ERROR;
    }

    /*
     * Parse tab stops.
     */

    if (textPtr->tabArrayPtr != NULL) {
	ckfree((char *) textPtr->tabArrayPtr);
	textPtr->tabArrayPtr = NULL;
    }
    if (textPtr->tabOptionString != NULL) {
	textPtr->tabArrayPtr = CkTextGetTabs(interp, textPtr->winPtr,
		textPtr->tabOptionString);
	if (textPtr->tabArrayPtr == NULL) {
	    Tcl_AddErrorInfo(interp,"\n    (while processing -tabs option)");
	    return TCL_ERROR;
	}
    }

    /*
     * Make sure that configuration options are properly mirrored
     * between the widget record and the "sel" tags.  NOTE: we don't
     * have to free up information during the mirroring;  old
     * information was freed when it was replaced in the widget
     * record.
     */

    textPtr->selTagPtr->bg = textPtr->selBg;
    textPtr->selTagPtr->fg = textPtr->selFg;
    textPtr->selTagPtr->attr = textPtr->selAttr;
    textPtr->selTagPtr->affectsDisplay = 0;
#if 0
/* ??? */
    if ((textPtr->selTagPtr->border != NULL)
	    || (textPtr->selTagPtr->bdString != NULL)
	    || (textPtr->selTagPtr->reliefString != NULL)
	    || (textPtr->selTagPtr->bgStipple != None)
	    || (textPtr->selTagPtr->fgColor != NULL)
	    || (textPtr->selTagPtr->fontPtr != None)
	    || (textPtr->selTagPtr->fgStipple != None)
	    || (textPtr->selTagPtr->justifyString != NULL)
	    || (textPtr->selTagPtr->lMargin1String != NULL)
	    || (textPtr->selTagPtr->lMargin2String != NULL)
	    || (textPtr->selTagPtr->offsetString != NULL)
	    || (textPtr->selTagPtr->overstrikeString != NULL)
	    || (textPtr->selTagPtr->rMarginString != NULL)
	    || (textPtr->selTagPtr->spacing1String != NULL)
	    || (textPtr->selTagPtr->spacing2String != NULL)
	    || (textPtr->selTagPtr->spacing3String != NULL)
	    || (textPtr->selTagPtr->tabString != NULL)
	    || (textPtr->selTagPtr->underlineString != NULL)
	    || (textPtr->selTagPtr->wrapMode != NULL)) {
	textPtr->selTagPtr->affectsDisplay = 1;
    }
#endif
    CkTextRedrawTag(textPtr, (CkTextIndex *) NULL, (CkTextIndex *) NULL,
	    textPtr->selTagPtr, 1);

    /*
     * Register the desired geometry for the window, and arrange for
     * the window to be redisplayed.
     */

    if (textPtr->width <= 0) {
	textPtr->width = 1;
    }
    if (textPtr->height <= 0) {
	textPtr->height = 1;
    }
    Ck_GeometryRequest(textPtr->winPtr, textPtr->width, textPtr->height);

    CkTextRelayoutWindow(textPtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TextEventProc --
 *
 *	This procedure is invoked by the Ck dispatcher on
 *	structure changes to a text.  For texts with 3D
 *	borders, this procedure is also invoked for exposures.
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
TextEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    register CkEvent *eventPtr;	/* Information about event. */
{
    register CkText *textPtr = (CkText *) clientData;
    CkWindow *winPtr = textPtr->winPtr;
    CkTextIndex index, index2;

    if (eventPtr->type == CK_EV_EXPOSE) {
	if ((textPtr->prevWidth != winPtr->width)
		|| (textPtr->prevHeight != winPtr->height)) {
	    CkTextRelayoutWindow(textPtr);
	    textPtr->prevWidth = winPtr->width;
	    textPtr->prevHeight = winPtr->height;
	}
	CkTextRedrawRegion(textPtr, 0, 0, winPtr->width, winPtr->height);
    } else if (eventPtr->type == CK_EV_DESTROY) {
        if (textPtr->winPtr != NULL) {
            textPtr->winPtr = NULL;
            Tcl_DeleteCommand(textPtr->interp,
                    Tcl_GetCommandName(textPtr->interp,
                    textPtr->widgetCmd));
        }
	Ck_EventuallyFree((ClientData) textPtr, (Ck_FreeProc *) DestroyText);
    } else if ((eventPtr->type == CK_EV_FOCUSIN)
        || (eventPtr->type == CK_EV_FOCUSOUT)) {
	if (eventPtr->type == CK_EV_FOCUSIN) {
	    textPtr->flags |= GOT_FOCUS;
	} else {
	    textPtr->flags &= ~GOT_FOCUS;
	}
	CkTextMarkSegToIndex(textPtr, textPtr->insertMarkPtr, &index);
	CkTextIndexForwChars(&index, 1, &index2);
	CkTextChanged(textPtr, &index, &index2);
	CkTextRedrawRegion(textPtr, 0, 0, winPtr->width,
	    winPtr->height);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TextCmdDeletedProc --
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
TextCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    CkText *textPtr = (CkText *) clientData;
    CkWindow *winPtr = textPtr->winPtr;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case ckwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (winPtr != NULL) {
	textPtr->winPtr = NULL;
	Ck_DestroyWindow(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * InsertChars --
 *
 *	This procedure implements most of the functionality of the
 *	"insert" widget command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The characters in "string" get added to the text just before
 *	the character indicated by "indexPtr".
 *
 *----------------------------------------------------------------------
 */

static void
InsertChars(textPtr, indexPtr, string)
    CkText *textPtr;		/* Overall information about text widget. */
    CkTextIndex *indexPtr;	/* Where to insert new characters.  May be
				 * modified and/or invalidated. */
    char *string;		/* Null-terminated string containing new
				 * information to add to text. */
{
    int lineIndex;

    /*
     * Don't allow insertions on the last (dummy) line of the text.
     */

    lineIndex = CkBTreeLineIndex(indexPtr->linePtr);
    if (lineIndex == CkBTreeNumLines(textPtr->tree)) {
	lineIndex--;
#if CK_USE_UTF
	CkTextMakeByteIndex(textPtr->tree, lineIndex, 1000000, indexPtr);
#else
	CkTextMakeIndex(textPtr->tree, lineIndex, 1000000, indexPtr);
#endif
    }

    /*
     * Notify the display module that lines are about to change, then do
     * the insertion.
     */

    CkTextChanged(textPtr, indexPtr, indexPtr);
    CkBTreeInsertChars(indexPtr, string);

    /*
     * Invalidate any selection retrievals in progress.
     */

    textPtr->abortSelections = 1;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteChars --
 *
 *	This procedure implements most of the functionality of the
 *	"delete" widget command.
 *
 * Results:
 *	Returns a standard Tcl result, and leaves an error message
 *	in textPtr->interp if there is an error.
 *
 * Side effects:
 *	Characters get deleted from the text.
 *
 *----------------------------------------------------------------------
 */

static int
DeleteChars(textPtr, index1String, index2String)
    CkText *textPtr;		/* Overall information about text widget. */
    char *index1String;		/* String describing location of first
				 * character to delete. */
    char *index2String;		/* String describing location of last
				 * character to delete.  NULL means just
				 * delete the one character given by
				 * index1String. */
{
    int line1, line2, line, charIndex, resetView;
    CkTextIndex index1, index2;

    /*
     * Parse the starting and stopping indices.
     */

    if (CkTextGetIndex(textPtr->interp, textPtr, index1String, &index1)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    if (index2String != NULL) {
	if (CkTextGetIndex(textPtr->interp, textPtr, index2String, &index2)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	index2 = index1;
	CkTextIndexForwChars(&index2, 1, &index2);
    }

    /*
     * Make sure there's really something to delete.
     */

    if (CkTextIndexCmp(&index1, &index2) >= 0) {
	return TCL_OK;
    }

    /*
     * The code below is ugly, but it's needed to make sure there
     * is always a dummy empty line at the end of the text.  If the
     * final newline of the file (just before the dummy line) is being
     * deleted, then back up index to just before the newline.  If
     * there is a newline just before the first character being deleted,
     * then back up the first index too, so that an even number of lines
     * gets deleted.  Furthermore, remove any tags that are present on
     * the newline that isn't going to be deleted after all (this simulates
     * deleting the newline and then adding a "clean" one back again).
     */

    line1 = CkBTreeLineIndex(index1.linePtr);
    line2 = CkBTreeLineIndex(index2.linePtr);
    if (line2 == CkBTreeNumLines(textPtr->tree)) {
	CkTextTag **arrayPtr;
	int arraySize, i;
	CkTextIndex oldIndex2;

	oldIndex2 = index2;
	CkTextIndexBackChars(&oldIndex2, 1, &index2);
	line2--;
	if ((index1.charIndex == 0) && (line1 != 0)) {
	    CkTextIndexBackChars(&index1, 1, &index1);
	    line1--;
	}
	arrayPtr = CkBTreeGetTags(&index2, &arraySize);
	if (arrayPtr != NULL) {
	    for (i = 0; i < arraySize; i++) {
		CkBTreeTag(&index2, &oldIndex2, arrayPtr[i], 0);
	    }
	    ckfree((char *) arrayPtr);
	}
    }

    /*
     * Tell the display what's about to happen so it can discard
     * obsolete display information, then do the deletion.  Also,
     * if the deletion involves the top line on the screen, then
     * we have to reset the view (the deletion will invalidate
     * textPtr->topIndex).  Compute what the new first character
     * will be, then do the deletion, then reset the view.
     */

    CkTextChanged(textPtr, &index1, &index2);
    resetView = line = charIndex = 0;
    if (CkTextIndexCmp(&index2, &textPtr->topIndex) >= 0) {
	if (CkTextIndexCmp(&index1, &textPtr->topIndex) <= 0) {
	    /*
	     * Deletion range straddles topIndex: use the beginning
	     * of the range as the new topIndex.
	     */

	    resetView = 1;
	    line = line1;
	    charIndex = index1.charIndex;
	} else if (index1.linePtr == textPtr->topIndex.linePtr) {
	    /*
	     * Deletion range starts on top line but after topIndex.
	     * Use the current topIndex as the new one.
	     */

	    resetView = 1;
	    line = line1;
	    charIndex = textPtr->topIndex.charIndex;
	}
    } else if (index2.linePtr == textPtr->topIndex.linePtr) {
	/*
	 * Deletion range ends on top line but before topIndex.
	 * Figure out what will be the new character index for
	 * the character currently pointed to by topIndex.
	 */

	resetView = 1;
	line = line2;
	charIndex = textPtr->topIndex.charIndex;
	if (index1.linePtr != index2.linePtr) {
	    charIndex -= index2.charIndex;
	} else {
	    charIndex -= (index2.charIndex - index1.charIndex);
	}
    }
    CkBTreeDeleteChars(&index1, &index2);
    if (resetView) {
#if CK_USE_UTF
	CkTextMakeByteIndex(textPtr->tree, line, charIndex, &index1);
#else
	CkTextMakeIndex(textPtr->tree, line, charIndex, &index1);
#endif
	CkTextSetYView(textPtr, &index1, 0);
    }

    /*
     * Invalidate any selection retrievals in progress.
     */

    textPtr->abortSelections = 1;

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TextSearchCmd --
 *
 *	This procedure is invoked to process the "search" widget command
 *	for text widgets.  See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
TextSearchCmd(textPtr, interp, argc, argv)
    CkText *textPtr;		/* Information about text widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    int backwards, exact, c, i, argsLeft, noCase, leftToScan;
    size_t length;
    int numLines, startingLine, startingChar, lineNum, firstChar, lastChar;
    int code, matchLength, matchChar, passes, stopLine, searchWholeText;
    int patLength;
    char *arg, *pattern, *varName, *p, *startOfLine;
    char buffer[20];
    CkTextIndex index, stopIndex;
    Tcl_DString line, patDString;
    CkTextSegment *segPtr;
    CkTextLine *linePtr;
    Tcl_RegExp regexp = NULL;		/* Initialization needed only to
					 * prevent compiler warning. */

    /*
     * Parse switches and other arguments.
     */

    exact = 1;
    backwards = 0;
    noCase = 0;
    varName = NULL;
    for (i = 2; i < argc; i++) {
	arg = argv[i];
	if (arg[0] != '-') {
	    break;
	}
	length = strlen(arg);
	if (length < 2) {
	    badSwitch:
	    Tcl_AppendResult(interp, "bad switch \"", arg,
		    "\": must be -forward, -backward, -exact, -regexp, ",
		    "-nocase, -count, or --", (char *) NULL);
	    return TCL_ERROR;
	}
	c = arg[1];
	if ((c == 'b') && (strncmp(argv[i], "-backwards", length) == 0)) {
	    backwards = 1;
	} else if ((c == 'c') && (strncmp(argv[i], "-count", length) == 0)) {
	    if (i >= (argc-1)) {
		interp->result = "no value given for \"-count\" option";
		return TCL_ERROR;
	    }
	    i++;
	    varName = argv[i];
	} else if ((c == 'e') && (strncmp(argv[i], "-exact", length) == 0)) {
	    exact = 1;
	} else if ((c == 'f') && (strncmp(argv[i], "-forwards", length) == 0)) {
	    backwards = 0;
	} else if ((c == 'n') && (strncmp(argv[i], "-nocase", length) == 0)) {
	    noCase = 1;
	} else if ((c == 'r') && (strncmp(argv[i], "-regexp", length) == 0)) {
	    exact = 0;
	} else if ((c == '-') && (strncmp(argv[i], "--", length) == 0)) {
	    i++;
	    break;
	} else {
	    goto badSwitch;
	}
    }
    argsLeft = argc - (i+2);
    if ((argsLeft != 0) && (argsLeft != 1)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " search ?switches? pattern index ?stopIndex?",
		(char *) NULL);
	return TCL_ERROR;
    }
    pattern = argv[i];

    /*
     * Convert the pattern to lower-case if we're supposed to ignore case.
     */

    if (noCase) {
	Tcl_DStringInit(&patDString);
	Tcl_DStringAppend(&patDString, pattern, -1);
	pattern = Tcl_DStringValue(&patDString);
#if CK_USE_UTF
	Tcl_UtfToLower(pattern);
#else
	for (p = pattern; *p != 0; p++) {
	    if (isupper((unsigned char) *p)) {
		*p = tolower((unsigned char) *p);
	    }
	}
#endif
    }

    if (CkTextGetIndex(interp, textPtr, argv[i+1], &index) != TCL_OK) {
	return TCL_ERROR;
    }
    numLines = CkBTreeNumLines(textPtr->tree);
    startingLine = CkBTreeLineIndex(index.linePtr);
    startingChar = index.charIndex;
    if (startingLine >= numLines) {
	if (backwards) {
	    startingLine = CkBTreeNumLines(textPtr->tree) - 1;
	    startingChar = CkBTreeCharsInLine(CkBTreeFindLine(textPtr->tree,
		    startingLine));
	} else {
	    startingLine = 0;
	    startingChar = 0;
	}
    }
    if (argsLeft == 1) {
	if (CkTextGetIndex(interp, textPtr, argv[i+2], &stopIndex) != TCL_OK) {
	    return TCL_ERROR;
	}
	stopLine = CkBTreeLineIndex(stopIndex.linePtr);
	if (!backwards && (stopLine == numLines)) {
	    stopLine = numLines-1;
	}
	searchWholeText = 0;
    } else {
	stopLine = 0;
	searchWholeText = 1;
    }

    /*
     * Scan through all of the lines of the text circularly, starting
     * at the given index.
     */

    matchLength = patLength = 0;	/* Only needed to prevent compiler
					 * warnings. */
    if (exact) {
	patLength = strlen(pattern);
    } else {
	regexp = Tcl_RegExpCompile(interp, pattern);
	if (regexp == NULL) {
	    return TCL_ERROR;
	}
    }
    lineNum = startingLine;
    code = TCL_OK;
    Tcl_DStringInit(&line);
    for (passes = 0; passes < 2; ) {
	if (lineNum >= numLines) {
	    /*
	     * Don't search the dummy last line of the text.
	     */

	    goto nextLine;
	}

	/*
	 * Extract the text from the line.  If we're doing regular
	 * expression matching, drop the newline from the line, so
	 * that "$" can be used to match the end of the line.
	 */

	linePtr = CkBTreeFindLine(textPtr->tree, lineNum);
	for (segPtr = linePtr->segPtr; segPtr != NULL;
		segPtr = segPtr->nextPtr) {
	    if (segPtr->typePtr != &ckTextCharType) {
		continue;
	    }
	    Tcl_DStringAppend(&line, segPtr->body.chars, segPtr->size);
	}
	if (!exact) {
	    Tcl_DStringSetLength(&line, Tcl_DStringLength(&line)-1);
	}
	startOfLine = Tcl_DStringValue(&line);

	/*
	 * If we're ignoring case, convert the line to lower case.
	 */

	if (noCase) {
#if CK_USE_UTF
	    Tcl_DStringSetLength(&line,
		Tcl_UtfToLower(Tcl_DStringValue(&line)));
#else
	    for (p = Tcl_DStringValue(&line); *p != 0; p++) {
		if (isupper((unsigned char) *p)) {
		    *p = tolower((unsigned char) *p);
		}
	    }
#endif
	}

	/*
	 * Check for matches within the current line.  If so, and if we're
	 * searching backwards, repeat the search to find the last match
	 * in the line.
	 */

	matchChar = -1;
	firstChar = 0;
	lastChar = INT_MAX;
	if (lineNum == startingLine) {
	    int indexInDString;

	    /*
	     * The starting line is tricky: the first time we see it
	     * we check one part of the line, and the second pass through
	     * we check the other part of the line.  We have to be very
	     * careful here because there could be embedded windows or
	     * other things that are not in the extracted line.  Rescan
	     * the original line to compute the index in it of the first
	     * character.
	     */

	    indexInDString = startingChar;
	    for (segPtr = linePtr->segPtr, leftToScan = startingChar;
		    leftToScan > 0; segPtr = segPtr->nextPtr) {
		if (segPtr->typePtr != &ckTextCharType) {
		    indexInDString -= segPtr->size;
		}
		leftToScan -= segPtr->size;
	    }

	    passes++;
	    if ((passes == 1) ^ backwards) {
		/*
		 * Only use the last part of the line.
		 */

		firstChar = indexInDString;
		if (firstChar >= Tcl_DStringLength(&line)) {
		    goto nextLine;
		}
	    } else {
		/*
		 * Use only the first part of the line.
		 */

		lastChar = indexInDString;
	    }
	}
	do {
	    int thisLength;
#if CK_USE_UTF
	    Tcl_UniChar ch;
#endif

	    if (exact) {
		p = strstr(startOfLine + firstChar, pattern);
		if (p == NULL) {
		    break;
		}
		i = p - startOfLine;
		thisLength = patLength;
	    } else {
		char *start, *end;
		int match;

		match = Tcl_RegExpExec(interp, regexp,
			startOfLine + firstChar, startOfLine);
		if (match < 0) {
		    code = TCL_ERROR;
		    goto done;
		}
		if (!match) {
		    break;
		}
		Tcl_RegExpRange(regexp, 0, &start, &end);
		i = start - startOfLine;
		thisLength = end - start;
	    }
	    if (i >= lastChar) {
		break;
	    }
	    matchChar = i;
	    matchLength = thisLength;
#if CK_USE_UTF
	    firstChar = i + Tcl_UtfToUniChar(startOfLine + matchChar, &ch);
#else
	    firstChar = matchChar+1;
#endif
	} while (backwards);

	/*
	 * If we found a match then we're done.  Make sure that
	 * the match occurred before the stopping index, if one was
	 * specified.
	 */

	if (matchChar >= 0) {
#if CK_USE_UTF
	    int numChars;
	   
	    numChars = Tcl_NumUtfChars(startOfLine + matchChar,
		matchLength);
#endif
	    
	    /*
	     * The index information returned by the regular expression
	     * parser only considers textual information:  it doesn't
	     * account for embedded windows or any other non-textual info.
	     * Scan through the line's segments again to adjust both
	     * matchChar and matchCount.
	     */

	    for (segPtr = linePtr->segPtr, leftToScan = matchChar;
		    leftToScan >= 0; segPtr = segPtr->nextPtr) {
		if (segPtr->typePtr != &ckTextCharType) {
		    matchChar += segPtr->size;
		    continue;
		}
		leftToScan -= segPtr->size;
	    }
	    for (leftToScan += matchLength; leftToScan > 0;
		    segPtr = segPtr->nextPtr) {
		if (segPtr->typePtr != &ckTextCharType) {
#if CK_USE_UTF
		    numChars += segPtr->size;
#else
		    matchLength += segPtr->size;
#endif
		    continue;
		}
		leftToScan -= segPtr->size;
	    }
#if CK_USE_UTF
	    CkTextMakeByteIndex(textPtr->tree, lineNum, matchChar, &index);
#else
	    CkTextMakeIndex(textPtr->tree, lineNum, matchChar, &index);
#endif
	    if (!searchWholeText) {
		if (!backwards && (CkTextIndexCmp(&index, &stopIndex) >= 0)) {
		    goto done;
		}
		if (backwards && (CkTextIndexCmp(&index, &stopIndex) < 0)) {
		    goto done;
		}
	    }
	    if (varName != NULL) {
#if CK_USE_UTF
		sprintf(buffer, "%d", numChars);
#else
		sprintf(buffer, "%d", matchLength);
#endif
		if (Tcl_SetVar(interp, varName, buffer, TCL_LEAVE_ERR_MSG)
			== NULL) {
		    code = TCL_ERROR;
		    goto done;
		}
	    }
	    CkTextPrintIndex(&index, interp->result);
	    goto done;
	}

	/*
	 * Go to the next (or previous) line;
	 */

	nextLine:
	if (backwards) {
	    lineNum--;
	    if (!searchWholeText) {
		if (lineNum < stopLine) {
		    break;
		}
	    } else if (lineNum < 0) {
		lineNum = numLines-1;
	    }
	} else {
	    lineNum++;
	    if (!searchWholeText) {
		if (lineNum > stopLine) {
		    break;
		}
	    } else if (lineNum >= numLines) {
		lineNum = 0;
	    }
	}
	Tcl_DStringSetLength(&line, 0);
    }
    done:
    Tcl_DStringFree(&line);
    if (noCase) {
	Tcl_DStringFree(&patDString);
    }
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * CkTextGetTabs --
 *
 *	Parses a string description of a set of tab stops.
 *
 * Results:
 *	The return value is a pointer to a malloc'ed structure holding
 *	parsed information about the tab stops.  If an error occurred
 *	then the return value is NULL and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	Memory is allocated for the structure that is returned.  It is
 *	up to the caller to free this structure when it is no longer
 *	needed.
 *
 *----------------------------------------------------------------------
 */

CkTextTabArray *
CkTextGetTabs(interp, winPtr, string)
    Tcl_Interp *interp;			/* Used for error reporting. */
    CkWindow *winPtr;			/* Window in which the tabs will be
					 * used. */
    char *string;			/* Description of the tab stops.  See
					 * text manual entry for details. */
{
    int argc, i, count, c = 0;
    char **argv;
    CkTextTabArray *tabArrayPtr;
    CkTextTab *tabPtr;
#if CK_USE_UTF
    Tcl_UniChar ch;
#endif

    if (Tcl_SplitList(interp, string, &argc, &argv) != TCL_OK) {
	return NULL;
    }

    /*
     * First find out how many entries we need to allocate in the
     * tab array.
     */

    count = 0;
    for (i = 0; i < argc; i++) {
	c = argv[i][0];
	if ((c != 'l') && (c != 'r') && (c != 'c') && (c != 'n')) {
	    count++;
	}
    }

    /*
     * Parse the elements of the list one at a time to fill in the
     * array.
     */

    tabArrayPtr = (CkTextTabArray *) ckalloc((unsigned)
	    (sizeof(CkTextTabArray) + (count-1)*sizeof(CkTextTab)));
    tabArrayPtr->numTabs = 0;
    for (i = 0, tabPtr = &tabArrayPtr->tabs[0]; i  < argc; i++, tabPtr++) {
	if (Ck_GetCoord(interp, winPtr, argv[i], &tabPtr->location)
		!= TCL_OK) {
	    goto error;
	}
	tabArrayPtr->numTabs++;

	/*
	 * See if there is an explicit alignment in the next list
	 * element.  Otherwise just use "left".
	 */

	tabPtr->alignment = LEFT;
	if ((i+1) == argc) {
	    continue;
	}
#if CK_USE_UTF
	Tcl_UtfToUniChar(argv[i+1], &ch);
	if (!Tcl_UniCharIsAlpha(ch)) {
	    continue;
	}
#else
	c = (unsigned char) argv[i+1][0];
	if (!isalpha(c)) {
	    continue;
	}
#endif
	i += 1;
	if ((c == 'l') && (strncmp(argv[i], "left",
		strlen(argv[i])) == 0)) {
	    tabPtr->alignment = LEFT;
	} else if ((c == 'r') && (strncmp(argv[i], "right",
		strlen(argv[i])) == 0)) {
	    tabPtr->alignment = RIGHT;
	} else if ((c == 'c') && (strncmp(argv[i], "center",
		strlen(argv[i])) == 0)) {
	    tabPtr->alignment = CENTER;
	} else if ((c == 'n') && (strncmp(argv[i],
		"numeric", strlen(argv[i])) == 0)) {
	    tabPtr->alignment = NUMERIC;
	} else {
	    Tcl_AppendResult(interp, "bad tab alignment \"",
		    argv[i], "\": must be left, right, center, or numeric",
		    (char *) NULL);
	    goto error;
	}
    }
    ckfree((char *) argv);
    return tabArrayPtr;

    error:
    ckfree((char *) tabArrayPtr);
    ckfree((char *) argv);
    return NULL;
}
