/* 
 * ckCmds.c --
 *
 *	This file contains a collection of Ck-related Tcl commands
 *	that didn't fit in any particular file of the toolkit.
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

static char *     WaitVariableProc _ANSI_ARGS_((ClientData clientData,
		      Tcl_Interp *interp, char *name1, char *name2,
                      int flags));
static void       WaitVisibilityProc _ANSI_ARGS_((ClientData clientData,
                      CkEvent *eventPtr));
static void       WaitWindowProc _ANSI_ARGS_((ClientData clientData,
                      CkEvent *eventPtr));


/*
 *----------------------------------------------------------------------
 *
 * Ck_DestroyCmd --
 *
 *	This procedure is invoked to process the "destroy" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_DestroyCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *winPtr;
    CkWindow *mainPtr = (CkWindow *) clientData;
    int i;

    for (i = 1; i < argc; i++) {
	winPtr = Ck_NameToWindow(interp, argv[i], mainPtr);
	if (winPtr == NULL)
	    return TCL_ERROR;
	Ck_DestroyWindow(winPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_ExitCmd --
 *
 *	This procedure is invoked to process the "exit" Tcl command.
 *	See the user documentation for details on what it does.
 *	Note: this command replaces the Tcl "exit" command in order
 *	to properly destroy all windows.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_ExitCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    extern CkMainInfo *ckMainInfo;
    int index = 1, noclear = 0, value = 0;

    if (argc > 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" ?-noclear? ?returnCode?\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (argc > 1 && strcmp(argv[1], "-noclear") == 0) {
	index++;
	noclear++;
    }
    if (argc > index &&
	Tcl_GetInt(interp, argv[index], &value) != TCL_OK) {
	return TCL_ERROR;
    }

    if (ckMainInfo != NULL) {
	if (noclear) {
	    ckMainInfo->flags |= CK_NOCLR_ON_EXIT;
	} else {
	    ckMainInfo->flags &= ~CK_NOCLR_ON_EXIT;
	}
	Ck_DestroyWindow((CkWindow *) clientData);
    }
    endwin();	/* just in case */
#if (TCL_MAJOR_VERSION >= 8)
    Tcl_Exit(value);
#else
    exit(value);
