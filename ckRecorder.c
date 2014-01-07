/* 
 * ckRecorder.c --
 *
 *	This file provides a simple event recorder.
 *
 * Copyright (c) 1996-1999 Christian Werner
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "ckPort.h"
#include "ck.h"

/*
 * There is one structure of the following type for the global data
 * of the recorder.
 */

typedef struct {
    CkWindow *mainPtr;
    Tcl_Interp *interp;
    int timerRunning;
    Tk_TimerToken timer;
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
    struct timeval lastEvent;
    FILE *record;
    FILE *replay;
#else
    Tcl_Time lastEvent;
    Tcl_Channel record;
    Tcl_Channel replay;
#endif
    int withDelay;
    CkEvent event;
} Recorder;

static Recorder *ckRecorder = NULL;

/*
 *   Internal procedures.
 */

static int	RecorderInput _ANSI_ARGS_((ClientData clientData,
		    CkEvent *eventPtr));
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
static int	DStringGets _ANSI_ARGS_((FILE *filePtr, Tcl_DString *dsPtr));
#else
static int	DStringGets _ANSI_ARGS_((Tcl_Channel chan,
		    Tcl_DString *dsPtr));
#endif
static void	DeliverEvent _ANSI_ARGS_((ClientData clientData));
static void	RecorderReplay _ANSI_ARGS_((ClientData clientData));

/*
 *----------------------------------------------------------------------
 *
 * RecorderInput --
 *
 *	This procedure is installed as generic event handler.
 *	For certain events it adds lines to the recorder file.
 *
 *----------------------------------------------------------------------
 */

