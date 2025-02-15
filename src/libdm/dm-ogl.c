/*                        D M - O G L . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libdm/dm-ogl.c
 *
 * An X11 OpenGL Display Manager.
 *
 */

#include "common.h"

#ifdef DM_OGL

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_X11_XOSDEFS_H
#  include <X11/Xfuncproto.h>
#  include <X11/Xosdefs.h>
#endif
#ifdef linux
#  undef X_NOT_STDC_ENV
#  undef X_NOT_POSIX
#endif

#include <X11/extensions/XInput.h>

/* glx.h on Mac OS X (and perhaps elsewhere) defines a slew of
 * parameter names that shadow system symbols.  protect the system
 * symbols by redefining the parameters prior to header inclusion.
 */
#define j1 J1
#define y1 Y1
#define read rd
#define index idx
#define access acs
#define remainder rem
#ifdef HAVE_GL_GLX_H
#  include <GL/glx.h>
#endif
#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif
#undef remainder
#undef access
#undef index
#undef read
#undef y1
#undef j1

#include "tk.h"

#undef VMIN		/* is used in vmath.h, too */

#include "vmath.h"
#include "bn.h"
#include "dm.h"
#include "dm-ogl.h"
#include "dm-Null.h"
#include "dm/dm_xvars.h"
#include "fb.h"
#include "fb/fb_ogl.h"
#include "rt/solid.h"

#include "./dm_private.h"

#define VIEWFACTOR      (1.0/(*dmp->dm_vp))
#define VIEWSIZE        (2.0*(*dmp->dm_vp))

/* these are from /usr/include/gl.h could be device dependent */
#define XMAXSCREEN	1279
#define YMAXSCREEN	1023
#define YSTEREO		491	/* subfield height, in scanlines */
#define YOFFSET_LEFT	532	/* YSTEREO + YBLANK ? */

#define Ogl_MV_O(_m) offsetof(struct modifiable_ogl_vars, _m)

struct modifiable_ogl_vars {
    dm *this_dm;
    int cueing_on;
    int zclipping_on;
    int zbuffer_on;
    int lighting_on;
    int transparency_on;
    int fastfog;
    double fogdensity;
    int zbuf;
    int rgb;
    int doublebuffer;
    int depth;
    int debug;
    struct bu_vls log;
    double bound;
    int boundFlag;
};


HIDDEN XVisualInfo *ogl_choose_visual(struct dm_internal *dmp, Tk_Window tkwin);

/* Display Manager package interface */
#define IRBOUND 4095.9	/* Max magnification in Rot matrix */
#define PLOTBOUND 1000.0	/* Max magnification in Rot matrix */

struct dm_internal *ogl_open(Tcl_Interp *interp, int argc, char **argv);
HIDDEN int ogl_close(struct dm_internal *dmp);
HIDDEN int ogl_drawBegin(struct dm_internal *dmp);
HIDDEN int ogl_drawEnd(struct dm_internal *dmp);
HIDDEN int ogl_normal(struct dm_internal *dmp);
HIDDEN int ogl_loadMatrix(struct dm_internal *dmp, fastf_t *mat, int which_eye);
HIDDEN int ogl_loadPMatrix(struct dm_internal *dmp, fastf_t *mat);
HIDDEN int ogl_drawString2D(struct dm_internal *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect);
HIDDEN int ogl_drawLine2D(struct dm_internal *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2);
HIDDEN int ogl_drawLine3D(struct dm_internal *dmp, point_t pt1, point_t pt2);
HIDDEN int ogl_drawLines3D(struct dm_internal *dmp, int npoints, point_t *points, int sflag);
HIDDEN int ogl_drawPoint2D(struct dm_internal *dmp, fastf_t x, fastf_t y);
HIDDEN int ogl_drawPoint3D(struct dm_internal *dmp, point_t point);
HIDDEN int ogl_drawPoints3D(struct dm_internal *dmp, int npoints, point_t *points);
HIDDEN int ogl_drawVList(struct dm_internal *dmp, register struct bn_vlist *vp);
HIDDEN int ogl_drawVListHiddenLine(struct dm_internal *dmp, register struct bn_vlist *vp);
HIDDEN int ogl_draw(struct dm_internal *dmp, struct bn_vlist *(*callback_function)(void *), void **data);
HIDDEN int ogl_setFGColor(struct dm_internal *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);
HIDDEN int ogl_setBGColor(struct dm_internal *dmp, unsigned char r, unsigned char g, unsigned char b);
HIDDEN int ogl_setLineAttr(struct dm_internal *dmp, int width, int style);
HIDDEN int ogl_configureWin_guts(struct dm_internal *dmp, int force);
HIDDEN int ogl_configureWin(struct dm_internal *dmp, int force);
HIDDEN int ogl_setLight(struct dm_internal *dmp, int lighting_on);
HIDDEN int ogl_setTransparency(struct dm_internal *dmp, int transparency_on);
HIDDEN int ogl_setDepthMask(struct dm_internal *dmp, int depthMask_on);
HIDDEN int ogl_setZBuffer(struct dm_internal *dmp, int zbuffer_on);
HIDDEN int ogl_setWinBounds(struct dm_internal *dmp, fastf_t *w);
HIDDEN int ogl_debug(struct dm_internal *dmp, int vl);
HIDDEN int ogl_logfile(struct dm_internal *dmp, const char *filename);
HIDDEN int ogl_beginDList(struct dm_internal *dmp, unsigned int list);
HIDDEN int ogl_endDList(struct dm_internal *dmp);
HIDDEN void ogl_drawDList(unsigned int list);
HIDDEN int ogl_freeDLists(struct dm_internal *dmp, unsigned int list, int range);
HIDDEN int ogl_genDLists(struct dm_internal *dmp, size_t range);
HIDDEN int ogl_getDisplayImage(struct dm_internal *dmp, unsigned char **image);
HIDDEN void ogl_reshape(struct dm_internal *dmp, int width, int height);
HIDDEN int ogl_makeCurrent(struct dm_internal *dmp);


static fastf_t default_viewscale = 1000.0;
static double xlim_view = 1.0;	/* args for glOrtho*/
static double ylim_view = 1.0;

/* lighting parameters */
static float amb_three[] = {0.3, 0.3, 0.3, 1.0};

static float light0_position[] = {0.0, 0.0, 1.0, 0.0};
static float light0_diffuse[] = {1.0, 1.0, 1.0, 1.0}; /* white */
static float wireColor[4];
static float ambientColor[4];
static float specularColor[4];
static float diffuseColor[4];
static float backDiffuseColorDark[4];
static float backDiffuseColorLight[4];

HIDDEN void
ogl_printmat(struct bu_vls *tmp_vls, fastf_t *mat) {
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[0], mat[4], mat[8], mat[12]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[1], mat[5], mat[9], mat[13]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[2], mat[6], mat[10], mat[14]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[3], mat[7], mat[11], mat[15]);
}

HIDDEN void
ogl_printglmat(struct bu_vls *tmp_vls, GLfloat *m) {
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", m[0], m[4], m[8], m[12]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", m[1], m[5], m[9], m[13]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", m[2], m[6], m[10], m[14]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", m[3], m[7], m[11], m[15]);
}


void
ogl_fogHint(struct dm_internal *dmp, int fastfog)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)dmp->m_vars;
    mvars->fastfog = fastfog;
    glHint(GL_FOG_HINT, fastfog ? GL_FASTEST : GL_NICEST);
}


HIDDEN int
ogl_setBGColor(struct dm_internal *dmp, unsigned char r, unsigned char g, unsigned char b)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)dmp->m_vars;
    if (dmp->dm_debugLevel == 1)
	bu_log("ogl_setBGColor()\n");

    dmp->dm_bg[0] = r;
    dmp->dm_bg[1] = g;
    dmp->dm_bg[2] = b;

    ((struct ogl_vars *)dmp->dm_vars.priv_vars)->r = r / 255.0;
    ((struct ogl_vars *)dmp->dm_vars.priv_vars)->g = g / 255.0;
    ((struct ogl_vars *)dmp->dm_vars.priv_vars)->b = b / 255.0;

    if (mvars->doublebuffer) {
	glXSwapBuffers(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		       ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win);
	glClearColor(((struct ogl_vars *)dmp->dm_vars.priv_vars)->r,
		     ((struct ogl_vars *)dmp->dm_vars.priv_vars)->g,
		     ((struct ogl_vars *)dmp->dm_vars.priv_vars)->b,
		     0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    return TCL_OK;
}


/*
 * Either initially, or on resize/reshape of the window,
 * sense the actual size of the window, and perform any
 * other initializations of the window configuration.
 *
 * also change font size if necessary
 */
HIDDEN int
ogl_configureWin_guts(struct dm_internal *dmp, int force)
{
    XWindowAttributes xwa;
    XFontStruct *newfontstruct;

    if (dmp->dm_debugLevel)
	bu_log("ogl_configureWin_guts()\n");

    XGetWindowAttributes(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			 ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win, &xwa);

    /* nothing to do */
    if (!force &&
	dmp->dm_height == xwa.height &&
	dmp->dm_width == xwa.width)
	return TCL_OK;

    ogl_reshape(dmp, xwa.width, xwa.height);

    /* First time through, load a font or quit */
    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct == NULL) {
	if ((((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct =
	     XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			    FONT9)) == NULL) {
	    /* Try hardcoded backup font */
	    if ((((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct =
		 XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				FONTBACK)) == NULL) {
		bu_log("ogl_configureWin_guts: Can't open font '%s' or '%s'\n", FONT9, FONTBACK);
		return TCL_ERROR;
	    }
	}
	glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
		    0, 127, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
    }

    if (DM_VALID_FONT_SIZE(dmp->dm_fontsize)) {
	if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != dmp->dm_fontsize) {
	    if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						DM_FONT_SIZE_TO_NAME(dmp->dm_fontsize))) != NULL) {
		XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
			    0, 127, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
	    }
	}
    } else {
	/* Always try to choose a the font that best fits the window size.
	 */

	if (dmp->dm_width < 582) {
	    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 5) {
		if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						    FONT5)) != NULL) {
		    XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		    glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
				0, 127, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
		}
	    }
	} else if (dmp->dm_width < 679) {
	    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 6) {
		if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						    FONT6)) != NULL) {
		    XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		    glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
				0, 127, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
		}
	    }
	} else if (dmp->dm_width < 776) {
	    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 7) {
		if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						    FONT7)) != NULL) {
		    XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		    glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
				0, 127, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
		}
	    }
	} else if (dmp->dm_width < 873) {
	    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 8) {
		if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						    FONT8)) != NULL) {
		    XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		    glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
				0, 127, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
		}
	    }
	} else if (dmp->dm_width < 1455) {
	    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 9) {
		if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						    FONT9)) != NULL) {
		    XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		    glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
				0, 127, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
		}
	    }
	} else if (dmp->dm_width < 2037) {
	    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 10) {
		if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						    FONT10)) != NULL) {
		    XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		    glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
				0, 127, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
		}
	    }
	} else {
	    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 12) {
		if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						    FONT12)) != NULL) {
		    XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		    glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
				0, 127, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
		}
	    }
	}
    }

    return TCL_OK;
}


