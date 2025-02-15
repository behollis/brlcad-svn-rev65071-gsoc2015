/*                       C E L L - F B . C
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
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "bu/getopt.h"
#include "bu/debug.h"
#include "fb.h"
#include "vmath.h"
#include "raytrace.h"


#define LORES 512
#define HIRES 1024
#define SNUG_FIT 1
#define LOOSE_FIT 2
#define MAX_LINE 32768
#define MAX_COLORTBL 11
#define WHITE colortbl[0]
#define BACKGROUND colortbl[MAX_COLORTBL]
#define OPT_STRING "CM:F:N:S:W:X:a:b:c:d:ef:gikl:m:p:s:v:x:h?"
#define BLEND_USING_HSV 1

#define STATE_VIEW_TOP 0
#define STATE_IN_HEADER 1
#define STATE_IN_DATA 2
#define STATE_BEYOND_DATA 3

/* Macros with arguments */
#ifndef Min
#  define Min(a, b)		((a) < (b) ? (a) : (b))
#  define Max(a, b)		((a) > (b) ? (a) : (b))
#  define MinMax(m, M, a) { m = Min(m, a); M = Max(M, a); }
#endif

/*
 * Translate between different coordinate systems at play:
 *	H, V	The units of the input file.  (from GIFT)
 *	C	The relative cell number, within the input
 *	VP	The pixel within the viewport (a sub-rectangle of the screen)
 *		Includes offsetting for the "key" area within the viewport.
 *	SCR	The pixel on the screen.  Framebuffer coordinates for LIBFB.
 *		Includes offsetting the viewport anywhere on the screen.
 */

#define H2CX(_h)	((int)(((_h) - xmin) / cell_size + 0.5))
#define V2CY(_v)	((int)(((_v) - ymin) / cell_size + 0.5))

#define CX2VPX(_cx)	(((_cx)) * (wid + grid_flag))
#define CY2VPY(_cy)	(((_cy) + key_height) * (hgt + grid_flag))

#define VPX2SCRX(_vp_x)	((_vp_x) + xorigin)
#define VPY2SCRY(_vp_y)	((_vp_y) + yorigin)

/* --- */

#define SCRX2VPX(_scr_x) ((_scr_x) - xorigin)
#define SCRY2VPY(_scr_y) ((_scr_y) - yorigin)

#define VPX2CX(_vp_x)	((_vp_x) / (wid+grid_flag))
#define VPY2CY(_vp_y)	((_vp_y) / (hgt+grid_flag) - key_height)

#define CX2H(_cx)	((_cx) * cell_size + xmin)
#define CY2V(_cy)	((_cy) * cell_size + ymin)

/* --- */

#define H2SCRX(_h) VPX2SCRX(CX2VPX(H2CX(_h)))
#define V2SCRY(_v) VPY2SCRY(CY2VPY(V2CY(_v)))

#define SCRX2H(_s_x) CX2H(VPX2CX(SCRX2VPX(_s_x)))
#define SCRY2V(_s_y) CY2V(VPY2CY(SCRY2VPY(_s_y)))


/* Debug flags */
#define CFB_DBG_MINMAX 0x01
#define CFB_DBG_GRID 0x02
#define CFB_DBG_MEM 0x010000	/* a la librt(3) */


/* Data structure definitions */
typedef int bool_t;
typedef union
{
    double v_scalar;
    RGBpixel v_color;
} cell_val;
typedef struct
{
    double c_x;
    double c_y;
    cell_val c_val;
} Cell;
struct locrec
{
    struct bu_list l;
    fastf_t h;
    fastf_t v;
};
#define LOCREC_MAGIC 0x6c637263


