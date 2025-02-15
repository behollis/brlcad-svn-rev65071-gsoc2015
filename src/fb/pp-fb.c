/*                         P P - F B . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2014 United States Government as represented by
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
 *
 */
/** @file pp-fb.c
 *
 * plot color shaded pictures from GIFT on a frame buffer.
 *
 * The plot file has a format similar to TEKTRONIX plots.
 * All data is represented with printable ASCII characters.
 * In the file layout, here are the character range uses:
 *
 *  	000 - 037	NULL to US	unused
 *  	040 - 057	SPACE to /	command characters
 *  	060 - 077	0 to ?		(0-7) 3 high bits of inten_high
 *  	100 - 137	@ to _		low 5 bits of inten or number.
 *
 * The following are command codes (encoded as code + 040 in the file):
 *	0 NUM	miss target (NUM=how many pixels)
 *  	1	switching to new surface (unused)
 *  	2	totally transparent surface (unused)
 *  	3 NUM	solid exterior item (NUM=item code)
 *  	4	transparent exterior, solid interior
 *  	6	point source of light (unused)
 *  	10 NUM	repeat intensity (NUM=how many pixels)
 *  	14	end of scanline
 *  	15	end of view
 *
 * Also, note that input lines are limited to 75 characters in length.
 *
 *	Original Version:  Gary Kuehl,  April 1983
 *	Ported to VAX:  Mike Muuss, January 1984
 *	Conversion to generic frame buffer utility using libfb(3).
 *	Gary S. Moss, BRL. 03/14/85
 *	This version: Gary Kuehl, Feb 1987
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#include "bu/color.h"
#include "bu/str.h"
#include "bu/log.h"
#include "fb.h"
#include "libtermio.h"


fb *fbp;

char ibuf[1024];	/* pp file buffer */

#define FBBUFSIZE 4096	/* Size of frame buffer DMA */
static unsigned char pix_buf[FBBUFSIZE]; /* Pixel buffer.			*/

#define FBWPIXEL(pix) \
    { COPYRGB(fb_p, pix); \
	fb_p += sizeof(RGBpixel); \
    }
char strg[51];
char g(void), gc(void);
struct colors {
    char *name;
    RGBpixel c_pixel;
}colortab[] = {
    {"black",	{0, 0, 0}},
    {"blue",	{0, 0, 255}},
    {"brown",	{200, 130, 0}},
    {"cyan",	{0, 255, 200}},
    {"flesh",	{255, 200, 160}},
    {"gray",	{120, 120, 120}},
    {"green",	{0, 255, 0}},
    {"lime", 	{200, 255, 0}},
    {"magenta",	{255, 0, 255}},
    {"olive",	{220, 190, 0}},
    {"orange",	{255, 100, 0}},
    {"pink",	{255, 200, 200}},
    {"red",		{255, 0, 0}},
    {"rose",	{255, 0, 175}},
    {"rust",	{200, 100, 0}},
    {"silver",	{237, 237, 237}},
    {"sky",		{0, 255, 255}},
    {"violet",	{200, 0, 255}},
    {"white",	{255, 255, 255}},
    {"yellow",	{255, 200, 0}}
};
int ifd, io, grid_w, grid_h, min_w, min_h, max_w, max_h, ni, opq=0;
int ib=0, ic=0, nc=0, ibc=0, itc=3;
int itmc[500];
long itm[500], ctoi(void);
off_t loci, loct=0, locd, loce;

void paint(void), prtclr(char raw), prtsmu(char raw);
int lookup(long int ix, long int *jx, int n);

