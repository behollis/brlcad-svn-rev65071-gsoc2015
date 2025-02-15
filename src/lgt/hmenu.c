/*                         H M E N U . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file lgt/hmenu.c
 *
 * This code is derived in part from menuhit(9.3) in AT&T 9th Edition
 * UNIX, Version 1 Programmer's Manual.
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "vmath.h"
#include "raytrace.h"
#include "fb.h"

#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"


extern int LI, CO;
#define MAX_PROMPT 10
#define Alloc(p_, t_, s_) (p_) = (t_ *) bu_malloc((unsigned)(s_), #p_);

#ifndef Max
#  define Max(_a, _b)	((_a)<(_b)?(_b):(_a))
#  define Min(_a, _b)	((_a)>(_b)?(_b):(_a))
#endif
#define ring_Bell() putchar('\07'),  (void) fflush(stdout)
#define M_UP 'u'
#define M_DOWN 'd'
#define M_HELP 'h'
#define M_SELECT ' '
#define M_RETURN '\r'
#define M_NOSELECT 'q'

#define P_OFF (0)
#define P_ON (1)
#define P_FORCE (1<<1)

#define PutMenuChar(_c, _co, _ro, _map, _bit)\
    {\
	static int lro = -1, lco = -1;\
	if ((_map) & (_bit) || (_bit) == 0) {\
	    if (lco++ != (_co)-1 || lro != (_ro)) {\
		MvCursor(_co, _ro);\
		lco = _co;\
	    }\
	    putchar((_c));\
	}\
	(_bit) <<= 1;\
	(_co)++;\
    }


int hm_dirty = 0;
static HWindow *windows = 0;

#define ENTRY (itemp-win->menup->item)

typedef struct nmllist HMllist;

struct nmllist {
    HMitem *itemp;
    HMllist *next;
};


static void
free_HMitems(HMitem *itemp)
{
    HMitem *citemp;
    for (citemp = itemp; citemp->text != NULL; citemp++) {
	free(citemp->text);
	if (citemp->help != NULL)
	    free(citemp->help);
    }
    free((char *) itemp);
    return;
}


static void
free_HMllist(HMllist *listp)
{
    if (listp == (HMllist *) 0)
	return;
    free_HMllist(listp->next);
    free((char *) listp->itemp);
    free((char *) listp);
    return;
}


static void
hm_Put_Item(HWindow *win, HMitem *itemp, int flag)
{
    int label_len = strlen(itemp->text);
    static char *buf;
    char *p;
    int col = win->menux;
    int row = win->menuy+(ENTRY-win->menup->prevtop)+1;
    int width = win->width;
    int bitmap = flag & P_FORCE ?
	~0 : win->dirty[ENTRY+1];
    int bit = 1;
    int writemask = 0;

    buf = (char *)bu_calloc(1, sizeof(char) * width, "alloc buf");
    p = buf;

    if (bitmap == 0)
	return;

    /* Pad text on left.					*/
    if (itemp->text[0] & 0200) {
	/* right-justified */
	int i;
	for (i = 0; i < width - label_len; i++)
	    *p++ = ' ';
	for (i = 1; itemp->text[i] != '\0'; i++)
	    *p++ = itemp->text[i];
    } else				/* left-justified */
	if (itemp->text[label_len-1] & 0200) {
	    int i;
	    for (i = 0; !(itemp->text[i] & 0200); i++)
		*p++ = itemp->text[i];
	    for (; i < width; i++)
		*p++ = ' ';
	} else {
	    /* centered */
	    int i, j;
	    for (i = 0; i < (width - label_len + 1)/2; i++)
		*p++ = ' ';
	    for (j = 0; itemp->text[j] != '\0'; j++)
		*p++ = itemp->text[j];
	    for (i += j; i < width; i++)
		*p++ = ' ';
	}
    *p = '\0';

    PutMenuChar('|', col, row, bitmap, bit);
    if (flag & P_ON)
	(void) SetStandout();
    else
	(void) ClrStandout();

    {
	/* Optimized printing of entry.				*/
	if (bitmap == ~0) {
	    (void) fputs(buf, stdout);
	    col += p-buf;
	    bit <<= p-buf;
	} else {
	    int i;
	    for (i = 0; i < p-buf; i++)
		writemask |= 1<<(i+1);
	    for (i = 0; i < p-buf; i++) {
		if ((bitmap & writemask) == writemask)
		    break;
		writemask &= ~bit;
		PutMenuChar(buf[i], col, row, bitmap, bit);
	    }
	    if (i < p-buf) {
		MvCursor(col, row);
		(void) fputs(&buf[i], stdout);
		col += (p-buf) - i;
		bit <<= (p-buf) - i;
	    }
	}
    }
    bu_free(buf, "free buf");

    if (flag & P_ON)
	(void) ClrStandout();
    PutMenuChar('|', col, row, bitmap, bit);
    return;
}


