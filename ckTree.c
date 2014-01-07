/* 
 * ckTree.c --
 *
 *	This module implements a tree widget.
 *
 * Copyright (c) 1996 Christian Werner
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "ckPort.h"
#include "ck.h"
#include "default.h"

/*
 * Widget defaults:
 */

#define DEF_TREE_ACTIVE_ATTR_COLOR "normal"
#define DEF_TREE_ACTIVE_ATTR_MONO  "reverse"
#define DEF_TREE_ACTIVE_BG_COLOR   "white"
#define DEF_TREE_ACTIVE_BG_MONO    "black"
#define DEF_TREE_ACTIVE_FG_COLOR   "black"
#define DEF_TREE_ACTIVE_FG_MONO    "white"
#define DEF_TREE_ATTR_COLOR        "normal"
#define DEF_TREE_ATTR_MONO         "normal"
#define DEF_TREE_BG_COLOR          "black"
#define DEF_TREE_BG_MONO           "black"
#define DEF_TREE_FG_COLOR          "white"
#define DEF_TREE_FG_MONO           "white"
#define DEF_TREE_HEIGHT            "10"
#define DEF_TREE_SELECT_ATTR_COLOR "bold"
#define DEF_TREE_SELECT_ATTR_MONO  "bold"
#define DEF_TREE_SELECT_BG_COLOR   "black"
#define DEF_TREE_SELECT_BG_MONO    "black"
#define DEF_TREE_SELECT_FG_COLOR   "white"
#define DEF_TREE_SELECT_FG_MONO    "white"
#define DEF_TREE_TAKE_FOCUS        "1"
#define DEF_TREE_WIDTH             "40"
#define DEF_TREE_SCROLL_COMMAND    NULL

/*
 * A node in the tree is represented by this data structure.
 */

#define TAG_SPACE 5

typedef struct Node {
    long id;			/* Unique id of the node. */
    int level;			/* Level in tree, 0 means root. */
    struct Tree *tree;		/* Pointer to widget. */
    struct Node *parent;	/* Pointer to parent node or NULL. */
    struct Node *next;		/* Pointer to next node in this level. */
    struct Node *firstChild, *lastChild;
    Ck_Uid staticTagSpace[TAG_SPACE];
    Ck_Uid *tagPtr;		/* Pointer to tag array. */
    int tagSpace;		/* Total size of tag array. */
    int numTags;		/* Number of tags in tag array. */
    int fg;			/* Foreground color of node's text. */
    int bg;			/* Background color of node's text. */
    int attr;			/* Video attributes of node's text. */
    char *text;			/* Text to display for this node. */
    int textWidth;		/* Width of node's text. */
    int flags;			/* Flag bits (see below). */
} Node;

/*
 * Flag bits for node:
 *
 * SELECTED:			Non-zero means node is selected
 * SHOWCHILDREN:		Non-zero means if node has children
 *				they shall be displayed.
 */

#define SELECTED     1
#define SHOWCHILDREN 2

/*
 * Custom option for handling "-tags" options for tree nodes:
 */

static int		TreeTagsParseProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, CkWindow *winPtr, char *value,
			    char *widgRec, int offset));
static char *		TreeTagsPrintProc _ANSI_ARGS_((ClientData clientData,
			    CkWindow *winPtr, char *widgRec, int offset,
			    Tcl_FreeProc **freeProcPtr));

Ck_CustomOption treeTagsOption = {
    TreeTagsParseProc,
    TreeTagsPrintProc,
    (ClientData) NULL
};

/*
 * Information used for parsing configuration specs for nodes:
 */

