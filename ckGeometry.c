/* 
 * ckGeometry.c --
 *
 *	This file contains generic code for geometry management
 *	(stuff that's used by all geometry managers).
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

/*
 * Data structures of the following type are used by Tk_MaintainGeometry.
 * For each slave managed by Tk_MaintainGeometry, there is one of these
 * structures associated with its master.
 */

typedef struct MaintainSlave {
    CkWindow *slave;		/* The slave window being positioned. */
    CkWindow *master;		/* The master that determines slave's
				 * position; it must be a descendant of
				 * slave's parent. */
    int x, y;			/* Desired position of slave relative to
				 * master. */
    int width, height;		/* Desired dimensions of slave. */
    struct MaintainSlave *nextPtr;
				/* Next in list of Maintains associated
				 * with master. */
} MaintainSlave;

/*
 * For each window that has been specified as a master to
 * Tk_MaintainGeometry, there is a structure of the following type:
 */

typedef struct MaintainMaster {
    CkWindow *ancestor;		/* The lowest ancestor of this window
				 * for which we have *not* created a
				 * StructureNotify handler.  May be the
				 * same as the window itself. */
    int checkScheduled;		/* Non-zero means that there is already a
				 * call to MaintainCheckProc scheduled as
				 * an idle handler. */
    MaintainSlave *slavePtr;	/* First in list of all slaves associated
				 * with this master. */
} MaintainMaster;

/*
 * Hash table that maps from a master's CkWindow pointer to a list of
 * Maintains for that master:
 */

static Tcl_HashTable maintainHashTable;

/*
 * Has maintainHashTable been initialized yet?
 */

static int initialized = 0;

/*
 * Prototypes for static procedures in this file:
 */

static void		MaintainCheckProc _ANSI_ARGS_((ClientData clientData));
static void		MaintainMasterProc _ANSI_ARGS_((ClientData clientData,
			    CkEvent *eventPtr));
static void		MaintainSlaveProc _ANSI_ARGS_((ClientData clientData,
			    CkEvent *eventPtr));

/*
 *--------------------------------------------------------------
 *
 * Ck_ManageGeometry --
 *
 *	Arrange for a particular procedure to manage the geometry
 *	of a given slave window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Proc becomes the new geometry manager for tkwin, replacing
 *	any previous geometry manager.  The geometry manager will
 *	be notified (by calling procedures in *mgrPtr) when interesting
 *	things happen in the future.  If there was an existing geometry
 *	manager for tkwin different from the new one, it is notified
 *	by calling its lostSlaveProc.
 *
 *--------------------------------------------------------------
 */

void
Ck_ManageGeometry(winPtr, mgrPtr, clientData)
    CkWindow *winPtr;		/* Window whose geometry is to
				 * be managed by proc.  */
    Ck_GeomMgr *mgrPtr;		/* Static structure describing the
				 * geometry manager.  This structure
				 * must never go away. */
    ClientData clientData;	/* Arbitrary one-word argument to
				 * pass to geometry manager procedures. */
{
    if ((winPtr->geomMgrPtr != NULL) && (mgrPtr != NULL)
	    && ((winPtr->geomMgrPtr != mgrPtr)
		|| (winPtr->geomData != clientData))
	    && (winPtr->geomMgrPtr->lostSlaveProc != NULL)) {
	(*winPtr->geomMgrPtr->lostSlaveProc)(winPtr->geomData, winPtr);
    }

    winPtr->geomMgrPtr = mgrPtr;
    winPtr->geomData = clientData;
}

/*
 *--------------------------------------------------------------
 *
 * Ck_GeometryRequest --
 *
 *	This procedure is invoked by widget code to indicate
 *	its preferences about the size of a window it manages.
 *	In general, widget code should call this procedure
 *	rather than Ck_ResizeWindow.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The geometry manager for winPtr (if any) is invoked to
 *	handle the request.  If possible, it will reconfigure
 *	winPtr and/or other windows to satisfy the request.  The
 *	caller gets no indication of success or failure, but it
 *	will get events if the window size was actually
 *	changed.
 *
 *--------------------------------------------------------------
 */