HIDDEN void
ogl_reshape(struct dm_internal *dmp, int width, int height)
{
    GLint mm;

    dmp->dm_height = height;
    dmp->dm_width = width;
    dmp->dm_aspect = (fastf_t)dmp->dm_width / (fastf_t)dmp->dm_height;

    if (dmp->dm_debugLevel) {
	GLfloat m[16];
	bu_log("ogl_reshape()\n");
	bu_log("width = %d, height = %d\n", dmp->dm_width, dmp->dm_height);
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	glGetFloatv (GL_PROJECTION_MATRIX, m);
    }

    glViewport(0, 0, dmp->dm_width, dmp->dm_height);

    glClearColor(((struct ogl_vars *)dmp->dm_vars.priv_vars)->r,
		 ((struct ogl_vars *)dmp->dm_vars.priv_vars)->g,
		 ((struct ogl_vars *)dmp->dm_vars.priv_vars)->b,
		 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->dm_clipmin[2], dmp->dm_clipmax[2]);
    glMatrixMode(mm);
}


HIDDEN int
ogl_makeCurrent(struct dm_internal *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("ogl_makeCurrent()\n");

    if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("ogl_makeCurrent: Couldn't make context current\n");
	return TCL_ERROR;
    }

    return TCL_OK;
}


HIDDEN int
ogl_configureWin(struct dm_internal *dmp, int force)
{
    if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("ogl_configureWin: Couldn't make context current\n");
	return TCL_ERROR;
    }

    return ogl_configureWin_guts(dmp, force);
}


HIDDEN int
ogl_setLight(struct dm_internal *dmp, int lighting_on)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)dmp->m_vars;
    if (dmp->dm_debugLevel)
	bu_log("ogl_setLight()\n");

    dmp->dm_light = lighting_on;
    mvars->lighting_on = dmp->dm_light;

    if (!dmp->dm_light) {
	/* Turn it off */
	glDisable(GL_LIGHTING);
    } else {
	/* Turn it on */

	if (1 < dmp->dm_light)
	    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
	else
	    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb_three);
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);

	glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light0_diffuse);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
    }

    return TCL_OK;
}


/**
 * currently, get a double buffered rgba visual that works with Tk and
 * OpenGL
 */
