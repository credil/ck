/* 
 * ckPack.c --
 *
 *	This file contains code to implement the "packer"
 *	geometry manager.
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

typedef enum {TOP, BOTTOM, LEFT, RIGHT} Side;

/* For each window that the packer cares about (either because
 * the window is managed by the packer or because the window
 * has slaves that are managed by the packer), there is a
 * structure of the following type:
 */

typedef struct Packer {
    CkWindow *winPtr;		/* Pointer to window.  NULL means that
				 * the window has been deleted, but the
				 * packer hasn't had a chance to clean up
				 * yet because the structure is still in
				 * use. */
    struct Packer *masterPtr;	/* Master window within which this window
				 * is packed (NULL means this window
				 * isn't managed by the packer). */
    struct Packer *nextPtr;	/* Next window packed within same
				 * parent.  List is priority-ordered:
				 * first on list gets packed first. */
    struct Packer *slavePtr;	/* First in list of slaves packed
				 * inside this window (NULL means
				 * no packed slaves). */
    Side side;			/* Side of parent against which
				 * this window is packed. */
    Ck_Anchor anchor;		/* If frame allocated for window is larger
				 * than window needs, this indicates how
				 * where to position window in frame. */
    int padX, padY;		/* Total additional pixels to leave around the
				 * window (half of this space is left on each
				 * side).  This is space *outside* the window:
				 * we'll allocate extra space in frame but
				 * won't enlarge window). */
    int iPadX, iPadY;		/* Total extra pixels to allocate inside the
				 * window (half this amount will appear on
				 * each side). */
    int *abortPtr;		/* If non-NULL, it means that there is a nested
				 * call to ArrangePacking already working on
				 * this window.  *abortPtr may be set to 1 to
				 * abort that nested call.  This happens, for
				 * example, if tkwin or any of its slaves
				 * is deleted. */
    int flags;			/* Miscellaneous flags;  see below
				 * for definitions. */
} Packer;

/*
 * Flag values for Packer structures:
 *
 * REQUESTED_REPACK:		1 means a Ck_DoWhenIdle request
 *				has already been made to repack
 *				all the slaves of this window.
 * FILLX:			1 means if frame allocated for window
 *				is wider than window needs, expand window
 *				to fill frame.  0 means don't make window
 *				any larger than needed.
 * FILLY:			Same as FILLX, except for height.
 * EXPAND:			1 means this window's frame will absorb any
 *				extra space in the parent window.
 * DONT_PROPAGATE:		1 means don't set this window's requested
 *				size.  0 means if this window is a master
 *				then Tk will set its requested size to fit
 *				the needs of its slaves.
 */

#define REQUESTED_REPACK	1
#define FILLX			2
#define FILLY			4
#define EXPAND			8
#define DONT_PROPAGATE		16

/*
 * Hash table used to map from CkWindow pointers to corresponding
 * Packer structures:
 */

static Tcl_HashTable packerHashTable;

/*
 * Have statics in this module been initialized?
 */

static int initialized = 0;

/*
 * The following structure is the official type record for the
 * packer:
 */

static void		PackReqProc _ANSI_ARGS_((ClientData clientData,
			    CkWindow *winPtr));
static void		PackLostSlaveProc _ANSI_ARGS_((ClientData clientData,
			    CkWindow *winPtr));

