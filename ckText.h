/*
 * ckText.h --
 *
 *	Declarations shared among the files that implement text
 *	widgets.
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#ifndef _CKTEXT_H
#define _CKTEXT_H

#ifndef _CK
#include "ck.h"
#endif

/*
 * Opaque types for structures whose guts are only needed by a single
 * file:
 */

typedef struct CkTextBTree *CkTextBTree;

/*
 * The data structure below defines a single line of text (from newline
 * to newline, not necessarily what appears on one line of the screen).
 */

typedef struct CkTextLine {
    struct Node *parentPtr;		/* Pointer to parent node containing
					 * line. */
    struct CkTextLine *nextPtr;		/* Next in linked list of lines with
					 * same parent node in B-tree.  NULL
					 * means end of list. */
    struct CkTextSegment *segPtr;	/* First in ordered list of segments
					 * that make up the line. */
} CkTextLine;

/*
 * -----------------------------------------------------------------------
 * Segments: each line is divided into one or more segments, where each
 * segment is one of several things, such as a group of characters, a
 * tag toggle, a mark, or an embedded widget.  Each segment starts with
 * a standard header followed by a body that varies from type to type.
 * -----------------------------------------------------------------------
 */

/*
 * The data structure below defines the body of a segment that represents
 * a tag toggle.  There is one of these structures at both the beginning
 * and end of each tagged range.
 */

typedef struct CkTextToggle {
    struct CkTextTag *tagPtr;		/* Tag that starts or ends here. */
    int inNodeCounts;			/* 1 means this toggle has been
					 * accounted for in node toggle
					 * counts; 0 means it hasn't, yet. */
} CkTextToggle;

/*
 * The data structure below defines line segments that represent
 * marks.  There is one of these for each mark in the text.
 */

typedef struct CkTextMark {
    struct CkText *textPtr;		/* Overall information about text
					 * widget. */
    CkTextLine *linePtr;		/* Line structure that contains the
					 * segment. */
    Tcl_HashEntry *hPtr;		/* Pointer to hash table entry for mark
					 * (in textPtr->markTable). */
} CkTextMark;

/*
 * A structure of the following type holds information for each window
 * embedded in a text widget.  This information is only used by the
 * file ckTextWind.c
 */

typedef struct CkTextEmbWindow {
    struct CkText *textPtr;		/* Information about the overall text
					 * widget. */
    CkTextLine *linePtr;		/* Line structure that contains this
					 * window. */
    CkWindow winPtr;			/* Window for this segment.  NULL
					 * means that the window hasn't
					 * been created yet. */
    char *create;			/* Script to create window on-demand.
					 * NULL means no such script.
					 * Malloc-ed. */
    int align;				/* How to align window in vertical
					 * space.  See definitions in
					 * ckTextWind.c. */
    int padX, padY;			/* Padding to leave around each side
					 * of window, in pixels. */
    int stretch;			/* Should window stretch to fill
					 * vertical space of line (except for
					 * pady)?  0 or 1. */
    int chunkCount;			/* Number of display chunks that
					 * refer to this window. */
    int displayed;			/* Non-zero means that the window
					 * has been displayed on the screen
					 * recently. */
} CkTextEmbWindow;

/*
 * The data structure below defines line segments.
 */

typedef struct CkTextSegment {
    struct Ck_SegType *typePtr;		/* Pointer to record describing
					 * segment's type. */
    struct CkTextSegment *nextPtr;	/* Next in list of segments for this
					 * line, or NULL for end of list. */
    int size;				/* Size of this segment (# of bytes
					 * of index space it occupies). */
    union {
	char chars[4];			/* Characters that make up character
					 * info.  Actual length varies to
					 * hold as many characters as needed.*/
	CkTextToggle toggle;		/* Information about tag toggle. */
	CkTextMark mark;		/* Information about mark. */
	CkTextEmbWindow ew;		/* Information about embedded
					 * window. */
    } body;
} CkTextSegment;

/*
 * Data structures of the type defined below are used during the
 * execution of Tcl commands to keep track of various interesting
 * places in a text.  An index is only valid up until the next
 * modification to the character structure of the b-tree so they
 * can't be retained across Tcl commands.  However, mods to marks
 * or tags don't invalidate indices.
 */