/* Global variables */
static char *usage[] = {
    "",
    "Usage: cell-fb [options] [file]",
    "Options:",
    " -C                Use first 3 fields as r, g, and b",
    " -M \"r g b r g b\"  Ramp between two colors",
    " -F dev            Use frame-buffer device `dev'",
    " -N n              Set frame-buffer height to `n' pixels",
    " -S n              Set frame-buffer height and width to `n' pixels",
    " -W n              Set frame-buffer width to `n' pixels",
    " -X n              Set local debug flag to hex value `n' (default is 0)",
    " -a \"h v\"          Print pixel coords of point",
    " -b n              Ignore values not equal to `n'",
    " -c n              Assume cell size of `n' user units (default is 100)",
    " -d \"m n\"          Expect input in interval [m, n] (default is [0, 1])",
    " -e                Erase frame buffer before displaying picture",
    " -f n              Display field `n' of cell data",
    " -g                Leave space between cells",
    " -i                Round values (default is to interpolate colors)",
    " -k                Display color key",
    " -l \"a e\"          Write log information to stdout",
    " -m \"n r g b\"      Map value `n' to color ``r g b''",
    " -p \"x y\"          Offset picture from bottom-left corner of display",
    " -s \"w h\"          Set cell width and height in pixels",
    " -v n              Display view number `n' (default is all views)",
    " -x n              Set LIBRT(3) debug flag to hex value `n' (default is 0)",
    0
};
static Cell *grid;

static char fbfile[MAX_LINE] = { 0 };/* Name of frame-buffer device */

static double az;			/* To dump to log file */
static double bool_val;			/* Only value displayed for -b option */
static double cell_size = 100.0;	/* Size of cell in user units */
static double el;			/* To dump to log file */
static double key_height = 0.0;		/* How many cell heights for key? */
static double xmin;			/* Extrema of coordinates */
static double ymin;			/* in user units */
static double xmax;			/* (set in read_Cell_Data())	*/
static double ymax;			/* */
static double dom_min = 0.0;		/* Extrema of data to plot */
static double dom_max = 1.0;		/* */
static double dom_cvt = 10.0;		/* To convert domain to [0, 10] */

static bool_t boolean_flag = 0;	/* Show only one value? */
static bool_t color_flag = 0;	/* Interpret fields as R, G, B? */
static bool_t erase_flag = 0;	/* Erase frame buffer first? */
static bool_t grid_flag = 0;	/* Leave space between cells? */
static bool_t interp_flag = 1;	/* Ramp between colortbl entries? */
static bool_t key_flag = 0;	/* Display color-mapping key? */
static bool_t log_flag = 0;	/* Make a log file? */

static int compute_fb_height;		/* User supplied height?  Else what? */
static int compute_fb_width;		/* User supplied width?  Else what? */
static unsigned int debug_flag = 0;	/* Control diagnostic prints */
static int fb_height = -1;		/* Height of frame buffer in pixels */
static int fb_width = -1;		/* Width of frame buffer in pixels */
static int field = 1;			/* The field that is of interest */
static int wid = 10, hgt = 10;		/* Number of pixels per cell, H & V */
static int xorigin = 0, yorigin = 0;	/* Pixel location of image low lft */
static int view_flag = 0;		/* The view that is of interest */

static long maxcells = 10000;	/* Max number of cells in the image */
static fb *fbiop = FB_NULL;	/* Frame-buffer device */
static FILE *filep;		/* Input stream */
static struct locrec gp_locs;

static RGBpixel colortbl[12] =	/* The map: value --> R, G, B */
{
    { 255, 255, 255 },		/* white */
    { 100, 100, 140 },		/* blue grey */
    {   0,   0, 255 },		/* blue */
    {   0, 120, 255 },		/* light blue */
    { 100, 200, 140 },		/* turquoise */
    {   0, 150,   0 },		/* dark green */
    {   0, 225,   0 },		/* green */
    { 255, 255,   0 },		/* yellow */
    { 255, 160,   0 },		/* tangerine */
    { 255, 100, 100 },		/* pink */
    { 255,   0,   0 },		/* red */
    {   0,   0,   0 }		/* black */
};


