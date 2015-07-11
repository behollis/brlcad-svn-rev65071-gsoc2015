/*                         N M G _ C M F A C E . C
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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
/** @file libged/nmg_cmface.c
 *
 * The cmface command.
 *
 */

#include "common.h"
#include "nmg.h"

#include <signal.h>
#include <string.h>

#include "bu/cmd.h"
#include "rt/geom.h"

#include "./ged_private.h"

#define VERTNUM 3

int
ged_nmg_cmface(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal internal;
    struct directory *dp;
    struct model* m;
    struct vertexuse* vu;
    struct vertex_g* vg;
    const char* name;
    struct nmgregion* r;
    struct shell* s;
    struct vertex *verts[VERTNUM];
    struct faceuse *fu;
    struct bn_tol tol;
    int idx;
    struct bu_list* lst;
    struct vertex ***face_verts;
    int len;
    int num_verts = VERTNUM;
    int i, j;
    fastf_t *tmp;

    static const char *usage = "nmg_name x0 y0 z0 x1 y1 z1 x2 y2 z2";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    if (argc != 2 + VERTNUM*3) {
       bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
       return GED_HELP;
    }

    /* attempt to resolve and verify */
    name = argv[1];

    if ( (dp=db_lookup(gedp->ged_wdbp->dbip, name, LOOKUP_QUIET))
       == RT_DIR_NULL ) {
       bu_vls_printf(gedp->ged_result_str, "%s does not exist\n", name);
       return GED_ERROR;
    }

    if (rt_db_get_internal(&internal, dp, gedp->ged_wdbp->dbip,
       bn_mat_identity, &rt_uniresource) < 0) {
       bu_vls_printf(gedp->ged_result_str, "rt_db_get_internal() error\n");
       return GED_ERROR;
    }

    if (internal.idb_type != ID_NMG) {
       bu_vls_printf(gedp->ged_result_str, "%s is not an NMG solid\n", name);
       rt_db_free_internal(&internal);
       return GED_ERROR;
    }

    m = (struct model *)internal.idb_ptr;
    NMG_CK_MODEL(m);

    if (BU_LIST_IS_EMPTY(&m->r_hd)) {
        r = nmg_mrsv(m);
        s = BU_LIST_FIRST(shell, &r->s_hd);
    } else {
        r = BU_LIST_FIRST(nmgregion, &m->r_hd);
        s = BU_LIST_FIRST(shell, &r->s_hd);
    }

    NMG_CK_REGION(r);
    NMG_CK_SHELL(s);

    for(idx = 0; idx < VERTNUM; idx++) {
        GET_VERTEX(verts[idx], m)

        BU_GET(lst, struct bu_list);
        BU_LIST_INIT_MAGIC(lst, NMG_VERTEXUSE_MAGIC)
        verts[idx]->vu_hd = *lst;
        /*
        vu = nmg_mvu(verts[idx], s, m);
        BU_LIST_INSERT(lst, vu)
        */

        GET_VERTEX_G(vg, m)
        vg->magic = NMG_VERTEX_G_MAGIC;
        verts[idx]->vg_p = vg;
        verts[idx]->magic = NMG_VERTEX_MAGIC;
    }

    face_verts = (struct vertex ***) bu_calloc( VERTNUM,
            sizeof(struct vertex **), "face_verts");
    fu = nmg_cmface( s, face_verts, VERTNUM );
    bu_free((char *) face_verts, "face_verts");

    for (idx=0; idx < VERTNUM*3; idx+=3){
        nmg_vertex_g(verts[idx/3], (fastf_t)atof(argv[idx+2]),
                                   (fastf_t)atof(argv[idx+3]),
                                   (fastf_t)atof(argv[idx+4]));
    }

    for (j=0; j<VERTNUM; j++) {
       face_verts[j] = &verts[j];
    }

    /* assign face geometry */
    if (s) {
        for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
            if (fu->orientation != OT_SAME)
                continue;
            nmg_calc_face_g(fu);
        }
    }

    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    nmg_rebound(m, &tol);

    return GED_OK;
}