static void
hm_Put_Border(HWindow *win, int row, int mark)
{
    int i;
    int bit = 1;
    int col = win->menux;
    int bitmap = win->dirty[row - win->menuy];
    static char *buf;
    char *p;

    buf = (char *)bu_calloc(1, sizeof(char) * win->width, "alloc buf");
    p = buf;
    *p++ = (char)mark;
    for (i = 0; i < win->width; i++)
	*p++ = '-';
    *p++ = (char)mark;
    *p = '\0';
    if (bitmap == ~0) {
	MvCursor(col, row);
	(void) fputs(buf, stdout);
    } else
	for (i = 0; i < p - buf; i++)
	    PutMenuChar(buf[i], col, row, bitmap, bit);

    bu_free(buf, "free buf");
    return;
}


static void
hm_Setbit(HWindow *win, int col, int row)
{
    int bit = col - win->menux;
    win->dirty[row-(win->menuy-win->menup->prevtop)] |= bit == 0 ? 1 : 1 << bit;
    return;
}


static void
hm_Clrmap(HWindow *win)
{
    int row;
    for (row = 0; row <= win->height+1; row++)
	win->dirty[row] = 0;
    return;
}


static void
hm_Setmap(HWindow *win)
{
    int row;
    for (row = 0; row <= win->height+1; row++)
	win->dirty[row] = ~0; /* 0xffff... */
    return;
}


static HWindow *
hm_In_Win(int x, int y, HWindow *win)
{
    for (; win != (HWindow *) 0; win = win->next) {
	int height = Min(win->height, MAXVISABLE);
	if (! (x < win->menux || x > win->menux + win->width + 1 ||
	       y < win->menuy || y > win->menuy + height + 1)
	    )
	    return win;
    }
    return (HWindow *) 0;
}


static void
hm_Draw_Win(HWindow *win)
{
    HMitem *itemp;
    int height;
    hm_Put_Border(win, win->menuy, win->menup->prevtop > 0 ? '^' : '+');
    for (itemp = win->menup->item + win->menup->prevtop;
	 ENTRY-win->menup->prevtop < MAXVISABLE && itemp->text != NULL;
	 itemp++
	)
	hm_Put_Item(win, itemp,
		    ENTRY == win->menup->prevhit ? P_ON : P_OFF
	    );
    height = Min(MAXVISABLE, win->height);
    hm_Put_Border(win, win->menuy+height+1, (char)(ENTRY < win->height ? 'v' : '+'));
    hm_Clrmap(win);
    (void) fflush(stdout);
    return;
}


static void
hm_Redraw_Win(HWindow *win)
{
    if (win == (HWindow *) 0) {
	hm_dirty = 0;
	return;
    }
    hm_Redraw_Win(win->next);
    hm_Draw_Win(win);
    return;
}


static void
hm_Help(HWindow *win, int entry)
{
    int bottomline = 1;
    HWindow *curwin;
    for (curwin = windows; curwin != (HWindow *) 0; curwin = curwin->next) {
	if (curwin->height > MAXVISABLE) {
	    bottomline = curwin->menuy + MAXVISABLE + 1;
	    break;
	}
	bottomline = Max(bottomline, curwin->menuy + curwin->height + 1);
    }
    MvCursor(0, bottomline+1);
    (void) ClrEOL();
    (void) SetStandout();
    (void) printf("%s", win->menup->item[entry].help);
    (void) ClrStandout();
    (void) fflush(stdout);
    (void) hm_ungetchar(hm_getchar());
    MvCursor(0, bottomline+1);
    (void) ClrEOL();
    return;
}


