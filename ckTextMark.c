/* 
 * ckTextMark.c --
 *
 *	This file contains the procedure that implement marks for
 *	text widgets.
 *
 * Copyright (c) 1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 * Copyright (c) 1995 Christian Werner
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "ckPort.h"
#include "ck.h"
#include "ckText.h"

/*
 * Macro that determines the size of a mark segment:
 */

#define MSEG_SIZE ((unsigned) (Ck_Offset(CkTextSegment, body) \
	+ sizeof(CkTextMark)))

/*
 * Forward references for procedures defined in this file:
 */

static void		InsertUndisplayProc _ANSI_ARGS_((CkText *textPtr,
			    CkTextDispChunk *chunkPtr));
static int		MarkDeleteProc _ANSI_ARGS_((CkTextSegment *segPtr,
			    CkTextLine *linePtr, int treeGone));
static CkTextSegment *	MarkCleanupProc _ANSI_ARGS_((CkTextSegment *segPtr,
			    CkTextLine *linePtr));
static void		MarkCheckProc _ANSI_ARGS_((CkTextSegment *segPtr,
			    CkTextLine *linePtr));
static int		MarkLayoutProc _ANSI_ARGS_((CkText *textPtr,
			    CkTextIndex *indexPtr, CkTextSegment *segPtr,
			    int offset, int maxX, int maxChars,
			    int noCharsYet, Ck_Uid wrapMode,
			    CkTextDispChunk *chunkPtr));

/*
 * The following structures declare the "mark" segment types.
 * There are actually two types for marks, one with left gravity
 * and one with right gravity.  They are identical except for
 * their gravity property.
 */

Ck_SegType ckTextRightMarkType = {
    "mark",					/* name */
    0,						/* leftGravity */
    (Ck_SegSplitProc *) NULL,			/* splitProc */
    MarkDeleteProc,				/* deleteProc */
    MarkCleanupProc,				/* cleanupProc */
    (Ck_SegLineChangeProc *) NULL,		/* lineChangeProc */
    MarkLayoutProc,				/* layoutProc */
    MarkCheckProc				/* checkProc */
};

Ck_SegType ckTextLeftMarkType = {
    "mark",					/* name */
    1,						/* leftGravity */
    (Ck_SegSplitProc *) NULL,			/* splitProc */
    MarkDeleteProc,				/* deleteProc */
    MarkCleanupProc,				/* cleanupProc */
    (Ck_SegLineChangeProc *) NULL,		/* lineChangeProc */
    MarkLayoutProc,				/* layoutProc */
    MarkCheckProc				/* checkProc */
};

