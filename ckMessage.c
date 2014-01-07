/* 
 * ckMessage.c --
 *
 *	This module implements a message widgets for the
 *	toolkit.  A message widget displays a multi-line string
 *	in a window according to a particular aspect ratio.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 * Copyright (c) 1995-2002 Christian Werner
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "ckPort.h"
#include "ck.h"
#include "default.h"

/*
 * A data structure of the following type is kept for each message
 * widget managed by this file:
 */

typedef struct {
    CkWindow *winPtr;		/* Window that embodies the message.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Tcl_Interp *interp;		/* Interpreter associated with message. */
    Tcl_Command widgetCmd;      /* Token for message's widget command. */
    char *string;		/* String displayed in message. */
    int numChars;		/* Number of characters in string, not
				 * including terminating NULL character. */
    char *textVarName;		/* Name of variable (malloc'ed) or NULL.
				 * If non-NULL, message displays the contents
				 * of this variable. */

    /*
     * Information used when displaying widget:
     */

    int bg, fg;			/* Foreground and background colors. */
    int attr;			/* Video attributes. */
    Ck_Anchor anchor;		/* Where to position text within window region
				 * if window is larger or smaller than
				 * needed. */
    int width;			/* User-requested width. 0 means compute
				 * width using aspect ratio below. */
    int aspect;			/* Desired aspect ratio for window
				 * (100*width/height). */
    int lineLength;		/* Length of each line.  Computed
				 * from width and/or aspect. */
    int msgHeight;		/* Total lines needed to display message. */
    Ck_Justify justify;		/* Justification for text. */

    /*
     * Miscellaneous information:
     */

    char *takeFocus;		/* Value of -takefocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
    int flags;			/* Various flags;  see below for
				 * definitions. */
} Message;

/*
 * Flag bits for messages:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 */

#define REDRAW_PENDING		1

/*
 * Information used for argv parsing.
 */

static Ck_ConfigSpec configSpecs[] = {
    {CK_CONFIG_ANCHOR, "-anchor", "anchor", "Anchor",
	DEF_MESSAGE_ANCHOR, Ck_Offset(Message, anchor), 0},
    {CK_CONFIG_INT, "-aspect", "aspect", "Aspect",
	DEF_MESSAGE_ASPECT, Ck_Offset(Message, aspect), 0},
    {CK_CONFIG_ATTR, "-attributes", "attributes", "Attributes",
	DEF_MESSAGE_ATTR, Ck_Offset(Message, attr), 0},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_MESSAGE_BG_COLOR, Ck_Offset(Message, bg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_MESSAGE_BG_MONO, Ck_Offset(Message, bg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_MESSAGE_FG_COLOR, Ck_Offset(Message, fg),
	CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_MESSAGE_FG_MONO, Ck_Offset(Message, fg),
	CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_JUSTIFY, "-justify", "justify", "Justify",
	DEF_MESSAGE_JUSTIFY, Ck_Offset(Message, justify), 0},
    {CK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_MESSAGE_TAKE_FOCUS, Ck_Offset(Message, takeFocus),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-text", "text", "Text",
	DEF_MESSAGE_TEXT, Ck_Offset(Message, string), 0},
    {CK_CONFIG_STRING, "-textvariable", "textVariable", "Variable",
	DEF_MESSAGE_TEXT_VARIABLE, Ck_Offset(Message, textVarName),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_COORD, "-width", "width", "Width",
	DEF_MESSAGE_WIDTH, Ck_Offset(Message, width), 0},
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		MessageEventProc _ANSI_ARGS_((ClientData clientData,
			    CkEvent *eventPtr));
static char *		MessageTextVarProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));
static int		MessageWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static void             MessageCmdDeletedProc _ANSI_ARGS_((
                            ClientData clientData));
static void		ComputeMessageGeometry _ANSI_ARGS_((Message *msgPtr));
static int		ConfigureMessage _ANSI_ARGS_((Tcl_Interp *interp,
			    Message *msgPtr, int argc, char **argv,
			    int flags));
