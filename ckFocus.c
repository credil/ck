/* 
 * ckFocus.c --
 *
 *	This file contains procedures that manage the input focus.
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
 *--------------------------------------------------------------
 *
 * Ck_FocusCmd --
 *
 *	This procedure is invoked to process the "focus" Tcl command.
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
Ck_FocusCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *winPtr = (CkWindow *) clientData;
    CkWindow *newPtr, *focusWinPtr;

    /*
     * If invoked with no arguments, just return the current focus window.
     */

    if (argc == 1) {
	focusWinPtr = winPtr->mainPtr->focusPtr;
	if (focusWinPtr != NULL)
	    interp->result = focusWinPtr->pathName;
	return TCL_OK;
    }

    /*
     * If invoked with a single argument beginning with "." then focus
     * on that window.
     */

    if (argc == 2) {
	if (argv[1][0] == 0) {
	    return TCL_OK;
	}
	if (argv[1][0] == '.') {
	    newPtr = (CkWindow *) Ck_NameToWindow(interp, argv[1], winPtr);
	    if (newPtr == NULL)
		return TCL_ERROR;
	    if (!(newPtr->flags & CK_ALREADY_DEAD))
		Ck_SetFocus(newPtr);
	    return TCL_OK;
	}
    }

    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	" ?pathName?\"", (char *) NULL);
    return TCL_ERROR;
}