static int
RecorderInput(clientData, eventPtr)
    ClientData clientData;
    CkEvent *eventPtr;
{
    Recorder *recPtr = (Recorder *) clientData;
    int hadEvent = 0, type = eventPtr->any.type;
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
    struct timeval now;
#else
    Tcl_Time now;
    extern void TclpGetTime _ANSI_ARGS_((Tcl_Time *timePtr));
#endif
    char buffer[64];
    char *keySym, *barCode, *result;
    char *argv[16];

    if (recPtr->record == NULL) {
	Ck_DeleteGenericHandler(RecorderInput, clientData);
	return 0;
    }

    if (type != CK_EV_KEYPRESS && type != CK_EV_BARCODE &&
    	type != CK_EV_MOUSE_UP && type != CK_EV_MOUSE_DOWN)
    	return 0;
    	
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
    gettimeofday(&now, (struct timezone *) NULL);
    if (recPtr->withDelay && recPtr->lastEvent.tv_sec != 0 &&
	recPtr->lastEvent.tv_usec != 0) {
	double diff;

	diff = now.tv_sec * 1000 + now.tv_usec / 1000;
	diff -= recPtr->lastEvent.tv_sec * 1000 +
	    recPtr->lastEvent.tv_usec / 1000;
	if (diff > 50) {
	    if (diff > 3600000)
		diff = 3600000;
	    fprintf(recPtr->record, "<Delay> %d\n", (int) diff);
	    hadEvent++;
	}
    }
#else
    TclpGetTime(&now);
    if (recPtr->withDelay && recPtr->lastEvent.sec != 0 &&
	recPtr->lastEvent.usec != 0) {
	double diff;
	char string[100];

	diff = now.sec * 1000 + now.usec / 1000;
	diff -= recPtr->lastEvent.sec * 1000 +
	    recPtr->lastEvent.usec / 1000;
	if (diff > 50) {
	    if (diff > 3600000)
		diff = 3600000;
	    sprintf(string, "<Delay> %d\n", (int) diff);
	    Tcl_Write(recPtr->record, string, strlen(string));
	    hadEvent++;
	}
    }
#endif

    switch (type) {
	case CK_EV_KEYPRESS:
	    argv[2] = NULL;
	    keySym = CkKeysymToString(eventPtr->key.keycode, 1);
	    if (strcmp(keySym, "NoSymbol") != 0)
		argv[2] = keySym;
	    else if (eventPtr->key.keycode > 0 &&
		eventPtr->key.keycode < 256) {
		/* Unsafe, ie not portable */
		sprintf(buffer, "0x%2x", eventPtr->key.keycode);
		argv[2] = buffer;
	    }
	    if (argv[2] != NULL) {
		argv[0] = "<Key>";
		argv[1] = eventPtr->key.winPtr == NULL ? "" :
		    eventPtr->key.winPtr->pathName;
		result = Tcl_Merge(3, argv);
printPctSNL:
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
		fprintf(recPtr->record, "%s\n", result);
#else
		Tcl_Write(recPtr->record, result, strlen(result));
		Tcl_Write(recPtr->record, "\n", 1);
#endif
		ckfree(result);
		hadEvent++;
	    }
	    break;

	case CK_EV_BARCODE:
	    barCode = CkGetBarcodeData(recPtr->mainPtr->mainPtr);
	    if (barCode != NULL) {
		argv[0] = "<BarCode>";
		argv[1] = eventPtr->key.winPtr == NULL ? "" :
		    eventPtr->key.winPtr->pathName;
		argv[2] = barCode;
		result = Tcl_Merge(3, argv);
	        goto printPctSNL;
	    }
	    break;

	case CK_EV_MOUSE_UP:
	case CK_EV_MOUSE_DOWN:
	    {
	        char bbuf[16], xbuf[16], ybuf[16], rxbuf[16], rybuf[16];

	        argv[0] = type == CK_EV_MOUSE_DOWN ?
		    "<ButtonPress>" : "<ButtonRelease>";
		argv[1] = eventPtr->mouse.winPtr == NULL ? "" :
		    eventPtr->mouse.winPtr->pathName;
		sprintf(bbuf, "%d", eventPtr->mouse.button);
		argv[2] = bbuf;
		sprintf(xbuf, "%d", eventPtr->mouse.x);
		argv[3] = xbuf;
		sprintf(ybuf, "%d", eventPtr->mouse.y);
		argv[4] = ybuf;
		sprintf(rxbuf, "%d", eventPtr->mouse.rootx);
		argv[5] = rxbuf;
		sprintf(rybuf, "%d", eventPtr->mouse.rooty);
		argv[6] = rybuf;
		result = Tcl_Merge(7, argv);
	        goto printPctSNL;
	    }
	    break;
    }

    if (hadEvent) {
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
	fflush(recPtr->record);
#else
	Tcl_Flush(recPtr->record);
#endif
	recPtr->lastEvent = now;
    }

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * DStringGets --
 *
 *	Similar to the fgets library routine, a dynamic string is
 *	read from a file. Can deal with backslash-newline continuation.
 *	lines.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */

#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
static int
DStringGets(filePtr, dsPtr)
    FILE *filePtr;
    Tcl_DString *dsPtr;
{
    int count, c, p = EOF;
    char buf;

    for (count = 0;;) {
	c = getc(filePtr);
	if (c == EOF)
	    return count ? TCL_OK : TCL_ERROR;
	else if (c == '\n') {
	    if (p == '\\')
		c = ' ';
	    else
		return TCL_OK;
	}
	buf = c;
	Tcl_DStringAppend(dsPtr, &buf, 1);
	p = c;
    }
    /* Not reached. */	
}
#else
static int
DStringGets(chan, dsPtr)
    Tcl_Channel chan;
    Tcl_DString *dsPtr;
{
    char *p;
    int length, code;

    for (;;) {
        code = Tcl_Gets(chan, dsPtr);
	length = Tcl_DStringLength(dsPtr);
	if (code == -1)
	    return length == 0 ? TCL_ERROR : TCL_OK;
	if (length > 0) {
	    p = Tcl_DStringValue(dsPtr) + length - 1;
	    if (*p != '\\')
	        return TCL_OK;
	    *p = ' ';
	} else {
	    return TCL_OK;
	}
    }
    /* Not reached. */	
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * DeliverEvent --
 *
 *	Call by do-when-idle mechanism, dispatched by replay handler.
 *	Deliver event, but first reschedule replay handler. This order
 *	is essential !
 *
 *----------------------------------------------------------------------
 */

static void
DeliverEvent(clientData)
    ClientData clientData;
{
    Recorder *recPtr = (Recorder *) clientData;

    Tk_DoWhenIdle(RecorderReplay, (ClientData) recPtr);
    Ck_HandleEvent(recPtr->mainPtr->mainPtr, &recPtr->event);
}

/*
 *----------------------------------------------------------------------
 *
 * RecorderReplay --
 *
 *	Replay handler, called by the do-when-idle mechanism or by a
 *	timer's expiration.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
RecorderReplay(clientData)
    ClientData clientData;
{
    Recorder *recPtr = (Recorder *) clientData;
    Tcl_DString input;
    char *p;
    int getsResult, delayValue = 0, doidle = 1;

    recPtr->timerRunning = 0;
    if (recPtr->replay == NULL)
	return;

    Tcl_DStringInit(&input);
    while ((getsResult = DStringGets(recPtr->replay, &input)) == TCL_OK) {
	p = Tcl_DStringValue(&input);
	while (*p == ' ' || *p == '\t')
	    ++p;
	if (*p == '#') {
	    Tcl_DStringTrunc(&input, 0);
	    continue;
	}
	if (*p == '<') {
	    CkEvent event;
	    int cmdError = TCL_OK, deliver = 0;
	    int argc;
	    char **argv;

	    if (Tcl_SplitList(recPtr->interp, p, &argc, &argv) != TCL_OK) {
		Tk_BackgroundError(recPtr->interp);
		getsResult = TCL_ERROR;
		break;
	    }
	    if (strcmp(argv[0], "<Delay>") == 0) {
		if (argc != 2) {
badNumArgs:
		    Tcl_AppendResult(recPtr->interp,
			"wrong # args for ", argv[0], (char *) NULL);
		    cmdError = TCL_ERROR;
		} else
		    cmdError = Tcl_GetInt(recPtr->interp, argv[1],
			&delayValue);
	    } else if (strcmp(argv[0], "<Key>") == 0) {
		int keySym;

		if (argc != 3)
		    goto badNumArgs;
		event.any.type = CK_EV_KEYPRESS;
		if (argv[1][0] == '\0')
		    event.any.winPtr = NULL;
		else if ((event.any.winPtr = Ck_NameToWindow(recPtr->interp,
		    argv[1], recPtr->mainPtr)) == NULL)
		    cmdError = TCL_ERROR;
		else if (strncmp(argv[2], "Control-", 8) == 0 &&
		    strlen(argv[2]) == 9) {
		    event.key.keycode = argv[2][8] - 0x40;
		    if (event.key.keycode > 0x20)
			event.key.keycode -= 0x20;
		    deliver++;
		} else if (strncmp(argv[2], "0x", 2) == 0 &&
		    strlen(argv[2]) == 4) {
		    sscanf(&argv[2][2], "%x", &event.key.keycode);
		    deliver++;
		} else if ((keySym = CkStringToKeysym(argv[2])) != NoSymbol) {
		    event.key.keycode = keySym;
		    deliver++;
		}
	    } else if (strcmp(argv[0], "<BarCode>") == 0) {
		if (argc != 3)
		    goto badNumArgs;

	    } else if (strcmp(argv[0], "<ButtonPress>") == 0) {
		if (argc != 7)
		    goto badNumArgs;
		event.any.type = CK_EV_MOUSE_DOWN;
doMouse:
		if (argv[1][0] == '\0')
		    event.any.winPtr = NULL;
		else if ((event.any.winPtr = Ck_NameToWindow(recPtr->interp,
		    argv[1], recPtr->mainPtr)) == NULL)
		    cmdError = TCL_ERROR;
		else {
		    cmdError |= Tcl_GetInt(recPtr->interp, argv[2],
		        &event.mouse.button);
		    cmdError |= Tcl_GetInt(recPtr->interp, argv[3],
		        &event.mouse.x);
		    cmdError |= Tcl_GetInt(recPtr->interp, argv[4],
		        &event.mouse.y);
		    cmdError |= Tcl_GetInt(recPtr->interp, argv[5],
		        &event.mouse.rootx);
		    cmdError |= Tcl_GetInt(recPtr->interp, argv[6],
		        &event.mouse.rooty);
		    if (cmdError == TCL_OK)
			deliver++;
		}
	    } else if (strcmp(argv[0], "<ButtonRelease>") == 0) {
		if (argc != 7)
		    goto badNumArgs;
		event.any.type = CK_EV_MOUSE_UP;
		goto doMouse;
	    }
	    ckfree((char *) argv);
	    if (cmdError != TCL_OK) {
		Tk_BackgroundError(recPtr->interp);
		getsResult = cmdError;
	    } else if (deliver) {
		doidle = delayValue = 0;
		recPtr->event = event;
		Tk_DoWhenIdle(DeliverEvent, (ClientData) recPtr);
	    }
	    break;
	} else if (Tcl_GlobalEval(recPtr->interp, p) != TCL_OK) {
	    Tk_BackgroundError(recPtr->interp);
	    getsResult = TCL_ERROR;
	    break;
	}
	Tcl_DStringTrunc(&input, 0);
    }
    if (getsResult != TCL_OK) {
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
	fclose(recPtr->replay);
#else
	Tcl_Close(NULL, recPtr->replay);
#endif
	recPtr->replay = NULL;
    } else if (delayValue != 0) {
	recPtr->timerRunning = 1;
	recPtr->timer = Tk_CreateTimerHandler(delayValue, RecorderReplay,
	    (ClientData) recPtr);
    } else if (doidle != 0) {
	Tk_DoWhenIdle(RecorderReplay, (ClientData) recPtr);
    }
    Tcl_DStringFree(&input);
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_RecorderCmd --
 *
 *	This procedure is invoked to process the "recorder" Tcl command.
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
Ck_RecorderCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Recorder *recPtr = ckRecorder;
    CkWindow *mainPtr = (CkWindow *) clientData;
    int length;
    char c;

    if (recPtr == NULL) {
	recPtr = (Recorder *) ckalloc(sizeof (Recorder));
	recPtr->mainPtr = mainPtr;
	recPtr->interp = NULL;
	recPtr->timerRunning = 0;
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
	recPtr->lastEvent.tv_sec = recPtr->lastEvent.tv_usec = 0;
#else
	recPtr->lastEvent.sec = recPtr->lastEvent.usec = 0;
#endif
	recPtr->record = NULL;
	recPtr->replay = NULL;
	recPtr->withDelay = 0;
	ckRecorder = recPtr;
    }

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
	    argv[0], " option ?arg?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'r') && (strncmp(argv[1], "replay", length) == 0)) {
	char *fileName;
	Tcl_DString buffer;
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
	FILE *newReplay;
#else
	Tcl_Channel newReplay;
#endif

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " replay fileName\"", (char *) NULL);
	    return TCL_ERROR;
	}

#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
	fileName = Tcl_TildeSubst(interp, argv[2], &buffer);
	if (fileName == NULL) {
replayError:
	    Tcl_DStringFree(&buffer);
	    return TCL_ERROR;
	}
	newReplay = fopen(fileName, "r");
	if (newReplay == NULL) {
	    Tcl_AppendResult(interp, "error opening \"", fileName,
	        "\": ", Tcl_PosixError(interp), (char *) NULL);
	    goto replayError;
	}
	Tcl_DStringFree(&buffer);
	DStringGets(newReplay, &buffer);
	if (strncmp("# CK-RECORDER", Tcl_DStringValue(&buffer), 13) != 0) {
	    fclose(newReplay);
	    Tcl_AppendResult(interp, "invalid file for replay", (char *) NULL);
	    goto replayError;
	}
#else
	fileName = Tcl_TranslateFileName(interp, argv[2], &buffer);
	if (fileName == NULL) {
replayError:
	    Tcl_DStringFree(&buffer);
	    return TCL_ERROR;
	}
	newReplay = Tcl_OpenFileChannel(interp, fileName, "r", 0);
	if (newReplay == NULL)
	    goto replayError;
	Tcl_DStringFree(&buffer);
	Tcl_Gets(newReplay, &buffer);
	if (strncmp("# CK-RECORDER", Tcl_DStringValue(&buffer), 13) != 0) {
	    Tcl_Close(NULL, newReplay);
	    Tcl_AppendResult(interp, "invalid file for replay", (char *) NULL);
	    goto replayError;
	}
#endif
	if (recPtr->replay != NULL) {
	    if (recPtr->timerRunning)
		Tk_DeleteTimerHandler(recPtr->timer);
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
	    fclose(recPtr->replay);
#else
	    Tcl_Close(NULL, recPtr->replay);
#endif
	    recPtr->timerRunning = 0;
	}
	recPtr->replay = newReplay;
	recPtr->interp = interp;
	Tk_DoWhenIdle(RecorderReplay, (ClientData) recPtr);
    } else if ((c == 's') && (strncmp(argv[1], "start", length) == 0) &&
	(length > 1)) {
	char *fileName;
	int withDelay = 0, fileArg = 2;
	Tcl_DString buffer;
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
	FILE *newRecord;
	time_t now;
#else
	Tcl_Channel newRecord;
	char *string;
#endif

	if (argc < 3 || argc > 4) {
badStartArgs:
	    Tcl_AppendResult(interp, "wrong # or bad args: should be \"",
		argv[0], " start ?-withdelay? fileName\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 4) {
	    if (strcmp(argv[2], "-withdelay") != 0)
		goto badStartArgs;
	    withDelay++;
	    fileArg++;
	}
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
	fileName = Tcl_TildeSubst(interp, argv[fileArg], &buffer);
	if (fileName == NULL) {
startError:
	    Tcl_DStringFree(&buffer);
	    return TCL_ERROR;
	}
	newRecord = fopen(fileName, "w");
	if (newRecord == NULL) {
	    Tcl_AppendResult(interp, "error opening \"", fileName,
	        "\": ", Tcl_PosixError(interp), (char *) NULL);
	    goto startError;
	}
	if (recPtr->record != NULL)
	    fclose(recPtr->record);
	else {
	    recPtr->lastEvent.tv_sec = recPtr->lastEvent.tv_usec = 0;
	    Ck_CreateGenericHandler(RecorderInput, recPtr);
	}
	recPtr->record = newRecord;
	recPtr->withDelay = withDelay;
	time(&now);
	fprintf(recPtr->record, "# CK-RECORDER\n# %s", ctime(&now));
	fprintf(recPtr->record, "# %s %s\n",
	    Tcl_GetVar(interp, "argv0", TCL_GLOBAL_ONLY),
	    Tcl_GetVar(interp, "argv", TCL_GLOBAL_ONLY));
	Tcl_DStringFree(&buffer);
#else
	fileName = Tcl_TranslateFileName(interp, argv[fileArg], &buffer);
	if (fileName == NULL) {
startError:
	    Tcl_DStringFree(&buffer);
	    return TCL_ERROR;
	}
	newRecord = Tcl_OpenFileChannel(interp, fileName, "w", 0666);
	if (newRecord == NULL)
	    goto startError;
	if (recPtr->record != NULL)
	    Tcl_Close(NULL, recPtr->record);
	else {
	    recPtr->lastEvent.sec = recPtr->lastEvent.usec = 0;
	    Ck_CreateGenericHandler(RecorderInput, (ClientData) recPtr);
	}
	recPtr->record = newRecord;
	recPtr->withDelay = withDelay;
	string = "# CK-RECORDER\n# ";
	Tcl_Write(recPtr->record, string, strlen(string));
	Tcl_Eval(interp, "clock format [clock seconds]");
	Tcl_Write(recPtr->record, interp->result, strlen(interp->result));
	Tcl_ResetResult(interp);
	Tcl_Write(recPtr->record, "\n# ", 3);
	string = Tcl_GetVar(interp, "argv0", TCL_GLOBAL_ONLY);
	Tcl_Write(recPtr->record, string, strlen(string));
	Tcl_Write(recPtr->record, " ", 1);
	string = Tcl_GetVar(interp, "argv", TCL_GLOBAL_ONLY);
	Tcl_Write(recPtr->record, string, strlen(string));
	Tcl_Write(recPtr->record, "\n", 1);
	Tcl_DStringFree(&buffer);
#endif
    } else if ((c == 's') && (strncmp(argv[1], "stop", length) == 0) &&
	(length > 1)) {
	if (argc > 3) {
badStopArgs:
	    Tcl_AppendResult(interp, "wrong # or bad args: should be \"",
		argv[0], " stop ?replay?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (strcmp(argv[2], "replay") != 0)
		goto badStopArgs;
	    if (recPtr->replay != NULL) {
		if (recPtr->timerRunning)
		    Tk_DeleteTimerHandler(recPtr->timer);
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
		fclose(recPtr->replay);
#else
		Tcl_Close(NULL, recPtr->replay);
#endif
		recPtr->replay = NULL;
		recPtr->timerRunning = 0;
	    }
	} else if (recPtr->record != NULL) {
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
	    fclose(recPtr->record);
#else
	    Tcl_Close(NULL, recPtr->record);
#endif
	    Ck_DeleteGenericHandler(RecorderInput, (ClientData) recPtr);
	    recPtr->record = NULL;
	}
    } else {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " replay, start, or stop\"", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}