#endif
    /* NOTREACHED */
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_LowerCmd --
 *
 *	This procedure is invoked to process the "lower" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_LowerCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *mainPtr = (CkWindow *) clientData;
    CkWindow *winPtr, *other;

    if ((argc != 2) && (argc != 3)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " window ?belowThis?\"", (char *) NULL);
	return TCL_ERROR;
    }

    winPtr = Ck_NameToWindow(interp, argv[1], mainPtr);
    if (winPtr == NULL)
	return TCL_ERROR;
    if (argc == 2)
	other = NULL;
    else {
	other = Ck_NameToWindow(interp, argv[2], mainPtr);
	if (other == NULL)
	    return TCL_ERROR;
    }
    if (Ck_RestackWindow(winPtr, CK_BELOW, other) != TCL_OK) {
	Tcl_AppendResult(interp, "can't lower \"", argv[1], "\" below \"",
		argv[2], "\"", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_RaiseCmd --
 *
 *	This procedure is invoked to process the "raise" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_RaiseCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *mainPtr = (CkWindow *) clientData;
    CkWindow *winPtr, *other;

    if ((argc != 2) && (argc != 3)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " window ?aboveThis?\"", (char *) NULL);
	return TCL_ERROR;
    }

    winPtr = Ck_NameToWindow(interp, argv[1], mainPtr);
    if (winPtr == NULL)
	return TCL_ERROR;
    if (argc == 2)
	other = NULL;
    else {
	other = Ck_NameToWindow(interp, argv[2], mainPtr);
	if (other == NULL)
	    return TCL_ERROR;
    }
    if (Ck_RestackWindow(winPtr, CK_ABOVE, other) != TCL_OK) {
	Tcl_AppendResult(interp, "can't raise \"", argv[1], "\" above \"",
		argv[2], "\"", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_BellCmd --
 *
 *	This procedure is invoked to process the "bell" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_BellCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    beep();
    doupdate();
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_UpdateCmd --
 *
 *	This procedure is invoked to process the "update" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_UpdateCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *mainPtr = (CkWindow *) clientData;
    int flags;

    if (argc == 1)
	flags = TK_DONT_WAIT;
    else if (argc == 2) {
	if (strncmp(argv[1], "screen", strlen(argv[1])) == 0) {
            wrefresh(curscr);
	    Ck_EventuallyRefresh(mainPtr);
	    return TCL_OK;
	}
	if (strncmp(argv[1], "idletasks", strlen(argv[1])) != 0) {
	    Tcl_AppendResult(interp, "bad argument \"", argv[1],
		    "\": must be idletasks or screen", (char *) NULL);
	    return TCL_ERROR;
	}
	flags = TK_IDLE_EVENTS;
    } else {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " ?idletasks|screen?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Handle all pending events, and repeat over and over
     * again until all pending events have been handled.
     */

    while (Tk_DoOneEvent(flags) != 0) {
	/* Empty loop body */
    }

    /*
     * Must clear the interpreter's result because event handlers could
     * have executed commands.
     */

    Tcl_ResetResult(interp);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_CursesCmd --
 *
 *	This procedure is invoked to process the "curses" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_CursesCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *winPtr = (CkWindow *) clientData;
    CkMainInfo *mainPtr = winPtr->mainPtr;
    int length;
    char c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
	    argv[0], " option ?arg?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'b') && (strncmp(argv[1], "barcode", length) == 0)) {
	return CkBarcodeCmd(clientData, interp, argc, argv);
    } else if ((c == 'b') && (strncmp(argv[1], "baudrate", length) == 0)) {
	char buf[32];

	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: must be \"", argv[0],
		" ", argv[1], "\"", (char *) NULL);
	    return TCL_ERROR;
        }
	sprintf(buf, "%d", baudrate());
	Tcl_AppendResult(interp, buf, (char *) NULL);
	return TCL_OK;
    } else if ((c == 'e') && (strncmp(argv[1], "encoding", length) == 0)) {
	if (argc == 2)
	    return Ck_GetEncoding(interp);
	else if (argc == 3)
	    return Ck_SetEncoding(interp, argv[2]);
	else {
	    Tcl_AppendResult(interp, "wrong # args: must be \"", argv[0],
		" ", argv[1], " ?name?\"", (char *) NULL);
	    return TCL_ERROR;
        }
    } else if ((c == 'g') && (strncmp(argv[1], "gchar", length) == 0)) {
	long gchar;
	int gc;

	if (argc == 3) {
	    if (Ck_GetGChar(interp, argv[2], &gchar) != TCL_OK)
		return TCL_ERROR;
	    sprintf(interp->result, "%ld", gchar);
	} else if (argc == 4) {
	    if (Tcl_GetInt(interp, argv[3], &gc) != TCL_OK)
		return TCL_ERROR;
	    gchar = gc;
	    if (Ck_SetGChar(interp, argv[2], gchar) != TCL_OK)
		return TCL_ERROR;
	} else {
	    Tcl_AppendResult(interp, "wrong # args: must be \"", argv[0],
		" ", argv[1], " charName ?value?\"", (char *) NULL);
	    return TCL_ERROR;
        }
    } else if ((c == 'h') && (strncmp(argv[1], "haskey", length) == 0)) {
	if (argc > 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " haskey ?keySym?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 2)
	    return CkAllKeyNames(interp);
	return CkTermHasKey(interp, argv[2]);
    } else if ((c == 'p') && (strncmp(argv[1], "purgeinput", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " purgeinput\"", (char *) NULL);
	    return TCL_ERROR;
	}
	while (getch() != ERR) {
	    /* Empty loop body. */
	}
	return TCL_OK;
    } else if ((c == 'r') && (strncmp(argv[1], "refreshdelay", length) == 0)) {
	if (argc == 2) {
	    char buf[32];

	    sprintf(buf, "%d", mainPtr->refreshDelay);
	    Tcl_AppendResult(interp, buf, (char *) NULL);
	    return TCL_OK;
	} else if (argc == 3) {
	    int delay;

	    if (Tcl_GetInt(interp, argv[2], &delay) != TCL_OK)
		return TCL_ERROR;
	    mainPtr->refreshDelay = delay < 0 ? 0 : delay;
	    return TCL_OK;
	} else {
	    Tcl_AppendResult(interp, "wrong # args: must be \"", argv[0],
		" ", argv[1], " ?milliseconds?\"", (char *) NULL);
	    return TCL_ERROR;
	}
    } else if ((c == 'r') && (strncmp(argv[1], "reversekludge", length)
        == 0)) {
	int onoff;

	if (argc == 2) {
	    interp->result = (mainPtr->flags & CK_REVERSE_KLUDGE) ?
		"1" : "0";
	} else if (argc == 3) {
	    if (Tcl_GetBoolean(interp, argv[2], &onoff) != TCL_OK)
		return TCL_ERROR;
	    mainPtr->flags |= CK_REVERSE_KLUDGE;
	} else {
	    Tcl_AppendResult(interp, "wrong # args: must be \"", argv[0],
		" ", argv[1], " ?bool?\"", (char *) NULL);
	    return TCL_ERROR;
        }
    } else if ((c == 's') && (strncmp(argv[1], "screendump", length) == 0)) {
	Tcl_DString buffer;
	char *fileName;
#ifdef HAVE_SCR_DUMP
	int ret;
#endif

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: must be \"", argv[0],
		" ", argv[1], " filename\"", (char *) NULL);
	    return TCL_ERROR;
	}
	fileName = Tcl_TildeSubst(interp, argv[2], &buffer);
	if (fileName == NULL) {
	    Tcl_DStringFree(&buffer);
	    return TCL_ERROR;
	}
#ifdef HAVE_SCR_DUMP
	ret = scr_dump(fileName);
	Tcl_DStringFree(&buffer);
	if (ret != OK) {
	    interp->result = "screen dump failed";
	    return TCL_ERROR;
	}
	return TCL_OK;
#else
	interp->result = "screen dump not supported by this curses";
	return TCL_ERROR;
#endif
    } else if ((c == 's') && (strncmp(argv[1], "suspend", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: must be \"", argv[0],
		" ", argv[1], "\"", (char *) NULL);
	    return TCL_ERROR;
	}
#ifndef __WIN32__
	curs_set(1);
	endwin();
#ifdef SIGTSTP
	kill(getpid(), SIGTSTP);
#else
	kill(getpid(), SIGSTOP);
#endif
	Ck_EventuallyRefresh(winPtr);
#endif
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
	    "\": must be barcode, baudrate, encoding, gchar, haskey, ",
	    "purgeinput, refreshdelay, reversekludge, screendump or suspend",
	    (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_WinfoCmd --
 *
 *	This procedure is invoked to process the "winfo" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_WinfoCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *mainPtr = (CkWindow *) clientData;
    int length;
    char c, *argName;
    CkWindow *winPtr;

#define SETUP(name) \
    if (argc != 3) {\
	argName = name; \
	goto wrongArgs; \
    } \
    winPtr = Ck_NameToWindow(interp, argv[2], mainPtr); \
    if (winPtr == NULL) { \
	return TCL_ERROR; \
    }


    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'c') && (strncmp(argv[1], "children", length) == 0)
	    && (length >= 2)) {
	SETUP("children");
	for (winPtr = winPtr->childList; winPtr != NULL;
		winPtr = winPtr->nextPtr) {
	    Tcl_AppendElement(interp, winPtr->pathName);
	}
    } else if ((c == 'c') && (strncmp(argv[1], "containing", length) == 0)
	    && (length >= 2)) {
	int x, y;

	argName = "containing";
	if (argc != 4)
	    goto wrongArgs;
	if (Tcl_GetInt(interp, argv[2], &x) != TCL_OK ||
	    Tcl_GetInt(interp, argv[3], &y) != TCL_OK) {
	    return TCL_ERROR;
	}
	winPtr = Ck_GetWindowXY(mainPtr->mainPtr, &x, &y, 0);
	if (winPtr != NULL) {
	    interp->result = winPtr->pathName;
	}
    } else if ((c == 'd') && (strncmp(argv[1], "depth", length) == 0)) {
	SETUP("depth");
	interp->result = (winPtr->mainPtr->flags & CK_HAS_COLOR) ? "3" : "1";
    } else if ((c == 'e') && (strncmp(argv[1], "exists", length) == 0)) {
	if (argc != 3) {
	    argName = "exists";
	    goto wrongArgs;
	}
	if (Ck_NameToWindow(interp, argv[2], mainPtr) == NULL) {
	    interp->result = "0";
	} else {
	    interp->result = "1";
	}
    } else if ((c == 'g') && (strncmp(argv[1], "geometry", length) == 0)) {
	SETUP("geometry");
	sprintf(interp->result, "%dx%d+%d+%d", winPtr->width,
		winPtr->height, winPtr->x, winPtr->y);
    } else if ((c == 'h') && (strncmp(argv[1], "height", length) == 0)) {
	SETUP("height");
	sprintf(interp->result, "%d", winPtr->height);
    } else if ((c == 'i') && (strncmp(argv[1], "ismapped", length) == 0)
	    && (length >= 2)) {
	SETUP("ismapped");
	interp->result = (winPtr->flags & CK_MAPPED) ? "1" : "0";
    } else if ((c == 'm') && (strncmp(argv[1], "manager", length) == 0)) {
	SETUP("manager");
	if (winPtr->geomMgrPtr != NULL)
	    interp->result = winPtr->geomMgrPtr->name;
    } else if ((c == 'n') && (strncmp(argv[1], "name", length) == 0)) {
	SETUP("name");
	interp->result = (char *) winPtr->nameUid;
    } else if ((c == 'c') && (strncmp(argv[1], "class", length) == 0)) {
	SETUP("class");
	interp->result = (char *) winPtr->classUid;
    } else if ((c == 'p') && (strncmp(argv[1], "parent", length) == 0)) {
	SETUP("parent");
	if (winPtr->parentPtr != NULL)
	    interp->result = winPtr->parentPtr->pathName;
    } else if ((c == 'r') && (strncmp(argv[1], "reqheight", length) == 0)
	    && (length >= 4)) {
	SETUP("reqheight");
	sprintf(interp->result, "%d", winPtr->reqHeight);
    } else if ((c == 'r') && (strncmp(argv[1], "reqwidth", length) == 0)
	    && (length >= 4)) {
	SETUP("reqwidth");
	sprintf(interp->result, "%d", winPtr->reqWidth);
    } else if ((c == 'r') && (strncmp(argv[1], "rootx", length) == 0)
	    && (length >= 4)) {
        int x;

	SETUP("rootx");
        Ck_GetRootGeometry(winPtr, &x, NULL, NULL, NULL);
	sprintf(interp->result, "%d", x);
    } else if ((c == 'r') && (strncmp(argv[1], "rooty", length) == 0)
	    && (length >= 4)) {
	int y;

	SETUP("rooty");
	Ck_GetRootGeometry(winPtr, NULL, &y, NULL, NULL);
	sprintf(interp->result, "%d", y);
    } else if ((c == 's') && (strncmp(argv[1], "screenheight", length) == 0)
	    && (length >= 7)) {
	SETUP("screenheight");
	sprintf(interp->result, "%d", winPtr->mainPtr->winPtr->height);
    } else if ((c == 's') && (strncmp(argv[1], "screenwidth", length) == 0)
	    && (length >= 7)) {
	SETUP("screenwidth");
	sprintf(interp->result, "%d", winPtr->mainPtr->winPtr->width);
    } else if ((c == 't') && (strncmp(argv[1], "toplevel", length) == 0)) {
        SETUP("toplevel");
	for (; winPtr != NULL; winPtr = winPtr->parentPtr) {
	    if (winPtr->flags & CK_TOPLEVEL) {
		interp->result = winPtr->pathName;
		break;
	    }
	}
    } else if ((c == 'w') && (strncmp(argv[1], "width", length) == 0)) {
	SETUP("width");
	sprintf(interp->result, "%d", winPtr->width);
    } else if ((c == 'x') && (argv[1][1] == '\0')) {
	SETUP("x");
	sprintf(interp->result, "%d", winPtr->x);
    } else if ((c == 'y') && (argv[1][1] == '\0')) {
	SETUP("y");
	sprintf(interp->result, "%d", winPtr->y);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be children, class, containing, depth ",
		"exists, geometry, height, ",
		"ismapped, manager, name, parent, ",
		"reqheight, reqwidth, rootx, rooty, ",
		"screenheight, screenwidth, ",
		"toplevel, width, x, or y", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;

    wrongArgs:
    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
	    argv[0], " ", argName, " window\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_BindCmd --
 *
 *	This procedure is invoked to process the "bind" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_BindCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *mainWin = (CkWindow *) clientData;
    CkWindow *winPtr;
    ClientData object;

    if ((argc < 2) || (argc > 4)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" window ?pattern? ?command?\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (argv[1][0] == '.') {
	winPtr = (CkWindow *) Ck_NameToWindow(interp, argv[1], mainWin);
	if (winPtr == NULL) {
	    return TCL_ERROR;
	}
	object = (ClientData) winPtr->pathName;
    } else {
	winPtr = (CkWindow *) clientData;
	object = (ClientData) Ck_GetUid(argv[1]);
    }

    if (argc == 4) {
	int append = 0;

	if (argv[3][0] == 0) {
	    return Ck_DeleteBinding(interp, winPtr->mainPtr->bindingTable,
		    object, argv[2]);
	}
	if (argv[3][0] == '+') {
	    argv[3]++;
	    append = 1;
	}
	if (Ck_CreateBinding(interp, winPtr->mainPtr->bindingTable,
		object, argv[2], argv[3], append) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else if (argc == 3) {
	char *command;

	command = Ck_GetBinding(interp, winPtr->mainPtr->bindingTable,
		object, argv[2]);
	if (command == NULL) {
	    Tcl_ResetResult(interp);
	    return TCL_OK;
	}
	interp->result = command;
    } else {
	Ck_GetAllBindings(interp, winPtr->mainPtr->bindingTable, object);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CkBindEventProc --
 *
 *	This procedure is invoked by Ck_HandleEvent for each event;  it
 *	causes any appropriate bindings for that event to be invoked.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on what bindings have been established with the "bind"
 *	command.
 *
 *----------------------------------------------------------------------
 */

void
CkBindEventProc(winPtr, eventPtr)
    CkWindow *winPtr;			/* Pointer to info about window. */
    CkEvent *eventPtr;			/* Information about event. */
{
#define MAX_OBJS 20
    ClientData objects[MAX_OBJS], *objPtr;
    static Ck_Uid allUid = NULL;
    int i, count;
    char *p;
    Tcl_HashEntry *hPtr;
    CkWindow *topLevPtr;

    if ((winPtr->mainPtr == NULL) || (winPtr->mainPtr->bindingTable == NULL)) {
	return;
    }

    objPtr = objects;
    if (winPtr->numTags != 0) {
	/*
	 * Make a copy of the tags for the window, replacing window names
	 * with pointers to the pathName from the appropriate window.
	 */

	if (winPtr->numTags > MAX_OBJS) {
	    objPtr = (ClientData *) ckalloc(winPtr->numTags *
	        sizeof (ClientData));
	}
	for (i = 0; i < winPtr->numTags; i++) {
	    p = (char *) winPtr->tagPtr[i];
	    if (*p == '.') {
		hPtr = Tcl_FindHashEntry(&winPtr->mainPtr->nameTable, p);
		if (hPtr != NULL) {
		    p = ((CkWindow *) Tcl_GetHashValue(hPtr))->pathName;
		} else {
		    p = NULL;
		}
	    }
	    objPtr[i] = (ClientData) p;
	}
	count = winPtr->numTags;
    } else {
	objPtr[0] = (ClientData) winPtr->pathName;
	objPtr[1] = (ClientData) winPtr->classUid;
	for (topLevPtr = winPtr; topLevPtr != NULL && 
	     !(topLevPtr->flags & CK_TOPLEVEL);
	     topLevPtr = topLevPtr->parentPtr) {
	     /* Empty loop body. */
	}
	if (winPtr != topLevPtr && topLevPtr != NULL) {
	    objPtr[2] = (ClientData) topLevPtr->pathName;
	    count = 4;
	} else
	    count = 3;
	if (allUid == NULL) {
	    allUid = Ck_GetUid("all");
	}
	objPtr[count - 1] = (ClientData) allUid;
    }
    Ck_BindEvent(winPtr->mainPtr->bindingTable, eventPtr, winPtr,
	    count, objPtr);
    if (objPtr != objects) {
	ckfree((char *) objPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_BindtagsCmd --
 *
 *	This procedure is invoked to process the "bindtags" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_BindtagsCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *mainWin = (CkWindow *) clientData;
    CkWindow *winPtr, *winPtr2;
    int i, tagArgc;
    char *p, **tagArgv;

    if ((argc < 2) || (argc > 3)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" window ?tags?\"", (char *) NULL);
	return TCL_ERROR;
    }
    winPtr = (CkWindow *) Ck_NameToWindow(interp, argv[1], mainWin);
    if (winPtr == NULL) {
	return TCL_ERROR;
    }
    if (argc == 2) {
	if (winPtr->numTags == 0) {
	    Tcl_AppendElement(interp, winPtr->pathName);
	    Tcl_AppendElement(interp, winPtr->classUid);
	    for (winPtr2 = winPtr; winPtr2 != NULL && 
		 !(winPtr2->flags & CK_TOPLEVEL);
		 winPtr2 = winPtr2->parentPtr) {
		 /* Empty loop body. */
	    }
	    if (winPtr != winPtr2 && winPtr2 != NULL)
		Tcl_AppendElement(interp, winPtr2->pathName);
	    Tcl_AppendElement(interp, "all");
	} else {
	    for (i = 0; i < winPtr->numTags; i++) {
		Tcl_AppendElement(interp, (char *) winPtr->tagPtr[i]);
	    }
	}
	return TCL_OK;
    }
    if (winPtr->tagPtr != NULL) {
	CkFreeBindingTags(winPtr);
    }
    if (argv[2][0] == 0) {
	return TCL_OK;
    }
    if (Tcl_SplitList(interp, argv[2], &tagArgc, &tagArgv) != TCL_OK) {
	return TCL_ERROR;
    }
    winPtr->numTags = tagArgc;
    winPtr->tagPtr = (ClientData *) ckalloc(tagArgc * sizeof(ClientData));
    for (i = 0; i < tagArgc; i++) {
	p = tagArgv[i];
	if (p[0] == '.') {
	    char *copy;

	    /*
	     * Handle names starting with "." specially: store a malloc'ed
	     * string, rather than a Uid;  at event time we'll look up the
	     * name in the window table and use the corresponding window,
	     * if there is one.
	     */

	    copy = (char *) ckalloc((unsigned) (strlen(p) + 1));
	    strcpy(copy, p);
	    winPtr->tagPtr[i] = (ClientData) copy;
	} else {
	    winPtr->tagPtr[i] = (ClientData) Ck_GetUid(p);
	}
    }
    ckfree((char *) tagArgv);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CkFreeBindingTags --
 *
 *	This procedure is called to free all of the binding tags
 *	associated with a window;  typically it is only invoked where
 *	there are window-specific tags.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Any binding tags for winPtr are freed.
 *
 *----------------------------------------------------------------------
 */

void
CkFreeBindingTags(winPtr)
    CkWindow *winPtr;		/* Window whose tags are to be released. */
{
    int i;
    char *p;

    for (i = 0; i < winPtr->numTags; i++) {
	p = (char *) (winPtr->tagPtr[i]);
	if (*p == '.') {
	    /*
	     * Names starting with "." are malloced rather than Uids, so
	     * they have to be freed.
	     */
    
	    ckfree(p);
	}
    }
    ckfree((char *) winPtr->tagPtr);
    winPtr->numTags = 0;
    winPtr->tagPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_TkwaitCmd --
 *
 *	This procedure is invoked to process the "tkwait" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_TkwaitCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *mainPtr = (CkWindow *) clientData;
    int c, done;
    size_t length;

    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " variable|visible|window name\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'v') && (strncmp(argv[1], "variable", length) == 0)
	    && (length >= 2)) {
	if (Tcl_TraceVar(interp, argv[2],
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		WaitVariableProc, (ClientData) &done) != TCL_OK) {
	    return TCL_ERROR;
	}
	done = 0;
	while (!done) {
	    Tk_DoOneEvent(0);
	}
	Tcl_UntraceVar(interp, argv[2],
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		WaitVariableProc, (ClientData) &done);
    } else if ((c == 'v') && (strncmp(argv[1], "visibility", length) == 0)
	    && (length >= 2)) {
	CkWindow *winPtr;

	winPtr = Ck_NameToWindow(interp, argv[2], mainPtr);
	if (winPtr == NULL) {
	    return TCL_ERROR;
	}
	Ck_CreateEventHandler(winPtr,
	    CK_EV_MAP | CK_EV_UNMAP | CK_EV_EXPOSE | CK_EV_DESTROY,
	    WaitVisibilityProc, (ClientData) &done);
	done = 0;
	while (!done) {
	    Tk_DoOneEvent(0);
	}
	Ck_DeleteEventHandler(winPtr,
	    CK_EV_MAP | CK_EV_UNMAP | CK_EV_EXPOSE | CK_EV_DESTROY,
	    WaitVisibilityProc, (ClientData) &done);
    } else if ((c == 'w') && (strncmp(argv[1], "window", length) == 0)) {
	CkWindow *winPtr;

	winPtr = Ck_NameToWindow(interp, argv[2], mainPtr);
	if (winPtr == NULL) {
	    return TCL_ERROR;
	}
	Ck_CreateEventHandler(winPtr, CK_EV_DESTROY,
	    WaitWindowProc, (ClientData) &done);
	done = 0;
	while (!done) {
	    Tk_DoOneEvent(0);
	}
	/*
	 * Note:  there's no need to delete the event handler.  It was
	 * deleted automatically when the window was destroyed.
	 */
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be variable, visibility, or window", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Clear out the interpreter's result, since it may have been set
     * by event handlers.
     */

    Tcl_ResetResult(interp);
    return TCL_OK;
}

static char *
WaitVariableProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Pointer to integer to set to 1. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    int *donePtr = (int *) clientData;

    *donePtr = 1;
    return (char *) NULL;
}

static void
WaitVisibilityProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to integer to set to 1. */
    CkEvent *eventPtr;		/* Information about event (not used). */
{
    int *donePtr = (int *) clientData;

    *donePtr = 1;
}

static void
WaitWindowProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to integer to set to 1. */
    CkEvent *eventPtr;		/* Information about event. */
{
    int *donePtr = (int *) clientData;

    if (eventPtr->type == CK_EV_DESTROY) {
	*donePtr = 1;
    }
}
