/* 
 * ckUtil.c --
 *
 *	Miscellaneous utility functions.
 *
 * Copyright (c) 1995 Christian Werner.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "ckPort.h"
#include "ck.h"

#if CK_USE_UTF
#include <wchar.h>
#endif

#define REPLACE 1
#define NORMAL  2
#define TAB     3
#define NEWLINE 4
#define GCHAR   5

struct charType {
    char type;          /* Type of char, see definitions above. */
    char width;         /* Width if replaced by backslash sequence. */
};

struct charEncoding {
    char *name;         /* Name for this encoding table. */
    struct charType ct[256];    /* Encoding table. */
};

/*
 * For ISO8859, codes 0x81..0x99 are mapped to ACS characters
 * according to this table:
 */

static char *gcharTab[] = {
    "ulcorner", "llcorner", "urcorner", "lrcorner",
    "ltee", "rtee", "btee", "ttee",
    "hline", "vline", "plus", "s1",
    "s9", "diamond", "ckboard", "degree",
    "plminus", "bullet", "larrow", "rarrow",
    "darrow", "uarrow", "board", "lantern",
    "block"
};

static struct charEncoding EncodingTable[] = {

/*
 *----------------------------------------------------------------------
 *
 * ISO 8859 encoding.
 *
 *----------------------------------------------------------------------
 */

{
    "ISO8859", {

	{ REPLACE, 4 },	/* \x00 */
	{ REPLACE, 4 },	/* \x01 */
	{ REPLACE, 4 },	/* \x02 */
	{ REPLACE, 4 },	/* \x03 */
	{ REPLACE, 4 },	/* \x04 */
	{ REPLACE, 4 },	/* \x05 */
	{ REPLACE, 4 },	/* \x06 */
	{ REPLACE, 4 },	/* \x07 */
	{ REPLACE, 2 },	/* \b */
	{ TAB, 2 },	/* \t */
	{ NEWLINE, 2 },	/* \n */
	{ REPLACE, 4 },	/* \x0b */
	{ REPLACE, 2 },	/* \f */
	{ REPLACE, 2 },	/* \r */
	{ REPLACE, 4 },	/* 0x0e */
	{ REPLACE, 4 },	/* 0x0f */

	/* 0x10 .. 0x1f */
	{ REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 },
	{ REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 },
	{ REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 },
	{ REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 },

	/* ' ' .. '/' */
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* '0' .. '?' */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* '@' .. 'O' */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* 'P' .. '_' */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* '`' .. 'o' */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* 'p' .. '~' */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
    
	{ REPLACE, 4 },	/* 0x7f */

	/* 0x80 .. 0x8f */
	{ REPLACE, 4 }, { GCHAR, 1 }, { GCHAR, 1 }, { GCHAR, 1 },
	{ GCHAR, 1 }, { GCHAR, 1 }, { GCHAR, 1 }, { GCHAR, 1 },
	{ GCHAR, 1 }, { GCHAR, 1 }, { GCHAR, 1 }, { GCHAR, 1 },
	{ GCHAR, 1 }, { GCHAR, 1 }, { GCHAR, 1 }, { GCHAR, 1 },

	/* 0x90 .. 0x9f */
	{ GCHAR, 1 }, { GCHAR, 1 }, { GCHAR, 1 }, { GCHAR, 1 },
	{ GCHAR, 1 }, { GCHAR, 1 }, { GCHAR, 1 }, { GCHAR, 1 },
	{ GCHAR, 1 }, { GCHAR, 1 }, { REPLACE, 4 }, { REPLACE, 4 },
	{ REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 },

	/* 0xa0 .. 0xaf */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* 0xb0 .. 0xbf */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* 0xc0 .. 0xcf */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* 0xd0 .. 0xdf */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* 0xe0 .. 0xef */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* 0xf0 .. 0xff */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

      }
},

/*
 *----------------------------------------------------------------------
 *
 * IBM code page 437 encoding.
 *
 *----------------------------------------------------------------------
 */

{
    "IBM437", {

	{ REPLACE, 4 },	/* \x00 */
	{ REPLACE, 4 },	/* \x01 */
	{ REPLACE, 4 },	/* \x02 */
	{ REPLACE, 4 },	/* \x03 */
	{ REPLACE, 4 },	/* \x04 */
	{ REPLACE, 4 },	/* \x05 */
	{ REPLACE, 4 },	/* \x06 */
	{ REPLACE, 4 },	/* \x07 */
	{ REPLACE, 2 },	/* \b */
	{ TAB, 2 },	/* \t */
	{ NEWLINE, 2 },	/* \n */
	{ REPLACE, 4 },	/* \x0b */
	{ REPLACE, 2 },	/* \f */
	{ REPLACE, 2 },	/* \r */
	{ REPLACE, 4 },	/* 0x0e */
	{ REPLACE, 4 },	/* 0x0f */

	/* 0x10 .. 0x1f */
	{ REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 },
	{ REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 },
	{ REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 },
	{ REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 }, { REPLACE, 4 },

	/* ' ' .. '/' */
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* '0' .. '?' */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* '@' .. 'O' */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* 'P' .. '_' */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* '`' .. 'o' */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* 'p' .. '~' */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
    
	{ REPLACE, 4 },	/* 0x7f */

	/* 0x80 .. 0x8f */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* 0x90 .. 0x9a */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	{ NORMAL, 1 },	/* 0x9b */

	/* 0x9c .. 0x9f */
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* 0xa0 .. 0xaf */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* 0xb0 .. 0xbf */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* 0xc0 .. 0xcf */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* 0xd0 .. 0xdf */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* 0xe0 .. 0xef */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },

	/* 0xf0 .. 0xff */
 	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 },
	{ NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }, { NORMAL, 1 }

      }
}

};