typedef struct CkTextIndex {
    CkTextBTree tree;			/* Tree containing desired position. */
    CkTextLine *linePtr;		/* Pointer to line containing position
					 * of interest. */
    int charIndex;			/* Index within line of desired
					 * character (0 means first one). */
} CkTextIndex;

/*
 * Types for procedure pointers stored in CkTextDispChunk strutures:
 */

typedef struct CkTextDispChunk CkTextDispChunk;

typedef void 		Ck_ChunkDisplayProc _ANSI_ARGS_((
			    CkTextDispChunk *chunkPtr, int x, int y,
			    int height, int baseline, WINDOW *window,
                            int screenY));
typedef void		Ck_ChunkUndisplayProc _ANSI_ARGS_((
			    struct CkText *textPtr,
			    CkTextDispChunk *chunkPtr));
typedef int		Ck_ChunkMeasureProc _ANSI_ARGS_((
			    CkTextDispChunk *chunkPtr, int x));
typedef void		Ck_ChunkBboxProc _ANSI_ARGS_((
			    CkTextDispChunk *chunkPtr, int index, int y,
			    int lineHeight, int baseline, int *xPtr,
			    int *yPtr, int *widthPtr, int *heightPtr));

/*
 * The structure below represents a chunk of stuff that is displayed
 * together on the screen.  This structure is allocated and freed by
 * generic display code but most of its fields are filled in by
 * segment-type-specific code.
 */

struct CkTextDispChunk {
    /*
     * The fields below are set by the type-independent code before
     * calling the segment-type-specific layoutProc.  They should not
     * be modified by segment-type-specific code.
     */

    int x;				/* X position of chunk, in pixels.
					 * This position is measured from the
					 * left edge of the logical line,
					 * not from the left edge of the
					 * window (i.e. it doesn't change
					 * under horizontal scrolling). */
    struct CkTextDispChunk *nextPtr;	/* Next chunk in the display line
					 * or NULL for the end of the list. */
    struct Style *stylePtr;		/* Display information, known only
					 * to ckTextDisp.c. */

    /*
     * The fields below are set by the layoutProc that creates the
     * chunk.
     */

    Ck_ChunkDisplayProc *displayProc;	/* Procedure to invoke to draw this
					 * chunk on the display or an
					 * off-screen pixmap. */
    Ck_ChunkUndisplayProc *undisplayProc;
					/* Procedure to invoke when segment
					 * ceases to be displayed on screen
					 * anymore. */
    Ck_ChunkMeasureProc *measureProc;	/* Procedure to find character under
					 * a given x-location. */
    Ck_ChunkBboxProc *bboxProc;		/* Procedure to find bounding box
					 * of character in chunk. */
    int numChars;			/* Number of characters that will be
					 * displayed in the chunk. */
    int minHeight;                      /* Minimum total line height needed
                                         * by this chunk. */
    int width;                          /* Width of this chunk, in pixels.
                                         * Initially set by chunk-specific
                                         * code, but may be increased to
                                         * include tab or extra space at end
                                         * of line. */
    int breakIndex;			/* Index within chunk of last
					 * acceptable position for a line
					 * (break just before this character).
					 * <= 0 means don't break during or
					 * immediately after this chunk. */
    ClientData clientData;		/* Additional information for use
					 * of displayProc and undisplayProc. */
};

/*
 * One data structure of the following type is used for each tag in a
 * text widget.  These structures are kept in textPtr->tagTable and
 * referred to in other structures.
 */