HIDDEN XVisualInfo *
ogl_choose_visual(struct dm_internal *dmp, Tk_Window tkwin)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)dmp->m_vars;
    XVisualInfo *vip, vitemp, *vibase, *maxvip;
    int tries, baddepth;
    int num, i, j;
    int fail;
    int *good = NULL;

    /* requirements */
    int screen;
    int use;
    int rgba;
    int dbfr;

    /* desires */
    int m_zbuffer = 1; /* m_zbuffer - try to get zbuffer */
    int zbuffer;

    int m_stereo; /* m_stereo - try to get stereo */
    int stereo;

    if (dmp->dm_stereo) {
	m_stereo = 1;
    } else {
	m_stereo = 0;
    }

    memset((void *)&vitemp, 0, sizeof(XVisualInfo));
    /* Try to satisfy the above desires with a color visual of the
     * greatest depth */

    vibase = XGetVisualInfo(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			    0, &vitemp, &num);
    screen = DefaultScreen(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy);

    good = (int *)bu_malloc(sizeof(int)*num, "alloc good visuals");

    while (1) {
	for (i=0, j=0, vip=vibase; i<num; i++, vip++) {
	    /* requirements */
	    if (vip->screen != screen)
		continue;

	    fail = glXGetConfig(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				vip, GLX_USE_GL, &use);
	    if (fail || !use)
		continue;

	    fail = glXGetConfig(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				vip, GLX_RGBA, &rgba);
	    if (fail || !rgba)
		continue;

	    fail = glXGetConfig(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				vip, GLX_DOUBLEBUFFER, &dbfr);
	    if (fail || !dbfr)
		continue;

	    /* desires */
	    if (m_zbuffer) {
		fail = glXGetConfig(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				    vip, GLX_DEPTH_SIZE, &zbuffer);
		if (fail || !zbuffer)
		    continue;
	    }

	    if (m_stereo) {
		fail = glXGetConfig(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				    vip, GLX_STEREO, &stereo);
		if (fail || !stereo) {
		    bu_log("ogl_choose_visual: failed visual - GLX_STEREO\n");
		    continue;
		}
	    }

	    /* this visual meets criteria */
	    good[j++] = i;
	}

	/* j = number of acceptable visuals under consideration */
	if (j >= 1) {
	    baddepth = 1000;
	    for (tries = 0; tries < j; ++tries) {
		maxvip = vibase + good[0];
		for (i=1; i<j; i++) {
		    vip = vibase + good[i];
		    if ((vip->depth > maxvip->depth)&&(vip->depth < baddepth)) {
			maxvip = vip;
		    }
		}

		((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap =
		    XCreateColormap(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				    RootWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
					       maxvip->screen), maxvip->visual, AllocNone);

		if (Tk_SetWindowVisual(tkwin,
				       maxvip->visual, maxvip->depth,
				       ((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap)) {

		    glXGetConfig(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				 maxvip, GLX_DEPTH_SIZE,
				 &mvars->depth);
		    if (mvars->depth > 0)
			mvars->zbuf = 1;

		    bu_free(good, "dealloc good visuals");
		    return maxvip; /* success */
		} else {
		    /* retry with lesser depth */
		    baddepth = maxvip->depth;
		    XFreeColormap(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap);
		}
	    }
	}

	/* if no success at this point, relax a desire and try again */

	if (m_stereo) {
	    m_stereo = 0;
	    bu_log("Stereo not available.\n");
	    continue;
	}

	if (m_zbuffer) {
	    m_zbuffer = 0;
	    continue;
	}

	/* ran out of visuals, give up */
	break;
    }

    bu_free(good, "dealloc good visuals");
    return (XVisualInfo *)NULL; /* failure */
}


/*
 * Gracefully release the display.
 */
HIDDEN int
ogl_close(struct dm_internal *dmp)
{
    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy) {
	if (((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc) {
	    glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy, None, NULL);
	    glXDestroyContext(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			      ((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc);
	}

	if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap)
	    XFreeColormap(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap);

	if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin)
	    Tk_DestroyWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);
    }

    bu_vls_free(&dmp->dm_pathName);
    bu_vls_free(&dmp->dm_tkName);
    bu_vls_free(&dmp->dm_dName);
    bu_free(dmp->dm_vars.priv_vars, "ogl_close: ogl_vars");
    bu_free(dmp->dm_vars.pub_vars, "ogl_close: dm_xvars");
    bu_free(dmp, "ogl_close: dmp");

    return TCL_OK;
}


/*
 * Fire up the display manager, and the display processor.
 *
 */
struct dm_internal *
ogl_open(Tcl_Interp *interp, int argc, char **argv)
{
    static int count = 0;
    GLfloat backgnd[4];
    int j, k;
    int make_square = -1;
    int ndevices;
    int nclass = 0;
    int unused;
    XDeviceInfoPtr olist = NULL, list = NULL;
    XDevice *dev = NULL;
    XEventClass e_class[15];
    XInputClassInfo *cip;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_vls init_proc_vls = BU_VLS_INIT_ZERO;
    Display *tmp_dpy = (Display *)NULL;
    struct dm_internal *dmp = (struct dm_internal *)NULL;
    struct modifiable_ogl_vars *mvars = NULL;
    Tk_Window tkwin = (Tk_Window)NULL;
    int screen_number = -1;

    struct dm_xvars *pubvars = NULL;
    struct ogl_vars *privvars = NULL;

    if ((tkwin = Tk_MainWindow(interp)) == NULL) {
	return DM_NULL;
    }

    BU_GET(dmp, struct dm_internal);

    *dmp = dm_ogl; /* struct copy */
    dmp->dm_interp = interp;
    dmp->dm_lineWidth = 1;
    dmp->dm_light = 1;
    dmp->dm_bytes_per_pixel = sizeof(GLuint);
    dmp->dm_bits_per_channel = 8;
    bu_vls_init(&(dmp->dm_log));

    BU_ALLOC(dmp->dm_vars.pub_vars, struct dm_xvars);
    if (dmp->dm_vars.pub_vars == (void *)NULL) {
	bu_free(dmp, "ogl_open: dmp");
	return DM_NULL;
    }
    pubvars = (struct dm_xvars *)dmp->dm_vars.pub_vars;

    BU_ALLOC(dmp->dm_vars.priv_vars, struct ogl_vars);
    if (dmp->dm_vars.priv_vars == (void *)NULL) {
	bu_free(dmp->dm_vars.pub_vars, "ogl_open: dmp->dm_vars.pub_vars");
	bu_free(dmp, "ogl_open: dmp");
	return DM_NULL;
    }
    privvars = (struct ogl_vars *)dmp->dm_vars.priv_vars;

    dmp->dm_get_internal(dmp);
    mvars = (struct modifiable_ogl_vars *)dmp->m_vars;

    dmp->dm_vp = &default_viewscale;

    bu_vls_init(&dmp->dm_pathName);
    bu_vls_init(&dmp->dm_tkName);
    bu_vls_init(&dmp->dm_dName);

    dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

    if (bu_vls_strlen(&dmp->dm_pathName) == 0)
	bu_vls_printf(&dmp->dm_pathName, ".dm_ogl%d", count);
    ++count;
    if (bu_vls_strlen(&dmp->dm_dName) == 0) {
	char *dp;

	dp = getenv("DISPLAY");
	if (dp)
	    bu_vls_strcpy(&dmp->dm_dName, dp);
	else
	    bu_vls_strcpy(&dmp->dm_dName, ":0.0");
    }
    if (bu_vls_strlen(&init_proc_vls) == 0)
	bu_vls_strcpy(&init_proc_vls, "bind_dm");

    /* initialize dm specific variables */
    pubvars->devmotionnotify = LASTEvent;
    pubvars->devbuttonpress = LASTEvent;
    pubvars->devbuttonrelease = LASTEvent;
    dmp->dm_aspect = 1.0;

    /* initialize modifiable variables */
    mvars->rgb = 1;
    mvars->doublebuffer = 1;
    mvars->fastfog = 1;
    mvars->fogdensity = 1.0;
    mvars->lighting_on = dmp->dm_light;
    mvars->zbuffer_on = dmp->dm_zbuffer;
    mvars->zclipping_on = dmp->dm_zclip;
    mvars->debug = dmp->dm_debugLevel;
    mvars->bound = dmp->dm_bound;
    mvars->boundFlag = dmp->dm_boundFlag;

    /* this is important so that ogl_configureWin knows to set the font */
    pubvars->fontstruct = NULL;

    if ((tmp_dpy = XOpenDisplay(bu_vls_addr(&dmp->dm_dName))) == NULL) {
	bu_vls_free(&init_proc_vls);
	(void)ogl_close(dmp);
	return DM_NULL;
    }

#ifdef HAVE_XQUERYEXTENSION
    {
	int return_val;

	if (!XQueryExtension(tmp_dpy, "GLX", &return_val, &return_val, &return_val)) {
	    bu_vls_free(&init_proc_vls);
	    (void)ogl_close(dmp);
	    return DM_NULL;
	}
    }
#endif

    screen_number = XDefaultScreen(tmp_dpy);
    if (screen_number < 0)
	bu_log("WARNING: screen number is [%d]\n", screen_number);

    if (dmp->dm_width == 0) {
	dmp->dm_width = XDisplayWidth(tmp_dpy, screen_number) - 30;
	++make_square;
    }
    if (dmp->dm_height == 0) {
	dmp->dm_height = XDisplayHeight(tmp_dpy, screen_number) - 30;
	++make_square;
    }

    if (make_square > 0) {
	/* Make window square */
	if (dmp->dm_height <
	    dmp->dm_width)
	    dmp->dm_width =
		dmp->dm_height;
	else
	    dmp->dm_height =
		dmp->dm_width;
    }

    XCloseDisplay(tmp_dpy);

    if (dmp->dm_top) {
	/* Make xtkwin a toplevel window */
	pubvars->xtkwin =
	    Tk_CreateWindowFromPath(interp,
				    tkwin,
				    bu_vls_addr(&dmp->dm_pathName),
				    bu_vls_addr(&dmp->dm_dName));
	pubvars->top = pubvars->xtkwin;
    } else {
	char *cp;

	cp = strrchr(bu_vls_addr(&dmp->dm_pathName), (int)'.');
	if (cp == bu_vls_addr(&dmp->dm_pathName)) {
	    pubvars->top = tkwin;
	} else {
	    struct bu_vls top_vls = BU_VLS_INIT_ZERO;

	    bu_vls_strncpy(&top_vls, (const char *)bu_vls_addr(&dmp->dm_pathName), cp - bu_vls_addr(&dmp->dm_pathName));

	    pubvars->top =
		Tk_NameToWindow(interp, bu_vls_addr(&top_vls), tkwin);
	    bu_vls_free(&top_vls);
	}

	/* Make xtkwin an embedded window */
	pubvars->xtkwin =
	    Tk_CreateWindow(interp, pubvars->top,
			    cp + 1, (char *)NULL);
    }

    if (pubvars->xtkwin == NULL) {
	bu_log("dm-Ogl: Failed to open %s\n", bu_vls_addr(&dmp->dm_pathName));
	bu_vls_free(&init_proc_vls);
	(void)ogl_close(dmp);
	return DM_NULL;
    }

    bu_vls_printf(&dmp->dm_tkName, "%s",
		  (char *)Tk_Name(pubvars->xtkwin));

    bu_vls_printf(&str, "_init_dm %s %s\n",
		  bu_vls_addr(&init_proc_vls),
		  bu_vls_addr(&dmp->dm_pathName));

    if (Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR) {
	bu_vls_free(&init_proc_vls);
	bu_vls_free(&str);
	(void)ogl_close(dmp);
	return DM_NULL;
    }

    bu_vls_free(&init_proc_vls);
    bu_vls_free(&str);

    pubvars->dpy =
	Tk_Display(pubvars->top);

    /* make sure there really is a display before proceeding. */
    if (!(pubvars->dpy)) {
	bu_vls_free(&init_proc_vls);
	bu_vls_free(&str);
	(void)ogl_close(dmp);
	return DM_NULL;
    }

    Tk_GeometryRequest(pubvars->xtkwin,
		       dmp->dm_width,
		       dmp->dm_height);

    /* must do this before MakeExist */
    if ((pubvars->vip=ogl_choose_visual(dmp, pubvars->xtkwin)) == NULL) {
	bu_log("ogl_open: Can't get an appropriate visual.\n");
	(void)ogl_close(dmp);
	return DM_NULL;
    }

    pubvars->depth = mvars->depth;

    Tk_MakeWindowExist(pubvars->xtkwin);

    pubvars->win = Tk_WindowId(pubvars->xtkwin);
    dmp->dm_id = pubvars->win;

    /* open GLX context */
    if ((privvars->glxc =
	 glXCreateContext(pubvars->dpy, pubvars->vip, (GLXContext)NULL, GL_TRUE))==NULL) {
	bu_log("ogl_open: couldn't create glXContext.\n");
	(void)ogl_close(dmp);
	return DM_NULL;
    }

    /* If we used an indirect context, then as far as sgi is concerned,
     * gl hasn't been used.
     */
    privvars->is_direct = (char) glXIsDirect(pubvars->dpy, privvars->glxc);

    /*
     * Take a look at the available input devices. We're looking
     * for "dial+buttons".
     */
    if (XQueryExtension(pubvars->dpy, "XInputExtension", &unused, &unused, &unused)) {
	olist = list = (XDeviceInfoPtr)XListInputDevices(pubvars->dpy, &ndevices);
    }

    if (list == (XDeviceInfoPtr)NULL ||
	list == (XDeviceInfoPtr)1) goto Done;

    for (j = 0; j < ndevices; ++j, list++) {
	if (list->use == IsXExtensionDevice) {
	    if (BU_STR_EQUAL(list->name, "dial+buttons")) {
		if ((dev = XOpenDevice(pubvars->dpy,
				       list->id)) == (XDevice *)NULL) {
		    bu_log("ogl_open: Couldn't open the dials+buttons\n");
		    goto Done;
		}

		for (cip = dev->classes, k = 0; k < dev->num_classes;
		     ++k, ++cip) {
		    switch (cip->input_class) {
#ifdef IR_BUTTONS
			case ButtonClass:
			    DeviceButtonPress(dev, pubvars->devbuttonpress,
					      e_class[nclass]);
			    ++nclass;
			    DeviceButtonRelease(dev, pubvars->devbuttonrelease,
						e_class[nclass]);
			    ++nclass;
			    break;
#endif
#ifdef IR_KNOBS
			case ValuatorClass:
			    DeviceMotionNotify(dev, pubvars->devmotionnotify,
					       e_class[nclass]);
			    ++nclass;
			    break;
#endif
			default:
			    break;
		    }
		}

		XSelectExtensionEvent(pubvars->dpy, pubvars->win, e_class, nclass);
		goto Done;
	    }
	}
    }
Done:
    XFreeDeviceList(olist);

    Tk_MapWindow(pubvars->xtkwin);

    if (!glXMakeCurrent(pubvars->dpy, pubvars->win, privvars->glxc)) {
	bu_log("ogl_open: Couldn't make context current\n");
	(void)ogl_close(dmp);
	return DM_NULL;
    }

    /* display list (fontOffset + char) will display a given ASCII char */
    if ((privvars->fontOffset = glGenLists(128))==0) {
	bu_log("dm-ogl: Can't make display lists for font.\n");
	(void)ogl_close(dmp);
	return DM_NULL;
    }

    /* This is the applications display list offset */
    dmp->dm_displaylist = privvars->fontOffset + 128;

    ogl_setBGColor(dmp, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (mvars->doublebuffer)
	glDrawBuffer(GL_BACK);
    else
	glDrawBuffer(GL_FRONT);

    /* do viewport, ortho commands and initialize font */
    (void)ogl_configureWin_guts(dmp, 1);

    /* Lines will be solid when stippling disabled, dashed when enabled*/
    glLineStipple(1, 0xCF33);
    glDisable(GL_LINE_STIPPLE);

    backgnd[0] = backgnd[1] = backgnd[2] = backgnd[3] = 0.0;
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 0.0);
    glFogf(GL_FOG_END, 2.0);
    glFogfv(GL_FOG_COLOR, backgnd);

    /*XXX Need to do something about VIEWFACTOR */
    glFogf(GL_FOG_DENSITY, VIEWFACTOR);

    /* Initialize matrices */
    /* Leave it in model_view mode normally */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0);
    glGetDoublev(GL_PROJECTION_MATRIX, privvars->faceplate_mat);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glPushMatrix();
    glLoadIdentity();
    privvars->face_flag = 1;	/* faceplate matrix is on top of stack */

    ogl_setZBuffer(dmp, dmp->dm_zbuffer);
    ogl_setLight(dmp, dmp->dm_light);

    return dmp;
}


