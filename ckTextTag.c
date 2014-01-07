/* 
 * ckTextTag.c --
 *
 *	This module implements the "tag" subcommand of the widget command
 *	for text widgets, plus most of the other high-level functions
 *	related to tags.
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
 * Information used for parsing tag configuration information:
 */

static Ck_ConfigSpec tagConfigSpecs[] = {
    {CK_CONFIG_ATTR, "-attributes", (char *) NULL, (char *) NULL,
	(char *) NULL, Ck_Offset(CkTextTag, attr), 0},
    {CK_CONFIG_COLOR, "-background", (char *) NULL, (char *) NULL,
	(char *) NULL, Ck_Offset(CkTextTag, bg), 0},
    {CK_CONFIG_COLOR, "-foreground", (char *) NULL, (char *) NULL,
	(char *) NULL, Ck_Offset(CkTextTag, fg), 0},
    {CK_CONFIG_STRING, "-justify", (char *) NULL, (char *) NULL,
	(char *) NULL, Ck_Offset(CkTextTag, justifyString), CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-lmargin1", (char *) NULL, (char *) NULL,
	(char *) NULL, Ck_Offset(CkTextTag, lMargin1String), CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-lmargin2", (char *) NULL, (char *) NULL,
	(char *) NULL, Ck_Offset(CkTextTag, lMargin2String), CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-rmargin", (char *) NULL, (char *) NULL,
	(char *) NULL, Ck_Offset(CkTextTag, rMarginString), CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-tabs", (char *) NULL, (char *) NULL,
	(char *) NULL, Ck_Offset(CkTextTag, tabString), CK_CONFIG_NULL_OK},
    {CK_CONFIG_UID, "-wrap", (char *) NULL, (char *) NULL,
	(char *) NULL, Ck_Offset(CkTextTag, wrapMode),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		ChangeTagPriority _ANSI_ARGS_((CkText *textPtr,
			    CkTextTag *tagPtr, int prio));
static CkTextTag *	FindTag _ANSI_ARGS_((Tcl_Interp *interp,
			    CkText *textPtr, char *tagName));
static void		SortTags _ANSI_ARGS_((int numTags,
			    CkTextTag **tagArrayPtr));
static int		TagSortProc _ANSI_ARGS_((CONST VOID *first,
			    CONST VOID *second));

/*
 *--------------------------------------------------------------
 *
 * CkTextTagCmd --
 *
 *	This procedure is invoked to process the "tag" options of
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
CkTextTagCmd(textPtr, interp, argc, argv)
    register CkText *textPtr;	/* Information about text widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings.  Someone else has already
				 * parsed this command enough to know that
				 * argv[1] is "tag". */
{
    int c, i, addTag;
    size_t length;
    char *fullOption;
    register CkTextTag *tagPtr;
    CkTextIndex first, last, index1, index2;

    if (argc < 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " tag option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[2][0];
    length = strlen(argv[2]);
    if ((c == 'a') && (strncmp(argv[2], "add", length) == 0)) {
	fullOption = "add";
	addTag = 1;

	addAndRemove:
	if (argc < 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag ", fullOption,
		    " tagName index1 ?index2 index1 index2 ...?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	tagPtr = CkTextCreateTag(textPtr, argv[3]);
	for (i = 4; i < argc; i += 2) {
	    if (CkTextGetIndex(interp, textPtr, argv[i], &index1) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (argc > (i+1)) {
		if (CkTextGetIndex(interp, textPtr, argv[i+1], &index2)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
		if (CkTextIndexCmp(&index1, &index2) >= 0) {
		    return TCL_OK;
		}
	    } else {
		index2 = index1;
		CkTextIndexForwChars(&index2, 1, &index2);
	    }
    
	    if (tagPtr->affectsDisplay) {
		CkTextRedrawTag(textPtr, &index1, &index2, tagPtr, !addTag);
	    } else {
		/*
		 * Still need to trigger enter/leave events on tags that
		 * have changed.
		 */
    
		CkTextEventuallyRepick(textPtr);
	    }
	    CkBTreeTag(&index1, &index2, tagPtr, addTag);
    
	    /*
	     * If the tag is "sel" then grab the selection if we're supposed
	     * to export it and don't already have it.  Also, invalidate
	     * partially-completed selection retrievals.
	     */
    
	    if (tagPtr == textPtr->selTagPtr) {
		textPtr->abortSelections = 1;
	    }
	}
    } else if ((c == 'b') && (strncmp(argv[2], "bind", length) == 0)) {
	if ((argc < 4) || (argc > 6)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag bind tagName ?sequence? ?command?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	tagPtr = CkTextCreateTag(textPtr, argv[3]);

	/*
	 * Make a binding table if the widget doesn't already have
	 * one.
	 */

	if (textPtr->bindingTable == NULL) {
	    textPtr->bindingTable = Ck_CreateBindingTable(interp);
	}

	if (argc == 6) {
	    int append = 0;
	    unsigned long mask;

	    if (argv[5][0] == 0) {
		return Ck_DeleteBinding(interp, textPtr->bindingTable,
			(ClientData) tagPtr, argv[4]);
	    }
	    if (argv[5][0] == '+') {
		argv[5]++;
		append = 1;
	    }
	    mask = Ck_CreateBinding(interp, textPtr->bindingTable,
		    (ClientData) tagPtr, argv[4], argv[5], append);
	    if (mask != TCL_OK) {
		return TCL_ERROR;
	    }
	} else if (argc == 5) {
	    char *command;
    
	    command = Ck_GetBinding(interp, textPtr->bindingTable,
		    (ClientData) tagPtr, argv[4]);
	    if (command == NULL) {
		return TCL_ERROR;
	    }
	    interp->result = command;
	} else {
	    Ck_GetAllBindings(interp, textPtr->bindingTable,
		    (ClientData) tagPtr);
	}
    } else if ((c == 'c') && (strncmp(argv[2], "cget", length) == 0)
	    && (length >= 2)) {
	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag cget tagName option\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	tagPtr = FindTag(interp, textPtr, argv[3]);
	if (tagPtr == NULL) {
	    return TCL_ERROR;
	}
	return Ck_ConfigureValue(interp, textPtr->winPtr, tagConfigSpecs,
		(char *) tagPtr, argv[4], 0);
    } else if ((c == 'c') && (strncmp(argv[2], "configure", length) == 0)
	    && (length >= 2)) {
	if (argc < 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag configure tagName ?option? ?value? ",
		    "?option value ...?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	tagPtr = CkTextCreateTag(textPtr, argv[3]);
	if (argc == 4) {
	    return Ck_ConfigureInfo(interp, textPtr->winPtr, tagConfigSpecs,
		    (char *) tagPtr, (char *) NULL, 0);
	} else if (argc == 5) {
	    return Ck_ConfigureInfo(interp, textPtr->winPtr, tagConfigSpecs,
		    (char *) tagPtr, argv[4], 0);
	} else {
	    int result;

	    result = Ck_ConfigureWidget(interp, textPtr->winPtr,
                    tagConfigSpecs, argc-4, argv+4, (char *) tagPtr, 0);
	    /*
	     * Some of the configuration options, like -underline
	     * and -justify, require additional translation (this is
	     * needed because we need to distinguish a particular value
	     * of an option from "unspecified").
	     */

	    if (tagPtr->justifyString != NULL) {
		if (Ck_GetJustify(interp, tagPtr->justifyString,
			&tagPtr->justify) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    if (tagPtr->lMargin1String != NULL) {
		if (Ck_GetCoord(interp, textPtr->winPtr,
			tagPtr->lMargin1String, &tagPtr->lMargin1) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    if (tagPtr->lMargin2String != NULL) {
		if (Ck_GetCoord(interp, textPtr->winPtr,
			tagPtr->lMargin2String, &tagPtr->lMargin2) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    if (tagPtr->rMarginString != NULL) {
		if (Ck_GetCoord(interp, textPtr->winPtr,
			tagPtr->rMarginString, &tagPtr->rMargin) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    if (tagPtr->tabArrayPtr != NULL) {
		ckfree((char *) tagPtr->tabArrayPtr);
		tagPtr->tabArrayPtr = NULL;
	    }
	    if (tagPtr->tabString != NULL) {
		tagPtr->tabArrayPtr = CkTextGetTabs(interp, textPtr->winPtr,
			tagPtr->tabString);
		if (tagPtr->tabArrayPtr == NULL) {
		    return TCL_ERROR;
		}
	    }
	    if ((tagPtr->wrapMode != NULL)
		    && (tagPtr->wrapMode != ckTextCharUid)
		    && (tagPtr->wrapMode != ckTextNoneUid)
		    && (tagPtr->wrapMode != ckTextWordUid)) {
		Tcl_AppendResult(interp, "bad wrap mode \"", tagPtr->wrapMode,
			"\":  must be char, none, or word", (char *) NULL);
		tagPtr->wrapMode = NULL;
		return TCL_ERROR;
	    }

	    /*
	     * If the "sel" tag was changed, be sure to mirror information
	     * from the tag back into the text widget record.   NOTE: we
	     * don't have to free up information in the widget record
	     * before overwriting it, because it was mirrored in the tag
	     * and hence freed when the tag field was overwritten.
	     */

	    if (tagPtr == textPtr->selTagPtr) {
		textPtr->selBg = tagPtr->bg;
		textPtr->selFg = tagPtr->fg;
		textPtr->selAttr = tagPtr->attr;
	    }
	    tagPtr->affectsDisplay = 1;
	    CkTextRedrawTag(textPtr, (CkTextIndex *) NULL,
		    (CkTextIndex *) NULL, tagPtr, 1);
	    return result;
	}
    } else if ((c == 'd') && (strncmp(argv[2], "delete", length) == 0)) {
	Tcl_HashEntry *hPtr;

	if (argc < 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag delete tagName tagName ...\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	for (i = 3; i < argc; i++) {
	    hPtr = Tcl_FindHashEntry(&textPtr->tagTable, argv[i]);
	    if (hPtr == NULL) {
		continue;
	    }
	    tagPtr = (CkTextTag *) Tcl_GetHashValue(hPtr);
	    if (tagPtr == textPtr->selTagPtr) {
		continue;
	    }
	    if (tagPtr->affectsDisplay) {
		CkTextRedrawTag(textPtr, (CkTextIndex *) NULL,
			(CkTextIndex *) NULL, tagPtr, 1);
	    }
#if CK_USE_UTF
	    CkBTreeTag(CkTextMakeByteIndex(textPtr->tree, 0, 0, &first),
		    CkTextMakeByteIndex(textPtr->tree,
			    CkBTreeNumLines(textPtr->tree), 0, &last),
		    tagPtr, 0);
#else
	    CkBTreeTag(CkTextMakeIndex(textPtr->tree, 0, 0, &first),
		    CkTextMakeIndex(textPtr->tree,
			    CkBTreeNumLines(textPtr->tree), 0, &last),
		    tagPtr, 0);
#endif
	    Tcl_DeleteHashEntry(hPtr);
	    if (textPtr->bindingTable != NULL) {
		Ck_DeleteAllBindings(textPtr->bindingTable,
			(ClientData) tagPtr);
	    }
	
	    /*
	     * Update the tag priorities to reflect the deletion of this tag.
	     */

	    ChangeTagPriority(textPtr, tagPtr, textPtr->numTags-1);
	    textPtr->numTags -= 1;
	    CkTextFreeTag(textPtr, tagPtr);
	}
    } else if ((c == 'l') && (strncmp(argv[2], "lower", length) == 0)) {
	CkTextTag *tagPtr2;
	int prio;

	if ((argc != 4) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag lower tagName ?belowThis?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	tagPtr = FindTag(interp, textPtr, argv[3]);
	if (tagPtr == NULL) {
	    return TCL_ERROR;
	}
	if (argc == 5) {
	    tagPtr2 = FindTag(interp, textPtr, argv[4]);
	    if (tagPtr2 == NULL) {
		return TCL_ERROR;
	    }
	    if (tagPtr->priority < tagPtr2->priority) {
		prio = tagPtr2->priority - 1;
	    } else {
		prio = tagPtr2->priority;
	    }
	} else {
	    prio = 0;
	}
	ChangeTagPriority(textPtr, tagPtr, prio);
	CkTextRedrawTag(textPtr, (CkTextIndex *) NULL, (CkTextIndex *) NULL,
		tagPtr, 1);
    } else if ((c == 'n') && (strncmp(argv[2], "names", length) == 0)
	    && (length >= 2)) {
	CkTextTag **arrayPtr;
	int arraySize;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag names ?index?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    Tcl_HashSearch search;
	    Tcl_HashEntry *hPtr;

	    arrayPtr = (CkTextTag **) ckalloc((unsigned)
		    (textPtr->numTags * sizeof(CkTextTag *)));
	    for (i = 0, hPtr = Tcl_FirstHashEntry(&textPtr->tagTable, &search);
		    hPtr != NULL; i++, hPtr = Tcl_NextHashEntry(&search)) {
		arrayPtr[i] = (CkTextTag *) Tcl_GetHashValue(hPtr);
	    }
	    arraySize = textPtr->numTags;
	} else {
	    if (CkTextGetIndex(interp, textPtr, argv[3], &index1)
		    != TCL_OK) {
		return TCL_ERROR;
	    }
	    arrayPtr = CkBTreeGetTags(&index1, &arraySize);
	    if (arrayPtr == NULL) {
		return TCL_OK;
	    }
	}
	SortTags(arraySize, arrayPtr);
	for (i = 0; i < arraySize; i++) {
	    tagPtr = arrayPtr[i];
	    Tcl_AppendElement(interp, tagPtr->name);
	}
	ckfree((char *) arrayPtr);
    } else if ((c == 'n') && (strncmp(argv[2], "nextrange", length) == 0)
	    && (length >= 2)) {
	CkTextSearch tSearch;
	char position[TK_POS_CHARS];

	if ((argc != 5) && (argc != 6)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag nextrange tagName index1 ?index2?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	tagPtr = FindTag((Tcl_Interp *) NULL, textPtr, argv[3]);
	if (tagPtr == NULL) {
	    return TCL_OK;
	}
	if (CkTextGetIndex(interp, textPtr, argv[4], &index1) != TCL_OK) {
	    return TCL_ERROR;
	}
#if CK_USE_UTF
	CkTextMakeByteIndex(textPtr->tree, CkBTreeNumLines(textPtr->tree),
		0, &last);
#else
	CkTextMakeIndex(textPtr->tree, CkBTreeNumLines(textPtr->tree),
		0, &last);
#endif
	if (argc == 5) {
	    index2 = last;
	} else if (CkTextGetIndex(interp, textPtr, argv[5], &index2)
		!= TCL_OK) {
	    return TCL_ERROR;
	}

	/*
	 * The search below is a bit tricky.  Rather than use the B-tree
	 * facilities to stop the search at index2, let it search up
	 * until the end of the file but check for a position past index2
	 * ourselves.  The reason for doing it this way is that we only
	 * care whether the *start* of the range is before index2;  once
	 * we find the start, we don't want CkBTreeNextTag to abort the
	 * search because the end of the range is after index2.
	 */

	CkBTreeStartSearch(&index1, &last, tagPtr, &tSearch);
	if (CkBTreeCharTagged(&index1, tagPtr)) {
	    CkTextSegment *segPtr;
	    int offset;

	    /*
	     * The first character is tagged.  See if there is an
	     * on-toggle just before the character.  If not, then
	     * skip to the end of this tagged range.
	     */

	    for (segPtr = index1.linePtr->segPtr, offset = index1.charIndex; 
		    offset >= 0;
		    offset -= segPtr->size, segPtr = segPtr->nextPtr) {
		if ((offset == 0) && (segPtr->typePtr == &ckTextToggleOnType)
			&& (segPtr->body.toggle.tagPtr == tagPtr)) {
		    goto gotStart;
		}
	    }
	    if (!CkBTreeNextTag(&tSearch)) {
		 return TCL_OK;
	    }
	}

	/*
	 * Find the start of the tagged range.
	 */

	if (!CkBTreeNextTag(&tSearch)) {
	    return TCL_OK;
	}
	gotStart:
	if (CkTextIndexCmp(&tSearch.curIndex, &index2) >= 0) {
	    return TCL_OK;
	}
	CkTextPrintIndex(&tSearch.curIndex, position);
	Tcl_AppendElement(interp, position);
	CkBTreeNextTag(&tSearch);
	CkTextPrintIndex(&tSearch.curIndex, position);
	Tcl_AppendElement(interp, position);
    } else if ((c == 'r') && (strncmp(argv[2], "raise", length) == 0)
	    && (length >= 3)) {
	CkTextTag *tagPtr2;
	int prio;

	if ((argc != 4) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag raise tagName ?aboveThis?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	tagPtr = FindTag(interp, textPtr, argv[3]);
	if (tagPtr == NULL) {
	    return TCL_ERROR;
	}
	if (argc == 5) {
	    tagPtr2 = FindTag(interp, textPtr, argv[4]);
	    if (tagPtr2 == NULL) {
		return TCL_ERROR;
	    }
	    if (tagPtr->priority <= tagPtr2->priority) {
		prio = tagPtr2->priority;
	    } else {
		prio = tagPtr2->priority + 1;
	    }
	} else {
	    prio = textPtr->numTags-1;
	}
	ChangeTagPriority(textPtr, tagPtr, prio);
	CkTextRedrawTag(textPtr, (CkTextIndex *) NULL, (CkTextIndex *) NULL,
		tagPtr, 1);
    } else if ((c == 'r') && (strncmp(argv[2], "ranges", length) == 0)
	    && (length >= 3)) {
	CkTextSearch tSearch;
	char position[TK_POS_CHARS];

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag ranges tagName\"", (char *) NULL);
	    return TCL_ERROR;
	}
	tagPtr = FindTag((Tcl_Interp *) NULL, textPtr, argv[3]);
	if (tagPtr == NULL) {
	    return TCL_OK;
	}
#if CK_USE_UTF
	CkTextMakeByteIndex(textPtr->tree, 0, 0, &first);
	CkTextMakeByteIndex(textPtr->tree, CkBTreeNumLines(textPtr->tree),
		0, &last);
#else
	CkTextMakeIndex(textPtr->tree, 0, 0, &first);
	CkTextMakeIndex(textPtr->tree, CkBTreeNumLines(textPtr->tree),
		0, &last);
#endif
	CkBTreeStartSearch(&first, &last, tagPtr, &tSearch);
	if (CkBTreeCharTagged(&first, tagPtr)) {
	    CkTextPrintIndex(&first, position);
	    Tcl_AppendElement(interp, position);
	}
	while (CkBTreeNextTag(&tSearch)) {
	    CkTextPrintIndex(&tSearch.curIndex, position);
	    Tcl_AppendElement(interp, position);
	}
    } else if ((c == 'r') && (strncmp(argv[2], "remove", length) == 0)
	    && (length >= 2)) {
	fullOption = "remove";
	addTag = 0;
	goto addAndRemove;
    } else {
	Tcl_AppendResult(interp, "bad tag option \"", argv[2],
		"\":  must be add, bind, cget, configure, delete, lower, ",
		"names, nextrange, raise, ranges, or remove",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CkTextCreateTag --
 *
 *	Find the record describing a tag within a given text widget,
 *	creating a new record if one doesn't already exist.
 *
 * Results:
 *	The return value is a pointer to the CkTextTag record for tagName.
 *
 * Side effects:
 *	A new tag record is created if there isn't one already defined
 *	for tagName.
 *
 *----------------------------------------------------------------------
 */

CkTextTag *
CkTextCreateTag(textPtr, tagName)
    CkText *textPtr;		/* Widget in which tag is being used. */
    char *tagName;		/* Name of desired tag. */
{
    register CkTextTag *tagPtr;
    Tcl_HashEntry *hPtr;
    int new;

    hPtr = Tcl_CreateHashEntry(&textPtr->tagTable, tagName, &new);
    if (!new) {
	return (CkTextTag *) Tcl_GetHashValue(hPtr);
    }

    /*
     * No existing entry.  Create a new one, initialize it, and add a
     * pointer to it to the hash table entry.
     */

    tagPtr = (CkTextTag *) ckalloc(sizeof(CkTextTag));
    tagPtr->name = Tcl_GetHashKey(&textPtr->tagTable, hPtr);
    tagPtr->priority = textPtr->numTags;
    tagPtr->bg = -1;
    tagPtr->fg = -1;
    tagPtr->attr = -1;
    tagPtr->justifyString = NULL;
    tagPtr->justify = CK_JUSTIFY_LEFT;
    tagPtr->lMargin1String = NULL;
    tagPtr->lMargin1 = 0;
    tagPtr->lMargin2String = NULL;
    tagPtr->lMargin2 = 0;
    tagPtr->rMarginString = NULL;
    tagPtr->rMargin = 0;
    tagPtr->tabString = NULL;
    tagPtr->tabArrayPtr = NULL;
    tagPtr->wrapMode = NULL;
    tagPtr->affectsDisplay = 0;
    textPtr->numTags++;
    Tcl_SetHashValue(hPtr, tagPtr);
    return tagPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * FindTag --
 *
 *	See if tag is defined for a given widget.
 *
 * Results:
 *	If tagName is defined in textPtr, a pointer to its CkTextTag
 *	structure is returned.  Otherwise NULL is returned and an
 *	error message is recorded in interp->result unless interp
 *	is NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static CkTextTag *
FindTag(interp, textPtr, tagName)
    Tcl_Interp *interp;		/* Interpreter to use for error message;
				 * if NULL, then don't record an error
				 * message. */
    CkText *textPtr;		/* Widget in which tag is being used. */
    char *tagName;		/* Name of desired tag. */
{
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&textPtr->tagTable, tagName);
    if (hPtr != NULL) {
	return (CkTextTag *) Tcl_GetHashValue(hPtr);
    }
    if (interp != NULL) {
	Tcl_AppendResult(interp, "tag \"", tagName,
		"\" isn't defined in text widget", (char *) NULL);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * CkTextFreeTag --
 *
 *	This procedure is called when a tag is deleted to free up the
 *	memory and other resources associated with the tag.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory and other resources are freed.
 *
 *----------------------------------------------------------------------
 */

void
CkTextFreeTag(textPtr, tagPtr)
    CkText *textPtr;			/* Info about overall widget. */
    register CkTextTag *tagPtr;		/* Tag being deleted. */
{
    if (tagPtr->justifyString != NULL) {
	ckfree(tagPtr->justifyString);
    }
    if (tagPtr->lMargin1String != NULL) {
	ckfree(tagPtr->lMargin1String);
    }
    if (tagPtr->lMargin2String != NULL) {
	ckfree(tagPtr->lMargin2String);
    }
    if (tagPtr->rMarginString != NULL) {
	ckfree(tagPtr->rMarginString);
    }
    if (tagPtr->tabString != NULL) {
	ckfree(tagPtr->tabString);
    }
    if (tagPtr->tabArrayPtr != NULL) {
	ckfree((char *) tagPtr->tabArrayPtr);
    }
    ckfree((char *) tagPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * SortTags --
 *
 *	This procedure sorts an array of tag pointers in increasing
 *	order of priority, optimizing for the common case where the
 *	array is small.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
SortTags(numTags, tagArrayPtr)
    int numTags;		/* Number of tag pointers at *tagArrayPtr. */
    CkTextTag **tagArrayPtr;	/* Pointer to array of pointers. */
{
    int i, j, prio;
    register CkTextTag **tagPtrPtr;
    CkTextTag **maxPtrPtr, *tmp;

    if (numTags < 2) {
	return;
    }
    if (numTags < 20) {
	for (i = numTags-1; i > 0; i--, tagArrayPtr++) {
	    maxPtrPtr = tagPtrPtr = tagArrayPtr;
	    prio = tagPtrPtr[0]->priority;
	    for (j = i, tagPtrPtr++; j > 0; j--, tagPtrPtr++) {
		if (tagPtrPtr[0]->priority < prio) {
		    prio = tagPtrPtr[0]->priority;
		    maxPtrPtr = tagPtrPtr;
		}
	    }
	    tmp = *maxPtrPtr;
	    *maxPtrPtr = *tagArrayPtr;
	    *tagArrayPtr = tmp;
	}
    } else {
	qsort((VOID *) tagArrayPtr, (unsigned) numTags, sizeof (CkTextTag *),
		    TagSortProc);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TagSortProc --
 *
 *	This procedure is called by qsort when sorting an array of
 *	tags in priority order.
 *
 * Results:
 *	The return value is -1 if the first argument should be before
 *	the second element (i.e. it has lower priority), 0 if it's
 *	equivalent (this should never happen!), and 1 if it should be
 *	after the second element.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TagSortProc(first, second)
    CONST VOID *first, *second;		/* Elements to be compared. */
{
    CkTextTag *tagPtr1, *tagPtr2;

    tagPtr1 = * (CkTextTag **) first;
    tagPtr2 = * (CkTextTag **) second;
    return tagPtr1->priority - tagPtr2->priority;
}

/*
 *----------------------------------------------------------------------
 *
 * ChangeTagPriority --
 *
 *	This procedure changes the priority of a tag by modifying
 *	its priority and the priorities of other tags that are affected
 *	by the change.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Priorities may be changed for some or all of the tags in
 *	textPtr.  The tags will be arranged so that there is exactly
 *	one tag at each priority level between 0 and textPtr->numTags-1,
 *	with tagPtr at priority "prio".
 *
 *----------------------------------------------------------------------
 */

static void
ChangeTagPriority(textPtr, tagPtr, prio)
    CkText *textPtr;			/* Information about text widget. */
    CkTextTag *tagPtr;			/* Tag whose priority is to be
					 * changed. */
    int prio;				/* New priority for tag. */
{
    int low, high, delta;
    register CkTextTag *tagPtr2;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;

    if (prio < 0) {
	prio = 0;
    }
    if (prio >= textPtr->numTags) {
	prio = textPtr->numTags-1;
    }
    if (prio == tagPtr->priority) {
	return;
    } else if (prio < tagPtr->priority) {
	low = prio;
	high = tagPtr->priority-1;
	delta = 1;
    } else {
	low = tagPtr->priority+1;
	high = prio;
	delta = -1;
    }
    for (hPtr = Tcl_FirstHashEntry(&textPtr->tagTable, &search);
	    hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	tagPtr2 = (CkTextTag *) Tcl_GetHashValue(hPtr);
	if ((tagPtr2->priority >= low) && (tagPtr2->priority <= high)) {
	    tagPtr2->priority += delta;
	}
    }
    tagPtr->priority = prio;
}

/*
 *--------------------------------------------------------------
 *
 * CkTextBindProc --
 *
 *	This procedure is invoked by the Ck dispatcher to handle
 *	events associated with bindings on items.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on the command invoked as part of the binding
 *	(if there was any).
 *
 *--------------------------------------------------------------
 */

void
CkTextBindProc(clientData, eventPtr)
    ClientData clientData;		/* Pointer to text structure. */
    CkEvent *eventPtr;			/* Pointer to X event that just
					 * happened. */
{
}

/*
 *--------------------------------------------------------------
 *
 * CkTextPickCurrent --
 *
 *	Find the character containing the coordinates in an event
 *	and place the "current" mark on that character.  If the
 *	"current" mark has moved then generate a fake leave event
 *	on the old current character and a fake enter event on the new
 *	current character.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The current mark for textPtr may change.  If it does,
 *	then the commands associated with character entry and leave
 *	could do just about anything.
 *
 *--------------------------------------------------------------
 */

void
CkTextPickCurrent(textPtr, eventPtr)
    register CkText *textPtr;		/* Text widget in which to select
					 * current character. */
    CkEvent *eventPtr;			/* Event describing location of
					 * mouse cursor.  Must be EnterWindow,
					 * LeaveWindow, ButtonRelease, or
					 * MotionNotify. */
{
}