static long
read_Cell_Data(void)
{
    static char linebuf[MAX_LINE];
    static char *lbp = NULL;
    static char format[MAX_LINE];
    int state = STATE_VIEW_TOP;
    int i;
    Cell *gp = grid;
    int view_ct = 1;

    /*
     * First time through...
     * 1) initialize line-buffer pointer and try to fill the line buffer
     * 2) build the format for sscanf()
     */
    if (lbp == NULL) {
	lbp = linebuf;
	bu_fgets(lbp, MAX_LINE, filep);
	bu_strlcpy(format, "%lf %lf", sizeof(format));
	if (color_flag)
	    bu_strlcat(format, " %d %d %d", sizeof(format));
	else {
	    /* Skip to field of interest */
	    for (i = 1; i < field; i++)
		bu_strlcat(format, " %*lf", sizeof(format));
	    bu_strlcat(format, " %lf", sizeof(format));
	}
    }
    /* EOF encountered before we found the desired view? */
    if (feof(filep))
	return 0;

    /* Read the data */
    do {
	double x, y;
	int r, g, b;
	cell_val value;

	if (lbp[strlen(lbp) - 1] != '\n')
	    bu_exit (1, "Overlong line\n");

	/* Have we run out of room for the cells?  If so reallocate memory */
	if (gp - grid >= maxcells) {
	    long ncells = gp - grid;

	    maxcells *= 2;
	    grid = (Cell *) bu_realloc((char *) grid,
				       sizeof(Cell) * maxcells, "grid");
	    if (debug_flag & CFB_DBG_MEM)
		bu_log("cell-fb: maxcells increased to %ld\n", maxcells);
	    gp = grid + ncells;
	}
	/* Process any non-data (i.e. view-header) lines */
	while ((state != STATE_BEYOND_DATA) &&
	       ((color_flag &&
		 (sscanf(lbp, format, &x, &y, &r, &g, &b) != 5))
		|| (! color_flag &&
		    (sscanf(lbp, format, &x, &y, &value.v_scalar) != 3))))
	{
	    if (state == STATE_VIEW_TOP)
		state = STATE_IN_HEADER;
	    else if (state == STATE_IN_DATA)
		state = STATE_BEYOND_DATA;
	    if (feof(filep) || bu_fgets(lbp, MAX_LINE, filep) == NULL)
		return gp - grid;
	}
	/*
	 * At this point we know we have a line of cell data,
	 * though it might be the first line of the next view.
	 */
	if (state == STATE_BEYOND_DATA) {
	    state = STATE_VIEW_TOP;
	    if ((view_flag == 0) || (view_flag == view_ct++))
		return gp - grid;
	    else	/* Not the selected view, read the next one. */
		continue;
	} else
	    state = STATE_IN_DATA;

	/* If user has selected a view, only store values for that view. */
	if ((view_flag == 0) || (view_flag == view_ct)) {
	    MinMax(xmin, xmax, x);
	    MinMax(ymin, ymax, y);
	    if (debug_flag & CFB_DBG_MINMAX)
		bu_log("x=%g, y=%g, xmin=%g, xmax=%g, ymin=%g, ymax=%g\n",
		       x, y, xmin, xmax, ymin, ymax);
	    gp->c_x = x;
	    gp->c_y = y;
	    if (color_flag) {
		gp->c_val.v_color[RED] = r;
		gp->c_val.v_color[GRN] = g;
		gp->c_val.v_color[BLU] = b;
	    } else
		gp->c_val.v_scalar = value.v_scalar;
	    gp++;
	}
    } while (bu_fgets(lbp, MAX_LINE, filep) != NULL);
    return gp - grid;
}


static bool_t
get_OK(void)
{
    int c;
    FILE *infp;

    if ((infp = fopen("/dev/tty", "rb")) == NULL) {
	bu_log("cell-fb: Cannot open /dev/tty for reading\n");
	return 0;
    }
    bu_log("Another view follows.  Display ? [y/n](y) ");
    switch ((c = getc(infp))) {
	case '\n':
	    break;
	default:
	    while (getc(infp) != '\n')
		; /* Read until user hits <RETURN>. */
	    break;
    }
    (void) fclose(infp);
    if (c == 'n')
	return 0;
    return 1;
}


static void
init_Globs(void)
{
    xmin = INFINITY;
    ymin = INFINITY;
    xmax = -INFINITY;
    ymax = -INFINITY;
    return;
}


static void
val_To_RGB(cell_val cv, unsigned char *rgb)
{
    double val;

    if (color_flag) {
	COPYRGB(rgb, cv.v_color);
	return;
    }
    val = (cv.v_scalar - dom_min) * dom_cvt;
    if ((boolean_flag && !ZERO(cv.v_scalar - bool_val))
	|| (val < SMALL_FASTF) || (val > 10.0))
    {
	COPYRGB(rgb, BACKGROUND);
    } else if (ZERO(val)) {
	COPYRGB(rgb, WHITE);
    } else {
	int idx;
	double rem;
	double res;

	if (interp_flag) {
	    vect_t prev_hsv;
	    vect_t hsv;
	    vect_t next_hsv;

	    idx = val + 0.01; /* convert to range [0 to 10] */
	    if ((rem = val - (double) idx) < 0.0) /* remainder */
		rem = 0.0;
	    res = 1.0 - rem;
#if BLEND_USING_HSV
	    bu_rgb_to_hsv(colortbl[idx], prev_hsv);
	    bu_rgb_to_hsv(colortbl[idx+1], next_hsv);
	    VBLEND2(hsv, res, prev_hsv, rem, next_hsv);
	    bu_hsv_to_rgb(hsv, rgb);
#else
	    VBLEND2(rgb, res, colortbl[idx], rem, colortbl[idx+1]);
#endif
	} else {
	    idx = val + 0.51;
	    COPYRGB(rgb, colortbl[idx]);
	}
    }
    return;
}