/*
 * This is the switch for char encoding.
 */

static int Encoding = 0;

#define CHARTYPE(x)    EncodingTable[Encoding].ct[(x)]

/*
 * Characters used when displaying control sequences.
 */

static char hexChars[] = "0123456789abcdefxtnvr\\";

/*
 * The following table maps some control characters to sequences
 * like '\n' rather than '\x10'.  A zero entry in the table means
 * no such mapping exists, and the table only maps characters
 * less than 0x10.
 */

static char mapChars[] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    'b', 't', 'n', 0, 'f', 'r', 0
};


/*
 *----------------------------------------------------------------------
 *
 * CkCopyAndGlobalEval --
 *
 *	This procedure makes a copy of a script then calls Tcl_GlobalEval
 *	to evaluate it.  It's used in situations where the execution of
 *	a command may cause the original command string to be reallocated.
 *
 * Results:
 *	Returns the result of evaluating script, including both a standard
 *	Tcl completion code and a string in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
CkCopyAndGlobalEval(interp, script)
    Tcl_Interp *interp;			/* Interpreter in which to evaluate
					 * script. */
    char *script;			/* Script to evaluate. */
{
    Tcl_DString buffer;
    int code;

    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, script, -1);
    code = Tcl_GlobalEval(interp, Tcl_DStringValue(&buffer));
    Tcl_DStringFree(&buffer);
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_GetScrollInfo --
 *
 *	This procedure is invoked to parse "xview" and "yview"
 *	scrolling commands for widgets using the new scrolling
 *	command syntax ("moveto" or "scroll" options).
 *
 * Results:
 *	The return value is either CK_SCROLL_MOVETO, CK_SCROLL_PAGES,
 *	CK_SCROLL_UNITS, or CK_SCROLL_ERROR.  This indicates whether
 *	the command was successfully parsed and what form the command
 *	took.  If CK_SCROLL_MOVETO, *dblPtr is filled in with the
 *	desired position;  if CK_SCROLL_PAGES or CK_SCROLL_UNITS,
 *	*intPtr is filled in with the number of lines to move (may be
 *	negative);  if CK_SCROLL_ERROR, interp->result contains an
 *	error message.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Ck_GetScrollInfo(interp, argc, argv, dblPtr, intPtr)
    Tcl_Interp *interp;			/* Used for error reporting. */
    int argc;				/* # arguments for command. */
    char **argv;			/* Arguments for command. */
    double *dblPtr;			/* Filled in with argument "moveto"
					 * option, if any. */
    int *intPtr;			/* Filled in with number of pages
					 * or lines to scroll, if any. */
{
    int c;
    size_t length;

    length = strlen(argv[2]);
    c = argv[2][0];
    if ((c == 'm') && (strncmp(argv[2], "moveto", length) == 0)) {
	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " ", argv[1], " moveto fraction\"",
		    (char *) NULL);
	    return CK_SCROLL_ERROR;
	}
	if (Tcl_GetDouble(interp, argv[3], dblPtr) != TCL_OK) {
	    return CK_SCROLL_ERROR;
	}
	return CK_SCROLL_MOVETO;
    } else if ((c == 's')
	    && (strncmp(argv[2], "scroll", length) == 0)) {
	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " ", argv[1], " scroll number units|pages\"",
		    (char *) NULL);
	    return CK_SCROLL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[3], intPtr) != TCL_OK) {
	    return CK_SCROLL_ERROR;
	}
	length = strlen(argv[4]);
	c = argv[4][0];
	if ((c == 'p') && (strncmp(argv[4], "pages", length) == 0)) {
	    return CK_SCROLL_PAGES;
	} else if ((c == 'u')
		&& (strncmp(argv[4], "units", length) == 0)) {
	    return CK_SCROLL_UNITS;
	} else {
	    Tcl_AppendResult(interp, "bad argument \"", argv[4],
		    "\": must be units or pages", (char *) NULL);
	    return CK_SCROLL_ERROR;
	}
    }
    Tcl_AppendResult(interp, "unknown option \"", argv[2],
	    "\": must be moveto or scroll", (char *) NULL);
    return CK_SCROLL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * Ck_SetEncoding --
 *
 *--------------------------------------------------------------
 */

