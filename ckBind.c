/* 
 * ckBind.c --
 *
 *	This file provides procedures that associate Tcl commands
 *	with events or sequences of events.
 *
 * Copyright (c) 1989-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 * Copyright (c) 1995 Christian Werner
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "ckPort.h"
#include "ck.h"

/*
 * The structure below represents a binding table.  A binding table
 * represents a domain in which event bindings may occur.  It includes
 * a space of objects relative to which events occur (usually windows,
 * but not always), a history of recent events in the domain, and
 * a set of mappings that associate particular Tcl commands with sequences
 * of events in the domain.  Multiple binding tables may exist at once,
 * either because there are multiple applications open, or because there
 * are multiple domains within an application with separate event
 * bindings for each (for example, each canvas widget has a separate
 * binding table for associating events with the items in the canvas).
 *
 * Note: it is probably a bad idea to reduce EVENT_BUFFER_SIZE much
 * below 30.  To see this, consider a triple mouse button click while
 * the Shift key is down (and auto-repeating).  There may be as many
 * as 3 auto-repeat events after each mouse button press or release
 * (see the first large comment block within Ck_BindEvent for more on
 * this), for a total of 20 events to cover the three button presses
 * and two intervening releases.  If you reduce EVENT_BUFFER_SIZE too
 * much, shift multi-clicks will be lost.
 * 
 */

#define EVENT_BUFFER_SIZE 30
typedef struct BindingTable {
    CkEvent eventRing[EVENT_BUFFER_SIZE];/* Circular queue of recent events
					 * (higher indices are for more recent
					 * events). */
    int detailRing[EVENT_BUFFER_SIZE];	/* "Detail" information (keycodes for
    					 * each entry in eventRing. */
    int curEvent;			/* Index in eventRing of most recent
					 * event.  Newer events have higher
					 * indices. */
    Tcl_HashTable patternTable;		/* Used to map from an event to a list
					 * of patterns that may match that
					 * event.  Keys are PatternTableKey
					 * structs, values are (PatSeq *). */
    Tcl_HashTable objectTable;		/* Used to map from an object to a list
					 * of patterns associated with that
					 * object.  Keys are ClientData,
					 * values are (PatSeq *). */
    Tcl_Interp *interp;			/* Interpreter in which commands are
					 * executed. */
} BindingTable;

/*
 * Structures of the following form are used as keys in the patternTable
 * for a binding table:
 */

typedef struct PatternTableKey {
    ClientData object;		/* Identifies object (or class of objects)
				 * relative to which event occurred.  For
				 * example, in the widget binding table for
				 * an application this is the path name of
				 * a widget, or a widget class, or "all". */
    int type;			/* Type of event. */
    int detail;			/* Additional information, such as
				 * keycode, or 0 if nothing
				 * additional.*/
} PatternTableKey;

/*
 * The following structure defines a pattern, which is matched
 * against events as part of the process of converting events
 * into Tcl commands.
 */

typedef struct Pattern {
    int eventType;		/* Type of event. */
    int detail;			/* Additional information that must
				 * match event.  Normally this is 0,
				 * meaning no additional information
				 * must match. For keystrokes this
				 * is the keycode. Keycode 0 means
				 * any keystroke, keycode -1 means
				 * control keystroke. */
} Pattern;

/*
 * The structure below defines a pattern sequence, which consists
 * of one or more patterns.  In order to trigger, a pattern
 * sequence must match the most recent X events (first pattern
 * to most recent event, next pattern to next event, and so on).
 */

typedef struct PatSeq {
    int numPats;		/* Number of patterns in sequence
				 * (usually 1). */
    char *command;		/* Command to invoke when this
				 * pattern sequence matches (malloc-ed). */
    struct PatSeq *nextSeqPtr;
				/* Next in list of all pattern
				 * sequences that have the same
				 * initial pattern.  NULL means
				 * end of list. */
    Tcl_HashEntry *hPtr;	/* Pointer to hash table entry for
				 * the initial pattern.  This is the
				 * head of the list of which nextSeqPtr
				 * forms a part. */
    ClientData object;		/* Identifies object with which event is
				 * associated (e.g. window). */
    struct PatSeq *nextObjPtr;
				/* Next in list of all pattern
				 * sequences for the same object
				 * (NULL for end of list).  Needed to
				 * implement Tk_DeleteAllBindings. */
    Pattern pats[1];		/* Array of "numPats" patterns.  Only
				 * one element is declared here but
				 * in actuality enough space will be
				 * allocated for "numPats" patterns.
				 * To match, pats[0] must match event
				 * n, pats[1] must match event n-1,
				 * etc. */
} PatSeq;

typedef struct {
    char *name;				/* Name of keysym. */
    KeySym value;			/* Numeric identifier for keysym. */
    char *tiname;			/* Terminfo name of keysym. */
} KeySymInfo;
static KeySymInfo keyArray[] = {
#include "ks_names.h"
    {(char *) NULL, 0}
};
static Tcl_HashTable keySymTable;	/* Hashed form of above structure. */
static Tcl_HashTable revKeySymTable;	/* Ditto, reversed. */

static int initialized = 0;

/*
 * This module also keeps a hash table mapping from event names
 * to information about those events.  The structure, an array
 * to use to initialize the hash table, and the hash table are
 * all defined below.
 */

typedef struct {
    char *name;			/* Name of event. */
    int type;			/* Event type for X, such as
				 * ButtonPress. */
    int eventMask;		/* Mask bits for this event type. */
} EventInfo;

static EventInfo eventArray[] = {
    {"Expose",		CK_EV_EXPOSE,		CK_EV_EXPOSE},
    {"FocusIn",		CK_EV_FOCUSIN,		CK_EV_FOCUSIN},
    {"FocusOut",	CK_EV_FOCUSOUT,		CK_EV_FOCUSOUT},
    {"Key",		CK_EV_KEYPRESS,		CK_EV_KEYPRESS},
    {"KeyPress",	CK_EV_KEYPRESS,		CK_EV_KEYPRESS},
    {"Control",		CK_EV_KEYPRESS,		CK_EV_KEYPRESS},
    {"Destroy",		CK_EV_DESTROY,		CK_EV_DESTROY},
    {"Map",		CK_EV_MAP,		CK_EV_MAP},
    {"Unmap",		CK_EV_UNMAP,		CK_EV_UNMAP},
    {"Button",		CK_EV_MOUSE_DOWN,	CK_EV_MOUSE_DOWN},
    {"ButtonPress",	CK_EV_MOUSE_DOWN,	CK_EV_MOUSE_DOWN},
    {"ButtonRelease",	CK_EV_MOUSE_UP,		CK_EV_MOUSE_UP},
    {"BarCode",		CK_EV_BARCODE,		CK_EV_BARCODE},
    {(char *) NULL,	0,			0}
};
static Tcl_HashTable eventTable;