static void		DestroyMessage _ANSI_ARGS_((ClientData clientData));
static void		DisplayMessage _ANSI_ARGS_((ClientData clientData));

/*
 *--------------------------------------------------------------
 *
 * Ck_MessageCmd --
 *
 *	This procedure is invoked to process the "message" Tcl
 *	command.  See the user documentation for details on what
 *	it does.
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
Ck_MessageCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    register Message *msgPtr;
    CkWindow *new;
    CkWindow *mainPtr = (CkWindow *) clientData;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args:  should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    new = Ck_CreateWindowFromPath(interp, mainPtr, argv[1], 0);
    if (new == NULL) {
	return TCL_ERROR;
    }

    msgPtr = (Message *) ckalloc(sizeof (Message));
    msgPtr->winPtr = new;
    msgPtr->interp = interp;
    msgPtr->widgetCmd = Tcl_CreateCommand(interp,
        msgPtr->winPtr->pathName, MessageWidgetCmd,
	    (ClientData) msgPtr, MessageCmdDeletedProc);
    msgPtr->string = NULL;
    msgPtr->numChars = 0;
    msgPtr->textVarName = NULL;
    msgPtr->bg = 0;
    msgPtr->fg = 0;
    msgPtr->attr = 0;
    msgPtr->anchor = CK_ANCHOR_CENTER;
    msgPtr->width = 0;
    msgPtr->aspect = 150;
    msgPtr->lineLength = 0;
    msgPtr->msgHeight = 0;
    msgPtr->justify = CK_JUSTIFY_LEFT;
    msgPtr->takeFocus = NULL;
    msgPtr->flags = 0;

    Ck_SetClass(msgPtr->winPtr, "Message");
    Ck_CreateEventHandler(msgPtr->winPtr,
    	CK_EV_EXPOSE | CK_EV_MAP | CK_EV_DESTROY,
        MessageEventProc, (ClientData) msgPtr);
    if (ConfigureMessage(interp, msgPtr, argc-2, argv+2, 0) != TCL_OK) {
	goto error;
    }

    interp->result = msgPtr->winPtr->pathName;
    return TCL_OK;

error:
    Ck_DestroyWindow(msgPtr->winPtr);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * MessageWidgetCmd --
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
MessageWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about message widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    register Message *msgPtr = (Message *) clientData;
    size_t length;
    int c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	&& (length >= 2)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "wrong # args: should be \"",
                    argv[0], " cget option\"",
                    (char *) NULL);
            return TCL_ERROR;
        }
        return Ck_ConfigureValue(interp, msgPtr->winPtr, configSpecs,
                (char *) msgPtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length  >= 2)) {
	if (argc == 2) {
	    return Ck_ConfigureInfo(interp, msgPtr->winPtr, configSpecs,
		    (char *) msgPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    return Ck_ConfigureInfo(interp, msgPtr->winPtr, configSpecs,
		    (char *) msgPtr, argv[2], 0);
	} else {
	    return ConfigureMessage(interp, msgPtr, argc-2, argv+2,
		    CK_CONFIG_ARGV_ONLY);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\":  must be cget or configure", (char *) NULL);
	return TCL_ERROR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyMessage --
 *
 *	This procedure is invoked by Ck_EventuallyFree or Ck_Release
 *	to clean up the internal structure of a message at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the message is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyMessage(clientData)
    ClientData clientData;	/* Info about message widget. */
{
    register Message *msgPtr = (Message *) clientData;

    /*
     * Free up all the stuff that requires special handling, then
     * let Ck_FreeOptions handle all the standard option-related
     * stuff.
     */

    if (msgPtr->textVarName != NULL) {
	Tcl_UntraceVar(msgPtr->interp, msgPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MessageTextVarProc, (ClientData) msgPtr);
    }
    Ck_FreeOptions(configSpecs, (char *) msgPtr, 0);
    ckfree((char *) msgPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * MessageCmdDeletedProc --
 *
 *      This procedure is invoked when a widget command is deleted.  If
 *      the widget isn't already in the process of being destroyed,
 *      this command destroys it.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The widget is destroyed.
 *
 *----------------------------------------------------------------------
 */

static void
MessageCmdDeletedProc(clientData)
    ClientData clientData;      /* Pointer to widget record for widget. */
{
    Message *msgPtr = (Message *) clientData;
    CkWindow *winPtr = msgPtr->winPtr;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case winPtr
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (winPtr != NULL) {
        msgPtr->winPtr = NULL;
        Ck_DestroyWindow(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureMessage --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the option database, in order to configure (or
 *	reconfigure) a message widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors,
 *	etc. get set for msgPtr;  old resources get freed, if there
 *	were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureMessage(interp, msgPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    register Message *msgPtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    /*
     * Eliminate any existing trace on a variable monitored by the message.
     */

    if (msgPtr->textVarName != NULL) {
	Tcl_UntraceVar(interp, msgPtr->textVarName, 
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MessageTextVarProc, (ClientData) msgPtr);
    }

    if (Ck_ConfigureWidget(interp, msgPtr->winPtr, configSpecs,
	    argc, argv, (char *) msgPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * If the message is to display the value of a variable, then set up
     * a trace on the variable's value, create the variable if it doesn't
     * exist, and fetch its current value.
     */

    if (msgPtr->textVarName != NULL) {
	char *value;

	value = Tcl_GetVar(interp, msgPtr->textVarName, TCL_GLOBAL_ONLY);
	if (value == NULL) {
	    Tcl_SetVar(interp, msgPtr->textVarName,
		    msgPtr->string == NULL ? "" : msgPtr->string,
		    TCL_GLOBAL_ONLY);
	} else {
	    if (msgPtr->string != NULL) {
		ckfree(msgPtr->string);
	    }
	    msgPtr->string = (char *) ckalloc((unsigned) (strlen(value) + 1));
	    strcpy(msgPtr->string, value);
	}
	Tcl_TraceVar(interp, msgPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MessageTextVarProc, (ClientData) msgPtr);
    }

    /*
     * A few other options need special processing, such as setting
     * the background from a 3-D border or handling special defaults
     * that couldn't be specified to Tk_ConfigureWidget.
     */

    if (msgPtr->string == NULL) {
	msgPtr->string = ckalloc(1);
	msgPtr->string[0] = '\0';
    }
    msgPtr->numChars = strlen(msgPtr->string);

    /*
     * Recompute the desired geometry for the window, and arrange for
     * the window to be redisplayed.
     */

    ComputeMessageGeometry(msgPtr);
    if ((msgPtr->winPtr != NULL) && (msgPtr->winPtr->flags & CK_MAPPED)
	    && !(msgPtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplayMessage, (ClientData) msgPtr);
	msgPtr->flags |= REDRAW_PENDING;
    }

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ComputeMessageGeometry --
 *
 *	Compute the desired geometry for a message window,
 *	taking into account the desired aspect ratio for the
 *	window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Ck_GeometryRequest is called to inform the geometry
 *	manager of the desired geometry for this window.
 *
 *--------------------------------------------------------------
 */

static void
ComputeMessageGeometry(msgPtr)
    register Message *msgPtr;	/* Information about window. */
{
    char *p;
    int width, inc, height, numLines;
    int thisWidth, maxWidth;
    int aspect, lowerBound, upperBound, nextC;
    CkWindow *winPtr = msgPtr->winPtr;

    /*
     * Compute acceptable bounds for the final aspect ratio.
     */

    aspect = msgPtr->aspect/10;
    if (aspect < 5) {
	aspect = 5;
    }
    lowerBound = msgPtr->aspect - aspect;
    upperBound = msgPtr->aspect + aspect;

    /*
     * Do the computation in multiple passes:  start off with
     * a very wide window, and compute its height.  Then change
     * the width and try again.  Reduce the size of the change
     * and iterate until dimensions are found that approximate
     * the desired aspect ratio.  Or, if the user gave an explicit
     * width then just use that.
     */

    if (msgPtr->width > 0) {
	width = msgPtr->width;
	inc = 0;
    } else {
	width = msgPtr->winPtr->mainPtr->winPtr->width;
	inc = width/2;
    }
    for ( ; ; inc /= 2) {
	maxWidth = 0;
	for (numLines = 1, p = msgPtr->string; ; numLines++)  {
	    if (*p == '\n') {
		p++;
		continue;
	    }
	    CkMeasureChars(winPtr->mainPtr, p,
		msgPtr->numChars - (p - msgPtr->string),
	        0, width, 0, CK_WHOLE_WORDS|CK_AT_LEAST_ONE, &thisWidth,
		&nextC);
	    p += nextC;

	    if (thisWidth > maxWidth) {
		maxWidth = thisWidth;
	    }
	    if (*p == 0) {
		break;
	    }

	    /*
	     * Skip spaces and tabs at the beginning of a line, unless
	     * they follow a user-requested newline.
	     */

	    while (isspace((unsigned char) (*p))) {
		if (*p == '\n') {
		    p++;
		    break;
		}
		p++;
	    }
	}

	height = numLines;
	if (inc <= 2) {
	    break;
	}
	aspect = (100 * maxWidth) / height;
	if (aspect < lowerBound) {
	    width += inc;
	} else if (aspect > upperBound) {
	    width -= inc;
	} else {
	    break;
	}
    }
    msgPtr->lineLength = maxWidth;
    msgPtr->msgHeight = numLines;
    Ck_GeometryRequest(msgPtr->winPtr, maxWidth, height);
}

/*
 *--------------------------------------------------------------
 *
 * DisplayMessage --
 *
 *	This procedure redraws the contents of a message window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information appears on the screen.
 *
 *--------------------------------------------------------------
 */

static void
DisplayMessage(clientData)
    ClientData clientData;	/* Information about window. */
{
    register Message *msgPtr = (Message *) clientData;
    register CkWindow *winPtr = msgPtr->winPtr;
    char *p;
    int x, y, lineLength, numChars, charsLeft, byteLength;

    msgPtr->flags &= ~REDRAW_PENDING;
    if (msgPtr->winPtr == NULL || !(winPtr->flags & CK_MAPPED)) {
	return;
    }

    Ck_SetWindowAttr(winPtr, msgPtr->fg, msgPtr->bg, msgPtr->attr);
    Ck_ClearToBot(winPtr, 0, 0);

    /*
     * Compute starting y-location for message based on message size
     * and anchor option.
     */

    switch (msgPtr->anchor) {
	case CK_ANCHOR_NW: case CK_ANCHOR_N: case CK_ANCHOR_NE:
	    y = 0;
	    break;
	case CK_ANCHOR_W: case CK_ANCHOR_CENTER: case CK_ANCHOR_E:
	    y = (winPtr->height - msgPtr->msgHeight) / 2;
	    break;
	default:
	    y = winPtr->height - msgPtr->msgHeight;
	    break;
    }
    if (y < 0) {
	y = 0;
    }

    /*
     * Work through the string to display one line at a time.
     * Display each line in three steps.  First compute the
     * line's width, then figure out where to display the
     * line to justify it properly, then display the line.
     */

    for (p = msgPtr->string, charsLeft = msgPtr->numChars; *p != 0; y++) {
	if (*p == '\n') {
	    p++;
	    charsLeft--;
	    continue;
	}
	numChars = CkMeasureChars(winPtr->mainPtr, p, charsLeft, 0,
		msgPtr->lineLength,
		0, CK_WHOLE_WORDS | CK_AT_LEAST_ONE, &lineLength, &byteLength);
	switch (msgPtr->anchor) {
	    case CK_ANCHOR_NW: case CK_ANCHOR_W: case CK_ANCHOR_SW:
	    	x = 0;
		break;
	    case CK_ANCHOR_N: case CK_ANCHOR_CENTER: case CK_ANCHOR_S:
		x = (winPtr->width - msgPtr->lineLength) / 2;
		break;
	    default:
		x = winPtr->width - msgPtr->lineLength;
		break;
	}
	if (msgPtr->justify == CK_JUSTIFY_CENTER) {
	    x += (msgPtr->lineLength - lineLength) / 2;
	} else if (msgPtr->justify == CK_JUSTIFY_RIGHT) {
	    x += msgPtr->lineLength - lineLength;
	}
	if (x < 0) {
	    x = 0;
	}
	CkDisplayChars(winPtr->mainPtr, winPtr->window, p, byteLength,
	    x, y, x, 0);
	p += byteLength;
	charsLeft -= byteLength;

	/*
	 * Skip blanks at the beginning of a line, unless they follow
	 * a user-requested newline.
	 */

	while (isspace((unsigned char) (*p))) {
	    charsLeft--;
	    if (*p == '\n') {
		p++;
		break;
	    }
	    p++;
	}
    }
    Ck_EventuallyRefresh(winPtr);
}

/*
 *--------------------------------------------------------------
 *
 * MessageEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on messages.
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
MessageEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    CkEvent *eventPtr;		/* Information about event. */
{
    Message *msgPtr = (Message *) clientData;

    if (eventPtr->type == CK_EV_EXPOSE) {
        if (msgPtr->winPtr != NULL && !(msgPtr->flags & REDRAW_PENDING)) {
	    Tk_DoWhenIdle(DisplayMessage, (ClientData) msgPtr);
	    msgPtr->flags |= REDRAW_PENDING;
	}
    } else if (eventPtr->type == CK_EV_DESTROY) {
        if (msgPtr->winPtr != NULL) {
            msgPtr->winPtr = NULL;
            Tcl_DeleteCommand(msgPtr->interp,
                    Tcl_GetCommandName(msgPtr->interp, msgPtr->widgetCmd));
        }
	if (msgPtr->flags & REDRAW_PENDING) {
	    Tk_CancelIdleCall(DisplayMessage, (ClientData) msgPtr);
	}
	Ck_EventuallyFree((ClientData) msgPtr, (Ck_FreeProc *) DestroyMessage);
    }
}

/*
 *--------------------------------------------------------------
 *
 * MessageTextVarProc --
 *
 *	This procedure is invoked when someone changes the variable
 *	whose contents are to be displayed in a message.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The text displayed in the message will change to match the
 *	variable.
 *
 *--------------------------------------------------------------
 */

static char *
MessageTextVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about message. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    register Message *msgPtr = (Message *) clientData;
    char *value;

    /*
     * If the variable is unset, then immediately recreate it unless
     * the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    Tcl_SetVar(interp, msgPtr->textVarName,
		    msgPtr->string == NULL ? "" : msgPtr->string,
		    TCL_GLOBAL_ONLY);
	    Tcl_TraceVar(interp, msgPtr->textVarName,
		    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    MessageTextVarProc, clientData);
	}
	return (char *) NULL;
    }

    value = Tcl_GetVar(interp, msgPtr->textVarName, TCL_GLOBAL_ONLY);
    if (value == NULL) {
	value = "";
    }
    if (msgPtr->string != NULL) {
	ckfree(msgPtr->string);
    }
    msgPtr->numChars = strlen(value);
    msgPtr->string = (char *) ckalloc((unsigned) (msgPtr->numChars + 1));
    strcpy(msgPtr->string, value);
    ComputeMessageGeometry(msgPtr);

    if ((msgPtr->winPtr != NULL) && (msgPtr->winPtr->flags & CK_MAPPED)
	    && !(msgPtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplayMessage, (ClientData) msgPtr);
	msgPtr->flags |= REDRAW_PENDING;
    }
    return (char *) NULL;
}