static bool_t
display_Cells(long int ncells)
{
    Cell *gp, *ep = &grid[ncells];
    static int zoom;
    unsigned char *buf;
    static RGBpixel pixel;
    double lasty = -INFINITY;
    double dx, dy;
    int y_0 = 0, y_1;

    if (compute_fb_height) {
	dy = ((ymax - ymin) / cell_size + 1.0) * (hgt + grid_flag);
	if (compute_fb_height == SNUG_FIT)
	    fb_height = dy + (key_flag * 2 * hgt) + yorigin;
	else if (dy > LORES)	/* LOOSE_FIT */
	    fb_height = HIRES;
	else
	    fb_height = LORES;
    }
    if (compute_fb_width) {
	dx = ((xmax - xmin) / cell_size + 1.0) * (wid + grid_flag);
	if (compute_fb_width == SNUG_FIT)
	    fb_width = dx + xorigin;
	else if (dx > LORES)	/* LOOSE_FIT */
	    fb_width = HIRES;
	else
	    fb_width = LORES;
    }

    zoom = 1;
    fbiop = fb_open((fbfile[0] != '\0') ? fbfile : NULL, fb_width, fb_height);
    if (fbiop == FB_NULL)
	return 0;
    if (compute_fb_height || compute_fb_width) {
	bu_log("fb_size requested: %d %d\n", fb_width, fb_height);
	fb_width = fb_getwidth(fbiop);
	fb_height = fb_getheight(fbiop);
	bu_log("fb_size obtained: %d %d\n", fb_width, fb_height);
    }
    if (fb_wmap(fbiop, COLORMAP_NULL) == -1)
	bu_log("Cannot initialize color map\n");
    if (fb_zoom(fbiop, zoom, zoom) == -1)
	bu_log("Cannot set zoom <%d, %d>\n", zoom, zoom);
    if (erase_flag && fb_clear(fbiop, BACKGROUND) == -1)
	bu_log("Cannot clear frame buffer\n");

    buf = (unsigned char *) bu_malloc(sizeof(RGBpixel) * fb_width,
				      "line of frame buffer");
    if (debug_flag & CFB_DBG_MEM)
	bu_log("buf = %p... %d pixels @ %lu bytes/pixel\n",
	       (void *)buf, fb_width, sizeof(RGBpixel));

    for (gp = grid; gp < ep; gp++) {
	int x0, x1;

	/* Whenever Y changes, write out row of cells. */
	if (!ZERO(lasty - gp->c_y)) {
	    /* If first time, nothing to write out. */
	    if (!ZERO(lasty - INFINITY)) {
		if (debug_flag & CFB_DBG_GRID)
		    bu_log("%g = V2SCRY(%g)\n", V2SCRY(lasty), lasty);
		y_0 = V2SCRY(lasty);
		if (y_0 >= 0 && y_0 < fb_height) {
		    for (y_1 = y_0 + hgt; y_0 < y_1; y_0++)
			if (fb_write(fbiop, 0, y_0, buf, fb_width) == -1) {
			    bu_log("Couldn't write to <%d, %d>\n", 0, y_0);
			    (void) fb_close(fbiop);
			    return 0;
			}
		}
	    }
	    /* Clear buffer. */
	    for (x0 = 0; x0 < fb_width; x0++) {
		COPYRGB(&buf[3*x0], BACKGROUND);
	    }

	    /* Draw grid line between rows of cells. */
	    if (grid_flag && !ZERO(lasty - INFINITY)) {
		if (fb_write(fbiop, 0, y_0, buf, fb_width) == -1) {
		    bu_log("Couldn't write to <%d, %d>\n", 0, y_0);
		    (void) fb_close(fbiop);
		    return 0;
		}
		if (debug_flag & CFB_DBG_GRID)
		    bu_log("Writing grid row at %d\n", y_0);
	    }
	    lasty = gp->c_y;
	}
	val_To_RGB(gp->c_val, pixel);
	/* Be careful only to write color within bounds of the screen */
	x0 = H2SCRX(gp->c_x);
	if (x0 >= 0 && x0 <= fb_width - wid) {
	    for (x1 = x0 + wid; x0 < x1;  x0++) {
		COPYRGB(&buf[3*x0], pixel);
	    }
	}
    }

    /* Write out last row of cells. */
    if (debug_flag & CFB_DBG_GRID)
	bu_log("%g = V2SCRY(%g)\n", V2SCRY(lasty), lasty);
    for (y_0 = V2SCRY(lasty), y_1 = y_0 + hgt; y_0 < y_1;  y_0++)
	if (fb_write(fbiop, 0, y_0, buf, fb_width) == -1) {
	    bu_log("Couldn't write to <%d, %d>\n", 0, y_0);
	    (void) fb_close(fbiop);
	    return 0;
	}
    /* Draw color key. */
    if (key_flag && (fb_width < (10 + 1) * wid))
	bu_log("Width of key (%d) would exceed frame-buffer width (%d)\n",
	       (10 + 1) * wid, fb_width);
    else if (key_flag) {
	int i, j;
	double base;
	int scr_min, scr_max;
	int scr_center;	/* screen coord of center of view */
	int center_cell;	/* cell # of center of view */

	/* Clear buffer. */
	for (i = 0; i < fb_width; i++) {
	    COPYRGB(&buf[3*i], BACKGROUND);
	}
	/* Center the color key from side-to-side in the viewport.
	 * Find screen coords of min and max vals, clip to (0, fb_width).
	 * If there are fewer than 11 cells, the run the key
	 * from the left edge to beyond the right edge.
	 */
	scr_min = H2SCRX(xmin);
	scr_max = H2SCRX(xmax);
	CLAMP(scr_min, 0, fb_width);
	CLAMP(scr_max, 0, fb_width);

	scr_center = (scr_max + scr_min)/2;
	if ((center_cell = VPX2CX(SCRX2VPX(scr_center))) < 5)
	    center_cell = 5;

	/* Draw 10 cells for the color key */
	dom_cvt = 10.0;
	for (i = 0; i <= 10; i++) {
	    cell_val cv;

	    /*
	     * Determine where to start the key,
	     * being careful not to back up beyond the beginning of buf.
	     */
	    base = VPX2SCRX(CX2VPX(center_cell - 10/2 + i));

	    cv.v_scalar = i / 10.0;

	    val_To_RGB(cv, pixel);
	    for (j = 0; j < wid; j++) {
		int idx = base + j;
		COPYRGB(&buf[3*idx], pixel);
	    }
	}
	dom_cvt = 10.0 / (dom_max - dom_min);

	for (i = yorigin; i < yorigin+hgt; i++)
	    if (fb_write(fbiop, 0, i, buf, fb_width) == -1) {
		bu_log("Couldn't write to <%d, %d>\n", 0, i);
		(void) fb_close(fbiop);
		return 0;
	    }
    }
    (void) fb_close(fbiop);

    bu_free((char *) buf, "line of frame buffer");
    if (debug_flag & CFB_DBG_MEM)
	bu_log("freed buf, which is now %p\n", (void *)buf);
    return 1;
}


