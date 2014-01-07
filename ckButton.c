/* 
 * ckButton.c --
 *
 *	This module implements a collection of button-like
 *	widgets for the Ck toolkit.  The widgets implemented
 *	include labels, buttons, check buttons, and radio
 *	buttons.
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
 * widget managed by this file:
 */

typedef struct {
    CkWindow *winPtr;		/* Window that embodies the button.  NULL
				 * means that the window has been destroyed. */
    Tcl_Interp *interp;		/* Interpreter associated with button. */
    Tcl_Command widgetCmd;      /* Token for button's widget command. */
    int type;			/* Type of widget:  restricts operations
				 * that may be performed on widget.  See
				 * below for possible values. */

    /*
     * Information about what's in the button.
     */

    char *text;			/* Text to display in button (malloc'ed)
				 * or NULL. */
    int textLength;		/* # of characters in text. */
    char *textVarName;		/* Name of variable (malloc'ed) or NULL.
				 * If non-NULL, button displays the contents
				 * of this variable. */

    /*
     * Information used when displaying widget:
     */

    Ck_Uid state;		/* State of button for display purposes:
				 * normal, active, or disabled. */
    int normalFg;		/* Foreground color in normal mode. */
    int normalBg;               /* Background color in normal mode. */
    int normalAttr;             /* Attributes in normal mode. */
    int activeFg;		/* Foreground color in active mode. */
    int activeBg;               /* Ditto, background color. */
    int activeAttr;		/* Attributes in active mode. */
    int disabledBg;             /* Background color when disabled. */
    int disabledFg;		/* Foreground color when disabled. */
    int disabledAttr;           /* Attributes when disabled. */
    int underline;              /* Index of underlined character, < 0 if
                                 * no underlining. */
    int underlineFg;            /* Foreground for underlined character. */
    int underlineAttr;          /* Attribute for underlined character. */
    int selectFg;		/* Foreground color for selector. */
    int width, height;		/* If > 0, these specify dimensions to request
				 * for window, in characters for text. */
    Ck_Anchor anchor;		/* Where text should be displayed
				 * inside button region. */

    /*
     * For check and radio buttons, the fields below are used
     * to manage the variable indicating the button's state.
     */

    char *selVarName;		/* Name of variable used to control selected
				 * state of button.  Malloc'ed (if
				 * not NULL). */
    char *onValue;		/* Value to store in variable when
				 * this button is selected.  Malloc'ed (if
				 * not NULL). */
    char *offValue;		/* Value to store in variable when this
				 * button isn't selected.  Malloc'ed
				 * (if not NULL).  Valid only for check
				 * buttons. */

    /*
     * Miscellaneous information:
     */

    char *command;		/* Command to execute when button is
				 * invoked; valid for buttons only.
				 * If not NULL, it's malloc-ed. */
    char *takeFocus;		/* Tk 4.0 like. */
    int flags;			/* Various flags;  see below for
				 * definitions. */
} Button;

/*
 * Possible "type" values for buttons.  These are the kinds of
 * widgets supported by this file.  The ordering of the type
 * numbers is significant:  greater means more features and is
 * used in the code.
 */

#define TYPE_LABEL		0
#define TYPE_BUTTON		1
#define TYPE_CHECK_BUTTON	2
#define TYPE_RADIO_BUTTON	3

/*
 * Class names for buttons, indexed by one of the type values above.
 */

static char *classNames[] = {"Label", "Button", "Checkbutton", "Radiobutton"};

/*
 * Flag bits for buttons:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 * SELECTED:			Non-zero means this button is selected,
 *				so special highlight should be drawn.
 */

#define REDRAW_PENDING		1
#define SELECTED		2

/*
 * Mask values used to selectively enable entries in the
 * configuration specs:
 */

#define LABEL_MASK		CK_CONFIG_USER_BIT
#define BUTTON_MASK		CK_CONFIG_USER_BIT << 1
#define CHECK_BUTTON_MASK	CK_CONFIG_USER_BIT << 2
#define RADIO_BUTTON_MASK	CK_CONFIG_USER_BIT << 3
#define ALL_MASK		(LABEL_MASK | BUTTON_MASK \
	| CHECK_BUTTON_MASK | RADIO_BUTTON_MASK)

static int configFlags[] = {LABEL_MASK, BUTTON_MASK,
	CHECK_BUTTON_MASK, RADIO_BUTTON_MASK};
/*
 * Information used for parsing configuration specs:
 */