void
Ck_GeometryRequest(winPtr, reqWidth, reqHeight)
    CkWindow *winPtr;		/* Window that geometry information
				 * pertains to. */
    int reqWidth, reqHeight;	/* Minimum desired dimensions for
				 * window, in pixels. */
{
    if (reqWidth <= 0) {
	reqWidth = 1;
    }
    if (reqHeight <= 0) {
	reqHeight = 1;
    }
    if ((reqWidth == winPtr->reqWidth) && (reqHeight == winPtr->reqHeight)) {
	return;
    }
    winPtr->reqWidth = reqWidth;
    winPtr->reqHeight = reqHeight;
    if ((winPtr->geomMgrPtr != NULL)
	    && (winPtr->geomMgrPtr->requestProc != NULL)) {
	(*winPtr->geomMgrPtr->requestProc)(winPtr->geomData, winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_SetInternalBorder --
 *
 *	Notify relevant geometry managers that a window has an internal
 *	border of zero or one character cells and that child windows
 *	should not be placed on that border.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The border is recorded for the window, and all geometry
 *	managers of all children are notified so that can re-layout, if
 *	necessary.
 *
 *----------------------------------------------------------------------
 */

void
Ck_SetInternalBorder(winPtr, onoff)
    CkWindow *winPtr;		/* Window to modify. */
    int onoff;			/* Border flag. */
{
    if ((onoff && (winPtr->flags & CK_BORDER)) ||
        (!onoff && !(winPtr->flags & CK_BORDER)))
	return;
    if (onoff)
	winPtr->flags |= CK_BORDER;
    else
    	winPtr->flags &= ~CK_BORDER;
    for (winPtr = winPtr->childList; winPtr != NULL;
	    winPtr = winPtr->nextPtr) {
	if (winPtr->geomMgrPtr != NULL) {
	    (*winPtr->geomMgrPtr->requestProc)(winPtr->geomData, winPtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_MaintainGeometry --
 *
 *	This procedure is invoked by geometry managers to handle slaves
 *	whose master's are not their parents.  It translates the desired
 *	geometry for the slave into the coordinate system of the parent
 *	and respositions the slave if it isn't already at the right place.
 *	Furthermore, it sets up event handlers so that if the master (or
 *	any of its ancestors up to the slave's parent) is mapped, unmapped,
 *	or moved, then the slave will be adjusted to match.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Event handlers are created and state is allocated to keep track
 *	of slave.  Note:  if slave was already managed for master by
 *	Tk_MaintainGeometry, then the previous information is replaced
 *	with the new information.  The caller must eventually call
 *	Tk_UnmaintainGeometry to eliminate the correspondence (or, the
 *	state is automatically freed when either window is destroyed).
 *
 *----------------------------------------------------------------------
 */

void
Ck_MaintainGeometry(slave, master, x, y, width, height)
    CkWindow *slave;		/* Slave for geometry management. */
    CkWindow *master;		/* Master for slave; must be a descendant
				 * of slave's parent. */
    int x, y;			/* Desired position of slave within master. */
    int width, height;		/* Desired dimensions for slave. */
{
    Tcl_HashEntry *hPtr;
    MaintainMaster *masterPtr;
    register MaintainSlave *slavePtr;
    int new, map;
    CkWindow *ancestor, *parent;

    if (!initialized) {
	initialized = 1;
	Tcl_InitHashTable(&maintainHashTable, TCL_ONE_WORD_KEYS);
    }

    /*
     * See if there is already a MaintainMaster structure for the master;
     * if not, then create one.
     */

    parent = slave->parentPtr;
    hPtr = Tcl_CreateHashEntry(&maintainHashTable, (char *) master, &new);
    if (!new) {
	masterPtr = (MaintainMaster *) Tcl_GetHashValue(hPtr);
    } else {
	masterPtr = (MaintainMaster *) ckalloc(sizeof(MaintainMaster));
	masterPtr->ancestor = master;
	masterPtr->checkScheduled = 0;
	masterPtr->slavePtr = NULL;
	Tcl_SetHashValue(hPtr, masterPtr);
    }

    /*
     * Create a MaintainSlave structure for the slave if there isn't
     * already one.
     */

    for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
	    slavePtr = slavePtr->nextPtr) {
	if (slavePtr->slave == slave) {
	    goto gotSlave;
	}
    }
    slavePtr = (MaintainSlave *) ckalloc(sizeof(MaintainSlave));
    slavePtr->slave = slave;
    slavePtr->master = master;
    slavePtr->nextPtr = masterPtr->slavePtr;
    masterPtr->slavePtr = slavePtr;
    Ck_CreateEventHandler(slave,
    	CK_EV_MAP | CK_EV_UNMAP | CK_EV_EXPOSE | CK_EV_DESTROY,
    	MaintainSlaveProc, (ClientData) slavePtr);

    /*
     * Make sure that there are event handlers registered for all
     * the windows between master and slave's parent (including master
     * but not slave's parent).  There may already be handlers for master
     * and some of its ancestors (masterPtr->ancestor tells how many).
     */

    for (ancestor = master; ancestor != parent;
	    ancestor = ancestor->parentPtr) {
	if (ancestor == masterPtr->ancestor) {
	    Ck_CreateEventHandler(ancestor,
	    	CK_EV_MAP | CK_EV_UNMAP | CK_EV_EXPOSE | CK_EV_DESTROY,
		MaintainMasterProc, (ClientData) masterPtr);
	    masterPtr->ancestor = ancestor->parentPtr;
	}
    }

    /*
     * Fill in up-to-date information in the structure, then update the
     * window if it's not currently in the right place or state.
     */

    gotSlave:
    slavePtr->x = x;
    slavePtr->y = y;
    slavePtr->width = width;
    slavePtr->height = height;
    map = 1;
    for (ancestor = slavePtr->master; ; ancestor = ancestor->parentPtr) {
	if (!(ancestor->flags & CK_MAPPED) && (ancestor != parent)) {
	    map = 0;
	}
	if (ancestor == parent) {
	    if ((x != slavePtr->slave->x)
		    || (y != slavePtr->slave->y)
		    || (width != slavePtr->slave->width)
		    || (height != slavePtr->slave->height)) {
		Ck_MoveWindow(slavePtr->slave, x, y);
		Ck_ResizeWindow(slavePtr->slave, width, height);
	    }
	    if (map) {
		Ck_MapWindow(slavePtr->slave);
	    } else {
		Ck_UnmapWindow(slavePtr->slave);
	    }
	    break;
	}
	x += ancestor->x;
	y += ancestor->y;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_UnmaintainGeometry --
 *
 *	This procedure cancels a previous Ck_MaintainGeometry call,
 *	so that the relationship between slave and master is no longer
 *	maintained.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The slave is unmapped and state is released, so that slave won't
 *	track master any more.  If we weren't previously managing slave
 *	relative to master, then this procedure has no effect.
 *
 *----------------------------------------------------------------------
 */

void
Ck_UnmaintainGeometry(slave, master)
    CkWindow *slave;		/* Slave for geometry management. */
    CkWindow *master;		/* Master for slave; must be a descendant
				 * of slave's parent. */
{
    Tcl_HashEntry *hPtr;
    MaintainMaster *masterPtr;
    register MaintainSlave *slavePtr, *prevPtr;
    CkWindow *ancestor;

    if (!initialized) {
	initialized = 1;
	Tcl_InitHashTable(&maintainHashTable, TCL_ONE_WORD_KEYS);
    }

    if (!(slave->flags & CK_ALREADY_DEAD)) {
	Ck_UnmapWindow(slave);
    }
    hPtr = Tcl_FindHashEntry(&maintainHashTable, (char *) master);
    if (hPtr == NULL) {
	return;
    }
    masterPtr = (MaintainMaster *) Tcl_GetHashValue(hPtr);
    slavePtr = masterPtr->slavePtr;
    if (slavePtr->slave == slave) {
	masterPtr->slavePtr = slavePtr->nextPtr;
    } else {
	for (prevPtr = slavePtr, slavePtr = slavePtr->nextPtr; ;
		prevPtr = slavePtr, slavePtr = slavePtr->nextPtr) {
	    if (slavePtr == NULL) {
		return;
	    }
	    if (slavePtr->slave == slave) {
		prevPtr->nextPtr = slavePtr->nextPtr;
		break;
	    }
	}
    }
    Ck_DeleteEventHandler(slavePtr->slave,
    	CK_EV_MAP | CK_EV_UNMAP | CK_EV_EXPOSE | CK_EV_DESTROY,
	MaintainSlaveProc, (ClientData) slavePtr);
    ckfree((char *) slavePtr);
    if (masterPtr->slavePtr == NULL) {
	if (masterPtr->ancestor != NULL) {
	    for (ancestor = master; ; ancestor = ancestor->parentPtr) {
		Ck_DeleteEventHandler(ancestor,
		    CK_EV_MAP | CK_EV_UNMAP | CK_EV_EXPOSE | CK_EV_DESTROY,
		    MaintainMasterProc, (ClientData) masterPtr);
		if (ancestor == masterPtr->ancestor) {
		    break;
		}
	    }
	}
	if (masterPtr->checkScheduled) {
	    Tk_CancelIdleCall(MaintainCheckProc, (ClientData) masterPtr);
	}
	Tcl_DeleteHashEntry(hPtr);
	ckfree((char *) masterPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MaintainMasterProc --
 *
 *	This procedure is invoked by the event dispatcher in
 *	response to StructureNotify events on the master or one
 *	of its ancestors, on behalf of Ck_MaintainGeometry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	It schedules a call to MaintainCheckProc, which will eventually
 *	caused the postions and mapped states to be recalculated for all
 *	the maintained slaves of the master.  Or, if the master window is
 *	being deleted then state is cleaned up.
 *
 *----------------------------------------------------------------------
 */

static void
MaintainMasterProc(clientData, eventPtr)
    ClientData clientData;		/* Pointer to MaintainMaster structure
					 * for the master window. */
    CkEvent *eventPtr;			/* Describes what just happened. */
{
    MaintainMaster *masterPtr = (MaintainMaster *) clientData;
    MaintainSlave *slavePtr;
    int done;

    if ((eventPtr->type == CK_EV_EXPOSE)
	    || (eventPtr->type == CK_EV_MAP)
	    || (eventPtr->type == CK_EV_UNMAP)) {
	if (!masterPtr->checkScheduled) {
	    masterPtr->checkScheduled = 1;
	    Tk_DoWhenIdle(MaintainCheckProc, (ClientData) masterPtr);
	}
    } else if (eventPtr->type == CK_EV_DESTROY) {
	/*
	 * Delete all of the state associated with this master, but
	 * be careful not to use masterPtr after the last slave is
	 * deleted, since its memory will have been freed.
	 */

	done = 0;
	do {
	    slavePtr = masterPtr->slavePtr;
	    if (slavePtr->nextPtr == NULL) {
		done = 1;
	    }
	    Ck_UnmaintainGeometry(slavePtr->slave, slavePtr->master);
	} while (!done);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MaintainSlaveProc --
 *
 *	This procedure is invoked by the Tk event dispatcher in
 *	response to StructureNotify events on a slave being managed
 *	by Tk_MaintainGeometry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the event is a DestroyNotify event then the Maintain state
 *	and event handlers for this slave are deleted.
 *
 *----------------------------------------------------------------------
 */

static void
MaintainSlaveProc(clientData, eventPtr)
    ClientData clientData;		/* Pointer to MaintainSlave structure
					 * for master-slave pair. */
    CkEvent *eventPtr;			/* Describes what just happened. */
{
    MaintainSlave *slavePtr = (MaintainSlave *) clientData;

    if (eventPtr->type == CK_EV_DESTROY) {
	Ck_UnmaintainGeometry(slavePtr->slave, slavePtr->master);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MaintainCheckProc --
 *
 *	This procedure is invoked by the Tk event dispatcher as an
 *	idle handler, when a master or one of its ancestors has been
 *	reconfigured, mapped, or unmapped.  Its job is to scan all of
 *	the slaves for the master and reposition them, map them, or
 *	unmap them as needed to maintain their geometry relative to
 *	the master.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Slaves can get repositioned, mapped, or unmapped.
 *
 *----------------------------------------------------------------------
 */

static void
MaintainCheckProc(clientData)
    ClientData clientData;		/* Pointer to MaintainMaster structure
					 * for the master window. */
{
    MaintainMaster *masterPtr = (MaintainMaster *) clientData;
    MaintainSlave *slavePtr;
    CkWindow *ancestor, *parent;
    int x, y, map;

    masterPtr->checkScheduled = 0;
    for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
	    slavePtr = slavePtr->nextPtr) {
	parent = slavePtr->slave->parentPtr;
	x = slavePtr->x;
	y = slavePtr->y;
	map = 1;
	for (ancestor = slavePtr->master; ; ancestor = ancestor->parentPtr) {
	    if (!(ancestor->flags & CK_MAPPED) && (ancestor != parent)) {
		map = 0;
	    }
	    if (ancestor == parent) {
		if ((x != slavePtr->slave->x)
			|| (y != slavePtr->slave->y)) {
		    Ck_MoveWindow(slavePtr->slave, x, y);
		}
		if (map) {
		    Ck_MapWindow(slavePtr->slave);
		} else {
		    Ck_UnmapWindow(slavePtr->slave);
		}
		break;
	    }
	    x += ancestor->x;
	    y += ancestor->y;
	}
    }
}