/*
 * Prototypes for local procedures defined in this file:
 */

static void		ExpandPercents _ANSI_ARGS_((CkWindow *winPtr,
			    char *before, CkEvent *eventPtr, KeySym keySym,
			    Tcl_DString *dsPtr));
static PatSeq *		FindSequence _ANSI_ARGS_((Tcl_Interp *interp,
			    BindingTable *bindPtr, ClientData object,
			    char *eventString, int create));
static char *		GetField _ANSI_ARGS_((char *p, char *copy, int size));
static PatSeq *		MatchPatterns _ANSI_ARGS_((BindingTable *bindPtr,
			    PatSeq *psPtr));

/*
 *--------------------------------------------------------------
 *
 * Ck_CreateBindingTable --
 *
 *	Set up a new domain in which event bindings may be created.
 *
 * Results:
 *	The return value is a token for the new table, which must
 *	be passed to procedures like Ck_CreateBinding.
 *
 * Side effects:
 *	Memory is allocated for the new table.
 *
 *--------------------------------------------------------------
 */

Ck_BindingTable
Ck_CreateBindingTable(interp)
    Tcl_Interp *interp;		/* Interpreter to associate with the binding
				 * table:  commands are executed in this
				 * interpreter. */
{
    BindingTable *bindPtr;
    int i;

    /*
     * If this is the first time a binding table has been created,
     * initialize the global data structures.
     */

    if (!initialized) {
	Tcl_HashEntry *hPtr;
	EventInfo *eiPtr;
	KeySymInfo *kPtr;
	int dummy;

	Tcl_InitHashTable(&keySymTable, TCL_STRING_KEYS);
	Tcl_InitHashTable(&revKeySymTable, TCL_ONE_WORD_KEYS);
	for (kPtr = keyArray; kPtr->name != NULL; kPtr++) {
	    hPtr = Tcl_CreateHashEntry(&keySymTable, kPtr->name, &dummy);
	    Tcl_SetHashValue(hPtr, (char *) kPtr);
	    hPtr = Tcl_CreateHashEntry(&revKeySymTable, (char *) kPtr->value,
	        &dummy);
	    Tcl_SetHashValue(hPtr, (char *) kPtr);
	}
	Tcl_InitHashTable(&eventTable, TCL_STRING_KEYS);
	for (eiPtr = eventArray; eiPtr->name != NULL; eiPtr++) {
	    hPtr = Tcl_CreateHashEntry(&eventTable, eiPtr->name, &dummy);
	    Tcl_SetHashValue(hPtr, eiPtr);
	}
	initialized = 1;
    }

    /*
     * Create and initialize a new binding table.
     */

    bindPtr = (BindingTable *) ckalloc(sizeof (BindingTable));
    for (i = 0; i < EVENT_BUFFER_SIZE; i++) {
	bindPtr->eventRing[i].type = -1;
    }
    bindPtr->curEvent = 0;
    Tcl_InitHashTable(&bindPtr->patternTable,
	    sizeof(PatternTableKey)/sizeof(int));
    Tcl_InitHashTable(&bindPtr->objectTable, TCL_ONE_WORD_KEYS);
    bindPtr->interp = interp;
    return (Ck_BindingTable) bindPtr;
}

/*
 *--------------------------------------------------------------
 *
 * Ck_DeleteBindingTable --
 *
 *	Destroy a binding table and free up all its memory.
 *	The caller should not use bindingTable again after
 *	this procedure returns.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is freed.
 *
 *--------------------------------------------------------------
 */

void
Ck_DeleteBindingTable(bindingTable)
    Ck_BindingTable bindingTable;	/* Token for the binding table to
					 * destroy. */
{
    BindingTable *bindPtr = (BindingTable *) bindingTable;
    PatSeq *psPtr, *nextPtr;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;

    /*
     * Find and delete all of the patterns associated with the binding
     * table.
     */

    for (hPtr = Tcl_FirstHashEntry(&bindPtr->patternTable, &search);
	    hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	for (psPtr = (PatSeq *) Tcl_GetHashValue(hPtr);
		psPtr != NULL; psPtr = nextPtr) {
	    nextPtr = psPtr->nextSeqPtr;
	    ckfree((char *) psPtr->command);
	    ckfree((char *) psPtr);
	}
    }

    /*
     * Clean up the rest of the information associated with the
     * binding table.
     */

    Tcl_DeleteHashTable(&bindPtr->patternTable);
    Tcl_DeleteHashTable(&bindPtr->objectTable);
    ckfree((char *) bindPtr);
}

/*
 *--------------------------------------------------------------
 *
 * Ck_CreateBinding --
 *
 *	Add a binding to a binding table, so that future calls to
 *	Ck_BindEvent may execute the command in the binding.
 *
 * Results:
 *	The return value is TCL_ERROR if an error occurred while setting
 *	up the binding.  In this case, an error message will be
 *	left in interp->result.  If all went well then the return
 *	value is TCL_OK.
 *
 * Side effects:
 *	The new binding may cause future calls to Ck_BindEvent to
 *	behave differently than they did previously.
 *
 *--------------------------------------------------------------
 */