int
ogl_share_dlist(struct dm_internal *dmp1, struct dm_internal *dmp2)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)dmp1->m_vars;
    GLfloat backgnd[4];
    GLfloat vf;
    GLXContext old_glxContext;

    if (dmp1 == (struct dm_internal *)NULL)
	return TCL_ERROR;

    if (dmp2 == (struct dm_internal *)NULL) {
	/* create a new graphics context for dmp1 with private display lists */

	old_glxContext = ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->glxc;

	if ((((struct ogl_vars *)dmp1->dm_vars.priv_vars)->glxc =
	     glXCreateContext(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->dpy,
			      ((struct dm_xvars *)dmp1->dm_vars.pub_vars)->vip,
			      (GLXContext)NULL, GL_TRUE))==NULL) {
	    bu_log("ogl_share_dlist: couldn't create glXContext.\nUsing old context\n.");
	    ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	if (!glXMakeCurrent(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->dpy,
			    ((struct dm_xvars *)dmp1->dm_vars.pub_vars)->win,
			    ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->glxc)) {
	    bu_log("ogl_share_dlist: Couldn't make context current\nUsing old context\n.");
	    ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	/* display list (fontOffset + char) will display a given ASCII char */
	if ((((struct ogl_vars *)dmp1->dm_vars.priv_vars)->fontOffset = glGenLists(128))==0) {
	    bu_log("dm-ogl: Can't make display lists for font.\nUsing old context\n.");
	    ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	/* This is the applications display list offset */
	dmp1->dm_displaylist = ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->fontOffset + 128;

	ogl_setBGColor(dmp1, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (mvars->doublebuffer)
	    glDrawBuffer(GL_BACK);
	else
	    glDrawBuffer(GL_FRONT);

	/* this is important so that ogl_configureWin knows to set the font */
	((struct dm_xvars *)dmp1->dm_vars.pub_vars)->fontstruct = NULL;

	/* do viewport, ortho commands and initialize font */
	(void)ogl_configureWin_guts(dmp1, 1);

	/* Lines will be solid when stippling disabled, dashed when enabled*/
	glLineStipple(1, 0xCF33);
	glDisable(GL_LINE_STIPPLE);

	backgnd[0] = backgnd[1] = backgnd[2] = backgnd[3] = 0.0;
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, 0.0);
	glFogf(GL_FOG_END, 2.0);
	glFogfv(GL_FOG_COLOR, backgnd);

	/*XXX Need to do something about VIEWFACTOR */
	vf = 1.0/(*dmp1->dm_vp);
	glFogf(GL_FOG_DENSITY, vf);

	/* Initialize matrices */
	/* Leave it in model_view mode normally */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0);
	glGetDoublev(GL_PROJECTION_MATRIX, ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->faceplate_mat);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPushMatrix();
	glLoadIdentity();
	((struct ogl_vars *)dmp1->dm_vars.priv_vars)->face_flag = 1; /* faceplate matrix is on top of stack */

	/* destroy old context */
	glXMakeCurrent(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->dpy, None, NULL);
	glXDestroyContext(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->dpy, old_glxContext);
    } else {
	/* dmp1 will share its display lists with dmp2 */

	old_glxContext = ((struct ogl_vars *)dmp2->dm_vars.priv_vars)->glxc;

	if ((((struct ogl_vars *)dmp2->dm_vars.priv_vars)->glxc =
	     glXCreateContext(((struct dm_xvars *)dmp2->dm_vars.pub_vars)->dpy,
			      ((struct dm_xvars *)dmp2->dm_vars.pub_vars)->vip,
			      ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->glxc,
			      GL_TRUE))==NULL) {
	    bu_log("ogl_share_dlist: couldn't create glXContext.\nUsing old context\n.");
	    ((struct ogl_vars *)dmp2->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	if (!glXMakeCurrent(((struct dm_xvars *)dmp2->dm_vars.pub_vars)->dpy,
			    ((struct dm_xvars *)dmp2->dm_vars.pub_vars)->win,
			    ((struct ogl_vars *)dmp2->dm_vars.priv_vars)->glxc)) {
	    bu_log("ogl_share_dlist: Couldn't make context current\nUsing old context\n.");
	    ((struct ogl_vars *)dmp2->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	((struct ogl_vars *)dmp2->dm_vars.priv_vars)->fontOffset = ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->fontOffset;
	dmp2->dm_displaylist = dmp1->dm_displaylist;

	ogl_setBGColor(dmp2, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (mvars->doublebuffer)
	    glDrawBuffer(GL_BACK);
	else
	    glDrawBuffer(GL_FRONT);

	/* do viewport, ortho commands and initialize font */
	(void)ogl_configureWin_guts(dmp2, 1);

	/* Lines will be solid when stippling disabled, dashed when enabled*/
	glLineStipple(1, 0xCF33);
	glDisable(GL_LINE_STIPPLE);

	backgnd[0] = backgnd[1] = backgnd[2] = backgnd[3] = 0.0;
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, 0.0);
	glFogf(GL_FOG_END, 2.0);
	glFogfv(GL_FOG_COLOR, backgnd);

	/*XXX Need to do something about VIEWFACTOR */
	vf = 1.0/(*dmp2->dm_vp);
	glFogf(GL_FOG_DENSITY, vf);

	/* Initialize matrices */
	/* Leave it in model_view mode normally */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0);
	glGetDoublev(GL_PROJECTION_MATRIX, ((struct ogl_vars *)dmp2->dm_vars.priv_vars)->faceplate_mat);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPushMatrix();
	glLoadIdentity();
	((struct ogl_vars *)dmp2->dm_vars.priv_vars)->face_flag = 1; /* faceplate matrix is on top of stack */

	/* destroy old context */
	glXMakeCurrent(((struct dm_xvars *)dmp2->dm_vars.pub_vars)->dpy, None, NULL);
	glXDestroyContext(((struct dm_xvars *)dmp2->dm_vars.pub_vars)->dpy, old_glxContext);
    }

    return TCL_OK;
}


/*
 * There are global variables which are parameters to this routine.
 */
HIDDEN int
ogl_drawBegin(struct dm_internal *dmp)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)dmp->m_vars;
    GLfloat fogdepth;

    if (dmp->dm_debugLevel) {
	bu_log("ogl_drawBegin\n");
    }

    if (dmp->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "initial view matrix = \n");

	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	ogl_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "initial projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	ogl_printglmat(&tmp_vls, m);

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }



    if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("ogl_drawBegin: Couldn't make context current\n");
	return TCL_ERROR;
    }

    /* clear back buffer */
    if (!dmp->dm_clearBufferAfter && mvars->doublebuffer) {
	glClearColor(((struct ogl_vars *)dmp->dm_vars.priv_vars)->r,
		     ((struct ogl_vars *)dmp->dm_vars.priv_vars)->g,
		     ((struct ogl_vars *)dmp->dm_vars.priv_vars)->b,
		     0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    if (((struct ogl_vars *)dmp->dm_vars.priv_vars)->face_flag) {
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	((struct ogl_vars *)dmp->dm_vars.priv_vars)->face_flag = 0;
	if (mvars->cueing_on) {
	    glEnable(GL_FOG);
	    /*XXX Need to do something with Viewscale */
	    fogdepth = 2.2 * (*dmp->dm_vp); /* 2.2 is heuristic */
	    glFogf(GL_FOG_END, fogdepth);
	    fogdepth = (GLfloat) (0.5*mvars->fogdensity/
				  (*dmp->dm_vp));
	    glFogf(GL_FOG_DENSITY, fogdepth);
	    glFogi(GL_FOG_MODE, dmp->dm_perspective ? GL_EXP : GL_LINEAR);
	}
	if (dmp->dm_light) {
	    glEnable(GL_LIGHTING);
	}
    }

    if (dmp->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "after begin view matrix = \n");

	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	ogl_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "after begin projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	ogl_printglmat(&tmp_vls, m);

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }


    return TCL_OK;
}


HIDDEN int
ogl_drawEnd(struct dm_internal *dmp)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)dmp->m_vars;
    if (dmp->dm_debugLevel)
	bu_log("ogl_drawEnd\n");

    if (dmp->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "beginning of end view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	ogl_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "beginning of end projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	ogl_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }


    if (dmp->dm_light) {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
    }

    if (mvars->doublebuffer) {
	glXSwapBuffers(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		       ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win);

	if (dmp->dm_clearBufferAfter) {
	    /* give Graphics pipe time to work */
	    glClearColor(((struct ogl_vars *)dmp->dm_vars.priv_vars)->r,
			 ((struct ogl_vars *)dmp->dm_vars.priv_vars)->g,
			 ((struct ogl_vars *)dmp->dm_vars.priv_vars)->b,
			 0.0);
	    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
    }

    if (dmp->dm_debugLevel) {
	int error;
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "ANY ERRORS?\n");

	while ((error = glGetError())!=0) {
	    bu_vls_printf(&tmp_vls, "Error: %x\n", error);
	}

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    if (dmp->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "end of drawend view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	ogl_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "end of drawend projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	ogl_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }


    return TCL_OK;
}