static Ck_ConfigSpec configSpecs[] = {
    {CK_CONFIG_ATTR, "-activeattributes", "activeAttributes",
        "ActiveAttributes", DEF_BUTTON_ACTIVE_ATTR_COLOR,
        Ck_Offset(Button, activeAttr),
        BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_ATTR, "-activeattributes", "activeAttributes",
        "ActiveAttributes", DEF_BUTTON_ACTIVE_ATTR_MONO,
        Ck_Offset(Button, activeAttr),
        BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-activebackground", "activeBackground", "Foreground",
	DEF_BUTTON_ACTIVE_BG_COLOR, Ck_Offset(Button, activeBg),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-activebackground", "activeBackground", "Foreground",
	DEF_BUTTON_ACTIVE_BG_MONO, Ck_Offset(Button, activeBg),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_BUTTON_ACTIVE_FG_COLOR, Ck_Offset(Button, activeFg), 
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_BUTTON_ACTIVE_FG_MONO, Ck_Offset(Button, activeFg), 
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_ANCHOR, "-anchor", "anchor", "Anchor",
	DEF_BUTTON_ANCHOR, Ck_Offset(Button, anchor), ALL_MASK},
    {CK_CONFIG_ATTR, "-attributes", "attributes", "Attributes",
	DEF_BUTTON_ATTR, Ck_Offset(Button, normalAttr),
        BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK},
    {CK_CONFIG_ATTR, "-attributes", "attributes", "Attributes",
	DEF_LABEL_ATTR, Ck_Offset(Button, normalAttr), LABEL_MASK},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_BUTTON_BG_COLOR, Ck_Offset(Button, normalBg),
	ALL_MASK | CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_BUTTON_BG_MONO, Ck_Offset(Button, normalBg),
	ALL_MASK | CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, ALL_MASK},
    {CK_CONFIG_STRING, "-command", "command", "Command",
	DEF_BUTTON_COMMAND, Ck_Offset(Button, command),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CK_CONFIG_NULL_OK},
    {CK_CONFIG_ATTR, "-disabledattributes", "disabledAttributes",
        "DisabledAttributes", DEF_BUTTON_DISABLED_ATTR,
        Ck_Offset(Button, disabledAttr),
        BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK},
    {CK_CONFIG_COLOR, "-disabledbackground", "disabledBackground",
	"DisabledBackground", DEF_BUTTON_DISABLED_BG_COLOR,
	Ck_Offset(Button, disabledBg), BUTTON_MASK|CHECK_BUTTON_MASK
	|RADIO_BUTTON_MASK|CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-disabledbackground", "disabledBackground",
	"DisabledBackground", DEF_BUTTON_DISABLED_BG_MONO,
	Ck_Offset(Button, disabledBg),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
	"DisabledForeground", DEF_BUTTON_DISABLED_FG_COLOR,
	Ck_Offset(Button, disabledFg),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
	"DisabledForeground", DEF_BUTTON_DISABLED_FG_MONO,
	Ck_Offset(Button, disabledFg),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, ALL_MASK},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_BUTTON_FG, Ck_Offset(Button, normalFg), ALL_MASK},
    {CK_CONFIG_INT, "-height", "height", "Height",
	DEF_BUTTON_HEIGHT, Ck_Offset(Button, height), ALL_MASK},
    {CK_CONFIG_STRING, "-offvalue", "offValue", "Value",
	DEF_BUTTON_OFF_VALUE, Ck_Offset(Button, offValue),
	CHECK_BUTTON_MASK},
    {CK_CONFIG_STRING, "-onvalue", "onValue", "Value",
	DEF_BUTTON_ON_VALUE, Ck_Offset(Button, onValue),
	CHECK_BUTTON_MASK},
    {CK_CONFIG_COLOR, "-selectcolor", "selectColor", "Background",
	DEF_BUTTON_SELECT_COLOR, Ck_Offset(Button, selectFg),
        CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-selectcolor", "selectColor", "Background",
	DEF_BUTTON_SELECT_MONO, Ck_Offset(Button, selectFg),
        CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_UID, "-state", "state", "State",
	DEF_BUTTON_STATE, Ck_Offset(Button, state),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK},
    {CK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_BUTTON_TAKE_FOCUS, Ck_Offset(Button, takeFocus),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_LABEL_TAKE_FOCUS, Ck_Offset(Button, takeFocus),
	LABEL_MASK|CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-text", "text", "Text",
	DEF_BUTTON_TEXT, Ck_Offset(Button, text), ALL_MASK},
    {CK_CONFIG_STRING, "-textvariable", "textVariable", "Variable",
	DEF_BUTTON_TEXT_VARIABLE, Ck_Offset(Button, textVarName),
	ALL_MASK|CK_CONFIG_NULL_OK},
    {CK_CONFIG_INT, "-underline", "underline", "Underline",
	DEF_BUTTON_UNDERLINE, Ck_Offset(Button, underline),
	ALL_MASK},
    {CK_CONFIG_ATTR, "-underlineattributes", "underlineAttributes",
        "UnderlineAttributes", DEF_BUTTON_UNDERLINE_ATTR,
        Ck_Offset(Button, underlineAttr), ALL_MASK},
    {CK_CONFIG_COLOR, "-underlineforeground", "underlineForeground",
        "UnderlineForeground", DEF_BUTTON_UNDERLINE_FG_COLOR,
        Ck_Offset(Button, underlineFg), ALL_MASK|CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-underlineforeground", "underlineForeground",
        "UnderlineForeground", DEF_BUTTON_UNDERLINE_FG_MONO,
        Ck_Offset(Button, underlineFg), ALL_MASK|CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_STRING, "-value", "value", "Value",
	DEF_BUTTON_VALUE, Ck_Offset(Button, onValue),
	RADIO_BUTTON_MASK|CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-variable", "variable", "Variable",
	DEF_RADIOBUTTON_VARIABLE, Ck_Offset(Button, selVarName),
	RADIO_BUTTON_MASK},
    {CK_CONFIG_STRING, "-variable", "variable", "Variable",
	DEF_CHECKBUTTON_VARIABLE, Ck_Offset(Button, selVarName),
	CHECK_BUTTON_MASK|CK_CONFIG_NULL_OK},
    {CK_CONFIG_INT, "-width", "width", "Width",
	DEF_BUTTON_WIDTH, Ck_Offset(Button, width), ALL_MASK},
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * String to print out in error messages, identifying options for
 * widget commands for different types of labels or buttons:
 */

