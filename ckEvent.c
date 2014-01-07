/* 
 * ckEvent.c --
 *
 *	This file provides basic event-managing facilities,
 *	whereby procedure callbacks may be attached to
 *	certain events.
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

#ifdef HAVE_GPM
#include "gpm.h"
#endif

#if (TCL_MAJOR_VERSION >= 8)
typedef struct {
    Tcl_Event header;		/* Standard event header. */
    CkEvent event;		/* Ck event data. */
    CkMainInfo *mainPtr;	/* Pointer to Ck main info. */
} CkQEvt;

static int	Ck_HandleQEvent _ANSI_ARGS_((Tcl_Event *evPtr, int flags));
#endif

/*
 * There's a potential problem if a handler is deleted while it's
 * current (i.e. its procedure is executing), since Ck_HandleEvent
 * will need to read the handler's "nextPtr" field when the procedure
 * returns.  To handle this problem, structures of the type below
 * indicate the next handler to be processed for any (recursively
 * nested) dispatches in progress.  The nextHandler fields get
 * updated if the handlers pointed to are deleted.  Ck_HandleEvent
 * also needs to know if the entire window gets deleted;  the winPtr
 * field is set to zero if that particular window gets deleted.
 */

typedef struct InProgress {
    CkEvent *eventPtr;		 /* Event currently being handled. */
    CkWindow *winPtr;		 /* Window for event.  Gets set to NULL if
				  * window is deleted while event is being
				  * handled. */
    CkEventHandler *nextHandler; /* Next handler in search. */
    struct InProgress *nextPtr;	 /* Next higher nested search. */
} InProgress;

static InProgress *pendingPtr = NULL;
				/* Topmost search in progress, or
				 * NULL if none. */

/*
 * For each call to Ck_CreateGenericHandler, an instance of the following
 * structure will be created.  All of the active handlers are linked into a
 * list.
 */

typedef struct GenericHandler {
    Ck_GenericProc *proc;	/* Procedure to dispatch on all events. */
    ClientData clientData;	/* Client data to pass to procedure. */
    int deleteFlag;		/* Flag to set when this handler is deleted. */
    struct GenericHandler *nextPtr;
				/* Next handler in list of all generic
				 * handlers, or NULL for end of list. */
} GenericHandler;

static GenericHandler *genericList = NULL;
				/* First handler in the list, or NULL. */
static GenericHandler *lastGenericPtr = NULL;
				/* Last handler in list. */

/*
 * There's a potential problem if Ck_HandleEvent is entered recursively.
 * A handler cannot be deleted physically until we have returned from
 * calling it.  Otherwise, we're looking at unallocated memory in advancing to
 * its `next' entry.  We deal with the problem by using the `delete flag' and
 * deleting handlers only when it's known that there's no handler active.
 *
 * The following variable has a non-zero value when a handler is active.
 */

static int genericHandlersActive = 0;

/*
 * For barcode readers an instance of the following structure is linked
 * to mainInfo. The supported DATA LOGIC barcode readers are connected
 * between PC keyboard and PC keyboard controller and generate a data
 * packet surrounded by start and end characters. If the start character
 * is received a timer is started and the following keystrokes are
 * collected into the buffer until the end character is received or the
 * timer expires.
 */

#define DEFAULT_BARCODE_TIMEOUT 1000

typedef struct barcodeData {
    Tk_TimerToken timer;/* Barcode packet timer. */
    int pkttime;	/* Timeout value. */
    int startChar;	/* Start of barcode packet character. */
    int endChar;	/* End of barcode packet character. */
    int delivered;	/* BarCode event has been delivered. */
    int index;		/* Current index into buffer. */
#if CK_USE_UTF
    char buffer[256];	/* Here the barcode packet is assembled. */
#else
    char buffer[128];	/* Here the barcode packet is assembled. */
#endif
} BarcodeData;

/*
 * Timeout procedure for reading barcode packet:
 */

static void BarcodeTimeout _ANSI_ARGS_((ClientData clientData));

/*
 *--------------------------------------------------------------
 *
 * Ck_CreateEventHandler --
 *
 *	Arrange for a given procedure to be invoked whenever
 *	events from a given class occur in a given window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	From now on, whenever an event of the type given by
 *	mask occurs for token and is processed by Ck_HandleEvent,
 *	proc will be called.  See the manual entry for details
 *	of the calling sequence and return value for proc.
 *
 *--------------------------------------------------------------
 */