/*
 *--------------------------------------------------------------
 *
 * CkTextMarkCmd --
 *
 *	This procedure is invoked to process the "mark" options of
 *	the widget command for text widgets. See the user documentation
 *	for details on what it does.
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
CkTextMarkCmd(textPtr, interp, argc, argv)
    register CkText *textPtr;	/* Information about text widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings.  Someone else has already
				 * parsed this command enough to know that
				 * argv[1] is "mark". */
{
    int c, i;
    size_t length;
    Tcl_HashEntry *hPtr;
    CkTextSegment *markPtr;
    Tcl_HashSearch search;
    CkTextIndex index;
    Ck_SegType *newTypePtr;

    if (argc < 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " mark option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[2][0];
    length = strlen(argv[2]);
    if ((c == 'g') && (strncmp(argv[2], "gravity", length) == 0)) {
	if (argc > 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " mark gravity markName ?gravity?",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	hPtr = Tcl_FindHashEntry(&textPtr->markTable, argv[3]);
	if (hPtr == NULL) {
	    Tcl_AppendResult(interp, "there is no mark named \"",
		    argv[3], "\"", (char *) NULL);
	    return TCL_ERROR;
	}
	markPtr = (CkTextSegment *) Tcl_GetHashValue(hPtr);
	if (argc == 4) {
	    if (markPtr->typePtr == &ckTextRightMarkType) {
		interp->result = "right";
	    } else {
		interp->result = "left";
	    }
	    return TCL_OK;
	}
	length = strlen(argv[4]);
	c = argv[4][0];
	if ((c == 'l') && (strncmp(argv[4], "left", length) == 0)) {
	    newTypePtr = &ckTextLeftMarkType;
	} else if ((c == 'r') && (strncmp(argv[4], "right", length) == 0)) {
	    newTypePtr = &ckTextRightMarkType;
	} else {
	    Tcl_AppendResult(interp, "bad mark gravity \"",
		    argv[4], "\": must be left or right", (char *) NULL);
	    return TCL_ERROR;
	}
	CkTextMarkSegToIndex(textPtr, markPtr, &index);
	CkBTreeUnlinkSegment(textPtr->tree, markPtr,
		markPtr->body.mark.linePtr);
	markPtr->typePtr = newTypePtr;
	CkBTreeLinkSegment(markPtr, &index);
    } else if ((c == 'n') && (strncmp(argv[2], "names", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " mark names\"", (char *) NULL);
	    return TCL_ERROR;
	}
	for (hPtr = Tcl_FirstHashEntry(&textPtr->markTable, &search);
		hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    Tcl_AppendElement(interp,
		    Tcl_GetHashKey(&textPtr->markTable, hPtr));
	}
    } else if ((c == 's') && (strncmp(argv[2], "set", length) == 0)) {
	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " mark set markName index\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (CkTextGetIndex(interp, textPtr, argv[4], &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	CkTextSetMark(textPtr, argv[3], &index);
    } else if ((c == 'u') && (strncmp(argv[2], "unset", length) == 0)) {
	for (i = 3; i < argc; i++) {
	    hPtr = Tcl_FindHashEntry(&textPtr->markTable, argv[i]);
	    if (hPtr != NULL) {
		markPtr = (CkTextSegment *) Tcl_GetHashValue(hPtr);
		if ((markPtr == textPtr->insertMarkPtr)
			|| (markPtr == textPtr->currentMarkPtr)) {
		    continue;
		}
		CkBTreeUnlinkSegment(textPtr->tree, markPtr,
			markPtr->body.mark.linePtr);
		Tcl_DeleteHashEntry(hPtr);
		ckfree((char *) markPtr);
	    }
	}
    } else {
	Tcl_AppendResult(interp, "bad mark option \"", argv[2],
		"\":  must be gravity, names, set, or unset",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CkTextSetMark --
 *
 *	Set a mark to a particular position, creating a new mark if
 *	one doesn't already exist.
 *
 * Results:
 *	The return value is a pointer to the mark that was just set.
 *
 * Side effects:
 *	A new mark is created, or an existing mark is moved.
 *
 *----------------------------------------------------------------------
 */

CkTextSegment *
CkTextSetMark(textPtr, name, indexPtr)
    CkText *textPtr;		/* Text widget in which to create mark. */
    char *name;			/* Name of mark to set. */
    CkTextIndex *indexPtr;	/* Where to set mark. */
{
    Tcl_HashEntry *hPtr;
    CkTextSegment *markPtr;
    CkTextIndex insertIndex;
    int new;

    hPtr = Tcl_CreateHashEntry(&textPtr->markTable, name, &new);
    markPtr = (CkTextSegment *) Tcl_GetHashValue(hPtr);
    if (!new) {
	/*
	 * If this is the insertion point that's being moved, be sure
	 * to force a display update at the old position.  Also, don't
	 * let the insertion cursor be after the final newline of the
	 * file.
	 */

	if (markPtr == textPtr->insertMarkPtr) {
	    CkTextIndex index, index2;
	    CkTextMarkSegToIndex(textPtr, textPtr->insertMarkPtr, &index);
	    CkTextIndexForwChars(&index, 1, &index2);
	    CkTextChanged(textPtr, &index, &index2);
	    if (CkBTreeLineIndex(indexPtr->linePtr)
		    == CkBTreeNumLines(textPtr->tree))  {
		CkTextIndexBackChars(indexPtr, 1, &insertIndex);
		indexPtr = &insertIndex;
	    }
	}
	CkBTreeUnlinkSegment(textPtr->tree, markPtr,
		markPtr->body.mark.linePtr);
    } else {
	markPtr = (CkTextSegment *) ckalloc(MSEG_SIZE);
	markPtr->typePtr = &ckTextRightMarkType;
	markPtr->size = 0;
	markPtr->body.mark.textPtr = textPtr;
	markPtr->body.mark.linePtr = indexPtr->linePtr;
	markPtr->body.mark.hPtr = hPtr;
	Tcl_SetHashValue(hPtr, markPtr);
    }
    CkBTreeLinkSegment(markPtr, indexPtr);

    /*
     * If the mark is the insertion cursor, then update the screen at the
     * mark's new location.
     */

    if (markPtr == textPtr->insertMarkPtr) {
	CkTextIndex index2;

	CkTextIndexForwChars(indexPtr, 1, &index2);
	CkTextChanged(textPtr, indexPtr, &index2);
    }
    return markPtr;
}

/*
 *--------------------------------------------------------------
 *
 * CkTextMarkSegToIndex --
 *
 *	Given a segment that is a mark, create an index that
 *	refers to the next text character (or other text segment
 *	with non-zero size) after the mark.
 *
 * Results:
 *	*IndexPtr is filled in with index information.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
CkTextMarkSegToIndex(textPtr, markPtr, indexPtr)
    CkText *textPtr;		/* Text widget containing mark. */
    CkTextSegment *markPtr;	/* Mark segment. */
    CkTextIndex *indexPtr;	/* Index information gets stored here.  */
{
    CkTextSegment *segPtr;

    indexPtr->tree = textPtr->tree;
    indexPtr->linePtr = markPtr->body.mark.linePtr;
    indexPtr->charIndex = 0;
    for (segPtr = indexPtr->linePtr->segPtr; segPtr != markPtr;
	    segPtr = segPtr->nextPtr) {
	indexPtr->charIndex += segPtr->size;
    }
}

/*
 *--------------------------------------------------------------
 *
 * CkTextMarkNameToIndex --
 *
 *	Given the name of a mark, return an index corresponding
 *	to the mark name.
 *
 * Results:
 *	The return value is TCL_OK if "name" exists as a mark in
 *	the text widget.  In this case *indexPtr is filled in with
 *	the next segment whose after the mark whose size is
 *	non-zero.  TCL_ERROR is returned if the mark doesn't exist
 *	in the text widget.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
CkTextMarkNameToIndex(textPtr, name, indexPtr)
    CkText *textPtr;		/* Text widget containing mark. */
    char *name;			/* Name of mark. */
    CkTextIndex *indexPtr;	/* Index information gets stored here. */
{
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&textPtr->markTable, name);
    if (hPtr == NULL) {
	return TCL_ERROR;
    }
    CkTextMarkSegToIndex(textPtr, (CkTextSegment *) Tcl_GetHashValue(hPtr),
	    indexPtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * MarkDeleteProc --
 *
 *	This procedure is invoked by the text B-tree code whenever
 *	a mark lies in a range of characters being deleted.
 *
 * Results:
 *	Returns 1 to indicate that deletion has been rejected.
 *
 * Side effects:
 *	None (even if the whole tree is being deleted we don't
 *	free up the mark;  it will be done elsewhere).
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static int
MarkDeleteProc(segPtr, linePtr, treeGone)
    CkTextSegment *segPtr;		/* Segment being deleted. */
    CkTextLine *linePtr;		/* Line containing segment. */
    int treeGone;			/* Non-zero means the entire tree is
					 * being deleted, so everything must
					 * get cleaned up. */
{
    return 1;
}

/*
 *--------------------------------------------------------------
 *
 * MarkCleanupProc --
 *
 *	This procedure is invoked by the B-tree code whenever a
 *	mark segment is moved from one line to another.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The linePtr field of the segment gets updated.
 *
 *--------------------------------------------------------------
 */

static CkTextSegment *
MarkCleanupProc(markPtr, linePtr)
    CkTextSegment *markPtr;		/* Mark segment that's being moved. */
    CkTextLine *linePtr;		/* Line that now contains segment. */
{
    markPtr->body.mark.linePtr = linePtr;
    return markPtr;
}

/*
 *--------------------------------------------------------------
 *
 * MarkLayoutProc --
 *
 *	This procedure is the "layoutProc" for mark segments.
 *
 * Results:
 *	If the mark isn't the insertion cursor then the return
 *	value is -1 to indicate that this segment shouldn't be
 *	displayed.  If the mark is the insertion character then
 *	1 is returned and the chunkPtr structure is filled in.
 *
 * Side effects:
 *	None, except for filling in chunkPtr.
 *
 *--------------------------------------------------------------
 */

	/*ARGSUSED*/
static int
MarkLayoutProc(textPtr, indexPtr, segPtr, offset, maxX, maxChars,
	noCharsYet, wrapMode, chunkPtr)
    CkText *textPtr;		/* Text widget being layed out. */
    CkTextIndex *indexPtr;	/* Identifies first character in chunk. */
    CkTextSegment *segPtr;	/* Segment corresponding to indexPtr. */
    int offset;			/* Offset within segPtr corresponding to
				 * indexPtr (always 0). */
    int maxX;			/* Chunk must not occupy pixels at this
				 * position or higher. */
    int maxChars;		/* Chunk must not include more than this
				 * many characters. */
    int noCharsYet;		/* Non-zero means no characters have been
				 * assigned to this line yet. */
    Ck_Uid wrapMode;		/* Not used. */
    register CkTextDispChunk *chunkPtr;
				/* Structure to fill in with information
				 * about this chunk.  The x field has already
				 * been set by the caller. */
{
    if (segPtr != textPtr->insertMarkPtr) {
	return -1;
    }

    chunkPtr->displayProc = CkTextInsertDisplayProc;
    chunkPtr->undisplayProc = InsertUndisplayProc;
    chunkPtr->measureProc = (Ck_ChunkMeasureProc *) NULL;
    chunkPtr->bboxProc = (Ck_ChunkBboxProc *) NULL;
    chunkPtr->numChars = 0;
    chunkPtr->minHeight = 0;
    chunkPtr->width = 0;

    /*
     * Note: can't break a line after the insertion cursor:  this
     * prevents the insertion cursor from being stranded at the end
     * of a line.
     */

    chunkPtr->breakIndex = -1;
    chunkPtr->clientData = (ClientData) textPtr;
    return 1;
}

/*
 *--------------------------------------------------------------
 *
 * CkTextInsertDisplayProc --
 *
 *	This procedure is called to display the insertion
 *	cursor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Graphics are drawn.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
void
CkTextInsertDisplayProc(chunkPtr, x, y, height, baseline, window, screenY)
    CkTextDispChunk *chunkPtr;		/* Chunk that is to be drawn. */
    int x;				/* X-position in dst at which to
					 * draw this chunk (may differ from
					 * the x-position in the chunk because
					 * of scrolling). */
    int y;				/* Y-position at which to draw this
					 * chunk in dst (x-position is in
					 * the chunk itself). */
    int height;				/* Total height of line. */
    int baseline;			/* Offset of baseline from y. */
    WINDOW *window;                     /* Curses window. */
    int screenY;			/* Y-coordinate in text window that
					 * corresponds to y. */
{
    CkText *textPtr = (CkText *) chunkPtr->clientData;

    textPtr->insertY = screenY;
    textPtr->insertX = x;
}

/*
 *--------------------------------------------------------------
 *
 * InsertUndisplayProc --
 *
 *	This procedure is called when the insertion cursor is no
 *	longer at a visible point on the display.  It does nothing
 *	right now.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static void
InsertUndisplayProc(textPtr, chunkPtr)
    CkText *textPtr;			/* Overall information about text
					 * widget. */
    CkTextDispChunk *chunkPtr;		/* Chunk that is about to be freed. */
{
    return;
}

/*
 *--------------------------------------------------------------
 *
 * MarkCheckProc --
 *
 *	This procedure is invoked by the B-tree code to perform
 *	consistency checks on mark segments.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The procedure panics if it detects anything wrong with
 *	the mark.
 *
 *--------------------------------------------------------------
 */

static void
MarkCheckProc(markPtr, linePtr)
    CkTextSegment *markPtr;		/* Segment to check. */
    CkTextLine *linePtr;		/* Line containing segment. */
{
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;

    if (markPtr->body.mark.linePtr != linePtr) {
	panic("MarkCheckProc: markPtr->body.mark.linePtr bogus");
    }

    /*
     * Make sure that the mark is still present in the text's mark
     * hash table.
     */

    for (hPtr = Tcl_FirstHashEntry(&markPtr->body.mark.textPtr->markTable,
	    &search); hPtr != markPtr->body.mark.hPtr;
	    hPtr = Tcl_NextHashEntry(&search)) {
	if (hPtr == NULL) {
	    panic("MarkCheckProc couldn't find hash table entry for mark");
	}
    }
}