int
Ck_SetEncoding(interp, name)
    Tcl_Interp *interp;
    char *name;
{
    int i;

    for (i = 0; i < sizeof (EncodingTable) / sizeof (EncodingTable[0]); i++)
	if (strcmp(name, EncodingTable[i].name) == 0) {
	    Encoding = i;
	    return TCL_OK;
	}
    Tcl_AppendResult(interp, "no encoding \"", name, "\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * Ck_GetEncoding --
 *
 *--------------------------------------------------------------
 */

int
Ck_GetEncoding(interp)
    Tcl_Interp *interp;
{
    interp->result = EncodingTable[Encoding].name;
    return TCL_OK;
}

#if CK_USE_UTF
/*
 *--------------------------------------------------------------
 *
 * MakeUCRepl --
 *
 *	Make replacement for unprintable chars.
 *
 *--------------------------------------------------------------
 */

static int
MakeUCRepl(uch, buf)
    Tcl_UniChar uch;
    char *buf;
{
    unsigned int i, need;

    if ((unsigned int) uch < sizeof (mapChars) &&
	mapChars[(unsigned int) uch]) {
	if (buf) {
	    *buf++ = '\\';
	    *buf++ = mapChars[(unsigned int) uch];
	    *buf++ = '\0';
	}
	return 2;
    }
    for (i = 0x100, need = 2; i; i++, need++) {
	if ((unsigned int) uch < i) {
	    break;
	}
	i = i << 4;
    }
    if (buf) {
	char *p;

	*buf++ = '\\';
	*buf++ = (need < 3) ? 'x' : 'u';
	p = buf + need;
	*p = '\0';
	for (i = 0; i < need; i++) {
	    --p;
	    *p = hexChars[uch & 0xf];
	    uch = uch >> 4;
	}
    }
    return need + 2;
}
#endif

/*
 *--------------------------------------------------------------
 *
 * CkMeasureChars --
 *
 *	Measure the number of characters from a string that
 *	will fit in a given horizontal span.  The measurement
 *	is done under the assumption that CkDisplayChars will
 *	be used to actually display the characters.
 *
 * Results:
 *	The return value is the number of characters from source
 *	that fit in the span given by startX and maxX.  *nextXPtr
 *	is filled in with the x-coordinate at which the first
 *	character that didn't fit would be drawn, if it were to
 *	be drawn.
 *	
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
CkMeasureChars(mainPtr, source, maxChars, startX, maxX,
	tabOrigin, flags, nextXPtr, nextCPtr)
    CkMainInfo *mainPtr;	/* Needed for encoding. */
    char *source;		/* Characters to be displayed.  Need not
				 * be NULL-terminated. */
    int maxChars;		/* Maximum # of characters to consider from
				 * source. */
    int startX;			/* X-position at which first character will
				 * be drawn. */
    int maxX;			/* Don't consider any character that would
				 * cross this x-position. */
    int tabOrigin;		/* X-location that serves as "origin" for
				 * tab stops. */
    int flags;			/* Various flag bits OR-ed together.
				 * CK_WHOLE_WORDS means stop on a word boundary
				 * (just before a space character) if
				 * possible.  CK_AT_LEAST_ONE means always
				 * return a value of at least one, even
				 * if the character doesn't fit. 
				 * CK_PARTIAL_OK means it's OK to display only
				 * a part of the last character in the line.
				 * CK_NEWLINES_NOT_SPECIAL means that newlines
				 * are treated just like other control chars:
				 * they don't terminate the line.
				 * CK_IGNORE_TABS means give all tabs zero
				 * width. */
    int *nextXPtr;		/* Return x-position of terminating
				 * character here. */
    int *nextCPtr;		/* Return byte position of terminating
				   character in source. */
{
    register char *p;		/* Current character. */
    register int c;
    char *term;			/* Pointer to most recent character that
				 * may legally be a terminating character. */
    int termX;			/* X-position just after term. */
    int curX;			/* X-position corresponding to p. */
    int newX;			/* X-position corresponding to p+1. */
    int rem;
    int nChars = 0;
#if CK_USE_UTF
    int n, m, srcRead, dstWrote, dstChars;
    Tcl_UniChar uch;
    char buf[TCL_UTF_MAX], buf2[TCL_UTF_MAX];

    /*
     * Scan the input string one character at a time, until a character
     * is found that crosses maxX.
     */

    newX = curX = startX;
    termX = 0;
    term = source;
    for (p = source; *p != '\0' && maxChars > 0;) {
        char *p2;

	n = Tcl_UtfToUniChar(p, &uch);
	p2 = p + n;
	++nChars;
	maxChars -= n;
	m = Tcl_UniCharToUtf(uch, buf);
	if (mainPtr->isoEncoding) {
	    buf2[0] = '\0';
	    Tcl_UtfToExternal(NULL, mainPtr->isoEncoding, buf, m,
			  TCL_ENCODING_START | TCL_ENCODING_END,
			  NULL, buf2, sizeof (buf2), &srcRead,
			  &dstWrote, &dstChars);
	} else {
	    buf2[0] = '?';
	    if (uch > 0 && uch < 0x100) {
		buf2[0] = uch;
	    }
	}
	c = buf2[0] & 0xFF;
	if (CHARTYPE(c).type == TAB) {
	    if (!(flags & CK_IGNORE_TABS)) {
		newX += 8;
		rem = (newX - tabOrigin) % 8;
		if (rem < 0) {
		    rem += 8;
		}
		newX -= rem;
	    }
	} else if (CHARTYPE(c).type == GCHAR) {
	    newX += CHARTYPE(c).width;
	} else if (CHARTYPE(c).type == NEWLINE) {
	    if (flags & CK_NEWLINES_NOT_SPECIAL) {
		newX += CHARTYPE(c).width;
	    } else {
		break;
	    }
	} else if (mainPtr->isoEncoding) {
	    if (c == 0 || (c == '?' && uch != '?')) {
		newX += MakeUCRepl(uch, 0);
	    } else {
		if (CHARTYPE(c).type == REPLACE) {
		    newX += CHARTYPE(c).width;
		} else {
		    newX++;
		}
	    }
	} else {
	    int len = wcwidth((wint_t) uch);

	    if (len < 0) {
		newX += MakeUCRepl(uch, 0);
	    } else {
		newX += len;
	    }
	}
	if (newX > maxX) {
	    break;
	}
	p = p2;
	if (maxChars > 1) {
	    n = Tcl_UtfToUniChar(p, &uch);
	    p2 = p + n;
	    m = Tcl_UniCharToUtf(uch, buf);
	    if (mainPtr->isoEncoding) {
		buf2[0] = '\0';
		Tcl_UtfToExternal(NULL, mainPtr->isoEncoding, buf, m,
			      TCL_ENCODING_START | TCL_ENCODING_END,
			      NULL, buf2, sizeof (buf2), &srcRead,
			      &dstWrote, &dstChars);
	    } else {
		buf2[0] = '?';
		if (uch > 0 && uch < 0x100) {
		    buf2[0] = uch;
		}
	    }
	    c = buf2[0] & 0xff;
	} else {
	    c = 0;
	}
	if (isspace(c) || (c == 0)) {
	    term = p2;
	    termX = newX;
	}
	curX = newX;
    }

    /*
     * P points to the first character that doesn't fit in the desired
     * span. Use the flags to figure out what to return.
     */

    if ((flags & CK_PARTIAL_OK) && (curX < maxX)) {
	curX = newX;
	if (*p) {
	    n = Tcl_UtfToUniChar(p, &uch);
	    p += n;
	    ++nChars;
	}
    }
    if ((flags & CK_AT_LEAST_ONE) && (term == source) && (maxChars > 0)
	     && !isspace((unsigned char) *term)) {
	term = p;
	termX = curX;
	if (term == source) {
	    n = Tcl_UtfToUniChar(term, &uch);
	    term += n;
	    ++nChars;
	}
    } else if ((maxChars == 0) || !(flags & CK_WHOLE_WORDS)) {
	term = p;
	termX = curX;
    }
    *nextXPtr = termX;
    *nextCPtr = term - source;
    return nChars;
#else
    /*
     * Scan the input string one character at a time, until a character
     * is found that crosses maxX.
     */

    newX = curX = startX;
    termX = 0;
    term = source;
    for (p = source, c = *p & 0xff; c != '\0' && maxChars > 0;
	p++, maxChars--) {
	++nChars;
	if ((CHARTYPE(c).type == NORMAL) || (CHARTYPE(c).type == REPLACE) ||
	    (CHARTYPE(c).type == GCHAR)) {
	    newX += CHARTYPE(c).width;
	} else if (CHARTYPE(c).type == TAB) {
	    if (!(flags & CK_IGNORE_TABS)) {
		newX += 8;
		rem = (newX - tabOrigin) % 8;
		if (rem < 0) {
		    rem += 8;
		}
		newX -= rem;
	    }
	} else if (CHARTYPE(c).type == NEWLINE) {
	    if (flags & CK_NEWLINES_NOT_SPECIAL) {
		newX += CHARTYPE(c).width;
	    } else {
		break;
	    }
	}
	if (newX > maxX) {
	    break;
	}
	if (maxChars > 1) {
	    c = p[1] & 0xff;
	} else {
	    c = 0;
	}
	if (isspace(c) || (c == 0)) {
	    term = p+1;
	    termX = newX;
	}
	curX = newX;
    }

    /*
     * P points to the first character that doesn't fit in the desired
     * span. Use the flags to figure out what to return.
     */

    if ((flags & CK_PARTIAL_OK) && (curX < maxX)) {
	curX = newX;
	p++;
	++nChars;
    }
    if ((flags & CK_AT_LEAST_ONE) && (term == source) && (maxChars > 0)
	     && !isspace((unsigned char) *term)) {
	term = p;
	termX = curX;
	if (term == source && *source) {
	    term++;
	    termX = newX;
	    ++nChars;
	}
    } else if ((maxChars == 0) || !(flags & CK_WHOLE_WORDS)) {
	term = p;
	termX = curX;
    }
    *nextXPtr = termX;
    *nextCPtr = term - source;
    return nChars;
#endif
}

/*
 *--------------------------------------------------------------
 *
 * CkDisplayChars --
 *
 *	Draw a string of characters on the screen, converting
 *	tabs to the right number of spaces and control characters
 *	to sequences of the form "\xhh" where hh are two hex
 *	digits.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets drawn on the screen.
 *
 *--------------------------------------------------------------
 */

void
CkDisplayChars(mainPtr, window, string, numChars, x, y, tabOrigin, flags)
    CkMainInfo *mainPtr;	/* Needed for encoding. */
    WINDOW *window;		/* Curses window. */
    char *string;		/* Characters to be displayed. */
    int numChars;		/* Number of characters to display from
				 * string. */
    int x, y;			/* Coordinates at which to draw string. */
    int tabOrigin;		/* X-location that serves as "origin" for
				 * tab stops. */
    int flags;			/* Flags to control display.  Only
				 * CK_NEWLINES_NOT_SPECIAL, CK_IGNORE_TABS
				 * and CK_FILL_UNTIL_EOL are supported right
				 * now.  See CkMeasureChars for information
				 * about it. */
{
    register char *p;		/* Current character being scanned. */
    register int c;
    int startX;			/* X-coordinate corresponding to start. */
    int curX;			/* X-coordinate corresponding to p. */
    char replace[16];
    int rem, dummy, maxX, nc = 0;

    /*
     * Scan the string one character at a time and display the
     * character.
     */

    getmaxyx(window, dummy, maxX);
#if CK_USE_UTF
    nc = Tcl_NumUtfChars(string, numChars);
    if (nc > maxX)
	numChars = Tcl_UtfAtIndex(string, maxX) - string;
    else
	numChars = nc;
#endif
    if (numChars > maxX)
        numChars = maxX;
    p = string;
    if (x < 0) {
	x = -x;
#if CK_USE_UTF
	x = Tcl_UtfAtIndex(p, x) - p;
#endif
	p += x;
	numChars -= x;
	x = 0;
    }
    wmove(window, y, x);
    startX = curX = x;
    for (; numChars > 0; numChars--, p += nc) {
#if CK_USE_UTF
	Tcl_UniChar uch;
	int len;

	if (*p == '\0')
	    break;
	nc = Tcl_UtfToUniChar(p, &uch);
	if (mainPtr->isoEncoding) {
	    int srcRead, dstWrote, dstChars;
	    char buf[TCL_UTF_MAX];

	    buf[0] = '\0';
	    Tcl_UtfToExternal(NULL, mainPtr->isoEncoding, p, nc,
			  TCL_ENCODING_START | TCL_ENCODING_END,
			  NULL, buf, sizeof (buf), &srcRead,
			  &dstWrote, &dstChars);
	    c = buf[0] & 0xff;
	    if (dstWrote != 1 || (c == '?' && uch != '?')) {
		c = '\0';
	    }
	} else {
	    c = uch & 0xff;
	}
	if ((unsigned int) uch < 0x100 && CHARTYPE(c).type == TAB) {
	    if (!(flags & CK_IGNORE_TABS)) {
		curX += 8;
		rem = (curX - tabOrigin) % 8;
		if (rem < 0) {
		    rem += 8;
		}
		curX -= rem;
	    }
	    while (startX < curX) {
	    	waddch(window, ' ');
	    	startX++;
	    }
	    continue;
	} else if ((unsigned int) uch < 0x100 && CHARTYPE(c).type == GCHAR) {
	    long gchar;

	    if (Ck_GetGChar(NULL, gcharTab[c - 0x81], &gchar) != TCL_OK)
		goto replaceChar;
	    waddch(window, gchar);
	    curX++;
	} else if ((unsigned int) uch < 0x100 &&
		   CHARTYPE(c).type == NEWLINE &&
		   !(flags & CK_NEWLINES_NOT_SPECIAL)) {
	    y++;
	    wmove(window, y, x);
	    curX = x;
	} else if (mainPtr->isoEncoding) {
	    if ((unsigned int) uch < 0x20 ||
		CHARTYPE(c).type == REPLACE) {

replaceChar:
		len = MakeUCRepl(uch, replace);
		if (len + curX > maxX) {
		    len = maxX - curX;
		    if (len < 0)
			len = 0;
		}
		waddnstr(window, replace, len);
	    } else {
		len = 1;
		waddch(window, c);
	    }
	    curX += len;
	} else {
	    len = wcwidth((wint_t) uch);
	    if (len < 0 || (unsigned int) uch < 0x20) {
		len = MakeUCRepl(uch, replace);
		if (len + curX > maxX) {
		    len = maxX - curX;
		    if (len < 0)
			len = 0;
		}
		waddnstr(window, replace, len);
	    } else {
#ifdef USE_NCURSESW
		wchar_t w[2];

		w[0] = uch;
		w[1] = 0;
		waddnwstr(window, w, 1);
#else
		waddnstr(window, p, nc);
#endif
	    }
	    curX += len;
	}
#else
	nc = 1;
	c = *p & 0xff;
	if (c == '\0')
	    break;
	if (CHARTYPE(c).type == NORMAL) {
	    waddch(window, c);
	    curX++;
	}
	if (CHARTYPE(c).type == TAB) {
	    if (!(flags & CK_IGNORE_TABS)) {
		curX += 8;
		rem = (curX - tabOrigin) % 8;
		if (rem < 0) {
		    rem += 8;
		}
		curX -= rem;
	    }
	    while (startX < curX) {
	    	waddch(window, ' ');
	    	startX++;
	    }
	    continue;
	} else if (CHARTYPE(c).type == GCHAR) {
	    long gchar;

	    if (Ck_GetGChar(NULL, gcharTab[c - 0x81], &gchar) != TCL_OK)
		goto replaceChar;
	    waddch(window, gchar);
	    startX++;
	    curX = startX;
	    continue;
	} else if (CHARTYPE(c).type == REPLACE || (CHARTYPE(c).type == NEWLINE
	    && (flags & CK_NEWLINES_NOT_SPECIAL))) {
replaceChar:
	    if ((c < sizeof(mapChars)) && (mapChars[c] != 0)) {
		replace[0] = '\\';
	        replace[1] = mapChars[c];
	        replace[2] = '\0';
		waddstr(window, replace);
	        curX += 2;
	    } else {
		replace[0] = '\\';
	        replace[1] = 'x';
	        replace[2] = hexChars[(c >> 4) & 0xf];
	        replace[3] = hexChars[c & 0xf];
	        replace[4] = '\0';
	        waddstr(window, replace);
	        curX += 4;
  	    }
	} else if (CHARTYPE(c).type == NEWLINE) {
	    y++;
	    wmove(window, y, x);
	    curX = x;
	}
#endif
	startX = curX;
    }
    if (flags & CK_FILL_UNTIL_EOL) {
	while (startX < maxX) {
	   waddch(window, ' ');
	   startX++;
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * CkUnderlineChars --
 *
 *	Draw a range of string of characters on the screen,
 *	converting tabs to the right number of spaces and control
 *	characters to sequences of the form "\xhh" where hh are two hex
 *	digits.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets drawn on the screen.
 *
 *--------------------------------------------------------------
 */

void
CkUnderlineChars(mainPtr, window, string, numChars, x, y, tabOrigin,
	flags, first, last)
    CkMainInfo *mainPtr;	/* Needed for encoding. */
    WINDOW *window;		/* Curses window. */
    char *string;		/* Characters to be displayed. */
    int numChars;		/* Number of characters to display from
				 * string. */
    int x, y;			/* Coordinates at which to draw string. */
    int tabOrigin;		/* X-location that serves as "origin" for
				 * tab stops. */
    int flags;			/* Flags to control display.  Only
				 * CK_NEWLINES_NOT_SPECIAL, CK_IGNORE_TABS
				 * and CK_FILL_UNTIL_EOL are supported right
				 * now.  See CkMeasureChars for information
				 * about it. */
    int first, last;            /* Range: First and last characters to
				 * display. */
{
    register char *p;		/* Current character being scanned. */
    register int c, count;
    int startX;			/* X-coordinate corresponding to start. */
    int curX;			/* X-coordinate corresponding to p. */
    char replace[10];
    int rem, dummy, maxX, nc = 0;

    /*
     * Scan the string one character at a time and display the
     * character.
     */

    count = 0;
    getmaxyx(window, dummy, maxX);
    maxX -= x;
#if CK_USE_UTF
    nc = Tcl_NumUtfChars(string, numChars);
    if (nc > maxX)
	numChars = Tcl_UtfAtIndex(string, maxX) - string;
    else
	numChars = nc;
#endif
    if (numChars > maxX)
        numChars = maxX;
    p = string;
    if (x < 0) {
	x = -x;
#if CK_USE_UTF
	x = Tcl_UtfAtIndex(p, x) - p;
#endif
	p += x;
	numChars -= x;
	count -= x;
	x = 0;
    }
    wmove(window, y, x);
    startX = curX = x;
    for (; numChars > 0 && count <= last; numChars -= nc, count++, p += nc) {
#if CK_USE_UTF
	Tcl_UniChar uch;
	int len;

	if (*p == '\0')
	    break;
	if (mainPtr->isoEncoding == NULL) {
	    nc = Tcl_UtfToUniChar(p, &uch);
	} else {
	    Tcl_UtfToUniChar(p, &uch);
	    nc = 1;
	}
	nc = Tcl_UtfToUniChar(p, &uch);
	if (mainPtr->isoEncoding) {
	    int srcRead, dstWrote, dstChars;
	    char buf[TCL_UTF_MAX];

	    buf[0] = '\0';
	    Tcl_UtfToExternal(NULL, mainPtr->isoEncoding, p, nc,
			  TCL_ENCODING_START | TCL_ENCODING_END,
			  NULL, buf, sizeof (buf), &srcRead,
			  &dstWrote, &dstChars);
	    c = buf[0] & 0xff;
	    if (dstWrote != 1 || (c == '?' && uch != '?')) {
		c = '\0';
	    }
	} else {
	    c = uch & 0xff;
	}
	if ((unsigned int) uch < 0x100 && CHARTYPE(c).type == TAB) {
	    if (!(flags & CK_IGNORE_TABS)) {
		curX += 8;
		rem = (curX - tabOrigin) % 8;
		if (rem < 0) {
		    rem += 8;
		}
		curX -= rem;
	    }
	    while (startX < curX) {
		startX++;
		if (count >= first)
		    waddch(window, ' ');
		else
		    wmove(window, y, startX);
	    }
	    continue;
	} else if ((unsigned int) uch < 0x100 && CHARTYPE(c).type == GCHAR) {
	    long gchar;

	    if (Ck_GetGChar(NULL, gcharTab[c - 0x81], &gchar) != TCL_OK)
		goto replaceChar;
	    if (count >= first)
		waddch(window, gchar);
	    else
		wmove(window, y, startX);
	    curX++;
	} else if ((unsigned int) uch < 0x100 &&
		   CHARTYPE(c).type == NEWLINE &&
		   !(flags & CK_NEWLINES_NOT_SPECIAL)) {
	    y++;
	    wmove(window, y, x);
	    curX = x;
	} else if (mainPtr->isoEncoding) {
	    if ((unsigned int) uch < 0x20 ||
		CHARTYPE(c).type == REPLACE) {

replaceChar:
		len = MakeUCRepl(uch, replace);
		if (len + curX > maxX) {
		    len = maxX - curX;
		    if (len < 0)
			len = 0;
		}
		if (count >= first)
		    waddnstr(window, replace, len);
	    } else {
		len = 1;
		if (count >= first)
		    waddch(window, c);
	    }
	    curX += len;
	    if (count < first)
		wmove(window, y, curX);
	} else {
	    len = wcwidth((wint_t) uch);
	    if (len < 0 || (unsigned int) uch < 0x20) {
		len = MakeUCRepl(uch, replace);
		if (len + curX > maxX) {
		    len = maxX - curX;
		    if (len < 0)
			len = 0;
		}
		if (count >= first)
		    waddnstr(window, replace, len);
	    } else {
#ifdef USE_NCURSESW
		wchar_t w[2];

		w[0] = uch;
		w[1] = 0;
		if (count >= first)
		    waddnwstr(window, w, 1);
#else
		if (count >= first)
		    waddnstr(window, p, nc);
#endif
	    }
	    curX += len;
	    if (count < first)
		wmove(window, y, curX);
	}
#else
	nc = 1;
	c = *p & 0xff;
	if (CHARTYPE(c).type == NORMAL) {
	    curX++;
	    if (count >= first) {
		waddch(window, c);
	    } else
		wmove(window, y, startX);
	}
	if (CHARTYPE(c).type == TAB) {
	    if (!(flags & CK_IGNORE_TABS)) {
		curX += 8;
		rem = (curX - tabOrigin) % 8;
		if (rem < 0) {
		    rem += 8;
		}
		curX -= rem;
	    }
	    while (startX < curX) {
		startX++;
		if (count >= first)
		    waddch(window, ' ');
		else
		    wmove(window, y, startX);
	    }
	    continue;
	} else if (CHARTYPE(c).type == GCHAR) {
	    long gchar;

	    if (Ck_GetGChar(NULL, gcharTab[c - 0x81], &gchar) != TCL_OK)
		goto replaceChar;
	    curX++;
	    if (count >= first)
		waddch(window, gchar);
	    else
		wmove(window, y, curX);
	} else if (CHARTYPE(c).type == REPLACE || (CHARTYPE(c).type == NEWLINE
	    && (flags & CK_NEWLINES_NOT_SPECIAL))) {
replaceChar:
	    if ((c < sizeof(mapChars)) && (mapChars[c] != 0)) {
		replace[0] = '\\';
	        replace[1] = mapChars[c];
	        replace[2] = '\0';
	        curX += 2;
		if (count >= first)
		    waddstr(window, replace);
		else
		    wmove(window, y, curX);
	    } else {
		replace[0] = '\\';
	        replace[1] = 'x';
	        replace[2] = hexChars[(c >> 4) & 0xf];
	        replace[3] = hexChars[c & 0xf];
	        replace[4] = '\0';
	        curX += 4;
		if (count >= first)
		    waddstr(window, replace);
		else
		    wmove(window, y, curX);
  	    }
	} else if (CHARTYPE(c).type == NEWLINE) {
	    y++;
	    wmove(window, y, x);
	    curX = x;
	}
#endif
	startX = curX;
    }
}

