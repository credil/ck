/* 
 * ckFrame.c --
 *
 *	This module implements "frame" and "toplevel" widgets for the
 *	toolkit.  Frames are windows with a background color
 *	and possibly border, but no other attributes.
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
#include "default.h"

/*
 * A data structure of the following type is kept for each
 * frame that currently exists for this process:
 */

typedef struct {
    CkWindow *winPtr;		/* Window that embodies the frame.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Tcl_Interp *interp;		/* Interpreter associated with widget.
    				 * Used to delete widget command.  */
    Tcl_Command widgetCmd;      /* Token for frame's widget command. */
    CkBorder *borderPtr;	/* Structure used to draw border. */
    int fg, bg;			/* Foreground/background colors. */
    int attr;			/* Video attributes. */
    int width;			/* Width to request for window.  <= 0 means
				 * don't request any size. */
    int height;			/* Height to request for window.  <= 0 means
				 * don't request any size. */
    char *takeFocus;		/* Value of -takefocus option. */
    int flags;			/* Various flags;  see below for
				 * definitions. */
} Frame;

/*
 * Flag bits for frames:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 */

#define REDRAW_PENDING		1

static Ck_ConfigSpec configSpecs[] = {
    {CK_CONFIG_ATTR, "-attributes", "attributes", "Attributes",
	DEF_FRAME_ATTRIB, Ck_Offset(Frame, attr), 0},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_FRAME_BG_COLOR, Ck_Offset(Frame, bg), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_FRAME_BG_MONO, Ck_Offset(Frame, bg), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_FRAME_FG_COLOR, Ck_Offset(Frame, fg), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_FRAME_FG_MONO, Ck_Offset(Frame, fg), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_BORDER, "-border", "border", "Border",
	DEF_FRAME_BORDER, Ck_Offset(Frame, borderPtr), CK_CONFIG_NULL_OK},
    {CK_CONFIG_COORD, "-height", "height", "Height",
	DEF_FRAME_HEIGHT, Ck_Offset(Frame, height), 0},
    {CK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_FRAME_TAKE_FOCUS, Ck_Offset(Frame, takeFocus),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_COORD, "-width", "width", "Width",
	DEF_FRAME_WIDTH, Ck_Offset(Frame, width), 0},
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static int	ConfigureFrame _ANSI_ARGS_((Tcl_Interp *interp,
		    Frame *framePtr, int argc, char **argv, int flags));
static void	DestroyFrame _ANSI_ARGS_((ClientData clientData));
static void     FrameCmdDeletedProc _ANSI_ARGS_((ClientData clientData));
static void	DisplayFrame _ANSI_ARGS_((ClientData clientData));
static void	FrameEventProc _ANSI_ARGS_((ClientData clientData,
		    CkEvent *eventPtr));
static int	FrameWidgetCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));

/*
 *--------------------------------------------------------------
 *
 * Ck_FrameCmd --
 *
 *	This procedure is invoked to process the "frame" and
 *	"toplevel" Tcl commands.  See the user documentation for
 *	details on what it does.
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
Ck_FrameCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *winPtr = (CkWindow *) clientData;
    CkWindow *new;
    char *className;
    int src, dst, toplevel;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * The code below is a special hack that extracts a few key
     * options from the argument list now, rather than letting
     * ConfigureFrame do it.  This is necessary because we have
     * to know the window's class before creating the window.
     */

    toplevel = *argv[0] == 't';
    className = NULL;
    for (src = 2, dst = 2; src < argc;  src += 2) {
	char c;

	c = argv[src][1];
	if ((c == 'c')
		&& (strncmp(argv[src], "-class", strlen(argv[src])) == 0)) {
	    className = argv[src+1];
	} else {
	    argv[dst] = argv[src];
	    argv[dst+1] = argv[src+1];
	    dst += 2;
	}
    }
    argc -= src-dst;

    /*
     * Create the window and initialize our structures and event handlers.
     */

    new = Ck_CreateWindowFromPath(interp, winPtr, argv[1], toplevel);
    if (new == NULL)
	return TCL_ERROR;
    if (className == NULL) {
        className = Ck_GetOption(new, "class", "Class");
        if (className == NULL) {
            className = (toplevel) ? "Toplevel" : "Frame";
        }
    }
    Ck_SetClass(new, className);
    return CkInitFrame(interp, new, argc-2, argv+2);
}

/*
 *----------------------------------------------------------------------
 *
 * CkInitFrame --
 *
 *	This procedure initializes a frame widget.  It's
 *	separate from Ck_FrameCmd so that it can be used for the
 *	main window, which has already been created elsewhere.
 *
 * Results:
 *	A standard Tcl completion code.
 *
 * Side effects:
 *	A widget record gets allocated, handlers get set up, etc..
 *
 *----------------------------------------------------------------------
 */