#if 0
int
rt_nmg_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct model *m;
    struct nmgregion *r=NULL;
    struct shell *s=NULL;
    struct faceuse *fu=NULL;
    Tcl_Obj *obj, **obj_array;
    int len;
    int num_verts = 0;
    int num_loops = 0;
    int *loop;
    int loop_len;
    int i, j;
    struct tmp_v *verts;
    fastf_t *tmp;
    struct bn_tol tol;

    RT_CK_DB_INTERNAL(intern);
    m = (struct model *)intern->idb_ptr;
    NMG_CK_MODEL(m);

    verts = (struct tmp_v *)NULL;
    for (i=0; i<argc; i += 2) {
    if (BU_STR_EQUAL(argv[i], "V")) {
        obj = Tcl_NewStringObj(argv[i+1], -1);
        if (Tcl_ListObjGetElements(brlcad_interp, obj, &num_verts,
                       &obj_array) != TCL_OK) {
        bu_vls_printf(logstr,
                  "ERROR: failed to parse vertex list\n");
        Tcl_DecrRefCount(obj);
        return BRLCAD_ERROR;
        }
        verts = (struct tmp_v *)bu_calloc(num_verts,
                          sizeof(struct tmp_v),
                          "verts");
        for (j=0; j<num_verts; j++) {
        len = 3;
        tmp = &verts[j].pt[0];
        if (tcl_obj_to_fastf_array(brlcad_interp, obj_array[j],
                       &tmp, &len) != 3) {
            bu_vls_printf(logstr,
                  "ERROR: incorrect number of coordinates for vertex\n");
            return BRLCAD_ERROR;
        }
        }

    }
    }

    while (argc >= 2) {
    struct vertex ***face_verts;

    if (BU_STR_EQUAL(argv[0], "V")) {
        /* vertex list handled above */
        goto cont;
    } else if (BU_STR_EQUAL(argv[0], "F")) {
        if (!verts) {
        bu_vls_printf(logstr,
                  "ERROR: cannot set faces without vertices\n");
        return BRLCAD_ERROR;
        }
        if (BU_LIST_IS_EMPTY(&m->r_hd)) {
        r = nmg_mrsv(m);
        s = BU_LIST_FIRST(shell, &r->s_hd);
        } else {
        r = BU_LIST_FIRST(nmgregion, &m->r_hd);
        s = BU_LIST_FIRST(shell, &r->s_hd);
        }
        obj = Tcl_NewStringObj(argv[1], -1);
        if (Tcl_ListObjGetElements(brlcad_interp, obj, &num_loops,
                       &obj_array) != TCL_OK) {
        bu_vls_printf(logstr,
                  "ERROR: failed to parse face list\n");
        Tcl_DecrRefCount(obj);
        return BRLCAD_ERROR;
        }
        for (i=0, fu=NULL; i<num_loops; i++) {
        struct vertex **loop_verts;
        /* struct faceuse fu is initialized in earlier scope */

        loop_len = 0;
        (void)tcl_obj_to_int_array(brlcad_interp, obj_array[i],
                       &loop, &loop_len);
        if (!loop_len) {
            bu_vls_printf(logstr,
                  "ERROR: unable to parse face list\n");
            return BRLCAD_ERROR;
        }
        if (i) {
            loop_verts = (struct vertex **)bu_calloc(
            loop_len,
            sizeof(struct vertex *),
            "loop_verts");
            for (i=0; i<loop_len; i++) {
            loop_verts[i] = verts[loop[i]].v;
            }
            fu = nmg_add_loop_to_face(s, fu,
                          loop_verts, loop_len,
                          OT_OPPOSITE);
            for (i=0; i<loop_len; i++) {
            verts[loop[i]].v = loop_verts[i];
            }
        } else {
            face_verts = (struct vertex ***)bu_calloc(
            loop_len,
            sizeof(struct vertex **),
            "face_verts");
            for (j=0; j<loop_len; j++) {
            face_verts[j] = &verts[loop[j]].v;
            }
            fu = nmg_cmface(s, face_verts, loop_len);
            bu_free((char *)face_verts, "face_verts");
        }
        }
    } else {
        bu_vls_printf(logstr,
              "ERROR: Unrecognized parameter, must be V or F\n");
        return BRLCAD_ERROR;
    }
    cont:
    argc -= 2;
    argv += 2;
    }

    /* assign geometry for entire vertex list (if we have one) */
    for (i=0; i<num_verts; i++) {
    if (verts[i].v)
        nmg_vertex_gv(verts[i].v, verts[i].pt);
    }

    /* assign face geometry */
    if (s) {
    for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
        if (fu->orientation != OT_SAME)
        continue;
        nmg_calc_face_g(fu);
    }
    }

    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    nmg_rebound(m, &tol);

    return BRLCAD_OK;
}
#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