/*
 * Load a new transformation matrix.  This will be followed by
 * many calls to ogl_draw().
 */
HIDDEN int
ogl_loadMatrix(struct dm_internal *dmp, fastf_t *mat, int which_eye)
{
    fastf_t *mptr;
    GLfloat gtmat[16];

    if (dmp->dm_debugLevel == 1)
	bu_log("ogl_loadMatrix()\n");

    if (dmp->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "beginning of loadMatrix view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	ogl_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "beginning of loadMatrix projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	ogl_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }


    if (dmp->dm_debugLevel == 3) {
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "transformation matrix = \n");
	ogl_printmat(&tmp_vls, mat);

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    switch (which_eye) {
	case 0:
	    /* Non-stereo */
	    break;
	case 1:
	    /* R eye */
	    glViewport(0,  0, (XMAXSCREEN)+1, (YSTEREO)+1);
	    glScissor(0,  0, (XMAXSCREEN)+1, (YSTEREO)+1);
	    ogl_drawString2D(dmp, "R", 0.986, 0.0, 0, 1);
	    break;
	case 2:
	    /* L eye */
	    glViewport(0,  0+YOFFSET_LEFT, (XMAXSCREEN)+1,
		       (YSTEREO+YOFFSET_LEFT)-(YOFFSET_LEFT)+1);
	    glScissor(0,  0+YOFFSET_LEFT, (XMAXSCREEN)+1,
		      (YSTEREO+YOFFSET_LEFT)-(YOFFSET_LEFT)+1);
	    break;
    }

    mptr = mat;

    gtmat[0] = *(mptr++);
    gtmat[4] = *(mptr++);
    gtmat[8] = *(mptr++);
    gtmat[12] = *(mptr++);

    gtmat[1] = *(mptr++) * dmp->dm_aspect;
    gtmat[5] = *(mptr++) * dmp->dm_aspect;
    gtmat[9] = *(mptr++) * dmp->dm_aspect;
    gtmat[13] = *(mptr++) * dmp->dm_aspect;

    gtmat[2] = *(mptr++);
    gtmat[6] = *(mptr++);
    gtmat[10] = *(mptr++);
    gtmat[14] = *(mptr++);

    gtmat[3] = *(mptr++);
    gtmat[7] = *(mptr++);
    gtmat[11] = *(mptr++);
    gtmat[15] = *(mptr++);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glLoadMatrixf(gtmat);

    if (dmp->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "end of loadMatrix view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	ogl_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "end of loadMatrix projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	ogl_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    return TCL_OK;
}


/*
 * Load a new projection matrix.
 *
 */
HIDDEN int
ogl_loadPMatrix(struct dm_internal *dmp, fastf_t *mat)
{
    fastf_t *mptr;
    GLfloat gtmat[16];

    glMatrixMode(GL_PROJECTION);

    if (mat == (fastf_t *)NULL) {
	if (((struct ogl_vars *)dmp->dm_vars.priv_vars)->face_flag) {
	    glPopMatrix();
	    glLoadIdentity();
	    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->dm_clipmin[2], dmp->dm_clipmax[2]);
	    glPushMatrix();
	    glLoadMatrixd(((struct ogl_vars *)dmp->dm_vars.priv_vars)->faceplate_mat);
	} else {
	    glLoadIdentity();
	    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->dm_clipmin[2], dmp->dm_clipmax[2]);
	}

	return TCL_OK;
    }

    mptr = mat;

    gtmat[0] = *(mptr++);
    gtmat[4] = *(mptr++);
    gtmat[8] = *(mptr++);
    gtmat[12] = *(mptr++);

    gtmat[1] = *(mptr++);
    gtmat[5] = *(mptr++);
    gtmat[9] = *(mptr++);
    gtmat[13] = *(mptr++);

    gtmat[2] = *(mptr++);
    gtmat[6] = *(mptr++);
    gtmat[10] = -*(mptr++);
    gtmat[14] = -*(mptr++);

    gtmat[3] = *(mptr++);
    gtmat[7] = *(mptr++);
    gtmat[11] = *(mptr++);
    gtmat[15] = *(mptr++);

    glLoadIdentity();
    glLoadMatrixf(gtmat);

    return TCL_OK;
}


HIDDEN int
ogl_drawVListHiddenLine(struct dm_internal *dmp, register struct bn_vlist *vp)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)dmp->m_vars;
    register struct bn_vlist *tvp;
    register int first;

    if (dmp->dm_debugLevel == 1)
	bu_log("ogl_drawVList()\n");


    /* First, draw polygons using background color. */

    if (dmp->dm_light) {
	glDisable(GL_LIGHTING);
    }

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPolygonOffset(1.0, 1.0);

    /* Set color to background color for drawing polygons. */
    glColor3f(((struct ogl_vars *)dmp->dm_vars.priv_vars)->r,
	      ((struct ogl_vars *)dmp->dm_vars.priv_vars)->g,
	      ((struct ogl_vars *)dmp->dm_vars.priv_vars)->b);

    /* Viewing region is from -1.0 to +1.0 */
    first = 1;
    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
	register int i;
	register int nused = tvp->nused;
	register int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    GLdouble dpt[3];
	    VMOVE(dpt, *pt); /* fastf_t-to-double */
/*
	    if (dmp->dm_debugLevel > 2)
		bu_log(" %d (%g %g %g)\n", *cmd, V3ARGS(dpt));*/

	    switch (*cmd) {
		case BN_VLIST_LINE_MOVE:
		case BN_VLIST_LINE_DRAW:
		    break;
		case BN_VLIST_POLY_START:
		    /* Start poly marker & normal */
		    if (first == 0)
			glEnd();
		    first = 0;

		    glBegin(GL_POLYGON);
		    /* Set surface normal (vl_pnt points outward) */
		    glNormal3dv(dpt);
		    break;
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_POLY_DRAW:
		case BN_VLIST_TRI_MOVE:
		case BN_VLIST_TRI_DRAW:
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_POLY_END:
		    /* Draw, End Polygon */
		    glEnd();
		    first = 1;
		    break;
		case BN_VLIST_POLY_VERTNORM:
		case BN_VLIST_TRI_VERTNORM:
		    /* Set per-vertex normal.  Given before vert. */
		    glNormal3dv(dpt);
		    break;
		case BN_VLIST_TRI_START:
		    if (first)
			glBegin(GL_TRIANGLES);

		    first = 0;

		    /* Set surface normal (vl_pnt points outward) */
		    glNormal3dv(dpt);

		    break;
		case BN_VLIST_TRI_END:
		    break;
	    }
	}
    }

    if (first == 0)
	glEnd();

    /* Last, draw wireframe/edges. */

    /* Set color to wireColor for drawing wireframe/edges */
    glColor3f(wireColor[0], wireColor[1], wireColor[2]);

    /* Viewing region is from -1.0 to +1.0 */
    first = 1;
    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
	register int i;
	register int nused = tvp->nused;
	register int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;

	for (i = 0; i < nused; i++, cmd++, pt++) {
	    GLdouble dpt[3];
	    VMOVE(dpt, *pt); /* fastf_t-to-double */
/*
	    if (dmp->dm_debugLevel > 2)
		bu_log(" %d (%g %g %g)\n", *cmd, V3ARGS(dpt));*/

	    switch (*cmd) {
		case BN_VLIST_LINE_MOVE:
		    /* Move, start line */
		    if (first == 0)
			glEnd();
		    first = 0;

		    glBegin(GL_LINE_STRIP);
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_POLY_START:
		case BN_VLIST_TRI_START:
		    /* Start poly marker & normal */
		    if (first == 0)
			glEnd();
		    first = 0;

		    glBegin(GL_LINE_STRIP);
		    break;
		case BN_VLIST_LINE_DRAW:
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_POLY_DRAW:
		case BN_VLIST_TRI_MOVE:
		case BN_VLIST_TRI_DRAW:
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_POLY_END:
		case BN_VLIST_TRI_END:
		    /* Draw, End Polygon */
		    glVertex3dv(dpt);
		    glEnd();
		    first = 1;
		    break;
		case BN_VLIST_POLY_VERTNORM:
		case BN_VLIST_TRI_VERTNORM:
		    /* Set per-vertex normal.  Given before vert. */
		    glNormal3dv(dpt);
		    break;
	    }
	}
    }

    if (first == 0)
	glEnd();

    if (dmp->dm_light) {
	glEnable(GL_LIGHTING);
    }

    if (!mvars->zbuffer_on)
	glDisable(GL_DEPTH_TEST);

    if (!dmp->dm_depthMask)
	glDepthMask(GL_FALSE);

    glDisable(GL_POLYGON_OFFSET_FILL);

    return TCL_OK;
}