void
Ck_CreateEventHandler(winPtr, mask, proc, clientData)
    CkWindow *winPtr;		/* Window in which to create handler. */
    long mask;			/* Events for which proc should be called. */
    Ck_EventProc *proc;		/* Procedure to call for each
				 * selected event */
    ClientData clientData;	/* Arbitrary data to pass to proc. */
{
    CkEventHandler *handlerPtr;
    int found;

    /*
     * Skim through the list of existing handlers to see if there's
     * already a handler declared with the same callback and clientData
     * (if so, just change the mask).  If no existing handler matches,
     * then create a new handler.
     */

    found = 0;
    if (winPtr->handlerList == NULL) {
	handlerPtr = (CkEventHandler *) ckalloc(sizeof (CkEventHandler));
	winPtr->handlerList = handlerPtr;
	goto initHandler;
    } else {
	for (handlerPtr = winPtr->handlerList; ;
		handlerPtr = handlerPtr->nextPtr) {
	    if ((handlerPtr->proc == proc)
		    && (handlerPtr->clientData == clientData)) {
		handlerPtr->mask = mask;
		found = 1;
	    }
	    if (handlerPtr->nextPtr == NULL) {
		break;
	    }
	}
    }

    /*
     * Create a new handler if no matching old handler was found.
     */

    if (!found) {
	handlerPtr->nextPtr = (CkEventHandler *) ckalloc(
	    sizeof (CkEventHandler));
	handlerPtr = handlerPtr->nextPtr;
initHandler:
	handlerPtr->mask = mask;
	handlerPtr->proc = proc;
	handlerPtr->clientData = clientData;
	handlerPtr->nextPtr = NULL;
    }
}

/*
 *--------------------------------------------------------------
 *
 * Ck_DeleteEventHandler --
 *
 *	Delete a previously-created handler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If there existed a handler as described by the
 *	parameters, the handler is deleted so that proc
 *	will not be invoked again.
 *
 *--------------------------------------------------------------
 */

void
Ck_DeleteEventHandler(winPtr, mask, proc, clientData)
    CkWindow *winPtr;		/* Same as corresponding arguments passed */
    long mask;			/* previously to Ck_CreateEventHandler. */
    Ck_EventProc *proc;
    ClientData clientData;
{
    CkEventHandler *handlerPtr;
    InProgress *ipPtr;
    CkEventHandler *prevPtr;

    /*
     * Find the event handler to be deleted, or return
     * immediately if it doesn't exist.
     */

    for (handlerPtr = winPtr->handlerList, prevPtr = NULL; ;
	    prevPtr = handlerPtr, handlerPtr = handlerPtr->nextPtr) {
	if (handlerPtr == NULL) {
	    return;
	}
	if ((handlerPtr->mask == mask) && (handlerPtr->proc == proc)
		&& (handlerPtr->clientData == clientData)) {
	    break;
	}
    }

    /*
     * If Ck_HandleEvent is about to process this handler, tell it to
     * process the next one instead.
     */

    for (ipPtr = pendingPtr; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	if (ipPtr->nextHandler == handlerPtr) {
	    ipPtr->nextHandler = handlerPtr->nextPtr;
	}
    }

    /*
     * Free resources associated with the handler.
     */

    if (prevPtr == NULL) {
	winPtr->handlerList = handlerPtr->nextPtr;
    } else {
	prevPtr->nextPtr = handlerPtr->nextPtr;
    }
    ckfree((char *) handlerPtr);
}

/*--------------------------------------------------------------
 *
 * Ck_CreateGenericHandler --
 *
 *	Register a procedure to be called on each event, regardless
 *	of window.  Generic handlers are useful for capturing
 *	events that aren't associated with windows, or events for windows
 *	not managed by Ck.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	From now on, whenever an event is given to Ck_HandleEvent,
 *	invoke proc, giving it clientData and the event as arguments.
 *
 *--------------------------------------------------------------
 */

void
Ck_CreateGenericHandler(proc, clientData)
     Ck_GenericProc *proc;	/* Procedure to call on every event. */
     ClientData clientData;	/* One-word value to pass to proc. */
{
    GenericHandler *handlerPtr;
    
    handlerPtr = (GenericHandler *) ckalloc (sizeof (GenericHandler));
    
    handlerPtr->proc = proc;
    handlerPtr->clientData = clientData;
    handlerPtr->deleteFlag = 0;
    handlerPtr->nextPtr = NULL;
    if (genericList == NULL) {
	genericList = handlerPtr;
    } else {
	lastGenericPtr->nextPtr = handlerPtr;
    }
    lastGenericPtr = handlerPtr;
}

/*
 *--------------------------------------------------------------
 *
 * Ck_DeleteGenericHandler --
 *
 *	Delete a previously-created generic handler.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	If there existed a handler as described by the parameters,
 *	that handler is logically deleted so that proc will not be
 *	invoked again.  The physical deletion happens in the event
 *	loop in Ck_HandleEvent.
 *
 *--------------------------------------------------------------
 */

