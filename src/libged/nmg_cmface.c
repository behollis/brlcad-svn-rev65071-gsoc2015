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
    struct vertex **pverts[VERTNUM];
    struct faceuse *fu;
    struct bn_tol tol;
    int size, idx;
    struct bu_list* lst;

    static const char *usage = "nmg_name x0 y0 z0 x1 y1 z1 x2 y2 z2";

    size = sizeof(struct vertex);

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

    /* For now, let's just add quad face to the first
    *  region's first shell.
    *  TODO: take shell from CLI
    */
    r = BU_LIST_FIRST(nmgregion, &m->r_hd);
    NMG_CK_REGION(r);

    s = BU_LIST_FIRST(shell, &r->s_hd);
    NMG_CK_SHELL(s);

    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.01;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 0.001;
    tol.para = 0.999;

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
        pverts[idx] = &verts[idx];
    }

    for (idx=0; idx < VERTNUM*3; idx+=3){
        nmg_vertex_g(verts[idx/3], (fastf_t)atof(argv[idx+2]),
                                   (fastf_t)atof(argv[idx+3]),
                                   (fastf_t)atof(argv[idx+4]));
    }

    fu = nmg_cmface(s, &pverts[0], VERTNUM);
    (void)nmg_fu_planeeqn(fu, &tol);

    NMG_CK_MODEL(m);

    return GED_OK;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