typedef struct CkTextTag {
    char *name;			/* Name of this tag.  This field is actually
				 * a pointer to the key from the entry in
				 * textPtr->tagTable, so it needn't be freed
				 * explicitly. */
    int priority;		/* Priority of this tag within widget.  0
				 * means lowest priority.  Exactly one tag
				 * has each integer value between 0 and
				 * numTags-1. */

    /*
     * Information for displaying text with this tag.  The information
     * belows acts as an override on information specified by lower-priority
     * tags.  If no value is specified, then the next-lower-priority tag
     * on the text determins the value.  The text widget itself provides
     * defaults if no tag specifies an override.
     */

    int bg, fg, attr;           /* Foreground/background/video attributes
				 * for text. -1 means no value specified. */
    char *justifyString;	/* -justify option string (malloc-ed).
				 * NULL means option not specified. */
    Ck_Justify justify;		/* How to justify text: CK_JUSTIFY_LEFT,
				 * CK_JUSTIFY_RIGHT, or CK_JUSTIFY_CENTER.
				 * Only valid if justifyString is non-NULL. */
    char *lMargin1String;	/* -lmargin1 option string (malloc-ed).
				 * NULL means option not specified. */
    int lMargin1;		/* Left margin for first display line of
				 * each text line, in pixels.  Only valid
				 * if lMargin1String is non-NULL. */
    char *lMargin2String;	/* -lmargin2 option string (malloc-ed).
				 * NULL means option not specified. */
    int lMargin2;		/* Left margin for second and later display
				 * lines of each text line, in pixels.  Only
				 * valid if lMargin2String is non-NULL. */
    char *rMarginString;	/* -rmargin option string (malloc-ed).
				 * NULL means option not specified. */
    int rMargin;		/* Right margin for text, in pixels.  Only
				 * valid if rMarginString is non-NULL. */
    char *tabString;		/* -tabs option string (malloc-ed).
				 * NULL means option not specified. */
    struct CkTextTabArray *tabArrayPtr;
				/* Info about tabs for tag (malloc-ed)
				 * or NULL.  Corresponds to tabString. */
    Ck_Uid wrapMode;		/* How to handle wrap-around for this tag.
				 * Must be ckTextCharUid, ckTextNoneUid,
				 * ckTextWordUid, or NULL to use wrapMode
				 * for whole widget. */
    int affectsDisplay;		/* Non-zero means that this tag affects the
				 * way information is displayed on the screen
				 * (so need to redisplay if tag changes). */
} CkTextTag;

#define TK_TAG_AFFECTS_DISPLAY	0x1
#define TK_TAG_UNDERLINE	0x2
#define TK_TAG_JUSTIFY		0x4
#define TK_TAG_OFFSET		0x10

/*
 * The data structure below is used for searching a B-tree for transitions
 * on a single tag (or for all tag transitions).  No code outside of
 * ckTextBTree.c should ever modify any of the fields in these structures,
 * but it's OK to use them for read-only information.
 */

typedef struct CkTextSearch {
    CkTextIndex curIndex;		/* Position of last tag transition
					 * returned by CkBTreeNextTag, or
					 * index of start of segment
					 * containing starting position for
					 * search if CkBTreeNextTag hasn't
					 * been called yet, or same as
					 * stopIndex if search is over. */
    CkTextSegment *segPtr;		/* Actual tag segment returned by last
					 * call to CkBTreeNextTag, or NULL if
					 * CkBTreeNextTag hasn't returned
					 * anything yet. */
    CkTextSegment *nextPtr;		/* Where to resume search in next
					 * call to CkBTreeNextTag. */
    CkTextSegment *lastPtr;		/* Stop search before just before
					 * considering this segment. */
    CkTextTag *tagPtr;			/* Tag to search for (or tag found, if
					 * allTags is non-zero). */
    int linesLeft;			/* Lines left to search (including
					 * curIndex and stopIndex).  When
					 * this becomes <= 0 the search is
					 * over. */
    int allTags;			/* Non-zero means ignore tag check:
					 * search for transitions on all
					 * tags. */
} CkTextSearch;

/*
 * The following data structure describes a single tab stop.
 */

typedef enum {LEFT, RIGHT, CENTER, NUMERIC} CkTextTabAlign;

typedef struct CkTextTab {
    int location;			/* Offset in pixels of this tab stop
					 * from the left margin (lmargin2) of
					 * the text. */
    CkTextTabAlign alignment;		/* Where the tab stop appears relative
					 * to the text. */
} CkTextTab;

typedef struct CkTextTabArray {
    int numTabs;			/* Number of tab stops. */
    CkTextTab tabs[1];			/* Array of tabs.  The actual size
					 * will be numTabs.  THIS FIELD MUST
					 * BE THE LAST IN THE STRUCTURE. */
} CkTextTabArray;

/*
 * A data structure of the following type is kept for each text widget that
 * currently exists for this process:
 */