static struct locrec *
mk_locrec(fastf_t h, fastf_t v)
{
    struct locrec *lrp;

    BU_ALLOC(lrp, struct locrec);
    lrp->l.magic = LOCREC_MAGIC;
    lrp->h = h;
    lrp->v = v;
    return lrp;
}


static void
fill_colortbl(unsigned char *lo_rgb, unsigned char *hi_rgb)
{
    int i;
    double a, b;

#if BLEND_USING_HSV

    fastf_t lo_hsv[3], hi_hsv[3];
    fastf_t hsv[3];

    bu_rgb_to_hsv(lo_rgb, lo_hsv);
    bu_rgb_to_hsv(hi_rgb, hi_hsv);
#endif

    for (i = 0; i < MAX_COLORTBL; ++i) {
	b = ((double) i) / (MAX_COLORTBL - 1);
	a = 1.0 - b;
#if BLEND_USING_HSV
	VBLEND2(hsv, a, lo_hsv, b, hi_hsv);
	bu_hsv_to_rgb(hsv, colortbl[i]);
#else
	VBLEND2(colortbl[i], a, lo_rgb, b, hi_rgb);
#endif
    }
}


static bool_t
pars_Argv(int argc, char **argv)
{
    int c;

    /* Parse options. */
    while ((c = bu_getopt(argc, argv, OPT_STRING)) != -1) {
	switch (c) {
	    case 'C':
		color_flag = 1;
		break;
	    case 'M': {
		RGBpixel lo_rgb, hi_rgb;
		int lo_red, lo_grn, lo_blu;
		int hi_red, hi_grn, hi_blu;

		if (sscanf(bu_optarg, "%d %d %d %d %d %d",
			   &lo_red, &lo_grn, &lo_blu,
			   &hi_red, &hi_grn, &hi_blu)
		    < 3) {
		    bu_log("cell-fb: Invalid color-mapping: '%s'\n",
			   bu_optarg);
		    return 0;
		}
		lo_rgb[RED] = lo_red;
		lo_rgb[GRN] = lo_grn;
		lo_rgb[BLU] = lo_blu;
		hi_rgb[RED] = hi_red;
		hi_rgb[GRN] = hi_grn;
		hi_rgb[BLU] = hi_blu;
		fill_colortbl(lo_rgb, hi_rgb);
		break;
	    }
	    case 'F':
		(void) bu_strlcpy(fbfile, bu_optarg, sizeof(fbfile));
		break;
	    case 'N':
		if (sscanf(bu_optarg, "%d", &fb_height) < 1) {
		    bu_log("cell-fb: Invalid frame-buffer height: '%s'\n", bu_optarg);
		    return 0;
		}
		if (fb_height < -1) {
		    bu_log("cell-fb: Frame-buffer height out of range: %d\n", fb_height);
		    return 0;
		}
		break;
	    case 'W':
		if (sscanf(bu_optarg, "%d", &fb_width) < 1) {
		    bu_log("cell-fb: Invalid frame-buffer width: '%s'\n", bu_optarg);
		    return 0;
		}
		if (fb_width < -1) {
		    bu_log("cell-fb: Frame-buffer width out of range: %d\n", fb_width);
		    return 0;
		}
		break;
	    case 'S':
		if (sscanf(bu_optarg, "%d", &fb_height) < 1) {
		    bu_log("cell-fb: Invalid frame-buffer dimension: '%s'\n", bu_optarg);
		    return 0;
		}
		if (fb_height < -1) {
		    bu_log("cell-fb: Frame-buffer dimensions out of range: %d\n",
			   fb_height);
		    return 0;
		}
		fb_width = fb_height;
		break;
	    case 'X':
		if (sscanf(bu_optarg, "%x", &debug_flag) < 1) {
		    bu_log("cell-fb: Invalid debug flag: '%s'\n", bu_optarg);
		    return 0;
		}
		break;
	    case 'a': {
		fastf_t h;
		fastf_t v;
		double scan[2];
		struct locrec *lrp;

		if (sscanf(bu_optarg, "%lf %lf", &scan[0], &scan[1]) != 2) {
		    bu_log("cell-fb: Invalid grid-plane location: '%s'\n", bu_optarg);
		    return 0;
		}
		/* double to fastf_t */
		h = scan[0];
		v = scan[1];
		lrp = mk_locrec(h, v);
		BU_LIST_INSERT(&(gp_locs.l), &(lrp->l));
	    }
		break;
	    case 'b':
		if (sscanf(bu_optarg, "%lf", &bool_val) != 1) {
		    bu_log("cell-fb: Invalid boolean value: '%s'\n", bu_optarg);
		    return 0;
		}
		boolean_flag = 1;
		break;
	    case 'c':
		if (sscanf(bu_optarg, "%lf", &cell_size) != 1) {
		    bu_log("cell-fb: Invalid cell size: '%s'\n", bu_optarg);
		    return 0;
		}
		if (cell_size <= 0) {
		    bu_log("cell-fb: Cell size out of range: %lf\n", cell_size);
		    return 0;
		}
		break;
	    case 'd':
		if (sscanf(bu_optarg, "%lf %lf", &dom_min, &dom_max) < 2) {
		    bu_log("cell-fb: Invalid domain for input: '%s'\n", bu_optarg);
		    return 0;
		}
		if (dom_min >= dom_max) {
		    bu_log("Bad domain for input: [%lf, %lf]\n",
			   dom_min, dom_max);
		    return 0;
		}
		dom_cvt = 10.0 / (dom_max - dom_min);
		break;
	    case 'e':
		erase_flag = 1;
		break;
	    case 'f':
		if (sscanf(bu_optarg, "%d", &field) != 1) {
		    bu_log("cell-fb: Invalid field: '%s'\n", bu_optarg);
		    return 0;
		}
		break;
	    case 'g':
		grid_flag = 1;
		break;
	    case 'i':
		interp_flag = 0;
		break;
	    case 'k':
		key_flag = 1;
		key_height = 2.5;
		break;
	    case 'l':
		if (sscanf(bu_optarg, "%lf%lf", &az, &el) != 2) {
		    bu_log("cell-fb: Invalid view: '%s'\n", bu_optarg);
		    return 0;
		}
		log_flag = 1;
		if (view_flag == 0)
		    view_flag = 1;
		break;
	    case 'm': {
		double value;
		RGBpixel rgb;
		int red, grn, blu;
		int idx;

		if (sscanf(bu_optarg, "%lf %d %d %d", &value, &red, &grn, &blu)
		    < 4) {
		    bu_log("cell-fb: Invalid color-mapping: '%s'\n", bu_optarg);
		    return 0;
		}
		value *= 10.0;
		idx = value + 0.01;
		if (idx < 0 || idx > MAX_COLORTBL) {
		    bu_log("Value out of range (%s)\n", bu_optarg);
		    return 0;
		}
		rgb[RED] = red;
		rgb[GRN] = grn;
		rgb[BLU] = blu;
		COPYRGB(colortbl[idx], rgb);
		break;
	    }
	    case 'p':
		switch (sscanf(bu_optarg, "%d %d", &xorigin, &yorigin)) {
		    case 2: break;
		    case 1: yorigin = xorigin; break;
		    default:
			bu_log("cell-fb: Invalid offset: '%s'\n", bu_optarg);
			return 0;
		}
		break;
	    case 's':
		switch (sscanf(bu_optarg, "%d %d", &wid, &hgt)) {
		    case 2: break;
		    case 1: hgt = wid; break;
		    default:
			bu_log("cell-fb: Invalid cell scale: '%s'\n", bu_optarg);
			return 0;
		}
		break;
	    case 'v':
		if (sscanf(bu_optarg, "%d", &view_flag) < 1) {
		    bu_log("cell-fb: Invalid view number: '%s'\n", bu_optarg);
		    return 0;
		}
		if (view_flag == 0)
		    log_flag = 0;
		break;
	    case 'x':
		if (sscanf(bu_optarg, "%x", (unsigned int *)&bu_debug) < 1) {
		    bu_log("cell-fb: Invalid debug flag: '%s'\n", bu_optarg);
		    return 0;
		}
		break;
	    default:
		return 0;
	}
    }

    if (argc == bu_optind + 1) {
	if ((filep = fopen(argv[bu_optind], "rb")) == NULL) {
	    bu_log("Cannot open file '%s'\n", argv[bu_optind]);
	    return 0;
	}
    } else if (argc != bu_optind) {
	bu_log("Too many arguments!\n");
	return 0;
    } else
	filep = stdin;

    /* if fb_height/width has not been set, do snug fit
     * else if fb_height/width set to 0 force loose fit
     * else take user specified dimensions
     */
    compute_fb_height = (fb_height == -1) ? SNUG_FIT :
	(fb_height == 0) ? LOOSE_FIT : 0;
    compute_fb_width = (fb_width == -1) ? SNUG_FIT :
	(fb_width == 0) ? LOOSE_FIT : 0;
    return 1;
}