static char *optionStrings[] = {
    "cget or configure",
    "activate, cget, configure, deactivate, flash, or invoke",
    "activate, cget, configure, deactivate, deselect, flash, invoke, select, or toggle",
    "activate, cget, configure, deactivate, deselect, flash, invoke, or select"
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void             ButtonCmdDeletedProc _ANSI_ARGS_((
                            ClientData clientData));
static void		ButtonEventProc _ANSI_ARGS_((ClientData clientData,
			    CkEvent *eventPtr));
static char *		ButtonTextVarProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));
static char *		ButtonVarProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));
static int		ButtonWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static void		ComputeButtonGeometry _ANSI_ARGS_((Button *butPtr));
static int		ConfigureButton _ANSI_ARGS_((Tcl_Interp *interp,
			    Button *butPtr, int argc, char **argv,
			    int flags));
static void		DestroyButton _ANSI_ARGS_((ClientData clientData));
static void		DisplayButton _ANSI_ARGS_((ClientData clientData));
static int		InvokeButton  _ANSI_ARGS_((Button *butPtr));

/*
 *--------------------------------------------------------------
 *
 * Ck_ButtonCmd --
 *
 *	This procedure is invoked to process the "button", "label",
 *	"radiobutton", and "checkbutton" Tcl commands.  See the
 *	user documentation for details on what it does.
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
Ck_ButtonCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Button *butPtr;
    int type;
    CkWindow *winPtr = (CkWindow *) clientData;
    CkWindow *new;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args:  should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    switch (argv[0][0]) {
	case 'l':
	    type = TYPE_LABEL;
	    break;
	case 'b':
	    type = TYPE_BUTTON;
	    break;
	case 'c':
	    type = TYPE_CHECK_BUTTON;
	    break;
	case 'r':
	    type = TYPE_RADIO_BUTTON;
	    break;
	default:
	    sprintf(interp->result,
		    "unknown button-creation command \"%.50s\"", argv[0]);
	    return TCL_ERROR;
    }

    /*
     * Create the new window.
     */

    new = Ck_CreateWindowFromPath(interp, winPtr, argv[1], 0);
    if (new == NULL) {
	return TCL_ERROR;
    }

    /*
     * Initialize the data structure for the button.
     */

    butPtr = (Button *) ckalloc(sizeof (Button));
    butPtr->winPtr = new;
    butPtr->interp = interp;
    butPtr->widgetCmd = Tcl_CreateCommand(interp,
        butPtr->winPtr->pathName, ButtonWidgetCmd,
	    (ClientData) butPtr, ButtonCmdDeletedProc);
    butPtr->type = type;
    butPtr->text = NULL;
    butPtr->textLength = 0;
    butPtr->textVarName = NULL;
    butPtr->state = ckNormalUid;
    butPtr->normalFg = 0;
    butPtr->normalBg = 0;
    butPtr->normalAttr = 0;
    butPtr->activeFg = 0;
    butPtr->activeBg = 0;
    butPtr->activeAttr = 0;
    butPtr->disabledFg = 0;
    butPtr->disabledBg = 0;
    butPtr->disabledAttr = 0;
    butPtr->underline = -1;
    butPtr->underlineFg = 0;
    butPtr->underlineAttr = 0;
    butPtr->selectFg = 0;
    butPtr->width = 0;
    butPtr->height = 0;
    butPtr->anchor = CK_ANCHOR_CENTER;
    butPtr->selVarName = NULL;
    butPtr->onValue = NULL;
    butPtr->offValue = NULL;
    butPtr->command = NULL;
    butPtr->takeFocus = NULL;
    butPtr->flags = 0;

    Ck_SetClass(new, classNames[type]);
    Ck_CreateEventHandler(butPtr->winPtr,
	    CK_EV_EXPOSE | CK_EV_MAP | CK_EV_DESTROY,
	    ButtonEventProc, (ClientData) butPtr);
    if (ConfigureButton(interp, butPtr, argc-2, argv+2,
	    configFlags[type]) != TCL_OK) {
	Ck_DestroyWindow(butPtr->winPtr);
	return TCL_ERROR;
    }

    interp->result = butPtr->winPtr->pathName;
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ButtonWidgetCmd --
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
ButtonWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about button widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Button *butPtr = (Button *) clientData;
    int result = TCL_OK;
    int length;
    char c;

    if (argc < 2) {
	sprintf(interp->result,
		"wrong # args: should be \"%.50s option [arg arg ...]\"",
		argv[0]);
	return TCL_ERROR;
    }
    Ck_Preserve((ClientData) butPtr);
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'a') && (strncmp(argv[1], "activate", length) == 0)
	    && (butPtr->type != TYPE_LABEL)) {
	if (argc > 2) {
	    sprintf(interp->result,
		    "wrong # args: should be \"%.50s activate\"",
		    argv[0]);
	    goto error;
	}
	if (butPtr->state != ckDisabledUid) {
	    butPtr->state = ckActiveUid;
	    goto redisplay;
	}
    } else if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	&& (length >= 2)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "wrong # args: should be \"",
                    argv[0], " cget option\"",
                    (char *) NULL);
            goto error;
        }
        result = Ck_ConfigureValue(interp, butPtr->winPtr, configSpecs,
                (char *) butPtr, argv[2], configFlags[butPtr->type]);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)) {
	if (argc == 2) {
	    result = Ck_ConfigureInfo(interp, butPtr->winPtr, configSpecs,
		    (char *) butPtr, (char *) NULL, configFlags[butPtr->type]);
	} else if (argc == 3) {
	    result = Ck_ConfigureInfo(interp, butPtr->winPtr, configSpecs,
		    (char *) butPtr, argv[2],
		    configFlags[butPtr->type]);
	} else {
	    result = ConfigureButton(interp, butPtr, argc-2, argv+2,
		    configFlags[butPtr->type] | CK_CONFIG_ARGV_ONLY);
	}
    } else if ((c == 'd') && (strncmp(argv[1], "deactivate", length) == 0)
	    && (length > 2) && (butPtr->type != TYPE_LABEL)) {
	if (argc > 2) {
	    sprintf(interp->result,
		    "wrong # args: should be \"%.50s deactivate\"",
		    argv[0]);
	    goto error;
	}
	if (butPtr->state != ckDisabledUid) {
	    butPtr->state = ckNormalUid;
	    goto redisplay;
	}
    } else if ((c == 'd') && (strncmp(argv[1], "deselect", length) == 0)
	    && (length > 2) && (butPtr->type >= TYPE_CHECK_BUTTON)) {
	if (argc > 2) {
	    sprintf(interp->result,
		    "wrong # args: should be \"%.50s deselect\"",
		    argv[0]);
	    goto error;
	}
	if (butPtr->type == TYPE_CHECK_BUTTON) {
	    Tcl_SetVar(interp, butPtr->selVarName, butPtr->offValue,
		    TCL_GLOBAL_ONLY);
	} else if (butPtr->flags & SELECTED) {
	    Tcl_SetVar(interp, butPtr->selVarName, "", TCL_GLOBAL_ONLY);
	}
    } else if ((c == 'i') && (strncmp(argv[1], "invoke", length) == 0)
	    && (butPtr->type > TYPE_LABEL)) {
	if (argc > 2) {
	    sprintf(interp->result,
		    "wrong # args: should be \"%.50s invoke\"",
		    argv[0]);
	    goto error;
	}
	if (butPtr->state != ckDisabledUid) {
	    result = InvokeButton(butPtr);
	}
    } else if ((c == 's') && (strncmp(argv[1], "select", length) == 0)
	    && (butPtr->type >= TYPE_CHECK_BUTTON)) {
	if (argc > 2) {
	    sprintf(interp->result,
		    "wrong # args: should be \"%.50s select\"",
		    argv[0]);
	    goto error;
	}
	Tcl_SetVar(interp, butPtr->selVarName, butPtr->onValue, TCL_GLOBAL_ONLY);
    } else if ((c == 't') && (strncmp(argv[1], "toggle", length) == 0)
	    && (length >= 2) && (butPtr->type == TYPE_CHECK_BUTTON)) {
	if (argc > 2) {
	    sprintf(interp->result,
		    "wrong # args: should be \"%.50s select\"",
		    argv[0]);
	    goto error;
	}
	if (butPtr->flags & SELECTED) {
	    Tcl_SetVar(interp, butPtr->selVarName, butPtr->offValue, TCL_GLOBAL_ONLY);
	} else {
	    Tcl_SetVar(interp, butPtr->selVarName, butPtr->onValue, TCL_GLOBAL_ONLY);
	}
    } else {
	sprintf(interp->result,
		"bad option \"%.50s\":  must be %s", argv[1],
		optionStrings[butPtr->type]);
	goto error;
    }
    Ck_Release((ClientData) butPtr);
    return result;

    redisplay:
    if ((butPtr->winPtr->flags & CK_MAPPED) &&
        !(butPtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplayButton, (ClientData) butPtr);
	butPtr->flags |= REDRAW_PENDING;
    }
    Ck_Release((ClientData) butPtr);
    return TCL_OK;

    error:
    Ck_Release((ClientData) butPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyButton --
 *
 *	This procedure is invoked by Ck_EventuallyFree or Ck_Release
 *	to clean up the internal structure of a button at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the widget is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyButton(clientData)
    ClientData clientData;		/* Info about entry widget. */
{
    Button *butPtr = (Button *) clientData;

    /*
     * Free up all the stuff that requires special handling, then
     * let Tk_FreeOptions handle all the standard option-related
     * stuff.
     */

    if (butPtr->textVarName != NULL) {
	Tcl_UntraceVar(butPtr->interp, butPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ButtonTextVarProc, (ClientData) butPtr);
    }
    if (butPtr->selVarName != NULL) {
	Tcl_UntraceVar(butPtr->interp, butPtr->selVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ButtonVarProc, (ClientData) butPtr);
    }
    Ck_FreeOptions(configSpecs, (char *) butPtr, configFlags[butPtr->type]);
    ckfree((char *) butPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonCmdDeletedProc --
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
ButtonCmdDeletedProc(clientData)
    ClientData clientData;      /* Pointer to widget record for widget. */
{
    Button *butPtr = (Button *) clientData;
    CkWindow *winPtr = butPtr->winPtr;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case winPtr
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (winPtr != NULL) {
        butPtr->winPtr = NULL;
        Ck_DestroyWindow(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureButton --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a button widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for butPtr;  old resources get freed, if there
 *	were any.  The button is redisplayed.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureButton(interp, butPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Button *butPtr;             /* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    /*
     * Eliminate any existing trace on variables monitored by the button.
     */

    if (butPtr->textVarName != NULL) {
	Tcl_UntraceVar(interp, butPtr->textVarName, 
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ButtonTextVarProc, (ClientData) butPtr);
    }
    if (butPtr->selVarName != NULL) {
	Tcl_UntraceVar(interp, butPtr->selVarName, 
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ButtonVarProc, (ClientData) butPtr);
    }

    if (Ck_ConfigureWidget(interp, butPtr->winPtr, configSpecs,
	    argc, argv, (char *) butPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * A few options need special processing.
     */

    if (butPtr->state != ckActiveUid && butPtr->state != ckDisabledUid)
        butPtr->state = ckNormalUid;

    if (butPtr->type >= TYPE_CHECK_BUTTON) {
	char *value;

	if (butPtr->selVarName == NULL) {
	    butPtr->selVarName = (char *) ckalloc(
                strlen((char *) butPtr->winPtr->nameUid) + 1);
	    strcpy(butPtr->selVarName, (char *) butPtr->winPtr->nameUid);
	}
	if (butPtr->onValue == NULL) {
	    butPtr->onValue = (char *) ckalloc(
		strlen((char *) butPtr->winPtr->nameUid) + 1);
	    strcpy(butPtr->onValue, (char *) butPtr->winPtr->nameUid);
	}

	/*
	 * Select the button if the associated variable has the
	 * appropriate value, initialize the variable if it doesn't
	 * exist, then set a trace on the variable to monitor future
	 * changes to its value.
	 */

	value = Tcl_GetVar(interp, butPtr->selVarName, TCL_GLOBAL_ONLY);
	butPtr->flags &= ~SELECTED;
	if (value != NULL) {
	    if (strcmp(value, butPtr->onValue) == 0) {
		butPtr->flags |= SELECTED;
	    }
	} else {
	    Tcl_SetVar(interp, butPtr->selVarName,
		(butPtr->type == TYPE_CHECK_BUTTON) ? butPtr->offValue : "",
		TCL_GLOBAL_ONLY);
	}
	Tcl_TraceVar(interp, butPtr->selVarName,
	    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
	    ButtonVarProc, (ClientData) butPtr);
    }

    /*
     * If the button is to display the value of a variable, then set up
     * a trace on the variable's value, create the variable if it doesn't
     * exist, and fetch its current value.
     */

    if (butPtr->textVarName != NULL) {
	char *value;

	value = Tcl_GetVar(interp, butPtr->textVarName, TCL_GLOBAL_ONLY);
	if (value == NULL) {
	    Tcl_SetVar(interp, butPtr->textVarName,
		    butPtr->text != NULL ? butPtr->text : "",
		    TCL_GLOBAL_ONLY);
	} else {
	    if (butPtr->text != NULL) {
		ckfree(butPtr->text);
	    }
	    butPtr->text = ckalloc((unsigned) (strlen(value) + 1));
	    strcpy(butPtr->text, value);
	}
	Tcl_TraceVar(interp, butPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ButtonTextVarProc, (ClientData) butPtr);
    }

    ComputeButtonGeometry(butPtr);

    /*
     * Lastly, arrange for the button to be redisplayed.
     */

    if ((butPtr->winPtr->flags & CK_MAPPED)
        && !(butPtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplayButton, (ClientData) butPtr);
	butPtr->flags |= REDRAW_PENDING;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayButton --
 *
 *	This procedure is invoked to display a button widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to display the button in its
 *	current mode.
 *
 *----------------------------------------------------------------------
 */

static void
DisplayButton(clientData)
    ClientData clientData;	/* Information about widget. */
{
    Button *butPtr = (Button *) clientData;
    int x, y, fg, bg, attr, textWidth, charWidth;
    CkWindow *winPtr = butPtr->winPtr;

    butPtr->flags &= ~REDRAW_PENDING;
    if ((butPtr->winPtr == NULL) || !(winPtr->flags & CK_MAPPED)) {
	return;
    }

    if (butPtr->state == ckDisabledUid) {
        fg = butPtr->disabledFg;
        bg = butPtr->disabledBg;
        attr = butPtr->disabledAttr;
    } else if (butPtr->state == ckActiveUid) {
        fg = butPtr->activeFg;
        bg = butPtr->activeBg;
        attr = butPtr->activeAttr;
    } else {
        fg = butPtr->normalFg;
        bg = butPtr->normalBg;
        attr = butPtr->normalAttr;
    }

    /*
     * Display text for button.
     */

    if (butPtr->text != NULL)
	CkMeasureChars(winPtr->mainPtr, butPtr->text, butPtr->textLength,
	    0, winPtr->width, 0, CK_NEWLINES_NOT_SPECIAL | CK_IGNORE_TABS,
	    &textWidth, &charWidth);
    else
	textWidth = 0;

    switch (butPtr->anchor) {
	case CK_ANCHOR_NW: case CK_ANCHOR_W: case CK_ANCHOR_SW:
	    x = butPtr->type >= TYPE_CHECK_BUTTON ? 4 : 0;
	    break;
	case CK_ANCHOR_N: case CK_ANCHOR_CENTER: case CK_ANCHOR_S:
	    x = (winPtr->width - textWidth) / 2;
            if (butPtr->type >= TYPE_CHECK_BUTTON)
                x += 2;
	    break;
	default:
	    x = winPtr->width - textWidth;
            if (butPtr->type >= TYPE_CHECK_BUTTON && x < 4)
                x = 4;
	    break;
    }
    if (x + textWidth > winPtr->width)
	textWidth = winPtr->width - x;

    switch (butPtr->anchor) {
	case CK_ANCHOR_NW: case CK_ANCHOR_N: case CK_ANCHOR_NE:
	    y = 0;
	    break;
	case CK_ANCHOR_W: case CK_ANCHOR_CENTER: case CK_ANCHOR_E:
	    y = (winPtr->height - 1) / 2;
	    break;
	default:
	    y = winPtr->height - 1;
            if (y < 0)
                y = 0;
	    break;
    }

    Ck_SetWindowAttr(winPtr, fg, bg, attr);
    Ck_ClearToBot(winPtr, 0, 0);
    if (butPtr->text != NULL) {
	CkDisplayChars(winPtr->mainPtr,
	    winPtr->window, butPtr->text, charWidth, x, y,
	    0, CK_NEWLINES_NOT_SPECIAL | CK_IGNORE_TABS);
	if (butPtr->underline >= 0 && butPtr->state == ckNormalUid) {
	    Ck_SetWindowAttr(winPtr, butPtr->underlineFg, bg,
		butPtr->underlineAttr);
	    CkUnderlineChars(winPtr->mainPtr,
	        winPtr->window, butPtr->text, charWidth, x, y,
		0, CK_NEWLINES_NOT_SPECIAL | CK_IGNORE_TABS,
		butPtr->underline, butPtr->underline);
	    Ck_SetWindowAttr(winPtr, fg, bg, attr);
	}
    }
    if (butPtr->type >= TYPE_CHECK_BUTTON) {
	long gchar;

        mvwaddstr(winPtr->window, y, 0, butPtr->type == TYPE_CHECK_BUTTON ?
            "[ ]" : "( )");
	Ck_SetWindowAttr(winPtr, butPtr->selectFg, bg, attr);
        if (!(butPtr->flags & SELECTED)) {
            mvwaddch(winPtr->window, y, 1, (unsigned char) ' ');
        } else if (butPtr->type == TYPE_CHECK_BUTTON) {
	    Ck_GetGChar(butPtr->interp, "diamond", &gchar);
            mvwaddch(winPtr->window, y, 1, gchar);
        } else if (butPtr->type == TYPE_RADIO_BUTTON) {
	    Ck_GetGChar(butPtr->interp, "bullet", &gchar);
            mvwaddch(winPtr->window, y, 1, gchar);
        }
    }
    Ck_SetWindowAttr(winPtr, fg, bg, attr);
    wmove(winPtr->window, y, (butPtr->type >= TYPE_CHECK_BUTTON) ? 1 : x);
    Ck_EventuallyRefresh(winPtr);
}

/*
 *--------------------------------------------------------------
 *
 * ButtonEventProc --
 *
 *	This procedure is invoked by the dispatcher for various
 *	events on buttons.
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
ButtonEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    CkEvent *eventPtr;		/* Information about event. */
{
    Button *butPtr = (Button *) clientData;

    if (eventPtr->type == CK_EV_EXPOSE || eventPtr->type == CK_EV_MAP) {
	if ((butPtr->winPtr != NULL) && !(butPtr->flags & REDRAW_PENDING)) {
	    Tk_DoWhenIdle(DisplayButton, (ClientData) butPtr);
	    butPtr->flags |= REDRAW_PENDING;
	}
    } else if (eventPtr->type == CK_EV_DESTROY) {
        if (butPtr->winPtr != NULL) {
            butPtr->winPtr = NULL;
            Tcl_DeleteCommand(butPtr->interp,
                    Tcl_GetCommandName(butPtr->interp, butPtr->widgetCmd));
        }
	if (butPtr->flags & REDRAW_PENDING) {
	    Tk_CancelIdleCall(DisplayButton, (ClientData) butPtr);
	}
	Ck_EventuallyFree((ClientData) butPtr, (Ck_FreeProc *) DestroyButton);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeButtonGeometry --
 *
 *	After changes in a button's text or bitmap, this procedure
 *	recomputes the button's geometry and passes this information
 *	along to the geometry manager for the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The button's window may change size.
 *
 *----------------------------------------------------------------------
 */

static void
ComputeButtonGeometry(butPtr)
    Button *butPtr;	/* Button whose geometry may have changed. */
{
    int width, height, dummy;
    CkWindow *winPtr = butPtr->winPtr;

    butPtr->textLength = butPtr->text == NULL ? 0 : strlen(butPtr->text);
    if (butPtr->height > 0)
        height = butPtr->height;
    else
        height = 1;
    if (butPtr->width > 0)
        width = butPtr->width;
    else
	CkMeasureChars(winPtr->mainPtr,
	    butPtr->text == NULL ? "" : butPtr->text,
	    butPtr->textLength, 0, 100000, 0,
	    CK_NEWLINES_NOT_SPECIAL | CK_IGNORE_TABS,
	    &width, &dummy);

    /*
     * When issuing the geometry request, add extra space for the selector,
     * if any.
     */

    if (butPtr->type >= TYPE_CHECK_BUTTON)
        width += 4;

    Ck_GeometryRequest(butPtr->winPtr, width, height);
}

/*
 *----------------------------------------------------------------------
 *
 * InvokeButton --
 *
 *	This procedure is called to carry out the actions associated
 *	with a button, such as invoking a Tcl command or setting a
 *	variable.  This procedure is invoked, for example, when the
 *	button is invoked via the mouse.
 *
 * Results:
 *	A standard Tcl return value.  Information is also left in
 *	interp->result.
 *
 * Side effects:
 *	Depends on the button and its associated command.
 *
 *----------------------------------------------------------------------
 */

static int
InvokeButton(butPtr)
    Button *butPtr;		/* Information about button. */
{
    if (butPtr->type == TYPE_CHECK_BUTTON) {
	if (butPtr->flags & SELECTED) {
	    Tcl_SetVar(butPtr->interp, butPtr->selVarName, butPtr->offValue,
		    TCL_GLOBAL_ONLY);
	} else {
	    Tcl_SetVar(butPtr->interp, butPtr->selVarName, butPtr->onValue,
		    TCL_GLOBAL_ONLY);
	}
    } else if (butPtr->type == TYPE_RADIO_BUTTON) {
	Tcl_SetVar(butPtr->interp, butPtr->selVarName, butPtr->onValue,
		TCL_GLOBAL_ONLY);
    }
    if ((butPtr->type != TYPE_LABEL) && (butPtr->command != NULL)) {
	return CkCopyAndGlobalEval(butPtr->interp, butPtr->command);
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ButtonVarProc --
 *
 *	This procedure is invoked when someone changes the
 *	state variable associated with a radio button.  Depending
 *	on the new value of the button's variable, the button
 *	may be selected or deselected.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The button may become selected or deselected.
 *
 *--------------------------------------------------------------
 */

static char *
ButtonVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about button. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    Button *butPtr = (Button *) clientData;
    char *value;

    /*
     * If the variable is being unset, then just re-establish the
     * trace unless the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	butPtr->flags &= ~SELECTED;
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    Tcl_TraceVar2(interp, name1, name2,
		    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    ButtonVarProc, clientData);
	}
	goto redisplay;
    }

    /*
     * Use the value of the variable to update the selected status of
     * the button.
     */

    value = Tcl_GetVar2(interp, name1, name2, flags & TCL_GLOBAL_ONLY);
    if (strcmp(value, butPtr->onValue) == 0) {
	if (butPtr->flags & SELECTED) {
	    return (char *) NULL;
	}
	butPtr->flags |= SELECTED;
    } else if (butPtr->flags & SELECTED) {
	butPtr->flags &= ~SELECTED;
    } else {
	return (char *) NULL;
    }

 redisplay:
    if ((butPtr->winPtr != NULL) && (butPtr->winPtr->flags & CK_MAPPED)
	    && !(butPtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplayButton, (ClientData) butPtr);
	butPtr->flags |= REDRAW_PENDING;
    }
    return (char *) NULL;
}

/*
 *--------------------------------------------------------------
 *
 * ButtonTextVarProc --
 *
 *	This procedure is invoked when someone changes the variable
 *	whose contents are to be displayed in a button.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The text displayed in the button will change to match the
 *	variable.
 *
 *--------------------------------------------------------------
 */

static char *
ButtonTextVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about button. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    Button *butPtr = (Button *) clientData;
    char *value;

    /*
     * If the variable is unset, then immediately recreate it unless
     * the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    Tcl_SetVar2(interp, name1, name2,
		    butPtr->text != NULL ? butPtr->text : "",
		    flags & TCL_GLOBAL_ONLY);
	    Tcl_TraceVar2(interp, name1, name2,
		    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    ButtonTextVarProc, clientData);
	}
	return (char *) NULL;
    }

    value = Tcl_GetVar2(interp, name1, name2, flags & TCL_GLOBAL_ONLY);
    if (value == NULL) {
	value = "";
    }
    if (butPtr->text != NULL) {
	ckfree(butPtr->text);
    }
    butPtr->text = ckalloc((unsigned) (strlen(value) + 1));
    strcpy(butPtr->text, value);
    ComputeButtonGeometry(butPtr);

    if ((butPtr->winPtr != NULL) && (butPtr->winPtr->flags & CK_MAPPED)
	    && !(butPtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplayButton, (ClientData) butPtr);
	butPtr->flags |= REDRAW_PENDING;
    }
    return (char *) NULL;
}