typedef struct CkText {
    CkWindow *winPtr;		/* Window that embodies the text.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Tcl_Interp *interp;		/* Interpreter associated with widget.  Used
				 * to delete widget command.  */
    Tcl_Command widgetCmd;      /* Token for text's widget command. */
    CkTextBTree tree;		/* B-tree representation of text and tags for
				 * widget. */
    Tcl_HashTable tagTable;	/* Hash table that maps from tag names to
				 * pointers to CkTextTag structures. */
    int numTags;		/* Number of tags currently defined for
				 * widget;  needed to keep track of
				 * priorities. */
    Tcl_HashTable markTable;	/* Hash table that maps from mark names to
				 * pointers to mark segments. */
    Tcl_HashTable windowTable;	/* Hash table that maps from window names
				 * to pointers to window segments.  If a
				 * window segment doesn't yet have an
				 * associated window, there is no entry for
				 * it here. */
    Ck_Uid state;		/* Normal or disabled.  Text is read-only
				 * when disabled. */

    /*
     * Default information for displaying (may be overridden by tags
     * applied to ranges of characters).
     */

    int bg, fg, attr;
    char *tabOptionString;	/* Value of -tabs option string (malloc'ed). */
    CkTextTabArray *tabArrayPtr;
				/* Information about tab stops (malloc'ed).
				 * NULL means perform default tabbing
				 * behavior. */

    /*
     * Additional information used for displaying:
     */

    Ck_Uid wrapMode;		/* How to handle wrap-around.  Must be
				 * ckTextCharUid, ckTextNoneUid, or
				 * ckTextWordUid. */
    int width, height;		/* Desired dimensions for window, measured
				 * in characters. */
    int prevWidth, prevHeight;	/* Last known dimensions of window;  used to
				 * detect changes in size. */
    CkTextIndex topIndex;	/* Identifies first character in top display
				 * line of window. */
    struct DInfo *dInfoPtr;	/* Information maintained by ckTextDisp.c. */

    /*
     * Information related to selection.
     */

    int selFg, selBg, selAttr;
    CkTextTag *selTagPtr;	/* Pointer to "sel" tag.  Used to tell when
				 * a new selection has been made. */
    CkTextIndex selIndex;	/* Used during multi-pass selection retrievals.
				 * This index identifies the next character
				 * to be returned from the selection. */
    int abortSelections;	/* Set to 1 whenever the text is modified
				 * in a way that interferes with selection
				 * retrieval:  used to abort incremental
				 * selection retrievals. */
    int selOffset;		/* Offset in selection corresponding to
				 * selLine and selCh.  -1 means neither
				 * this information nor selIndex is of any
				 * use. */
    int insertX, insertY;       /* Window coordinates of HW cursor. */

    /*
     * Information related to insertion cursor:
     */

    CkTextSegment *insertMarkPtr;
				/* Points to segment for "insert" mark. */

    /*
     * Information used for event bindings associated with tags:
     */

    Ck_BindingTable bindingTable;
				/* Table of all bindings currently defined
				 * for this widget.  NULL means that no
				 * bindings exist, so the table hasn't been
				 * created.  Each "object" used for this
				 * table is the address of a tag. */
    CkTextSegment *currentMarkPtr;
				/* Pointer to segment for "current" mark,
				 * or NULL if none. */
    CkEvent pickEvent;		/* The event from which the current character
				 * was chosen.  Must be saved so that we
				 * can repick after modifications to the
				 * text. */
    int numCurTags;		/* Number of tags associated with character
				 * at current mark. */
    CkTextTag **curTagArrayPtr;	/* Pointer to array of tags for current
				 * mark, or NULL if none. */

    /*
     * Miscellaneous additional information:
     */

    char *takeFocus;		/* Value of -takeFocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
    char *xScrollCmd;		/* Prefix of command to issue to update
				 * horizontal scrollbar when view changes. */
    char *yScrollCmd;		/* Prefix of command to issue to update
				 * vertical scrollbar when view changes. */
    int flags;			/* Miscellaneous flags;  see below for
				 * definitions. */
} CkText;

/*
 * Flag values for CkText records:
 *
 * GOT_SELECTION:		Non-zero means we've already claimed the
 *				selection.
 * INSERT_ON:			Non-zero means insertion cursor should be
 *				displayed on screen.
 * GOT_FOCUS:			Non-zero means this window has the input
 *				focus.
 * UPDATE_SCROLLBARS:		Non-zero means scrollbar(s) should be updated
 *				during next redisplay operation.
 */