void
Ck_DeleteGenericHandler(proc, clientData)
     Ck_GenericProc *proc;
     ClientData clientData;
{
    GenericHandler * handler;
    
    for (handler = genericList; handler; handler = handler->nextPtr) {
	if ((handler->proc == proc) && (handler->clientData == clientData)) {
	    handler->deleteFlag = 1;
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * Ck_HandleEvent --
 *
 *	Given an event, invoke all the handlers that have
 *	been registered for the event.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on the handlers.
 *
 *--------------------------------------------------------------
 */

void
Ck_HandleEvent(mainPtr, eventPtr)
    CkMainInfo *mainPtr;
    CkEvent *eventPtr;		/* Event to dispatch. */
{
    CkEventHandler *handlerPtr;
    GenericHandler *genericPtr;
    GenericHandler *genPrevPtr;
    CkWindow *winPtr;
    InProgress ip;

    /* 
     * Invoke all the generic event handlers (those that are
     * invoked for all events).  If a generic event handler reports that
     * an event is fully processed, go no further.
     */

    for (genPrevPtr = NULL, genericPtr = genericList;  genericPtr != NULL; ) {
	if (genericPtr->deleteFlag) {
	    if (!genericHandlersActive) {
		GenericHandler *tmpPtr;

		/*
		 * This handler needs to be deleted and there are no
		 * calls pending through the handler, so now is a safe
		 * time to delete it.
		 */

		tmpPtr = genericPtr->nextPtr;
		if (genPrevPtr == NULL) {
		    genericList = tmpPtr;
		} else {
		    genPrevPtr->nextPtr = tmpPtr;
		}
		if (tmpPtr == NULL) {
		    lastGenericPtr = genPrevPtr;
		}
		(void) ckfree((char *) genericPtr);
		genericPtr = tmpPtr;
		continue;
	    }
	} else {
	    int done;

	    genericHandlersActive++;
	    done = (*genericPtr->proc)(genericPtr->clientData, eventPtr);
	    genericHandlersActive--;
	    if (done) {
		return;
	    }
	}
	genPrevPtr = genericPtr;
	genericPtr = genPrevPtr->nextPtr;
    }

    if (Tcl_FindHashEntry(&mainPtr->winTable, (char *) eventPtr->any.winPtr)
	== NULL) {
	/*
	 * There isn't a CkWindow structure for this window.
	 */
	return;
    }
    winPtr = eventPtr->any.winPtr;
    ip.eventPtr = eventPtr;
    ip.winPtr = winPtr;
    ip.nextHandler = NULL;
    ip.nextPtr = pendingPtr;
    pendingPtr = &ip;
    for (handlerPtr = winPtr->handlerList; handlerPtr != NULL; ) {
	if ((handlerPtr->mask & eventPtr->type) != 0) {
	    ip.nextHandler = handlerPtr->nextPtr;
	    (*(handlerPtr->proc))(handlerPtr->clientData, eventPtr);
	    handlerPtr = ip.nextHandler;
	} else {
	    handlerPtr = handlerPtr->nextPtr;
	}
    }

    /*
     * Pass the event to the "bind" command mechanism.
     */

    CkBindEventProc(winPtr, eventPtr);

    pendingPtr = ip.nextPtr;
}

/*
 *--------------------------------------------------------------
 *
 * CkEventDeadWindow --
 *
 *	This procedure is invoked when it is determined that
 *	a window is dead.  It cleans up event-related information
 *	about the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Various things get cleaned up and recycled.
 *
 *--------------------------------------------------------------
 */

void
CkEventDeadWindow(winPtr)
    CkWindow *winPtr;		/* Information about the window
				 * that is being deleted. */
{
    CkEventHandler *handlerPtr;
    InProgress *ipPtr;

    /*
     * While deleting all the handlers, be careful to check for
     * Ck_HandleEvent being about to process one of the deleted
     * handlers.  If it is, tell it to quit (all of the handlers
     * are being deleted).
     */

    while (winPtr->handlerList != NULL) {
	handlerPtr = winPtr->handlerList;
	winPtr->handlerList = handlerPtr->nextPtr;
	for (ipPtr = pendingPtr; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	    if (ipPtr->nextHandler == handlerPtr) {
		ipPtr->nextHandler = NULL;
	    }
	    if (ipPtr->winPtr == winPtr) {
		ipPtr->winPtr = NULL;
	    }
	}
	ckfree((char *) handlerPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * CkHandleInput --
 *
 *	Process keyboard events from curses.
 *
 * Results:
 *	The return value is TK_FILE_HANDLED if the procedure
 *	actually found an event to process.  If no event was found
 *	then TK_READABLE is returned.
 *
 * Side effects:
 *	The handling of the event could cause additional
 *	side effects.
 *
 *--------------------------------------------------------------
 */

#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
int
CkHandleInput(clientData, mask, flags)
    ClientData clientData;      /* Pointer to main info. */
    int mask;                   /* OR-ed combination of the bits TK_READABLE,
                                 * TK_WRITABLE, and TK_EXCEPTION, indicating
                                 * current state of file. */
    int flags;                  /* Flag bits passed to Tk_DoOneEvent;
                                 * contains bits such as TK_DONT_WAIT,
                                 * TK_X_EVENTS, Tk_FILE_EVENTS, etc. */
{
    CkEvent event;
    CkMainInfo *mainPtr = (CkMainInfo *) clientData;
    int code;
    static int buttonpressed = 0;
    static int errCount = 0;

    if (!(flags & TK_FILE_EVENTS))
	return 0;

    if (!(mask & TK_READABLE))
	return TK_READABLE;

    code = getch();
    if (code == ERR) {
	if (++errCount > 100) {
	    Tcl_Eval(mainPtr->interp, "exit 99");
	    exit(99);			/* just in case */
	}
	return TK_READABLE;
    }
    errCount = 0;

    /*
     * Barcode reader handling.
     */

    if (mainPtr->flags & CK_HAS_BARCODE) {
	BarcodeData *bd = (BarcodeData *) mainPtr->barcodeData;

	/*
	 * Here, special handling for nested event loops:
	 * If BarCode event has been delivered already, we must
	 * reset the buffer index in order to get normal Key events.
	 */
	if (bd->delivered && bd->index >= 0) {
	    bd->delivered = 0;
	    bd->index = -1;
	}

	if (bd->index >= 0 || code == bd->startChar) {
	    if (code == bd->startChar) {
		Tk_DeleteTimerHandler(bd->timer);
		bd->timer = Tk_CreateTimerHandler(bd->pkttime, BarcodeTimeout,
		    (ClientData) mainPtr);
		bd->index = 0;
	    } else if (code == bd->endChar) {
		Tk_DeleteTimerHandler(bd->timer);
		bd->timer = (Tk_TimerToken) NULL;
		bd->delivered = 1;
		event.key.type = CK_EV_BARCODE;
		event.key.winPtr = mainPtr->focusPtr;
		event.key.keycode = 0;
		Ck_HandleEvent(mainPtr, &event);
		/*
		 * Careful, event handler could turn barcode off.
		 * Only reset buffer index if BarCode event delivered
		 * flag is set.
		 */
		bd = (BarcodeData *) mainPtr->barcodeData;
		if (bd != NULL && bd->delivered) {
		    bd->delivered = 0;
		    bd->index = -1;
		}
		return TK_FILE_HANDLED;
	    } else {
		/* Leave space for one NUL byte. */
		if (bd->index < sizeof (bd->buffer) - 1)
		    bd->buffer[bd->index] = code;
		bd->index++;
	    }
	    return TK_READABLE;
	}
    }

#ifdef NCURSES_MOUSE_VERSION
    /*
     * ncurses-1.9.8a has builtin mouse support for at least xterm.
     */

    if (code == KEY_MOUSE) {
        MEVENT mEvent;
	int i;

	if (mainPtr->flags & CK_MOUSE_XTERM) {
	    goto getMouse;
	}

	if (getmouse(&mEvent) == ERR)
	    return TK_FILE_HANDLED;

	for (i = 1; i <= 3; i++) {
	    if (BUTTON_PRESS(mEvent.bstate, i)) {
		event.mouse.type = CK_EV_MOUSE_DOWN;
		goto mouseEventNC;
	    } else if (BUTTON_RELEASE(mEvent.bstate, i)) {
		event.mouse.type = CK_EV_MOUSE_UP;
mouseEventNC:
	        event.mouse.button = i;
		event.mouse.x = mEvent.x;
		event.mouse.y = mEvent.y;
		event.mouse.winPtr = Ck_GetWindowXY(mainPtr, &event.mouse.x,
		    &event.mouse.y, 1);
		Ck_HandleEvent(mainPtr, &event);
		return TK_FILE_HANDLED;
	    }
	}
    }
#endif

#ifdef __WIN32__
    if ((mainPtr->flags & CK_HAS_MOUSE) && code == KEY_MOUSE) {
	int i;

	request_mouse_pos();
	for (i = 0; i < 3; i++) {
	    if (Mouse_status.button[i] == BUTTON_PRESSED) {
		event.mouse.type = CK_EV_MOUSE_DOWN;
		goto mouseEvt;
	    } else if (Mouse_status.button[i] == BUTTON_RELEASED) {
		event.mouse.type = CK_EV_MOUSE_UP;
mouseEvt:
	        event.mouse.button = i + 1;
		event.mouse.x = Mouse_status.x;
		event.mouse.y = Mouse_status.y;
		event.mouse.winPtr = Ck_GetWindowXY(mainPtr, &event.mouse.x,
		    &event.mouse.y, 1);
		Ck_HandleEvent(mainPtr, &event);
		return TK_FILE_HANDLED;
	    }
	}
    }
#endif 

    /*
     * Xterm mouse report handling: Although GPM has an xterm module
     * this is separately done here, since I want to be as independent
     * as possible from GPM.
     * It is assumed that the entire mouse report comes in one piece
     * ie without any delay between the 6 relevant characters.
     * Only a single button down/up event is generated.
     */

#ifndef __WIN32__
    if ((mainPtr->flags & CK_MOUSE_XTERM) && (code == 0x1b || code == 0x9b)) {
	int code2;

	if (code == 0x9b)
	    goto getM;
	code2 = getch();
	if (code2 != ERR) {
	    if (code2 == '[')
		goto getM;
	    ungetch(code2);
	} else
	    errCount++;
	goto keyEvent;
getM:
	code2 = getch();
	if (code2 != ERR) {
	    if (code2 == 'M')
		goto getMouse;
	    ungetch(code2);
	} else
	    errCount++;
	goto keyEvent;
getMouse:
	code2 = getch();
	if (code2 == ERR) {
	    errCount++;
	    return TK_READABLE;
	}
	event.mouse.button = ((code2 - 0x20) & 0x03) + 1;
	code2 = getch();
	if (code2 == ERR) {
	    errCount++;
	    return TK_READABLE;
	}
	event.mouse.x = event.mouse.rootx = code2 - 0x20 - 1;
	code2 = getch();
	if (code2 == ERR) {
	    errCount++;
	    return TK_READABLE;
	}
	event.mouse.y = event.mouse.rooty = code2 - 0x20 - 1;
	if (event.mouse.button > 3) {
	    event.mouse.button = buttonpressed;
	    buttonpressed = 0;
	    event.mouse.type = CK_EV_MOUSE_UP;
	    goto mouseEvent;
	} else if (buttonpressed == 0) {
	    buttonpressed = event.mouse.button;
	    event.mouse.type = CK_EV_MOUSE_DOWN;
mouseEvent:
	    event.mouse.winPtr = Ck_GetWindowXY(mainPtr, &event.mouse.x,
	        &event.mouse.y, 1);
	    Ck_HandleEvent(mainPtr, &event);
	    return TK_FILE_HANDLED;
	}
	return TK_READABLE;
    }
#endif

keyEvent:
    event.key.type = CK_EV_KEYPRESS;
    event.key.winPtr = mainPtr->focusPtr;
    event.key.keycode = code;
    if (event.key.keycode < 0)
	event.key.keycode &= 0xff;
    Ck_HandleEvent(mainPtr, &event);
    return TK_FILE_HANDLED;
}
#else
void
CkHandleInput(clientData, mask)
    ClientData clientData;      /* Pointer to main info. */
    int mask;                   /* OR-ed combination of the bits TK_READABLE,
                                 * TK_WRITABLE, and TK_EXCEPTION, indicating
                                 * current state of file. */
{
    CkEvent event;
    CkQEvt *qev;
    CkMainInfo *mainPtr = (CkMainInfo *) clientData;
    int code;
    static int buttonpressed = 0;
    static int errCount = 0;
#if CK_USE_UTF
    int ucp = 0;
    char ucbuf[16];
    Tcl_UniChar uch = 0;
#endif

    if (!(mask & TCL_READABLE))
	return;

    code = getch();
    if (code == ERR) {
	if (++errCount > 100) {
	    Tcl_Eval(mainPtr->interp, "exit 99");
#if (TCL_MAJOR_VERSION >= 8)
	    Tcl_Exit(99);			/* just in case */
#else
	    exit(99);				/* just in case */
#endif
	}
	return;
    }
    errCount = 0;
#if CK_USE_UTF
    if (mainPtr->isoEncoding == NULL && code >= 0xc0 && code < 0x100) {
	int need = 2;

	if (code >= 0xfc)
	    need = 6;
	else if (code >= 0xf8)
	    need = 5;
	else if (code >= 0xf0)
	    need = 4;
	else if (code >= 0xe0)
	    need = 3;
	nodelay(curscr, FALSE);
	while (need-- > 0 && code >= 0x80 && code < 0x100) {
	    ucbuf[ucp++] = code;
	    code = getch();
	    if (code == ERR)
		break;
	    if (code < 0x80 || code >= 0xc0) {
		ungetch(code);
		break;
	    }
	}
	nodelay(curscr, TRUE);
	if (code >= 0x100 && code != ERR)
	    ungetch(code);
	ucbuf[ucp] = '\0';
	Tcl_UtfToUniChar(ucbuf, &uch);
	code = 0;
    }
#endif

    /*
     * Barcode reader handling.
     */

    if (mainPtr->flags & CK_HAS_BARCODE) {
	BarcodeData *bd = (BarcodeData *) mainPtr->barcodeData;

	/*
	 * Here, special handling for nested event loops:
	 * If BarCode event has been delivered already, we must
	 * reset the buffer index in order to get normal Key events.
	 */
	if (bd->delivered && bd->index >= 0) {
	    bd->delivered = 0;
	    bd->index = -1;
	}

	if (bd->index >= 0 || code == bd->startChar) {
	    if (code == bd->startChar) {
		Tk_DeleteTimerHandler(bd->timer);
		bd->timer = Tk_CreateTimerHandler(bd->pkttime, BarcodeTimeout,
		    (ClientData) mainPtr);
		bd->index = 0;
	    } else if (code == bd->endChar) {
		Tk_DeleteTimerHandler(bd->timer);
		bd->timer = (Tk_TimerToken) NULL;
		bd->delivered = 1;
		event.key.type = CK_EV_BARCODE;
		event.key.winPtr = mainPtr->focusPtr;
		event.key.keycode = 0;
		Ck_HandleEvent(mainPtr, &event);
		/*
		 * Careful, event handler could turn barcode off.
		 * Only reset buffer index if BarCode event delivered
		 * flag is set.
		 */
		bd = (BarcodeData *) mainPtr->barcodeData;
		if (bd != NULL && bd->delivered) {
		    bd->delivered = 0;
		    bd->index = -1;
		}
		return;
	    } else {
		/* Leave space for one NUL byte. */
		if (bd->index < sizeof (bd->buffer) - 1) {
#if CK_USE_UTF
		    char c, utfb[8];
		    int numc, i;

		    c = code;
		    if (mainPtr->isoEncoding) {
			Tcl_ExternalToUtf(NULL, mainPtr->isoEncoding,
			    &c, 1, 0, NULL, utfb, sizeof (utfb),
			    NULL, &numc, NULL);
			if (bd->index + numc < sizeof (bd->buffer) - 1) {
			    for (i = 0; i < numc; i++)
				bd->buffer[bd->index + i] = utfb[i];
			} else
			    bd->buffer[bd->index] = '\0';
			bd->index += numc - 1;
		    } else {
			if (code != 0) {
			    bd->buffer[bd->index] = code;
			} else if (uch < 0x100) {
			    bd->buffer[bd->index] = uch;
			}
		    }
#else
		    bd->buffer[bd->index] = code;
#endif
		}
		bd->index++;
	    }
	    return;
	}
    }

#ifdef NCURSES_MOUSE_VERSION
    /*
     * ncurses-1.9.8a has builtin mouse support for at least xterm.
     */

    if (code == KEY_MOUSE) {
        MEVENT mEvent;
	int i;

	if (mainPtr->flags & CK_MOUSE_XTERM) {
	    goto getMouse;
	}

        if (getmouse(&mEvent) == ERR)
	    return;

	for (i = 1; i <= 3; i++) {
	    if (BUTTON_PRESS(mEvent.bstate, i)) {
		event.mouse.type = CK_EV_MOUSE_DOWN;
		goto mouseEventNC;
	    } else if (BUTTON_RELEASE(mEvent.bstate, i)) {
		event.mouse.type = CK_EV_MOUSE_UP;
mouseEventNC:
	        event.mouse.button = i;
		event.mouse.rootx = mEvent.x;
		event.mouse.rooty = mEvent.y;
		event.mouse.x = mEvent.x;
		event.mouse.y = mEvent.y;
		event.mouse.winPtr = Ck_GetWindowXY(mainPtr, &event.mouse.x,
		    &event.mouse.y, 1);
		goto mkEvent;
	    }
	}
    }
#endif

#ifdef __WIN32__
    if ((mainPtr->flags & CK_HAS_MOUSE) && code == KEY_MOUSE) {
	int i;

	request_mouse_pos();
	for (i = 0; i < 3; i++) {
	    if (Mouse_status.button[i] == BUTTON_PRESSED) {
		event.mouse.type = CK_EV_MOUSE_DOWN;
		goto mouseEvt;
	    } else if (Mouse_status.button[i] == BUTTON_RELEASED) {
		event.mouse.type = CK_EV_MOUSE_UP;
mouseEvt:
	        event.mouse.button = i + 1;
		event.mouse.x = Mouse_status.x;
		event.mouse.y = Mouse_status.y;
		event.mouse.winPtr = Ck_GetWindowXY(mainPtr, &event.mouse.x,
		    &event.mouse.y, 1);
		goto mkEvent;
	    }
	}
    }
#endif

    /*
     * Xterm mouse report handling: Although GPM has an xterm module
     * this is separately done here, since I want to be as independent
     * as possible from GPM.
     * It is assumed that the entire mouse report comes in one piece
     * ie without any delay between the 6 relevant characters.
     * Only a single button down/up event is generated.
     */

#ifndef __WIN32__
    if ((mainPtr->flags & CK_MOUSE_XTERM) && (code == 0x1b || code == 0x9b)) {
	int code2;

	if (code == 0x9b)
	    goto getM;
	code2 = getch();
	if (code2 != ERR) {
	    if (code2 == '[')
		goto getM;
	    ungetch(code2);
	} else
	    errCount++;
	goto keyEvent;
getM:
	code2 = getch();
	if (code2 != ERR) {
	    if (code2 == 'M')
		goto getMouse;
	    ungetch(code2);
	} else
	    errCount++;
	goto keyEvent;
getMouse:
	code2 = getch();
	if (code2 == ERR) {
	    errCount++;
	    return;
	}
	event.mouse.button = ((code2 - 0x20) & 0x03) + 1;
	code2 = getch();
	if (code2 == ERR) {
	    errCount++;
	    return;
	}
	event.mouse.x = event.mouse.rootx = code2 - 0x20 - 1;
	code2 = getch();
	if (code2 == ERR) {
	    errCount++;
	    return;
	}
	event.mouse.y = event.mouse.rooty = code2 - 0x20 - 1;
	if (event.mouse.button > 3) {
	    event.mouse.button = buttonpressed;
	    buttonpressed = 0;
	    event.mouse.type = CK_EV_MOUSE_UP;
	    goto mouseEvent;
	} else if (buttonpressed == 0) {
	    buttonpressed = event.mouse.button;
	    event.mouse.type = CK_EV_MOUSE_DOWN;
mouseEvent:
	    event.mouse.winPtr = Ck_GetWindowXY(mainPtr, &event.mouse.x,
	        &event.mouse.y, 1);
	    goto mkEvent;
	}
	return;
    }
#endif

keyEvent:
    event.key.type = CK_EV_KEYPRESS;
    event.key.winPtr = mainPtr->focusPtr;
    event.key.keycode = code;
    if (event.key.keycode < 0)
	event.key.keycode &= 0xff;
#if CK_USE_UTF
    event.key.is_uch = 0;
    if (ucp > 0) {
	event.key.is_uch = 1;
	event.key.keycode = 0;
	event.key.uch = uch;
    } else
	event.key.uch = code;
    if (mainPtr->isoEncoding == NULL &&
	(code >= 0x20 && code < 0x100))
	event.key.is_uch = 1;
#endif

mkEvent:
    qev = (CkQEvt *) ckalloc(sizeof (CkQEvt));
    qev->header.proc = Ck_HandleQEvent;
    qev->event = event;
    qev->mainPtr = mainPtr;
    Tcl_QueueEvent(&qev->header, TCL_QUEUE_TAIL);
}

static int
Ck_HandleQEvent(evPtr, flags)
    Tcl_Event *evPtr;
    int flags;
{
    CkQEvt *qev = (CkQEvt *) evPtr;

    if (!(flags & TCL_WINDOW_EVENTS)) {
	return 0;
    }
    Ck_HandleEvent(qev->mainPtr, &qev->event);
    return 1;
}
#endif /* TCL_MAJOR_VERSION == 7 && TCL_MINOR_VERSION <= 4 */

#ifdef HAVE_GPM
/*
 *--------------------------------------------------------------
 *
 * CkHandleGPMInput --
 *
 *	Process mouse events from GPM.
 *
 * Results:
 *	The return value is TK_FILE_HANDLED if the procedure
 *	actually found an event to process.  If no event was found
 *	then TK_READABLE is returned.
 *
 * Side effects:
 *	The handling of the event could cause additional
 *	side effects.
 *
 *--------------------------------------------------------------
 */

#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
int
CkHandleGPMInput(clientData, mask, flags)
    ClientData clientData;      /* Pointer to main info. */
    int mask;                   /* OR-ed combination of the bits TK_READABLE,
                                 * TK_WRITABLE, and TK_EXCEPTION, indicating
                                 * current state of file. */
    int flags;                  /* Flag bits passed to Tk_DoOneEvent;
                                 * contains bits such as TK_DONT_WAIT,
                                 * TK_X_EVENTS, Tk_FILE_EVENTS, etc. */
{
    Gpm_Event gpmEvent;
    CkEvent event;
    CkMainInfo *mainPtr = (CkMainInfo *) clientData;
    int ret, type;

    if (!(flags & TK_FILE_EVENTS))
	return 0;

    if (!(mask & TK_READABLE))
	return TK_READABLE;

    ret = Gpm_GetEvent(&gpmEvent);
    if (ret == 0) {
	/*
	 * GPM connection is closed; delete this file handler.
	 */

	Tk_DeleteFileHandler((int) mainPtr->mouseData);
	mainPtr->mouseData = (ClientData) -1;
	return 0;
    } else if (ret == -1)
	return TK_READABLE;

    GPM_DRAWPOINTER(&gpmEvent);
    type = gpmEvent.type & (GPM_DOWN | GPM_UP);
    if (type == GPM_DOWN || type == GPM_UP) {
	event.mouse.type = type == GPM_DOWN ? CK_EV_MOUSE_DOWN :
	    CK_EV_MOUSE_UP;
	if (gpmEvent.buttons & GPM_B_LEFT)
	    event.mouse.button = 1;
	else if (gpmEvent.buttons & GPM_B_MIDDLE)
	    event.mouse.button = 2;
	else if (gpmEvent.buttons & GPM_B_RIGHT)
	    event.mouse.button = 3;
	event.mouse.x = event.mouse.rootx = gpmEvent.x - 1;
	event.mouse.y = event.mouse.rooty = gpmEvent.y - 1;
	event.mouse.winPtr = Ck_GetWindowXY(mainPtr, &event.mouse.x,
	    &event.mouse.y, 1);
	Ck_HandleEvent(mainPtr, &event);
	return TK_FILE_HANDLED;
    }
    return TK_READABLE;
}
#else
void
CkHandleGPMInput(clientData, mask)
    ClientData clientData;      /* Pointer to main info. */
    int mask;                   /* OR-ed combination of the bits TK_READABLE,
                                 * TK_WRITABLE, and TK_EXCEPTION, indicating
                                 * current state of file. */
{
    Gpm_Event gpmEvent;
    CkEvent event;
    CkQEvt *qev;
    CkMainInfo *mainPtr = (CkMainInfo *) clientData;
    int ret, type;

    if (!(mask & TCL_READABLE))
	return;

    ret = Gpm_GetEvent(&gpmEvent);
    if (ret == 0) {
	/*
	 * GPM connection is closed; delete this file handler.
	 */
#if (TCL_MAJOR_VERSION == 7) 
	Tcl_DeleteFileHandler((Tcl_File) mainPtr->mouseData);
#else
	Tcl_DeleteFileHandler((int) mainPtr->mouseData);
#endif
	mainPtr->mouseData = (ClientData) 0;
	return;
    } else if (ret == -1)
	return;

    GPM_DRAWPOINTER(&gpmEvent);
    type = gpmEvent.type & (GPM_DOWN | GPM_UP);
    if (type == GPM_DOWN || type == GPM_UP) {
	event.mouse.type = type == GPM_DOWN ? CK_EV_MOUSE_DOWN :
	    CK_EV_MOUSE_UP;
	if (gpmEvent.buttons & GPM_B_LEFT)
	    event.mouse.button = 1;
	else if (gpmEvent.buttons & GPM_B_MIDDLE)
	    event.mouse.button = 2;
	else if (gpmEvent.buttons & GPM_B_RIGHT)
	    event.mouse.button = 3;
	event.mouse.x = event.mouse.rootx = gpmEvent.x - 1;
	event.mouse.y = event.mouse.rooty = gpmEvent.y - 1;
	event.mouse.winPtr = Ck_GetWindowXY(mainPtr, &event.mouse.x,
	    &event.mouse.y, 1);
	qev = (CkQEvt *) ckalloc(sizeof (CkQEvt));
	qev->header.proc = Ck_HandleQEvent;
	qev->event = event;
	qev->mainPtr = mainPtr;
	Tcl_QueueEvent(&qev->header, TCL_QUEUE_TAIL);
    }
}
#endif /* TCL_MAJOR_VERSION == 7 && TCL_MINOR_VERSION <= 4 */
#endif /* HAVE_GPM */

/*
 *--------------------------------------------------------------
 *
 * Ck_MainLoop --
 *
 *	Call Ck_DoOneEvent over and over again in an infinite
 *	loop as long as there exist any main windows.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arbitrary;  depends on handlers for events.
 *
 *--------------------------------------------------------------
 */

void
Ck_MainLoop()
{
    extern CkMainInfo *ckMainInfo;

    while (ckMainInfo != NULL) {
	Tk_DoOneEvent(0);
    }
}

/*
 *--------------------------------------------------------------
 *
 * BarcodeTimeout --
 *
 *	Handle timeout while reading barcode packet.
 *
 *--------------------------------------------------------------
 */

static void
BarcodeTimeout(clientData)
    ClientData clientData;
{
    CkMainInfo *mainPtr = (CkMainInfo *) clientData;
    BarcodeData *bd = (BarcodeData *) mainPtr->barcodeData;

    if (bd != NULL) {
	bd->index = -1;
	bd->timer = (Tk_TimerToken) NULL;
    }
}

/*
 *--------------------------------------------------------------
 *
 * CkGetBarcodeData --
 *
 *	Return data collected in barcode packet buffer.
 *
 *--------------------------------------------------------------
 */

char *
CkGetBarcodeData(mainPtr)
    CkMainInfo *mainPtr;
{
    BarcodeData *bd = (BarcodeData *) mainPtr->barcodeData;

    if (bd == NULL || bd->index < 0)
	return NULL;
    if (bd->index >= sizeof (bd->buffer) - 1)
	bd->buffer[sizeof (bd->buffer) - 1] = '\0';
    else
	bd->buffer[bd->index] = '\0';
    return bd->buffer;
}

/*
 *--------------------------------------------------------------
 *
 * CkBarcodeCmd --
 *
 *	Minor command handler to deal with barcode reader.
 *	Called by "curses" Tcl command.
 *
 * Results:
 *	TCL_OK or TCL_ERROR.
 *
 *
 *--------------------------------------------------------------
 */

int
CkBarcodeCmd(clientData, interp, argc, argv)
    ClientData clientData;      /* Main window associated with
			         * interpreter. */
    Tcl_Interp *interp;         /* Current interpreter. */
    int argc;                   /* Number of arguments. */
    char **argv;                /* Argument strings. */
{
    CkMainInfo *mainPtr = ((CkWindow *) (clientData))->mainPtr;
    BarcodeData *bd = (BarcodeData *) mainPtr->barcodeData;

    if (argc == 2) {
	if (mainPtr->flags & CK_HAS_BARCODE) {
	    char buffer[32];

	    sprintf(buffer, "%d %d %d", bd->startChar, bd->endChar,
		bd->pkttime);
	    Tcl_AppendResult(interp, buffer, (char *) NULL);
	}
	return TCL_OK;
    } else if (argc == 3) {
	if (strcmp(argv[2], "off") != 0)
	    goto badArgs;
	if (mainPtr->flags & CK_HAS_BARCODE) {
	    Tk_DeleteTimerHandler(bd->timer);
	    mainPtr->flags &= ~CK_HAS_BARCODE;
	    mainPtr->barcodeData = NULL;
	    ckfree((char *) bd);
	}
	return TCL_OK;
    } else if (argc == 4 || argc == 5) {
	int start, end, pkttime;

	if (Tcl_GetInt(interp, argv[2], &start) != TCL_OK ||
	    Tcl_GetInt(interp, argv[3], &end) != TCL_OK)
	    return TCL_ERROR;
	if (argc > 4 && Tcl_GetInt(interp, argv[4], &pkttime) != TCL_OK)
	    return TCL_ERROR;
	if (!(mainPtr->flags & CK_HAS_BARCODE)) {
	    bd = (BarcodeData *) ckalloc(sizeof (BarcodeData));
	    mainPtr->flags |= CK_HAS_BARCODE;
	    mainPtr->barcodeData = (ClientData) bd;
	    bd->pkttime = DEFAULT_BARCODE_TIMEOUT;
	    bd->timer = (Tk_TimerToken) NULL;
	    bd->delivered = 0;
	    bd->index = -1;
	}
	if (argc > 4 && pkttime > 50)
	    bd->pkttime = pkttime;
	bd->startChar = start;
	bd->endChar = end;
	return TCL_OK;
    } else {
badArgs:
	Tcl_AppendResult(interp, "bad or wrong # args: should be \"", argv[0],
	    " barcode ?off?\" or \"",
            argv[0], " barcode startChar endChar ?timeout?\"", (char *) NULL);
    }
    return TCL_ERROR;
}