int
main(int argc, char **argv)
{
    int c;
    char *cp;
    char cs[4];
    int i, j, k, lclr, ichg=0, gclr(void), cclr(char *pc);
    int il, iu, iclr, iskp, jclr, bsp(void);
    int scr_w=512, scr_h=512, scr_set=0;
    int ret;

    bu_log("DEPRECATION WARNING:  This command is scheduled for removal.  Please contact the developers if you use this command.\n\n");
    sleep(5);

    printf("GIFT-PRETTY File painted on Generic Framebuffer\n");

    /* check invocation */
    if (argc<2) {
	fprintf(stderr,"Usage: pp-fb [options] PPfilename\n");
	fprintf(stderr,"(See BRL-CAD Package Documentation for more info)\n");
	fprintf(stderr,"Options:\n");
	fprintf(stderr,"  -F framebuffer");
	fprintf(stderr," (Alternatively set environment variable FB_FILE)\n");
	fprintf(stderr,"  -W screen_width\n");
	fprintf(stderr,"  -N screen_height\n");
	bu_exit(10, NULL);
    }
    for (i=1;i<argc;i++) {
	if (BU_STR_EQUAL("-F", argv[i])) {
	    ++i;
	} else if (BU_STR_EQUAL("-W", argv[i])) {
	    sscanf(argv[++i], "%d", &scr_w);
	    scr_set=1;
	} else if (BU_STR_EQUAL("-N", argv[i])) {
	    sscanf(argv[++i], "%d", &scr_h);
	    scr_set=1;
	} else if (bu_strncmp("-", argv[i], 1)==0) {
	    printf("Unknown option: %s\n", argv[i]);
	    bu_exit(10, NULL);
/* get plot file */
	} else {
	    if ((ifd=open(argv[i], 2)) == -1) {
		perror(argv[i]);
		bu_exit(10, NULL);
	    }
	}
    }
    save_Tty(0);
    printf("Program set: February 13, 1987\n");
    printf("Input 3 characters for colors\n");
    printf("Default colors:  Items       - silver\n");
    printf("                 Transparent - cyan\n");
    printf("                 Background  - black\n\n");

/* print data on first two lines of view in plot file */

view:	printf("Title: ");
    for (i=0;i<64;i++) {
	c=gc();
	putchar(c);
    }
    printf("\nDate :");
    while ((c=gc())!='\n') putchar(c);
    printf("\nView:");
    for (i=0;i<20;i++) putchar(gc());
    grid_w=ctoi();
    grid_h=ctoi();
    printf("\nHorz, Vert: %4d %4d\n", grid_w, grid_h);
    if ((grid_w > 512 || grid_h > 512) && scr_set==0) {
	if (grid_w>1024 || grid_h>1024) {
	    printf("Number of pixels gt 1024\n");
	    bu_exit(10, NULL);
	}
	scr_w=1024;
	scr_h=1024;
	printf("High resolution set\n");
    }
/* open frame buffer */
    if ((fbp=fb_open(NULL, scr_w, scr_h))==NULL) {
	printf("No device opened\n");
	bu_exit(10, NULL);
    }
    (void)fb_wmap(fbp, COLORMAP_NULL);	/* std map */

/* compute screen coordinates of min and max */
    min_w=(scr_w-grid_w)/2;
    min_h=(scr_h-grid_h)/2;
    max_w=min_w+grid_w;
    max_h=min_h+grid_h;
    locd=loct;
/* printf("min_w %d min_h %d\n", min_w, min_h); */

/* find item - color table (default color = silver) */
    while ((c=gc())!='/') if (c==0) bu_exit(1, NULL);
    gc();
    loci=loct;
    for (ni=0;;ni++) {
	if (ni>=500) {
	    printf("Not enough room to store item colors\n");
	    bu_exit(10, NULL);
	}
	itm[ni]=ctoi();
	if (itm[ni]<0) break;
	for (i=0;i<3;i++) cs[i]=gc();
	if ((itmc[ni]=cclr(cs))<0) itmc[ni]=15;
	while (gc()!='\n');
    }
    loce=loct;
    while (1) {
	printf("Option (?=menu)? ");

	if ((c=getchar()) == EOF) break;

	switch (c) {
	    case '\n':
		continue;

	    case '?':
		printf("Options\n");
		printf(" a - Set all items of a color to another\n");
		printf(" b - Set background color\n");
		printf(" c - List available colors\n");
		printf(" l - List items & their colors\n");
		printf(" o - Opaque-transparent toggle\n");
		printf(" p - Paint picture\n");
		printf(" q - Quit\n");
		printf(" r - Set all items in a range to a color\n");
		printf(" s - Scroll target colors for changing\n");
		printf(" t - Set transparent color\n");
		printf(" v - New view\n");
		break;
	    case 'a':
		printf("Old color? ");
		ret = scanf("%3s", cs);
		if (ret != 1)
		    perror("scanf");
		iclr=cclr(cs);
		if (iclr<0) {
		    prtclr(0);
		    break;
		}
		printf("New color? ");
		ret = scanf("%3s", cs);
		if (ret != 1)
		    perror("scanf");
		jclr=cclr(cs);
		if (jclr<0) {
		    prtclr(0);
		    break;
		}
		ichg=1;
		lseek(ifd, loci, 0);
		loct=loci;
		ic=0;
		for (i=0;i<ni;i++) {
		    for (j=0;j<10;j++) gc();
		    for (k=0;(c=gc())!='\n';) strg[k++]=c;
		    strg[k]='\0';
		    if (itmc[i]!=iclr) continue;
		    printf("%5ld %-7s  %s\n", itm[i],
			   colortab[jclr].name, strg);
		    itmc[i]=jclr;
		}
		break;
	    case 'b':
		printf("%s background changed to ", colortab[ibc].name);
		ret = scanf("%3s", cs);
		if (ret != 1)
		    perror("scanf");
		ibc=cclr(cs);
		if (ibc<0) {
		    ibc=0;
		    prtclr(0);
		}
		break;
	    case 't':
		printf("%s transparent color changed to ",
		       colortab[itc].name);
		ret = scanf("%3s", cs);
		if (ret != 1)
		    perror("scanf");
		itc=cclr(cs);
		if (itc<0) {
		    prtclr(0);
		    itc=3;
		}
		break;
	    case 'c':
		prtclr(0);
		break;
	    case 'l':
		printf("Background color is %s\n", colortab[ibc].name);
		printf("Transparent color is %s\n\n", colortab[itc].name);
		lseek(ifd, loci, 0);
		loct=loci;
		ic=0;
		for (i=0;i<ni;i++) {
		    for (j=0;j<10;j++) gc();
		    printf("%5ld %-7s  ", itm[i],
			   colortab[itmc[i]].name);
		    while ((c=gc())!='\n') putchar(c);
		    putchar('\n');
		    if ((i%20)==19) {
			char cbuf[16];
			printf("(c)ontine, (s)top? ");
			ret = scanf("%1s", cbuf);
			if (ret != 1)
			    perror("scanf");
			c = cbuf[0];
			if (c=='s') break;
		    }
		}
		break;
	    case 'o':
		opq++;
		opq &= 1;
		if (opq) {
		    printf("Transparent items now opaque\n");
		} else {
		    printf("Transparent items restored\n");
		}
		break;
	    case 'p':
		paint();
		break;
	    case 'q': /* quit */
	    case 'v':
		if (ichg!=0) {
		    ssize_t writeret;
		    for (i=0;i<ni;i++) {
			loci+=6;
			lseek(ifd, loci, 0);
			ic=0;
			for (j=0, cp=colortab[itmc[i]].name;j<3;
			     cp++, j++) {
			    loci++;
			    writeret = write(ifd, cp, 1);
			    if (writeret < 0)
				perror("write");
			}
			lseek(ifd, ++loci, 0);
			while ((c=gc())!='\n') loci++;
			loci++;
		    }
		    ichg=0;
		}
		if (c == 'q') {
		    /* quit */
		    bu_exit(0, NULL);
		}
		loct=loce;
		lseek(ifd, loce, 0);
		ic=0;
		fb_close(fbp);
		goto view;
	    case 'r':
		printf("Lower limit? ");
		ret = scanf("%d", &il);
		if (ret != 1)
		    perror("scanf");
		printf("Upper limit? ");
		ret = scanf("%d", &iu);
		if (ret != 1)
		    perror("scanf");
		printf("Color? ");
		ret = scanf("%3s", cs);
		if (ret != 1)
		    perror("scanf");
		iclr=cclr(cs);
		if (iclr<0) {
		    prtclr(0);
		    break;
		}
		ichg=1;
		lseek(ifd, loci, 0);
		loct=loci;
		ic=0;
		for (i=0;i<ni;i++) {
		    for (j=0;j<10;j++) gc();
		    for (k=0;(c=gc())!='\n';) strg[k++]=c;
		    strg[k]='\0';
		    if (itm[i]<il || itm[i]>iu) continue;
		    printf("%5ld %-7s  %s\n", itm[i],
			   colortab[iclr].name, strg);
		    itmc[i]=iclr;
		}
		break;
	    case 's':
		prtsmu(0);
		ichg=1;
		lseek(ifd, loci, 0);
		loct=loci;
		ic=0;
		iskp=0;
		set_Raw(0);		/* set raw mode */
		lclr=15;
		for (i=0;i<ni;i++) {
		back:				for (j=0;j<10;j++) gc();
		    for (k=0;(c=gc())!='\n';) strg[k++]=c;
		    strg[k]='\0';
		again:				printf("      %-7s  %s%c%5ld ",
						       colortab[itmc[i]].name, strg, 13, itm[i]);
		    if (iskp>0) {
			iskp--;
			printf("\015\n");
			continue;
		    }
		    if ((k=gclr())>=0) {
			itmc[i]=k;
/* ctrl b - backup one line */
		    } else if (k==-2) {
			printf("\015\n");
			if (bsp()==0) goto again;
			if (bsp()==0) goto again;
			i--;
			goto back;
/* ctrl c - stop */
		    } else if (k==-3) {
			printf("%c\n", 13);
			break;
/* ctrl v - skip 20 lines */
		    } else if (k==-22) {
			iskp=20;
			continue;
/*space - same as last color */
		    } else if (k==-32) {
			itmc[i]=lclr;
/* ? - print menu and colors */
		    } else if (k==-63) {
			prtsmu(1);
			prtclr(1);
			goto again;
		    } else {
			bu_log("error: unrecognized key sequence.\n");
			continue;
		    }
		    printf("%c%5ld %-7s%c\n", 13, itm[i],
			   colortab[itmc[i]].name, 13);
		    lclr=itmc[i];
		}
		reset_Tty(0);
		break;
	}
    }
    return 0;
}