static Ck_GeomMgr packerType = {
    "pack",			/* name */
    PackReqProc,		/* requestProc */
    PackLostSlaveProc,		/* lostSlaveProc */
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		ArrangePacking _ANSI_ARGS_((ClientData clientData));
static int		ConfigureSlaves _ANSI_ARGS_((Tcl_Interp *interp,
			    CkWindow *winPtr, int argc, char *argv[]));
static Packer *		GetPacker _ANSI_ARGS_((CkWindow *winPtr));
static void		PackStructureProc _ANSI_ARGS_((ClientData clientData,
			    CkEvent *eventPtr));
static void		Unlink _ANSI_ARGS_((Packer *packPtr));
static int		XExpansion _ANSI_ARGS_((Packer *slavePtr,
			    int cavityWidth));
static int		YExpansion _ANSI_ARGS_((Packer *slavePtr,
			    int cavityHeight));

/*
 *--------------------------------------------------------------
 *
 * Ck_PackCmd --
 *
 *	This procedure is invoked to process the "pack" Tcl command.
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
Ck_PackCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *mainPtr = (CkWindow *) clientData;
    size_t length;
    int c;

    if ((argc >= 2) && (argv[1][0] == '.')) {
	return ConfigureSlaves(interp, mainPtr, argc-1, argv+1);
    }
    if (argc < 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option arg ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)) {
	if (argv[2][0] != '.') {
	    Tcl_AppendResult(interp, "bad argument \"", argv[2],
		    "\": must be name of window", (char *) NULL);
	    return TCL_ERROR;
	}
	return ConfigureSlaves(interp, mainPtr, argc-2, argv+2);
    } else if ((c == 'f') && (strncmp(argv[1], "forget", length) == 0)) {
	CkWindow *slave;
	Packer *slavePtr;
	int i;

	for (i = 2; i < argc; i++) {
	    slave = Ck_NameToWindow(interp, argv[i], mainPtr);
	    if (slave == NULL) {
		continue;
	    }
	    slavePtr = GetPacker(slave);
	    if ((slavePtr != NULL) && (slavePtr->masterPtr != NULL)) {
		Ck_ManageGeometry(slave, (Ck_GeomMgr *) NULL,
			(ClientData) NULL);
		Unlink(slavePtr);
		Ck_UnmapWindow(slavePtr->winPtr);
	    }
	}
    } else if ((c == 'i') && (strncmp(argv[1], "info", length) == 0)) {
	register Packer *slavePtr;
	CkWindow *slave;
	char buffer[300];
	static char *sideNames[] = {"top", "bottom", "left", "right"};

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " info window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	slave = Ck_NameToWindow(interp, argv[2], mainPtr);
	if (slave == NULL) {
	    return TCL_ERROR;
	}
	slavePtr = GetPacker(slave);
	if (slavePtr->masterPtr == NULL) {
	    Tcl_AppendResult(interp, "window \"", argv[2],
		    "\" isn't packed", (char *) NULL);
	    return TCL_ERROR;
	}
	Tcl_AppendElement(interp, "-anchor");
	Tcl_AppendElement(interp, Ck_NameOfAnchor(slavePtr->anchor));
	Tcl_AppendResult(interp, " -expand ",
		(slavePtr->flags & EXPAND) ? "1" : "0", " -fill ",
		(char *) NULL);
	switch (slavePtr->flags & (FILLX|FILLY)) {
	    case 0:
		Tcl_AppendResult(interp, "none", (char *) NULL);
		break;
	    case FILLX:
		Tcl_AppendResult(interp, "x", (char *) NULL);
		break;
	    case FILLY:
		Tcl_AppendResult(interp, "y", (char *) NULL);
		break;
	    case FILLX|FILLY:
		Tcl_AppendResult(interp, "both", (char *) NULL);
		break;
	}
	sprintf(buffer, " -ipadx %d -ipady %d -padx %d -pady %d",
		slavePtr->iPadX/2, slavePtr->iPadY/2, slavePtr->padX/2,
		slavePtr->padY/2);
	Tcl_AppendResult(interp, buffer, " -side ", sideNames[slavePtr->side],
		(char *) NULL);
    } else if ((c == 'p') && (strncmp(argv[1], "propagate", length) == 0)) {
	CkWindow *master;
	Packer *masterPtr;
	int propagate;

	if (argc > 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " propagate window ?boolean?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	master = Ck_NameToWindow(interp, argv[2], mainPtr);
	if (master == NULL) {
	    return TCL_ERROR;
	}
	masterPtr = GetPacker(master);
	if (argc == 3) {
	    if (masterPtr->flags & DONT_PROPAGATE) {
		interp->result = "0";
	    } else {
		interp->result = "1";
	    }
	    return TCL_OK;
	}
	if (Tcl_GetBoolean(interp, argv[3], &propagate) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (propagate) {
	    masterPtr->flags &= ~DONT_PROPAGATE;

	    /*
	     * Repack the master to allow new geometry information to
	     * propagate upwards to the master's master.
	     */

	    if (masterPtr->abortPtr != NULL) {
		*masterPtr->abortPtr = 1;
	    }
	    if (!(masterPtr->flags & REQUESTED_REPACK)) {
		masterPtr->flags |= REQUESTED_REPACK;
		Tk_DoWhenIdle(ArrangePacking, (ClientData) masterPtr);
	    }
	} else {
	    masterPtr->flags |= DONT_PROPAGATE;
	}
    } else if ((c == 's') && (strncmp(argv[1], "slaves", length) == 0)) {
	CkWindow *master;
	Packer *masterPtr, *slavePtr;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " slaves window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	master = Ck_NameToWindow(interp, argv[2], mainPtr);
	if (master == NULL) {
	    return TCL_ERROR;
	}
	masterPtr = GetPacker(master);
	for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
		slavePtr = slavePtr->nextPtr) {
	    Tcl_AppendElement(interp, slavePtr->winPtr->pathName);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\":  must be configure, forget, info, ",
		"propagate, or slaves", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * PackReqProc --
 *
 *	This procedure is invoked by Ck_GeometryRequest for
 *	windows managed by the packer.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for winPtr, and all its managed siblings, to
 *	be re-packed at the next idle point.
 *
 *--------------------------------------------------------------
 */

static void
PackReqProc(clientData, winPtr)
    ClientData clientData;	/* Packer's information about
				 * window that got new preferred
				 * geometry.  */
    CkWindow *winPtr;		/* Other information about the window. */
{
    register Packer *packPtr = (Packer *) clientData;

    packPtr = packPtr->masterPtr;
    if (!(packPtr->flags & REQUESTED_REPACK)) {
	packPtr->flags |= REQUESTED_REPACK;
	Tk_DoWhenIdle(ArrangePacking, (ClientData) packPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * PackLostSlaveProc --
 *
 *	This procedure is invoked whenever some other geometry
 *	claims control over a slave that used to be managed by us.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Forgets all packer-related information about the slave.
 *
 *--------------------------------------------------------------
 */

static void
PackLostSlaveProc(clientData, winPtr)
    ClientData clientData;	/* Packer structure for slave window that
				 * was stolen away. */
    CkWindow *winPtr;		/* Pointer to window. */
{
    register Packer *slavePtr = (Packer *) clientData;

    if (slavePtr->masterPtr->winPtr != slavePtr->winPtr->parentPtr) {
	Ck_UnmaintainGeometry(slavePtr->winPtr, slavePtr->masterPtr->winPtr);
    }
    Unlink(slavePtr);
    Ck_UnmapWindow(slavePtr->winPtr);
}

/*
 *--------------------------------------------------------------
 *
 * ArrangePacking --
 *
 *	This procedure is invoked (using the Tk_DoWhenIdle
 *	mechanism) to re-layout a set of windows managed by
 *	the packer.  It is invoked at idle time so that a
 *	series of packer requests can be merged into a single
 *	layout operation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The packed slaves of masterPtr may get resized or
 *	moved.
 *
 *--------------------------------------------------------------
 */

static void
ArrangePacking(clientData)
    ClientData clientData;	/* Structure describing parent whose slaves
				 * are to be re-layed out. */
{
    register Packer *masterPtr = (Packer *) clientData;
    register Packer *slavePtr;	
    int cavityX, cavityY, cavityWidth, cavityHeight;
				/* These variables keep track of the
				 * as-yet-unallocated space remaining in
				 * the middle of the parent window. */
    int frameX, frameY, frameWidth, frameHeight;
				/* These variables keep track of the frame
				 * allocated to the current window. */
    int x, y, width, height;	/* These variables are used to hold the
				 * actual geometry of the current window. */
    int intBWidth;		/* Width of internal border in parent window,
				 * if any. */
    int abort;			/* May get set to non-zero to abort this
				 * repacking operation. */
    int borderX, borderY;
    int maxWidth, maxHeight, tmp;

    masterPtr->flags &= ~REQUESTED_REPACK;

    /*
     * If the parent has no slaves anymore, then don't do anything
     * at all:  just leave the parent's size as-is.
     */

    if (masterPtr->slavePtr == NULL) {
	return;
    }

    /*
     * Abort any nested call to ArrangePacking for this window, since
     * we'll do everything necessary here, and set up so this call
     * can be aborted if necessary.  
     */

    if (masterPtr->abortPtr != NULL) {
	*masterPtr->abortPtr = 1;
    }
    masterPtr->abortPtr = &abort;
    abort = 0;
    Ck_Preserve((ClientData) masterPtr);

    /*
     * Pass #1: scan all the slaves to figure out the total amount
     * of space needed.  Two separate width and height values are
     * computed:
     *
     * width -		Holds the sum of the widths (plus padding) of
     *			all the slaves seen so far that were packed LEFT
     *			or RIGHT.
     * height -		Holds the sum of the heights (plus padding) of
     *			all the slaves seen so far that were packed TOP
     *			or BOTTOM.
     *
     * maxWidth -	Gradually builds up the width needed by the master
     *			to just barely satisfy all the slave's needs.  For
     *			each slave, the code computes the width needed for
     *			all the slaves so far and updates maxWidth if the
     *			new value is greater.
     * maxHeight -	Same as maxWidth, except keeps height info.
     */

    intBWidth = (masterPtr->winPtr->flags & CK_BORDER) ? 1 : 0;
    width = height = maxWidth = maxHeight = 2*intBWidth;
    for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
	    slavePtr = slavePtr->nextPtr) {
	if ((slavePtr->side == TOP) || (slavePtr->side == BOTTOM)) {
	    tmp = slavePtr->winPtr->reqWidth
		    + slavePtr->padX + slavePtr->iPadX + width;
	    if (tmp > maxWidth) {
		maxWidth = tmp;
	    }
	    height += slavePtr->winPtr->reqHeight
		    + slavePtr->padY + slavePtr->iPadY;
	} else {
	    tmp = slavePtr->winPtr->reqHeight
		    + slavePtr->padY + slavePtr->iPadY + height;
	    if (tmp > maxHeight) {
		maxHeight = tmp;
	    }
	    width += slavePtr->winPtr->reqWidth
		    + slavePtr->padX + slavePtr->iPadX;
	}
    }
    if (width > maxWidth) {
	maxWidth = width;
    }
    if (height > maxHeight) {
	maxHeight = height;
    }

    /*
     * If the total amount of space needed in the parent window has
     * changed, and if we're propagating geometry information, then
     * notify the next geometry manager up and requeue ourselves to
     * start again after the parent has had a chance to
     * resize us.
     */

    if (((maxWidth != masterPtr->winPtr->reqWidth)
	    || (maxHeight != masterPtr->winPtr->reqHeight))
	    && !(masterPtr->flags & DONT_PROPAGATE)) {
	Ck_GeometryRequest(masterPtr->winPtr, maxWidth, maxHeight);
	masterPtr->flags |= REQUESTED_REPACK;
	Tk_DoWhenIdle(ArrangePacking, (ClientData) masterPtr);
	goto done;
    }

    /*
     * Pass #2: scan the slaves a second time assigning
     * new sizes.  The "cavity" variables keep track of the
     * unclaimed space in the cavity of the window;  this
     * shrinks inward as we allocate windows around the
     * edges.  The "frame" variables keep track of the space
     * allocated to the current window and its frame.  The
     * current window is then placed somewhere inside the
     * frame, depending on anchor.
     */

    cavityX = cavityY = x = y = intBWidth;
    cavityWidth = masterPtr->winPtr->width - 2*intBWidth;
    cavityHeight = masterPtr->winPtr->height - 2*intBWidth;
    for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
	    slavePtr = slavePtr->nextPtr) {
	if ((slavePtr->side == TOP) || (slavePtr->side == BOTTOM)) {
	    frameWidth = cavityWidth;
	    frameHeight = slavePtr->winPtr->reqHeight
		    + slavePtr->padY + slavePtr->iPadY;
	    if (slavePtr->flags & EXPAND) {
		frameHeight += YExpansion(slavePtr, cavityHeight);
	    }
	    cavityHeight -= frameHeight;
	    if (cavityHeight < 0) {
		frameHeight += cavityHeight;
		cavityHeight = 0;
	    }
	    frameX = cavityX;
	    if (slavePtr->side == TOP) {
		frameY = cavityY;
		cavityY += frameHeight;
	    } else {
		frameY = cavityY + cavityHeight;
	    }
	} else {
	    frameHeight = cavityHeight;
	    frameWidth = slavePtr->winPtr->reqWidth
		    + slavePtr->padX + slavePtr->iPadX;
	    if (slavePtr->flags & EXPAND) {
		frameWidth += XExpansion(slavePtr, cavityWidth);
	    }
	    cavityWidth -= frameWidth;
	    if (cavityWidth < 0) {
		frameWidth += cavityWidth;
		cavityWidth = 0;
	    }
	    frameY = cavityY;
	    if (slavePtr->side == LEFT) {
		frameX = cavityX;
		cavityX += frameWidth;
	    } else {
		frameX = cavityX + cavityWidth;
	    }
	}

	/*
	 * Now that we've got the size of the frame for the window,
	 * compute the window's actual size and location using the
	 * fill, padding, and frame factors.  The variables "borderX"
	 * and "borderY" are used to handle the differences between
	 * old-style packing and the new style (in old-style, iPadX
	 * and iPadY are always zero and padding is completely ignored
	 * except when computing frame size).
	 */

	borderX = slavePtr->padX;
	borderY = slavePtr->padY;
	width = slavePtr->winPtr->reqWidth + slavePtr->iPadX;
	if ((slavePtr->flags & FILLX)
		|| (width > (frameWidth - borderX))) {
	    width = frameWidth - borderX;
	}
	height = slavePtr->winPtr->reqHeight + slavePtr->iPadY;
	if ((slavePtr->flags & FILLY)
		|| (height > (frameHeight - borderY))) {
	    height = frameHeight - borderY;
	}
	borderX /= 2;
	borderY /= 2;
	switch (slavePtr->anchor) {
	    case CK_ANCHOR_N:
		x = frameX + (frameWidth - width)/2;
		y = frameY + borderY;
		break;
	    case CK_ANCHOR_NE:
		x = frameX + frameWidth - width - borderX;
		y = frameY + borderY;
		break;
	    case CK_ANCHOR_E:
		x = frameX + frameWidth - width - borderX;
		y = frameY + (frameHeight - height)/2;
		break;
	    case CK_ANCHOR_SE:
		x = frameX + frameWidth - width - borderX;
		y = frameY + frameHeight - height - borderY;
		break;
	    case CK_ANCHOR_S:
		x = frameX + (frameWidth - width)/2;
		y = frameY + frameHeight - height - borderY;
		break;
	    case CK_ANCHOR_SW:
		x = frameX + borderX;
		y = frameY + frameHeight - height - borderY;
		break;
	    case CK_ANCHOR_W:
		x = frameX + borderX;
		y = frameY + (frameHeight - height)/2;
		break;
	    case CK_ANCHOR_NW:
		x = frameX + borderX;
		y = frameY + borderY;
		break;
	    case CK_ANCHOR_CENTER:
		x = frameX + (frameWidth - width)/2;
		y = frameY + (frameHeight - height)/2;
		break;
	    default:
		panic("bad frame factor in ArrangePacking");
	}

	/*
	 * The final step is to set the position, size, and mapped/unmapped
	 * state of the slave.
	 */

	if (width <= 0 || height <= 0) {
	    Ck_UnmapWindow(slavePtr->winPtr);
	} else {
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
	    if (abort)
		goto done;
	    Ck_MapWindow(slavePtr->winPtr);
	}

	/*
	 * Changes to the window's structure could cause almost anything
	 * to happen, including deleting the parent or child.  If this
	 * happens, we'll be told to abort.
	 */

	if (abort) {
	    goto done;
	}
    }

done:
    masterPtr->abortPtr = NULL;
    Ck_Release((ClientData) masterPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * XExpansion --
 *
 *	Given a list of packed slaves, the first of which is packed
 *	on the left or right and is expandable, compute how much to
 *	expand the child.
 *
 * Results:
 *	The return value is the number of additional pixels to give to
 *	the child.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
XExpansion(slavePtr, cavityWidth)
    register Packer *slavePtr;		/* First in list of remaining
					 * slaves. */
    int cavityWidth;			/* Horizontal space left for all
					 * remaining slaves. */
{
    int numExpand, minExpand, curExpand;
    int childWidth;

    /*
     * This procedure is tricky because windows packed top or bottom can
     * be interspersed among expandable windows packed left or right.
     * Scan through the list, keeping a running sum of the widths of
     * all left and right windows (actually, count the cavity space not
     * allocated) and a running count of all expandable left and right
     * windows.  At each top or bottom window, and at the end of the
     * list, compute the expansion factor that seems reasonable at that
     * point.  Return the smallest factor seen at any of these points.
     */

    minExpand = cavityWidth;
    numExpand = 0;
    for ( ; slavePtr != NULL; slavePtr = slavePtr->nextPtr) {
	childWidth = slavePtr->winPtr->reqWidth
		+ slavePtr->padX + slavePtr->iPadX;
	if ((slavePtr->side == TOP) || (slavePtr->side == BOTTOM)) {
	    curExpand = (cavityWidth - childWidth)/numExpand;
	    if (curExpand < minExpand) {
		minExpand = curExpand;
	    }
	} else {
	    cavityWidth -= childWidth;
	    if (slavePtr->flags & EXPAND) {
		numExpand++;
	    }
	}
    }
    curExpand = cavityWidth/numExpand;
    if (curExpand < minExpand) {
	minExpand = curExpand;
    }
    return (minExpand < 0) ? 0 : minExpand;
}

/*
 *----------------------------------------------------------------------
 *
 * YExpansion --
 *
 *	Given a list of packed slaves, the first of which is packed
 *	on the top or bottom and is expandable, compute how much to
 *	expand the child.
 *
 * Results:
 *	The return value is the number of additional pixels to give to
 *	the child.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
YExpansion(slavePtr, cavityHeight)
    register Packer *slavePtr;		/* First in list of remaining
					 * slaves. */
    int cavityHeight;			/* Vertical space left for all
					 * remaining slaves. */
{
    int numExpand, minExpand, curExpand;
    int childHeight;

    /*
     * See comments for XExpansion.
     */

    minExpand = cavityHeight;
    numExpand = 0;
    for ( ; slavePtr != NULL; slavePtr = slavePtr->nextPtr) {
	childHeight = slavePtr->winPtr->reqHeight
		+ slavePtr->padY + slavePtr->iPadY;
	if ((slavePtr->side == LEFT) || (slavePtr->side == RIGHT)) {
	    curExpand = (cavityHeight - childHeight)/numExpand;
	    if (curExpand < minExpand) {
		minExpand = curExpand;
	    }
	} else {
	    cavityHeight -= childHeight;
	    if (slavePtr->flags & EXPAND) {
		numExpand++;
	    }
	}
    }
    curExpand = cavityHeight/numExpand;
    if (curExpand < minExpand) {
	minExpand = curExpand;
    }
    return (minExpand < 0) ? 0 : minExpand;
}

/*
 *--------------------------------------------------------------
 *
 * GetPacker --
 *
 *	This internal procedure is used to locate a Packer
 *	structure for a given window, creating one if one
 *	doesn't exist already.
 *
 * Results:
 *	The return value is a pointer to the Packer structure
 *	corresponding to tkwin.
 *
 * Side effects:
 *	A new packer structure may be created.  If so, then
 *	a callback is set up to clean things up when the
 *	window is deleted.
 *
 *--------------------------------------------------------------
 */

static Packer *
GetPacker(winPtr)
    CkWindow *winPtr;		/* Pointer to window for which
				 * packer structure is desired. */
{
    register Packer *packPtr;
    Tcl_HashEntry *hPtr;
    int new;

    if (!initialized) {
	initialized = 1;
	Tcl_InitHashTable(&packerHashTable, TCL_ONE_WORD_KEYS);
    }

    /*
     * See if there's already packer for this window.  If not,
     * then create a new one.
     */

    hPtr = Tcl_CreateHashEntry(&packerHashTable, (char *) winPtr, &new);
    if (!new) {
	return (Packer *) Tcl_GetHashValue(hPtr);
    }
    packPtr = (Packer *) ckalloc(sizeof (Packer));
    packPtr->winPtr = winPtr;
    packPtr->masterPtr = NULL;
    packPtr->nextPtr = NULL;
    packPtr->slavePtr = NULL;
    packPtr->side = TOP;
    packPtr->anchor = CK_ANCHOR_CENTER;
    packPtr->padX = packPtr->padY = 0;
    packPtr->iPadX = packPtr->iPadY = 0;
    packPtr->abortPtr = NULL;
    packPtr->flags = 0;
    Tcl_SetHashValue(hPtr, packPtr);
    Ck_CreateEventHandler(winPtr,
    	CK_EV_DESTROY | CK_EV_MAP | CK_EV_EXPOSE,
	PackStructureProc, (ClientData) packPtr);
    return packPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Unlink --
 *
 *	Remove a packer from its parent's list of slaves.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The parent will be scheduled for repacking.
 *
 *----------------------------------------------------------------------
 */

static void
Unlink(packPtr)
    register Packer *packPtr;		/* Window to unlink. */
{
    register Packer *masterPtr, *packPtr2;

    masterPtr = packPtr->masterPtr;
    if (masterPtr == NULL) {
	return;
    }
    if (masterPtr->slavePtr == packPtr) {
	masterPtr->slavePtr = packPtr->nextPtr;
    } else {
	for (packPtr2 = masterPtr->slavePtr; ; packPtr2 = packPtr2->nextPtr) {
	    if (packPtr2 == NULL) {
		panic("Unlink couldn't find previous window");
	    }
	    if (packPtr2->nextPtr == packPtr) {
		packPtr2->nextPtr = packPtr->nextPtr;
		break;
	    }
	}
    }
    if (!(masterPtr->flags & REQUESTED_REPACK)) {
	masterPtr->flags |= REQUESTED_REPACK;
	Tk_DoWhenIdle(ArrangePacking, (ClientData) masterPtr);
    }
    if (masterPtr->abortPtr != NULL) {
	*masterPtr->abortPtr = 1;
    }

    packPtr->masterPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyPacker --
 *
 *	This procedure is invoked by Ck_EventuallyFree or Ck_Release
 *	to clean up the internal structure of a packer at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the packer is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyPacker(clientData)
    ClientData clientData;		/* Info about packed window that
					 * is now dead. */
{
    register Packer *packPtr = (Packer *) clientData;
    ckfree((char *) packPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * PackStructureProc --
 *
 *	This procedure is invoked by the event dispatcher in response
 *	to CK_EV_MAP/CK_EV_EXPOSE/CK_EV_DESTROY events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If a window was just deleted, clean up all its packer-related
 *	information.  If it was just resized, repack its slaves, if
 *	any.
 *
 *----------------------------------------------------------------------
 */

static void
PackStructureProc(clientData, eventPtr)
    ClientData clientData;		/* Our information about window
					 * referred to by eventPtr. */
    CkEvent *eventPtr;			/* Describes what just happened. */
{
    register Packer *packPtr = (Packer *) clientData;

    if (eventPtr->type == CK_EV_MAP || eventPtr->type == CK_EV_EXPOSE) {
	if ((packPtr->slavePtr != NULL)
		&& !(packPtr->flags & REQUESTED_REPACK)) {
	    packPtr->flags |= REQUESTED_REPACK;
	    Tk_DoWhenIdle(ArrangePacking, (ClientData) packPtr);
	}
    } else if (eventPtr->type == CK_EV_DESTROY) {
	register Packer *slavePtr, *nextPtr;

	if (packPtr->masterPtr != NULL) {
	    Unlink(packPtr);
	}
	for (slavePtr = packPtr->slavePtr; slavePtr != NULL;
		slavePtr = nextPtr) {
	    Ck_ManageGeometry(slavePtr->winPtr, (Ck_GeomMgr *) NULL,
		    (ClientData) NULL);
	    Ck_UnmapWindow(slavePtr->winPtr);
	    slavePtr->masterPtr = NULL;
	    nextPtr = slavePtr->nextPtr;
	    slavePtr->nextPtr = NULL;
	}
	Tcl_DeleteHashEntry(Tcl_FindHashEntry(&packerHashTable,
	    (char *) packPtr->winPtr));
	if (packPtr->flags & REQUESTED_REPACK) {
	    Tk_CancelIdleCall(ArrangePacking, (ClientData) packPtr);
	}
	packPtr->winPtr = NULL;
	Ck_EventuallyFree((ClientData) packPtr, (Ck_FreeProc *) DestroyPacker);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureSlaves --
 *
 *	This implements the guts of the "pack configure" command.  Given
 *	a list of slaves and configuration options, it arranges for the
 *	packer to manage the slaves and sets the specified options.
 *
 * Results:
 *	TCL_OK is returned if all went well.  Otherwise, TCL_ERROR is
 *	returned and interp->result is set to contain an error message.
 *
 * Side effects:
 *	Slave windows get taken over by the packer.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureSlaves(interp, winPtr, argc, argv)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    CkWindow *winPtr;		/* Any window in application containing
				 * slaves.  Used to look up slave names. */
    int argc;			/* Number of elements in argv. */
    char *argv[];		/* Argument strings:  contains one or more
				 * window names followed by any number
				 * of "option value" pairs.  Caller must
				 * make sure that there is at least one
				 * window name. */
{
    Packer *masterPtr, *slavePtr, *prevPtr, *otherPtr;
    CkWindow *other, *slave, *parent;
    int i, j, numWindows, c, tmp, positionGiven;
    size_t length;

    /*
     * Find out how many windows are specified.
     */

    for (numWindows = 0; numWindows < argc; numWindows++) {
	if (argv[numWindows][0] != '.') {
	    break;
	}
    }

    /*
     * Iterate over all of the slave windows, parsing the configuration
     * options for each slave.  It's a bit wasteful to re-parse the
     * options for each slave, but things get too messy if we try to
     * parse the arguments just once at the beginning.  For example,
     * if a slave already is packed we want to just change a few
     * existing values without resetting everything.  If there are
     * multiple windows, the -after, -before, and -in options only
     * get processed for the first window.
     */

    masterPtr = NULL;
    prevPtr = NULL;
    positionGiven = 0;
    for (j = 0; j < numWindows; j++) {
	slave = Ck_NameToWindow(interp, argv[j], winPtr);
	if (slave == NULL) {
	    return TCL_ERROR;
	}
	if (slave->flags & CK_TOPLEVEL) {
	    Tcl_AppendResult(interp, "can't pack \"", argv[j],
		    "\": it's a top-level window", (char *) NULL);
	    return TCL_ERROR;
	}
	slavePtr = GetPacker(slave);

	/*
	 * If the slave isn't currently packed, reset all of its
	 * configuration information to default values (there could
	 * be old values left from a previous packing).
	 */

	if (slavePtr->masterPtr == NULL) {
	    slavePtr->side = TOP;
	    slavePtr->anchor = CK_ANCHOR_CENTER;
	    slavePtr->padX = slavePtr->padY = 0;
	    slavePtr->iPadX = slavePtr->iPadY = 0;
	    slavePtr->flags &= ~(FILLX|FILLY|EXPAND);
	}

	for (i = numWindows; i < argc; i+=2) {
	    if ((i+2) > argc) {
		Tcl_AppendResult(interp, "extra option \"", argv[i],
			"\" (option with no value?)", (char *) NULL);
		return TCL_ERROR;
	    }
	    length = strlen(argv[i]);
	    if (length < 2) {
		goto badOption;
	    }
	    c = argv[i][1];
	    if ((c == 'a') && (strncmp(argv[i], "-after", length) == 0)
		    && (length >= 2)) {
		if (j == 0) {
		    other = Ck_NameToWindow(interp, argv[i+1], winPtr);
		    if (other == NULL) {
			return TCL_ERROR;
		    }
		    prevPtr = GetPacker(other);
		    if (prevPtr->masterPtr == NULL) {
			notPacked:
			Tcl_AppendResult(interp, "window \"", argv[i+1],
				"\" isn't packed", (char *) NULL);
			return TCL_ERROR;
		    }
		    masterPtr = prevPtr->masterPtr;
		    positionGiven = 1;
		}
	    } else if ((c == 'a') && (strncmp(argv[i], "-anchor", length) == 0)
		    && (length >= 2)) {
		if (Ck_GetAnchor(interp, argv[i+1], &slavePtr->anchor)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
	    } else if ((c == 'b')
		    && (strncmp(argv[i], "-before", length) == 0)) {
		if (j == 0) {
		    other = Ck_NameToWindow(interp, argv[i+1], winPtr);
		    if (other == NULL) {
			return TCL_ERROR;
		    }
		    otherPtr = GetPacker(other);
		    if (otherPtr->masterPtr == NULL) {
			goto notPacked;
		    }
		    masterPtr = otherPtr->masterPtr;
		    prevPtr = masterPtr->slavePtr;
		    if (prevPtr == otherPtr) {
			prevPtr = NULL;
		    } else {
			while (prevPtr->nextPtr != otherPtr) {
			    prevPtr = prevPtr->nextPtr;
			}
		    }
		    positionGiven = 1;
		}
	    } else if ((c == 'e')
		    && (strncmp(argv[i], "-expand", length) == 0)) {
		if (Tcl_GetBoolean(interp, argv[i+1], &tmp) != TCL_OK) {
		    return TCL_ERROR;
		}
		slavePtr->flags &= ~EXPAND;
		if (tmp) {
		    slavePtr->flags |= EXPAND;
		}
	    } else if ((c == 'f') && (strncmp(argv[i], "-fill", length) == 0)) {
		if (strcmp(argv[i+1], "none") == 0) {
		    slavePtr->flags &= ~(FILLX|FILLY);
		} else if (strcmp(argv[i+1], "x") == 0) {
		    slavePtr->flags = (slavePtr->flags & ~FILLY) | FILLX;
		} else if (strcmp(argv[i+1], "y") == 0) {
		    slavePtr->flags = (slavePtr->flags & ~FILLX) | FILLY;
		} else if (strcmp(argv[i+1], "both") == 0) {
		    slavePtr->flags |= FILLX|FILLY;
		} else {
		    Tcl_AppendResult(interp, "bad fill style \"", argv[i+1],
			    "\": must be none, x, y, or both", (char *) NULL);
		    return TCL_ERROR;
		}
	    } else if ((c == 'i') && (strcmp(argv[i], "-ipadx") == 0)) {
		if ((Ck_GetCoord(interp, slave, argv[i+1], &tmp) != TCL_OK)
			|| (tmp < 0)) {
		    badPad:
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, "bad pad value \"", argv[i+1],
			    "\": must be positive screen distance",
			    (char *) NULL);
		    return TCL_ERROR;
		}
		slavePtr->iPadX = tmp*2;
	    } else if ((c == 'i') && (strcmp(argv[i], "-ipady") == 0)) {
		if ((Ck_GetCoord(interp, slave, argv[i+1], &tmp) != TCL_OK)
			|| (tmp< 0)) {
		    goto badPad;
		}
		slavePtr->iPadY = tmp*2;
	    } else if ((c == 'p') && (strcmp(argv[i], "-padx") == 0)) {
		if ((Ck_GetCoord(interp, slave, argv[i+1], &tmp) != TCL_OK)
			|| (tmp< 0)) {
		    goto badPad;
		}
		slavePtr->padX = tmp*2;
	    } else if ((c == 'p') && (strcmp(argv[i], "-pady") == 0)) {
		if ((Ck_GetCoord(interp, slave, argv[i+1], &tmp) != TCL_OK)
			|| (tmp< 0)) {
		    goto badPad;
		}
		slavePtr->padY = tmp*2;
	    } else if ((c == 's') && (strncmp(argv[i], "-side", length) == 0)) {
		c = argv[i+1][0];
		if ((c == 't') && (strcmp(argv[i+1], "top") == 0)) {
		    slavePtr->side = TOP;
		} else if ((c == 'b') && (strcmp(argv[i+1], "bottom") == 0)) {
		    slavePtr->side = BOTTOM;
		} else if ((c == 'l') && (strcmp(argv[i+1], "left") == 0)) {
		    slavePtr->side = LEFT;
		} else if ((c == 'r') && (strcmp(argv[i+1], "right") == 0)) {
		    slavePtr->side = RIGHT;
		} else {
		    Tcl_AppendResult(interp, "bad side \"", argv[i+1],
			    "\": must be top, bottom, left, or right",
			    (char *) NULL);
		    return TCL_ERROR;
		}
	    } else {
		badOption:
		Tcl_AppendResult(interp, "unknown or ambiguous option \"",
			argv[i], "\": must be -after, -anchor, -before, ",
			"-expand, -fill, -ipadx, -ipady, -padx, ",
			"-pady, or -side", (char *) NULL);
		return TCL_ERROR;
	    }
	}

	/*
	 * If no position in a packing list was specified and the slave
	 * is already packed, then leave it in its current location in
	 * its current packing list.
	 */

	if (!positionGiven && (slavePtr->masterPtr != NULL)) {
	    masterPtr = slavePtr->masterPtr;
	    goto scheduleLayout;
	}

	/*
	 * If the slave is going to be put back after itself then
	 * skip the whole operation, since it won't work anyway.
	 */

	if (prevPtr == slavePtr) {
	    masterPtr = slavePtr->masterPtr;
	    goto scheduleLayout;
	}
    
	/*
	 * If none of the "-before", or "-after" options has
	 * been specified, arrange for the slave to go at the end of
	 * the order for its parent.
	 */
    
	if (!positionGiven) {
	    masterPtr = GetPacker(slave->parentPtr);
	    prevPtr = masterPtr->slavePtr;
	    if (prevPtr != NULL) {
		while (prevPtr->nextPtr != NULL) {
		    prevPtr = prevPtr->nextPtr;
		}
	    }
	}

	/*
	 * Make sure that the slave's parent is either the master or
	 * an ancestor of the master.
	 */
    
	parent = slave->parentPtr;
	if (masterPtr->winPtr != slave->parentPtr) {
	    Tcl_AppendResult(interp, "can't pack ", argv[j],
		" inside ", masterPtr->winPtr->pathName,
		(char *) NULL);
	    return TCL_ERROR;
	}

	/*
	 * Unpack the slave if it's currently packed, then position it
	 * after prevPtr.
	 */

	if (slavePtr->masterPtr != NULL) {
	    Unlink(slavePtr);
	}
	slavePtr->masterPtr = masterPtr;
	if (prevPtr == NULL) {
	    slavePtr->nextPtr = masterPtr->slavePtr;
	    masterPtr->slavePtr = slavePtr;
	} else {
	    slavePtr->nextPtr = prevPtr->nextPtr;
	    prevPtr->nextPtr = slavePtr;
	}
	Ck_ManageGeometry(slave, &packerType, (ClientData) slavePtr);
	prevPtr = slavePtr;

	/*
	 * Arrange for the parent to be re-packed at the first
	 * idle moment.
	 */

	scheduleLayout:
	if (masterPtr->abortPtr != NULL) {
	    *masterPtr->abortPtr = 1;
	}
	if (!(masterPtr->flags & REQUESTED_REPACK)) {
	    masterPtr->flags |= REQUESTED_REPACK;
	    Tk_DoWhenIdle(ArrangePacking, (ClientData) masterPtr);
	}
    }
    return TCL_OK;
}