#define GOT_SELECTION		1
#define INSERT_ON		2
#define GOT_FOCUS		4
#define UPDATE_SCROLLBARS	0x10
#define NEED_REPICK		0x20

/*
 * Records of the following type define segment types in terms of
 * a collection of procedures that may be called to manipulate
 * segments of that type.
 */

typedef CkTextSegment *	Ck_SegSplitProc _ANSI_ARGS_((
			    struct CkTextSegment *segPtr, int index));
typedef int		Ck_SegDeleteProc _ANSI_ARGS_((
			    struct CkTextSegment *segPtr,
			    CkTextLine *linePtr, int treeGone));
typedef CkTextSegment *	Ck_SegCleanupProc _ANSI_ARGS_((
			    struct CkTextSegment *segPtr, CkTextLine *linePtr));
typedef void		Ck_SegLineChangeProc _ANSI_ARGS_((
			    struct CkTextSegment *segPtr, CkTextLine *linePtr));
typedef int		Ck_SegLayoutProc _ANSI_ARGS_((struct CkText *textPtr,
			    struct CkTextIndex *indexPtr, CkTextSegment *segPtr,
			    int offset, int maxX, int maxChars,
			    int noCharsYet, Ck_Uid wrapMode,
			    struct CkTextDispChunk *chunkPtr));
typedef void		Ck_SegCheckProc _ANSI_ARGS_((CkTextSegment *segPtr,
			    CkTextLine *linePtr));

typedef struct Ck_SegType {
    char *name;				/* Name of this kind of segment. */
    int leftGravity;			/* If a segment has zero size (e.g. a
					 * mark or tag toggle), does it
					 * attach to character to its left
					 * or right?  1 means left, 0 means
					 * right. */
    Ck_SegSplitProc *splitProc;		/* Procedure to split large segment
					 * into two smaller ones. */
    Ck_SegDeleteProc *deleteProc;	/* Procedure to call to delete
					 * segment. */
    Ck_SegCleanupProc *cleanupProc;	/* After any change to a line, this
					 * procedure is invoked for all
					 * segments left in the line to
					 * perform any cleanup they wish
					 * (e.g. joining neighboring
					 * segments). */
    Ck_SegLineChangeProc *lineChangeProc;
					/* Invoked when a segment is about
					 * to be moved from its current line
					 * to an earlier line because of
					 * a deletion.  The linePtr is that
					 * for the segment's old line.
					 * CleanupProc will be invoked after
					 * the deletion is finished. */
    Ck_SegLayoutProc *layoutProc;	/* Returns size information when
					 * figuring out what to display in
					 * window. */
    Ck_SegCheckProc *checkProc;		/* Called during consistency checks
					 * to check internal consistency of
					 * segment. */
} Ck_SegType;

/*
 * The constant below is used to specify a line when what is really
 * wanted is the entire text.  For now, just use a very big number.
 */

#define TK_END_OF_TEXT 1000000

/*
 * The following definition specifies the maximum number of characters
 * needed in a string to hold a position specifier.
 */

#define TK_POS_CHARS 30

/*
 * Declarations for variables shared among the text-related files:
 */

extern int		ckBTreeDebug;
extern int		ckTextDebug;
extern Ck_SegType	ckTextCharType;
extern Ck_Uid		ckTextCharUid;
extern Ck_Uid		ckTextDisabledUid;
extern Ck_SegType	ckTextLeftMarkType;
extern Ck_Uid		ckTextNoneUid;
extern Ck_Uid 		ckTextNormalUid;
extern Ck_SegType	ckTextRightMarkType;
extern Ck_SegType	ckTextToggleOnType;
extern Ck_SegType	ckTextToggleOffType;
extern Ck_Uid		ckTextWordUid;

/*
 * Declarations for procedures that are used by the text-related files
 * but shouldn't be used anywhere else in Ck (or by Ck clients):
 */

extern int		CkBTreeCharTagged _ANSI_ARGS_((CkTextIndex *indexPtr,
			    CkTextTag *tagPtr));
