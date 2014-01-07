/* 
 * ckPlace.c --
 *
 *	This file contains code to implement a simple geometry manager
 *	for Ck based on absolute placement or "rubber-sheet" placement.
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

/*
 * Border modes for relative placement:
 *
 * BM_INSIDE:		relative distances computed using area inside
 *			all borders of master window.
 * BM_IGNORE:		border issues are ignored:  place relative to
 *			master's actual window size.
 */

typedef enum {BM_INSIDE, BM_IGNORE} BorderMode;

/*
 * For each window whose geometry is managed by the placer there is
 * a structure of the following type:
 */

typedef struct Slave {
    CkWindow *winPtr;		/* Pointer to window. */
    struct Master *masterPtr;	/* Pointer to information for window
				 * relative to which winPtr is placed.
				 * This isn't necessarily the logical
				 * parent of winPtr.  NULL means the
				 * master was deleted or never assigned. */
    struct Slave *nextPtr;	/* Next in list of windows placed relative
				 * to same master (NULL for end of list). */

    /*
     * Geometry information for window;  where there are both relative
     * and absolute values for the same attribute (e.g. x and relX) only
     * one of them is actually used, depending on flags.
     */

    int x, y;			/* X and Y coordinates for winPtr. */
    double relX, relY;		/* X and Y coordinates relative to size of
				 * master. */
    int width, height;		/* Absolute dimensions for winPtr. */
    double relWidth, relHeight;	/* Dimensions for winPtr relative to size of
				 * master. */
    Ck_Anchor anchor;		/* Which point on winPtr is placed at the
				 * given position. */
    BorderMode borderMode;	/* How to treat borders of master window. */
    int flags;			/* Various flags;  see below for bit
				 * definitions. */
} Slave;

/*
 * Flag definitions for Slave structures:
 *
 * CHILD_REL_X -		1 means use relX field;  0 means use x.
 * CHILD_REL_Y -		1 means use relY field;  0 means use y;
 * CHILD_WIDTH -		1 means use width field;
 * CHILD_REL_WIDTH -		1 means use relWidth;  if neither this nor
 *				CHILD_WIDTH is 1, use window's requested
 *				width.
 * CHILD_HEIGHT -		1 means use height field;
 * CHILD_REL_HEIGHT -		1 means use relHeight;  if neither this nor
 *				CHILD_HEIGHT is 1, use window's requested
 *				height.
 */

#define CHILD_REL_X		1
#define CHILD_REL_Y		2
#define CHILD_WIDTH		4
#define CHILD_REL_WIDTH		8
#define CHILD_HEIGHT		0x10
#define CHILD_REL_HEIGHT	0x20

/*
 * For each master window that has a slave managed by the placer there
 * is a structure of the following form:
 */

typedef struct Master {
    CkWindow *winPtr;		/* Pointer to master window. */
    struct Slave *slavePtr;	/* First in linked list of slaves
				 * placed relative to this master. */
    int flags;			/* See below for bit definitions. */
} Master;

/*
 * Flag definitions for masters:
 *
 * PARENT_RECONFIG_PENDING -	1 means that a call to RecomputePlacement
 *				is already pending via a Do_When_Idle handler.
 */

#define PARENT_RECONFIG_PENDING	1

/*
 * The hash tables below both use CkWindow pointers as keys.  They map
 * from CkWindows to Slave and Master structures for windows, if they
 * exist.
 */

static int initialized = 0;
static Tcl_HashTable masterTable;
static Tcl_HashTable slaveTable;

/*
 * The following structure is the official type record for the
 * placer:
 */

static void		PlaceRequestProc _ANSI_ARGS_((ClientData clientData,
			    CkWindow *winPtr));
static void		PlaceLostSlaveProc _ANSI_ARGS_((ClientData clientData,
			    CkWindow *winPtr));

