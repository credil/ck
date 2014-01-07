/* 
 * ckWindow.c --
 *
 *	This file provides basic window-manipulation procedures.
 *
 * Copyright (c) 1995-2002 Christian Werner.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "ckPort.h"
#include "ck.h"

#ifdef HAVE_GPM
#include "gpm.h"
#endif

/*
 * Main information.
 */

CkMainInfo *ckMainInfo = NULL;

#ifdef __WIN32__

/*
 * Curses input event handling information.
 */

typedef struct {
    HANDLE stdinHandle;
    HWND hwnd;
    HANDLE thread;
    CkMainInfo *mainPtr;
} InputInfo;

static InputInfo inputInfo = {
    INVALID_HANDLE_VALUE,
    NULL,
    INVALID_HANDLE_VALUE,
    NULL
};

static void		InputSetup _ANSI_ARGS_((InputInfo *inputInfo));
static void		InputExit _ANSI_ARGS_((ClientData clientData));
static void		InputThread _ANSI_ARGS_((void *arg));
static LRESULT CALLBACK InputHandler _ANSI_ARGS_((HWND hwnd, UINT message,
						 WPARAM wParam,
						 LPARAM lParam));
static void		InputHandler2 _ANSI_ARGS_((ClientData clientData));
#endif

#if (TCL_MAJOR_VERSION >= 8)
static void		CkEvtExit _ANSI_ARGS_((ClientData clientData));
static void		CkEvtSetup _ANSI_ARGS_((ClientData clientData,
						int flags));
static void		CkEvtCheck _ANSI_ARGS_((ClientData clientData,
						int flags));
#endif

/*
 * The variables below hold several uid's that are used in many places
 * in the toolkit.
 */

Ck_Uid ckDisabledUid = NULL;
Ck_Uid ckActiveUid = NULL;
Ck_Uid ckNormalUid = NULL;

/*
 * The following structure defines all of the commands supported by
 * the toolkit, and the C procedures that execute them.
 */

typedef int (CkCmdProc) _ANSI_ARGS_((ClientData clientData,
				     Tcl_Interp *interp,
				     int argc, char **argv));

typedef struct {
    char *name;				/* Name of command. */
    CkCmdProc *cmdProc;			/* Command procedure. */
} CkCmd;

CkCmd commands[] = {
    /*
     * Commands that are part of the intrinsics:
     */

#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
    {"after",		Tk_AfterCmd},
#endif
    {"bell",		Ck_BellCmd},
    {"bind",		Ck_BindCmd},
    {"bindtags",	Ck_BindtagsCmd},
    {"curses",          Ck_CursesCmd},
    {"destroy",		Ck_DestroyCmd},
    {"exit",		Ck_ExitCmd},
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
    {"fileevent",	Tk_FileeventCmd},
#endif
    {"focus",		Ck_FocusCmd},
    {"grid",		Ck_GridCmd},
    {"lower",		Ck_LowerCmd},
    {"option",		Ck_OptionCmd},
    {"pack",		Ck_PackCmd},
    {"place",		Ck_PlaceCmd},
    {"raise",		Ck_RaiseCmd},
    {"recorder",	Ck_RecorderCmd},
    {"tkwait",		Ck_TkwaitCmd},
    {"update",		Ck_UpdateCmd},
    {"winfo",		Ck_WinfoCmd},

    /*
     * Widget-creation commands.
     */

    {"button",		Ck_ButtonCmd},
    {"checkbutton",	Ck_ButtonCmd},
    {"entry",		Ck_EntryCmd},
    {"frame",		Ck_FrameCmd},
    {"label",		Ck_ButtonCmd},
    {"listbox",		Ck_ListboxCmd},
    {"menu",		Ck_MenuCmd},
    {"menubutton",	Ck_MenubuttonCmd},
    {"message",		Ck_MessageCmd},
    {"radiobutton",	Ck_ButtonCmd},
    {"scrollbar",	Ck_ScrollbarCmd},
    {"text",		Ck_TextCmd},
    {"toplevel",	Ck_FrameCmd},
    {"tree",		Ck_TreeCmd},

    {(char *) NULL,	(CkCmdProc *) NULL}
};

/*
 * Static procedures of this module.
 */

static void	UnlinkWindow _ANSI_ARGS_((CkWindow *winPtr));
static void	UnlinkToplevel _ANSI_ARGS_((CkWindow *winPtr));
static void     ChangeToplevelFocus _ANSI_ARGS_((CkWindow *winPtr));
static void	DoRefresh _ANSI_ARGS_((ClientData clientData));
static void	RefreshToplevels _ANSI_ARGS_((CkWindow *winPtr));
static void	RefreshThem _ANSI_ARGS_((CkWindow *winPtr));
static void     UpdateHWCursor _ANSI_ARGS_((CkMainInfo *mainPtr));
static CkWindow *GetWindowXY _ANSI_ARGS_((CkWindow *winPtr, int *xPtr,
			int *yPtr));
static int	DeadAppCmd _ANSI_ARGS_((ClientData clientData,
			Tcl_Interp *interp, int argc, char **argv));
static int      ExecCmd _ANSI_ARGS_((ClientData clientData,
			Tcl_Interp *interp, int argc, char **argv));
static int      PutsCmd _ANSI_ARGS_((ClientData clientData,
			Tcl_Interp *interp, int argc, char **argv));
static int      CloseCmd _ANSI_ARGS_((ClientData clientData,
			Tcl_Interp *interp, int argc, char **argv));
static int      FlushCmd _ANSI_ARGS_((ClientData clientData,
			Tcl_Interp *interp, int argc, char **argv));
static int      ReadCmd _ANSI_ARGS_((ClientData clientData,
			Tcl_Interp *interp, int argc, char **argv));
static int      GetsCmd _ANSI_ARGS_((ClientData clientData,
			Tcl_Interp *interp, int argc, char **argv));

/*
 * Some plain Tcl commands are handled specially.
 */

CkCmd redirCommands[] = {
#ifndef __WIN32__
    {"exec",    ExecCmd},
#endif
    {"puts",    PutsCmd},
    {"close",   CloseCmd},
    {"flush",   FlushCmd},
    {"read",    ReadCmd},
    {"gets",    GetsCmd},
    {(char *) NULL, (CkCmdProc *) NULL}
};

/*
 * The following structure is used as ClientData for redirected
 * plain Tcl commands.
 */

typedef struct {
    CkMainInfo *mainPtr;
    Tcl_CmdInfo cmdInfo;
} RedirInfo;

/*
 *--------------------------------------------------------------
 *
 * NewWindow --
 *
 *	This procedure creates and initializes a CkWindow structure.
 *
 * Results:
 *	The return value is a pointer to the new window.
 *
 * Side effects:
 *	A new window structure is allocated and all its fields are
 *	initialized.
 *
 *--------------------------------------------------------------
 */