int
Ck_CreateBinding(interp, bindingTable, object, eventString, command, append)
    Tcl_Interp *interp;			/* Used for error reporting. */
    Ck_BindingTable bindingTable;	/* Table in which to create binding. */
    ClientData object;			/* Token for object with which binding
					 * is associated. */
    char *eventString;			/* String describing event sequence
					 * that triggers binding. */
    char *command;			/* Contains Tcl command to execute
					 * when binding triggers. */
    int append;				/* 0 means replace any existing
					 * binding for eventString;  1 means
					 * append to that binding. */
{
    BindingTable *bindPtr = (BindingTable *) bindingTable;
    PatSeq *psPtr;

    psPtr = FindSequence(interp, bindPtr, object, eventString, 1);
    if (psPtr == NULL)
	return TCL_ERROR;
    if (append && (psPtr->command != NULL)) {
	int length;
	char *new;

	length = strlen(psPtr->command) + strlen(command) + 2;
	new = (char *) ckalloc((unsigned) length);
	sprintf(new, "%s\n%s", psPtr->command, command);
	ckfree((char *) psPtr->command);
	psPtr->command = new;
    } else {
	if (psPtr->command != NULL) {
	    ckfree((char *) psPtr->command);
	}
	psPtr->command = (char *) ckalloc((unsigned) (strlen(command) + 1));
	strcpy(psPtr->command, command);
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Ck_DeleteBinding --
 *
 *	Remove an event binding from a binding table.
 *
 * Results:
 *	The result is a standard Tcl return value.  If an error
 *	occurs then interp->result will contain an error message.
 *
 * Side effects:
 *	The binding given by object and eventString is removed
 *	from bindingTable.
 *
 *--------------------------------------------------------------
 */

int
Ck_DeleteBinding(interp, bindingTable, object, eventString)
    Tcl_Interp *interp;			/* Used for error reporting. */
    Ck_BindingTable bindingTable;	/* Table in which to delete binding. */
    ClientData object;			/* Token for object with which binding
					 * is associated. */
    char *eventString;			/* String describing event sequence
					 * that triggers binding. */
{
    BindingTable *bindPtr = (BindingTable *) bindingTable;
    register PatSeq *psPtr, *prevPtr;
    Tcl_HashEntry *hPtr;

    psPtr = FindSequence(interp, bindPtr, object, eventString, 0);
    if (psPtr == NULL) {
	Tcl_ResetResult(interp);
	return TCL_OK;
    }

    /*
     * Unlink the binding from the list for its object, then from the
     * list for its pattern.
     */

    hPtr = Tcl_FindHashEntry(&bindPtr->objectTable, (char *) object);
    if (hPtr == NULL) {
	panic("Ck_DeleteBinding couldn't find object table entry");
    }
    prevPtr = (PatSeq *) Tcl_GetHashValue(hPtr);
    if (prevPtr == psPtr) {
	Tcl_SetHashValue(hPtr, psPtr->nextObjPtr);
    } else {
	for ( ; ; prevPtr = prevPtr->nextObjPtr) {
	    if (prevPtr == NULL) {
		panic("Ck_DeleteBinding couldn't find on object list");
	    }
	    if (prevPtr->nextObjPtr == psPtr) {
		prevPtr->nextObjPtr = psPtr->nextObjPtr;
		break;
	    }
	}
    }
    prevPtr = (PatSeq *) Tcl_GetHashValue(psPtr->hPtr);
    if (prevPtr == psPtr) {
	if (psPtr->nextSeqPtr == NULL) {
	    Tcl_DeleteHashEntry(psPtr->hPtr);
	} else {
	    Tcl_SetHashValue(psPtr->hPtr, psPtr->nextSeqPtr);
	}
    } else {
	for ( ; ; prevPtr = prevPtr->nextSeqPtr) {
	    if (prevPtr == NULL) {
		panic("Tk_DeleteBinding couldn't find on hash chain");
	    }
	    if (prevPtr->nextSeqPtr == psPtr) {
		prevPtr->nextSeqPtr = psPtr->nextSeqPtr;
		break;
	    }
	}
    }
    ckfree((char *) psPtr->command);
    ckfree((char *) psPtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Ck_GetBinding --
 *
 *	Return the command associated with a given event string.
 *
 * Results:
 *	The return value is a pointer to the command string
 *	associated with eventString for object in the domain
 *	given by bindingTable.  If there is no binding for
 *	eventString, or if eventString is improperly formed,
 *	then NULL is returned and an error message is left in
 *	interp->result.  The return value is semi-static:  it
 *	will persist until the binding is changed or deleted.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
Ck_GetBinding(interp, bindingTable, object, eventString)
    Tcl_Interp *interp;			/* Interpreter for error reporting. */
    Ck_BindingTable bindingTable;	/* Table in which to look for
					 * binding. */
    ClientData object;			/* Token for object with which binding
					 * is associated. */
    char *eventString;			/* String describing event sequence
					 * that triggers binding. */
{
    BindingTable *bindPtr = (BindingTable *) bindingTable;
    PatSeq *psPtr;

    psPtr = FindSequence(interp, bindPtr, object, eventString, 0);
    if (psPtr == NULL) {
	return NULL;
    }
    return psPtr->command;
}

/*
 *--------------------------------------------------------------
 *
 * Ck_GetAllBindings --
 *
 *	Return a list of event strings for all the bindings
 *	associated with a given object.
 *
 * Results:
 *	There is no return value.  Interp->result is modified to
 *	hold a Tcl list with one entry for each binding associated
 *	with object in bindingTable.  Each entry in the list
 *	contains the event string associated with one binding.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Ck_GetAllBindings(interp, bindingTable, object)
    Tcl_Interp *interp;			/* Interpreter returning result or
					 * error. */
    Ck_BindingTable bindingTable;	/* Table in which to look for
					 * bindings. */
    ClientData object;			/* Token for object. */

{
    BindingTable *bindPtr = (BindingTable *) bindingTable;
    register PatSeq *psPtr;
    register Pattern *patPtr;
    Tcl_HashEntry *hPtr;
    Tcl_DString ds;
    char c, buffer[10];
    int patsLeft;
    register EventInfo *eiPtr;

    hPtr = Tcl_FindHashEntry(&bindPtr->objectTable, (char *) object);
    if (hPtr == NULL) {
	return;
    }
    Tcl_DStringInit(&ds);
    for (psPtr = (PatSeq *) Tcl_GetHashValue(hPtr); psPtr != NULL;
	    psPtr = psPtr->nextObjPtr) {
	Tcl_DStringTrunc(&ds, 0);

	/*
	 * For each binding, output information about each of the
	 * patterns in its sequence.  The order of the patterns in
	 * the sequence is backwards from the order in which they
	 * must be output.
	 */

	for (patsLeft = psPtr->numPats,
		patPtr = &psPtr->pats[psPtr->numPats - 1];
		patsLeft > 0; patsLeft--, patPtr--) {

	    /*
	     * Check for button presses.
	     */

	    if ((patPtr->eventType == CK_EV_MOUSE_DOWN)
		    && (patPtr->detail != 0)) {
		sprintf(buffer, "<%d>", patPtr->detail);
		Tcl_DStringAppend(&ds, buffer, -1);
		continue;
	    }

	    /*
	     * Check for simple case of an ASCII character.
	     */

	    if ((patPtr->eventType == CK_EV_KEYPRESS)
		    && (patPtr->detail < 128)
		    && isprint((unsigned char) patPtr->detail)
		    && (patPtr->detail != '<')
		    && (patPtr->detail != ' ')) {
		c = patPtr->detail;
		Tcl_DStringAppend(&ds, &c, 1);
		continue;
	    }

	    /*
	     * It's a more general event specification.  First check
	     * event type, then keysym or button detail.
	     */

	    Tcl_DStringAppend(&ds, "<", 1);

	    for (eiPtr = eventArray; eiPtr->name != NULL; eiPtr++) {
		if (eiPtr->type == patPtr->eventType) {
		    if (patPtr->eventType == CK_EV_KEYPRESS &&
			patPtr->detail == -1) {
			Tcl_DStringAppend(&ds, "Control", -1);
			goto endPat;
		    }
		    if (patPtr->eventType == CK_EV_KEYPRESS &&
		        patPtr->detail > 0 && patPtr->detail < 0x20) {
			char *string;

		        string = CkKeysymToString((KeySym) patPtr->detail, 0);
		        if (string == NULL) {
			    sprintf(buffer, "Control-%c",
			        patPtr->detail + 0x40);
			    string = buffer;
			}
			Tcl_DStringAppend(&ds, string, -1);
			goto endPat;
		    }
		    Tcl_DStringAppend(&ds, eiPtr->name, -1);
		    if (patPtr->detail != 0) {
			Tcl_DStringAppend(&ds, "-", 1);
		    }
		    break;
		}
	    }

	    if (patPtr->detail != 0) {
		if (patPtr->eventType == CK_EV_KEYPRESS) {
		    char *string;

		    string = CkKeysymToString((KeySym) patPtr->detail, 0);
		    if (string != NULL) {
			Tcl_DStringAppend(&ds, string, -1);
		    }
		} else {
		    sprintf(buffer, "%d", patPtr->detail);
		    Tcl_DStringAppend(&ds, buffer, -1);
		}
	    }
endPat:
	    Tcl_DStringAppend(&ds, ">", 1);
	}
	Tcl_AppendElement(interp, Tcl_DStringValue(&ds));
    }
    Tcl_DStringFree(&ds);
}

/*
 *--------------------------------------------------------------
 *
 * Ck_DeleteAllBindings --
 *
 *	Remove all bindings associated with a given object in a
 *	given binding table.
 *
 * Results:
 *	All bindings associated with object are removed from
 *	bindingTable.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Ck_DeleteAllBindings(bindingTable, object)
    Ck_BindingTable bindingTable;	/* Table in which to delete
					 * bindings. */
    ClientData object;			/* Token for object. */
{
    BindingTable *bindPtr = (BindingTable *) bindingTable;
    PatSeq *psPtr, *prevPtr;
    PatSeq *nextPtr;
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&bindPtr->objectTable, (char *) object);
    if (hPtr == NULL) {
	return;
    }
    for (psPtr = (PatSeq *) Tcl_GetHashValue(hPtr); psPtr != NULL;
	    psPtr = nextPtr) {
	nextPtr  = psPtr->nextObjPtr;

	/*
	 * Be sure to remove each binding from its hash chain in the
	 * pattern table.  If this is the last pattern in the chain,
	 * then delete the hash entry too.
	 */

	prevPtr = (PatSeq *) Tcl_GetHashValue(psPtr->hPtr);
	if (prevPtr == psPtr) {
	    if (psPtr->nextSeqPtr == NULL) {
		Tcl_DeleteHashEntry(psPtr->hPtr);
	    } else {
		Tcl_SetHashValue(psPtr->hPtr, psPtr->nextSeqPtr);
	    }
	} else {
	    for ( ; ; prevPtr = prevPtr->nextSeqPtr) {
		if (prevPtr == NULL) {
		    panic("Ck_DeleteAllBindings couldn't find on hash chain");
		}
		if (prevPtr->nextSeqPtr == psPtr) {
		    prevPtr->nextSeqPtr = psPtr->nextSeqPtr;
		    break;
		}
	    }
	}
	ckfree((char *) psPtr->command);
	ckfree((char *) psPtr);
    }
    Tcl_DeleteHashEntry(hPtr);
}

/*
 *--------------------------------------------------------------
 *
 * Ck_BindEvent --
 *
 *	This procedure is invoked to process an event.  The
 *	event is added to those recorded for the binding table.
 *	Then each of the objects at *objectPtr is checked in
 *	order to see if it has a binding that matches the recent
 *	events.  If so, that binding is invoked and the rest of
 *	objects are skipped.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on the command associated with the matching
 *	binding.
 *
 *--------------------------------------------------------------
 */

void
Ck_BindEvent(bindingTable, eventPtr, winPtr, numObjects, objectPtr)
    Ck_BindingTable bindingTable;	/* Table in which to look for
					 * bindings. */
    CkEvent *eventPtr;			/* What actually happened. */
    CkWindow *winPtr;			/* Window where event occurred. */
    int numObjects;			/* Number of objects at *objectPtr. */
    ClientData *objectPtr;		/* Array of one or more objects
					 * to check for a matching binding. */
{
    BindingTable *bindPtr = (BindingTable *) bindingTable;
    CkMainInfo *mainPtr;
    CkEvent *ringPtr;
    PatSeq *matchPtr;
    PatternTableKey key;
    Tcl_HashEntry *hPtr;
    int detail, code;
    Tcl_Interp *interp;
    Tcl_DString scripts, savedResult;
    char *p, *end;

    /*
     * Add the new event to the ring of saved events for the
     * binding table.
     */

    bindPtr->curEvent++;
    if (bindPtr->curEvent >= EVENT_BUFFER_SIZE)
	bindPtr->curEvent = 0;
    ringPtr = &bindPtr->eventRing[bindPtr->curEvent];
    memcpy((VOID *) ringPtr, (VOID *) eventPtr, sizeof (CkEvent));
    detail = 0;
    bindPtr->detailRing[bindPtr->curEvent] = 0;
    if (ringPtr->type == CK_EV_KEYPRESS)
	detail = ringPtr->key.keycode;
    else if (ringPtr->type == CK_EV_MOUSE_DOWN ||
        ringPtr->type == CK_EV_MOUSE_UP)
	detail = ringPtr->mouse.button;
    bindPtr->detailRing[bindPtr->curEvent] = detail;

    /*
     * Loop over all the objects, finding the binding script for each
     * one.  Append all of the binding scripts, with %-sequences expanded,
     * to "scripts", with null characters separating the scripts for
     * each object.
     */

    Tcl_DStringInit(&scripts);
    for ( ; numObjects > 0; numObjects--, objectPtr++) {

	/*
	 * Match the new event against those recorded in the
	 * pattern table, saving the longest matching pattern.
	 * For events with details (key events) first
	 * look for a binding for the specific key or button.
	 * If none is found, then look for a binding for all
         * control-keys (detail of -1, if the keycode is a control
         * character), else look for a binding for all keys
         * (detail of 0).
	 */
    
	matchPtr = NULL;
	key.object = *objectPtr;
	key.type = ringPtr->type;
	key.detail = detail;
	hPtr = Tcl_FindHashEntry(&bindPtr->patternTable, (char *) &key);
	if (hPtr != NULL) {
	    matchPtr = MatchPatterns(bindPtr,
		    (PatSeq *) Tcl_GetHashValue(hPtr));
	}
	if (ringPtr->type == CK_EV_KEYPRESS && detail > 0 && detail < 0x20 &&
            matchPtr == NULL) {
	    key.detail = -1;
	    hPtr = Tcl_FindHashEntry(&bindPtr->patternTable, (char *) &key);
	    if (hPtr != NULL) {
		matchPtr = MatchPatterns(bindPtr,
			(PatSeq *) Tcl_GetHashValue(hPtr));
	    }
	}
	if (detail != 0 && matchPtr == NULL) {
	    key.detail = 0;
	    hPtr = Tcl_FindHashEntry(&bindPtr->patternTable, (char *) &key);
	    if (hPtr != NULL) {
		matchPtr = MatchPatterns(bindPtr,
			(PatSeq *) Tcl_GetHashValue(hPtr));
	    }
	}
    
	if (matchPtr != NULL) {
	    ExpandPercents(winPtr, matchPtr->command, eventPtr,
		    (KeySym) detail, &scripts);
	    Tcl_DStringAppend(&scripts, "", 1);
	}
    }

    /*
     * Now go back through and evaluate the script for each object,
     * in order, dealing with "break" and "continue" exceptions
     * appropriately.
     *
     * There are two tricks here:
     * 1. Bindings can be invoked from in the middle of Tcl commands,
     *    where interp->result is significant (for example, a widget
     *    might be deleted because of an error in creating it, so the
     *    result contains an error message that is eventually going to
     *    be returned by the creating command).  To preserve the result,
     *    we save it in a dynamic string.
     * 2. The binding's action can potentially delete the binding,
     *    so bindPtr may not point to anything valid once the action
     *    completes.  Thus we have to save bindPtr->interp in a
     *    local variable in order to restore the result.
     * 3. When the screen changes, must invoke a Tcl script to update
     *    Tcl level information such as tkPriv.
     */

    mainPtr = winPtr->mainPtr;
    interp = bindPtr->interp;
    Tcl_DStringInit(&savedResult);
    Tcl_DStringGetResult(interp, &savedResult);
    p = Tcl_DStringValue(&scripts);
    end = p + Tcl_DStringLength(&scripts);
    while (p != end) {
	Tcl_AllowExceptions(interp);
	code = Tcl_GlobalEval(interp, p);
	if (code != TCL_OK) {
	    if (code == TCL_CONTINUE) {
		/*
		 * Do nothing:  just go on to the next script.
		 */
	    } else if (code == TCL_BREAK) {
		break;
	    } else {
		Tcl_AddErrorInfo(interp, "\n    (command bound to event)");
		Tk_BackgroundError(interp);
		break;
	    }
	}

	/*
	 * Skip over the current script and its terminating null character.
	 */

	while (*p != 0) {
	    p++;
	}
	p++;
    }
    Tcl_DStringResult(interp, &savedResult);
    Tcl_DStringFree(&scripts);
}

/*
 *----------------------------------------------------------------------
 *
 * FindSequence --
 *
 *	Find the entry in a binding table that corresponds to a
 *	particular pattern string, and return a pointer to that
 *	entry.
 *
 * Results:
 *	The return value is normally a pointer to the PatSeq
 *	in patternTable that corresponds to eventString.  If an error
 *	was found while parsing eventString, or if "create" is 0 and
 *	no pattern sequence previously existed, then NULL is returned
 *	and interp->result contains a message describing the problem.
 *	If no pattern sequence previously existed for eventString, then
 *	a new one is created with a NULL command field.  In a successful
 *	return, *maskPtr is filled in with a mask of the event types
 *	on which the pattern sequence depends.
 *
 * Side effects:
 *	A new pattern sequence may be created.
 *
 *----------------------------------------------------------------------
 */

static PatSeq *
FindSequence(interp, bindPtr, object, eventString, create)
    Tcl_Interp *interp;		/* Interpreter to use for error
				 * reporting. */
    BindingTable *bindPtr;	/* Table to use for lookup. */
    ClientData object;		/* Token for object(s) with which binding
				 * is associated. */
    char *eventString;		/* String description of pattern to
				 * match on.  See user documentation
				 * for details. */
    int create;			/* 0 means don't create the entry if
				 * it doesn't already exist.   Non-zero
				 * means create. */

{
    Pattern pats[EVENT_BUFFER_SIZE];
    int numPats, isCtrl;
    register char *p;
    register Pattern *patPtr;
    register PatSeq *psPtr;
    register Tcl_HashEntry *hPtr;
#define FIELD_SIZE 48
    char field[FIELD_SIZE];
    int new;
    size_t sequenceSize;
    unsigned long eventMask;
    PatternTableKey key;

    /*
     *-------------------------------------------------------------
     * Step 1: parse the pattern string to produce an array
     * of Patterns.  The array is generated backwards, so
     * that the lowest-indexed pattern corresponds to the last
     * event that must occur.
     *-------------------------------------------------------------
     */

    p = eventString;
    eventMask = 0;
    for (numPats = 0, patPtr = &pats[EVENT_BUFFER_SIZE-1];
	    numPats < EVENT_BUFFER_SIZE;
	    numPats++, patPtr--) {
	patPtr->eventType = -1;
	patPtr->detail = 0;
	while (isspace((unsigned char) *p)) {
	    p++;
	}
	if (*p == '\0') {
	    break;
	}

	/*
	 * Handle simple ASCII characters.
	 */

	if (*p != '<') {
	    char string[2];

	    patPtr->eventType = CK_EV_KEYPRESS;
	    string[0] = *p;
	    string[1] = 0;
	    patPtr->detail = CkStringToKeysym(string);
	    if (patPtr->detail == NoSymbol) {
		if (isprint((unsigned char) *p)) {
		    patPtr->detail = *p;
		} else {
		    sprintf(interp->result,
			    "bad ASCII character 0x%x", (unsigned char) *p);
		    return NULL;
		}
	    }
	    p++;
	    continue;
	}

	p++;

	/*
	 * Abbrevated button press event.
	 */

	if (isdigit((unsigned char) *p) && p[1] == '>') {
	    register EventInfo *eiPtr;

	    hPtr = Tcl_FindHashEntry(&eventTable, "ButtonPress");
	    eiPtr = (EventInfo *) Tcl_GetHashValue(hPtr);
	    patPtr->eventType = eiPtr->type;
	    eventMask |= eiPtr->eventMask;
	    patPtr->detail = *p - '0';
	    p += 2;
	    continue;
	}

	/*
	 * A fancier event description.  Must consist of
	 * 1. open angle bracket.
	 * 2. optional event name.
	 * 3. an option keysym name.  Either this or
	 *    item 2 *must* be present;  if both are present
	 *    then they are separated by spaces or dashes.
	 * 4. a close angle bracket.
	 */

	isCtrl = 0;
        p = GetField(p, field, FIELD_SIZE);
	hPtr = Tcl_FindHashEntry(&eventTable, field);
	if (hPtr != NULL) {
	    register EventInfo *eiPtr;

	    eiPtr = (EventInfo *) Tcl_GetHashValue(hPtr);
	    patPtr->eventType = eiPtr->type;
	    eventMask |= eiPtr->eventMask;
	    isCtrl = strcmp(eiPtr->name, "Control") == 0;
	    if (isCtrl) {
		patPtr->detail = -1;
	    }
	    while ((*p == '-') || isspace((unsigned char) *p)) {
		p++;
	    }
	    p = GetField(p, field, FIELD_SIZE);
	}
	if (*field != '\0') {
	    if (patPtr->eventType == CK_EV_MOUSE_DOWN ||
		patPtr->eventType == CK_EV_MOUSE_UP) {
		if (!isdigit((unsigned char) *field) && field[1] != '\0') {
		    Tcl_AppendResult(interp, "bad mouse button \"",
			field, "\"", (char *) NULL);
		    return NULL;
		}
		patPtr->detail = field[0] - '0';
		goto closeAngle;
	    }

	    patPtr->detail = CkStringToKeysym(field);
	    if (patPtr->detail == NoSymbol) {
badKeySym:
		Tcl_AppendResult(interp, "bad event type or keysym \"",
			field, "\"", (char *) NULL);
		return NULL;
	    } else if (patPtr->eventType == CK_EV_KEYPRESS && isCtrl) {
		patPtr->detail -= 0x40;
		if (patPtr->detail >= 0x20)
		    patPtr->detail -= 0x20;
		if (patPtr->detail < 0 || patPtr->detail >= 0x20)
		    goto badKeySym;
	    }
	    if (patPtr->eventType == -1) {
		patPtr->eventType = CK_EV_KEYPRESS;
	    } else if (patPtr->eventType != CK_EV_KEYPRESS) {
		Tcl_AppendResult(interp, "specified keysym \"", field,
			"\" for non-key event", (char *) NULL);
		return NULL;
	    }
	} else if (patPtr->eventType == -1) {
	    interp->result = "no event type or keysym";
	    return NULL;
	}
	while ((*p == '-') || isspace((unsigned char) *p)) {
	    p++;
	}
closeAngle:
	if (*p != '>') {
	    interp->result = "missing \">\" in binding";
	    return NULL;
	}
	p++;

    }

    /*
     *-------------------------------------------------------------
     * Step 2: find the sequence in the binding table if it exists,
     * and add a new sequence to the table if it doesn't.
     *-------------------------------------------------------------
     */

    if (numPats == 0) {
	interp->result = "no events specified in binding";
	return NULL;
    }
    patPtr = &pats[EVENT_BUFFER_SIZE-numPats];
    key.object = object;
    key.type = patPtr->eventType;
    key.detail = patPtr->detail;
    hPtr = Tcl_CreateHashEntry(&bindPtr->patternTable, (char *) &key, &new);
    sequenceSize = numPats*sizeof(Pattern);
    if (!new) {
	for (psPtr = (PatSeq *) Tcl_GetHashValue(hPtr); psPtr != NULL;
		psPtr = psPtr->nextSeqPtr) {
	    if ((numPats == psPtr->numPats)
		    && (memcmp((char *) patPtr, (char *) psPtr->pats,
		    sequenceSize) == 0)) {
		goto done;
	    }
	}
    }
    if (!create) {
	if (new) {
	    Tcl_DeleteHashEntry(hPtr);
	}
	Tcl_AppendResult(interp, "no binding exists for \"",
		eventString, "\"", (char *) NULL);
	return NULL;
    }
    psPtr = (PatSeq *) ckalloc((unsigned) (sizeof(PatSeq)
	    + (numPats-1)*sizeof(Pattern)));
    psPtr->numPats = numPats;
    psPtr->command = NULL;
    psPtr->nextSeqPtr = (PatSeq *) Tcl_GetHashValue(hPtr);
    psPtr->hPtr = hPtr;
    Tcl_SetHashValue(hPtr, psPtr);

    /*
     * Link the pattern into the list associated with the object.
     */

    psPtr->object = object;
    hPtr = Tcl_CreateHashEntry(&bindPtr->objectTable, (char *) object, &new);
    if (new) {
	psPtr->nextObjPtr = NULL;
    } else {
	psPtr->nextObjPtr = (PatSeq *) Tcl_GetHashValue(hPtr);
    }
    Tcl_SetHashValue(hPtr, psPtr);

    memcpy((VOID *) psPtr->pats, (VOID *) patPtr, sequenceSize);

done:
    return psPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * GetField --
 *
 *	Used to parse pattern descriptions.  Copies up to
 *	size characters from p to copy, stopping at end of
 *	string, space, "-", ">", or whenever size is
 *	exceeded.
 *
 * Results:
 *	The return value is a pointer to the character just
 *	after the last one copied (usually "-" or space or
 *	">", but could be anything if size was exceeded).
 *	Also places NULL-terminated string (up to size
 *	character, including NULL), at copy.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
GetField(p, copy, size)
    register char *p;		/* Pointer to part of pattern. */
    register char *copy;	/* Place to copy field. */
    int size;			/* Maximum number of characters to
				 * copy. */
{
    while ((*p != '\0') && !isspace((unsigned char) *p) && (*p != '>')
	    && (*p != '-') && (size > 1)) {
	*copy = *p;
	p++;
	copy++;
	size--;
    }
    *copy = '\0';
    return p;
}

/*
 *----------------------------------------------------------------------
 *
 * MatchPatterns --
 *
 *	Given a list of pattern sequences and a list of
 *	recent events, return a pattern sequence that matches
 *	the event list.
 *
 * Results:
 *	The return value is NULL if no pattern matches the
 *	recent events from bindPtr.  If one or more patterns
 *	matches, then the longest (or most specific) matching
 *	pattern is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static PatSeq *
MatchPatterns(bindPtr, psPtr)
    BindingTable *bindPtr;	/* Information about binding table, such
				 * as ring of recent events. */
    register PatSeq *psPtr;	/* List of pattern sequences. */
{
    register PatSeq *bestPtr = NULL;

    /*
     * Iterate over all the pattern sequences.
     */

    for ( ; psPtr != NULL; psPtr = psPtr->nextSeqPtr) {
	register CkEvent *eventPtr;
	register Pattern *patPtr;
	CkWindow *winPtr;
	int *detailPtr;
	int patCount, ringCount;

	/*
	 * Iterate over all the patterns in a sequence to be
	 * sure that they all match.
	 */

	eventPtr = &bindPtr->eventRing[bindPtr->curEvent];
	detailPtr = &bindPtr->detailRing[bindPtr->curEvent];
	winPtr = eventPtr->any.winPtr;
	patPtr = psPtr->pats;
	patCount = psPtr->numPats;
	ringCount = EVENT_BUFFER_SIZE;
	while (patCount > 0) {
	    if (ringCount <= 0) {
		goto nextSequence;
	    }
	    if (eventPtr->any.type != patPtr->eventType) {
		if (patPtr->eventType == CK_EV_KEYPRESS)
		    goto nextEvent;
	    }
	    if (eventPtr->any.winPtr != winPtr)
		goto nextSequence;

	    /*
	     * Note: it's important for the keysym check to go before
	     * the modifier check, so we can ignore unwanted modifier
	     * keys before choking on the modifier check.
	     */

	    if ((patPtr->detail != 0) && (patPtr->detail != -1)
		    && (patPtr->detail != *detailPtr))
		goto nextSequence;

	    if ((patPtr->detail == -1) && (*detailPtr >= 0x20))
		goto nextSequence;

	    patPtr++;
	    patCount--;
	    nextEvent:
	    if (eventPtr == bindPtr->eventRing) {
		eventPtr = &bindPtr->eventRing[EVENT_BUFFER_SIZE-1];
		detailPtr = &bindPtr->detailRing[EVENT_BUFFER_SIZE-1];
	    } else {
		eventPtr--;
		detailPtr--;
	    }
	    ringCount--;
	}

	/*
	 * This sequence matches.  If we've already got another match,
	 * pick whichever is most specific.
	 */

	if (bestPtr != NULL) {
	    register Pattern *patPtr2;
	    int i;

	    if (psPtr->numPats != bestPtr->numPats) {
		if (bestPtr->numPats > psPtr->numPats) {
		    goto nextSequence;
		} else {
		    goto newBest;
		}
	    }
	    for (i = 0, patPtr = psPtr->pats, patPtr2 = bestPtr->pats;
		    i < psPtr->numPats; i++, patPtr++, patPtr2++) {
		if (patPtr->detail != patPtr2->detail) {
		    if (patPtr->detail == -1 && patPtr2->detail == 0) {
			goto newBest;
		    } else if (patPtr->detail == 0 || patPtr->detail == -1) {
			goto nextSequence;
		    } else {
			goto newBest;
		    }
		}
	    }
	    goto nextSequence;	/* Tie goes to newest pattern. */
	}
	newBest:
	bestPtr = psPtr;

	nextSequence: continue;
    }
    return bestPtr;
}

/*
 *--------------------------------------------------------------
 *
 * ExpandPercents --
 *
 *	Given a command and an event, produce a new command
 *	by replacing % constructs in the original command
 *	with information from the X event.
 *
 * Results:
 *	The new expanded command is appended to the dynamic string
 *	given by dsPtr.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
ExpandPercents(winPtr, before, eventPtr, keySym, dsPtr)
    CkWindow *winPtr;		/* Window where event occurred:  needed to
				 * get input context. */
    register char *before;	/* Command containing percent
				 * expressions to be replaced. */
    register CkEvent *eventPtr;	/* Event containing information
				 * to be used in % replacements. */
    KeySym keySym;		/* KeySym: only relevant for
				 * CK_EV_KEYPRESS events). */
    Tcl_DString *dsPtr;		/* Dynamic string in which to append
				 * new command. */
{
    int spaceNeeded, cvtFlags;	/* Used to substitute string as proper Tcl
				 * list element. */
    int number;
#define NUM_SIZE 40
    char *string, *string2;
    char numStorage[NUM_SIZE+1];

    while (1) {
	/*
	 * Find everything up to the next % character and append it
	 * to the result string.
	 */

	for (string = before; (*string != 0) && (*string != '%'); string++) {
	    /* Empty loop body. */
	}
	if (string != before) {
	    Tcl_DStringAppend(dsPtr, before, string-before);
	    before = string;
	}
	if (*before == 0) {
	    break;
	}

	/*
	 * There's a percent sequence here.  Process it.
	 */

	number = 0;
	string = "??";
	switch (before[1]) {
	    case 'k':
		number = eventPtr->key.keycode;
		goto doNumber;
	    case 'A':
		if (eventPtr->type == CK_EV_KEYPRESS) {
		    int numChars = 0;

		    if ((eventPtr->key.keycode & ~0xff) == 0 &&
		        eventPtr->key.keycode != 0) {
#if CK_USE_UTF
			char c = eventPtr->key.keycode;
			int numc = 0;

			if (winPtr->mainPtr->isoEncoding) {
			    Tcl_ExternalToUtf(NULL,
				winPtr->mainPtr->isoEncoding,
				&c, 1, 0, NULL,
				numStorage + numChars,
				sizeof (numStorage) - numChars,
				NULL, &numc, NULL);
			    numChars += numc;
			} else {
		    	    numStorage[numChars++] = eventPtr->key.keycode;
			}
#else
		    	numStorage[numChars++] = eventPtr->key.keycode;
#endif
		    }
#if CK_USE_UTF
		    if (eventPtr->key.is_uch) {
			numChars = Tcl_UniCharToUtf(eventPtr->key.uch,
						    numStorage);
		    }
#endif
		    numStorage[numChars] = '\0';
		    string = numStorage;
		} else if (eventPtr->type == CK_EV_BARCODE) {
		    string = CkGetBarcodeData(winPtr->mainPtr);
		    if (string == NULL) {
			numStorage[0] = '\0';
			string = numStorage;
		    }
		}
		goto doString;
	    case 'K':
		if (eventPtr->type == CK_EV_KEYPRESS) {
		    char *name;

		    name = CkKeysymToString(keySym, 1);
		    if (name != NULL) {
			string = name;
		    }
		}
		goto doString;
	    case 'N':
		number = (int) keySym;
		goto doNumber;
	    case 'W':
		if (Tcl_FindHashEntry(&winPtr->mainPtr->winTable,
		    (char *) eventPtr->any.winPtr) != NULL) {
		    string = eventPtr->any.winPtr->pathName;
		} else {
		    string = "??";
		}
		goto doString;
	    case 'x':
		if (eventPtr->type == CK_EV_MOUSE_UP ||
		    eventPtr->type == CK_EV_MOUSE_DOWN) {
		    number = eventPtr->mouse.x;
		}
		goto doNumber;
	    case 'y':
		if (eventPtr->type == CK_EV_MOUSE_UP ||
		    eventPtr->type == CK_EV_MOUSE_DOWN) {
		    number = eventPtr->mouse.y;
		}
		goto doNumber;
	    case 'b':
		if (eventPtr->type == CK_EV_MOUSE_UP ||
		    eventPtr->type == CK_EV_MOUSE_DOWN) {
		    number = eventPtr->mouse.button;
		}
		goto doNumber;
	    case 'X':
		if (eventPtr->type == CK_EV_MOUSE_UP ||
		    eventPtr->type == CK_EV_MOUSE_DOWN) {
		    number = eventPtr->mouse.rootx;
		}
		goto doNumber;
	    case 'Y':
		if (eventPtr->type == CK_EV_MOUSE_UP ||
		    eventPtr->type == CK_EV_MOUSE_DOWN) {
		    number = eventPtr->mouse.rooty;
		}
		goto doNumber;
	    default:
		numStorage[0] = before[1];
		numStorage[1] = '\0';
		string = numStorage;
		goto doString;
	}

	doNumber:
	sprintf(numStorage, "%d", number);
	string = numStorage;

	doString:
	spaceNeeded = Tcl_ScanElement(string, &cvtFlags);
        string2 = ckalloc(spaceNeeded + 1);
	spaceNeeded = Tcl_ConvertElement(string, string2,
		cvtFlags | TCL_DONT_USE_BRACES);
	Tcl_DStringAppend(dsPtr, string2, -1);
	ckfree((char *) string2);
	before += 2;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CkStringToKeysym --
 *
 *	This procedure finds the keysym associated with a given keysym
 *	name.
 *
 * Results:
 *	The return value is the keysym that corresponds to name, or
 *	NoSymbol if there is no such keysym.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

KeySym
CkStringToKeysym(name)
    char *name;			/* Name of a keysym. */
{
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&keySymTable, name);
    if (hPtr != NULL) {
	return ((KeySymInfo *) Tcl_GetHashValue(hPtr))->value;
    }
    return NoSymbol;
}

/*
 *----------------------------------------------------------------------
 *
 * CkKeysymToString --
 *
 *	This procedure finds the keysym name associated with a given
 *	keysym.
 *
 * Results:
 *	The return value is the keysym name that corresponds to name,
 *	or NoSymbol if there is no name.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
CkKeysymToString(keySym, printControl)
    KeySym keySym;
    int printControl;
{
    Tcl_HashEntry *hPtr;
    static char buffer[64];

    hPtr = Tcl_FindHashEntry(&revKeySymTable, (char *) keySym);
    if (hPtr != NULL) {
	return ((KeySymInfo *) Tcl_GetHashValue(hPtr))->name;
    }
    if (printControl && keySym >= 0x00 && keySym < 0x20) {
    	keySym += 0x40;
	sprintf(buffer, "Control-%c", (int) keySym);
	return buffer;
    }
    return printControl ? "NoSymbol" : NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * CkTermHasKey --
 *
 *	This procedure checks if the terminal has a key for given keysym.
 *
 * Results:
 *	TCL_OK or TCL_ERROR, a string is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
CkTermHasKey(interp, name)
    Tcl_Interp *interp;		/* Interpreter used for result. */
    char *name;			/* Name of a keysym. */
{
#ifndef __WIN32__
    Tcl_HashEntry *hPtr;
    char *tiname, *tivalue;
    extern char *tigetstr();
#endif
    char buf[8];

    if (strncmp("Control-", name, 8) == 0) {
	if (sscanf(name, "Control-%7s", buf) != 1 || strlen(buf) != 1)
	    goto error;
	if (buf[0] < 'A' && buf[0] > 'z')
	    goto error;
	interp->result = "1";
	return TCL_OK;
    }
#ifdef __WIN32__
    interp->result = "1";
    return TCL_OK;
#else
    hPtr = Tcl_FindHashEntry(&keySymTable, name);
    if (hPtr != NULL) {
tifind:
	tiname = ((KeySymInfo *) Tcl_GetHashValue(hPtr))->tiname;
	if (tiname == NULL || ((tivalue = tigetstr(tiname)) != NULL &&
	    tivalue != (char *) -1))
	    interp->result = "1";
	else
	    interp->result = "0";
	return TCL_OK;
    }
    if (strlen(name) == 1) {
	if (name[0] > 0x01 && name[0] < ' ') {
	    interp->result = "1";
	    return TCL_OK;
	}
	hPtr = Tcl_FindHashEntry(&revKeySymTable, (char *)
	    ((long) ((unsigned char) name[0])));
	if (hPtr != NULL)
	    goto tifind;
    }
#endif    
error:
    Tcl_AppendResult(interp, "invalid key symbol \"", name,
	"\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * CkAllKeyNames --
 *
 *	This procedure returns a list of all key names.
 *
 * Results:
 *	Always TCL_OK and list in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
CkAllKeyNames(interp)
    Tcl_Interp *interp;		/* Interpreter used for result. */
{
    KeySymInfo *kPtr;
    int i;

    for (i = 0x01; i < ' '; i++) {
	unsigned code;
	char buf[16];

	code = i + 'A' - 1;
	sprintf(buf, "Control-%c", tolower(code));
	Tcl_AppendElement(interp, buf);
    }
    for (kPtr = keyArray; kPtr->name != NULL; kPtr++) {
	Tcl_AppendElement(interp, kPtr->name);
    }
    return TCL_OK;
}