static Ck_GeomMgr placerType = {
    "place",				/* name */
    PlaceRequestProc,			/* requestProc */
    PlaceLostSlaveProc,			/* lostSlaveProc */
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		SlaveStructureProc _ANSI_ARGS_((ClientData clientData,
			    CkEvent *eventPtr));
static int		ConfigureSlave _ANSI_ARGS_((Tcl_Interp *interp,
			    Slave *slavePtr, int argc, char **argv));
static Slave *		FindSlave _ANSI_ARGS_((CkWindow *winPtr));
static Master *		FindMaster _ANSI_ARGS_((CkWindow *winPtr));
static void		MasterStructureProc _ANSI_ARGS_((ClientData clientData,
			    CkEvent *eventPtr));
static void		RecomputePlacement _ANSI_ARGS_((ClientData clientData));
static void		UnlinkSlave _ANSI_ARGS_((Slave *slavePtr));

/*
 *--------------------------------------------------------------
 *
 * Ck_PlaceCmd --
 *
 *	This procedure is invoked to process the "place" Tcl
 *	commands.  See the user documentation for details on
 *	what it does.
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
Ck_PlaceCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *winPtr;
    Slave *slavePtr;
    Tcl_HashEntry *hPtr;
    int length;
    char c;

    /*
     * Initialize, if that hasn't been done yet.
     */

    if (!initialized) {
	Tcl_InitHashTable(&masterTable, TCL_ONE_WORD_KEYS);
	Tcl_InitHashTable(&slaveTable, TCL_ONE_WORD_KEYS);
	initialized = 1;
    }

    if (argc < 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option|pathName args", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);

    /*
     * Handle special shortcut where window name is first argument.
     */

    if (c == '.') {
	winPtr = Ck_NameToWindow(interp, argv[1], (CkWindow *) clientData);
	if (winPtr == NULL) {
	    return TCL_ERROR;
	}
	slavePtr = FindSlave(winPtr);
	return ConfigureSlave(interp, slavePtr, argc-2, argv+2);
    }

    /*
     * Handle more general case of option followed by window name followed
     * by possible additional arguments.
     */

    winPtr = Ck_NameToWindow(interp, argv[2], (CkWindow *) clientData);
    if (winPtr == NULL) {
	return TCL_ERROR;
    }
    if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)) {
	if (argc < 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0],
		    " configure pathName option value ?option value ...?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	slavePtr = FindSlave(winPtr);
	return ConfigureSlave(interp, slavePtr, argc-3, argv+3);
    } else if ((c == 'f') && (strncmp(argv[1], "forget", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " forget pathName\"", (char *) NULL);
	    return TCL_ERROR;
	}
	hPtr = Tcl_FindHashEntry(&slaveTable, (char *) winPtr);
	if (hPtr == NULL) {
	    return TCL_OK;
	}
	slavePtr = (Slave *) Tcl_GetHashValue(hPtr);
	UnlinkSlave(slavePtr);
	Tcl_DeleteHashEntry(hPtr);
	Ck_DeleteEventHandler(winPtr, CK_EV_MAP | CK_EV_EXPOSE | CK_EV_DESTROY,
	        SlaveStructureProc, (ClientData) slavePtr);
	Ck_ManageGeometry(winPtr, (Ck_GeomMgr *) NULL, (ClientData) NULL);
	Ck_UnmapWindow(winPtr);
	ckfree((char *) slavePtr);
    } else if ((c == 'i') && (strncmp(argv[1], "info", length) == 0)) {
	char buffer[50];

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " info pathName\"", (char *) NULL);
	    return TCL_ERROR;
	}
	hPtr = Tcl_FindHashEntry(&slaveTable, (char *) winPtr);
	if (hPtr == NULL) {
	    return TCL_OK;
	}
	slavePtr = (Slave *) Tcl_GetHashValue(hPtr);
	if (slavePtr->flags & CHILD_REL_X) {
	    sprintf(buffer, "-relx %.4g", slavePtr->relX);
	} else {
	    sprintf(buffer, "-x %d", slavePtr->x);
	}
	Tcl_AppendResult(interp, buffer, (char *) NULL);
	if (slavePtr->flags & CHILD_REL_Y) {
	    sprintf(buffer, " -rely %.4g", slavePtr->relY);
	} else {
	    sprintf(buffer, " -y %d", slavePtr->y);
	}
	Tcl_AppendResult(interp, buffer, (char *) NULL);
	if (slavePtr->flags & CHILD_REL_WIDTH) {
	    sprintf(buffer, " -relwidth %.4g", slavePtr->relWidth);
	    Tcl_AppendResult(interp, buffer, (char *) NULL);
	} else if (slavePtr->flags & CHILD_WIDTH) {
	    sprintf(buffer, " -width %d", slavePtr->width);
	    Tcl_AppendResult(interp, buffer, (char *) NULL);
	}
	if (slavePtr->flags & CHILD_REL_HEIGHT) {
	    sprintf(buffer, " -relheight %.4g", slavePtr->relHeight);
	    Tcl_AppendResult(interp, buffer, (char *) NULL);
	} else if (slavePtr->flags & CHILD_HEIGHT) {
	    sprintf(buffer, " -height %d", slavePtr->height);
	    Tcl_AppendResult(interp, buffer, (char *) NULL);
	}
	Tcl_AppendResult(interp, " -anchor ", Ck_NameOfAnchor(slavePtr->anchor),
		(char *) NULL);
	if (slavePtr->borderMode == BM_IGNORE) {
	    Tcl_AppendResult(interp, " -bordermode ignore", (char *) NULL);
	}
    } else if ((c == 's') && (strncmp(argv[1], "slaves", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " slaves pathName\"", (char *) NULL);
	    return TCL_ERROR;
	}
	hPtr = Tcl_FindHashEntry(&masterTable, (char *) winPtr);
	if (hPtr != NULL) {
	    Master *masterPtr;
	    masterPtr = (Master *) Tcl_GetHashValue(hPtr);
	    for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
		    slavePtr = slavePtr->nextPtr) {
		Tcl_AppendElement(interp, slavePtr->winPtr->pathName);
	    }
	}
    } else {
	Tcl_AppendResult(interp, "unknown or ambiguous option \"", argv[1],
		"\": must be configure, forget, info, or slaves",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FindSlave --
 *
 *	Given a CkWindow *, find the Slave structure corresponding
 *	to that window (making a new one if necessary).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new Slave structure may be created.
 *
 *----------------------------------------------------------------------
 */

static Slave *
FindSlave(winPtr)
    CkWindow *winPtr;		/* Pointer to desired slave. */
{
    Tcl_HashEntry *hPtr;
    Slave *slavePtr;
    int new;

    hPtr = Tcl_CreateHashEntry(&slaveTable, (char *) winPtr, &new);
    if (new) {
	slavePtr = (Slave *) ckalloc(sizeof (Slave));
	slavePtr->winPtr = winPtr;
	slavePtr->masterPtr = NULL;
	slavePtr->nextPtr = NULL;
	slavePtr->x = slavePtr->y = 0;
	slavePtr->relX = slavePtr->relY = 0.0;
	slavePtr->width = slavePtr->height = 0;
	slavePtr->relWidth = slavePtr->relHeight = 0.0;
	slavePtr->anchor = CK_ANCHOR_NW;
	slavePtr->borderMode = BM_INSIDE;
	slavePtr->flags = 0;
	Tcl_SetHashValue(hPtr, slavePtr);
	Ck_CreateEventHandler(winPtr, CK_EV_MAP | CK_EV_EXPOSE | CK_EV_DESTROY,
                SlaveStructureProc, (ClientData) slavePtr);
	Ck_ManageGeometry(winPtr, &placerType, (ClientData) slavePtr);
    } else {
	slavePtr = (Slave *) Tcl_GetHashValue(hPtr);
    }
    return slavePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * UnlinkSlave --
 *
 *	This procedure removes a slave window from the chain of slaves
 *	in its master.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The slave list of slavePtr's master changes.
 *
 *----------------------------------------------------------------------
 */

static void
UnlinkSlave(slavePtr)
    Slave *slavePtr;		/* Slave structure to be unlinked. */
{
    register Master *masterPtr;
    register Slave *prevPtr;

    masterPtr = slavePtr->masterPtr;
    if (masterPtr == NULL) {
	return;
    }
    if (masterPtr->slavePtr == slavePtr) {
	masterPtr->slavePtr = slavePtr->nextPtr;
    } else {
	for (prevPtr = masterPtr->slavePtr; ;
		prevPtr = prevPtr->nextPtr) {
	    if (prevPtr == NULL) {
		panic("UnlinkSlave couldn't find slave to unlink");
	    }
	    if (prevPtr->nextPtr == slavePtr) {
		prevPtr->nextPtr = slavePtr->nextPtr;
		break;
	    }
	}
    }
    slavePtr->masterPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * FindMaster --
 *
 *	Given a CkWindow *, find the Master structure corresponding
 *	to that window (making a new one if necessary).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new Master structure may be created.
 *
 *----------------------------------------------------------------------
 */

static Master *
FindMaster(winPtr)
    CkWindow *winPtr;		/* Pointer to desired master. */
{
    Tcl_HashEntry *hPtr;
    Master *masterPtr;
    int new;

    hPtr = Tcl_CreateHashEntry(&masterTable, (char *) winPtr, &new);
    if (new) {
	masterPtr = (Master *) ckalloc(sizeof (Master));
	masterPtr->winPtr = winPtr;
	masterPtr->slavePtr = NULL;
	masterPtr->flags = 0;
	Tcl_SetHashValue(hPtr, masterPtr);
	/*
	 * Special case: for toplevels winPtr is NULL,
	 * therefore don't create event handler.
	 */
	if (winPtr != NULL)
	    Ck_CreateEventHandler(masterPtr->winPtr,
		CK_EV_MAP | CK_EV_EXPOSE | CK_EV_DESTROY,
		MasterStructureProc, (ClientData) masterPtr);
    } else {
	masterPtr = (Master *) Tcl_GetHashValue(hPtr);
    }
    return masterPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureSlave --
 *
 *	This procedure is called to process an argv/argc list to
 *	reconfigure the placement of a window.
 *
 * Results:
 *	A standard Tcl result.  If an error occurs then a message is
 *	left in interp->result.
 *
 * Side effects:
 *	Information in slavePtr may change, and slavePtr's master is
 *	scheduled for reconfiguration.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureSlave(interp, slavePtr, argc, argv)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Slave *slavePtr;		/* Pointer to current information
				 * about slave. */
    int argc;			/* Number of config arguments. */
    char **argv;		/* String values for arguments. */
{
    Master *masterPtr;
    int c, length, result;
    double d;

    result = TCL_OK;
    for ( ; argc > 0; argc -= 2, argv += 2) {
	if (argc < 2) {
	    Tcl_AppendResult(interp, "extra option \"", argv[0],
		    "\" (option with no value?)", (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}
	length = strlen(argv[0]);
	c = argv[0][1];
	if ((c == 'a') && (strncmp(argv[0], "-anchor", length) == 0)) {
	    if (Ck_GetAnchor(interp, argv[1], &slavePtr->anchor) != TCL_OK) {
		result = TCL_ERROR;
		goto done;
	    }
	} else if ((c == 'b')
		&& (strncmp(argv[0], "-bordermode", length) == 0)) {
	    c = argv[1][0];
	    length = strlen(argv[1]);
	    if ((c == 'i') && (strncmp(argv[1], "ignore", length) == 0)
		    && (length >= 2)) {
		slavePtr->borderMode = BM_IGNORE;
	    } else if ((c == 'i') && (strncmp(argv[1], "inside", length) == 0)
		    && (length >= 2)) {
		slavePtr->borderMode = BM_INSIDE;
	    } else {
		Tcl_AppendResult(interp, "bad border mode \"", argv[1],
			"\": must be ignore or inside",
			(char *) NULL);
		result = TCL_ERROR;
		goto done;
	    }
	} else if ((c == 'h') && (strncmp(argv[0], "-height", length) == 0)) {
	    if (argv[1][0] == 0) {
		slavePtr->flags &= ~(CHILD_REL_HEIGHT|CHILD_HEIGHT);
	    } else {
		if (Ck_GetCoord(interp, slavePtr->winPtr, argv[1],
			&slavePtr->height) != TCL_OK) {
		    result = TCL_ERROR;
		    goto done;
		}
		slavePtr->flags &= ~CHILD_REL_HEIGHT;
		slavePtr->flags |= CHILD_HEIGHT;
	    }
	} else if ((c == 'r') && (strncmp(argv[0], "-relheight", length) == 0)
		&& (length >= 5)) {
	    if (Tcl_GetDouble(interp, argv[1], &d) != TCL_OK) {
		result = TCL_ERROR;
		goto done;
	    }
	    slavePtr->relHeight = d;
	    slavePtr->flags |= CHILD_REL_HEIGHT;
	    slavePtr->flags &= ~CHILD_HEIGHT;
	} else if ((c == 'r') && (strncmp(argv[0], "-relwidth", length) == 0)
		&& (length >= 5)) {
	    if (Tcl_GetDouble(interp, argv[1], &d) != TCL_OK) {
		result = TCL_ERROR;
		goto done;
	    }
	    slavePtr->relWidth = d;
	    slavePtr->flags |= CHILD_REL_WIDTH;
	    slavePtr->flags &= ~CHILD_WIDTH;
	} else if ((c == 'r') && (strncmp(argv[0], "-relx", length) == 0)
		&& (length >= 5)) {
	    if (Tcl_GetDouble(interp, argv[1], &d) != TCL_OK) {
		result = TCL_ERROR;
		goto done;
	    }
	    slavePtr->relX = d;
	    slavePtr->flags |= CHILD_REL_X;
	} else if ((c == 'r') && (strncmp(argv[0], "-rely", length) == 0)
		&& (length >= 5)) {
	    if (Tcl_GetDouble(interp, argv[1], &d) != TCL_OK) {
		result = TCL_ERROR;
		goto done;
	    }
	    slavePtr->relY = d;
	    slavePtr->flags |= CHILD_REL_Y;
	} else if ((c == 'w') && (strncmp(argv[0], "-width", length) == 0)) {
	    if (argv[1][0] == 0) {
		slavePtr->flags &= ~(CHILD_REL_WIDTH|CHILD_WIDTH);
	    } else {
		if (Ck_GetCoord(interp, slavePtr->winPtr, argv[1],
			&slavePtr->width) != TCL_OK) {
		    result = TCL_ERROR;
		    goto done;
		}
		slavePtr->flags &= ~CHILD_REL_WIDTH;
		slavePtr->flags |= CHILD_WIDTH;
	    }
	} else if ((c == 'x') && (strncmp(argv[0], "-x", length) == 0)) {
	    if (Ck_GetCoord(interp, slavePtr->winPtr, argv[1],
		    &slavePtr->x) != TCL_OK) {
		result = TCL_ERROR;
		goto done;
	    }
	    slavePtr->flags &= ~CHILD_REL_X;
	} else if ((c == 'y') && (strncmp(argv[0], "-y", length) == 0)) {
	    if (Ck_GetCoord(interp, slavePtr->winPtr, argv[1],
		    &slavePtr->y) != TCL_OK) {
		result = TCL_ERROR;
		goto done;
	    }
	    slavePtr->flags &= ~CHILD_REL_Y;
	} else {
	    Tcl_AppendResult(interp, "unknown or ambiguous option \"",
		    argv[0], "\": must be -anchor, -bordermode, -height, ",
		    "-relheight, -relwidth, -relx, -rely, -width, ",
		    "-x, or -y", (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}
    }

    /*
     * Arrange for a placement recalculation in the master.
     */

done:
    masterPtr = slavePtr->masterPtr;
    if (masterPtr == NULL) {
	masterPtr = FindMaster((slavePtr->winPtr->flags & CK_TOPLEVEL) ?
	    NULL : slavePtr->winPtr->parentPtr);
	slavePtr->masterPtr = masterPtr;
	slavePtr->nextPtr = masterPtr->slavePtr;
	masterPtr->slavePtr = slavePtr;
    }
    if (!(masterPtr->flags & PARENT_RECONFIG_PENDING)) {
	masterPtr->flags |= PARENT_RECONFIG_PENDING;
	Tk_DoWhenIdle(RecomputePlacement, (ClientData) masterPtr);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * RecomputePlacement --
 *
 *	This procedure is called as a when-idle handler.  It recomputes
 *	the geometries of all the slaves of a given master.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Windows may change size or shape.
 *
 *----------------------------------------------------------------------
 */

static void
RecomputePlacement(clientData)
    ClientData clientData;	/* Pointer to Master record. */
{
    Master *masterPtr = (Master *) clientData;
    Slave *slavePtr;
    CkWindow *ancestor, *realMaster;
    int x, y, width, height;
    int masterWidth, masterHeight, masterBW;

    masterPtr->flags &= ~PARENT_RECONFIG_PENDING;

    /*
     * Iterate over all the slaves for the master.  Each slave's
     * geometry can be computed independently of the other slaves.
     */

    for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
	    slavePtr = slavePtr->nextPtr) {
	/*
	 * Step 1: compute size and borderwidth of master, taking into
	 * account desired border mode.
	 */

	masterBW = 0;

	/*
	 * Special case: masterPtr->winPtr == NULL, use entire screen !
	 */

	if (masterPtr->winPtr == NULL) {
	    masterWidth = slavePtr->winPtr->mainPtr->maxWidth;
	    masterHeight = slavePtr->winPtr->mainPtr->maxHeight;
	} else {
	    masterWidth = masterPtr->winPtr->width;
	    masterHeight = masterPtr->winPtr->height;
	    if (slavePtr->borderMode == BM_INSIDE) {
		masterBW = (masterPtr->winPtr->flags & CK_BORDER) ? 1 : 0;
	    }
	    masterWidth -= 2*masterBW;
	    masterHeight -= 2*masterBW;
	}

	/*
	 * Step 2:  compute size of slave (outside dimensions including
	 * border) and location of anchor point within master.
	 */

	x = slavePtr->x;
	if (slavePtr->flags & CHILD_REL_X) {
	    x = (int) ((slavePtr->relX*masterWidth) +
		       ((slavePtr->relX > 0) ? 0.5 : -0.5));
	}
	x += masterBW;
	y = slavePtr->y;
	if (slavePtr->flags & CHILD_REL_Y) {
	    y = (int) ((slavePtr->relY*masterHeight) +
		       ((slavePtr->relY > 0) ? 0.5 : -0.5));
	}
	y += masterBW;
	if (slavePtr->flags & CHILD_REL_WIDTH) {
	    width = (int) ((slavePtr->relWidth*masterWidth) + 0.5);
	} else if (slavePtr->flags & CHILD_WIDTH) {
	    width = slavePtr->width;
	} else {
	    width = slavePtr->winPtr->reqWidth;
	}
	if (slavePtr->flags & CHILD_REL_HEIGHT) {
	    height = (int) ((slavePtr->relHeight*masterHeight) + 0.5);
	} else if (slavePtr->flags & CHILD_HEIGHT) {
	    height = slavePtr->height;
	} else {
	    height = slavePtr->winPtr->reqHeight;
	}

	/*
	 * Step 3: adjust the x and y positions so that the desired
	 * anchor point on the slave appears at that position.  Also
	 * adjust for the border mode and master's border.
	 */

	switch (slavePtr->anchor) {
	    case CK_ANCHOR_N:
		x -= width/2;
		break;
	    case CK_ANCHOR_NE:
		x -= width;
		break;
	    case CK_ANCHOR_E:
		x -= width;
		y -= height/2;
		break;
	    case CK_ANCHOR_SE:
		x -= width;
		y -= height;
		break;
	    case CK_ANCHOR_S:
		x -= width/2;
		y -= height;
		break;
	    case CK_ANCHOR_SW:
		y -= height;
		break;
	    case CK_ANCHOR_W:
		y -= height/2;
		break;
	    case CK_ANCHOR_NW:
		break;
	    case CK_ANCHOR_CENTER:
		x -= width/2;
		y -= height/2;
		break;
	}

	/*
	 * Step 4: if masterPtr isn't actually the master of slavePtr,
	 * then translate the x and y coordinates back into the coordinate
	 * system of masterPtr.
	 */

	for (ancestor = masterPtr->winPtr,
	     realMaster = slavePtr->winPtr->parentPtr;
	     ancestor != NULL && ancestor != realMaster;
	     ancestor = ancestor->parentPtr) {
	    x += ancestor->x;
	    y += ancestor->y;
	}

	/*
	 * Step 5: adjust width and height again to reflect inside dimensions
	 * of window rather than outside.  Also make sure that the width and
	 * height aren't zero.
	 */

	if (width <= 0) {
	    width = 1;
	}
	if (height <= 0) {
	    height = 1;
	}

	/*
	 * Step 6: see if the window's size or location has changed;  if
	 * so then resize and/or move it.
	 */

	if (width != slavePtr->winPtr->width ||
	    height != slavePtr->winPtr->height)
	    Ck_ResizeWindow(slavePtr->winPtr, width, height);
	if (x != slavePtr->winPtr->x ||
	    y != slavePtr->winPtr->y)
            Ck_MoveWindow(slavePtr->winPtr, x, y);
	/*
	 * Temporary kludge til Ck_MoveResizeWindow available !!!
	 */
	if (width != slavePtr->winPtr->width ||
	    height != slavePtr->winPtr->height)
	    Ck_ResizeWindow(slavePtr->winPtr, width, height);

	Ck_MapWindow(slavePtr->winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MasterStructureProc --
 *
 *	This procedure is invoked by the event handler when
 *	CK_EV_MAP/CK_EV_EXPOSE/CK_EV_DESTROY events occur for
 *      a master window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Structures get cleaned up if the window was deleted.  If the
 *	window was resized then slave geometries get recomputed.
 *
 *----------------------------------------------------------------------
 */

static void
MasterStructureProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to Master structure for window
				 * referred to by eventPtr. */
    CkEvent *eventPtr;		/* Describes what just happened. */
{
    Master *masterPtr = (Master *) clientData;
    Slave *slavePtr, *nextPtr;

    if (eventPtr->type == CK_EV_EXPOSE ||
        eventPtr->type == CK_EV_MAP) {
	if ((masterPtr->slavePtr != NULL)
		&& !(masterPtr->flags & PARENT_RECONFIG_PENDING)) {
	    masterPtr->flags |= PARENT_RECONFIG_PENDING;
	    Tk_DoWhenIdle(RecomputePlacement, (ClientData) masterPtr);
	}
    } else if (eventPtr->type == CK_EV_DESTROY) {
	for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
		slavePtr = nextPtr) {
	    slavePtr->masterPtr = NULL;
	    nextPtr = slavePtr->nextPtr;
	    slavePtr->nextPtr = NULL;
	}
	Tcl_DeleteHashEntry(Tcl_FindHashEntry(&masterTable,
		(char *) masterPtr->winPtr));
	if (masterPtr->flags & PARENT_RECONFIG_PENDING) {
	    Tk_CancelIdleCall(RecomputePlacement, (ClientData) masterPtr);
	}
	masterPtr->winPtr = NULL;
	ckfree((char *) masterPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SlaveStructureProc --
 *
 *	This procedure is invoked by the event handler when
 *	CK_EV_MAP/CK_EV_EXPOSE/CK_EV_DESTROY events occur for a
 *      slave window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Structures get cleaned up if the window was deleted.
 *
 *----------------------------------------------------------------------
 */

static void
SlaveStructureProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to Slave structure for window
				 * referred to by eventPtr. */
    CkEvent *eventPtr;		/* Describes what just happened. */
{
    Slave *slavePtr = (Slave *) clientData;

    if (eventPtr->type == CK_EV_DESTROY) {
	UnlinkSlave(slavePtr);
	Tcl_DeleteHashEntry(Tcl_FindHashEntry(&slaveTable,
		(char *) slavePtr->winPtr));
	ckfree((char *) slavePtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PlaceRequestProc --
 *
 *	This procedure is invoked whenever a slave managed by us
 *	changes its requested geometry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window will get relayed out, if its requested size has
 *	anything to do with its actual size.
 *
 *----------------------------------------------------------------------
 */

static void
PlaceRequestProc(clientData, winPtr)
    ClientData clientData;		/* Pointer to our record for slave. */
    CkWindow *winPtr;			/* Window that changed its desired
					 * size. */
{
    Slave *slavePtr = (Slave *) clientData;
    Master *masterPtr;

    if (((slavePtr->flags & (CHILD_WIDTH|CHILD_REL_WIDTH)) != 0)
	    && ((slavePtr->flags & (CHILD_HEIGHT|CHILD_REL_HEIGHT)) != 0)) {
	return;
    }
    masterPtr = slavePtr->masterPtr;
    if (masterPtr == NULL) {
	return;
    }
    if (!(masterPtr->flags & PARENT_RECONFIG_PENDING)) {
	masterPtr->flags |= PARENT_RECONFIG_PENDING;
	Tk_DoWhenIdle(RecomputePlacement, (ClientData) masterPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * PlaceLostSlaveProc --
 *
 *	This procedure is invoked whenever some other geometry
 *	claims control over a slave that used to be managed by us.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Forgets all placer-related information about the slave.
 *
 *--------------------------------------------------------------
 */

static void
PlaceLostSlaveProc(clientData, winPtr)
    ClientData clientData;	/* Slave structure for slave window that
				 * was stolen away. */
    CkWindow *winPtr;		/* Slave window. */
{
    register Slave *slavePtr = (Slave *) clientData;

    if (slavePtr->masterPtr->winPtr != slavePtr->winPtr->parentPtr) {
	Ck_UnmaintainGeometry(slavePtr->winPtr, slavePtr->masterPtr->winPtr);
    }
    Ck_UnmapWindow(winPtr);
    UnlinkSlave(slavePtr);
    Tcl_DeleteHashEntry(Tcl_FindHashEntry(&slaveTable, (char *) winPtr));
    Ck_DeleteEventHandler(winPtr, CK_EV_MAP | CK_EV_EXPOSE | CK_EV_DESTROY,
    	SlaveStructureProc, (ClientData) slavePtr);
    ckfree((char *) slavePtr);
}