static void
prnt_Usage(void)
{
    char **p = usage;

    while (*p)
	bu_log("%s\n", *p++);
    return;
}


static void
log_Run(void)
{
    time_t clock_time;
    mat_t model2hv;		/* model to h, v matrix */
    mat_t hv2model;		/* h, v tp model matrix */
    quat_t orient;			/* orientation */
    point_t hv_eye;			/* eye position in h, v coords */
    point_t m_eye;			/* eye position in model coords */
    fastf_t hv_viewsize;		/* size of view in h, v coords */
    fastf_t m_viewsize;		/* size of view in model coords. */

    /* Current date and time get printed in header comment */
    (void) time(&clock_time);

    (void) printf("# Log information produced by cell-fb %s\n",
		  ctime(&clock_time));
    (void) printf("az_el: %f %f\n", az, el);
    (void) printf("view_extrema: %f %f %f %f\n",
		  SCRX2H(0), SCRX2H(fb_width), SCRY2V(0), SCRY2V(fb_height));
    (void) printf("fb_size: %d %d\n", fb_width, fb_height);

    /* Produce the orientation, the model eye_pos, and the model
     * view size for input into rtregis.
     * First use the azimuth and elevation to produce the model2hv
     * matrix and use that to find the orientation.
     */

    MAT_IDN(model2hv);
    MAT_IDN(hv2model);

    /* Print out the "view" just to keep rtregis from belly-aching */

    printf("View: %g azimuth, %g elevation\n", az, el);

    /** mat_ae(model2hv, az, el); **/
    /* Formula from rt/do.c */
    bn_mat_angles(model2hv, 270.0+el, 0.0, 270.0-az);
    model2hv[15] = 25.4;		/* input is in inches */
    bn_mat_inv(hv2model, model2hv);

    quat_mat2quat(orient, model2hv);

    printf("Orientation: %.6f, %.6f, %.6f, %.6f\n", V4ARGS(orient));

    /* Now find the eye position in h, v space.  Note that the eye
     * is located at the center of the image; in this case, the center
     * of the screen space, i.e., the framebuffer.)
     * Also find the hv_viewsize at this time.
     */
    hv_viewsize = SCRX2H((double)fb_width) - SCRX2H(0.0);
    hv_eye[0] = SCRX2H((double)fb_width/2);
    hv_eye[1] = SCRY2V((double)fb_height/2);
    hv_eye[2] = hv_viewsize/2;

    /* Debugging */
    printf("hv_viewsize= %g\n", hv_viewsize);
    printf("hv_eye= %.6f, %.6f, %.6f\n", V3ARGS(hv_eye));

    /* Now find the model eye_position and report on that */
    MAT4X3PNT(m_eye, hv2model, hv_eye);
    printf("Eye_pos: %.6f, %.6f, %.6f\n", V3ARGS(m_eye));

    /*
     * Find the view size in model coordinates and print that as well.
     * Important:  Don't use %g format, it may round to nearest integer!
     */
    m_viewsize = hv_viewsize/hv2model[15];
    printf("Size: %.6f\n", m_viewsize);
}