extern void		CkBTreeCheck _ANSI_ARGS_((CkTextBTree tree));
extern int		CkBTreeCharsInLine _ANSI_ARGS_((CkTextLine *linePtr));
extern CkTextBTree	CkBTreeCreate _ANSI_ARGS_((void));
extern void		CkBTreeDestroy _ANSI_ARGS_((CkTextBTree tree));
extern void		CkBTreeDeleteChars _ANSI_ARGS_((CkTextIndex *index1Ptr,
			    CkTextIndex *index2Ptr));
extern CkTextLine *	CkBTreeFindLine _ANSI_ARGS_((CkTextBTree tree,
			    int line));
extern CkTextTag **	CkBTreeGetTags _ANSI_ARGS_((CkTextIndex *indexPtr,
			    int *numTagsPtr));
extern void		CkBTreeInsertChars _ANSI_ARGS_((CkTextIndex *indexPtr,
			    char *string));
extern int		CkBTreeLineIndex _ANSI_ARGS_((CkTextLine *linePtr));
extern void		CkBTreeLinkSegment _ANSI_ARGS_((CkTextSegment *segPtr,
			    CkTextIndex *indexPtr));
extern CkTextLine *	CkBTreeNextLine _ANSI_ARGS_((CkTextLine *linePtr));
extern int		CkBTreeNextTag _ANSI_ARGS_((CkTextSearch *searchPtr));
extern int		CkBTreeNumLines _ANSI_ARGS_((CkTextBTree tree));
extern void		CkBTreeStartSearch _ANSI_ARGS_((CkTextIndex *index1Ptr,
			    CkTextIndex *index2Ptr, CkTextTag *tagPtr,
			    CkTextSearch *searchPtr));
extern void		CkBTreeTag _ANSI_ARGS_((CkTextIndex *index1Ptr,
			    CkTextIndex *index2Ptr, CkTextTag *tagPtr,
			    int add));
extern void		CkBTreeUnlinkSegment _ANSI_ARGS_((CkTextBTree tree,
			    CkTextSegment *segPtr, CkTextLine *linePtr));
extern void		CkTextBindProc _ANSI_ARGS_((ClientData clientData,
			    CkEvent *eventPtr));
extern void		CkTextChanged _ANSI_ARGS_((CkText *textPtr,
			    CkTextIndex *index1Ptr, CkTextIndex *index2Ptr));
extern int		CkTextCharBbox _ANSI_ARGS_((CkText *textPtr,
			    CkTextIndex *indexPtr, int *xPtr, int *yPtr,
			    int *widthPtr, int *heightPtr));
extern int		CkTextCharLayoutProc _ANSI_ARGS_((CkText *textPtr,
			    CkTextIndex *indexPtr, CkTextSegment *segPtr,
			    int offset, int maxX, int maxChars, int noBreakYet,
			    Ck_Uid wrapMode, CkTextDispChunk *chunkPtr));
extern void		CkTextCreateDInfo _ANSI_ARGS_((CkText *textPtr));
extern int		CkTextDLineInfo _ANSI_ARGS_((CkText *textPtr,
			    CkTextIndex *indexPtr, int *xPtr, int *yPtr,
			    int *widthPtr, int *heightPtr, int *basePtr));
extern CkTextTag *	CkTextCreateTag _ANSI_ARGS_((CkText *textPtr,
			    char *tagName));
extern void		CkTextFreeDInfo _ANSI_ARGS_((CkText *textPtr));
extern void		CkTextFreeTag _ANSI_ARGS_((CkText *textPtr,
			    CkTextTag *tagPtr));
extern int		CkTextGetIndex _ANSI_ARGS_((Tcl_Interp *interp,
			    CkText *textPtr, char *string,
			    CkTextIndex *indexPtr));
extern CkTextTabArray *	CkTextGetTabs _ANSI_ARGS_((Tcl_Interp *interp,
			    CkWindow *winPtr, char *string));
#if CK_USE_UTF
extern void		CkTextIndexBackBytes _ANSI_ARGS_((CkTextIndex *srcPtr,
			    int count, CkTextIndex *dstPtr));
#endif
extern void		CkTextIndexBackChars _ANSI_ARGS_((CkTextIndex *srcPtr,
			    int count, CkTextIndex *dstPtr));
extern int		CkTextIndexCmp _ANSI_ARGS_((CkTextIndex *index1Ptr,
			    CkTextIndex *index2Ptr));
