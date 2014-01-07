/*
 * ckBorder.c --
 *
 *	Manage borders by using alternate character set.
 *
 * Copyright (c) 1995 Christian Werner
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "ckPort.h"
#include "ck.h"

/*
 * Variables used in this module.
 */

static Tcl_HashTable gCharTable;          /* Maps gChar names to values. */
static int initialized = 0;               /* gCharTable initialized. */


/*
 *------------------------------------------------------------------------
 *
 * Ck_GetGChar --
 *
 *	Return curses ACS character given string.
 *
 *------------------------------------------------------------------------
 */

int
Ck_GetGChar(interp, name, gchar)
    Tcl_Interp *interp;
    char *name;
    long *gchar;
{
    Tcl_HashEntry *hPtr;

    if (!initialized) {
	int new;

	Tcl_InitHashTable(&gCharTable, TCL_STRING_KEYS);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "ulcorner", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_ULCORNER);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "urcorner", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_URCORNER);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "llcorner", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_LLCORNER);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "lrcorner", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_LRCORNER);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "rtee", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_RTEE);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "ltee", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_LTEE);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "btee", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_BTEE);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "ttee", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_TTEE);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "hline", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_HLINE);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "vline", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_VLINE);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "plus", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_PLUS);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "s1", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_S1);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "s9", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_S9);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "diamond", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_DIAMOND);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "ckboard", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_CKBOARD);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "degree", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_DEGREE);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "plminus", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_PLMINUS);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "bullet", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_BULLET);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "larrow", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_LARROW);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "rarrow", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_RARROW);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "darrow", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_DARROW);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "uarrow", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_UARROW);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "board", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_BOARD);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "lantern", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_LANTERN);
	hPtr = Tcl_CreateHashEntry(&gCharTable, "block", &new);
	Tcl_SetHashValue(hPtr, (ClientData) ACS_BLOCK);

	initialized = 1;
    }
    
    hPtr = Tcl_FindHashEntry(&gCharTable, name);
    if (hPtr == NULL) {
	if (interp != NULL)
	    Tcl_AppendResult(interp,
		"bad gchar \"", name, "\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (gchar != NULL)
	*gchar = (long) Tcl_GetHashValue(hPtr);
    return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * Ck_SetGChar --
 *
 *	Modify ACS mapping.
 *
 *------------------------------------------------------------------------
 */

int
Ck_SetGChar(interp, name, gchar)
    Tcl_Interp *interp;
    char *name;
    long gchar;
{
    Tcl_HashEntry *hPtr;

    if (!initialized)
	Ck_GetGChar(interp, "ulcorner", NULL);
    hPtr = Tcl_FindHashEntry(&gCharTable, name);    
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "bad gchar \"", name, "\"", (char *) NULL);
	return TCL_ERROR;
    }
    Tcl_SetHashValue(hPtr, (ClientData) gchar);
    return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * Ck_GetBorder --
 *
 *	Create border from string.
 *
 *------------------------------------------------------------------------
 */

CkBorder *
Ck_GetBorder(interp, string)
    Tcl_Interp *interp;
    char *string;
{
    int i, largc;
    long bchar[8];
    char **largv;
    CkBorder *borderPtr;

    if (Tcl_SplitList(interp, string, &largc, &largv) != TCL_OK)
	return NULL;
    if (largc != 1 && largc != 3 && largc != 6 && largc != 8) {
	ckfree((char *) largv);
	Tcl_AppendResult(interp, "illegal number of box characters",
	    (char *) NULL);
	return NULL;
    }
    for (i = 0; i < sizeof (bchar) / sizeof (bchar[0]); i++)
	bchar[i] = ' ';
    for (i = 0; i < largc; i++) {
	if (strlen(largv[i]) == 1)
	    bchar[i] = (unsigned char) largv[i][0];
	else if (Ck_GetGChar(interp, largv[i], &bchar[i]) != TCL_OK) {
	    ckfree((char *) largv);
	    return NULL;
	}
    }
    if (largc == 1) {
    	for (i = 1; i < sizeof (bchar) / sizeof (bchar[0]); i++)
	    bchar[i] = bchar[0];
    } else if (largc == 3) {
	bchar[3] = bchar[7] = bchar[2];
	bchar[2] = bchar[4] = bchar[6] = bchar[0];
	bchar[5] = bchar[1];
    } else if (largc == 6) {
	bchar[6] = bchar[5];
	bchar[5] = bchar[1];
	bchar[7] = bchar[3];
    }
    ckfree((char *) largv);
    borderPtr = (CkBorder *) ckalloc(sizeof (CkBorder));
    memset(borderPtr, 0, sizeof (CkBorder));
    for (i = 0; i < 8; i++)
	borderPtr->gchar[i] = bchar[i];
    borderPtr->name = ckalloc(strlen(string) + 1);
    strcpy(borderPtr->name, string);	
    return borderPtr;
}

/*
 *------------------------------------------------------------------------
 *
 * Ck_FreeBorder --
 *
 *	Release memory related to border.
 *
 *------------------------------------------------------------------------
 */

void
Ck_FreeBorder(borderPtr)
    CkBorder *borderPtr;
{
    ckfree(borderPtr->name);
    ckfree((char *) borderPtr);
}

/*
 *------------------------------------------------------------------------
 *
 * Ck_NameOfBorder --
 *
 *	Create border from string.
 *
 *------------------------------------------------------------------------
 */

char *
Ck_NameOfBorder(borderPtr)
    CkBorder *borderPtr;
{
    return borderPtr->name;
}

/*
 *------------------------------------------------------------------------
 *
 * Ck_DrawBorder --
 *
 *	Given window, border and bounding box, draw border.
 *
 *------------------------------------------------------------------------
 */

void
Ck_DrawBorder(winPtr, borderPtr, x, y, width, height)
    CkWindow *winPtr;
    CkBorder *borderPtr;
    int x, y, width, height;
{
    int i, *gchar;
    WINDOW *w;

    if (winPtr->window == NULL)
	return;
    w = winPtr->window;
    gchar = borderPtr->gchar;
    if (width < 1 || height < 1)
	return;
    if (width == 1) {
	for (i = y; i < height + y; i++)
	    mvwaddch(w, i, x, gchar[3]);
	return;
    }
    if (height == 1) {
	for (i = x; i < width + x; i++)
	    mvwaddch(w, y, i, gchar[1]);
	return;
    }
    if (width == 2) {
	mvwaddch(w, y, x, gchar[0]);
	mvwaddch(w, y, x + 1, gchar[2]);
        for (i = y + 1; i < height - 1 + y; i++)
	    mvwaddch(w, i, x, gchar[7]);
        for (i = y + 1; i < height - 1 + y; i++)
	    mvwaddch(w, i, x + 1, gchar[3]);
	mvwaddch(w, height - 1 + y, x, gchar[6]);
	mvwaddch(w, height - 1 + y, x + 1, gchar[4]);
	return;
    }
    if (height == 2) {
	mvwaddch(w, y, x, gchar[0]);
	mvwaddch(w, y + 1, x, gchar[6]);
        for (i = x + 1; i < width - 1 + x; i++)
	    mvwaddch(w, y, i, gchar[1]);
        for (i = x + 1; i < width - 1 + x; i++)
	    mvwaddch(w, y + 1, i, gchar[5]);
	mvwaddch(w, y, width - 1 + x, gchar[2]);
	mvwaddch(w, y + 1, width - 1 + x, gchar[4]);
	return;
    }
    mvwaddch(w, y, x, gchar[0]);
    for (i = x + 1; i < width - 1 + x; i++)
	mvwaddch(w, y, i, gchar[1]);
    mvwaddch(w, y, width - 1 + x, gchar[2]);
    for (i = y + 1; i < height - 1 + y; i++)
        mvwaddch(w, i, width - 1 + x, gchar[3]);
    mvwaddch(w, height - 1 + y, width - 1 + x, gchar[4]);
    for (i = x + 1; i < width - 1 + x; i++)
	mvwaddch(w, height - 1 + y, i, gchar[5]);
    mvwaddch(w, height - 1 + y, x, gchar[6]);
    for (i = y + 1; i < height - 1 + y; i++)
	mvwaddch(w, i, x, gchar[7]);
}