int
main(int argc, char **argv)
{
    static long ncells;

    bu_debug = BU_DEBUG_MEM_CHECK | BU_DEBUG_MEM_LOG;
    bu_debug = 0;

    BU_LIST_INIT(&(gp_locs.l));
    if (! pars_Argv(argc, argv)) {
	prnt_Usage();
	return 1;
    }
    grid = (Cell *) bu_malloc(sizeof(Cell) * maxcells, "grid");
    if (debug_flag & CFB_DBG_MEM)
	bu_log("grid = %p... %ld cells @ %lu bytes/cell\n",
	       (void *)grid, maxcells, sizeof(Cell));
    do {
	struct locrec *lrp;

	init_Globs();
	if ((ncells = read_Cell_Data()) == 0) {
	    bu_log("cell-fb: failed to read view\n");
	    return 1;
	}
	if (BU_LIST_NON_EMPTY(&(gp_locs.l))) {
	    while (BU_LIST_WHILE(lrp, locrec, (&(gp_locs.l)))) {
		BU_LIST_DEQUEUE(&(lrp->l));
		bu_log("%g %g	%d %d\n", lrp->h, lrp->v,
		       (int) H2SCRX(lrp->h), (int) V2SCRY(lrp->v));
		bu_free((char *) lrp, "location record");
	    }
	} else {
	    bu_log("Displaying %ld cells\n", ncells);
	    if (! display_Cells(ncells)) {
		bu_log("cell-fb: failed to display %ld cells\n", ncells);
		return 1;
	    }
	    if (log_flag)
		log_Run();
	}
    } while ((view_flag == 0) && ! feof(filep) && get_OK());

    return 0;
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