static void
hm_Lift_Win(HWindow *win)
{
    int row, col;
    int lastcol = -1, lastrow = -1;
    int endcol = win->menux + win->width + 2;
    int endrow = win->menuy +
	Min(win->height, MAXVISABLE) + 2;
    for (row = win->menuy; row < endrow; row++) {
	for (col = win->menux; col < endcol; col++) {
	    HWindow *olwin;
	    if ((olwin = hm_In_Win(col, row, win->next))
		== (HWindow *) 0
		)
	    {
		if (lastcol != col-1 || lastrow != row)
		    MvCursor(col, row);
		lastcol = col; lastrow = row;
		putchar(' ');
	    } else {
		hm_Setbit(olwin, col, row);
		hm_dirty = 1;
	    }
	}
    }
    (void) fflush(stdout);
    return;
}


void
hmredraw(void)
{
    HWindow *win;
    for (win = windows; win != (HWindow *) 0; win = win->next)
	hm_Setmap(win);
    hm_dirty = 1;
    return;
}


HMitem *
hmenuhit(HMenu *menup, int menux, int menuy)
/* -> first HMitem in array.		*/

{
    HMitem *itemp;
    HMitem *retitemp = 0;
    HWindow *win;
    int done = 0;
    int dynamic = 0;
    static int hmlevel = 0;

    if (++hmlevel == 1) {
	save_Tty(0);
	set_Cbreak(0);
	clr_Echo(0);
    }

    /* If generator function is provided, dynamically allocate the
       menu items.
    */
    if ((dynamic = (menup->item == (HMitem *) 0))) {
	int i;
	HMitem *gitemp;
	HMllist llhead, **listp;
	for (i = 0, listp = &llhead.next;
	     ;
	     i++, listp = &(*listp)->next
	    )
	{
	    Alloc(*listp, HMllist, sizeof(HMllist));
	    Alloc((*listp)->itemp, HMitem, sizeof(HMitem));
	    itemp = (*listp)->itemp;
	    if ((gitemp = (*menup->generator)(i)) == (HMitem *) 0) {
		itemp->text = NULL;
		(*listp)->next = 0;
		break;
	    }
	    if (gitemp->text != NULL) {
		size_t len = strlen(gitemp->text) + 1;
		Alloc(itemp->text, char, len);
		bu_strlcpy(itemp->text, gitemp->text, len);
	    } else
		itemp->text = NULL;
	    if (gitemp->help != NULL) {
		size_t len = strlen(gitemp->help) + 1;
		Alloc(itemp->help, char, len);
		bu_strlcpy(itemp->help, gitemp->help, len);
	    } else
		itemp->help = NULL;
	    itemp->next = gitemp->next;
	    itemp->dfn = gitemp->dfn;
	    itemp->bfn = gitemp->bfn;
	    itemp->hfn = gitemp->hfn;
	    itemp->data = gitemp->data;
	}
	/*prnt_HMllist(llhead.next);*/

	/* Steal the field that the user isn't using temporarily to
	   emulate the static allocation of menu items.
	*/
	if (i > 0) {
	    Alloc(menup->item, HMitem, (i+1)*sizeof(HMitem));
	    for (i = 0, listp = &llhead.next;
		 (*listp) != (HMllist *) 0;
		 i++,   listp = &(*listp)->next
		)
		menup->item[i] = *(*listp)->itemp;
	}
	free_HMllist(llhead.next);
	if (i == 0) {
	    /* Zero items, so return NULL */
	    if (hmlevel-- == 1)
		reset_Tty(0);
	    return (HMitem *) 0;
	}
    }
    Alloc(win, HWindow, sizeof(HWindow));
    win->menup = menup;
    win->width = 0;
    win->next = windows;
    windows = win;

    /* Determine width of menu, allowing for border.		*/
    for (itemp = menup->item; itemp->text != NULL; itemp++) {
	int len = 0;
	int i;
	for (i = 0; itemp->text[i] != '\0'; i++)
	    if (! (itemp->text[i] & 0200))
		len++;
	win->width = Max(win->width, len);
    }
    win->height = ENTRY;

    /* Determine origin (top-left corner) of menu.			*/
    if (win->next != (HWindow *) 0) {
	win->menux = win->next->menux + win->next->width + 2;
	win->menuy = win->next->menuy;
	if (win->menux + win->width + 2 > CO) {
	    /* Wrap-around screen. */
	    win->menux = menux += 2;
	    win->menuy = menuy += 2;
	}
    } else {
	/* Top-level menu. */
	win->menux = menux;
	win->menuy = menuy;
    }
    if (menup->prevhit < 0 || menup->prevhit >= win->height)
	menup->prevhit = 0;
    itemp = &menup->item[menup->prevhit];

    Alloc(win->dirty, int, (win->height+2)*sizeof(int));
    hm_Setmap(win);
    hm_Draw_Win(win);
    while (! done) {
	if (hm_dirty)
	    hm_Redraw_Win(windows);
	MvCursor(win->menux+win->width+2, win->menuy+(ENTRY-win->menup->prevtop)+1);
	(void) fflush(stdout);
	switch (hm_getchar()) {
	    case M_UP :
		if (ENTRY == 0)
		    ring_Bell();
		else {
		    hm_Put_Item(win, itemp, P_OFF | P_FORCE);
		    itemp--;
		    menup->prevhit = ENTRY;
		    if (ENTRY < win->menup->prevtop) {
			win->menup->prevtop -=
			    ENTRY > MAXVISABLE/2 ?
			    MAXVISABLE/2 : ENTRY;
			win->menup->prevtop--;
			hm_Setmap(win);
			hm_Draw_Win(win);
		    } else
			hm_Put_Item(win, itemp, P_ON | P_FORCE);
		}
		break;
	    case M_DOWN :
		if (ENTRY >= win->height-1)
		    ring_Bell();
		else {
		    hm_Put_Item(win, itemp, P_OFF | P_FORCE);
		    itemp++;
		    menup->prevhit = ENTRY;
		    if (ENTRY - win->menup->prevtop >= MAXVISABLE) {
			win->menup->prevtop +=
			    win->height-ENTRY > MAXVISABLE/2 ?
			    MAXVISABLE/2 : win->height-ENTRY;
			hm_Setmap(win);
			hm_Draw_Win(win);
		    } else
			hm_Put_Item(win, itemp, P_ON | P_FORCE);
		}
		break;
	    case '?' :
	    case M_HELP :
		hm_Help(win, (int)ENTRY);
		break;
	    case M_SELECT :
	    case M_RETURN :
		if (itemp->next != (HMenu *) 0) {
		    HMitem *subitemp;
		    if (itemp->dfn != (void (*)()) 0)
			(*itemp->dfn)(itemp);
		    subitemp = hmenuhit(itemp->next, menux, menuy);
		    if (itemp->bfn != (void (*)()) 0)
			(*itemp->bfn)(itemp);
		    if (subitemp != (HMitem *) 0) {
			retitemp = subitemp;
			done = !menup->sticky;
		    }
		} else {
		    int level = hmlevel;
		    retitemp = itemp;
		    if (itemp->hfn != (int (*)()) 0) {
			reset_Tty(0);
			hmlevel = 0;
			retitemp->data = (*itemp->hfn)(itemp, (char **) NULL);
			hmlevel = level;
			set_Cbreak(0);
			clr_Echo(0);
		    }
		    done = ! menup->sticky;
		    if (menup->func != (void (*)()) 0) {
			reset_Tty(0);
			hmlevel = 0;
			(*menup->func)();
			hmlevel = level;
			set_Cbreak(0);
			clr_Echo(0);
		    }
		}
		break;
	    case M_NOSELECT :
		done = 1;
		break;
	    default :
		MvCursor(1, LI-1);
		(void) fflush(stdout);
		prnt_Scroll("Type 'd' down, 'u' up, 'h' help, <space> to select, 'q' no selection.");
		break;
	}
	(void) fflush(stdout);
    }
    /* Free storage of dynamic menu.				*/
    if (dynamic) {
	free_HMitems(menup->item);
	menup->item = 0;
    }
    hm_Lift_Win(win);
    windows = win->next;
    free((char *) win->dirty);
    free((char *) win);
    if (hmlevel-- == 1)
	reset_Tty(0);
    return retitemp;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