HIDDEN int
ogl_drawVList(struct dm_internal *dmp, struct bn_vlist *vp)
{
    struct bn_vlist *tvp;
    register int first;
    register int mflag = 1;
    static float black[4] = {0.0, 0.0, 0.0, 0.0};
    GLfloat originalPointSize, originalLineWidth;

    glGetFloatv(GL_POINT_SIZE, &originalPointSize);
    glGetFloatv(GL_LINE_WIDTH, &originalLineWidth);

    if (dmp->dm_debugLevel == 1)
	bu_log("ogl_drawVList()\n");

    /* Viewing region is from -1.0 to +1.0 */
    first = 1;
    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
	int i;
	int nused = tvp->nused;
	int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    GLdouble dpt[3];
	    VMOVE(dpt, *pt);
/*
	    if (dmp->dm_debugLevel > 2)
		bu_log(" %d (%g %g %g)\n", *cmd, V3ARGS(dpt));*/

	    switch (*cmd) {
		case BN_VLIST_LINE_MOVE:
		    /* Move, start line */
		    if (first == 0)
			glEnd();
		    first = 0;

		    if (dmp->dm_light && mflag) {
			mflag = 0;
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, wireColor);
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);

			if (dmp->dm_transparency)
			    glDisable(GL_BLEND);
		    }

		    glBegin(GL_LINE_STRIP);
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_POLY_START:
		case BN_VLIST_TRI_START:
		    /* Start poly marker & normal */

		    if (dmp->dm_light && mflag) {
			mflag = 0;
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambientColor);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);
			glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuseColor);

			switch (dmp->dm_light) {
			    case 1:
				break;
			    case 2:
				glMaterialfv(GL_BACK, GL_DIFFUSE, diffuseColor);
				break;
			    case 3:
				glMaterialfv(GL_BACK, GL_DIFFUSE, backDiffuseColorDark);
				break;
			    default:
				glMaterialfv(GL_BACK, GL_DIFFUSE, backDiffuseColorLight);
				break;
			}

			if (dmp->dm_transparency)
			    glEnable(GL_BLEND);
		    }

		    if (*cmd == BN_VLIST_POLY_START) {
			if (first == 0)
			    glEnd();

			glBegin(GL_POLYGON);
		    } else if (first)
			glBegin(GL_TRIANGLES);

		    /* Set surface normal (vl_pnt points outward) */
		    glNormal3dv(dpt);

		    first = 0;

		    break;
		case BN_VLIST_LINE_DRAW:
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_POLY_DRAW:
		case BN_VLIST_TRI_MOVE:
		case BN_VLIST_TRI_DRAW:
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_POLY_END:
		    /* Draw, End Polygon */
		    glEnd();
		    first = 1;
		    break;
		case BN_VLIST_TRI_END:
		    break;
		case BN_VLIST_POLY_VERTNORM:
		case BN_VLIST_TRI_VERTNORM:
		    /* Set per-vertex normal.  Given before vert. */
		    glNormal3dv(dpt);
		    break;
		case BN_VLIST_POINT_DRAW:
		    if (first == 0)
			glEnd();
		    first = 0;
		    glBegin(GL_POINTS);
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_LINE_WIDTH: {
		    GLfloat lineWidth = (GLfloat)(*pt)[0];
		    if (lineWidth > 0.0) {
			glLineWidth(lineWidth);
		    }
		    break;
		}
		case BN_VLIST_POINT_SIZE: {
		    GLfloat pointSize = (GLfloat)(*pt)[0];
		    if (pointSize > 0.0) {
			glPointSize(pointSize);
		    }
		    break;
		}
	    }
	}
    }

    if (first == 0)
	glEnd();

    if (dmp->dm_light && dmp->dm_transparency)
	glDisable(GL_BLEND);

    glPointSize(originalPointSize);
    glLineWidth(originalLineWidth);

    return TCL_OK;
}


HIDDEN int
ogl_draw(struct dm_internal *dmp, struct bn_vlist *(*callback_function)(void *), void **data)
{
    struct bn_vlist *vp;
    if (!callback_function) {
	if (data) {
	    vp = (struct bn_vlist *)data;
	    ogl_drawVList(dmp, vp);
	}
    } else {
	if (!data) {
	    return TCL_ERROR;
	} else {
	    (void)callback_function(data);
	}
    }
    return TCL_OK;
}


/*
 * Restore the display processor to a normal mode of operation
 * (i.e., not scaled, rotated, displaced, etc.).
 */
HIDDEN int
ogl_normal(struct dm_internal *dmp)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)dmp->m_vars;
    if (dmp->dm_debugLevel)
	bu_log("ogl_normal\n");

    if (dmp->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "beginning of ogl_normal view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	ogl_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "beginning of ogl_normal projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	ogl_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    if (!((struct ogl_vars *)dmp->dm_vars.priv_vars)->face_flag) {
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixd(((struct ogl_vars *)dmp->dm_vars.priv_vars)->faceplate_mat);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	((struct ogl_vars *)dmp->dm_vars.priv_vars)->face_flag = 1;
	if (mvars->cueing_on)
	    glDisable(GL_FOG);
	if (dmp->dm_light)
	    glDisable(GL_LIGHTING);
    }

    if (dmp->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "end of ogl_normal view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	ogl_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "end of ogl_normal projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	ogl_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    return TCL_OK;
}


/*
 * Output a string.
 * The starting position of the beam is as specified.
 */
HIDDEN int
ogl_drawString2D(struct dm_internal *dmp, const char *str, fastf_t x, fastf_t y, int UNUSED(size), int use_aspect)
{
    if (dmp->dm_debugLevel)
	bu_log("ogl_drawString2D()\n");

    if (use_aspect)
	glRasterPos2f(x, y * dmp->dm_aspect);
    else
	glRasterPos2f(x, y);

    glListBase(((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
    glCallLists(strlen(str), GL_UNSIGNED_BYTE,  str);

    return TCL_OK;
}


HIDDEN int
ogl_drawLine2D(struct dm_internal *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2)
{
    return drawLine2D(dmp, X1, Y1, X2, Y2, "ogl_drawLine2D()\n");
}


HIDDEN int
ogl_drawLine3D(struct dm_internal *dmp, point_t pt1, point_t pt2)
{
    return drawLine3D(dmp, pt1, pt2, "ogl_drawLine3D()\n", wireColor);
}


HIDDEN int
ogl_drawLines3D(struct dm_internal *dmp, int npoints, point_t *points, int sflag)
{
    return drawLines3D(dmp, npoints, points, sflag, "ogl_drawLine3D()\n", wireColor);
}


HIDDEN int
ogl_drawPoint2D(struct dm_internal *dmp, fastf_t x, fastf_t y)
{
    if (dmp->dm_debugLevel) {
	bu_log("ogl_drawPoint2D():\n");
	bu_log("\tdmp: %p\tx - %lf\ty - %lf\n", (void *)dmp, x, y);
    }

    glBegin(GL_POINTS);
    glVertex2f(x, y);
    glEnd();

    return TCL_OK;
}


HIDDEN int
ogl_drawPoint3D(struct dm_internal *dmp, point_t point)
{
    GLdouble dpt[3];

    if (!dmp || !point)
	return TCL_ERROR;

    if (dmp->dm_debugLevel) {
	bu_log("ogl_drawPoint3D():\n");
	bu_log("\tdmp: %p\tpt - %lf %lf %lf\n", (void*)dmp, V3ARGS(point));
    }

    /* fastf_t to double */
    VMOVE(dpt, point);

    glBegin(GL_POINTS);
    glVertex3dv(dpt);
    glEnd();

    return TCL_OK;
}


HIDDEN int
ogl_drawPoints3D(struct dm_internal *dmp, int npoints, point_t *points)
{
    GLdouble dpt[3];
    register int i;

    if (!dmp || npoints < 0 || !points)
	return TCL_ERROR;

    if (dmp->dm_debugLevel) {
	bu_log("ogl_drawPoint3D():\n");
    }


    glBegin(GL_POINTS);
    for (i = 0; i < npoints; ++i) {
	/* fastf_t to double */
	VMOVE(dpt, points[i]);
	glVertex3dv(dpt);
    }
    glEnd();

    return TCL_OK;
}


HIDDEN int
ogl_setFGColor(struct dm_internal *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    /*if (dmp->dm_debugLevel)
	bu_log("ogl_setFGColor()\n");*/

    dmp->dm_fg[0] = r;
    dmp->dm_fg[1] = g;
    dmp->dm_fg[2] = b;

    /* wireColor gets the full rgb */
    wireColor[0] = r / 255.0;
    wireColor[1] = g / 255.0;
    wireColor[2] = b / 255.0;
    wireColor[3] = transparency;

    if (strict) {
	glColor3ub((GLubyte)r, (GLubyte)g, (GLubyte)b);
    } else {

	if (dmp->dm_light) {
	    /* Ambient = .2, Diffuse = .6, Specular = .2 */

	    ambientColor[0] = wireColor[0] * 0.2;
	    ambientColor[1] = wireColor[1] * 0.2;
	    ambientColor[2] = wireColor[2] * 0.2;
	    ambientColor[3] = wireColor[3];

	    specularColor[0] = ambientColor[0];
	    specularColor[1] = ambientColor[1];
	    specularColor[2] = ambientColor[2];
	    specularColor[3] = ambientColor[3];

	    diffuseColor[0] = wireColor[0] * 0.6;
	    diffuseColor[1] = wireColor[1] * 0.6;
	    diffuseColor[2] = wireColor[2] * 0.6;
	    diffuseColor[3] = wireColor[3];

	    backDiffuseColorDark[0] = wireColor[0] * 0.3;
	    backDiffuseColorDark[1] = wireColor[1] * 0.3;
	    backDiffuseColorDark[2] = wireColor[2] * 0.3;
	    backDiffuseColorDark[3] = wireColor[3];

	    backDiffuseColorLight[0] = wireColor[0] * 0.9;
	    backDiffuseColorLight[1] = wireColor[1] * 0.9;
	    backDiffuseColorLight[2] = wireColor[2] * 0.9;
	    backDiffuseColorLight[3] = wireColor[3];

	    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambientColor);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuseColor);
	} else {
	    glColor3ub((GLubyte)r,  (GLubyte)g,  (GLubyte)b);
	}
    }

    return TCL_OK;
}


HIDDEN int
ogl_setLineAttr(struct dm_internal *dmp, int width, int style)
{
    /*if (dmp->dm_debugLevel)
	bu_log("ogl_setLineAttr()\n");*/

    dmp->dm_lineWidth = width;
    dmp->dm_lineStyle = style;

    glLineWidth((GLfloat) width);

    if (style == DM_DASHED_LINE)
	glEnable(GL_LINE_STIPPLE);
    else
	glDisable(GL_LINE_STIPPLE);

    return TCL_OK;
}


HIDDEN int
ogl_debug(struct dm_internal *dmp, int lvl)
{
    dmp->dm_debugLevel = lvl;

    return TCL_OK;
}

HIDDEN int
ogl_logfile(struct dm_internal *dmp, const char *filename)
{
    bu_vls_sprintf(&dmp->dm_log, "%s", filename);

    return TCL_OK;
}

HIDDEN int
ogl_setWinBounds(struct dm_internal *dmp, fastf_t *w)
{
    GLint mm;

    if (dmp->dm_debugLevel)
	bu_log("ogl_setWinBounds()\n");

    dmp->dm_clipmin[0] = w[0];
    dmp->dm_clipmin[1] = w[2];
    dmp->dm_clipmin[2] = w[4];
    dmp->dm_clipmax[0] = w[1];
    dmp->dm_clipmax[1] = w[3];
    dmp->dm_clipmax[2] = w[5];

    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glLoadIdentity();
    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->dm_clipmin[2], dmp->dm_clipmax[2]);
    glPushMatrix();
    glMatrixMode(mm);

    return TCL_OK;
}


HIDDEN int
ogl_setTransparency(struct dm_internal *dmp,
		    int transparency_on)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)dmp->m_vars;
    if (dmp->dm_debugLevel)
	bu_log("ogl_setTransparency()\n");

    dmp->dm_transparency = transparency_on;
    mvars->transparency_on = dmp->dm_transparency;

    if (transparency_on) {
	/* Turn it on */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
	/* Turn it off */
	glDisable(GL_BLEND);
    }

    return TCL_OK;
}