static CkWindow *
NewWindow(parentPtr)
    CkWindow *parentPtr;
{
    CkWindow *winPtr;

    winPtr = (CkWindow *) ckalloc(sizeof (CkWindow));
    winPtr->window = NULL;
    winPtr->childList = NULL;
    winPtr->lastChildPtr = NULL;
    winPtr->parentPtr = NULL;
    winPtr->nextPtr = NULL;
    winPtr->topLevPtr = NULL;
    winPtr->mainPtr = NULL;
    winPtr->pathName = NULL;
    winPtr->nameUid = NULL;
    winPtr->classUid = NULL;
    winPtr->handlerList = NULL;
    winPtr->tagPtr = NULL;
    winPtr->numTags = 0;
    winPtr->focusPtr = NULL;
    winPtr->geomMgrPtr = NULL;
    winPtr->geomData = NULL;
    winPtr->optionLevel = -1;
    winPtr->reqWidth = winPtr->reqHeight = 1;
    winPtr->x = winPtr->y = 0;
    winPtr->width = winPtr->height = 1;
    winPtr->fg = COLOR_WHITE;
    winPtr->bg = COLOR_BLACK;
    winPtr->attr = A_NORMAL;
    winPtr->flags = 0;

    return winPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * NameWindow --
 *
 *	This procedure is invoked to give a window a name and insert
 *	the window into the hierarchy associated with a particular
 *	application.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *      See above.
 *
 *----------------------------------------------------------------------
 */

static int
NameWindow(interp, winPtr, parentPtr, name)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    CkWindow *winPtr;		/* Window that is to be named and inserted. */
    CkWindow *parentPtr;	/* Pointer to logical parent for winPtr
				 * (used for naming, options, etc.). */
    char *name;			/* Name for winPtr;   must be unique among
				 * parentPtr's children. */
{
#define FIXED_SIZE 200
    char staticSpace[FIXED_SIZE];
    char *pathName;
    int new;
    Tcl_HashEntry *hPtr;
    int length1, length2;

    /*
     * Setup all the stuff except name right away, then do the name stuff
     * last.  This is so that if the name stuff fails, everything else
     * will be properly initialized (needed to destroy the window cleanly
     * after the naming failure).
     */
    winPtr->parentPtr = parentPtr;
    winPtr->nextPtr = NULL;
    if (parentPtr->childList == NULL) {
	parentPtr->lastChildPtr = winPtr;
	parentPtr->childList = winPtr;
    } else {
	parentPtr->lastChildPtr->nextPtr = winPtr;
	parentPtr->lastChildPtr = winPtr;
    }
    winPtr->mainPtr = parentPtr->mainPtr;
    winPtr->nameUid = Ck_GetUid(name);

    /*
     * Don't permit names that start with an upper-case letter:  this
     * will just cause confusion with class names in the option database.
     */

    if (isupper((unsigned char) name[0])) {
	Tcl_AppendResult(interp,
		"window name starts with an upper-case letter: \"",
		name, "\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * To permit names of arbitrary length, must be prepared to malloc
     * a buffer to hold the new path name.  To run fast in the common
     * case where names are short, use a fixed-size buffer on the
     * stack.
     */

    length1 = strlen(parentPtr->pathName);
    length2 = strlen(name);
    if ((length1+length2+2) <= FIXED_SIZE) {
	pathName = staticSpace;
    } else {
	pathName = (char *) ckalloc((unsigned) (length1+length2+2));
    }
    if (length1 == 1) {
	pathName[0] = '.';
	strcpy(pathName+1, name);
    } else {
	strcpy(pathName, parentPtr->pathName);
	pathName[length1] = '.';
	strcpy(pathName+length1+1, name);
    }
    hPtr = Tcl_CreateHashEntry(&parentPtr->mainPtr->nameTable, pathName, &new);
    if (pathName != staticSpace) {
	ckfree(pathName);
    }
    if (!new) {
	Tcl_AppendResult(interp, "window name \"", name,
		"\" already exists in parent", (char *) NULL);
	return TCL_ERROR;
    }
    Tcl_SetHashValue(hPtr, winPtr);
    winPtr->pathName = Tcl_GetHashKey(&parentPtr->mainPtr->nameTable, hPtr);
    Tcl_CreateHashEntry(&parentPtr->mainPtr->winTable, (char *) winPtr, &new);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_MainWindow --
 *
 *	Returns the main window for an application.
 *
 * Results:
 *	If interp is associated with the main window, the main
 *	window is returned. Otherwise NULL is returned and an
 *	error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

CkWindow *
Ck_MainWindow(interp)
    Tcl_Interp *interp;		/* Interpreter that embodies application,
    				 * also used for error reporting. */
{
    if (ckMainInfo == NULL || ckMainInfo->interp != interp) {
	if (interp != NULL)
	    interp->result = "no main window for application.";
	return NULL;
    }
    return ckMainInfo->winPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_CreateMainWindow --
 *
 *	Make the main window.
 *
 * Results:
 *	The return value is a token for the new window, or NULL if
 *	an error prevented the new window from being created.  If
 *	NULL is returned, an error message will be left in
 *	interp->result.
 *
 * Side effects:
 *	A new window structure is allocated locally.
 *
 *----------------------------------------------------------------------
 */

CkWindow *
Ck_CreateMainWindow(interp, className)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    char *className;		/* Class name of the new main window. */
{
    int dummy;
    Tcl_HashEntry *hPtr;
    CkMainInfo *mainPtr;
    CkWindow *winPtr;
    CkCmd *cmdPtr;
#ifdef SIGTSTP
#ifdef HAVE_SIGACTION
    struct sigaction oldsig, newsig;
#else
    Ck_SignalProc sigproc;
#endif
#endif
#ifdef NCURSES_MOUSE_VERSION
    MEVENT mEvent;
#endif
    char *term;
    int isxterm = 0;

    /*
     * For now, only one main window may exists for the application.
     */
    if (ckMainInfo != NULL)
        return NULL;

    /*
     * Create the basic CkWindow structure.
     */

    winPtr = NewWindow(NULL);
    
    /*
     * Create the CkMainInfo structure for this application, and set
     * up name-related information for the new window.
     */

    mainPtr = (CkMainInfo *) ckalloc(sizeof(CkMainInfo));
    mainPtr->winPtr = winPtr;
    mainPtr->interp = interp;
    Tcl_InitHashTable(&mainPtr->nameTable, TCL_STRING_KEYS);
    Tcl_InitHashTable(&mainPtr->winTable, TCL_ONE_WORD_KEYS);
    mainPtr->topLevPtr = NULL;
    mainPtr->focusPtr = winPtr;
    mainPtr->bindingTable = Ck_CreateBindingTable(interp);
    mainPtr->optionRootPtr = NULL;
    mainPtr->refreshCount = 0;
    mainPtr->refreshDelay = 0;
    mainPtr->lastRefresh = 0;
    mainPtr->refreshTimer = NULL;
    mainPtr->flags = 0;
    ckMainInfo = mainPtr;
    winPtr->mainPtr = mainPtr;
    winPtr->nameUid = Ck_GetUid(".");
    winPtr->classUid = Ck_GetUid("Main"); /* ??? */
    winPtr->flags |= CK_TOPLEVEL;
    hPtr = Tcl_CreateHashEntry(&mainPtr->nameTable, (char *) winPtr->nameUid,
		&dummy);
    Tcl_SetHashValue(hPtr, winPtr);
    winPtr->pathName = Tcl_GetHashKey(&mainPtr->nameTable, hPtr);
    Tcl_CreateHashEntry(&mainPtr->winTable, (char *) winPtr, &dummy);

    ckNormalUid = Ck_GetUid("normal");
    ckDisabledUid = Ck_GetUid("disabled");
    ckActiveUid = Ck_GetUid("active");

#if CK_USE_UTF
#ifdef __WIN32__
    {
	char enc[32], *envcp = getenv("CK_USE_ENCODING");
	unsigned int cp = GetConsoleCP();

	if (envcp && strncmp(envcp, "cp", 2) == 0) {
	    cp = atoi(envcp + 2);
	    SetConsoleCP(cp);
	    cp = GetConsoleCP();
	}
	if (GetConsoleOutputCP() != cp) {
	    SetConsoleOutputCP(cp);
	}
	sprintf(enc, "cp%d", cp);
	mainPtr->isoEncoding = Tcl_GetEncoding(NULL, enc);
    }
#else
    /*
     * Use default system encoding as suggested by
     * Anton Kovalenko <a_kovalenko@mtu-net.ru>.
     * May be overriden by environment variable.
     */
    mainPtr->isoEncoding = Tcl_GetEncoding(NULL, getenv("CK_USE_ENCODING"));
    if (mainPtr->isoEncoding == NULL) {
	mainPtr->isoEncoding = Tcl_GetEncoding(NULL, NULL);
    }
#endif
    if (mainPtr->isoEncoding == NULL) {
	panic("standard encoding not found");
    }
    if (strcmp(Tcl_GetEncodingName(mainPtr->isoEncoding), "utf-8") == 0) {
	Tcl_FreeEncoding(mainPtr->isoEncoding);
	mainPtr->isoEncoding = NULL;
    }
#endif

    /* Curses related initialization */

#ifdef SIGTSTP
    /* This is essential for ncurses-1.9.4 */
#ifdef HAVE_SIGACTION
    newsig.sa_handler = SIG_IGN;
    sigfillset(&newsig.sa_mask);
    newsig.sa_flags = 0;
    sigaction(SIGTSTP, &newsig, &oldsig);
#else
    sigproc = (Ck_SignalProc) signal(SIGTSTP, SIG_IGN);
#endif
#endif

#ifndef __WIN32__
    /*
     * Fix for problem when X server left linux console
     * in non-blocking mode
     */
    fcntl(0, F_SETFL, fcntl(0, F_GETFL) & (~O_NDELAY));
#endif

    if (initscr() == (WINDOW *) ERR) {
    	ckfree((char *) winPtr);
    	return NULL;
    }
#ifdef SIGTSTP
    /* This is essential for ncurses-1.9.4 */
#ifdef HAVE_SIGACTION
    sigaction(SIGTSTP, &oldsig, NULL);
#else
    signal(SIGTSTP, sigproc);
#endif
#endif
    raw();
    noecho();
    idlok(stdscr, TRUE);
    scrollok(stdscr, FALSE);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    meta(stdscr, TRUE);
    nonl();
    mainPtr->maxWidth = COLS;
    mainPtr->maxHeight = LINES;
    winPtr->width = mainPtr->maxWidth;
    winPtr->height = mainPtr->maxHeight;
    winPtr->window = newwin(winPtr->height, winPtr->width, 0, 0);
    if (has_colors()) {
    	start_color();
    	mainPtr->flags |= CK_HAS_COLOR;
    }
#ifdef NCURSES_MOUSE_VERSION
    mouseinterval(1);
    mousemask(BUTTON1_PRESSED | BUTTON1_RELEASED |
	      BUTTON2_PRESSED | BUTTON2_RELEASED |
	      BUTTON3_PRESSED | BUTTON3_RELEASED, NULL);
    mainPtr->flags |= (getmouse(&mEvent) != ERR) ? CK_HAS_MOUSE : 0;
#endif	/* NCURSES_MOUSE_VERSION */

#ifdef __WIN32__
    mouse_set(BUTTON1_PRESSED | BUTTON1_RELEASED |
	      BUTTON2_PRESSED | BUTTON2_RELEASED |
	      BUTTON3_PRESSED | BUTTON3_RELEASED);
    mainPtr->flags |= CK_HAS_MOUSE;
    term = "win32";
#else
    term = getenv("TERM");
    isxterm = strncmp(term, "xterm", 5) == 0 ||
	strncmp(term, "rxvt", 4) == 0 ||
	strncmp(term, "kterm", 5) == 0 ||
	strncmp(term, "color_xterm", 11) == 0 ||
	(term[0] != '\0' && strncmp(term + 1, "xterm", 5) == 0);
    if (!(mainPtr->flags & CK_HAS_MOUSE) && isxterm) {
	mainPtr->flags |= CK_HAS_MOUSE | CK_MOUSE_XTERM;
	fflush(stdout);
	fputs("\033[?1000h", stdout);
	fflush(stdout);
    }
#endif	/* __WIN32__ */

#ifdef HAVE_GPM
    /*
     * Some ncurses aren't compiled with GPM support built in,
     * therefore by setting the following environment variable
     * usage of GPM can be turned on.
     */
    if (!isxterm && (mainPtr->flags & CK_HAS_MOUSE)) {
	char *forcegpm = getenv("CK_USE_GPM");

	if (forcegpm && strchr("YyTt123456789", forcegpm[0])) {
	    mainPtr->flags &= ~CK_HAS_MOUSE;
	}
    }
    if (!isxterm && !(mainPtr->flags & CK_HAS_MOUSE)) {
	int fd;
	Gpm_Connect conn;
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
	EXTERN int CkHandleGPMInput _ANSI_ARGS_((ClientData clientData,
	    int mask, int flags));
#else
	EXTERN void CkHandleGPMInput _ANSI_ARGS_((ClientData clientData,
	    int mask));
#endif

	conn.eventMask = GPM_DOWN | GPM_UP | GPM_MOVE;
	conn.defaultMask = 0;
	conn.minMod = 0;
	conn.maxMod = 0;
	fd = Gpm_Open(&conn, 0);
	if (fd >= 0) {
	    mainPtr->flags |= CK_HAS_MOUSE;
#if (TCL_MAJOR_VERSION >= 8)
	    mainPtr->mouseData = (ClientData) fd;
	    Tcl_CreateFileHandler(fd, TCL_READABLE,
				  CkHandleGPMInput, (ClientData) mainPtr);
#else	    
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
	    mainPtr->mouseData = (ClientData) fd;
	    Tk_CreateFileHandler2(fd, CkHandleGPMInput, (ClientData) mainPtr);
#else
	    mainPtr->mouseData = (ClientData)
	        Tcl_GetFile((ClientData) fd, TCL_UNIX_FD);
	    Tcl_CreateFileHandler((Tcl_File) mainPtr->mouseData, TCL_READABLE,
				  CkHandleGPMInput, (ClientData) mainPtr);
#endif
#endif
	}
    }
#endif	/* HAVE_GPM */

#ifdef __WIN32__
    /* PDCurses specific !!! */
    inputInfo.mainPtr = mainPtr;
    inputInfo.stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
    typeahead(-1);
    SetConsoleMode(inputInfo.stdinHandle,
		   ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT);
    InputSetup(&inputInfo);
#else
#if (TCL_MAJOR_VERSION >= 8)
    Tcl_CreateFileHandler(0, 
	TCL_READABLE, CkHandleInput, (ClientData) mainPtr);
#else
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
    Tk_CreateFileHandler2(0, CkHandleInput, (ClientData) mainPtr);
#else
    Tcl_CreateFileHandler(Tcl_GetFile((ClientData) 0, TCL_UNIX_FD),
			  TCL_READABLE, CkHandleInput, (ClientData) mainPtr);
#endif
#endif
#endif

#if (TCL_MAJOR_VERSION >= 8)
    Tcl_CreateEventSource(CkEvtSetup, CkEvtCheck, (ClientData) mainPtr);
    Tcl_CreateExitHandler(CkEvtExit, (ClientData) mainPtr);
#endif

    idlok(winPtr->window, TRUE);
    scrollok(winPtr->window, FALSE);
    keypad(winPtr->window, TRUE);
    nodelay(winPtr->window, TRUE);
    meta(winPtr->window, TRUE);
    curs_set(0);
    while (getch() != ERR) {
	/* empty loop body. */
    }
    winPtr->flags |= CK_MAPPED;
    Ck_SetWindowAttr(winPtr, winPtr->fg, winPtr->bg, winPtr->attr);
    Ck_ClearToBot(winPtr, 0, 0);
    Ck_EventuallyRefresh(winPtr);

    /*
     * Bind in Ck's commands.
     */

    for (cmdPtr = commands; cmdPtr->name != NULL; cmdPtr++) {
	Tcl_CreateCommand(interp, cmdPtr->name, cmdPtr->cmdProc,
		(ClientData) winPtr, (Tcl_CmdDeleteProc *) NULL);
    }

    /*
     * Redirect some critical Tcl commands to our own procedures
     */
    for (cmdPtr = redirCommands; cmdPtr->name != NULL; cmdPtr++) {
        RedirInfo *redirInfo;
#if (TCL_MAJOR_VERSION >= 8)
	Tcl_DString cmdName;
	extern int TclRenameCommand _ANSI_ARGS_((Tcl_Interp *interp,
						 char *oldName, char *newName));
#endif
        redirInfo = (RedirInfo *) ckalloc(sizeof (RedirInfo));
        redirInfo->mainPtr = mainPtr;
        Tcl_GetCommandInfo(interp, cmdPtr->name, &redirInfo->cmdInfo);
#if (TCL_MAJOR_VERSION >= 8)
	Tcl_DStringInit(&cmdName);
	Tcl_DStringAppend(&cmdName, "____", -1);
	Tcl_DStringAppend(&cmdName, cmdPtr->name, -1);
	TclRenameCommand(interp, cmdPtr->name, Tcl_DStringValue(&cmdName));
	Tcl_DStringFree(&cmdName);
#endif
        Tcl_CreateCommand(interp, cmdPtr->name, cmdPtr->cmdProc,
            (ClientData) redirInfo, (Tcl_CmdDeleteProc *) free);
    }

    /*
     * Set variables for the intepreter.
     */

#if (TCL_MAJOR_VERSION < 8)
    if (Tcl_GetVar(interp, "ck_library", TCL_GLOBAL_ONLY) == NULL) {
        /*
         * A library directory hasn't already been set, so figure out
         * which one to use.
         */

	char *libDir = getenv("CK_LIBRARY");

        if (libDir == NULL) {
            libDir = CK_LIBRARY;
        }
        Tcl_SetVar(interp, "ck_library", libDir, TCL_GLOBAL_ONLY);
    }
#endif
    Tcl_SetVar(interp, "ck_version", CK_VERSION, TCL_GLOBAL_ONLY);

    /*
     * Make main window into a frame widget.
     */

    Ck_SetClass(winPtr, className);
    CkInitFrame(interp, winPtr, 0, NULL);
    mainPtr->topLevPtr = winPtr;
    winPtr->focusPtr = winPtr;
    return winPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_Init --
 *
 *      This procedure is invoked to add Ck to an interpreter.  It
 *      incorporates all of Ck's commands into the interpreter and
 *      creates the main window for a new Ck application.
 *
 * Results:
 *      Returns a standard Tcl completion code and sets interp->result
 *      if there is an error.
 *
 * Side effects:
 *      Depends on what's in the ck.tcl script.
 *
 *----------------------------------------------------------------------
 */

int
Ck_Init(interp)
    Tcl_Interp *interp;         /* Interpreter to initialize. */
{
    CkWindow *mainWindow;
    char *p, *name, *class;
    int code;
    static char initCmd[] =
#if (TCL_MAJOR_VERSION >= 8)
"proc init {} {\n\
    global ck_library ck_version\n\
    rename init {}\n\
    tcl_findLibrary ck $ck_version 0 ck.tcl CK_LIBRARY ck_library\n\
}\n\
init";
#else
"proc init {} {\n\
    global ck_library ck_version env\n\
    rename init {}\n\
    set dirs {}\n\
    if [info exists env(CK_LIBRARY)] {\n\
        lappend dirs $env(CK_LIBRARY)\n\
    }\n\
    lappend dirs $ck_library\n\
    lappend dirs [file dirname [info library]]/lib/ck$ck_version\n\
    catch {lappend dirs [file dirname [file dirname \\\n\
        [info nameofexecutable]]]/lib/ck$ck_version}\n\
    set lib ck$ck_version\n\
    lappend dirs [file dirname [file dirname [pwd]]]/$lib/library\n\
    lappend dirs [file dirname [file dirname [info library]]]/$lib/library\n\
    lappend dirs [file dirname [pwd]]/library\n\
    foreach i $dirs {\n\
        set ck_library $i\n\
        if ![catch {uplevel #0 source $i/ck.tcl}] {\n\
            return\n\
        }\n\
    }\n\
    set msg \"Can't find a usable ck.tcl in the following directories: \n\"\n\
    append msg \"    $dirs\n\"\n\
    append msg \"This probably means that Ck wasn't installed properly.\n\"\n\
    error $msg\n\
}\n\
init";
#endif

    p = Tcl_GetVar(interp, "argv0", TCL_GLOBAL_ONLY);
    if (p == NULL || *p == '\0')
        p = "Ck";
    name = strrchr(p, '/');
    if (name != NULL)
        name++;
    else
        name = p;
    class = (char *) ckalloc((unsigned) (strlen(name) + 1));
    strcpy(class, name);
    class[0] = toupper((unsigned char) class[0]);
    mainWindow = Ck_CreateMainWindow(interp, class);
    ckfree(class);

#if !((TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4))
    if (Tcl_PkgRequire(interp, "Tcl", TCL_VERSION, 0) == NULL)
        return TCL_ERROR;
    code = Tcl_PkgProvide(interp, "Ck", CK_VERSION);
    if (code != TCL_OK)
        return TCL_ERROR;
#endif
    return Tcl_Eval(interp, initCmd);
}

/*
 *--------------------------------------------------------------
 *
 * Ck_CreateWindow --
 *
 *	Create a new window as a child of an existing window.
 *
 * Results:
 *	The return value is the pointer to the new window.
 *	If an error occurred in creating the window, then an
 *	error message is left in interp->result and NULL is
 *	returned.
 *
 * Side effects:
 *	A new window structure is allocated locally.  A curses
 *	window is not initially created, but will be created
 *	the first time the window is mapped.
 *
 *--------------------------------------------------------------
 */

CkWindow *
Ck_CreateWindow(interp, parentPtr, name, toplevel)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting.
				 * Interp->result is assumed to be
				 * initialized by the caller. */
    CkWindow *parentPtr;	/* Parent of new window. */
    char *name;			/* Name for new window.  Must be unique
				 * among parent's children. */
    int toplevel;               /* If true, create toplevel window. */
{
    CkWindow *winPtr;

    winPtr = NewWindow(parentPtr);
    if (NameWindow(interp, winPtr, parentPtr, name) != TCL_OK) {
	Ck_DestroyWindow(winPtr);
	return NULL;
    }
    if (toplevel) {
	CkWindow *wPtr;

	winPtr->flags |= CK_TOPLEVEL;
	winPtr->focusPtr = winPtr;
	if (winPtr->mainPtr->topLevPtr == NULL) {
	    winPtr->topLevPtr = winPtr->mainPtr->topLevPtr;
	    winPtr->mainPtr->topLevPtr = winPtr;
	} else {
	    for (wPtr = winPtr->mainPtr->topLevPtr; wPtr->topLevPtr != NULL;
						    wPtr = wPtr->topLevPtr) {
		/* Empty loop body. */
	    }
	    winPtr->topLevPtr = wPtr->topLevPtr;
	    wPtr->topLevPtr = winPtr;
	}
    }
    return winPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_CreateWindowFromPath --
 *
 *	This procedure is similar to Ck_CreateWindow except that
 *	it uses a path name to create the window, rather than a
 *	parent and a child name.
 *
 * Results:
 *	The return value is the pointer to the new window.
 *	If an error occurred in creating the window, then an
 *	error message is left in interp->result and NULL is
 *	returned.
 *
 * Side effects:
 *	A new window structure is allocated locally.  A curses
 *	window is not initially created, but will be created
 *	the first time the window is mapped.
 *
 *----------------------------------------------------------------------
 */

CkWindow *
Ck_CreateWindowFromPath(interp, anywin, pathName, toplevel)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting.
				 * Interp->result is assumed to be
				 * initialized by the caller. */
    CkWindow *anywin;		/* Pointer to any window in application
				 * that is to contain new window. */
    char *pathName;		/* Path name for new window within the
				 * application of anywin. The parent of
				 * this window must already exist, but
				 * the window itself must not exist. */
    int toplevel;               /* If true, create toplevel window. */
{
#define FIXED_SPACE 5
    char fixedSpace[FIXED_SPACE+1];
    char *p;
    CkWindow *parentPtr, *winPtr;
    int numChars;

    /*
     * Strip the parent's name out of pathName (it's everything up
     * to the last dot).  There are two tricky parts: (a) must
     * copy the parent's name somewhere else to avoid modifying
     * the pathName string (for large names, space for the copy
     * will have to be malloc'ed);  (b) must special-case the
     * situation where the parent is ".".
     */

    p = strrchr(pathName, '.');
    if (p == NULL) {
	Tcl_AppendResult(interp, "bad window path name \"", pathName,
		"\"", (char *) NULL);
	return NULL;
    }
    numChars = p - pathName;
    if (numChars > FIXED_SPACE) {
	p = (char *) ckalloc((unsigned) (numChars+1));
    } else {
	p = fixedSpace;
    }
    if (numChars == 0) {
	*p = '.';
	p[1] = '\0';
    } else {
	strncpy(p, pathName, numChars);
	p[numChars] = '\0';
    }

    /*
     * Find the parent window.
     */

    parentPtr = Ck_NameToWindow(interp, p, anywin);
    if (p != fixedSpace) {
	ckfree(p);
    }
    if (parentPtr == NULL)
	return NULL;

    /*
     * Create the window.
     */

    winPtr = NewWindow(parentPtr);
    if (NameWindow(interp, winPtr, parentPtr, pathName + numChars + 1)
	!= TCL_OK) {
	Ck_DestroyWindow(winPtr);
	return NULL;
    }
    if (toplevel) {
	CkWindow *wPtr;

	winPtr->flags |= CK_TOPLEVEL;
	winPtr->focusPtr = winPtr;
	if (winPtr->mainPtr->topLevPtr == NULL) {
	    winPtr->topLevPtr = winPtr->mainPtr->topLevPtr;
	    winPtr->mainPtr->topLevPtr = winPtr;
	} else {
	    for (wPtr = winPtr->mainPtr->topLevPtr; wPtr->topLevPtr != NULL;
						    wPtr = wPtr->topLevPtr) {
		/* Empty loop body. */
	    }
	    winPtr->topLevPtr = wPtr->topLevPtr;
	    wPtr->topLevPtr = winPtr;
	}
    }
    return winPtr;
}

/*
 *--------------------------------------------------------------
 *
 * Ck_DestroyWindow --
 *
 *	Destroy an existing window.  After this call, the caller
 *	should never again use the pointer.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is deleted, along with all of its children.
 *	Relevant callback procedures are invoked.
 *
 *--------------------------------------------------------------
 */

void
Ck_DestroyWindow(winPtr)
    CkWindow *winPtr;		/* Window to destroy. */
{
    CkWindowEvent event;
    Tcl_HashEntry *hPtr;
#ifdef NCURSES_MOUSE_VERSION
    MEVENT mEvent;
#endif

    if (winPtr->flags & CK_ALREADY_DEAD)
	return;
    winPtr->flags |= CK_ALREADY_DEAD;

    /*
     * Recursively destroy children.  The CK_RECURSIVE_DESTROY
     * flags means that the child's window needn't be explicitly
     * destroyed (the destroy of the parent already did it), nor
     * does it need to be removed from its parent's child list,
     * since the parent is being destroyed too.
     */

    while (winPtr->childList != NULL) {
	winPtr->childList->flags |= CK_RECURSIVE_DESTROY;
	Ck_DestroyWindow(winPtr->childList);
    }
    if (winPtr->mainPtr->focusPtr == winPtr) {
	event.type = CK_EV_FOCUSOUT;
	event.winPtr = winPtr;
	Ck_HandleEvent(winPtr->mainPtr, (CkEvent *) &event);
    }
    if (winPtr->window != NULL) {
	delwin(winPtr->window);
	winPtr->window = NULL;
    }
    CkOptionDeadWindow(winPtr);
    event.type = CK_EV_DESTROY;
    event.winPtr = winPtr;
    Ck_HandleEvent(winPtr->mainPtr, (CkEvent *) &event);
    if (winPtr->tagPtr != NULL) {
        CkFreeBindingTags(winPtr);
    }
    UnlinkWindow(winPtr);
    CkEventDeadWindow(winPtr);
    hPtr = Tcl_FindHashEntry(&winPtr->mainPtr->winTable, (char *) winPtr);
    if (hPtr != NULL)
	Tcl_DeleteHashEntry(hPtr);
    if (winPtr->pathName != NULL) {
	CkMainInfo *mainPtr = winPtr->mainPtr;

	Ck_DeleteAllBindings(mainPtr->bindingTable,
		(ClientData) winPtr->pathName);
	Tcl_DeleteHashEntry(Tcl_FindHashEntry(&mainPtr->nameTable,
		winPtr->pathName));
	if (mainPtr->winPtr == winPtr) {
	    CkCmd *cmdPtr;

	    for (cmdPtr = commands; cmdPtr->name != NULL; cmdPtr++)
		if (cmdPtr->cmdProc != Ck_ExitCmd)
		    Tcl_CreateCommand(mainPtr->interp, cmdPtr->name,
			DeadAppCmd, (ClientData) NULL,
			(Tcl_CmdDeleteProc *) NULL);
	    Tcl_DeleteHashTable(&mainPtr->nameTable);
	    Ck_DeleteBindingTable(mainPtr->bindingTable);

#ifdef NCURSES_MOUSE_VERSION
	    mousemask(0, NULL);
	    mainPtr->flags &= (getmouse(&mEvent) != ERR) ? ~CK_HAS_MOUSE : ~0;
#endif	/* NCURSES_MOUSE_VERSION */

	    if (mainPtr->flags & CK_HAS_MOUSE) {
#ifdef __WIN32__
		mouse_set(0);
#endif
		if (mainPtr->flags & CK_MOUSE_XTERM) {
		    fflush(stdout);
		    fputs("\033[?1000l", stdout);
		    fflush(stdout);
		} else {
#ifdef HAVE_GPM
#if (TCL_MAJOR_VERSION >= 8)
		    Tcl_DeleteFileHandler((int) mainPtr->mouseData);
#else
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
		    Tk_DeleteFileHandler((int) mainPtr->mouseData);
#else
		    Tcl_DeleteFileHandler((Tcl_File) mainPtr->mouseData);
#endif
#endif
		    Gpm_Close();
#endif
		}
	    }

	    curs_set(1);
	    if (mainPtr->flags & CK_NOCLR_ON_EXIT) {
		wattrset(stdscr, A_NORMAL);
	    } else {
		wclear(stdscr);
		wrefresh(stdscr);
	    } 
	    endwin();
#if CK_USE_UTF
	    if (mainPtr->isoEncoding != NULL) {
		Tcl_FreeEncoding(mainPtr->isoEncoding);
	    }
#endif
	    ckfree((char *) mainPtr);
	    ckMainInfo = NULL;
	    goto done;
	}
    }
    if (winPtr->flags & CK_TOPLEVEL) {
	UnlinkToplevel(winPtr);
	ChangeToplevelFocus(winPtr->mainPtr->topLevPtr);
    } else if (winPtr->mainPtr->focusPtr == winPtr) {
	winPtr->mainPtr->focusPtr = winPtr->parentPtr;
	if (winPtr->mainPtr->focusPtr != NULL &&
            (winPtr->mainPtr->focusPtr->flags & CK_MAPPED)) {
	    event.type = CK_EV_FOCUSIN;
	    event.winPtr = winPtr->mainPtr->focusPtr;
	    Ck_HandleEvent(winPtr->mainPtr, (CkEvent *) &event);
	}
    } else {
	CkWindow *topPtr;

	for (topPtr = winPtr; topPtr != NULL && !(topPtr->flags & CK_TOPLEVEL);
	     topPtr = topPtr->parentPtr) {
	    /* Empty loop body. */
	}
	if (topPtr->focusPtr == winPtr)
	    topPtr->focusPtr = winPtr->parentPtr;
    }
    Ck_EventuallyRefresh(winPtr);
done:
    ckfree((char *) winPtr);
}

/*
 *--------------------------------------------------------------
 *
 * Ck_MapWindow --
 *
 *	Map a window within its parent.  This may require the
 *	window and/or its parents to actually be created.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The given window will be mapped.  Windows may also
 *	be created.
 *
 *--------------------------------------------------------------
 */

void
Ck_MapWindow(winPtr)
    CkWindow *winPtr;		/* Pointer to window to map. */
{
    if (winPtr == NULL || (winPtr->flags & CK_MAPPED))
	return;
    if (!(winPtr->parentPtr->flags & CK_MAPPED))
	return;
    if (winPtr->window == NULL)
	Ck_MakeWindowExist(winPtr);
}

/*
 *--------------------------------------------------------------
 *
 * Ck_MakeWindowExist --
 *
 *	Ensure that a particular window actually exists.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the procedure returns, the curses window associated
 *	with winPtr is guaranteed to exist.  This may require the
 *	window's ancestors to be created also.
 *
 *--------------------------------------------------------------
 */

void
Ck_MakeWindowExist(winPtr)
    CkWindow *winPtr;		/* Pointer to window. */
{
    int x, y;
    CkMainInfo *mainPtr;
    CkWindow *parentPtr;
    CkWindowEvent event;

    if (winPtr == NULL || winPtr->window != NULL)
	return;

    mainPtr = winPtr->mainPtr;
    if (winPtr->parentPtr->window == NULL)
	Ck_MakeWindowExist(winPtr->parentPtr);

    if (winPtr->x >= mainPtr->maxWidth)
	winPtr->x = mainPtr->maxWidth - 1;
    if (winPtr->x < 0)
	winPtr->x = 0;
    if (winPtr->y >= mainPtr->maxHeight)
	winPtr->y = mainPtr->maxHeight - 1;
    if (winPtr->y < 0)
	winPtr->y = 0;

    x = winPtr->x;
    y = winPtr->y;

    if (!(winPtr->flags & CK_TOPLEVEL)) {
	parentPtr = winPtr->parentPtr;
	if (x < 0)
	    x = winPtr->x = 0;
	else if (x >= parentPtr->width)
	    x = winPtr->x = parentPtr->width - 1;
	if (y < 0)
	    y = winPtr->y = 0;
	else if (y >= parentPtr->height)
	    y = winPtr->y = parentPtr->height - 1;
	if (x + winPtr->width >= parentPtr->width)
	    winPtr->width = parentPtr->width - x;
	if (y + winPtr->height >= parentPtr->height)
	    winPtr->height = parentPtr->height - y;
	parentPtr = winPtr;
	while ((parentPtr = parentPtr->parentPtr) != NULL) {
	    x += parentPtr->x;
	    y += parentPtr->y;
	    if (parentPtr->flags & CK_TOPLEVEL)
		break;
	}
    }
    if (winPtr->width <= 0)
	winPtr->width = 1;
    if (winPtr->height <= 0)
	winPtr->height = 1;

    winPtr->window = newwin(winPtr->height, winPtr->width, y, x);
    idlok(winPtr->window, TRUE);
    scrollok(winPtr->window, FALSE);
    keypad(winPtr->window, TRUE);
    nodelay(winPtr->window, TRUE);
    meta(winPtr->window, TRUE);
    winPtr->flags |= CK_MAPPED;
    Ck_ClearToBot(winPtr, 0, 0);
    Ck_SetWindowAttr(winPtr, winPtr->fg, winPtr->bg, winPtr->attr);
    Ck_EventuallyRefresh(winPtr);

    event.type = CK_EV_MAP;
    event.winPtr = winPtr;
    Ck_HandleEvent(mainPtr, (CkEvent *) &event);
    event.type = CK_EV_EXPOSE;
    event.winPtr = winPtr;
    Ck_HandleEvent(mainPtr, (CkEvent *) &event);
    if (winPtr == mainPtr->focusPtr) {
	event.type = CK_EV_FOCUSIN;
	event.winPtr = winPtr;
	Ck_HandleEvent(mainPtr, (CkEvent *) &event);
    }
}

/*
 *--------------------------------------------------------------
 *
 * Ck_MoveWindow --
 *
 *	Move given window and its children.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Ck_MoveWindow(winPtr, x, y)
    CkWindow *winPtr;		/* Window to move. */
    int x, y;			/* New location for window (within
				 * parent). */
{
    CkWindow *childPtr, *parentPtr;
    int newx, newy;

    if (winPtr == NULL)
	return;

    winPtr->x = x;
    winPtr->y = y;
    if (winPtr->window == NULL)
    	return;

    newx = x;
    newy = y;
    if (!(winPtr->flags & CK_TOPLEVEL)) {
	parentPtr = winPtr;
	while ((parentPtr = parentPtr->parentPtr) != NULL) {
	    newx += parentPtr->x;
	    newy += parentPtr->y;
	    if (parentPtr->flags & CK_TOPLEVEL)
		break;
	}
    }
    if (newx + winPtr->width >= winPtr->mainPtr->maxWidth) {
	winPtr->x -= newx - (winPtr->mainPtr->maxWidth - winPtr->width);
	newx = winPtr->mainPtr->maxWidth - winPtr->width;
    }
    if (newy + winPtr->height >= winPtr->mainPtr->maxHeight) {
	winPtr->y -= newy - (winPtr->mainPtr->maxHeight - winPtr->height);
	newy = winPtr->mainPtr->maxHeight - winPtr->height;
    }
    if (newx < 0) {
	winPtr->x -= newx;
	newx = 0;
    }
    if (newy < 0) {
	winPtr->y -= newy;
	newy = 0;
    }

    mvwin(winPtr->window, newy, newx);

    for (childPtr = winPtr->childList;
         childPtr != NULL; childPtr = childPtr->nextPtr)
	if (!(childPtr->flags & CK_TOPLEVEL))
	    Ck_MoveWindow(childPtr, childPtr->x, childPtr->y);
    Ck_EventuallyRefresh(winPtr);
}

/*
 *--------------------------------------------------------------
 *
 * Ck_ResizeWindow --
 *
 *	Resize given window and eventually its children.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Ck_ResizeWindow(winPtr, width, height)
    CkWindow *winPtr;		/* Window to resize. */
    int width, height;		/* New dimensions for window. */
{
    CkWindow *childPtr, *parentPtr;
    CkWindow *mainWin = winPtr->mainPtr->winPtr;
    CkWindowEvent event;
    WINDOW *new;
    int x, y, evMap = 0, doResize = 0;

    if (winPtr == NULL || winPtr == mainWin)
	return;

    /*
     * Special case: if both width/height set to -12345, adjust
     * within parent window !
     */

    parentPtr = winPtr->parentPtr;
    if (!(width == -12345 && height == -12345)) {
    	winPtr->width = width;
    	winPtr->height = height;
    	doResize++;
    }

    if (!(winPtr->flags & CK_TOPLEVEL)) {
	if (winPtr->x + winPtr->width >= parentPtr->width) {
	    winPtr->width = parentPtr->width - winPtr->x;
	    doResize++;
	}
	if (winPtr->y + winPtr->height >= parentPtr->height) {
	    winPtr->height = parentPtr->height - winPtr->y;
	    doResize++;
	}

	if (!doResize)
	    return;

	if (winPtr->window == NULL)
	    return;

	parentPtr = winPtr;
	x = winPtr->x;
	y = winPtr->y;
	while ((parentPtr = parentPtr->parentPtr) != NULL) {
	    x += parentPtr->x;
	    y += parentPtr->y;
	    if (parentPtr->flags & CK_TOPLEVEL)
		break;
	}
    } else {
	x = winPtr->x;
	y = winPtr->y;
    }

    if (winPtr->width <= 0)
	winPtr->width = 1;
    if (winPtr->height <= 0)
	winPtr->height = 1;

    if (x + winPtr->width > winPtr->mainPtr->maxWidth)
	winPtr->width = winPtr->mainPtr->maxWidth - x;
    if (y + winPtr->height > winPtr->mainPtr->maxHeight)
	winPtr->height = winPtr->mainPtr->maxHeight - y;

    new = newwin(winPtr->height, winPtr->width, y, x);
    if (winPtr->window == NULL) {
	winPtr->flags |= CK_MAPPED;
	evMap++;
    } else {
        delwin(winPtr->window);
    }
    winPtr->window = new;
    idlok(winPtr->window, TRUE);
    scrollok(winPtr->window, FALSE);
    keypad(winPtr->window, TRUE);
    nodelay(winPtr->window, TRUE);
    meta(winPtr->window, TRUE);
    Ck_SetWindowAttr(winPtr, winPtr->fg, winPtr->bg, winPtr->attr);
    Ck_ClearToBot(winPtr, 0, 0);

    for (childPtr = winPtr->childList;
         childPtr != NULL; childPtr = childPtr->nextPtr) {
	if (childPtr->flags & CK_TOPLEVEL)
	    continue;
	Ck_ResizeWindow(childPtr, -12345, -12345);
    }
    Ck_EventuallyRefresh(winPtr);

    event.type = CK_EV_MAP;
    event.winPtr = winPtr;
    Ck_HandleEvent(mainWin->mainPtr, (CkEvent *) &event);
    event.type = CK_EV_EXPOSE;
    event.winPtr = winPtr;
    Ck_HandleEvent(mainWin->mainPtr, (CkEvent *) &event);
}

/*
 *--------------------------------------------------------------
 *
 * Ck_UnmapWindow, etc. --
 *
 *	There are several procedures under here, each of which
 *	mirrors an existing X procedure.  In addition to performing
 *	the functions of the corresponding procedure, each
 *	procedure also updates the local window structure and
 *	synthesizes an X event (if the window's structure is being
 *	managed internally).
 *
 * Results:
 *	See the manual entries.
 *
 * Side effects:
 *	See the manual entries.
 *
 *--------------------------------------------------------------
 */

void
Ck_UnmapWindow(winPtr)
    CkWindow *winPtr;		/* Pointer to window to unmap. */
{
    CkWindow *childPtr;
    CkMainInfo *mainPtr = winPtr->mainPtr;
    CkWindowEvent event;

    for (childPtr = winPtr->childList;
         childPtr != NULL; childPtr = childPtr->nextPtr) {
	if (childPtr->flags & CK_TOPLEVEL)
	    continue;
	Ck_UnmapWindow(childPtr);
    }
    if (!(winPtr->flags & CK_MAPPED))
	return;
    winPtr->flags &= ~CK_MAPPED;
    delwin(winPtr->window);
    winPtr->window = NULL;
    Ck_EventuallyRefresh(winPtr);

    if (mainPtr->focusPtr == winPtr) {
	CkWindow *parentPtr;

	parentPtr = winPtr->parentPtr;
	while (parentPtr != NULL && !(parentPtr->flags & CK_TOPLEVEL))
	    parentPtr = parentPtr->parentPtr;
	mainPtr->focusPtr = parentPtr;
	event.type = CK_EV_FOCUSOUT;
	event.winPtr = winPtr;
	Ck_HandleEvent(mainPtr, (CkEvent *) &event);
    }
    event.type = CK_EV_UNMAP;
    event.winPtr = winPtr;
    Ck_HandleEvent(mainPtr, (CkEvent *) &event);
}

void
Ck_SetWindowAttr(winPtr, fg, bg, attr)
    CkWindow *winPtr;		/* Window to manipulate. */
    int fg, bg;			/* Foreground/background colors. */
    int attr;			/* Video attributes. */
{
    winPtr->fg = fg;
    winPtr->bg = bg;
    winPtr->attr = attr;
    if (winPtr->window != NULL) {
	if ((winPtr->mainPtr->flags & (CK_HAS_COLOR | CK_REVERSE_KLUDGE)) ==
	    (CK_HAS_COLOR | CK_REVERSE_KLUDGE)) {
	    if (attr & A_REVERSE) {
		int tmp;

		attr &= ~A_REVERSE;
		tmp = bg;
		bg = fg;
		fg = tmp;
	    }
	}
	wattrset(winPtr->window, attr | Ck_GetPair(winPtr, fg, bg));
    }
}

void
Ck_GetRootGeometry(winPtr, xPtr, yPtr, widthPtr, heightPtr)
    CkWindow *winPtr;
    int *xPtr, *yPtr, *widthPtr, *heightPtr;
{
    int x, y;

    if (widthPtr != NULL)
	*widthPtr = winPtr->width;
    if (heightPtr != NULL)
	*heightPtr = winPtr->height;

    x = y = 0;
    do {
	x += winPtr->x;
	y += winPtr->y;
	if (winPtr->flags & CK_TOPLEVEL)
	    break;
	winPtr = winPtr->parentPtr;
    } while (winPtr != NULL);
    if (xPtr != NULL)
	*xPtr = x;
    if (yPtr != NULL)
	*yPtr = y;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_NameToWindow --
 *
 *	Given a string name for a window, this procedure
 *	returns the pointer to the window, if there exists a
 *	window corresponding to the given name.
 *
 * Results:
 *	The return result is either the pointer to the window corresponding
 *	to "name", or else NULL to indicate that there is no such
 *	window.  In this case, an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

CkWindow *
Ck_NameToWindow(interp, pathName, winPtr)
    Tcl_Interp *interp;		/* Where to report errors. */
    char *pathName;		/* Path name of window. */
    CkWindow *winPtr;		/* Pointer to window:  name is assumed to
				 * belong to the same main window as winPtr. */
{
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&winPtr->mainPtr->nameTable, pathName);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "bad window path name \"",
		pathName, "\"", (char *) NULL);
	return NULL;
    }
    return (CkWindow *) Tcl_GetHashValue(hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_SetClass --
 *
 *	This procedure is used to give a window a class.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new class is stored for winPtr, replacing any existing
 *	class for it.
 *
 *----------------------------------------------------------------------
 */

void
Ck_SetClass(winPtr, className)
    CkWindow *winPtr;		/* Window to assign class. */
    char *className;		/* New class for window. */
{
    winPtr->classUid = Ck_GetUid(className);
    CkOptionClassChanged(winPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * UnlinkWindow --
 *
 *	This procedure removes a window from the childList of its
 *	parent.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is unlinked from its childList.
 *
 *----------------------------------------------------------------------
 */

static void
UnlinkWindow(winPtr)
    CkWindow *winPtr;			/* Child window to be unlinked. */
{
    CkWindow *prevPtr;

    if (winPtr->parentPtr == NULL)
	return;
    prevPtr = winPtr->parentPtr->childList;
    if (prevPtr == winPtr) {
	winPtr->parentPtr->childList = winPtr->nextPtr;
	if (winPtr->nextPtr == NULL)
	    winPtr->parentPtr->lastChildPtr = NULL;
    } else {
	while (prevPtr->nextPtr != winPtr) {
	    prevPtr = prevPtr->nextPtr;
	    if (prevPtr == NULL)
		panic("UnlinkWindow couldn't find child in parent");
	}
	prevPtr->nextPtr = winPtr->nextPtr;
	if (winPtr->nextPtr == NULL)
	    winPtr->parentPtr->lastChildPtr = prevPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * UnlinkToplevel --
 *
 *	This procedure removes a window from the toplevel list.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is unlinked from the toplevel list.
 *
 *----------------------------------------------------------------------
 */

static void
UnlinkToplevel(winPtr)
    CkWindow *winPtr;
{
    CkWindow *prevPtr;

    prevPtr = winPtr->mainPtr->topLevPtr;
    if (prevPtr == winPtr) {
	winPtr->mainPtr->topLevPtr = winPtr->topLevPtr;
    } else {
	while (prevPtr->topLevPtr != winPtr) {
	    prevPtr = prevPtr->topLevPtr;
	    if (prevPtr == NULL)
		panic("UnlinkToplevel couldn't find toplevel");
	}
	prevPtr->topLevPtr = winPtr->topLevPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_RestackWindow --
 *
 *	Change a window's position in the stacking order.
 *
 * Results:
 *	TCL_OK is normally returned.  If other is not a descendant
 *	of winPtr's parent then TCL_ERROR is returned and winPtr is
 *	not repositioned.
 *
 * Side effects:
 *	WinPtr is repositioned in the stacking order.
 *
 *----------------------------------------------------------------------
 */

int
Ck_RestackWindow(winPtr, aboveBelow, otherPtr)
    CkWindow *winPtr;		/* Pointer to window whose position in
				 * the stacking order is to change. */
    int aboveBelow;		/* Indicates new position of winPtr relative
				 * to other;  must be Above or Below. */
    CkWindow *otherPtr;		/* WinPtr will be moved to a position that
				 * puts it just above or below this window.
				 * If NULL then winPtr goes above or below
				 * all windows in the same parent. */
{
    CkWindow *prevPtr;

    if (winPtr->flags & CK_TOPLEVEL) {
	if (otherPtr != NULL) {
	    while (otherPtr != NULL && !(otherPtr->flags & CK_TOPLEVEL))
		otherPtr = otherPtr->parentPtr;
	}
	if (otherPtr == winPtr)
	    return TCL_OK;

	UnlinkToplevel(winPtr);
	if (aboveBelow == CK_ABOVE) {
	    if (otherPtr == NULL) {
		winPtr->topLevPtr = winPtr->mainPtr->topLevPtr;
		winPtr->mainPtr->topLevPtr = winPtr;
	    } else {
		CkWindow *thisPtr = winPtr->mainPtr->topLevPtr;

		prevPtr = NULL;
		while (thisPtr != NULL && thisPtr != otherPtr) {
		    prevPtr = thisPtr;
		    thisPtr = thisPtr->topLevPtr;
		}
		if (prevPtr == NULL) {
		    winPtr->topLevPtr = winPtr->mainPtr->topLevPtr;
		    winPtr->mainPtr->topLevPtr = winPtr;
		} else {
		    winPtr->topLevPtr = prevPtr->topLevPtr;
		    prevPtr->topLevPtr = winPtr;
		}
	    }
	} else {
	    CkWindow *thisPtr = winPtr->mainPtr->topLevPtr;

	    prevPtr = NULL;
	    while (thisPtr != NULL && thisPtr != otherPtr) {
		prevPtr = thisPtr;
		thisPtr = thisPtr->topLevPtr;
	    }
	    if (thisPtr == NULL) {
		winPtr->topLevPtr = prevPtr->topLevPtr;
		prevPtr->topLevPtr = winPtr;
	    } else {
		winPtr->topLevPtr = thisPtr->topLevPtr;
		thisPtr->topLevPtr = winPtr;
	    }
	}
	ChangeToplevelFocus(winPtr->mainPtr->topLevPtr);
	goto done;
    }

    /*
     * Find an ancestor of otherPtr that is a sibling of winPtr.
     */

    if (otherPtr == NULL) {
	if (aboveBelow == CK_BELOW)
	    otherPtr = winPtr->parentPtr->lastChildPtr;
	else
	    otherPtr = winPtr->parentPtr->childList;
    } else {
	while (winPtr->parentPtr != otherPtr->parentPtr) {
	    otherPtr = otherPtr->parentPtr;
	    if (otherPtr == NULL)
	    	return TCL_ERROR;
	}
    }
    if (otherPtr == winPtr)
	return TCL_OK;

    /*
     * Reposition winPtr in the stacking order.
     */

    UnlinkWindow(winPtr);
    if (aboveBelow == CK_BELOW) {
	winPtr->nextPtr = otherPtr->nextPtr;
	if (winPtr->nextPtr == NULL)
	    winPtr->parentPtr->lastChildPtr = winPtr;
	otherPtr->nextPtr = winPtr;
    } else {
	prevPtr = winPtr->parentPtr->childList;
	if (prevPtr == otherPtr)
	    winPtr->parentPtr->childList = winPtr;
	else {
	    while (prevPtr->nextPtr != otherPtr)
		prevPtr = prevPtr->nextPtr;
	    prevPtr->nextPtr = winPtr;
	}
	winPtr->nextPtr = otherPtr;
    }

done:
    Ck_EventuallyRefresh(winPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_SetFocus --
 *
 *	This procedure is invoked to change the focus window for a
 *	given display in a given application.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Event handlers may be invoked to process the change of
 *	focus.
 *
 *----------------------------------------------------------------------
 */

void
Ck_SetFocus(winPtr)
    CkWindow *winPtr;		/* Window that is to be the new focus. */
{
    CkMainInfo *mainPtr = winPtr->mainPtr;
    CkEvent event;
    CkWindow *oldTop = NULL, *newTop, *oldFocus;

    if (winPtr == mainPtr->focusPtr)
	return;

    oldFocus = mainPtr->focusPtr;
    if (oldFocus != NULL) {
	for (oldTop = oldFocus; oldTop != NULL &&
	     !(oldTop->flags & CK_TOPLEVEL); oldTop = oldTop->parentPtr) {
	    /* Empty loop body. */
	}
	event.win.type = CK_EV_FOCUSOUT;
	event.win.winPtr = oldFocus;
	Ck_HandleEvent(mainPtr, &event);
    }
    mainPtr->focusPtr = winPtr;
    for (newTop = winPtr; newTop != NULL &&
	 !(newTop->flags & CK_TOPLEVEL); newTop = newTop->parentPtr) {
	/* Empty loop body. */
    }
    if (oldTop != newTop) {
	if (oldTop != NULL)
	    oldTop->focusPtr = oldFocus;
    	Ck_RestackWindow(newTop, CK_ABOVE, NULL);
        Ck_EventuallyRefresh(mainPtr->winPtr);
    }
    if (winPtr->flags & CK_MAPPED) {
        event.win.type = CK_EV_FOCUSIN;
        event.win.winPtr = winPtr;
        Ck_HandleEvent(mainPtr, &event);
        Ck_EventuallyRefresh(mainPtr->winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ChangeToplevelFocus --
 *
 *----------------------------------------------------------------------
 */

static void
ChangeToplevelFocus(winPtr)
    CkWindow *winPtr;
{
    CkWindow *winTop, *oldTop;
    CkMainInfo *mainPtr;

    if (winPtr == NULL)
	return;

    mainPtr = winPtr->mainPtr;
    for (winTop = winPtr; winTop != NULL && !(winTop->flags & CK_TOPLEVEL);
	 winTop = winTop->parentPtr) {
	/* Empty loop body. */
    }
    for (oldTop = mainPtr->focusPtr; oldTop != NULL &&
         !(oldTop->flags & CK_TOPLEVEL); oldTop = oldTop->parentPtr) {
	/* Empty loop body. */
    }
    if (winTop != oldTop) {
	CkEvent event;

	if (oldTop != NULL) {
	    oldTop->focusPtr = mainPtr->focusPtr;
	    event.win.type = CK_EV_FOCUSOUT;
	    event.win.winPtr = mainPtr->focusPtr;
	    Ck_HandleEvent(mainPtr, &event);
	}
	mainPtr->focusPtr = winTop->focusPtr;
	event.win.type = CK_EV_FOCUSIN;
	event.win.winPtr = mainPtr->focusPtr;
	Ck_HandleEvent(mainPtr, &event);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_EventuallyRefresh --
 *
 *	Dispatch refresh of entire screen.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Ck_EventuallyRefresh(winPtr)
    CkWindow *winPtr;
{
    if (++winPtr->mainPtr->refreshCount == 1)
	Tk_DoWhenIdle(DoRefresh, (ClientData) winPtr->mainPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * DoRefresh --
 *
 *	Refresh all curses windows. If the terminal is connected via
 *	a network connection (ie terminal server) the curses typeahead
 *	mechanism is not sufficient for delaying screen updates due to
 *	TCP buffering.
 *	Therefore the refreshDelay may be used in order to limit updates
 *	to happen not more often than 1000/refreshDelay times per second.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
DoRefresh(clientData)
    ClientData clientData;
{
    CkMainInfo *mainPtr = (CkMainInfo *) clientData;

    if (mainPtr->flags & CK_REFRESH_TIMER) {
	Tk_DeleteTimerHandler(mainPtr->refreshTimer);
	mainPtr->flags &= ~CK_REFRESH_TIMER;
    }
    if (--mainPtr->refreshCount > 0) {
	Tk_DoWhenIdle2(DoRefresh, clientData);
	return;
    }
    mainPtr->refreshCount = 0;
    if (mainPtr->refreshDelay > 0) {
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION <= 4)
	struct timeval tv;
	double t0;

	gettimeofday(&tv, (struct timezone *) NULL);
	t0 = (tv.tv_sec + 0.000001 * tv.tv_usec) * 1000;
#else
	Tcl_Time tv;
	double t0;
	extern void TclpGetTime _ANSI_ARGS_((Tcl_Time *timePtr));

	TclpGetTime(&tv);
	t0 = (tv.sec + 0.000001 * tv.usec) * 1000;
#endif
	if (t0 - mainPtr->lastRefresh < mainPtr->refreshDelay) {
	    mainPtr->refreshTimer = Tk_CreateTimerHandler(
		mainPtr->refreshDelay - (int) (t0 - mainPtr->lastRefresh),
	        DoRefresh, clientData);
	    mainPtr->flags |= CK_REFRESH_TIMER;
	    return;
	}
	mainPtr->lastRefresh = t0;
    }
    curs_set(0);
    RefreshToplevels(mainPtr->topLevPtr);
    UpdateHWCursor(ckMainInfo);
    doupdate();
}

/*
 *----------------------------------------------------------------------
 *
 * RefreshToplevels --
 *
 *	Recursively refresh all toplevel windows starting at winPtr.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
RefreshToplevels(winPtr)
    CkWindow *winPtr;
{
    if (winPtr->topLevPtr != NULL)
	RefreshToplevels(winPtr->topLevPtr);
    if (winPtr->window != NULL) {
	touchwin(winPtr->window);
	wnoutrefresh(winPtr->window);
	if (winPtr->childList != NULL)
	    RefreshThem(winPtr->childList);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RefreshThem --
 *
 *	Recursively refresh all curses windows starting at winPtr.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
RefreshThem(winPtr)
    CkWindow *winPtr;
{
    if (winPtr->nextPtr != NULL)
        RefreshThem(winPtr->nextPtr);
    if (winPtr->flags & CK_TOPLEVEL)
	return;
    if (winPtr->window != NULL) {
	touchwin(winPtr->window);
	wnoutrefresh(winPtr->window);
    }
    if (winPtr->childList != NULL)
        RefreshThem(winPtr->childList);
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateHWCursor --
 *
 *	Make hardware cursor (in)visible for given window using curses.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateHWCursor(mainPtr)
    CkMainInfo *mainPtr;
{
    int x, y;
    CkWindow *wPtr, *stopAtWin, *winPtr = mainPtr->focusPtr;

    if (winPtr == NULL || winPtr->window == NULL ||
        (winPtr->flags & (CK_SHOW_CURSOR | CK_ALREADY_DEAD)) == 0) {
invisible:
	curs_set(0);
	if (mainPtr->focusPtr != NULL && mainPtr->focusPtr->window != NULL)
	    wnoutrefresh(mainPtr->focusPtr->window);
        return;
    }

    /*
     * Get position of HW cursor in winPtr coordinates.
     */

    getyx(winPtr->window, y, x);

    stopAtWin = NULL;
    while (winPtr != NULL) {
        for (wPtr = winPtr->childList;
             wPtr != NULL && wPtr != stopAtWin; wPtr = wPtr->nextPtr) {
	    if ((wPtr->flags & CK_TOPLEVEL) || wPtr->window == NULL)
	        continue;
  	    if (x >= wPtr->x && x < wPtr->x + wPtr->width &&
	        y >= wPtr->y && y < wPtr->y + wPtr->height)
	        goto invisible;
	}
	x += winPtr->x;
	y += winPtr->y;
        stopAtWin = winPtr;
	if (winPtr->parentPtr == NULL)
	    break;
	winPtr = winPtr->parentPtr;
	if (winPtr->flags & CK_TOPLEVEL)
	    break;
    }
    for (wPtr = mainPtr->topLevPtr; wPtr != NULL && wPtr != winPtr;
	 wPtr = wPtr->topLevPtr)
	if (x >= wPtr->x && x < wPtr->x + wPtr->width &&
	    y >= wPtr->y && y < wPtr->y + wPtr->height)
	    goto invisible;
    curs_set(1);
    wnoutrefresh(mainPtr->focusPtr->window);
}

/*
 *----------------------------------------------------------------------
 *
 * GetWindowXY --
 *
 *	Given X, Y coordinates, return topmost window.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static CkWindow *
GetWindowXY(winPtr, xPtr, yPtr)
    CkWindow *winPtr;
    int *xPtr, *yPtr;
{
    int x, y;
    CkWindow *wPtr;

    x = *xPtr - winPtr->x; y = *yPtr - winPtr->y;
    for (wPtr = winPtr->childList; wPtr != NULL; wPtr = wPtr->nextPtr) {
	if (!(wPtr->flags & CK_MAPPED) || (wPtr->flags & CK_TOPLEVEL))
	    continue;
	if (x >= wPtr->x && x < wPtr->x + wPtr->width &&
	    y >= wPtr->y && y < wPtr->y + wPtr->height) {
	    wPtr = GetWindowXY(wPtr, &x, &y);
	    *xPtr = x;
	    *yPtr = y;
	    return wPtr;
	}
    }
    *xPtr = x;
    *yPtr = y;
    return winPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_GetWindowXY --
 *
 *	Given X, Y coordinates, return topmost window.
 *	If mode is zero, consider all toplevels, otherwise consider
 *	only topmost toplevel in search.
 *
 * Results:
 *	Window pointer or NULL. *xPtr, *yPtr are adjusted to window
 *	coordinate system if possible.
 *
 *----------------------------------------------------------------------
 */

CkWindow *
Ck_GetWindowXY(mainPtr, xPtr, yPtr, mode)
    CkMainInfo *mainPtr;
    int *xPtr, *yPtr, mode;
{
    int x, y, x0, y0;
    CkWindow *wPtr;

    x0 = *xPtr; y0 = *yPtr;
    wPtr = mainPtr->topLevPtr;
nextToplevel:
    x = x0; y = y0;
    if (wPtr->flags & CK_MAPPED) {
	if (x >= wPtr->x && x < wPtr->x + wPtr->width &&
	    y >= wPtr->y && y < wPtr->y + wPtr->height) {
	    wPtr = GetWindowXY(wPtr, &x, &y);
	    *xPtr = x;
	    *yPtr = y;
	    return wPtr;
	} else {
	    *xPtr = -1;
	    *yPtr = -1;
	}
    } else if (mode) {
	return NULL;
    }
    if (mode == 0) {
	wPtr = wPtr->topLevPtr;
	if (wPtr != NULL)
	    goto nextToplevel;
    }
    return wPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_SetHWCursor --
 *
 *	Make hardware cursor (in)visible for given window.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Ck_SetHWCursor(winPtr, newState)
    CkWindow *winPtr;
    int newState;
{
    int oldState = winPtr->flags & CK_SHOW_CURSOR;

    if (newState == oldState)
	return;

    if (newState)
	winPtr->flags |= CK_SHOW_CURSOR;
    else
	winPtr->flags &= ~CK_SHOW_CURSOR;
    if (winPtr == winPtr->mainPtr->focusPtr)
	UpdateHWCursor(winPtr->mainPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_ClearToEol --
 *
 *	Clear window starting from given position, til end of line.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Ck_ClearToEol(winPtr, x, y)
    CkWindow *winPtr;
    int x, y;
{
    WINDOW *window = winPtr->window;

    if (window == NULL)
	return;

    if (x == -1 && y == -1)
	getyx(window, y, x);
    else
	wmove(window, y, x);
    for (; x < winPtr->width; x++)
	waddch(window, ' ');
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_ClearToBot --
 *
 *	Clear window starting from given position, til end of screen.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Ck_ClearToBot(winPtr, x, y)
    CkWindow *winPtr;
    int x, y;
{
    WINDOW *window = winPtr->window;

    if (window == NULL)
	return;

    wmove(window, y, x);
    for (; x < winPtr->width; x++)
	waddch(window, ' ');
    for (++y; y < winPtr->height; y++) {
	wmove(window, y, 0);
	for (x = 0; x < winPtr->width; x++)
	    waddch(window, ' ');
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DeadAppCmd --
 *
 *	Report error since toolkit gone.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
DeadAppCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    interp->result = "toolkit uninstalled";
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * ExecCmd --
 *
 *	Own version of "exec" Tcl command which supports the -endwin
 *      option.
 *
 * Results:
 *	See documentation for "exec".
 *
 *----------------------------------------------------------------------
 */

static int
ExecCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    RedirInfo *redirInfo = (RedirInfo *) clientData;
    Tcl_CmdInfo *cmdInfo = &redirInfo->cmdInfo;
    int result, endWin = 0, length;
    char *savedargv1 = NULL;
    char *clrCmd = NULL;
#ifdef SIGINT
#ifdef HAVE_SIGACTION
    struct sigaction oldsig, newsig;
#else
    Ck_SignalProc sigproc;
#endif
#endif

    length = strlen(argv[1]);
    if (argc > 1 && length >= 7 && strncmp(argv[1], "-endwin", 7) == 0) {
        endWin = 1;
	if (length >= 8 && strncmp(argv[1], "-endwinc", 8) == 0) {
	    clrCmd = tigetstr("clear");
	}
        savedargv1 = argv[1];
        argv[1] = argv[0];
    	curs_set(1);
	nodelay(stdscr, FALSE);
        endwin();
#ifdef SIGINT
#ifdef HAVE_SIGACTION
	newsig.sa_handler = SIG_IGN;
	sigfillset(&newsig.sa_mask);
	newsig.sa_flags = 0;
	sigaction(SIGINT, &newsig, &oldsig);
#else
	sigproc = signal(SIGINT, SIG_IGN);
#endif
#endif
#ifndef __WIN32__
	if (clrCmd != NULL && clrCmd != (char *) -1) {
	    write(1, clrCmd, strlen(clrCmd));
	}
#endif
    }
    result = (*cmdInfo->proc)(cmdInfo->clientData, interp,
        argc - endWin, argv + endWin);
    if (endWin) {
#ifdef SIGINT
#ifdef HAVE_SIGACTION
	sigaction(SIGINT, &oldsig, NULL);
#else
	signal(SIGINT, sigproc);
#endif
#endif
        argv[0] = argv[1];
        argv[1] = savedargv1;
	nodelay(stdscr, TRUE);
        Ck_EventuallyRefresh(redirInfo->mainPtr->winPtr);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * PutsCmd --
 *
 *	Redirect "puts" Tcl command from "stdout" to "stderr" in the
 *      hope that it will not destroy our screen.
 *
 * Results:
 *	See documentation of "puts" command.
 *
 *----------------------------------------------------------------------
 */

static int
PutsCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    RedirInfo *redirInfo = (RedirInfo *) clientData;
    Tcl_CmdInfo *cmdInfo = &redirInfo->cmdInfo;
    int index = 0;
    char *newArgv[5];

    newArgv[0] = argv[0];
    if (argc > 1 && strcmp(argv[1], "-nonewline") == 0) {
        newArgv[1] = argv[1];
        index++;
    }
    if (argc == index + 2) {
        newArgv[index + 2] = argv[index + 1];
toStderr:
        newArgv[index + 1] = "stderr";
        return (*cmdInfo->proc)(cmdInfo->clientData, interp,
	    index + 3, newArgv);
    } else if (argc == index + 3 &&
       (strcmp(argv[index + 1], "stdout") == 0 ||
        strcmp(argv[index + 1], "file1") == 0)) {
        newArgv[index + 2] = argv[index + 2];
        goto toStderr;
    }
    return (*cmdInfo->proc)(cmdInfo->clientData, interp, argc, argv);
}

/*
 *----------------------------------------------------------------------
 *
 * CloseCmd --
 *
 *	Report error when attempt is made to close stdin or stdout.
 *
 * Results:
 *	See documentation of "close" command.
 *
 *----------------------------------------------------------------------
 */

static int
CloseCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    RedirInfo *redirInfo = (RedirInfo *) clientData;
    Tcl_CmdInfo *cmdInfo = &redirInfo->cmdInfo;

    if (argc == 2 &&
       (strcmp(argv[1], "stdin") == 0 ||
        strcmp(argv[1], "file0") == 0 ||
        strcmp(argv[1], "stdout") == 0 ||
        strcmp(argv[1], "file1") == 0)) {
        Tcl_AppendResult(interp, "may not close fileId \"",
	     argv[1], "\" while in toolkit", (char *) NULL);
	return TCL_ERROR;
    }
    return (*cmdInfo->proc)(cmdInfo->clientData, interp, argc, argv);
}

/*
 *----------------------------------------------------------------------
 *
 * FlushCmd --
 *
 *	Report error when attempt is made to flush stdin or stdout.
 *
 * Results:
 *	See documentation of "flush" command.
 *
 *----------------------------------------------------------------------
 */

static int
FlushCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    RedirInfo *redirInfo = (RedirInfo *) clientData;
    Tcl_CmdInfo *cmdInfo = &redirInfo->cmdInfo;

    if (argc == 2 &&
       (strcmp(argv[1], "stdin") == 0 ||
        strcmp(argv[1], "file0") == 0 ||
        strcmp(argv[1], "stdout") == 0 ||
        strcmp(argv[1], "file1") == 0)) {
        Tcl_AppendResult(interp, "may not flush fileId \"",
	     argv[1], "\" while in toolkit", (char *) NULL);
	return TCL_ERROR;
    }
    return (*cmdInfo->proc)(cmdInfo->clientData, interp, argc, argv);
}

/*
 *----------------------------------------------------------------------
 *
 * ReadCmd --
 *
 *	Report error when attempt is made to read from stdin.
 *
 * Results:
 *	See documentation of "read" command.
 *
 *----------------------------------------------------------------------
 */

static int
ReadCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    RedirInfo *redirInfo = (RedirInfo *) clientData;
    Tcl_CmdInfo *cmdInfo = &redirInfo->cmdInfo;

    if ((argc > 1 &&
         (strcmp(argv[1], "stdin") == 0 ||
          strcmp(argv[1], "file0") == 0)) ||
        (argc > 2 &&
         (strcmp(argv[2], "stdin") == 0 ||
          strcmp(argv[2], "file0") == 0))) {
        Tcl_AppendResult(interp, "may not read from fileId \"",
	     argv[1], "\" while in toolkit", (char *) NULL);
	return TCL_ERROR;
    }
    return (*cmdInfo->proc)(cmdInfo->clientData, interp, argc, argv);
}

/*
 *----------------------------------------------------------------------
 *
 * GetsCmd --
 *
 *	Report error when attempt is made to read from stdin.
 *
 * Results:
 *	See documentation of "gets" command.
 *
 *----------------------------------------------------------------------
 */

static int
GetsCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    RedirInfo *redirInfo = (RedirInfo *) clientData;
    Tcl_CmdInfo *cmdInfo = &redirInfo->cmdInfo;

    if (argc >= 2 &&
       (strcmp(argv[1], "stdin") == 0 ||
        strcmp(argv[1], "file0") == 0)) {
        Tcl_AppendResult(interp, "may not gets from fileId \"",
	     argv[1], "\" while in toolkit", (char *) NULL);
	return TCL_ERROR;
    }
    return (*cmdInfo->proc)(cmdInfo->clientData, interp, argc, argv);
}

#ifdef __WIN32__

/*
 *----------------------------------------------------------------------
 *
 * WIN32 specific curses input event handling.
 *
 *----------------------------------------------------------------------
 */

static void
InputSetup(inputInfo)
    InputInfo *inputInfo;
{
    WNDCLASS class;
    DWORD id;

    /*
     * Create the async notification window with a new class.
     */
    class.style = 0;
    class.cbClsExtra = 0;
    class.cbWndExtra = 0;
    class.hInstance = GetModuleHandle(NULL);
    class.hbrBackground = NULL;
    class.lpszMenuName = NULL;
    class.lpszClassName = "CursesInput";
    class.lpfnWndProc = InputHandler;
    class.hIcon = NULL;
    class.hCursor = NULL;
    if (RegisterClass(&class)) {
	inputInfo->hwnd =
	    CreateWindow("CursesInput", "CursesInput", WS_TILED, 0, 0,
			 0, 0, NULL, NULL, class.hInstance, NULL);
    }
    if (inputInfo->hwnd == NULL) {
        panic("cannot create curses input window");
    }
    SetWindowLong(inputInfo->hwnd, GWL_USERDATA, (LONG) inputInfo);
    inputInfo->thread = CreateThread(NULL, 4096,
				     (LPTHREAD_START_ROUTINE) InputThread,
				     (void *) inputInfo, 0, &id);
    Tcl_CreateExitHandler(InputExit, (ClientData) inputInfo);
}

static void
InputExit(clientData)
    ClientData clientData;
{
    InputInfo *inputInfo = (InputInfo *) clientData;

    if (inputInfo->hwnd != NULL) {
        HWND hwnd = inputInfo->hwnd;

	inputInfo->hwnd = NULL;
        DestroyWindow(hwnd);
    }
    if (inputInfo->thread != INVALID_HANDLE_VALUE) {
	WaitForSingleObject(inputInfo->thread, 1000);
    }
}

static void
InputThread(arg)
    void *arg;
{
    InputInfo *inputInfo = (InputInfo *) arg;
    INPUT_RECORD ip;
    DWORD nRead;

    while (inputInfo->hwnd != NULL) {
	nRead = 0;
	PeekConsoleInput(inputInfo->stdinHandle, &ip, 1, &nRead);
	if (nRead > 0) {
	    PostMessage(inputInfo->hwnd, WM_USER + 42, 0, 0);
	}
	Sleep(10);
    }
    inputInfo->thread = INVALID_HANDLE_VALUE;
    ExitThread(0);
}

static LRESULT CALLBACK
InputHandler(hwnd, message, wParam, lParam)
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
{
    InputInfo *inputInfo = (InputInfo *) GetWindowLong(hwnd, GWL_USERDATA);

    if (message != WM_USER + 42) {
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    Tk_DoWhenIdle(InputHandler2, (ClientData) inputInfo);
    return 0;
}

static void
InputHandler2(clientData)
    ClientData clientData;
{
    InputInfo *inputInfo = (InputInfo *) clientData;
    INPUT_RECORD ip;
    DWORD nRead;

    do {
	CkHandleInput((ClientData) inputInfo->mainPtr, TCL_READABLE);
	nRead = 0;
	PeekConsoleInput(inputInfo->stdinHandle, &ip, 1, &nRead);
    } while (nRead != 0);
}

#endif

#if (TCL_MAJOR_VERSION >= 8)
/*
 *----------------------------------------------------------------------
 *
 * Event source functions for TCL_WINDOW_EVENTs.
 *
 *----------------------------------------------------------------------
 */

static void
CkEvtExit(clientData)
    ClientData clientData;
{
    Tcl_DeleteEventSource(CkEvtSetup, CkEvtCheck, clientData);
}

static void
CkEvtSetup(clientData, flags)
    ClientData clientData;
    int flags;
{
    if (!(flags & TCL_WINDOW_EVENTS)) {
	return;
    }
}

static void
CkEvtCheck(clientData, flags)
    ClientData clientData;
    int flags;
{
    if (!(flags & TCL_WINDOW_EVENTS)) {
	return;
    }
}
#endif