#if CK_USE_UTF
extern void		CkTextIndexForwBytes _ANSI_ARGS_((CkTextIndex *srcPtr,
			    int count, CkTextIndex *dstPtr));
#endif
extern void		CkTextIndexForwChars _ANSI_ARGS_((CkTextIndex *srcPtr,
			    int count, CkTextIndex *dstPtr));
extern CkTextSegment *	CkTextIndexToSeg _ANSI_ARGS_((CkTextIndex *indexPtr,
			    int *offsetPtr));
extern void		CkTextInsertDisplayProc _ANSI_ARGS_((
			    CkTextDispChunk *chunkPtr, int x, int y, int height,
			    int baseline, WINDOW *window, int screenY));
extern void		CkTextLostSelection _ANSI_ARGS_((
			    ClientData clientData));
#if CK_USE_UTF
extern CkTextIndex *	CkTextMakeByteIndex _ANSI_ARGS_((CkTextBTree tree,
			    int lineIndex, int byteIndex,
			    CkTextIndex *indexPtr));
#endif
extern CkTextIndex *	CkTextMakeIndex _ANSI_ARGS_((CkTextBTree tree,
			    int lineIndex, int charIndex,
			    CkTextIndex *indexPtr));
extern int		CkTextMarkCmd _ANSI_ARGS_((CkText *textPtr,
			    Tcl_Interp *interp, int argc, char **argv));
extern int		CkTextMarkNameToIndex _ANSI_ARGS_((CkText *textPtr,
			    char *name, CkTextIndex *indexPtr));
extern void		CkTextMarkSegToIndex _ANSI_ARGS_((CkText *textPtr,
			    CkTextSegment *markPtr, CkTextIndex *indexPtr));
extern void		CkTextEventuallyRepick _ANSI_ARGS_((CkText *textPtr));
extern void		CkTextPickCurrent _ANSI_ARGS_((CkText *textPtr,
			    CkEvent *eventPtr));
extern void		CkTextPixelIndex _ANSI_ARGS_((CkText *textPtr,
			    int x, int y, CkTextIndex *indexPtr));
extern void		CkTextPrintIndex _ANSI_ARGS_((CkTextIndex *indexPtr,
			    char *string));
extern void		CkTextRedrawRegion _ANSI_ARGS_((CkText *textPtr,
			    int x, int y, int width, int height));
extern void		CkTextRedrawTag _ANSI_ARGS_((CkText *textPtr,
			    CkTextIndex *index1Ptr, CkTextIndex *index2Ptr,
			    CkTextTag *tagPtr, int withTag));
extern void		CkTextRelayoutWindow _ANSI_ARGS_((CkText *textPtr));
extern int		CkTextScanCmd _ANSI_ARGS_((CkText *textPtr,
			    Tcl_Interp *interp, int argc, char **argv));
extern int		CkTextSeeCmd _ANSI_ARGS_((CkText *textPtr,
			    Tcl_Interp *interp, int argc, char **argv));
extern int		CkTextSegToOffset _ANSI_ARGS_((CkTextSegment *segPtr,
			    CkTextLine *linePtr));
extern CkTextSegment *	CkTextSetMark _ANSI_ARGS_((CkText *textPtr, char *name,
			    CkTextIndex *indexPtr));
extern void		CkTextSetYView _ANSI_ARGS_((CkText *textPtr,
			    CkTextIndex *indexPtr, int pickPlace));
extern int		CkTextTagCmd _ANSI_ARGS_((CkText *textPtr,
			    Tcl_Interp *interp, int argc, char **argv));
extern int		CkTextWindowCmd _ANSI_ARGS_((CkText *textPtr,
			    Tcl_Interp *interp, int argc, char **argv));
extern int		CkTextWindowIndex _ANSI_ARGS_((CkText *textPtr,
			    char *name, CkTextIndex *indexPtr));
extern int		CkTextXviewCmd _ANSI_ARGS_((CkText *textPtr,
			    Tcl_Interp *interp, int argc, char **argv));
extern int		CkTextYviewCmd _ANSI_ARGS_((CkText *textPtr,
			    Tcl_Interp *interp, int argc, char **argv));

#endif /* _CKTEXT_H */