HIDDEN int
ogl_setDepthMask(struct dm_internal *dmp,
		 int enable) {
    if (dmp->dm_debugLevel)
	bu_log("ogl_setDepthMask()\n");

    dmp->dm_depthMask = enable;

    if (enable)
	glDepthMask(GL_TRUE);
    else
	glDepthMask(GL_FALSE);

    return TCL_OK;
}


HIDDEN int
ogl_setZBuffer(struct dm_internal *dmp, int zbuffer_on)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)dmp->m_vars;
    if (dmp->dm_debugLevel)
	bu_log("ogl_setZBuffer:\n");

    dmp->dm_zbuffer = zbuffer_on;
    mvars->zbuffer_on = dmp->dm_zbuffer;

    if (mvars->zbuf == 0) {
	dmp->dm_zbuffer = 0;
	mvars->zbuffer_on = dmp->dm_zbuffer;
    }

    if (mvars->zbuffer_on) {
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
    } else {
	glDisable(GL_DEPTH_TEST);
    }

    return TCL_OK;
}


HIDDEN int
ogl_beginDList(struct dm_internal *dmp, unsigned int list)
{
    if (dmp->dm_debugLevel)
	bu_log("ogl_beginDList()\n");

    glNewList((GLuint)list, GL_COMPILE);
    return TCL_OK;
}


HIDDEN int
ogl_endDList(struct dm_internal *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("ogl_endDList()\n");

    glEndList();
    return TCL_OK;
}


HIDDEN void
ogl_drawDList(unsigned int list)
{
    glCallList((GLuint)list);
}


HIDDEN int
ogl_freeDLists(struct dm_internal *dmp, unsigned int list, int range)
{
    if (dmp->dm_debugLevel)
	bu_log("ogl_freeDLists()\n");

    glDeleteLists((GLuint)list, (GLsizei)range);
    return TCL_OK;
}


HIDDEN int
ogl_genDLists(struct dm_internal *dmp, size_t range)
{
    if (dmp->dm_debugLevel)
	bu_log("ogl_freeDLists()\n");

    return glGenLists((GLsizei)range);
}

HIDDEN int
ogl_draw_obj(struct dm_internal *dmp, struct display_list *obj)
{
    struct solid *sp;
    FOR_ALL_SOLIDS(sp, &obj->dl_headSolid) {
	if (sp->s_dlist == 0)
	    sp->s_dlist = dm_gen_dlists(dmp, 1);

	(void)dm_make_current(dmp);
	(void)dm_begin_dlist(dmp, sp->s_dlist);
	if (sp->s_iflag == UP)
	    (void)dm_set_fg(dmp, 255, 255, 255, 0, sp->s_transparency);
	else
	    (void)dm_set_fg(dmp,
		    (unsigned char)sp->s_color[0],
		    (unsigned char)sp->s_color[1],
		    (unsigned char)sp->s_color[2], 0, sp->s_transparency);
	(void)dm_draw_vlist(dmp, (struct bn_vlist *)&sp->s_vlist);
	(void)dm_end_dlist(dmp);
    }
    return 0;
}

HIDDEN int
ogl_getDisplayImage(struct dm_internal *dmp, unsigned char **image)
{
    unsigned char *idata = NULL;
    int width = 0;
    int height = 0;
    int bytes_per_pixel = 3; /*rgb no alpha for raw pix */
    GLuint *pixels;
    unsigned int pixel;
    unsigned int red_mask = 0xff000000;
    unsigned int green_mask = 0x00ff0000;
    unsigned int blue_mask = 0x0000ff00;
    int h, w;
#if defined(DM_WGL)
    unsigned int alpha_mask = 0x000000ff;
    int big_endian;
    int swap_bytes;

    if ((bu_byteorder() == BU_BIG_ENDIAN))
	big_endian = 1;
    else
	big_endian = 0;

    /* WTF */
    swap_bytes = !big_endian;
#endif

    if (dmp->dm_type == DM_TYPE_WGL || dmp->dm_type == DM_TYPE_OGL) {
	width = dmp->dm_width;
	height = dmp->dm_height;

	pixels = (GLuint *)bu_calloc(width * height, sizeof(GLuint), "pixels");

	{
	    glReadBuffer(GL_FRONT);
#if defined(DM_WGL)
	    /* XXX GL_UNSIGNED_INT_8_8_8_8 is currently not
	     * available on windows.  Need to update when it
	     * becomes available.
	     */
	    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
#else
	    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, pixels);
#endif

	    idata = (unsigned char *)bu_calloc(height * width * bytes_per_pixel, sizeof(unsigned char), "rgb data");
	    *image = idata;

	    for (h = 0; h < height; h++) {
		for (w = 0; w < width; w++) {
		    int i = h*width + w;
		    int i_h_inv = (height - h - 1)*width + w;
		    int j = i*bytes_per_pixel;
		    unsigned char *value = (unsigned char *)(idata + j);
#if defined(DM_WGL)
		    unsigned char alpha;
#endif

		    pixel = pixels[i_h_inv];

		    value[0] = (pixel & red_mask) >> 24;
		    value[1] = (pixel & green_mask) >> 16;
		    value[2] = (pixel & blue_mask) >> 8;

#if defined(DM_WGL)
		    alpha = pixel & alpha_mask;
		    if (swap_bytes) {
			unsigned char tmp_byte;

			value[0] = alpha;
			/* swap byte1 and byte2 */
			tmp_byte = value[1];
			value[1] = value[2];
			value[2] = tmp_byte;
		    }
#endif
		}

	    }

	    bu_free(pixels, "pixels");
	}
    } else {
	bu_log("ogl_getDisplayImage: Display type not set as OGL or WGL\n");
	return TCL_ERROR;
    }

    return TCL_OK; /* caller will need to bu_free(idata, "image data"); */
}