void
paint(void)
/* Paint picture */
{
    char c;
    int i, j, iw, ih, iwih, trnf, flop;
    int inten = 0;
    int inten_high = 0;
    long li, lj, numb(void);
    RGBpixel ocl = {0, 0, 0};
    RGBpixel tcl = {0, 0, 0};
    RGBpixel pmix = {0, 0, 0};
    RGBpixel tp = {0, 0, 0};
    RGBpixel bp = {0, 0, 0};
    unsigned char *fb_p = NULL;	/* Current position in buffer.	*/

    printf("Picture is being painted\n");
    bp[RED]=colortab[ibc].c_pixel[RED];
    bp[GRN]=colortab[ibc].c_pixel[GRN];
    bp[BLU]=colortab[ibc].c_pixel[BLU];
    tp[RED]=colortab[itc].c_pixel[RED];
    tp[GRN]=colortab[itc].c_pixel[GRN];
    tp[BLU]=colortab[itc].c_pixel[BLU];
    fb_clear(fbp, bp);
    lseek(ifd, locd, 0);
    loct=locd;
    ic=0;
    nc=0;
    trnf=0;
    inten_high=0;
    ih=max_h;
    iw=min_w;
    fb_p=pix_buf;
    iwih=iw+ih;
    iwih&=1;
    flop=1;
    while ((c=g())!='/') {
	io=c-32;
    noread:		if (io>31) {
/* ignore one of pair of intensities if trnf=4 */
	    if (flop) {
		iwih++;
		iwih&=1;
	    }
	    inten=(io&31)+inten_high;
	    if (trnf==4) {
		++flop;
		flop&=1;
		if (opq&&flop) continue;
		if (opq==0&&flop!=iwih) continue;
	    }
/* compute intensity */
	    iw++;
	    if (trnf==0||(trnf==4&&iwih&&opq==0)) {
		ocl[RED]= ((int)pmix[RED]*inten)>>8;
		ocl[GRN]= ((int)pmix[GRN]*inten)>>8;
		ocl[BLU]= ((int)pmix[BLU]*inten)>>8;
		FBWPIXEL(ocl);
	    } else if (trnf==2&&iwih&&opq==0) {
		FBWPIXEL(bp);
	    } else {
		tcl[RED]= ((int)tp[RED]*inten)>>8;
		tcl[GRN]= ((int)tp[GRN]*inten)>>8;
		tcl[BLU]= ((int)tp[BLU]*inten)>>8;
		FBWPIXEL(tcl);
	    }
/* high order intensity */
	} else if (io>15) {
	    inten_high=(io-16)<<5;
/* control character */
	} else switch (io) {
/* miss target (<sp>)*/
		case 0:
		    lj=numb();
		    for (li=0;li<lj;li++, iw++) FBWPIXEL(bp);
		    trnf=0;
		    flop=1;
		    iwih=(iw+ih)&1;
		    goto noread;
/* new surface (!)*/
		case 1:
		    break;
/* transparent (")*/
		case 2:
/* transparent outside - opaque inside ($)*/
		case 4:
		    if (io==trnf) {
			trnf=0;
		    } else {
			flop=1;
			trnf=io;
		    }
		    break;
/* opaque item (#) */
		case 3:
		    lj=numb();
		    if ((i=lookup(lj, itm, ni))<0) {
			printf("Item %ld not in table\n", lj);
			j=15;
		    } else {
			j=itmc[i];
		    }
		    pmix[RED]=colortab[j].c_pixel[RED];
		    pmix[GRN]=colortab[j].c_pixel[GRN];
		    pmix[BLU]=colortab[j].c_pixel[BLU];
		    break;
/* shadow (&) */
		case 6:
		    break;
/* repeat intensity (*) */
		case 10:
		    lj=numb();
		    if (trnf!=0) {
			ocl[RED]= ((int)pmix[RED]*inten)>>8;
			ocl[GRN]= ((int)pmix[GRN]*inten)>>8;
			ocl[BLU]= ((int)pmix[BLU]*inten)>>8;
			tcl[RED]= ((int)tp[RED]*inten)>>8;
			tcl[GRN]= ((int)tp[GRN]*inten)>>8;
			tcl[BLU]= ((int)tp[BLU]*inten)>>8;
		    }
		    for (li=0;li<lj;li++, iw++) {
			if (flop) {
			    ++iwih;
			    iwih&=1;
			}
			if (trnf==4) {
			    ++flop;
			    flop&=1;
			    if ((opq&&flop)||(flop!=iwih&&opq==0)) {
				iw--;
				continue;
			    }
			}
			if (trnf==0||(trnf==4&&iwih&&opq==0)) {
			    FBWPIXEL(ocl);
			} else if (trnf==2&&iwih&&opq==0) {
			    FBWPIXEL(bp);
			} else {
			    FBWPIXEL(tcl);
			}
		    }
		    if (io!=10) goto noread;
		    break;
/* end of line (.)*/
		case 14:
		    if (iw>min_w) {
			fb_write(fbp, min_w, ih, pix_buf, (iw-min_w));
			iw=min_w;
			fb_p=pix_buf;
		    }
		    ih--;
		    iwih=(iw+ih)&1;
		    flop=1;
	    }
    }
}
long numb(void)
/*
 * get number from packed word */
{
    long n;
    int shift;
    n=0;
    shift=0;
    while ((io=g()-32)>31) {
	n+=((long)(io&31))<<shift;
	shift += 5;
    }
    return n;
}
int cclr(char *pc)

