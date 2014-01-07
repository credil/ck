/* 
 * ckGet.c --
 *
 *	This file contains a number of "Ck_GetXXX" procedures, which
 *	parse text strings into useful forms for Ck.
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

typedef struct {
    short fg, bg;
} CPair;

static CPair *cPairs = NULL;
static int numPairs, newPair;

/*
 * The hash table below is used to keep track of all the Ck_Uids created
 * so far.
 */

static Tcl_HashTable uidTable;
static int initialized = 0;

static struct {
    char *name;
    int value;
} ctab[] = {
    { "black", COLOR_BLACK },
    { "blue", COLOR_BLUE },
    { "cyan", COLOR_CYAN },
    { "green", COLOR_GREEN },
    { "magenta", COLOR_MAGENTA },
    { "red", COLOR_RED },
    { "white", COLOR_WHITE },
    { "yellow", COLOR_YELLOW }
};

static struct {
    char *name;
    int value;
} atab[] = {
    { "blink", A_BLINK },
    { "bold", A_BOLD },
    { "dim", A_DIM },
    { "normal", A_NORMAL },
    { "reverse", A_REVERSE },
    { "standout", A_STANDOUT },
    { "underline", A_UNDERLINE }
};

/*
 *----------------------------------------------------------------------
 *
 * Ck_GetUid --
 *
 *	Given a string, this procedure returns a unique identifier
 *	for the string.
 *
 * Results:
 *	This procedure returns a Ck_Uid corresponding to the "string"
 *	argument.  The Ck_Uid has a string value identical to string
 *	(strcmp will return 0), but it's guaranteed that any other
 *	calls to this procedure with a string equal to "string" will
 *	return exactly the same result (i.e. can compare Ck_Uid
 *	*values* directly, without having to call strcmp on what they
 *	point to).
 *
 * Side effects:
 *	New information may be entered into the identifier table.
 *
 *----------------------------------------------------------------------
 */

Ck_Uid
Ck_GetUid(string)
    char *string;		/* String to convert. */
{
    int dummy;

    if (!initialized) {
	Tcl_InitHashTable(&uidTable, TCL_STRING_KEYS);
	initialized = 1;
    }
    return (Ck_Uid) Tcl_GetHashKey(&uidTable,
	    Tcl_CreateHashEntry(&uidTable, string, &dummy));
}

/*
 *------------------------------------------------------------------------
 *
 * Ck_GetColor --
 *
 *	Given a color specification, return curses color value.
 *
 * Results:
 *	TCL_OK if color found, curses color in *colorPtr.
 *	TCL_ERROR if color not found; interp->result contains an
 *	error message.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */

int
Ck_GetColor(interp, name, colorPtr)
    Tcl_Interp *interp;
    char *name;
    int *colorPtr;
{
    int i, len;

    len = strlen(name);
    if (len > 0)
	for (i = 0; i < sizeof (ctab) / sizeof (ctab[0]); i++)
	    if (strncmp(name, ctab[i].name, len) == 0) {
	    	if (colorPtr != NULL)
		    *colorPtr = ctab[i].value;
		return TCL_OK;
	    }
    Tcl_AppendResult(interp, "bad color \"", name, "\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *------------------------------------------------------------------------
 *
 * Ck_NameOfColor --
 *
 *	Given a curses color, return its name.
 *
 * Results:
 *	String: name of color, or NULL if no valid color.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */

char *
Ck_NameOfColor(color)
    int color;		/* Curses color to get name for */
{
    int i;

    for (i = 0; i < sizeof (ctab) / sizeof (ctab[0]); i++)
	if (ctab[i].value == color)
	    return ctab[i].name;
    return NULL;
}

/*
 *------------------------------------------------------------------------
 *
 * Ck_GetAttr --
 *
 *	Given an attribute specification, return attribute value.
 *
 * Results:
 *	TCL_OK if color found, curses color in *colorPtr.
 *	TCL_ERROR if color not found; interp->result contains an
 *	error message.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */

int
Ck_GetAttr(interp, name, attrPtr)
    Tcl_Interp *interp;
    char *name;
    int *attrPtr;
{
    int i, k, len, largc;
    char **largv;

    if (Tcl_SplitList(interp, name, &largc, &largv) != TCL_OK)
	return TCL_ERROR;
    if (attrPtr != NULL)
	*attrPtr = A_NORMAL;
    if (largc > 1 || (largc == 1 && largv[0][0] != '\0')) {
	for (i = 0; i < largc; i++) {
	    len = strlen(largv[i]);
	    if (len > 0) {
		for (k = 0; k < sizeof (atab) / sizeof (atab[0]); k++)
		    if (strncmp(largv[i], atab[k].name, len) == 0) {
			if (attrPtr != NULL)
		            *attrPtr |= atab[k].value;
			break;
		    }
		if (k >= sizeof (atab) / sizeof (atab[0])) {
		    Tcl_AppendResult(interp, "bad attribute \"",
			name, "\"", (char *) NULL);
		    ckfree((char *) largv);
		    return TCL_ERROR;
		}
	    }
	}
    }
    ckfree((char *) largv);
    return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * Ck_NameOfAttr --
 *
 *	Given an attribute value, return its textual specification.
 *
 * Results:
 *	interp->result contains result or message.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */

char *
Ck_NameOfAttr(attr)
    int attr;
{
    int i;
    char *result;
    Tcl_DString list;

    Tcl_DStringInit(&list);
    if (attr == -1 || attr == A_NORMAL)
        Tcl_DStringAppendElement(&list, "normal");
    else {
	for (i = 0; i < sizeof (atab) / sizeof (atab[0]); i++)
	    if (attr & atab[i].value)
                Tcl_DStringAppendElement(&list, atab[i].name);
    }
    result = ckalloc(Tcl_DStringLength(&list) + 1);
    strcpy(result, Tcl_DStringValue(&list));
    Tcl_DStringFree(&list);
    return result;
}
/*
 *------------------------------------------------------------------------
 *
 * Ck_GetColorPair --
 *
 *	Given background/foreground curses colors, a color pair
 *	is allocated and returned.
 *
 * Results:
 *	TCL_OK if color found, curses color in *colorPtr.
 *	TCL_ERROR if color not found; interp->result contains an
 *	error message.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */

int
Ck_GetPair(winPtr, fg, bg)
    CkWindow *winPtr;
    int fg, bg;
{
    int i;

    if (!(winPtr->mainPtr->flags & CK_HAS_COLOR))
	return COLOR_PAIR(0);
    if (cPairs == NULL) {
	cPairs = (CPair *) ckalloc(sizeof (CPair) * (COLOR_PAIRS + 2));
	numPairs = 0;
	newPair = 1;
    }
    for (i = 1; i < numPairs; i++)
	if (cPairs[i].fg == fg && cPairs[i].bg == bg)
	    return COLOR_PAIR(i);
    i = newPair;
    cPairs[i].fg = fg;
    cPairs[i].bg = bg;
    init_pair((short) i, (short) fg, (short) bg);
    if (++newPair >= COLOR_PAIRS)
	newPair = 1;
    else
	numPairs = newPair;
    return COLOR_PAIR(i);
}

/*
 *--------------------------------------------------------------
 *
 * Ck_GetAnchor --
 *
 *	Given a string, return the corresponding Ck_Anchor.
 *
 * Results:
 *	The return value is a standard Tcl return result.  If
 *	TCL_OK is returned, then everything went well and the
 *	position is stored at *anchorPtr;  otherwise TCL_ERROR
 *	is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Ck_GetAnchor(interp, string, anchorPtr)
    Tcl_Interp *interp;		/* Use this for error reporting. */
    char *string;		/* String describing a direction. */
    Ck_Anchor *anchorPtr;	/* Where to store Ck_Anchor corresponding
				 * to string. */
{
    switch (string[0]) {
	case 'n':
	    if (string[1] == 0) {
		*anchorPtr = CK_ANCHOR_N;
		return TCL_OK;
	    } else if ((string[1] == 'e') && (string[2] == 0)) {
		*anchorPtr = CK_ANCHOR_NE;
		return TCL_OK;
	    } else if ((string[1] == 'w') && (string[2] == 0)) {
		*anchorPtr = CK_ANCHOR_NW;
		return TCL_OK;
	    }
	    goto error;
	case 's':
	    if (string[1] == 0) {
		*anchorPtr = CK_ANCHOR_S;
		return TCL_OK;
	    } else if ((string[1] == 'e') && (string[2] == 0)) {
		*anchorPtr = CK_ANCHOR_SE;
		return TCL_OK;
	    } else if ((string[1] == 'w') && (string[2] == 0)) {
		*anchorPtr = CK_ANCHOR_SW;
		return TCL_OK;
	    } else {
		goto error;
	    }
	case 'e':
	    if (string[1] == 0) {
		*anchorPtr = CK_ANCHOR_E;
		return TCL_OK;
	    }
	    goto error;
	case 'w':
	    if (string[1] == 0) {
		*anchorPtr = CK_ANCHOR_W;
		return TCL_OK;
	    }
	    goto error;
	case 'c':
	    if (strncmp(string, "center", strlen(string)) == 0) {
		*anchorPtr = CK_ANCHOR_CENTER;
		return TCL_OK;
	    }
	    goto error;
    }

    error:
    Tcl_AppendResult(interp, "bad anchor position \"", string,
	    "\": must be n, ne, e, se, s, sw, w, nw, or center",
	    (char *) NULL);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * Ck_NameOfAnchor --
 *
 *	Given a Ck_Anchor, return the string that corresponds
 *	to it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
Ck_NameOfAnchor(anchor)
    Ck_Anchor anchor;		/* Anchor for which identifying string
				 * is desired. */
{
    switch (anchor) {
	case CK_ANCHOR_N: return "n";
	case CK_ANCHOR_NE: return "ne";
	case CK_ANCHOR_E: return "e";
	case CK_ANCHOR_SE: return "se";
	case CK_ANCHOR_S: return "s";
	case CK_ANCHOR_SW: return "sw";
	case CK_ANCHOR_W: return "w";
	case CK_ANCHOR_NW: return "nw";
	case CK_ANCHOR_CENTER: return "center";
    }
    return "unknown anchor position";
}

/*
 *--------------------------------------------------------------
 *
 * Ck_GetJustify --
 *
 *	Given a string, return the corresponding Ck_Justify.
 *
 * Results:
 *	The return value is a standard Tcl return result.  If
 *	TCL_OK is returned, then everything went well and the
 *	justification is stored at *justifyPtr;  otherwise
 *	TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Ck_GetJustify(interp, string, justifyPtr)
    Tcl_Interp *interp;		/* Use this for error reporting. */
    char *string;		/* String describing a justification style. */
    Ck_Justify *justifyPtr;	/* Where to store Ck_Justify corresponding
				 * to string. */
{
    int c, length;

    c = string[0];
    length = strlen(string);

    if ((c == 'l') && (strncmp(string, "left", length) == 0)) {
	*justifyPtr = CK_JUSTIFY_LEFT;
	return TCL_OK;
    }
    if ((c == 'r') && (strncmp(string, "right", length) == 0)) {
	*justifyPtr = CK_JUSTIFY_RIGHT;
	return TCL_OK;
    }
    if ((c == 'c') && (strncmp(string, "center", length) == 0)) {
	*justifyPtr = CK_JUSTIFY_CENTER;
	return TCL_OK;
    }
    if ((c == 'f') && (strncmp(string, "fill", length) == 0)) {
	*justifyPtr = CK_JUSTIFY_FILL;
	return TCL_OK;
    }

    Tcl_AppendResult(interp, "bad justification \"", string,
	    "\": must be left, right, center, or fill",
	    (char *) NULL);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * Ck_NameOfJustify --
 *
 *	Given a Ck_Justify, return the string that corresponds
 *	to it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
Ck_NameOfJustify(justify)
    Ck_Justify justify;		/* Justification style for which
				 * identifying string is desired. */
{
    switch (justify) {
	case CK_JUSTIFY_LEFT: return "left";
	case CK_JUSTIFY_RIGHT: return "right";
	case CK_JUSTIFY_CENTER: return "center";
	case CK_JUSTIFY_FILL: return "fill";
    }
    return "unknown justification style";
}

/*
 *--------------------------------------------------------------
 *
 * Ck_GetCoord --
 *
 *	Given a string, return the coordinate that corresponds
 *	to it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Ck_GetCoord(interp, winPtr, string, intPtr)
    Tcl_Interp *interp;        /* Use this for error reporting. */
    CkWindow *winPtr;          /* Window (not used). */
    char *string;              /* String to convert. */
    int *intPtr;               /* Place to store converted result. */
{
    int value;

    if (Tcl_GetInt(interp, string, &value) != TCL_OK)
	return TCL_ERROR;
    if (value < 0) {
        Tcl_AppendResult(interp, "coordinate may not be negative",
	    (char *) NULL);
        return TCL_ERROR;
    }
    *intPtr = value;
    return TCL_OK;
}