int
ogl_openFb(struct dm_internal *dmp)
{
    struct fb_platform_specific *fb_ps;
    struct ogl_fb_info *ofb_ps;
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)dmp->m_vars;
    struct dm_xvars *pubvars = (struct dm_xvars *)dmp->dm_vars.pub_vars;
    struct ogl_vars *privars = (struct ogl_vars *)dmp->dm_vars.priv_vars;

    fb_ps = fb_get_platform_specific(FB_OGL_MAGIC);
    ofb_ps = (struct ogl_fb_info *)fb_ps->data;
    ofb_ps->dpy = pubvars->dpy;
    ofb_ps->win = pubvars->win;
    ofb_ps->cmap = pubvars->cmap;
    ofb_ps->vip = pubvars->vip;
    ofb_ps->glxc = privars->glxc;
    ofb_ps->double_buffer = mvars->doublebuffer;
    ofb_ps->soft_cmap = 0;
    dmp->fbp = fb_open_existing("ogl", dm_get_width(dmp), dm_get_height(dmp), fb_ps);
    fb_put_platform_specific(fb_ps);
    return 0;
}

void
ogl_get_internal(struct dm_internal *dmp)
{
    struct modifiable_ogl_vars *mvars = NULL;
    if (!dmp->m_vars) {
	BU_GET(dmp->m_vars, struct modifiable_ogl_vars);
	mvars = (struct modifiable_ogl_vars *)dmp->m_vars;
	mvars->this_dm = dmp;
	bu_vls_init(&(mvars->log));
    }
}

void
ogl_put_internal(struct dm_internal *dmp)
{
    struct modifiable_ogl_vars *mvars = NULL;
    if (dmp->m_vars) {
	mvars = (struct modifiable_ogl_vars *)dmp->m_vars;
	bu_vls_free(&(mvars->log));
	BU_PUT(dmp->m_vars, struct modifiable_ogl_vars);
    }
}

void
Ogl_colorchange(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)base;
    if (mvars->cueing_on) {
	glEnable(GL_FOG);
    } else {
	glDisable(GL_FOG);
    }

    dm_generic_hook(sdp, name, base, value, data);
}

static void
ogl_zclip_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)base;
    dm *dmp = mvars->this_dm;
    fastf_t bounds[6] = { GED_MIN, GED_MAX, GED_MIN, GED_MAX, GED_MIN, GED_MAX };

    dmp->dm_zclip = mvars->zclipping_on;

    if (dmp->dm_zclip) {
	bounds[4] = -1.0;
	bounds[5] = 1.0;
    }

    (void)dm_make_current(dmp);
    (void)dm_set_win_bounds(dmp, bounds);

    dm_generic_hook(sdp, name, base, value, data);
}

static void
ogl_debug_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)base;
    dm *dmp = mvars->this_dm;

    dm_debug(dmp, mvars->debug);

    dm_generic_hook(sdp, name, base, value, data);
}


static void
ogl_logfile_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)base;
    dm *dmp = mvars->this_dm;

    dm_logfile(dmp, bu_vls_addr(&mvars->log));

    dm_generic_hook(sdp, name, base, value, data);
}

static void
ogl_bound_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)base;
    dm *dmp = mvars->this_dm;

    dmp->dm_bound = mvars->bound;

    dm_generic_hook(sdp, name, base, value, data);
}

static void
ogl_bound_flag_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)base;
    dm *dmp = mvars->this_dm;

    dmp->dm_boundFlag = mvars->boundFlag;

    dm_generic_hook(sdp, name, base, value, data);
}

static void
ogl_zbuffer_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)base;
    dm *dmp = mvars->this_dm;

    (void)dm_make_current(dmp);
    (void)dm_set_zbuffer(dmp, mvars->zbuffer_on);

    dm_generic_hook(sdp, name, base, value, data);
}

static void
ogl_lighting_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)base;
    dm *dmp = mvars->this_dm;

    (void)dm_make_current(dmp);
    (void)dm_set_light(dmp, mvars->lighting_on);

    dm_generic_hook(sdp, name, base, value, data);
}

static void
ogl_transparency_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)base;
    dm *dmp = mvars->this_dm;

    (void)dm_make_current(dmp);
    (void)dm_set_transparency(dmp, mvars->transparency_on);

    dm_generic_hook(sdp, name, base, value, data);
}

static void
ogl_fog_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_ogl_vars *mvars = (struct modifiable_ogl_vars *)base;
    dm *dmp = mvars->this_dm;

    dm_fogHint(dmp, mvars->fastfog);

    dm_generic_hook(sdp, name, base, value, data);
}

struct bu_structparse Ogl_vparse[] = {
    {"%d",  1, "depthcue",              Ogl_MV_O(cueing_on),    Ogl_colorchange, NULL, NULL },
    {"%d",  1, "zclip",         	Ogl_MV_O(zclipping_on), ogl_zclip_hook, NULL, NULL },
    {"%d",  1, "zbuffer",               Ogl_MV_O(zbuffer_on),   ogl_zbuffer_hook, NULL, NULL },
    {"%d",  1, "lighting",              Ogl_MV_O(lighting_on),  ogl_lighting_hook, NULL, NULL },
    {"%d",  1, "transparency",  	Ogl_MV_O(transparency_on), ogl_transparency_hook, NULL, NULL },
    {"%d",  1, "fastfog",               Ogl_MV_O(fastfog),      ogl_fog_hook, NULL, NULL },
    {"%g",  1, "density",               Ogl_MV_O(fogdensity),   dm_generic_hook, NULL, NULL },
    {"%d",  1, "has_zbuf",              Ogl_MV_O(zbuf),         dm_generic_hook, NULL, NULL },
    {"%d",  1, "has_rgb",               Ogl_MV_O(rgb),          dm_generic_hook, NULL, NULL },
    {"%d",  1, "has_doublebuffer",      Ogl_MV_O(doublebuffer), dm_generic_hook, NULL, NULL },
    {"%d",  1, "depth",         	Ogl_MV_O(depth),        dm_generic_hook, NULL, NULL },
    {"%d",  1, "debug",         	Ogl_MV_O(debug),        ogl_debug_hook, NULL, NULL },
    {"%V",  1, "log",   		Ogl_MV_O(log),  	 ogl_logfile_hook, NULL, NULL },
    {"%g",  1, "bound",         	Ogl_MV_O(bound),        ogl_bound_hook, NULL, NULL },
    {"%d",  1, "useBound",              Ogl_MV_O(boundFlag),    ogl_bound_flag_hook, NULL, NULL },
    {"",        0,  (char *)0,          0,                      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

struct dm_internal dm_ogl = {
    ogl_close,
    ogl_drawBegin,
    ogl_drawEnd,
    ogl_normal,
    ogl_loadMatrix,
    ogl_loadPMatrix,
    ogl_drawString2D,
    ogl_drawLine2D,
    ogl_drawLine3D,
    ogl_drawLines3D,
    ogl_drawPoint2D,
    ogl_drawPoint3D,
    ogl_drawPoints3D,
    ogl_drawVList,
    ogl_drawVListHiddenLine,
    ogl_draw,
    ogl_setFGColor,
    ogl_setBGColor,
    ogl_setLineAttr,
    ogl_configureWin,
    ogl_setWinBounds,
    ogl_setLight,
    ogl_setTransparency,
    ogl_setDepthMask,
    ogl_setZBuffer,
    ogl_debug,
    ogl_logfile,
    ogl_beginDList,
    ogl_endDList,
    ogl_drawDList,
    ogl_freeDLists,
    ogl_genDLists,
    ogl_draw_obj,
    ogl_getDisplayImage, /* display to image function */
    ogl_reshape,
    ogl_makeCurrent,
    ogl_openFb,
    ogl_get_internal,
    ogl_put_internal,
    0,
    1,				/* has displaylist */
    0,                          /* no stereo by default */
    1.0,			/* zoom-in limit */
    1,				/* bound flag */
    "ogl",
    "X Windows with OpenGL graphics",
    DM_TYPE_OGL,
    1,
    0,
    0,
    0, /* bytes per pixel */
    0, /* bits per channel */
    0,
    0,
    1.0, /* aspect ratio */
    0,
    {0, 0},
    NULL,
    NULL,
    BU_VLS_INIT_ZERO,		/* bu_vls path name*/
    BU_VLS_INIT_ZERO,		/* bu_vls full name drawing window */
    BU_VLS_INIT_ZERO,		/* bu_vls short name drawing window */
    {0, 0, 0},			/* bg color */
    {0, 0, 0},			/* fg color */
    {GED_MIN, GED_MIN, GED_MIN},	/* clipmin */
    {GED_MAX, GED_MAX, GED_MAX},	/* clipmax */
    0,				/* no debugging */
    BU_VLS_INIT_ZERO,		/* bu_vls logfile */
    0,				/* no perspective */
    0,				/* no lighting */
    0,				/* no transparency */
    1,				/* depth buffer is writable */
    1,				/* zbuffer */
    0,				/* no zclipping */
    0,                          /* clear back buffer after drawing and swap */
    0,                          /* not overriding the auto font size */
    Ogl_vparse,
    FB_NULL,
    0				/* Tcl interpreter */
};


#endif /* DM_OGL */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