/* compare input color to colors available */
{
    char *cp;
    int i;
    for (i=0;i<20;i++) {
	cp=colortab[i].name;
	if (*cp== *pc&&*(cp+1)== *(pc+1)&&*(cp+2)==*(pc+2)) return i;
	else if (*cp> *pc) return -1;
    }
    return -1;
}
long ctoi(void)
/* change char string to integer */
{
    long num, neg;
    char cc;
    num=0;
    neg=1;
    while ((cc=gc())==' ');
    if (cc=='-') {
	neg= -1;
	cc=gc();
    }
    while (cc>='0'&&cc<='9') {
	num=10*num+cc-'0';
	cc=gc();
    }
    return num*neg;
}
char g(void)
/* get char from plot file - check for 75 columns and discard rest */
{
    static char c;
    if ((c=gc())!='\n') {
	if ((++nc)>75) {
	    while ((c=gc())!='\n');
	    nc=1;
	    return gc();
	}
	return c;
    } else if (nc==75) {
	nc=1;
	return gc();
    } else {
	nc++;
	ib--;
	return ' ';
    }
}
int bsp(void)
/* back up a line in plot file buff */
{
    int kloct, kib;
    kloct=loct;
    kib=ib;
    while (ibuf[--ib]!='\n') {
	loct--;
	if (ib<=0) {
	    if (loct<=loci) {
		loct=loci;
		ib=0;
		return 1;
	    }
	    ib=kib;
	    loct=kloct;
	    return 0;
	}
    }
    return 1;
}
char gc(void)
/* get char from plot file buff */
{
    loct++;
    if ((++ib)>=ic) {
	ic=read(ifd, ibuf, 1024);
	ib=0;
	if (ic<=0) return 0;
    }
    if (ibuf[ib]=='>') ibuf[ib]='^';
    if (ibuf[ib]=='?') ibuf[ib]='@';
/* printf("GC: ibuf[ib], ib, ic %c %d %d \n", ibuf[ib], ib, ic);*/
/* putchar(ibuf[ib]); */
    return ibuf[ib];
}
int gclr(void)
{
    char cs[3];
    int i, c;
    for (i=0;i<3;i++) {
	while ((c=getchar())<97||c>122) {
	    if (c==2) return -2;
	    if (c==3) return -3;
	    if (c==13) return -13;
	    if (c==22) return -22;
	    if (c==32) return -32;
	    if (c=='?') return -63;
	    if (c==EOF) return EOF;
	}
	cs[i]=(char)c;
    }
    return cclr(cs);
}
int lookup(long int ix, long int *jx, int n)
{
    int i, i_a, i_b;
    i_a= -1;
    i_b=n;
    while (1) {
	i=(i_a+i_b)/2;
	/*printf("LOOKUP: ix, jx, ia, ib, i %d %d %d %d %d\n", ix, *(jx+i), i_a, i_b, i);*/
	if (ix== *(jx+i)) return i;
	if (i<=i_a) return -1;
	if (ix> *(jx+i)) i_a=i;
	else i_b=i;
    }
}
void
prtclr(char raw)
{
    int i;
    printf("Available Colors\n");
    if (raw) putchar('\015');
    for (i=0;i<20;i++) {
	printf("%-8s", colortab[i].name);
	if ((i%7)==6) {
	    if (raw) putchar('\015');
	    putchar('\n');
	}
    }
    if (raw) putchar('\015');
    putchar('\n');
    return;
}
void
prtsmu(char raw)
{
    if (raw) printf("\015\n");
    printf("Menu\n");
    if (raw) putchar('\015');
    printf("   ?  = Color list + this Menu\n");
    if (raw) putchar('\015');
    printf(" <^b> = Backup one line\n");
    if (raw) putchar('\015');
    printf(" <^c> = Quit\n");
    if (raw) putchar('\015');
    printf(" <^v> = Skip 20 lines\n");
    if (raw) putchar('\015');
    printf(" <cr> = No change in color\n");
    if (raw) putchar('\015');
    printf(" <sp> = Same color as previous item\n");
    if (raw) putchar('\015');
    printf("  ccc = 3 character color code\n\n");
    if (raw) putchar('\015');
    return;
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