int
CkInitFrame(interp, winPtr, argc, argv)
    Tcl_Interp *interp;			/* Interpreter associated with the
					 * application. */
    CkWindow *winPtr;			/* Window to use for frame or
					 * top-level. Caller must already
					 * have set window's class. */
    int argc;				/* Number of configuration arguments
					 * (not including class command and
					 * window name). */
    char *argv[];			/* Configuration arguments. */
{
    Frame *framePtr;

    framePtr = (Frame *) ckalloc(sizeof (Frame));
    framePtr->winPtr = winPtr;
    framePtr->interp = interp;
    framePtr->widgetCmd = Tcl_CreateCommand(interp,
        framePtr->winPtr->pathName, FrameWidgetCmd,
	    (ClientData) framePtr, FrameCmdDeletedProc);
    framePtr->borderPtr = NULL;
    framePtr->fg = 0;
    framePtr->bg = 0;
    framePtr->attr = 0;
    framePtr->width = 1;
    framePtr->height = 1;
    framePtr->takeFocus = NULL;
    framePtr->flags = 0;
    Ck_CreateEventHandler(framePtr->winPtr,
    	    CK_EV_MAP | CK_EV_EXPOSE | CK_EV_DESTROY,
	    FrameEventProc, (ClientData) framePtr);
    if (ConfigureFrame(interp, framePtr, argc, argv, 0) != TCL_OK) {
	Ck_DestroyWindow(framePtr->winPtr);
	return TCL_ERROR;
    }
    interp->result = framePtr->winPtr->pathName;
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * FrameWidgetCmd --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a frame widget.  See the user
 *	documentation for details on what it does.
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
FrameWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about frame widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Frame *framePtr = (Frame *) clientData;
    int result = TCL_OK;
    int length;
    char c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Ck_Preserve((ClientData) framePtr);
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	&& (length >= 2)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "wrong # args: should be \"",
                    argv[0], " cget option\"",
                    (char *) NULL);
            goto error;
        }
        result = Ck_ConfigureValue(interp, framePtr->winPtr, configSpecs,
                (char *) framePtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)) {
	if (argc == 2) {
	    result = Ck_ConfigureInfo(interp, framePtr->winPtr, configSpecs,
		    (char *) framePtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Ck_ConfigureInfo(interp, framePtr->winPtr, configSpecs,
		    (char *) framePtr, argv[2], 0);
	} else {
	    result = ConfigureFrame(interp, framePtr, argc-2, argv+2,
		    CK_CONFIG_ARGV_ONLY);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\":  must be cget or configure", (char *) NULL);
	goto error;
    }
    Ck_Release((ClientData) framePtr);
    return result;

error:
    Ck_Release((ClientData) framePtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyFrame --
 *
 *	This procedure is invoked by Ck_EventuallyFree or Ck_Release
 *	to clean up the internal structure of a frame at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the frame is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyFrame(clientData)
    ClientData clientData;	/* Info about frame widget. */
{
    Frame *framePtr = (Frame *) clientData;

    Ck_FreeOptions(configSpecs, (char *) framePtr, 0);
    ckfree((char *) framePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * FrameCmdDeletedProc --
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
FrameCmdDeletedProc(clientData)
    ClientData clientData;      /* Pointer to widget record for widget. */
{
    Frame *framePtr = (Frame *) clientData;
    CkWindow *winPtr = framePtr->winPtr;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (winPtr != NULL) {
        framePtr->winPtr = NULL;
        Ck_DestroyWindow(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureFrame --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the option database, in order to configure (or
 *	reconfigure) a frame widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for framePtr;  old resources get freed, if there
 *	were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureFrame(interp, framePtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Frame *framePtr;		/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    if (Ck_ConfigureWidget(interp, framePtr->winPtr, configSpecs,
	    argc, argv, (char *) framePtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    Ck_SetWindowAttr(framePtr->winPtr, framePtr->fg, framePtr->bg,
    	framePtr->attr);
    Ck_SetInternalBorder(framePtr->winPtr, framePtr->borderPtr != NULL);
    if ((framePtr->width > 0) || (framePtr->height > 0))
	Ck_GeometryRequest(framePtr->winPtr, framePtr->width,
	    framePtr->height);
    if ((framePtr->winPtr->flags & CK_MAPPED)
	    && !(framePtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplayFrame, (ClientData) framePtr);
	framePtr->flags |= REDRAW_PENDING;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayFrame --
 *
 *	This procedure is invoked to display a frame widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to display the frame in its
 *	current mode.
 *
 *----------------------------------------------------------------------
 */

static void
DisplayFrame(clientData)
    ClientData clientData;	/* Information about widget. */
{
    Frame *framePtr = (Frame *) clientData;
    CkWindow *winPtr = framePtr->winPtr;

    framePtr->flags &= ~REDRAW_PENDING;
    if ((framePtr->winPtr == NULL) || !(winPtr->flags & CK_MAPPED)) {
	return;
    }
    Ck_ClearToBot(winPtr, 0, 0);
    if (framePtr->borderPtr != NULL)
	Ck_DrawBorder(winPtr, framePtr->borderPtr, 0, 0,
	    winPtr->width, winPtr->height);
    Ck_EventuallyRefresh(winPtr);
}

/*
 *--------------------------------------------------------------
 *
 * FrameEventProc --
 *
 *	This procedure is invoked by the dispatcher on
 *	structure changes to a frame.
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
FrameEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    CkEvent *eventPtr;		/* Information about event. */
{
    Frame *framePtr = (Frame *) clientData;

    if (eventPtr->type == CK_EV_EXPOSE && framePtr->winPtr != NULL &&
	!(framePtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplayFrame, (ClientData) framePtr);
	framePtr->flags |= REDRAW_PENDING;
    } else if (eventPtr->type == CK_EV_DESTROY) {
        if (framePtr->winPtr != NULL) {
            framePtr->winPtr = NULL;
            Tcl_DeleteCommand(framePtr->interp,
                    Tcl_GetCommandName(framePtr->interp, framePtr->widgetCmd));
        }
	if (framePtr->flags & REDRAW_PENDING)
	    Tk_CancelIdleCall(DisplayFrame, (ClientData) framePtr);
	Ck_EventuallyFree((ClientData) framePtr, (Ck_FreeProc *) DestroyFrame);
    }
}