static Ck_ConfigSpec nodeConfigSpecs[] = {
    {CK_CONFIG_ATTR, "-attributes", (char *) NULL, (char *) NULL,
        "", Ck_Offset(Node, attr), CK_CONFIG_DONT_SET_DEFAULT },
    {CK_CONFIG_COLOR, "-background", (char *) NULL, (char *) NULL,
	"", Ck_Offset(Node, bg), CK_CONFIG_DONT_SET_DEFAULT },
    {CK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_COLOR, "-foreground", (char *) NULL, (char *) NULL,
	"", Ck_Offset(Node, fg), CK_CONFIG_DONT_SET_DEFAULT},
    {CK_CONFIG_CUSTOM, "-tags", (char *) NULL, (char *) NULL,
	(char *) NULL, 0, CK_CONFIG_NULL_OK, &treeTagsOption},
    {CK_CONFIG_STRING, "-text", (char *) NULL, (char *) NULL,
	NULL, Ck_Offset(Node, text), CK_CONFIG_NULL_OK},
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * A data structure of the following type is kept for each
 * widget managed by this file:
 */

typedef struct Tree {
    CkWindow *winPtr;		/* Window that embodies the widget.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Tcl_Interp *interp;		/* Interpreter associated with menubutton. */
    Tcl_Command widgetCmd;	/* Token for menubutton's widget command. */

    long idCount;		/* For unique ids for nodes. */

    /*
     * Information about what's displayed in the menu button:
     */

    Node *firstChild, *lastChild;
    Tcl_HashTable nodeTable;

    /*
     * Information used when displaying widget:
     */

    int normalFg;		/* Foreground color in normal mode. */
    int normalBg;               /* Background color in normal mode. */
    int normalAttr;             /* Attributes in normal mode. */
    int activeFg;		/* Foreground color in active mode. */
    int activeBg;               /* Ditto, background color. */
    int activeAttr;		/* Attributes in active mode. */
    int selectFg;		/* Foreground color for selected nodes. */
    int selectBg;               /* Ditto, background color. */
    int selectAttr;		/* Attributes for selected nodes. */

    int width, height;		/* If > 0, these specify dimensions to request
				 * for window, in characters for text and in
				 * pixels for bitmaps.  In this case the actual
				 * size of the text string or bitmap is
				 * ignored in computing desired window size. */

    int visibleNodes;		/* Total number of visible nodes. */
    int topIndex;		/* Index of starting line. */
    Node *topNode;		/* Node at top line of window. */
    Node *activeNode;		/* Node which has active tag or NULL. */

    int leadingSpace;		/* For displaying: size of leadingString. */
    int *leadingString;		/* Malloc'ed leading vertical lines for
    				 * displaying. */

    /*
     * Miscellaneous information:
     */

    char *takeFocus;		/* Value of -takefocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
    char *yScrollCmd;           /* Command prefix for communicating with
                                 * vertical scrollbar.  NULL means no command
                                 * to issue.  Malloc'ed. */
    char *xScrollCmd;           /* Command prefix for communicating with
                                 * horizontal scrollbar.  NULL means no command
                                 * to issue.  Malloc'ed. */
    int flags;			/* Various flags;  see below for
				 * definitions. */
} Tree;

/*
 * Flag bits for entire tree:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 * GOT_FOCUS:			Non-zero means this button currently
 *				has the input focus.
 * UPDATE_V_SCROLLBAR:          Non-zero means vertical scrollbar needs
 *                              to be updated.
 * UPDATE_H_SCROLLBAR:          Non-zero means horizontal scrollbar needs
 *                              to be updated.
 */

#define REDRAW_PENDING		1
#define GOT_FOCUS		2
#define UPDATE_V_SCROLLBAR      4
#define UPDATE_H_SCROLLBAR      4

/*
 * Information used for parsing configuration specs:
 */

static Ck_ConfigSpec configSpecs[] = {
    {CK_CONFIG_ATTR, "-activeattributes", "activeAttributes",
        "ActiveAttributes", DEF_TREE_ACTIVE_ATTR_COLOR,
        Ck_Offset(Tree, activeAttr), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_ATTR, "-activeattributes", "activeAttributes",
        "ActiveAttributes", DEF_TREE_ACTIVE_ATTR_MONO,
        Ck_Offset(Tree, activeAttr), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_ATTR, "-attributes", "attributes", "Attributes",
	DEF_TREE_ATTR_COLOR, Ck_Offset(Tree, normalAttr),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_ATTR, "-attributes", "attributes", "Attributes",
	DEF_TREE_ATTR_MONO, Ck_Offset(Tree, normalAttr),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-activebackground", "activeBackground", "Foreground",
	DEF_TREE_ACTIVE_BG_COLOR, Ck_Offset(Tree, activeBg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-activebackground", "activeBackground", "Foreground",
	DEF_TREE_ACTIVE_BG_MONO, Ck_Offset(Tree, activeBg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_TREE_ACTIVE_FG_COLOR, Ck_Offset(Tree, activeFg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_TREE_ACTIVE_FG_MONO, Ck_Offset(Tree, activeFg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_TREE_BG_COLOR, Ck_Offset(Tree, normalBg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_TREE_BG_MONO, Ck_Offset(Tree, normalBg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_TREE_FG_COLOR, Ck_Offset(Tree, normalFg), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_TREE_FG_MONO, Ck_Offset(Tree, normalFg), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COORD, "-height", "height", "Height",
	DEF_TREE_HEIGHT, Ck_Offset(Tree, height), 0},
    {CK_CONFIG_ATTR, "-selectattributes", "selectAttributes",
        "SelectAttributes", DEF_TREE_SELECT_ATTR_COLOR,
        Ck_Offset(Tree, selectAttr), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_ATTR, "-selectattributes", "selectAttributes",
        "SelectAttributes", DEF_TREE_SELECT_ATTR_MONO,
        Ck_Offset(Tree, selectAttr), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-selectbackground", "selectBackground", "Foreground",
	DEF_TREE_SELECT_BG_COLOR, Ck_Offset(Tree, selectBg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-selectbackground", "selectBackground", "Foreground",
	DEF_TREE_SELECT_BG_MONO, Ck_Offset(Tree, selectBg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_TREE_SELECT_FG_COLOR, Ck_Offset(Tree, selectFg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_TREE_SELECT_FG_MONO, Ck_Offset(Tree, selectFg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_TREE_TAKE_FOCUS, Ck_Offset(Tree, takeFocus),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_COORD, "-width", "width", "Width",
	DEF_TREE_WIDTH, Ck_Offset(Tree, width), 0},
    {CK_CONFIG_STRING, "-xscrollcommand", "xScrollCommand", "ScrollCommand",
        DEF_TREE_SCROLL_COMMAND, Ck_Offset(Tree, xScrollCmd),
        CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-yscrollcommand", "yScrollCommand", "ScrollCommand",
        DEF_TREE_SCROLL_COMMAND, Ck_Offset(Tree, yScrollCmd),
        CK_CONFIG_NULL_OK},
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * The structure defined below is used to keep track of a tag search
 * in progress.  Only the "prevPtr" field should be accessed by anyone
 * other than StartTagSearch and NextNode.
 */

typedef struct TagSearch {
    Tree *treePtr;		/* Tree widget being searched. */
    Tcl_HashSearch search;	/* Hash search for nodeTable. */
    Ck_Uid tag;			/* Tag to search for. 0 means return
				 * all nodes. */
    int searchOver;		/* Non-zero means NextNode should always
				 * return NULL. */
} TagSearch;

static Ck_Uid allUid = NULL;
static Ck_Uid hideChildrenUid = NULL;
static Ck_Uid activeUid = NULL;

/*
 * Forward declarations for procedures defined later in this file:
 */

static Node *		StartTagSearch _ANSI_ARGS_((Tree *treePtr,
			    char *tag, TagSearch *searchPtr));
static Node *		NextNode _ANSI_ARGS_((TagSearch *searchPtr));
static void		DoNode _ANSI_ARGS_((Tcl_Interp *interp,
			    Node *nodePtr, Ck_Uid tag));
static void		TreeCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static void		TreeEventProc _ANSI_ARGS_((ClientData clientData,
			    CkEvent *eventPtr));
static int		TreeWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static int		ConfigureTree _ANSI_ARGS_((Tcl_Interp *interp,
			    Tree *treePtr, int argc, char **argv,
			    int flags));
static void		DestroyTree _ANSI_ARGS_((ClientData clientData));
static void		DisplayTree _ANSI_ARGS_((ClientData clientData));
static void		TreeEventuallyRedraw _ANSI_ARGS_((Tree *treePtr));
static int		FindNodes _ANSI_ARGS_((Tcl_Interp *interp,
			    Tree *treePtr, int argc, char **argv,
			    char *newTag, char *cmdName, char *option));
static void		DeleteNode _ANSI_ARGS_((Tree *treePtr, Node *nodePtr));
static void		RecomputeVisibleNodes _ANSI_ARGS_((Tree *treePtr));
static void		ChangeTreeView _ANSI_ARGS_((Tree *treePtr, int index));
static void		TreeUpdateVScrollbar _ANSI_ARGS_((Tree *treePtr));
static int		GetNodeYCoord _ANSI_ARGS_((Tree *treePtr,
			    Node *thisPtr, int *yPtr));

/*
 *--------------------------------------------------------------
 *
 * Ck_TreeCmd --
 *
 *	This procedure is invoked to process the "tree"
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
Ck_TreeCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tree *treePtr;
    CkWindow *mainPtr = (CkWindow *) clientData;
    CkWindow *new;

    allUid = Ck_GetUid("all");
    hideChildrenUid = Ck_GetUid("hidechildren");
    activeUid = Ck_GetUid("active");

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

    treePtr = (Tree *) ckalloc(sizeof (Tree));
    treePtr->winPtr = new;
    treePtr->interp = interp;
    treePtr->widgetCmd = Tcl_CreateCommand(interp, treePtr->winPtr->pathName,
	    TreeWidgetCmd, (ClientData) treePtr, TreeCmdDeletedProc);
    treePtr->idCount = 0;
    treePtr->firstChild = treePtr->lastChild = NULL;
    Tcl_InitHashTable(&treePtr->nodeTable, TCL_ONE_WORD_KEYS);
    treePtr->normalBg = 0;
    treePtr->normalFg = 0;
    treePtr->normalAttr = 0;
    treePtr->activeBg = 0;
    treePtr->activeFg = 0;
    treePtr->activeAttr = 0;
    treePtr->selectBg = 0;
    treePtr->selectFg = 0;
    treePtr->selectAttr = 0;
    treePtr->width = 0;
    treePtr->height = 0;
    treePtr->visibleNodes = 0;
    treePtr->topIndex = 0;
    treePtr->topNode = NULL;
    treePtr->activeNode = NULL;
    treePtr->leadingSpace = 0;
    treePtr->leadingString = NULL;
    treePtr->takeFocus = NULL;
    treePtr->xScrollCmd = NULL;
    treePtr->yScrollCmd = NULL;
    treePtr->flags = 0;

    Ck_SetClass(treePtr->winPtr, "Tree");
    Ck_CreateEventHandler(treePtr->winPtr,
	CK_EV_EXPOSE | CK_EV_MAP | CK_EV_DESTROY |
	CK_EV_FOCUSIN | CK_EV_FOCUSOUT, TreeEventProc, (ClientData) treePtr);
    if (ConfigureTree(interp, treePtr, argc-2, argv+2, 0) != TCL_OK) {
	Ck_DestroyWindow(treePtr->winPtr);
	return TCL_ERROR;
    }

    interp->result = treePtr->winPtr->pathName;
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TreeWidgetCmd --
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
TreeWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about button widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tree *treePtr = (Tree *) clientData;
    int result = TCL_OK, redraw = 0, recompute = 0;
    size_t length;
    int c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Ck_Preserve((ClientData) treePtr);
    c = argv[1][0];
    length = strlen(argv[1]);

    if ((c == 'a') && (strncmp(argv[1], "addtag", length) == 0)) {
	if (argc < 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " addtags tag searchCommand ?arg arg ...?\"",
		    (char *) NULL);
	    goto error;
	}
	result = FindNodes(interp, treePtr, argc-3, argv+3, argv[2], argv[0],
		" addtag tag");
    } else if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " cget option\"",
		    (char *) NULL);
	    goto error;
	}
	result = Ck_ConfigureValue(interp, treePtr->winPtr, configSpecs,
		(char *) treePtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "children", length) == 0)
	    && (length >= 2)) {
	Node *nodePtr = NULL;
	TagSearch search;

	if (argc > 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " children ?tagOrId?\"",
		    (char *) NULL);
	    goto error;
	}
	if (argc > 2) {
	    nodePtr = StartTagSearch(treePtr, argv[2], &search);
	    if (nodePtr == NULL)
		goto error;
	}
	if (nodePtr == NULL)
	    nodePtr = treePtr->firstChild;
	else
	    nodePtr = nodePtr->firstChild;
	while (nodePtr != NULL) {
	    DoNode(interp, nodePtr, (Ck_Uid) NULL);
	    nodePtr = nodePtr->next;
	}
	result = TCL_OK;
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 2)) {
	if (argc == 2) {
	    result = Ck_ConfigureInfo(interp, treePtr->winPtr, configSpecs,
		    (char *) treePtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Ck_ConfigureInfo(interp, treePtr->winPtr, configSpecs,
		    (char *) treePtr, argv[2], 0);
	} else {
	    result = ConfigureTree(interp, treePtr, argc-2, argv+2,
		    CK_CONFIG_ARGV_ONLY);
	}
    } else if ((c == 'd') && (strncmp(argv[1], "delete", length) == 0)
	    && (length >= 2)) {
	int i;

	for (i = 2; i < argc; i++) {
	    for (;;) {
		Node *nodePtr;
		TagSearch search;

		nodePtr = StartTagSearch(treePtr, argv[i], &search);
		if (nodePtr == NULL)
		    break;
		DeleteNode(treePtr, nodePtr);
		recompute++;
	    }
	}
	if (recompute)
	    redraw++;
    } else if ((c == 'd') && (strncmp(argv[1], "dtag", length) == 0)
	    && (length >= 2)) {
	Ck_Uid tag;
	int i;
	Node *nodePtr;
	TagSearch search;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " dtag tagOrId ?tagToDelete?\"",
		    (char *) NULL);
	    goto error;
	}
	if (argc == 4) {
	    tag = Ck_GetUid(argv[3]);
	} else {
	    tag = Ck_GetUid(argv[2]);
	}
	for (nodePtr = StartTagSearch(treePtr, argv[2], &search);
		nodePtr != NULL; nodePtr = NextNode(&search)) {
	    for (i = nodePtr->numTags-1; i >= 0; i--) {
		if (nodePtr->tagPtr[i] == tag) {
		    nodePtr->tagPtr[i] = nodePtr->tagPtr[nodePtr->numTags-1];
		    nodePtr->numTags--;
		    if (tag == activeUid)
			redraw++;
		    else if (tag == hideChildrenUid) {
			if (!(nodePtr->flags & SHOWCHILDREN)) {
			    nodePtr->flags |= SHOWCHILDREN;
			    recompute++;
			    redraw++;
			}
		    }
		}
	    }
	}
    } else if ((c == 'f') && (strncmp(argv[1], "find", length) == 0)
	    && (length >= 2)) {
	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " find searchCommand ?arg arg ...?\"",
		    (char *) NULL);
	    goto error;
	}
	result = FindNodes(interp, treePtr, argc - 2, argv + 2, (char *) NULL,
		argv[0], " find");
    } else if ((c == 'g') && (strncmp(argv[1], "gettags", length) == 0)) {
	Node *nodePtr;
	TagSearch search;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " gettags tagOrId\"", (char *) NULL);
	    goto error;
	}
	nodePtr = StartTagSearch(treePtr, argv[2], &search);
	if (nodePtr != NULL) {
	    int i;

	    for (i = 0; i < nodePtr->numTags; i++) {
		Tcl_AppendElement(interp, (char *) nodePtr->tagPtr[i]);
	    }
	}
    } else if ((c == 'i') && (strncmp(argv[1], "insert", length) == 0)) {
	int optargc = 2;
	long id;
	Node *nodePtr = NULL, *new;
	char *end;

	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " insert ?id? ?option value ...?\"",
		(char *) NULL);
	    goto error;
	}
	id = strtoul(argv[2], &end, 0);
	if (*end == 0) {
	    if (end != argv[2]) {
		Tcl_HashEntry *hPtr;

		hPtr = Tcl_FindHashEntry(&treePtr->nodeTable, (char *) id);
		if (hPtr == NULL) {
	    	    Tcl_AppendResult(interp, "no node with id \"", argv[2],
	    	        "\"", (char *) NULL);
		    goto error;
		}
		nodePtr = (Node *) Tcl_GetHashValue(hPtr);
	    }
	    optargc = 3;
	}

	new = (Node *) ckalloc (sizeof (Node));
	new->id = treePtr->idCount++;
	new->level = nodePtr == NULL ? 0 : nodePtr->level + 1;
	new->tree = treePtr;
	new->parent = nodePtr;
	new->next = NULL;
	new->firstChild = new->lastChild = NULL;
	new->tagPtr = new->staticTagSpace;
	new->tagSpace = TAG_SPACE;
	new->numTags = 0;
	new->fg = new->bg = new->attr = -1;
	new->text = NULL;
	new->textWidth = 0;
	new->flags = SHOWCHILDREN;

	if (new->level * 2 > treePtr->leadingSpace) {
	    int *newString;

	    treePtr->leadingSpace = new->level * 8;
	    newString = (int *) ckalloc(treePtr->leadingSpace * sizeof (int));
	    if (treePtr->leadingString != NULL)
	    	ckfree((char *) treePtr->leadingString);
	    treePtr->leadingString = newString;
	}

	result = Ck_ConfigureWidget(interp, treePtr->winPtr,
	    nodeConfigSpecs, argc - optargc, &argv[optargc],
	    (char *) new, CK_CONFIG_ARGV_ONLY);

	if (result == TCL_OK) {
	    Tcl_HashEntry *hPtr;
	    int newHash;
	    char buf[32];

	    hPtr = Tcl_CreateHashEntry(&treePtr->nodeTable,
		(char *) new->id, &newHash);
	    Tcl_SetHashValue(hPtr, (ClientData) new);
	    if (new->parent == NULL) {
		new->level = 0;
		if (treePtr->lastChild == NULL)
		    treePtr->firstChild = new;
		else
		    treePtr->lastChild->next = new;
		treePtr->lastChild = new;
	    } else {
		new->level = nodePtr->level + 1;
		if (nodePtr->lastChild == NULL)
		    nodePtr->firstChild = new;
		else
		    nodePtr->lastChild->next = new;
		nodePtr->lastChild = new;
	    }
	    recompute++;
	    redraw++;
	    sprintf(buf, "%ld", new->id);
	    Tcl_AppendResult(interp, buf, (char *) NULL);
	} else {
	    ckfree((char *) new);
	}
    } else if ((c == 'n') && (strncmp(argv[1], "nodecget", length) == 0)
	    && (length >= 6)) {
	Node *nodePtr;
	TagSearch search;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " nodecget tagOrId option\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	nodePtr = StartTagSearch(treePtr, argv[2], &search);
	if (nodePtr != NULL) {
	    result = Ck_ConfigureValue(treePtr->interp, treePtr->winPtr,
		    nodeConfigSpecs, (char *) nodePtr,
		    argv[3], 0);
	}
    } else if ((c == 'n') && (strncmp(argv[1], "nodeconfigure", length) == 0)
	    && (length >= 6)) {
	Node *nodePtr;
	TagSearch search;

	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " nodeconfigure tagOrId ?option value ...?\"",
		    (char *) NULL);
	    goto error;
	}
	for (nodePtr = StartTagSearch(treePtr, argv[2], &search);
		nodePtr != NULL; nodePtr = NextNode(&search)) {
	    if (argc == 3) {
		result = Ck_ConfigureInfo(treePtr->interp, treePtr->winPtr,
			nodeConfigSpecs, (char *) nodePtr,
			(char *) NULL, 0);
	    } else if (argc == 4) {
		result = Ck_ConfigureInfo(treePtr->interp, treePtr->winPtr,
			nodeConfigSpecs, (char *) nodePtr,
			argv[3], 0);
	    } else {
		result = Ck_ConfigureWidget(interp, treePtr->winPtr,
			nodeConfigSpecs, argc - 3, &argv[3],
			(char *) nodePtr, CK_CONFIG_ARGV_ONLY);
		redraw++;
	    }
	    if ((result != TCL_OK) || (argc < 5)) {
		break;
	    }
	}
    } else if ((c == 'p') && (strncmp(argv[1], "parent", length) == 0)
	    && (length >= 2)) {
	Node *nodePtr;
	TagSearch search;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " parent tagOrId\"",
		    (char *) NULL);
	    goto error;
	}
	nodePtr = StartTagSearch(treePtr, argv[2], &search);
	if (nodePtr == NULL)
	    goto error;
	if (nodePtr->parent != NULL)
	    DoNode(interp, nodePtr->parent, (Ck_Uid) NULL);
	result = TCL_OK;
    } else if ((c == 's') && (strncmp(argv[1], "select", length) == 0)
	    && (length >= 2)) {
	Node *nodePtr;
	TagSearch search;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " select option tagOrId\"", (char *) NULL);
	    goto error;
	}
	nodePtr = StartTagSearch(treePtr, argv[3], &search);
	if (nodePtr == NULL) {
	    Tcl_AppendResult(interp, "can't find a selectable node \"",
		argv[3], "\"", (char *) NULL);
	    goto error;
	}
	length = strlen(argv[2]);
	c = argv[2][0];
	if ((c == 'c') && (argv[2] != NULL) &&
	    (strncmp(argv[2], "clear", length) == 0)) {
	    do {
		nodePtr->flags &= ~SELECTED;
		nodePtr = NextNode(&search);
		redraw++;
	    } while (nodePtr != NULL);
	} else if ((c == 'i') && (strncmp(argv[2], "includes", length) == 0)) {
	    interp->result = (nodePtr->flags & SELECTED) ? "1" : "0";
	} else if ((c == 's') && (strncmp(argv[2], "set", length) == 0)) {
	    do {
		nodePtr->flags |= SELECTED;
		nodePtr = NextNode(&search);
		redraw++;
	    } while (nodePtr != NULL);
	} else {
	    Tcl_AppendResult(interp, "bad select option \"", argv[2],
		    "\": must be clear, includes, or set", (char *) NULL);
	    goto error;
	}
    } else if ((c == 'x') && (strncmp(argv[1], "xview", length) == 0)) {
	int type, count;
	double fraction;

	if (argc == 2) {
	} else {
	    type = Ck_GetScrollInfo(interp, argc, argv, &fraction, &count);
	    switch (type) {
		case CK_SCROLL_ERROR:
		    goto error;
		case CK_SCROLL_MOVETO:
		    break;
		case CK_SCROLL_PAGES:
		    break;
		case CK_SCROLL_UNITS:
		    break;
	    }
	}
    } else if ((c == 'y') && (strncmp(argv[1], "yview", length) == 0)) {
	int type, count, index = 0;
	double fraction;

	if (argc == 2) {
	    if (treePtr->visibleNodes == 0) {
		interp->result = "0 1";
	    } else {
		double fraction2;

		fraction = treePtr->topIndex / (double) treePtr->visibleNodes;
		fraction2 = (treePtr->topIndex + treePtr->winPtr->height) /
		    (double) treePtr->visibleNodes;
		if (fraction2 > 1.0) {
                    fraction2 = 1.0;
                }
                sprintf(interp->result, "%g %g", fraction, fraction2);
	    }
	} else if (argc == 3) {
	    Node *nodePtr;
	    TagSearch search;

	    nodePtr = StartTagSearch(treePtr, argv[2], &search);
	    if (nodePtr == NULL) {
		Tcl_AppendResult(interp, "can't find a selectable node \"",
		    argv[3], "\"", (char *) NULL);
		goto error;
	    }
	    if (GetNodeYCoord(treePtr, nodePtr, &index) == TCL_OK) {
		if (index < treePtr->topIndex ||
		    index >= treePtr->topIndex + treePtr->winPtr->height)
		    ChangeTreeView(treePtr,
		        index - treePtr->winPtr->height / 2);
	    }
	} else {
	    type = Ck_GetScrollInfo(interp, argc, argv, &fraction, &count);
	    switch (type) {
		case CK_SCROLL_ERROR:
		    goto error;
		case CK_SCROLL_MOVETO:
		    index = (int) (treePtr->visibleNodes * fraction + 0.5);
		    break;
		case CK_SCROLL_PAGES:
                    if (treePtr->visibleNodes > 2) {
                        index = treePtr->topIndex
                                + count * (treePtr->winPtr->height - 2);
                    } else {
                        index = treePtr->topIndex + count;
                    }
                    break;
		case CK_SCROLL_UNITS:
		    index = treePtr->topIndex + count;
		    break;
	    }
	    ChangeTreeView(treePtr, index);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\":  must be cget or configure",
		(char *) NULL);
	goto error;
    }
    if (recompute)
	RecomputeVisibleNodes(treePtr);
    if (redraw)
	TreeEventuallyRedraw(treePtr);

    Ck_Release((ClientData) treePtr);
    return result;

error:
    Ck_Release((ClientData) treePtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyTree --
 *
 *	This procedure is invoked to recycle all of the resources
 *	associated with a tree widget.  It is invoked as a
 *	when-idle handler in order to make sure that there is no
 *	other use of the tree pending at the time of the deletion.
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
DestroyTree(clientData)
    ClientData clientData;	/* Info about tree widget. */
{
    Tree *treePtr = (Tree *) clientData;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Node *nodePtr;

    /*
     * Free up all the stuff that requires special handling, then
     * let Ck_FreeOptions handle all the standard option-related
     * stuff.
     */

    if (treePtr->leadingString != NULL) {
	ckfree((char *) treePtr->leadingString);
	treePtr->leadingString = NULL;
    }

    hPtr = Tcl_FirstHashEntry(&treePtr->nodeTable, &search);
    while (hPtr != NULL) {
	nodePtr = (Node *) Tcl_GetHashValue(hPtr);
	Ck_FreeOptions(nodeConfigSpecs, (char *) nodePtr, 0);
	if (nodePtr->tagPtr != nodePtr->staticTagSpace)
	    ckfree((char *) nodePtr->tagPtr);
	ckfree((char *) nodePtr);
	hPtr = Tcl_NextHashEntry(&search);
    }
    Tcl_DeleteHashTable(&treePtr->nodeTable);

    Ck_FreeOptions(configSpecs, (char *) treePtr, 0);
    ckfree((char *) treePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureTree --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the option database, in order to configure (or
 *	reconfigure) a tree widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for treePtr;  old resources get freed, if there
 *	were any.  The tree is redisplayed.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureTree(interp, treePtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tree *treePtr;		/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Ck_ConfigureWidget. */
{
    int result, width, height;

    result = Ck_ConfigureWidget(interp, treePtr->winPtr, configSpecs,
	    argc, argv, (char *) treePtr, flags);
    if (result != TCL_OK)
	return TCL_ERROR;
    width = treePtr->width;
    if (width <= 0)
	width = 1;
    height = treePtr->height;
    if (height <= 0)
	height = 1;
    Ck_GeometryRequest(treePtr->winPtr, width, height);
    TreeEventuallyRedraw(treePtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeEventuallyRedraw --
 *
 *	This procedure is called to dispatch a do-when-idle
 *	handler for redrawing the tree.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
TreeEventuallyRedraw(treePtr)
    Tree *treePtr;
{
    if ((treePtr->winPtr->flags & CK_MAPPED)
	&& !(treePtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplayTree, (ClientData) treePtr);
	treePtr->flags |= REDRAW_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteNode --
 *
 *	This procedure is called to delete a node and its children.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteNode(treePtr, nodePtr)
    Tree *treePtr;
    Node *nodePtr;
{
    Node *childPtr, *thisPtr, *prevPtr;
    Tcl_HashEntry *hPtr;

    while ((childPtr = nodePtr->firstChild) != NULL)
	DeleteNode(treePtr, childPtr);

    if (treePtr->topNode == nodePtr)
	treePtr->topNode = nodePtr->parent;
    if (treePtr->activeNode == nodePtr)
	treePtr->activeNode = NULL;

    prevPtr = NULL;
    if (nodePtr->parent == NULL) {
	thisPtr = treePtr->firstChild;
    } else {
	thisPtr = nodePtr->parent->firstChild;
    }
    for (; thisPtr != NULL; prevPtr = thisPtr, thisPtr = thisPtr->next) {
	if (thisPtr == nodePtr) {
	    if (prevPtr == NULL) {
		if (nodePtr->parent == NULL)
		    treePtr->firstChild = nodePtr->next;
		else
		    nodePtr->parent->firstChild = nodePtr->next;
	    } else
		prevPtr->next = nodePtr->next;
	    if (nodePtr->next == NULL) {
		if (nodePtr->parent == NULL)
		    treePtr->lastChild = prevPtr;
		else
		    nodePtr->parent->lastChild = prevPtr;
	    }
	    break;
	}
    }

    hPtr = Tcl_FindHashEntry(&treePtr->nodeTable, (char *) nodePtr->id);
    Tcl_DeleteHashEntry(hPtr);
    Ck_FreeOptions(nodeConfigSpecs, (char *) nodePtr, 0);
    if (nodePtr->tagPtr != nodePtr->staticTagSpace)
	ckfree((char *) nodePtr->tagPtr);
    ckfree((char *) nodePtr);
}


static void
DeleteActiveTag(treePtr)
    Tree *treePtr;
{
    int i;
    Node *nodePtr = treePtr->activeNode;

    if (nodePtr == NULL)
	return;

    for (i = nodePtr->numTags-1; i >= 0; i--) {
	if (nodePtr->tagPtr[i] == activeUid) {
	    nodePtr->tagPtr[i] = nodePtr->tagPtr[nodePtr->numTags-1];
	    nodePtr->numTags--;
	}
    }
    treePtr->activeNode = NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * DisplayTree --
 *
 *	This procedure is invoked to display a tree widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to display the tree.
 *
 *----------------------------------------------------------------------
 */

static void
DisplayTree(clientData)
    ClientData clientData;	/* Information about widget. */
{
    Tree *treePtr = (Tree *) clientData;
    CkWindow *winPtr = treePtr->winPtr;
    Node *nodePtr, *nextPtr, *parentPtr;
    int i, x, y, mustRestore;
    long rarrow;
    long ulcorner, urcorner, llcorner, lrcorner, lvline, lhline, ltee, ttee;
    WINDOW *window;

    treePtr->flags &= ~REDRAW_PENDING;
    if ((treePtr->winPtr == NULL) || !(winPtr->flags & CK_MAPPED)) {
	return;
    }
    if (treePtr->flags & UPDATE_V_SCROLLBAR) {
        TreeUpdateVScrollbar(treePtr);
    }
    treePtr->flags &= ~(REDRAW_PENDING|UPDATE_H_SCROLLBAR|UPDATE_V_SCROLLBAR);

    if (treePtr->firstChild == NULL) {
	Ck_ClearToBot(winPtr, 0, 0);
	Ck_EventuallyRefresh(winPtr);
	return;
    }

    Ck_GetGChar(NULL, "rarrow", &rarrow);
    Ck_GetGChar(NULL, "ulcorner", &ulcorner);
    Ck_GetGChar(NULL, "urcorner", &urcorner);
    Ck_GetGChar(NULL, "llcorner", &llcorner);
    Ck_GetGChar(NULL, "lrcorner", &lrcorner);
    Ck_GetGChar(NULL, "vline", &lvline);
    Ck_GetGChar(NULL, "hline", &lhline);
    Ck_GetGChar(NULL, "ltee", &ltee);
    Ck_GetGChar(NULL, "ttee", &ttee);

    Ck_SetWindowAttr(winPtr, treePtr->normalFg, treePtr->normalBg,
	treePtr->normalAttr);

    nodePtr = treePtr->topNode;

    i = nodePtr->level * 2 - 1;
    nextPtr = nodePtr->parent;
    while (i >= 0) {
        treePtr->leadingString[i] = ' ';
    	parentPtr = nextPtr->parent;
    	if (parentPtr != NULL) {
	    if (parentPtr->lastChild == nextPtr)
	        treePtr->leadingString[i - 1] = ' ';
	    else
	        treePtr->leadingString[i - 1] = lvline;
	} else if (treePtr->lastChild == nextPtr)
	    treePtr->leadingString[i - 1] = ' ';
	else
	    treePtr->leadingString[i - 1] = lvline;
	nextPtr = parentPtr;
	i -= 2;
    }

    window = winPtr->window;
    y = 0;
    while (nodePtr != NULL && y < winPtr->height) {
    	x = mustRestore = 0;
	wmove(window, y, x);
	if (nodePtr == treePtr->firstChild) {
    	    waddch(window, (nodePtr == treePtr->lastChild) ? lhline : ulcorner);
	    x++;
	} else if (nodePtr == treePtr->lastChild) {
	    waddch(window, llcorner);
	    x++;
	} else if (nodePtr->level == 0) {
	    waddch(window, ltee);
	    x++;
	}
	for (i = 0; i < nodePtr->level * 2 && x < winPtr->width; i++) {
	    waddch(window, treePtr->leadingString[i]);
	    x++;
	}
	if (nodePtr->parent != NULL) {
	   if (x < winPtr->width) {
		if (nodePtr == nodePtr->parent->lastChild)
	    	    waddch(window, llcorner);
		else
	    	    waddch(window, ltee);
		x++;
	    }
	}
	if (x < winPtr->width) {
	    waddch(window, lhline);
	    x++;
	}
	if (nodePtr->firstChild != NULL && (nodePtr->flags & SHOWCHILDREN)) {
	    if (x < winPtr->width) {
		waddch(window, ttee);
		x++;
	    }
	    nextPtr = nodePtr->firstChild;
	} else {
	    if (x < winPtr->width) {
		waddch(window, (nodePtr->firstChild == NULL) ? lhline : rarrow);
		x++;
	    }
	    nextPtr = nodePtr->next;
	    if (nextPtr == NULL) {
		parentPtr = nodePtr->parent;
		while (nextPtr == NULL) {
		    if (parentPtr == NULL) {
		    	break;
		    }
		    nextPtr = parentPtr->next;
		    if (nextPtr == NULL) {
		    	parentPtr = parentPtr->parent;
		    }
		}	        
	    }
	}
	if (x < winPtr->width) {
	    waddch(window, ' ');
	    x++;
	}

	if (nodePtr == treePtr->activeNode && (treePtr->flags & GOT_FOCUS)) {
	    Ck_SetWindowAttr(winPtr, treePtr->activeFg, treePtr->activeBg,
		treePtr->activeAttr | ((nodePtr->flags & SELECTED) ? 
		treePtr->selectAttr : 0));
	    mustRestore = 1;
	} else if (nodePtr->flags & SELECTED) {
	    Ck_SetWindowAttr(winPtr, treePtr->selectFg, treePtr->selectBg,
		treePtr->selectAttr);
	    mustRestore = 1;
	}
	CkDisplayChars(winPtr->mainPtr, window,
	    nodePtr->text, strlen(nodePtr->text),
	    x, y, 0,
	    CK_NEWLINES_NOT_SPECIAL | CK_IGNORE_TABS);
	if (mustRestore)
	    Ck_SetWindowAttr(winPtr, treePtr->normalFg, treePtr->normalBg,
		treePtr->normalAttr);
	Ck_ClearToEol(winPtr, -1, -1);

	i = nodePtr->level * 2;
	treePtr->leadingString[i] = nodePtr->next != NULL ? lvline : ' ';
	treePtr->leadingString[i + 1] = ' ';

	nodePtr = nextPtr;
	y++;
    }
    if (y < winPtr->height)
	Ck_ClearToBot(winPtr, 0, y);
    Ck_EventuallyRefresh(winPtr);
}

/*
 *--------------------------------------------------------------
 *
 * TreeEventProc --
 *
 *	This procedure is invoked by the dispatcher for various
 *	events on trees.
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
TreeEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    CkEvent *eventPtr;		/* Information about event. */
{
    Tree *treePtr = (Tree *) clientData;

    if (eventPtr->type == CK_EV_EXPOSE) {
	TreeEventuallyRedraw(treePtr);
    } else if (eventPtr->type == CK_EV_DESTROY) {
	if (treePtr->winPtr != NULL) {
	    treePtr->winPtr = NULL;
	    Tcl_DeleteCommand(treePtr->interp,
		    Tcl_GetCommandName(treePtr->interp, treePtr->widgetCmd));
	}
	if (treePtr->flags & REDRAW_PENDING) {
	    Tk_CancelIdleCall(DisplayTree, (ClientData) treePtr);
	}
	Ck_EventuallyFree((ClientData) treePtr, (Ck_FreeProc *) DestroyTree);
    } else if (eventPtr->type == CK_EV_FOCUSIN) {
    	treePtr->flags |= GOT_FOCUS;
    	TreeEventuallyRedraw(treePtr);
    } else if (eventPtr->type == CK_EV_FOCUSOUT) {
    	treePtr->flags &= ~GOT_FOCUS;
    	TreeEventuallyRedraw(treePtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeCmdDeletedProc --
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
TreeCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    Tree *treePtr = (Tree *) clientData;
    CkWindow *winPtr = treePtr->winPtr;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case winPtr
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (winPtr != NULL) {
	treePtr->winPtr = NULL;
	Ck_DestroyWindow(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RecomputeVisibleNodes --
 *
 *	Display parameters are recomputed.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
RecomputeVisibleNodes(treePtr)
    Tree *treePtr;
{
    int count = 0, top = -1;
    Node *nodePtr, *nextPtr = NULL;

    nodePtr = treePtr->firstChild;
    if (nodePtr == NULL)
    	treePtr->topNode = NULL;
    while (nodePtr != NULL) {
	if (nodePtr->parent == NULL)
	    nextPtr = nodePtr->next;
    	if (nodePtr == treePtr->topNode)
	    top = count;
    	if (nodePtr->firstChild != NULL && (nodePtr->flags & SHOWCHILDREN)) {
	    nodePtr = nodePtr->firstChild;
	} else if (nodePtr->next != NULL)
	    nodePtr = nodePtr->next;
	else {
	    while (nodePtr != NULL) {
		nodePtr = nodePtr->parent;
		if (nodePtr != NULL && nodePtr->next != NULL) {
		    nodePtr = nodePtr->next;
		    break;
		}
	    }
	    if (nodePtr == NULL)
		nodePtr = nextPtr;
	}
	count++;
    }
    if (top < 0) {
    	treePtr->topNode = treePtr->firstChild;
    	top = 0;
    }
    if (top != treePtr->topIndex || count != treePtr->visibleNodes)
	treePtr->flags |= UPDATE_V_SCROLLBAR;
    treePtr->topIndex = top;
    treePtr->visibleNodes = count;
}    

/*
 *----------------------------------------------------------------------
 *
 * ChangeTreeView --
 *
 *	Change the vertical view on a tree widget so that a given element
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
ChangeTreeView(treePtr, index)
    Tree *treePtr;			/* Information about widget. */
    int index;				/* Index of element in treePtr
					 * that should now appear at the
					 * top of the tree. */
{
    int count;
    Node *nodePtr, *nextPtr = NULL;

    if (index >= treePtr->visibleNodes - treePtr->winPtr->height)
	index = treePtr->visibleNodes - treePtr->winPtr->height;
    if (index < 0)
	index = 0;
    if (treePtr->topIndex != index) {
	if (index < treePtr->topIndex) {
	    count = 0;
	    nodePtr = treePtr->firstChild;
	} else {
	    count = treePtr->topIndex;
	    nodePtr = treePtr->topNode;
	}
	while (nodePtr != NULL) {
	    if (nodePtr->parent == NULL)
		nextPtr = nodePtr->next;
	    if (count == index)
		break;
	    if (nodePtr->firstChild != NULL &&
		(nodePtr->flags & SHOWCHILDREN)) {
		nodePtr = nodePtr->firstChild;
	    } else if (nodePtr->next != NULL)
		nodePtr = nodePtr->next;
	    else {
		while (nodePtr != NULL) {
		    nodePtr = nodePtr->parent;
		    if (nodePtr != NULL && nodePtr->next != NULL) {
			nodePtr = nodePtr->next;
			break;
		    }
		}
		if (nodePtr == NULL)
		    nodePtr = nextPtr;
	    }
	    count++;
	}
	treePtr->topNode = nodePtr;
	treePtr->topIndex = count;
	treePtr->flags |= UPDATE_V_SCROLLBAR;
	TreeEventuallyRedraw(treePtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetNodeYCoord --
 *
 *	Given node return the window Y coordinate corresponding to it.
 *
 *----------------------------------------------------------------------
 */

static int
GetNodeYCoord(treePtr, thisPtr, yPtr)
    Tree *treePtr;			/* Information about widget. */
    Node *thisPtr;
    int *yPtr;
{
    int count;
    Node *nodePtr, *nextPtr = NULL;

    count = 0;
    nodePtr = treePtr->firstChild;
    while (nodePtr != NULL) {
	if (thisPtr == nodePtr) {
	    *yPtr = count;
	    return TCL_OK;
	}
	if (nodePtr->parent == NULL)
	    nextPtr = nodePtr->next;
	if (nodePtr->firstChild != NULL && (nodePtr->flags & SHOWCHILDREN)) {
	    nodePtr = nodePtr->firstChild;
	} else if (nodePtr->next != NULL)
	    nodePtr = nodePtr->next;
	else {
	    while (nodePtr != NULL) {
		nodePtr = nodePtr->parent;
		if (nodePtr != NULL && nodePtr->next != NULL) {
		    nodePtr = nodePtr->next;
		    break;
		}
	    }
	    if (nodePtr == NULL)
		nodePtr = nextPtr;
	}
	count++;
    }
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * TreeTagsParseProc --
 *
 *	This procedure is invoked during option processing to handle
 *	"-tags" options for tree nodes.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *	The tags for a given node get replaced by those indicated
 *	in the value argument.
 *
 *--------------------------------------------------------------
 */

static int
TreeTagsParseProc(clientData, interp, winPtr, value, widgRec, offset)
    ClientData clientData;		/* Not used.*/
    Tcl_Interp *interp;			/* Used for reporting errors. */
    CkWindow *winPtr;			/* Window containing tree widget. */
    char *value;			/* Value of option (list of tag
					 * names). */
    char *widgRec;			/* Pointer to record for item. */
    int offset;				/* Offset into item (ignored). */
{
    Node *nodePtr = (Node *) widgRec, *activeNode = NULL;
    int argc, i, hideChildren = 0, redraw = 0, recompute = 0;
    char **argv;
    Ck_Uid *newPtr;

    /*
     * Break the value up into the individual tag names.
     */

    if (Tcl_SplitList(interp, value, &argc, &argv) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Check for special tags.
     */
    for (i = 0; i < nodePtr->numTags; i++) {
	if (nodePtr->tagPtr[i] == activeUid) {
	    DeleteActiveTag(nodePtr->tree);
	    redraw++;
	}
    }

    /*
     * Make sure that there's enough space in the node to hold the
     * tag names.
     */

    if (nodePtr->tagSpace < argc) {
	newPtr = (Ck_Uid *) ckalloc((unsigned) (argc * sizeof(Ck_Uid)));
	for (i = nodePtr->numTags-1; i >= 0; i--) {
	    newPtr[i] = nodePtr->tagPtr[i];
	}
	if (nodePtr->tagPtr != nodePtr->staticTagSpace) {
	    ckfree((char *) nodePtr->tagPtr);
	}
	nodePtr->tagPtr = newPtr;
	nodePtr->tagSpace = argc;
    }
    nodePtr->numTags = argc;
    for (i = 0; i < argc; i++) {
	nodePtr->tagPtr[i] = Ck_GetUid(argv[i]);
	if (nodePtr->tagPtr[i] == hideChildrenUid)
	    hideChildren++;
	else if (nodePtr->tagPtr[i] == activeUid)
	    activeNode = nodePtr;
    }
    ckfree((char *) argv);
    if (hideChildren && (nodePtr->flags & SHOWCHILDREN)) {
	nodePtr->flags &= ~SHOWCHILDREN;
	recompute++;
	redraw++;
    } else if (!hideChildren && !(nodePtr->flags & SHOWCHILDREN)) {
	nodePtr->flags |= SHOWCHILDREN;
	recompute++;
	redraw++;
    }
    if (activeNode != NULL) {
	DeleteActiveTag(nodePtr->tree);
	nodePtr->tree->activeNode = activeNode;
	redraw++;
    }
    if (recompute)
	RecomputeVisibleNodes(nodePtr->tree);
    if (redraw)
	TreeEventuallyRedraw(nodePtr->tree);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TreeTagsPrintProc --
 *
 *	This procedure is invoked by the Ck configuration code
 *	to produce a printable string for the "-tags" configuration
 *	option for tree nodes.
 *
 * Results:
 *	The return value is a string describing all the tags for
 *	the node referred to by "widgRec".  In addition, *freeProcPtr
 *	is filled in with the address of a procedure to call to free
 *	the result string when it's no longer needed (or NULL to
 *	indicate that the string doesn't need to be freed).
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static char *
TreeTagsPrintProc(clientData, winPtr, widgRec, offset, freeProcPtr)
    ClientData clientData;		/* Ignored. */
    CkWindow *winPtr;			/* Window containing tree widget. */
    char *widgRec;			/* Pointer to record for item. */
    int offset;				/* Ignored. */
    Tcl_FreeProc **freeProcPtr;		/* Pointer to variable to fill in with
					 * information about how to reclaim
					 * storage for return string. */
{
    Node *nodePtr = (Node *) widgRec;

    if (nodePtr->numTags == 0) {
	*freeProcPtr = (Tcl_FreeProc *) NULL;
	return "";
    }
    if (nodePtr->numTags == 1) {
	*freeProcPtr = (Tcl_FreeProc *) NULL;
	return (char *) nodePtr->tagPtr[0];
    }
    *freeProcPtr = (Tcl_FreeProc *) free;
    return Tcl_Merge(nodePtr->numTags, (char **) nodePtr->tagPtr);
}

/*
 *--------------------------------------------------------------
 *
 * StartTagSearch --
 *
 *	This procedure is called to initiate an enumeration of
 *	all nodes in a given tree that contain a given tag.
 *
 * Results:
 *	The return value is a pointer to the first node in
 *	treePtr that matches tag, or NULL if there is no
 *	such node.  The information at *searchPtr is initialized
 *	such that successive calls to NextNode will return
 *	successive nodes that match tag.
 *
 * Side effects:
 *	SearchPtr is linked into a list of searches in progress
 *	on treePtr, so that elements can safely be deleted
 *	while the search is in progress.  EndTagSearch must be
 *	called at the end of the search to unlink searchPtr from
 *	this list.
 *
 *--------------------------------------------------------------
 */

static Node *
StartTagSearch(treePtr, tag, searchPtr)
    Tree *treePtr;			/* Tree whose nodes are to be
					 * searched. */
    char *tag;				/* String giving tag value. */
    TagSearch *searchPtr;		/* Record describing tag search;
					 * will be initialized here. */
{
    long id;
    Tcl_HashEntry *hPtr;
    Node *nodePtr;
    Ck_Uid *tagPtr;
    Ck_Uid uid;
    int count;

    /*
     * Initialize the search.
     */

    nodePtr = NULL;
    searchPtr->treePtr = treePtr;
    searchPtr->searchOver = 0;

    /*
     * Find the first matching node in one of several ways. If the tag
     * is a number then it selects the single node with the matching
     * identifier.
     */

    if (isdigit((unsigned char) (*tag))) {
	char *end;

	id = strtoul(tag, &end, 0);
	if (*end == 0) {

	    hPtr = Tcl_FindHashEntry(&treePtr->nodeTable, (char *) id);
	    if (hPtr != NULL)
		nodePtr = (Node *) Tcl_GetHashValue(hPtr);
	    searchPtr->searchOver = 1;
	    return nodePtr;
	}
    }

    hPtr = Tcl_FirstHashEntry(&treePtr->nodeTable, &searchPtr->search);
    if (hPtr == NULL) {
	searchPtr->searchOver = 1;
	return nodePtr;
    }
    searchPtr->tag = uid = Ck_GetUid(tag);
    if (uid == allUid) {

	/*
	 * All nodes match.
	 */

	searchPtr->tag = NULL;
	return (Node *) Tcl_GetHashValue(hPtr);
    }

    do {
	nodePtr = (Node *) Tcl_GetHashValue(hPtr);
	for (tagPtr = nodePtr->tagPtr, count = nodePtr->numTags;
	     count > 0;
	     tagPtr++, count--) {
	    if (*tagPtr == uid) {
		return nodePtr;
	    }
	}
	hPtr = Tcl_NextHashEntry(&searchPtr->search);
    } while (hPtr != NULL);

    searchPtr->searchOver = 1;
    return NULL;
}

/*
 *--------------------------------------------------------------
 *
 * NextNode --
 *
 *	This procedure returns successive nodes that match a given
 *	tag;  it should be called only after StartTagSearch has been
 *	used to begin a search.
 *
 * Results:
 *	The return value is a pointer to the next node that matches
 *	the tag specified to StartTagSearch, or NULL if no such
 *	node exists.  *SearchPtr is updated so that the next call
 *	to this procedure will return the next node.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static Node *
NextNode(searchPtr)
    TagSearch *searchPtr;		/* Record describing search in
					 * progress. */
{
    Node *nodePtr;
    Tcl_HashEntry *hPtr;
    int count;
    Ck_Uid uid;
    Ck_Uid *tagPtr;

    if (searchPtr->searchOver)
	return NULL;
    
    hPtr = Tcl_NextHashEntry(&searchPtr->search);
    if (hPtr == NULL) {
	searchPtr->searchOver = 1;
	return NULL;
    }

    /*
     * Handle special case of "all" search by returning next node.
     */

    uid = searchPtr->tag;
    if (uid == NULL) {
	return (Node *) Tcl_GetHashValue(hPtr);
    }

    /*
     * Look for a node with a particular tag.
     */

    do {
	nodePtr = (Node *) Tcl_GetHashValue(hPtr);
	for (tagPtr = nodePtr->tagPtr, count = nodePtr->numTags;
	     count > 0;
	     tagPtr++, count--) {
	    if (*tagPtr == uid) {
		return nodePtr;
	    }
	}
	hPtr = Tcl_NextHashEntry(&searchPtr->search);
    } while (hPtr != NULL);

    searchPtr->searchOver = 1;
    return NULL;
}

/*
 *--------------------------------------------------------------
 *
 * DoNode --
 *
 *	This is a utility procedure called by FindNodes.  It
 *	either adds nodePtr's id to the result forming in interp,
 *	or it adds a new tag to nodePtr, depending on the value
 *	of tag.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If tag is NULL then nodePtr's id is added as a list element
 *	to interp->result;  otherwise tag is added to nodePtr's
 *	list of tags.
 *
 *--------------------------------------------------------------
 */

static void
DoNode(interp, nodePtr, tag)
    Tcl_Interp *interp;			/* Interpreter in which to (possibly)
					 * record node id. */
    Node *nodePtr;			/* Node to (possibly) modify. */
    Ck_Uid tag;				/* Tag to add to those already
					 * present for node, or NULL. */
{
    Ck_Uid *tagPtr;
    int count;

    /*
     * Handle the "add-to-result" case and return, if appropriate.
     */

    if (tag == NULL) {
	char msg[30];

	sprintf(msg, "%ld", nodePtr->id);
	Tcl_AppendElement(interp, msg);
	return;
    }

    for (tagPtr = nodePtr->tagPtr, count = nodePtr->numTags;
	    count > 0; tagPtr++, count--) {
	if (tag == *tagPtr)
	    return;
    }

    /*
     * Grow the tag space if there's no more room left in the current
     * block.
     */

    if (nodePtr->tagSpace == nodePtr->numTags) {
	Ck_Uid *newTagPtr;

	nodePtr->tagSpace += TAG_SPACE;
	newTagPtr = (Ck_Uid *) ckalloc((unsigned)
		(nodePtr->tagSpace * sizeof (Ck_Uid)));
	memcpy(newTagPtr, nodePtr->tagPtr, nodePtr->numTags * sizeof (Ck_Uid));
	if (nodePtr->tagPtr != nodePtr->staticTagSpace) {
	    ckfree((char *) nodePtr->tagPtr);
	}
	nodePtr->tagPtr = newTagPtr;
	tagPtr = &nodePtr->tagPtr[nodePtr->numTags];
    }

    /*
     * Add in the new tag.
     */

    *tagPtr = tag;
    nodePtr->numTags++;

    if (tag == activeUid) {
	DeleteActiveTag(nodePtr->tree);
	nodePtr->tree->activeNode = nodePtr;
	TreeEventuallyRedraw(nodePtr->tree);
    } else if (tag == hideChildrenUid) {
	nodePtr->flags &= ~SHOWCHILDREN;
	RecomputeVisibleNodes(nodePtr->tree);
	TreeEventuallyRedraw(nodePtr->tree);
    }
}

/*
 *--------------------------------------------------------------
 *
 * FindNodes --
 *
 *	This procedure does all the work of implementing the
 *	"find" and "addtag" options of the tree widget command,
 *	which locate nodes that have certain features (location,
 *	tags).
 *
 * Results:
 *	A standard Tcl return value.  If newTag is NULL, then a
 *	list of ids from all the nodes that match argc/argv is
 *	returned in interp->result.  If newTag is NULL, then
 *	the normal interp->result is an empty string.  If an error
 *	occurs, then interp->result will hold an error message.
 *
 * Side effects:
 *	If newTag is non-NULL, then all the nodes that match the
 *	information in argc/argv have that tag added to their
 *	lists of tags.
 *
 *--------------------------------------------------------------
 */

static int
FindNodes(interp, treePtr, argc, argv, newTag, cmdName, option)
    Tcl_Interp *interp;			/* Interpreter for error reporting. */
    Tree *treePtr;			/* Tree whose nodes are to be
					 * searched. */
    int argc;				/* Number of entries in argv.  Must be
					 * greater than zero. */
    char **argv;			/* Arguments that describe what items
					 * to search for (see user doc on
					 * "find" and "addtag" options). */
    char *newTag;			/* If non-NULL, gives new tag to set
					 * on all found items;  if NULL, then
					 * ids of found items are returned
					 * in interp->result. */
    char *cmdName;			/* Name of original Tcl command, for
					 * use in error messages. */
    char *option;			/* For error messages:  gives option
					 * from Tcl command and other stuff
					 * up to what's in argc/argv. */
{
    int c;
    size_t length;
    TagSearch search;
    Node *nodePtr;
    Ck_Uid uid;

    if (newTag != NULL) {
	uid = Ck_GetUid(newTag);
    } else {
	uid = NULL;
    }
    c = argv[0][0];
    length = strlen(argv[0]);
    if ((c == 'a') && (strncmp(argv[0], "all", length) == 0)
	    && (length >= 2)) {
	if (argc != 1) {
	    Tcl_AppendResult(interp, "wrong # args:  must be \"",
		    cmdName, option, " all", (char *) NULL);
	    return TCL_ERROR;
	}
	for (nodePtr = StartTagSearch(treePtr, "all", &search);
		nodePtr != NULL; nodePtr = NextNode(&search)) {
	    DoNode(interp, nodePtr, uid);
	}
    } else if ((c == 'n') && (strncmp(argv[0], "next", length) == 0) &&
	length > 2) {

	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args:  must be \"",
		    cmdName, option, " next tagOrId", (char *) NULL);
	    return TCL_ERROR;
	}
	nodePtr = StartTagSearch(treePtr, argv[1], &search);
	if (nodePtr == NULL)
	    nodePtr = treePtr->firstChild;
	if (nodePtr != NULL) {
	    if (nodePtr->firstChild != NULL && (nodePtr->flags & SHOWCHILDREN))
		nodePtr = nodePtr->firstChild;
	    else if (nodePtr->next != NULL)
		nodePtr = nodePtr->next;
	    else {
		while (nodePtr != NULL) {
		    nodePtr = nodePtr->parent;
		    if (nodePtr != NULL && nodePtr->next != NULL) {
			nodePtr = nodePtr->next;
			break;
		    }
		}
	    }
	    if (nodePtr != NULL)
		DoNode(interp, nodePtr, uid);
	}
    } else if ((c == 'n') && (strncmp(argv[0], "nearest", length) == 0) &&
	length > 2) {
	int x, y, count;
	Node *nextPtr = NULL;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args:  must be \"",
		    cmdName, option, " nearest x y", (char *) NULL);
	    return TCL_ERROR;
	}
	if (Ck_GetCoord(interp, treePtr->winPtr, argv[1], &x) != TCL_OK ||
	    Ck_GetCoord(interp, treePtr->winPtr, argv[2], &y) != TCL_OK)
	    return TCL_ERROR;
	if (y >= treePtr->winPtr->height)
	    y = treePtr->winPtr->height - 1;

	count = 0;
	nodePtr = treePtr->topNode;
	while (nodePtr != NULL) {
	    if (count == y)
		break;
	    if (nodePtr->parent == NULL)
		nextPtr = nodePtr->next;
	    if (nodePtr->firstChild != NULL && 
	        (nodePtr->flags & SHOWCHILDREN)) {
		nodePtr = nodePtr->firstChild;
	    } else if (nodePtr->next != NULL)
		nodePtr = nodePtr->next;
	    else {
		while (nodePtr != NULL) {
		    nodePtr = nodePtr->parent;
		    if (nodePtr != NULL && nodePtr->next != NULL) {
			nodePtr = nodePtr->next;
			break;
		    }
		}
		if (nodePtr == NULL)
		    nodePtr = nextPtr;
	    }
	    count++;
	}
	if (nodePtr != NULL)
	    sprintf(interp->result, "%ld", nodePtr->id);
    } else if ((c == 'p') && (strncmp(argv[0], "prev", length) == 0)) {
    	int done = 0;
	Node *parentPtr, *nextPtr;

	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args:  must be \"",
		    cmdName, option, " prev tagOrId", (char *) NULL);
	    return TCL_ERROR;
	}
	nodePtr = StartTagSearch(treePtr, argv[1], &search);
	if (nodePtr == NULL)
	    nodePtr = treePtr->firstChild;
	if (nodePtr != NULL) {
	    parentPtr = nodePtr->parent;
	    if (parentPtr != NULL) {
		if (nodePtr == parentPtr->firstChild) {
		    nextPtr = parentPtr;
		    done = 1;
		} else
		    nextPtr = parentPtr->firstChild;
	    } else
	    	parentPtr = nextPtr = treePtr->firstChild;
	    if (!done) {
		for (;nextPtr != NULL && nextPtr->next != nodePtr;
		     nextPtr = nextPtr->next) {
		    /* Empty loop body. */
		}
		if (nextPtr == NULL)
		    nextPtr = parentPtr->parent;
		if (nextPtr == NULL)
		    nextPtr = treePtr->firstChild;
		else {
		    while (nextPtr->lastChild != NULL &&
			   (nextPtr->flags & SHOWCHILDREN))
			nextPtr = nextPtr->lastChild;
		}
	    }
	    DoNode(interp, nextPtr, uid);
	}
    } else if ((c == 'w') && (strncmp(argv[0], "withtag", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args:  must be \"",
		    cmdName, option, " withtag tagOrId", (char *) NULL);
	    return TCL_ERROR;
	}
	for (nodePtr = StartTagSearch(treePtr, argv[1], &search);
		nodePtr != NULL; nodePtr = NextNode(&search)) {
	    DoNode(interp, nodePtr, uid);
	}
    } else  {
	Tcl_AppendResult(interp, "bad search command \"", argv[0],
		"\": must be all, nearest, or withtag", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeUpdateVScrollbar --
 *
 *	This procedure is invoked whenever information has changed in
 *	a tree in a way that would invalidate a vertical scrollbar
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
TreeUpdateVScrollbar(treePtr)
    Tree *treePtr;		/* Information about widget. */
{
    char string[100];
    double first, last;
    int result;

    if (treePtr->yScrollCmd == NULL) {
	return;
    }
    if (treePtr->visibleNodes == 0) {
	first = 0.0;
	last = 1.0;
    } else {
	first = treePtr->topIndex / ((double) treePtr->visibleNodes);
	last = (treePtr->topIndex + treePtr->winPtr->height)
		/ ((double) treePtr->visibleNodes);
	if (last > 1.0) {
	    last = 1.0;
	}
    }
    sprintf(string, " %g %g", first, last);
    result = Tcl_VarEval(treePtr->interp, treePtr->yScrollCmd, string,
	    (char *) NULL);
    if (result != TCL_OK) {
	Tcl_AddErrorInfo(treePtr->interp,
		"\n    (vertical scrolling command executed by tree)");
	Tk_BackgroundError(treePtr->interp);
    }
}
